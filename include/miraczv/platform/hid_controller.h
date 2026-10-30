/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIRACZV_PLATFORM_HID_CONTROLLER_H_
#define MIRACZV_PLATFORM_HID_CONTROLLER_H_

#include <zephyr/device.h>
#include <miraczv/control/controller.h>

/** USB HID host controller source. */
extern const struct miraczv_controller_ops miraczv_hid_controller_ops;

#endif /* MIRACZV_PLATFORM_HID_CONTROLLER_H_ */
