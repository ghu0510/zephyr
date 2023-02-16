# SPDX-License-Identifier: Apache-2.0

zephyr_library()

if (CONFIG_SENSS)
  zephyr_library_sources(
  sensor_mgmt.c
  runtime.c
  senss_api.c
  )
endif()

add_subdirectory_ifdef(CONFIG_SENSS_SENSOR_PHY_3D_SENSOR sensor/phy_3d_sensor)
add_subdirectory_ifdef(CONFIG_SENSS_SENSOR_MOTION_DETECTOR sensor/motion_detector)
add_subdirectory_ifdef(CONFIG_SENSS_SENSOR_HINGE_ANGLE sensor/hinge_angle)
