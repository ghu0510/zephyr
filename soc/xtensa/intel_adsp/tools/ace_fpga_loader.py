#!/usr/bin/env python3
#
# Copyright(c) 2022 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import asyncio
import ctypes
import struct
import sys
import time

class IpcResponder():
    def __init__(self, tool_obj):
        self.tool_obj = tool_obj
        self.ipc_timestamp = 0
        self.host_win = None

    async def ipc_delay(self):
        await asyncio.sleep(0.1)

    def set_host_windows(self, host_windows):
        self.host_win = host_windows

    def process_ipc(self, data, ext_data):
        send_msg = False
        done = True

        if data == 0:
            pass
        elif data == 1: # async command: signal DONE after a delay (on 1.8+)
            asyncio.ensure_future(self.ipc_delay())
        elif data == 2: # echo back ext_data as a message command
            send_msg = True
        elif data == 3: # set ADSPCS
            self.tool_obj.reg_set_adspcs(ext_data)
        elif data == 4: # echo back microseconds since last timestamp command
            tsp = round(time.time() * 1e6)
            ext_data = tsp - self.ipc_timestamp
            self.ipc_timestamp = tsp
            send_msg = True
        elif data == 5: # copy word at outbox[ext_data >> 16] to inbox[ext_data & 0xffff]
            if self.host_win:
                offset_outbox = 4096
                offset_inbox = 0

                src = offset_outbox + 4 * (ext_data >> 16)
                dst = offset_inbox + 4 * (ext_data & 0xffff)
                for i in range(4):
                    self.host_win[1][dst + i] = self.host_win[0][src + i]
        else:
            sys.stderr.write(f"ERROR: Unrecognized IPC command 0x{data:x} ext 0x{ext_data:x}\n")
            send_msg = False
            done = False

        self.tool_obj.ipc_target_busy_clear()

        if done:
            self.tool_obj.ipc_target_response(0)

        if send_msg:
            self.tool_obj.ipc_initiator_send(ext_data, ext_data)


# Syntactic sugar to make register block definition & use look nice.
# Instantiate from a base address, assign offsets to (uint32) named registers as
# fields, call freeze(), then the field acts as a direct alias for the register!
class Regs:
    def __init__(self, base_addr):
        vars(self)["base_addr"] = base_addr
        vars(self)["ptrs"] = {}
        vars(self)["frozen"] = False
    def freeze(self):
        vars(self)["frozen"] = True
    def __setattr__(self, name, val):
        if not self.frozen and name not in self.ptrs:
            addr = self.base_addr + val
            self.ptrs[name] = ctypes.c_uint32.from_address(addr)
        else:
            self.ptrs[name].value = val
    def __getattr__(self, name):
        return self.ptrs[name].value


class WinStream():
    def __init__(self, slow_read=True):
        # if slow_read is True, it will read from self.host_win byte by byte.
        # Some interfaces are not very reliable when doing self.host_win[a:b]
        # so set slow_read to True if that is the case.
        self.slow_read = slow_read

    def parse_args(self, args):
        """
        Parse command line arguments.
        """
        if args.no_log_history:
            self.args_no_log_history = True
        else:
            self.args_no_log_history = False

    def setup(self, host_win, error_output):
        """
        Setup WinStream to read and output console logs.

        error_output can be sys.stderr.write or
        logging.error.
        """
        self.host_win = host_win
        self.error_output = error_output

    def win_read(self, start, length):
        """
        Read raw data from host window.
        """
        try:
            if not self.slow_read:
                return bytes(self.host_win[start:(start + length)])
            else:
                return b''.join(self.host_win[x].to_bytes(1, 'little')
                                for x in range(start, start + length))
        except IndexError as ie1:
            # A FW in a bad state may cause winstream garbage
            self.error_output("IndexError in host_win[%d]", start)
            raise ie1

    def win_hdr(self):
        """
        Unpack the winstream header.
        """
        return struct.unpack("<IIII", self.win_read(0, 16))

    # Python implementation of the same algorithm in sys_winstream_read(),
    # see there for details.
    def winstream_read(self, last_seq):
        """
        Decipher winstream into readable text.
        """
        while True:
            (wlen, start, end, seq) = self.win_hdr()
            if last_seq == 0:
                last_seq = seq if self.args_no_log_history else (seq - ((end - start) % wlen))
            if seq == last_seq or start == end:
                return (seq, "")
            behind = seq - last_seq
            if behind > ((end - start) % wlen):
                return (seq, "")
            copy = (end - behind) % wlen
            suffix = min(behind, wlen - copy)
            result = self.win_read(16 + copy, suffix)
            if suffix < behind:
                result += self.win_read(16, behind - suffix)
            (wlen, start1, end, seq1) = self.win_hdr()
            if start1 == start and seq1 == seq:
                # Best effort attempt at decoding, replacing unusable characters
                # Found to be useful when it really goes wrong
                return (seq, result.decode("utf-8", "replace"))


