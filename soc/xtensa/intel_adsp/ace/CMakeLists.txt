# Intel ACE SoC family CMake file
#
# Copyright (c) 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0


zephyr_library_sources(
  sram.c
  multiprocessing.c
  irq.c
  power_down.S
  power.c
  boot.c
  )

zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_COMM_WIDGET comm_widget.c)
zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_COMM_WIDGET comm_widget_messages.c)

if (CONFIG_XTENSA_MMU)
  zephyr_library_sources_ifdef(CONFIG_SOC_INTEL_ACE30_PTL mmu_ptl.c)
endif()

if (NOT CONFIG_SOF AND NOT CONFIG_INTEL_ADSP_SIM)
  # SOF does their own firmware ready message so
  # there is no need to do it on the Zephyr side.
  #
  # And simulator does not understand this message
  # and will be confused. So skip it on simulator.
  zephyr_library_sources(fw_ready.c)
endif()
