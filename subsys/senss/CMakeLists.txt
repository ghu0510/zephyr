# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_include_directories(.)
zephyr_include_directories_ifdef(CONFIG_SENSS ${ZEPHYR_BASE}/include/zephyr)
zephyr_library_compile_definitions_ifdef(
  CONFIG_NEWLIB_LIBC __LINUX_ERRNO_EXTENSIONS__
  )

if (CONFIG_SENSS)
  zephyr_library_sources(
  sensor_mgmt.c
  runtime.c
  senss_api.c
  )
endif()

add_subdirectory_ifdef(CONFIG_SENSS_SENSOR_PHY_ACC sensor/phy_acc)
