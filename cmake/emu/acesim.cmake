# SPDX-License-Identifier: Apache-2.0

# The ACE_SIM_DIR environment variable specifies the directory
# where the simulator was put, it is $HOME/acesim by default
# if ACE_SIM_DIR is not assign.
if(DEFINED ENV{ACE_SIM_DIR})
  set(SIM_DIR $ENV{ACE_SIM_DIR})
endif()

find_program(
  ACESIM
  PATHS ${SIM_DIR}
  NO_DEFAULT_PATH
  NAMES acesim.py
  )

set(ACESIM_FLAGS
  --soc ${CONFIG_SOC}
  --rimage ${APPLICATION_BINARY_DIR}/zephyr/zephyr.ri
  --gdb ${CMAKE_GDB}
  --trace
  )

add_custom_target(run_acesim
  COMMAND
  ${ACESIM}
  ${ACESIM_FLAGS}
  WORKING_DIRECTORY ${APPLICATION_BINARY_DIR}
  DEPENDS gen_signed_image
  USES_TERMINAL
  )
