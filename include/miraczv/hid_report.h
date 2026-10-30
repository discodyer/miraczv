/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIRACZV_HID_REPORT_H_
#define MIRACZV_HID_REPORT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MIRACZV_HID_AXIS_COUNT 4U

/** Parsed gamepad input report fields. */
struct miraczv_hid_report {
	uint16_t offset[MIRACZV_HID_AXIS_COUNT];
	uint8_t bits[MIRACZV_HID_AXIS_COUNT];
	uint8_t axis_count;
	uint32_t usage[MIRACZV_HID_AXIS_COUNT];
	bool is_signed[MIRACZV_HID_AXIS_COUNT];
	int64_t minimum[MIRACZV_HID_AXIS_COUNT];
	int64_t maximum[MIRACZV_HID_AXIS_COUNT];
	uint8_t reports[MIRACZV_HID_AXIS_COUNT];
	bool parsed;
};

int miraczv_hid_report_parse(const uint8_t *descriptor, size_t length,
			     struct miraczv_hid_report *report);
int miraczv_hid_report_axis(const struct miraczv_hid_report *report, uint8_t index,
			    const uint8_t *data, size_t length, int32_t *value);

#endif /* MIRACZV_HID_REPORT_H_ */
