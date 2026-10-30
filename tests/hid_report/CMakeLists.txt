# Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
project(miraczv_hid_report_test)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

target_sources(app PRIVATE
  src/main.c
  ../../src/platform/hid_report.c
)

target_include_directories(app PRIVATE ../../include)
