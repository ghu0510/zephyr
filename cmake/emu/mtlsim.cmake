# SPDX-License-Identifier: Apache-2.0

# The MTL_SIM_DIR environment variable specifies the directory
# where the simulator was put, it is $HOME/mtlsim by default
# if MTL_SIM_DIR is not assign.
if(DEFINED ENV{MTL_SIM_DIR})
  set(SIM_DIR $ENV{MTL_SIM_DIR})
endif()

find_program(
  MTLSIM
  PATHS ${SIM_DIR}
  NO_DEFAULT_PATH
  NAMES mtlsim.py
  )

if(DEFINED SIM_DIR)
set(ROM_FILE ${SIM_DIR}/bin/dsp_rom_mtl_sim.hex)
if(NOT EXISTS "${ROM_FILE}")
	message(FATAL_ERROR "Cannot find ROM: ${ROM_FILE} . Abort.")
endif()

set(SIM_WRAP ${SIM_DIR}/dsp_fw_sim.wrapper)
if(NOT EXISTS "${SIM_WRAP}")
  message(FATAL_ERROR "Cannot find simulator wrapper: ${SIM_WRAP} . Abort.")
endif()
endif()

set(MTLSIM_FLAGS
  --rom ${ROM_FILE}
  --sim ${SIM_WRAP}
  --rimage ${APPLICATION_BINARY_DIR}/zephyr/zephyr.ri
  )

add_custom_target(run_mtlsim
  COMMAND
  ${MTLSIM}
  ${MTLSIM_FLAGS}
  WORKING_DIRECTORY ${APPLICATION_BINARY_DIR}
  DEPENDS gen_signed_image
  USES_TERMINAL
  )