class ToolBase():
    """
    Base class for tools (including firmware load and console logs reader).

    These must be called in the following order:
        tool.parse_args(args)
        tool.setup()

        tool.firmware_loader_setup()
        tool.firmware_load()

        tool.logging_setup()
        tool.logging_output_sync()
    """

    @staticmethod
    def argparse_options(parser):
        """
        Add arguments to the global argument parsers.
        """

    def parse_args(self, args):
        """
        Parse command line arguments.
        """

    def setup(self):
        """
        Perform necessary setup for the tool to be functional.
        """

    def firmware_loader_setup(self):
        """
        Perform necesseary setup to prepare the firmware loader
        before loading firmware onto target device.
        """

    def firmware_load(self):
        """
        Load firmware onto target device.
        """

    def map_host_windows(self):
        """
        Map various host windows on the DSP so we can access
        the directly.
        """

    def logging_setup(self):
        """
        Perform necesseary preparations before reading
        console logs from the target device.
        """

    def logging_output_sync(self):
        """
        Read the console logs from the target device
        synchronously and display them onto terminal.
        """

    def ipc_init(self):
        """
        Perform any initialization steps necessary to IPC
        to function.
        """

    def ipc_target_busy_clear(self):
        """
        Acknowledge the remote IPC request to local target
        by clearing the BUSY bit in HIPCTDR.
        """

    def ipc_target_response(self, response):
        """
        Send an response to a remote IPC request.
        This sets the DONE bit and response value in HIPCTDA.
        """

    def ipc_target_is_waiting(self):
        """
        Check if there is a remote IPC waiting to be processed.
        """

    def ipc_initiator_send(self, data, ext_data):
        """
        Send an IPC from local host to remote target.
        """

    def ipc_initiator_is_acked(self):
        """
        Wait for a loca host-initiated IPC to be acked by the remote target.
        This waits for BUSY bit to be cleared in HIPCIDR.
        """

    def ipc_initiator_read_response(self):
        """
        Read the response of the a local host-initiated IPC
        from the remote target.
        """

    def reg_set_adspcs(self, val):
        """
        !!! FOR TESTING USE ONLY!!!

        Some cAVS versions require the host to power up/down Xtensa cores.
        So this is used to set the register for CPU power.
        """


