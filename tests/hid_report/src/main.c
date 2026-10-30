/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <miraczv/hid_report.h>
#include <stdint.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/ztest.h>

static const uint8_t gamepad_descriptor[] = {
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_GAMEPAD),
	HID_COLLECTION(HID_COLLECTION_APPLICATION),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_POINTER),
	HID_COLLECTION(HID_COLLECTION_PHYSICAL),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(2),
	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(255),
	HID_INPUT(0x02),
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_X),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),
	HID_LOGICAL_MIN16(0x00, 0x80),
	HID_LOGICAL_MAX16(0xff, 0x7f),
	HID_REPORT_SIZE(16),
	HID_REPORT_COUNT(2),
	HID_INPUT(0x02),
	HID_END_COLLECTION,
	HID_END_COLLECTION,
};

static const uint8_t report_id_descriptor[] = {
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_GAMEPAD),
	HID_COLLECTION(HID_COLLECTION_APPLICATION),
	HID_REPORT_ID(7),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(1),
	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(255),
	HID_INPUT(0x02),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_X),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),
	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(255),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(2),
	HID_INPUT(0x02),
	HID_END_COLLECTION,
};

static const uint8_t right_stick_descriptor[] = {
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_GAMEPAD),
	HID_COLLECTION(HID_COLLECTION_APPLICATION),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(4),
	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(255),
	HID_USAGE(MIRACZV_HID_USAGE_DESKTOP_Z),
	HID_USAGE(MIRACZV_HID_USAGE_DESKTOP_RZ),
	HID_USAGE(MIRACZV_HID_USAGE_DESKTOP_RX),
	HID_USAGE(MIRACZV_HID_USAGE_DESKTOP_RY),
	HID_INPUT(0x02),
	HID_END_COLLECTION,
};

static const uint8_t gamepad_8bit_descriptor[] = {
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_GAMEPAD),
	HID_COLLECTION(HID_COLLECTION_APPLICATION),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_POINTER),
	HID_COLLECTION(HID_COLLECTION_PHYSICAL),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(2),
	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(255),
	HID_INPUT(0x02),
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_X),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),
	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(255),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(2),
	HID_INPUT(0x02),
	HID_END_COLLECTION,
	HID_END_COLLECTION,
};

ZTEST(miraczv_hid_report, test_finds_rotational_axes)
{
	struct miraczv_hid_report report;
	const uint8_t input[4] = {0x40, 0xc0, 0x20, 0xe0};
	int32_t value;

	zassert_ok(miraczv_hid_report_parse(right_stick_descriptor, sizeof(right_stick_descriptor),
					    &report));
	zassert_equal(report.axis_count, 4);
	zassert_equal(miraczv_hid_report_find_axis(&report, MIRACZV_HID_USAGE_DESKTOP_Z), 0);
	zassert_equal(miraczv_hid_report_find_axis(&report, MIRACZV_HID_USAGE_DESKTOP_RZ), 1);
	zassert_equal(miraczv_hid_report_find_axis(&report, MIRACZV_HID_USAGE_DESKTOP_RX), 2);
	zassert_equal(miraczv_hid_report_find_axis(&report, MIRACZV_HID_USAGE_DESKTOP_RY), 3);
	zassert_equal(miraczv_hid_report_find_axis(&report, MIRACZV_HID_USAGE_DESKTOP_X), -ENOENT);
	zassert_ok(miraczv_hid_report_axis(&report, 1, input, sizeof(input), &value));
	zassert_equal(value, 0xc0);
}

ZTEST(miraczv_hid_report, test_gamepad_8bit_centered_axes)
{
	struct miraczv_hid_report report;
	const uint8_t input[5] = {0x00, 0x00, 0x80, 0x80, 0x00};
	int32_t x;
	int32_t y;

	zassert_ok(miraczv_hid_report_parse(gamepad_8bit_descriptor,
					    sizeof(gamepad_8bit_descriptor), &report));
	zassert_equal(report.axis_count, 2);
	zassert_equal(report.minimum[0], 0);
	zassert_equal(report.maximum[0], 255);
	zassert_ok(miraczv_hid_report_axis(&report, 0, input, sizeof(input), &x));
	zassert_ok(miraczv_hid_report_axis(&report, 1, input, sizeof(input), &y));
	zassert_equal(x, 128);
	zassert_equal(y, 128);
}

ZTEST(miraczv_hid_report, test_no_report_id_signed_axes)
{
	struct miraczv_hid_report report;
	const uint8_t input[6] = {0x00, 0x00, 0x00, 0x80, 0xff, 0x7f};
	int32_t x;
	int32_t y;

	zassert_ok(
		miraczv_hid_report_parse(gamepad_descriptor, sizeof(gamepad_descriptor), &report));
	zassert_equal(report.axis_count, 2);
	zassert_equal(report.usage[0], HID_USAGE_GEN_DESKTOP_X);
	zassert_equal(report.usage[1], HID_USAGE_GEN_DESKTOP_Y);
	zassert_true(report.is_signed[0]);
	zassert_true(report.is_signed[1]);
	zassert_equal(report.minimum[0], INT16_MIN);
	zassert_equal(report.maximum[0], INT16_MAX);
	zassert_ok(miraczv_hid_report_axis(&report, 0, input, sizeof(input), &x));
	zassert_ok(miraczv_hid_report_axis(&report, 1, input, sizeof(input), &y));
	zassert_equal(x, INT16_MIN);
	zassert_equal(y, INT16_MAX);
}

ZTEST(miraczv_hid_report, test_report_id_axis_offsets)
{
	struct miraczv_hid_report report;
	const uint8_t report_id_7[4] = {7, 0, 0x22, 0xdd};
	const uint8_t wrong_id[4] = {6, 0, 0x22, 0xdd};
	int32_t value;

	zassert_ok(miraczv_hid_report_parse(report_id_descriptor, sizeof(report_id_descriptor),
					    &report));
	zassert_equal(report.axis_count, 2);
	zassert_equal(report.reports[0], 7);
	zassert_ok(miraczv_hid_report_axis(&report, 0, report_id_7, sizeof(report_id_7), &value));
	zassert_equal(value, 0x22);
	zassert_ok(miraczv_hid_report_axis(&report, 1, report_id_7, sizeof(report_id_7), &value));
	zassert_equal(value, 0xdd);
	zassert_equal(miraczv_hid_report_axis(&report, 0, wrong_id, sizeof(wrong_id), &value),
		      -ENOENT);
}

ZTEST(miraczv_hid_report, test_rejects_invalid_or_short_input)
{
	struct miraczv_hid_report report;
	struct miraczv_hid_report empty_report = {0};
	const uint8_t input[] = {0, 0};
	int32_t value;

	zassert_equal(miraczv_hid_report_parse(gamepad_descriptor, 3, &report), -EINVAL);
	zassert_equal(miraczv_hid_report_axis(&empty_report, 0, input, sizeof(input), &value),
		      -EINVAL);
	zassert_ok(
		miraczv_hid_report_parse(gamepad_descriptor, sizeof(gamepad_descriptor), &report));
	zassert_equal(miraczv_hid_report_axis(&report, 0, input, sizeof(input), NULL), -EINVAL);
	zassert_equal(miraczv_hid_report_axis(&report, 0, input, sizeof(input), &value), -ENODATA);
	zassert_equal(
		miraczv_hid_report_axis(&report, report.axis_count, input, sizeof(input), &value),
		-EINVAL);
}

ZTEST_SUITE(miraczv_hid_report, NULL, NULL, NULL, NULL, NULL);
