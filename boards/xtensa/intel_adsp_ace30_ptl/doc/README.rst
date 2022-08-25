Intel DSP Panther Lake Simulator
*******************************

Environment Setup
#################

Start with a Zephyr-capable build host on the Intel network (VPN is
fine as long as it's up).

The Zephyr tree is best cloned via west.

.. code-block:: console

   mkdir ~/zephyrproject
   cd ~/zephyrproject
   west init -m https://github.com/intel-innersource/os.rtos.zephyr.zephyr
   west update


Toolchain Setup on Host
=======================


If you wish to use the Cadence toolchain instead of GCC, you will need
install the XCC toolchain for this core. At least 2 files are needed:

- ace30_LX7HiFi4_linux.tgz
- XtensaTools_RG_2021_8_linux.tgz

With the above tools package and the DSP Build Configuration package,
the toolchain can be setup as follows.


.. code-block:: console

   # Create Xtensa install root
   mkdir -p ~/xtensa/install/tools
   mkdir -p ~/xtensa/install/builds
   # Set up the configuration-independent Xtensa Tool:
   tar zxvf XtensaTools_RG_2021_8_linux.tgz -C ~/xtensa/install/tools
   # Set up the configuration-specific core files:
   tar zxvf ace30_LX7HiFi4_linux.tgz -C ~/xtensa/install/builds
   # Install the Xtensa development toolchain:
   cd ~/xtensa/install
   ./builds/RG-2021.8-linux/ace30_LX7HiFi4/install \
   --xtensa-tools ./tools/RG-2021.8-linux/XtensaTools/ \
   --registry  ./tools/RG-2021.8-linux/XtensaTools/config/

You might need to install some 32-bit libraries to run this command and some of
the binaries included in the toolchain. Install all needed 32 bit packages:

.. code-block:: console

   sudo dpkg --add-architecture i386
   sudo apt-get install libncurses5:i386 zlib1g:i386 libcrypt1:i386

Once installed, you are ready to build and run a zephyr application for this hardware
using the Cadence XCC compiler and the software simulator.

A few environment variables are needed to tell Zephyr where the toolchain is:

.. code-block:: console

   export ZEPHYR_TOOLCHAIN_VARIANT=xcc-clang

   export XTENSA_TOOLS_VERSION=RG-2021.8-linux
   export XTENSA_TOOLS_DIR=$HOME/xtensa/install/tools
   export XTENSA_TOOLCHAIN_PATH=$XTENSA_TOOLS_DIR/$XTENSA_TOOLS_VERSION

Additionally, you might need to define the license server for XCC, this can be
setup using the environment variable `XTENSAD_LICENSE_FILE`

Building a Zephyr Application
#############################

The board name is "intel_adsp_ace30_ptl_sim" and is maintained in zephyr internal since
it is an embargoed code.

The board would be available for development as any other upstream board.

.. code-block:: console

   west build -p auto -b intel_adsp_ace30_ptl_sim samples/hello_world


Run in the Simulator
####################

Invocation of the simulator itself is somewhat involved, so the
details are now handled by a wrapper script (mtlsim.py) which is
integrated as a zephyr native emulator.

After build with west, call

.. code-block:: console

   ninja -C build run

You can also build and run in one single command::

   west build -p auto -b intel_adsp_ace30_ptl_sim samples/hello_world -t run


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
reset address 0x3ff80000.  You can start the simulator with a
"continue" command, set breakpoints first, etc...

Note that the ROM addresses are part of the ROM binary and not Zephyr,
so the symbol table for early boot will not be available in the
debugger.  As long as the ROM does its job and hands off to Zephyr,
you will be in a safe environment with symbols after a few dozen
instructions.  If you do need to debug the ROM, you can specify it's
ELF file on the command line instead, or use the gdb "file" command to
change the symbol table.