class AceFpgaTool(ToolBase):
    MIN_FW_FILE_SIZE = 512 * 1024
    IPC_BUSY_BIT = (1 << 31)
    IPC_DONE_BIT = (1 << 31)

    @staticmethod
    def argparse_options(parser):

        subparser = parser.add_argument_group("Options for ACE FPGA")

        subparser.add_argument("--fat", help="Path to FAT tool")

        subparser.add_argument("--rom", action="append",
                help="Path to ROM File(s)")
        subparser.add_argument("--romext", help="Path to ROM EXT File")

        subparser.add_argument("--fatlogger", action="store_true",
                help="Use FAT's logger for firmware console output instead of stdout")

        subparser.add_argument("--ignore-fpga-load-errors", action="store_true",
                help="Ignore any firmware loading errors on FPGA")


    def parse_args(self, args):
        self.args = args

        # Add to the search path for modules if path to FAT
        # is specified. Otherwise, Python will search under
        # the current directory.
        if args.fat:
            # Some systems have search path pre-set pointing
            # to a copy of FAT. We do not want to use a copy
            # of unknown origin. So prepend the specified path
            # such that the one specified is actually being used.
            sys.path.insert(0, args.fat)

        if not args.log_only:
            # ROM EXT is a must for FPGA.
            if not args.romext:
                sys.stderr.write("ROM EXT file required!")
                sys.exit(-1)

            # Of course, the firmware itself is also a MUST.
            if not args.fw:
                sys.stderr.write("Firmware file required!")
                sys.exit(-1)

            self.file_romext = args.romext
            self.file_fw = args.fw

    def setup(self):
        # Set platform type.
        #
        # This needs to be done before importing anything
        # under [FAT]/utilities as some of them do init
        # at import, and they expect platform type to be
        # set already.
        #
        # pylint: disable=import-outside-toplevel
        from platform_defs.fw_type import CAVS, try_set
        try_set(CAVS)

        from utilities.fw_loader_pci_ace import FwLoaderOverPciAce
        from utilities.platform_helper import get_platform
        from utilities.test_controller import TestController
        from utilities.test_logger import get_logger
        from utilities.utils import get_dev_audio, get_platform_def

        class ZephyrFwLoaderOverPciAce(FwLoaderOverPciAce):
            @property
            def use_auth_bypass(self) -> bool:
                # FwLoaderOverPciAce gets auth bypass switch from pytest.
                # Since this is not invoked by pytest, it would fail.
                # So override it here.
                return False

        # Setup logger
        self.logger = get_logger(logger_name="zephyr",
                default_logger=True,
                logger_types=['text']
                )

        # Create the test controller as lots of stuff in FAT depends on it.
        self.test_controller = TestController()
        self.test_controller.env_init(
                self.logger,
                CAVS,
                pnp_sim=False,
                platform=get_platform()
                )

        # Tell test controller to use the Zephyr firmware loader class
        self.fw_loader = ZephyrFwLoaderOverPciAce(called_by_getter=True)
        self.test_controller.set_fw_loader(self.fw_loader)
        self.dev_audio = get_dev_audio()

        # Setup access to registers on DSP
        #
        # pylint doesn't like variable names in uppercases.
        ipc_base_offset = get_platform_def().IPC_BASE_OFFSET
        reg_dsp = Regs(self.dev_audio.bar_dsp_virt_address + ipc_base_offset)

        # pylint: disable=invalid-name
        reg_dsp.HIPCTDR        = 0x00000000
        reg_dsp.HIPCTDA        = 0x00000004
        reg_dsp.HIPCTDD        = 0x00000100
        reg_dsp.HIPCIDR        = 0x00000010
        reg_dsp.HIPCIDA        = 0x00000014
        reg_dsp.HIPCIDD        = 0x00000180
        reg_dsp.HIPCCST        = 0x00000020
        reg_dsp.HIPCCSR        = 0x00000024
        reg_dsp.HIPCCTL        = 0x00000028
        reg_dsp.HIPCCAP        = 0x0000002c
        reg_dsp.freeze()
        self.reg_dsp = reg_dsp
        # pylint: enable=invalid-name

        # Direct access to FW status in host window #0
        host_win0_offset = get_platform_def().ADSP_OFFSET_WINDOW_0
        reg_fw_status = Regs(self.dev_audio.bar_dsp_virt_address + host_win0_offset)
        reg_fw_status.fw_status = 0x00000000
        reg_fw_status.freeze()
        self.reg_fw_status = reg_fw_status

        if not self.args.log_only:
            self.ipc_responder = IpcResponder(self)

    def firmware_load(self):
        # pylint: disable=import-outside-toplevel
        from utilities.fw_loader import load_firmware_base

        # Read the firmware into memory
        fw_bin = None
        with open(self.file_fw, "rb") as fw:
            fw_bin = fw.read()
            fw.close()

        if not fw_bin:
            self.logger.error(f"Cannot read firmware from file {self.file_fw}")
            sys.exit(-1)

        # For some reason, the DMA transfer needs to be at least certain size.
        # So pad the binary data.
        if len(fw_bin) < self.MIN_FW_FILE_SIZE:
            padding_sz = self.MIN_FW_FILE_SIZE - len(fw_bin)
            fw_bin = fw_bin + b'\x00' * padding_sz

        if self.args.ignore_fpga_load_errors:
            timeout = 0.5
        else:
            timeout = 5.0

        rom = self.args.rom if self.args.rom else None

        # Load the firmware onto FPGA
        try:
            load_firmware_base(
                    fpga=True, silent_load=True,
                    rom=rom,
                    romext=self.file_romext, fw=fw_bin,
                    timeout=timeout
                    )
        except TimeoutError as toe:
            if not self.args.ignore_fpga_load_errors:
                raise toe


    def map_host_windows(self):
        # pylint: disable=import-outside-toplevel
        from utilities.utils import get_platform_def

        self.host_win = [None, None, None, None]

        # cast the host window #0 as it contains fw status and outbox
        host_win0_offset = get_platform_def().ADSP_OFFSET_WINDOW_0
        self.host_win[0] = ctypes.cast(
                self.dev_audio.bar_dsp_virt_address + host_win0_offset,
                ctypes.POINTER(ctypes.c_ubyte)
                )

        # cast the host window #1 as it contains inbox
        host_win1_offset = get_platform_def().ADSP_OFFSET_WINDOW_1
        self.host_win[1] = ctypes.cast(
                self.dev_audio.bar_dsp_virt_address + host_win1_offset,
                ctypes.POINTER(ctypes.c_ubyte)
                )

        # cast the host window #2 as it contains traces and debug info
        host_win2_offset = get_platform_def().ADSP_OFFSET_WINDOW_2
        self.host_win[2] = ctypes.cast(
                self.dev_audio.bar_dsp_virt_address + host_win2_offset,
                ctypes.POINTER(ctypes.c_ubyte)
                )

        # cast the host window #3 as it contains the winstream log
        host_win3_offset = get_platform_def().ADSP_OFFSET_WINDOW_3
        self.host_win[3] = ctypes.cast(
                self.dev_audio.bar_dsp_virt_address + host_win3_offset,
                ctypes.POINTER(ctypes.c_ubyte)
                )

        if not self.args.log_only:
            self.ipc_responder.set_host_windows(self.host_win)

    def logging_setup(self):
        # pylint: disable=import-outside-toplevel
        from utilities.ipc_driver import get_ipc_driver

        # setup WinStream-er
        self.winstream = WinStream(slow_read=False)
        self.winstream.parse_args(self.args)
        self.winstream.setup(self.host_win[3], self.logger.error)

        # Stop FAT's own IPC driver so we can do IPC our own.
        get_ipc_driver().close()
        self.ipc_init()

    def logging_output_sync(self):
        class SyncObj():
            def __init__(self):
                self.start_output = True

        async def read_log(winstream, sync_obj, args):
            last_seq = 0
            while sync_obj.start_output is True:
                (last_seq, output) = winstream.winstream_read(last_seq)
                if output:
                    if args.fatlogger:
                        self.logger.info(output)
                    else:
                        sys.stdout.write(output)
                        sys.stdout.flush()

                if not args.log_only:
                    if self.ipc_target_is_waiting():
                        #self.ipc_print_regs("w")
                        self.ipc_responder.process_ipc(
                                self.reg_dsp.HIPCTDR & ~self.IPC_BUSY_BIT,
                                self.reg_dsp.HIPCTDD
                                )

        sync_obj = SyncObj()

        try:
            asyncio.get_event_loop().run_until_complete(
                    read_log(self.winstream, sync_obj, self.args))
        except KeyboardInterrupt:
            sync_obj.start_output = False

    def ipc_init(self):
        self.ipc_target_busy_clear()
        self.reg_dsp.HIPCTDA = 0

        self.reg_dsp.HIPCIDR = 0
        self.reg_dsp.HIPCIDA = self.IPC_DONE_BIT
        self.reg_dsp.HIPCIDD = 0

    def ipc_target_busy_clear(self):
        self.reg_dsp.HIPCTDR = self.IPC_BUSY_BIT

    def ipc_target_response(self, response):
        self.reg_dsp.HIPCTDA = response & ~self.IPC_DONE_BIT

    def ipc_target_is_waiting(self):
        return (self.reg_dsp.HIPCTDR & self.IPC_BUSY_BIT) == self.IPC_BUSY_BIT

    def ipc_initiator_send(self, data, ext_data):
        self.reg_dsp.HIPCIDD = ext_data
        self.reg_dsp.HIPCIDR = self.IPC_BUSY_BIT | data

    def ipc_initiator_is_acked(self):
        return (self.reg_dsp.HIPCIDR & self.IPC_BUSY_BIT) == 0

    def ipc_initiator_read_response(self):
        if (self.reg_dsp.HIPCIDA & self.IPC_DONE_BIT) == 0:
            return None
        else:
            return self.reg_dsp.HIPCIDA & ~self.IPC_DONE_BIT


def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser()

    parser.add_argument("--fw", help="Path Firmware File.")
    parser.add_argument("--log-only", action="store_true",
            help="Do not load firmware. Only show log output.")
    parser.add_argument("--load-only", action="store_true",
            help="Load firmware only. No log output.")
    parser.add_argument("--no-log-history", action="store_true",
            help="Only new log output, ignoring existing log entries.")

    AceFpgaTool.argparse_options(parser)

    args = parser.parse_args()

    # Load the firmware and start logging output
    fw_tool = AceFpgaTool()

    fw_tool.parse_args(args)
    fw_tool.setup()

    if not args.log_only:
        fw_tool.firmware_loader_setup()
        fw_tool.firmware_load()

    fw_tool.map_host_windows()

    if not args.load_only:
        fw_tool.logging_setup()
        fw_tool.logging_output_sync()


if __name__ == "__main__":
    main()
