# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(senss)

if (CONFIG_SENSS)
  if (CONFIG_CHRE)
    zephyr_library_sources(
      src/main.cpp
      src/nanoapps/sensor_world.cpp
    )
  endif()
endif()
