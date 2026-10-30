# Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/module
)

list(APPEND DTS_ROOT
  ${CMAKE_CURRENT_SOURCE_DIR}/module
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(MiracZV)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

target_sources(app PRIVATE
  src/main.c
  src/control/controller.c
  src/control/pid.c
  src/control/differential_drive.c
)

if(CONFIG_MIRACZV_MOTOR_TB6612)
  target_sources(app PRIVATE src/platform/tb6612.c)
endif()

if(CONFIG_MIRACZV_ENCODER)
  target_sources(app PRIVATE src/platform/qeiv2_backend.c)
endif()

if(CONFIG_MIRACZV_HID_HOST)
  target_sources(app PRIVATE
    src/platform/hid_controller.c
    src/platform/hid_report.c
  )
  target_include_directories(app PRIVATE ${ZEPHYR_BASE}/subsys/usb/host)
endif()
