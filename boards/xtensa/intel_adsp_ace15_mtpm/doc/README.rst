Intel DSP Meteor Lake Simulator
*******************************

Environment Setup
#################

Start with a Zephyr-capable build host on the Intel network (VPN is
fine as long as it's up).

The Zephyr tree is best cloned via west.

.. code-block:: console

   mkdir ~/zephyrproject
   cd ~/zephyrproject
   west init -m https://github.com/intel-innersource/os.rtos.zephyr.zephyr-intel
   west update


Toolchain Setup on Host
=======================

No special instructions are required to build using the Zephyr SDK.
West works like it does for any other platform.

However, if you want to use the Cadence toolchain instead of GCC, you will need to
install the XCC toolchain for your core. We provide an all-in-one package
that help you to install the toolchain easily:

- xtensa-dist.tar.gz

You can download it from http://gale.hf.intel.com/~nashif/audio/, along with
the simulator; see the next section.

With the above package, set the toolchain up:

.. code-block:: console

   # Uncompress the Xtensa toolchain
   tar zxvf xtensa-dist.tar.gz

   # Enter the directory
   cd xtensa-dist

   # Run the install script
   ./install.sh

The script will install the toolchain for you and set up the core registry
required by the toolchain. The toolchain will be installed in ~/xtensa.

You might need to install some 32-bit libraries to run this command and some of
the binaries included in the toolchain. Install all needed 32 bit packages:

.. code-block:: console

   sudo dpkg --add-architecture i386
   sudo apt-get install libncurses5:i386 zlib1g:i386 libcrypt1:i386

Make sure also that you have these additional libraries installed:

.. code-block:: console

   sudo apt-get install libncurses5 libncurses6 libncursesw5 libncursesw6 libtinfo6 libcrypt1 libssl-dev

Once installed, you are ready to build and run a Zephyr application for your board using the Cadence XCC compiler and the software simulator.

A few environment variables are needed to tell Zephyr where the toolchain is:

.. code-block:: console

   export ZEPHYR_TOOLCHAIN_VARIANT=xcc

   export XTENSA_TOOLCHAIN_PATH=$HOME/xtensa/XtDevTools/install/tools
   export TOOLCHAIN_VER=RG-2019.12-linux
   export XTENSA_CORE=ace10_LX7HiFi4

You may also need to define the license server for XCC. Set the environment
variable `XTENSAD_LICENSE_FILE`:

.. code-block:: console

   export XTENSAD_LICENSE_FILE=84300@xtensa01p.elic.intel.com


Install the prebuilt simulator and ROM
======================================

The simulator team has release version of prebuilt simulators we can
run Zephyr on. The Zephyr team has already customized the available
prebuilt binary package for you. You can download it as an archive from
the same folder as the toolchain. The naming scheme for the archives follow
the template sim_{board}_{date}.tar.bz2. For example,
"sim_mtl_20221018.tar.bz2". The later the date, the more recent the
release. Install the simulator by downloading and extracting the archive:

.. code-block:: console

   tar xvf sim_mtl_20221018.tar.bz2 -C ~/

After the simulator and the ROM are installed, you will need to set the
ACE_SIM_DIR environment variable. To run on another version of the simulator,
export the path to that version. For example:

.. code-block:: console

   export ACE_SIM_DIR=~/sim_mtl_20221018

Building Rimage
###############

You will need to build the upstream rimage and install it:

.. code-block:: console

   git clone https://github.com/thesofproject/rimage
   cd rimage
   git submodule init
   git submodule update
   cmake -B build/
   sudo make -C build/ install

If you get a error message "error: Unsupported config version 3.0",
you might need to get the latest code of rimage tool from upstream and
rebuild it.

Building a Zephyr Application
#############################

The `intel_adsp_ace15_mtpm_sim` board currently exists as patches on the internal
`Zephyr` repository. You can still build for it as you would any other
upstream board:

.. code-block:: console

   west build -p auto -b intel_adsp_ace15_mtpm_sim samples/hello_world

Run in the Simulator
####################

Invocation of the simulator itself is somewhat involved, so the
details are now handled by a wrapper script (acesim.py) which is
integrated as a Zephyr native emulator.

After building with west, call

.. code-block:: console

   ninja -C build run

You can also build and run in one single command::

.. code-block:: console

   west build -p auto -b intel_adsp_ace15_mtpm_sim samples/hello_world -t run

This is a typical output after running the command:

.. code-block:: console

   (/root/conda_envs/baseline) root@d57b9ae6c812:/z/zephyr-intel#  ninja -C build run
   -- west flash: rebuilding
   [1/1] cd /z/zephyr-intel/build/zephyr/soc/xtensa/intel_adsp/soc/family/common &&...yr-intel/build/zephyr/main.mod /z/zephyr-intel/build/zephyr/main.mod 2>/dev/null
   fix_elf_addrs.py: Moving section .noinit to cached SRAM region
   fix_elf_addrs.py: Moving section .data to cached SRAM region
   fix_elf_addrs.py: Moving section sw_isr_table to cached SRAM region
   fix_elf_addrs.py: Moving section k_pipe_area to cached SRAM region
   fix_elf_addrs.py: Moving section k_sem_area to cached SRAM region
   fix_elf_addrs.py: Moving section .bss to cached SRAM region
   -- west flash: using runner misc-flasher
   + export XTENSA_CORE=ace10_LX7HiFi4
   + fgrep RUNPATH
   + readelf -d sim_prebuilt/dsp_fw_sim
   + sed s/.*\[//+ sed s/\].*//

   + sed s/:/\n/g
   + fgrep /XtDevTools/install/tools/
   + head -1
   + XTLIBS=/root/xtensa/XtDevTools/install/tools/RG-2019.12-linux/XtensaTools/lib64/iss
   + echo /root/xtensa/XtDevTools/install/tools/RG-2019.12-linux/XtensaTools/lib64/iss
   + sed s/.*\/XtDevTools\/install\/tools\///
   + sed s/\/.*//
   + VER=RG-2019.12-linux
   + echo /root/xtensa/XtDevTools/install/tools/RG-2019.12-linux/XtensaTools/lib64/iss
   + sed s/\/RG-2019.12-linux\/.*//
   + TOOLS=/root/xtensa/XtDevTools/install/tools
   + [ ! -z /root/xtensa/XtDevTools/install/tools ]
   + dirname /root/xtensa/XtDevTools/install/tools
   + SDK=/root/xtensa/XtDevTools/install
   + [ ! -z /root/xtensa/XtDevTools/install
   /z/zephyr-intel/boards/xtensa/intel_adsp_ace15_mtpm/support/dsp_fw_sim: 31: [: missing ]
   + export XTENSA_TOOLS_VERSION=RG-2019.12-linux
   + dirname /root/xtensa/XtDevTools/install/tools
   + export XTENSA_BUILDS_DIR=/root/xtensa/XtDevTools/install/builds
   + export LD_LIBRARY_PATH=/root/xtensa/XtDevTools/install/tools/RG-2019.12-linux/XtensaTools/lib64/iss:/std_sim/lib/gna-lib
   + export LD_LIBRARY_PATH=/root/xtensa/XtDevTools/install/tools/RG-2019.12-linux/XtensaTools/lib64:/root/xtensa/XtDevTools/install/tools/RG-2019.12-linux/XtensaTools/lib64/iss:/std_sim/lib/gna-lib
   + echo PREBUILT: xt-bin-path: /root/xtensa/XtDevTools/install/tools/RG-2019.12-linux/XtensaTools/bin
   PREBUILT: xt-bin-path: /root/xtensa/XtDevTools/install/tools/RG-2019.12-linux/XtensaTools/bin
   + cd sim_prebuilt
   + exec ./dsp_fw_sim --platform=mtl --config=/tmp/tmpb7hvl7xg --comm_port=40008 --xtsc.turbo=true --xxdebug=0 --xxdebug=1 --xxdebug=2

               SystemC 2.3.0-ASI --- Feb 22 2019 23:24:20
               Copyright (c) 1996-2012 by all Contributors,
                           ALL RIGHTS RESERVED

   NOTE:        0.0/000: SC_MAIN start, 1.0.0.0 version built Nov 17 2021 at 23:41:22
   NOTE:        0.0/000: setting config for mtl with core ace10_LX7HiFi4
   log4xtensa:ERROR No appenders could be found for logger (dsp_system_parms).
   log4xtensa:ERROR Please initialize the log4xtensa system properly.
   NOTE:        0.0/000: XTENSA_TOOLS_VERSION = RG-2019.12-linux
   NOTE:        0.0/000: XTENSA_BUILDS = /root/xtensa/XtDevTools/install/builds
   NOTE:        0.0/000: ulp config:
   NOTE:        0.0/000: registry: /root/xtensa/XtDevTools/install/builds/RG-2019.12-linux//config
   NOTE:        0.0/000: config: ace10_LX7HiFi4
   NOTE:        0.0/000: registry: /root/xtensa/XtDevTools/install/builds/RG-2019.12-linux/ace10_LX7HiFi4/config
   NOTE:        0.0/000: dsp program to load: /z/zephyr-intel/boards/xtensa/intel_adsp_ace15_mtpm/support/dsp_rom_mtl_sim.hex
   NOTE    dsp_system      -        0.0/000: Connecting host_fabric to dsp_fabric.
   NOTE    dsp_system      -        0.0/000: 0[ms]: Creating DSP Core0 with following params: core_id: 0, core_type: 1, l1_mmio_name:dram0
   WARN    dsp_system      -        0.0/000: 0[ms]: loading /z/zephyr-intel/boards/xtensa/intel_adsp_ace15_mtpm/support/dsp_rom_mtl_sim.hex on core 0
   NOTE    dsp_system      -        0.0/000: 0[ms]: Creating DSP Core1 with following params: core_id: 1, core_type: 2, l1_mmio_name:dram0
   WARN    dsp_system      -        0.0/000: 0[ms]: loading /z/zephyr-intel/boards/xtensa/intel_adsp_ace15_mtpm/support/dsp_rom_mtl_sim.hex on core 1
   NOTE    dsp_system      -        0.0/000: 0[ms]: Creating DSP Core2 with following params: core_id: 2, core_type: 2, l1_mmio_name:dram0
   WARN    dsp_system      -        0.0/000: 0[ms]: loading /z/zephyr-intel/boards/xtensa/intel_adsp_ace15_mtpm/support/dsp_rom_mtl_sim.hex on core 2
   NOTE    dsp_system      -        0.0/000: Configuring module dsp_mmio.
   NOTE    dsp_system      -        0.0/000: Connecting module dsp_mmio to fabric... Port: 0.
   NOTE    dsp_system      -        0.0/000: Configuring IMR... (delay=360)
   NOTE    dsp_system      -        0.0/000: Connecting IMR to fabric...
   NOTE    dsp_system      -        0.0/000: Connecting HPSRAM to fabric...
   NOTE    dsp_system      -        0.0/000: Configure ulp_l2_sram... (delay=7)
   NOTE    dsp_system      -        0.0/000: Connecting ulp_l2_sram to fabric...
   NOTE    dsp_system      -        0.0/000: Configuring LPSRAM... (delay=7), turbo_lpsram=1
   NOTE    dsp_system      -        0.0/000: Connecting LPSRAM to fabric...
   NOTE    dsp_system      -        0.0/000: Building host...
   NOTE    dsp_system      -        0.0/000: Building host module...
   NOTE    host_module     -        0.0/000: Comm port:40008.
   NOTE    dsp_system      -        0.0/000: Building host module... DONE
   NOTE    dsp_system      -        0.0/000: Creating host mmio...
   NOTE    dsp_system      -        0.0/000: Connect mmio to fabric...
   NOTE    dsp_system      -        0.0/000: Creating host mmio...
   NOTE    dsp_system      -        0.0/000: Connect mmio to fabric...
   NOTE    dsp_system      -        0.0/000: Creating host memory...
   NOTE    dsp_system      -        0.0/000: Connecting memory to fabric...
   NOTE    dsp_system      -        0.0/000: Host memory... DONE
   NOTE    dsp_system      -        0.0/000: Building ace interrupts...
   NOTE    dsp_system      -        0.0/000: Building ace interrupts... DONE
   NOTE    dsp_system      -        0.0/000: FW File loaded into local memory. Copying to IMR to address a1040000, size = 1d000
   NOTE    dsp_system      -        0.0/000: Disable ROM-EXT bypass
   NOTE    dsp_system      -        0.0/000: Building ace controls...
   NOTE    dsp_system      -        0.0/000: Creating ssp control...
   NOTE    dsp_system      -        0.0/000: Creating ssp control...
   NOTE    dsp_system      -        0.0/000: Creating uaol control...
   NOTE    dsp_system      -        0.0/000: Creating soundwire control...
   NOTE    dsp_system      -        0.0/000: Creating soundwire master 0 control...
   NOTE    soundwire_master_0 -        0.0/000: 0[ms]: soundwire_master::soundwire_master()
   NOTE    dsp_system      -        0.0/000: Creating soundwire master 1 control...
   NOTE    soundwire_master_1 -        0.0/000: 0[ms]: soundwire_master::soundwire_master()
   NOTE    dsp_system      -        0.0/000: Creating soundwire master 2 control...
   NOTE    soundwire_master_2 -        0.0/000: 0[ms]: soundwire_master::soundwire_master()
   NOTE    dsp_system      -        0.0/000: Creating soundwire master 3 control...
   NOTE    soundwire_master_3 -        0.0/000: 0[ms]: soundwire_master::soundwire_master()
   NOTE    dsp_system      -        0.0/000: Creating tlb module on HP SRAM ...
   NOTE    dsp_system      -        0.0/000: Connecting TLB to mmio...
   NOTE    dsp_system      -        0.0/000: Connecting tlb module to fabric...
   NOTE    dsp_system      -        0.0/000: Creating hda_dma...
   NOTE    dsp_system      -        0.0/000: Connecting hda_dma to fabric.
   NOTE    dmic_ctrl.hq_inject -        0.0/000: Clock period set to: 8333 ns.
   NOTE    dmic_ctrl.hq_inject -        0.0/000: Basic period: 1 ns.
   NOTE    dmic_ctrl.lp_inject -        0.0/000: Clock period set to: 25 us.
   NOTE    dmic_ctrl.lp_inject -        0.0/000: Basic period: 1 ns.
   NOTE    dmic_ctrl       -        0.0/000: Allocating dmic handshake.
   NOTE    gpdma_0         -        0.0/000: Creating dma: gpdma_0. m_channel_cnt = 8
   NOTE    gpdma_1         -        0.0/000: Creating dma: gpdma_1. m_channel_cnt = 8
   NOTE    gpdma_2         -        0.0/000: Creating dma: gpdma_2. m_channel_cnt = 8
   NOTE    dsp_system      -        0.0/000: Connecting GNA accelerator to dsp fabric.
   NOTE    dp_dma_int_aggr -        0.0/000: dp_gpdma_int_aggr_ace resizing with channels. Current size: 1
   NOTE    dp_gpdma_0      -        0.0/000: Creating dma: dp_gpdma_0. m_channel_cnt = 2
   core0: SOCKET:20000
   NOTE    core0           -        0.0/000: Debug info: port=20000 wait=true ()
   Core 0 active:(start with "(xt-gdb) target remote :20000")
   core1: SOCKET:20001
   NOTE    core1           -        0.0/000: Debug info: port=20001 wait=true ()
   Core 1 active:(start with "(xt-gdb) target remote :20001")
   core2: SOCKET:20002
   NOTE    core2           -        0.0/000: Debug info: port=20002 wait=true ()
   Core 2 active:(start with "(xt-gdb) target remote :20002")
   NOTE    hpsram_memory   -        0.0/000: Thread started.
   NOTE    hpsram_memory   -        0.0/000: Thread started.
   NOTE    lpsram_memory   -        0.0/000: Thread started.
   NOTE    lpsram_memory   -        0.0/000: Thread started.
   NOTE    host_module     -        0.0/000: Main thread started.
   NOTE    host_module     -        0.0/000: Interrupt thread started.
   NOTE    host_module     -        0.0/000: Tick thread started. Period: 400 us.
   NOTE    timer_control   -        0.0/000: Wall Clock Thread started.
   NOTE    ipc_control     -        0.0/000: IPC Control Thread started.
   NOTE    sb_ipc_control  -        0.0/000: IPC Control Thread started.
   NOTE    idc_control     -        0.0/000: IDC Control Thread started.
   NOTE    power_control   -        0.0/000: Thread started.
   NOTE    hda_controller  -        0.0/000: HD-A Controller Thread started.
   NOTE    hda_dma         -        0.0/000: Thread started.
   NOTE    dmic_ctrl       -        0.0/000: LP channel cnt changed 2 -> 1.
   NOTE    dmic_ctrl       -        0.0/000: HQ sample size changed 2 -> 2.
   NOTE    dmic_ctrl       -        0.0/000: HQ channel cnt changed 2 -> 1.
   NOTE    gpdma_int_aggr  -        0.0/000: Thread started.
   NOTE    gna_accelerator -        0.0/000: GNA thread started.
   NOTE    gna_accelerator -        0.0/000: GNA Hardware Device not available, using Gna2DeviceVersionSoftwareEmulation.
   NOTE    gna_accelerator -        0.0/000: GNA DMA thread started.
   NOTE    gna_accelerator -        0.0/000: GNA compute thread started.
   NOTE    memory_control  -        0.0/000: Thread started.
   NOTE    dp_dma_int_aggr -        0.0/000: Thread started.
   Running test suite test_semaphore
   ===================================================================
   START - test_k_sem_define
   PASS - test_k_sem_define in 0.1 seconds
   ===================================================================
   START - test_k_sem_init
   PASS - test_k_sem_init in 0.1 seconds
   ===================================================================
   START - test_sem_thread2thread
   PASS - test_sem_thread2thread in 0.1 seconds
   ===================================================================
   START - test_sem_thread2isr
   PASS - test_sem_thread2isr in 0.1 seconds
   ===================================================================
   START - test_sem_reset
   PASS - test_sem_reset in 0.101 seconds
   ===================================================================
   START - test_sem_reset_waiting
   PASS - test_sem_reset_waiting in 0.2 seconds
   ===================================================================
   START - test_sem_count_get
   PASS - test_sem_count_get in 0.1 seconds
   ===================================================================
   START - test_sem_give_from_isr
   PASS - test_sem_give_from_isr in 0.1 seconds
   ===================================================================
   START - test_sem_give_from_thread
   PASS - test_sem_give_from_thread in 0.1 seconds
   ===================================================================
   START - test_sem_take_no_wait
   PASS - test_sem_take_no_wait in 0.1 seconds
   ===================================================================
   START - test_sem_take_no_wait_fails
   PASS - test_sem_take_no_wait_fails in 0.1 seconds
   ===================================================================
   START - test_sem_take_timeout_fails
   PASS - test_sem_take_timeout_fails in 0.501 seconds


Note that startup is slow, taking ~18 seconds on a tyipcal laptop to reach
Zephyr initialization.  And once running, it seems to be 200-400x
slower than the emulated cores.  Be patient, especially with code that
busy waits (timers will warp ahead as long as all the cores reach
idle).

By default there is much output printed to the screen while it works.
You can use "--verbose" to get even more logging from the simulator,
or "--quiet" to suppress everything but the Zephyr logging.

By default, the wrapper script is configured to use prebuilt versions of the
ROM, signing key, simulator and rimage.

Check the --help output, arguments exist to specify either a
zephyr.elf location in a build directory (which must contain the \*.mod
files, not just zephyr.elf) or a pre-signed zephyr.ri file, you can
specify paths to alternate binary verions, etc...

Finally, note that the wrapper script is written to use the
Ubuntu-provided Python 3.8 in /usr/bin and NOT the half-decade-stale
Anaconda python 3.6 you'll find ahead of it on PATH. Don't try to run
it with "python" on the command line or change the #! line to use
/usr/bin/env.


Run tests with twister
======================

We can run one or multiple tests with Twister after exporting the necessary
environment varibles, for example:

.. code-block:: console

   scripts/twister -W -p intel_adsp_ace15_mtpm_sim -T samples/hello_world -vv


GDB access
##########

GDB protocol (the Xtensa variant thereof -- you must use xt-gdb, an
upstream GNU gdb won't work) debugger access to the cores is provided
by the simulator.  At boot, you will see messages emitted that look
like (these can be hard to find in the scrollback, I apologize):

.. code-block:: console

  Core 0 active:(start with "(xt-gdb) target remote :20000")
  Core 1 active:(start with "(xt-gdb) target remote :20001")
  Core 2 active:(start with "(xt-gdb) target remote :20002")

Note that each core is separately managed.  There is no gdb
"threading" support provided, so it's not possible to e.g. trap a
breakpoint on any core, etc...

Simply choose the core you want (almost certainly 0, debugging SMP
code this way is extremely difficult) and connect to it in a different
shell on the container:

.. code-block:: console

   xt-gdb build/zepyr/zephyr.elf
   (xt-gdb) target remote :20000

Note that the core will already have started, so you will see it
stopped in an arbitrary state, likely in the idle thread.  This
probably isn't what you want, so acesim.py provides a
-d/--start-halted option that suppresses the automatic start of the
DSP cores.

Now when gdb connects, the emulated core 0 is halted at the hardware
reset address 0x1ff80000.  You can start the simulator with a
"continue" command, set breakpoints first, etc...

Note that the ROM addresses are part of the ROM binary and not Zephyr,
so the symbol table for early boot will not be available in the
debugger.  As long as the ROM does its job and hands off to Zephyr,
you will be in a safe environment with symbols after a few dozen
instructions.  If you do need to debug the ROM, you can specify it's
ELF file on the command line instead, or use the gdb "file" command to
change the symbol table.

Troubleshooting
################

Here are some possible failures you might encounter for reference:

- Cannot find the C complier

This error can happen as a result of license server issues. You can export
FLEXLM_DIAGNOSTICS=3 to get the detailed server connection log. The incorrect
machine time will cause failure. If the connection still fails, you can try to
clear your caches by deleting the ~/.cache and ~/.ccache directories.

- Cannot find simulator

When West can't find the simulator on the path you gave it, you'll get an error like this:

.. code-block:: console

   ...
   Firmware manifest and signing completed !
   [2/3] cd /home/laurenmu/intel-zephyrproject/zephyr/build && ACESIM-NOTFOUND --rom --sim --rimage /home/laurenmu/intel-zephyrproject/zephyr/build/zephyr/zephyr.ri
   /bin/sh: 1: ACESIM-NOTFOUND: not found
   FAILED: zephyr/CMakeFiles/run_acesim /home/laurenmu/intel-zephyrproject/zephyr/build/zephyr/CMakeFiles/run_acesim
   cd /home/laurenmu/intel-zephyrproject/zephyr/build && ACESIM-NOTFOUND --rom --sim --rimage /home/laurenmu/intel-zephyrproject/zephyr/build/zephyr/zephyr.ri
   ninja: build stopped: subcommand failed.
   FATAL ERROR: command exited with status 1: /usr/bin/cmake --build /home/laurenmu/intel-zephyrproject/zephyr/build --target run

If you've set the path but still get this error, rebuild Zephyr with the
pristine option -p. The Linux environment variable is copied into CMake during
the configuration stage, so if you set or change the environment variable without
redoing the build from scratch, it won't know where the simulator is.

- No Zephyr output message

Sometimes the simulator runs, but hangs after the message "Thread started"
For example:

.. code-block:: console

   NOTE    gna_accelerator -        0.0/000: GNA DMA thread started.
   NOTE    gna_accelerator -        0.0/000: GNA compute thread started.
   NOTE    memory_control  -        0.0/000: Thread started.
   NOTE    dp_dma_int_aggr -        0.0/000: Thread started.

One of the possible reason is the xt-gdb failed to start. You can run
$XTENSA_TOOLCHAIN_PATH/$TOOLCHAIN_VER/XtensaTools/bin/xt-gdb in the
terminal to check. It is likely that the environment variable is
not setting correctly or some shared library using by xt-gdb is missing.

- Zephyr cache issues

Clearing your cache is good for more than just license server issues.
If you're having inexplicable errors, try rebuilding after deleting the
caches:

.. code-block:: console

   rm -rf ~/.ccache ~/.cache/zephyr
