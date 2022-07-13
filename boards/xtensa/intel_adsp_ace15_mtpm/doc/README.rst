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


Ideally, you will want to build and run the simulator on your host. Building on
the host using XCC is currently supported and documented below, however, running
in the simulator is currently working in the docker envirnment.

.. note::

   Support and documentation for running the firmware image on the host using the
   simulator is work in progress.

Toolchain Setup on Host
=======================

No special instructions are required to build Zephyr using the Zephyr
SDK.  West works like it does for any other platform.

If you wish to use the Cadence toolchain instead of GCC, you will need
install the XCC toolchain for this core. At least 2 files are needed:

- ace10_LX7HiFi4_linux.tgz
- XtensaTools_RG_2019_12_linux.tgz

With the above tools package and the DSP Build Configuration package,
the toolchain can be setup as follows.


.. code-block:: console

   # Create Xtensa install root
   mkdir -p ~/xtensa/install/tools
   mkdir -p ~/xtensa/install/builds
   # Set up the configuration-independent Xtensa Tool:
   tar zxvf XtensaTools_RG_2019_12_linux.tgz -C ~/xtensa/install/tools
   # Set up the configuration-specific core files:
   tar zxvf ace10_LX7HiFi4_linux.tgz -C ~/xtensa/install/builds
   # Install the Xtensa development toolchain:
   cd ~/xtensa/install
   ./builds/RG-2019.12-linux/ace10_LX7HiFi4/install \
   --xtensa-tools ./tools/RG-2019.12-linux/XtensaTools/ \
   --registry  ./tools/RG-2019.12-linux/XtensaTools/config/

You might need to install some 32-bit libraries to run this command and some of
the binaries included in the toolchain. Install all needed 32 bit packages:

.. code-block:: console

   sudo dpkg --add-architecture i386
   sudo apt-get install libncurses5:i386 zlib1g:i386 libcrypt1:i386

Once installed, you are ready to build and run a zephyr application for this hardware
using the Cadence XCC compiler and the software simulator.

A few environment variables are needed to tell Zephyr where the toolchain is:

.. code-block:: console

   export ZEPHYR_TOOLCHAIN_VARIANT=xcc

   export XTENSA_TOOLCHAIN_PATH=$HOME/xtensa/install/tools
   export TOOLCHAIN_VER=RG-2019.12-linux

Additionally, you might need to define the license server for XCC, this can be
setup using the environment variable `XTENSAD_LICENSE_FILE`


Using Docker
============

Host Setup
----------

By convention, these instructions group all shared files between the
container and host in a single directory.  For example, if $HOME/zephyrproject
is what you have on the host, it is mounted as /z in the container.

Docker Container Setup
----------------------

The audio team maintains a docker image sufficient to build and run
all needed Meteor Lake DSP (ACE) simulator and firmware tools, including Zephyr.  Pull
it with:

.. code-block:: console

    docker pull ger-registry.caas.intel.com/ace-devel/std_sim_mtl

The first clone is slow (coming cross-continent over the IT network).
You'll want to re-pull this regularly as it seems they like to update
it, but that only requires deltas and not the base OS image.

Run it with:

.. code-block:: console

   docker run --name ace_sim -d -i -t \
      --mount type=bind,source=$HOME/z,target=/z \
      ger-registry.caas.intel.com/ace-devel/std_sim_mtl

Open shells in the container with (instead of /bin/bash, you can just
run tools directly from the host too):

.. code-block:: console

   docker exec -it ace_sim /bin/bash

Note that this runs the shell as root.  The image inexplicably lacks
an account with uid=1000 to use for building (i.e. one that matches
the default host user account) and everything in the images expects you to be
running as root.

If for any reason you need to start over with a clean container image,
you can delete the ace_sim container with:

.. code-block:: console

   docker stop ace_sim
   docker rm ace_sim

Finally, we need west in the docker container so we can build
Zephyr. (Note the python interpreter on PATH is a custom installed 3.6
variant and not the distro one, but it works fine.)

.. code-block:: console

   pip3 install west


Toolchain Setup inside Docker
-----------------------------

Inside the docker image, the follwoing environment variables are required:


.. code-block:: console

   export ZEPHYR_TOOLCHAIN_VARIANT=xcc

   export XTENSA_CORE=ace10_LX7HiFi4
   export XTENSA_TOOLS_VERSION=RG-2019.12-linux
   export XTENSA_INSTALL_PATH=/root/xtensa
   export XTENSA_TOOLS_DIR=$XTENSA_INSTALL_PATH/XtDevTools/install/tools
   export XTENSA_TOOLS=$XTENSA_INSTALL_PATH/XtDevTools/install/tools/$XTENSA_TOOLS_VERSION/XtensaTools
   export XTENSA_TOOLCHAIN_PATH=$XTENSA_TOOLS_DIR/$XTENSA_TOOLS_VERSION
   export XTENSA_BUILDS_DIR=/root/xtensa/XtDevTools/install/builds
   export XTENSA_SYSTEM=$XTENSA_BUILDS_DIR/$XTENSA_TOOLS_VERSION/$XTENSA_CORE/config


Building a Zephyr Application
#############################

The board name is "intel_adsp_ace15_mtpm_sim" and is maintained in the `zephyr-intel` tree which is
dedicated for internal platforms and embargoed code.

The board would be available for development as any other upstream board. You
can either build applications in main Zephyr tree or in the `zephyr-intel` code
base.

.. code-block:: console

   west build -p auto -b intel_adsp_ace15_mtpm_sim samples/hello_world


Run in the Simulator
####################

Invocation of the simulator itself is somewhat involved, so the
details are now handled by a wrapper script (mtlsim.py) which is
integrated as a zephyr native emulator.

After build with west, call

.. code-block:: console

   ninja -C build run

You can also build and run in one single command::

   west build -p auto -b intel_adsp_ace15_mtpm_sim samples/hello_world -t run

This is a typical output after running the flash command:

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
probably isn't what you want, so mtlsim.py provides a
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

Building Rimage
###############

The included binary should be good enough, but if you need to track
upstream changes, the SOF rimage tool is available from public github.
Build it in your host environment, not the docker:

.. code-block:: console

   git clone https://github.com/thesofproject/rimage
   cd rimage
   git submodule init
   git submodule update
   cmake .
   make
   cp ./rimage /z/zephyr-ace #FIXME

If you do need to make changes to rimage, please make sure to tell
Andy so the prebuilt binary gets updated!

Building the simulator
######################

The DSP simulator itself can be built from scratch in the container.
The source code from the audio team is on the internal gitlab:

.. code-block:: console

   git clone https://gitlab.devtools.intel.com/audio_tools/std_sim.git

The tool is itself a C++ program linked against libraries in the
Xtensa SDK.  It's a straightforward build in the container:

.. code-block:: console

  cd /z/std_sim
  source ./scripts/linux/mtl_config.sh
  ./scripts/linux/build_mtl_sim.sh

Likewise, if you do need to make changes to the simulator, please make
sure to tell Andy so the prebuilt binary gets updated!

And for anyone (including Andy) interested in updating the prebuilt:
There are three files to copy (dsp_fw_sim, libgna.so.2 and
intel_dsp/intel_dsp.so -- yes, the extra directory in the path
matters, that's how it's linked).  And note that C++ debug info is
extremely large.  Remember to strip the binaries before committing!

Building the ROM image
######################

This is the boot ROM for the device, built from audio team code that
we don't touch.  The source code is on a audio team git server in
Europe, which requires the "cAVS_FW_ro" permission in AGS to access.

.. code-block:: console

   git clone -b ace git@repos.igk.intel.com:cavs_fw

The build itself is, like the simulator, trivial to do with a single
script in the container:

.. code-block:: console

   cd /z/cavs_fw/builder
   ./build.sh -e buildenvs/buildenv_mtl.sh \
       -e buildenvs/buildenv_sim_rom.sh \
       -e buildenvs/buildenv_local.sh
   cp /z/cavs_fw/artifacts/FW/bin/mtl/rom/sim/dsp_rom_mtl_sim.hex /z/zephyr-ace #FIXME
