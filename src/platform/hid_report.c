/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <miraczv/hid_report.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/hid.h>

struct hid_global_state {
	uint32_t usage_page;
	uint32_t report_size;
	uint32_t report_count;
	uint32_t report_id;
	int64_t logical_min;
	int64_t logical_max;
	bool logical_min_negative;
};

struct hid_local_state {
	uint32_t usage[MIRACZV_HID_AXIS_COUNT];
	size_t usage_count;
	uint32_t usage_min;
	uint32_t usage_max;
	bool has_usage_range;
};

static uint32_t item_unsigned(const uint8_t *data, size_t size)
{
	uint32_t value = 0U;

	for (size_t i = 0U; i < size; ++i) {
		value |= (uint32_t)data[i] << (8U * i);
	}
	return value;
}

static int64_t item_signed(const uint8_t *data, size_t size)
{
	uint32_t value = item_unsigned(data, size);

	if (size == 0U) {
		return 0;
	}
	if ((value & (1U << ((8U * size) - 1U))) != 0U && (size < 4U)) {
		value |= UINT32_MAX << (8U * size);
	}
	return (int32_t)value;
}

static bool is_axis_usage(uint32_t usage_page, uint32_t usage)
{
	return (usage_page == HID_USAGE_GEN_DESKTOP) && (usage >= MIRACZV_HID_USAGE_DESKTOP_X) &&
	       (usage <= MIRACZV_HID_USAGE_DESKTOP_WHEEL);
}

static void add_axis(struct miraczv_hid_report *report, uint16_t bit_offset, uint8_t bits,
		     uint32_t usage, bool is_signed, int64_t minimum, int64_t maximum,
		     uint8_t report_id, size_t *axis_count)
{
	size_t index = *axis_count;

	if (index >= MIRACZV_HID_AXIS_COUNT) {
		return;
	}
	report->bits[index] = bits;
	report->usage[index] = usage;
	report->is_signed[index] = is_signed;
	report->minimum[index] = minimum;
	report->maximum[index] = maximum;
	report->reports[index] = report_id;
	report->offset[index] = bit_offset;
	(*axis_count)++;
}

int miraczv_hid_report_parse(const uint8_t *descriptor, size_t length,
			     struct miraczv_hid_report *report)
{
	struct hid_global_state global = {0};
	struct hid_local_state local = {0};
	size_t offset = 0U;
	size_t axis_count = 0U;
	uint32_t bit_position = 0U;
	uint32_t previous_report_id = 0U;
	bool report_id_seen = false;

	if ((descriptor == NULL) || (report == NULL)) {
		return -EINVAL;
	}
	memset(report, 0, sizeof(*report));

	while (offset < length) {
		uint8_t prefix = descriptor[offset++];
		uint8_t size;
		uint8_t type;
		uint8_t tag;
		uint8_t value_size;

		if (prefix == 0xFEU) {
			if (((size_t)offset + 2U) > length) {
				return -EINVAL;
			}
			value_size = descriptor[offset];
			if (((size_t)offset + 2U + value_size) > length) {
				return -EINVAL;
			}
			offset += 2U + value_size;
			continue;
		}

		size = prefix & 0x03U;
		value_size = size == 3U ? 4U : size;
		if (((size_t)offset + value_size) > length) {
			return -EINVAL;
		}
		type = (prefix >> 2U) & 0x03U;
		tag = (prefix >> 4U) & 0x0FU;

		if (type == HID_ITEM_TYPE_GLOBAL) {
			switch (tag) {
			case 0x0U: /* Usage Page */
				global.usage_page = item_unsigned(&descriptor[offset], value_size);
				break;
			case HID_ITEM_TAG_LOGICAL_MIN:
				global.logical_min = item_signed(&descriptor[offset], value_size);
				global.logical_min_negative = global.logical_min < 0;
				break;
			case HID_ITEM_TAG_LOGICAL_MAX:
				global.logical_max =
					global.logical_min_negative
						? item_signed(&descriptor[offset], value_size)
						: item_unsigned(&descriptor[offset], value_size);
				break;
			case HID_ITEM_TAG_REPORT_SIZE:
				global.report_size = item_unsigned(&descriptor[offset], value_size);
				break;
			case HID_ITEM_TAG_REPORT_ID:
				global.report_id = item_unsigned(&descriptor[offset], value_size);
				if (!report_id_seen || (global.report_id != previous_report_id)) {
					bit_position = 0U;
					previous_report_id = global.report_id;
					report_id_seen = true;
				}
				break;
			case HID_ITEM_TAG_REPORT_COUNT:
				global.report_count =
					item_unsigned(&descriptor[offset], value_size);
				break;
			default:
				break;
			}
		} else if (type == HID_ITEM_TYPE_LOCAL) {
			uint32_t usage = item_unsigned(&descriptor[offset], value_size);

			if ((tag == HID_ITEM_TAG_USAGE) &&
			    (local.usage_count < MIRACZV_HID_AXIS_COUNT)) {
				local.usage[local.usage_count++] = usage;
			} else if (tag == HID_ITEM_TAG_USAGE_MIN) {
				local.usage_min = usage;
				local.has_usage_range = true;
			} else if (tag == HID_ITEM_TAG_USAGE_MAX) {
				local.usage_max = usage;
			}
		} else if (type == HID_ITEM_TYPE_MAIN) {
			if ((tag == HID_ITEM_TAG_INPUT) && (global.report_size > 0U) &&
			    (global.report_size <= 32U)) {
				uint64_t next_bit =
					(uint64_t)bit_position +
					((uint64_t)global.report_count * global.report_size);

				if (next_bit > UINT16_MAX) {
					return -ERANGE;
				}
				for (uint32_t i = 0U; i < global.report_count; ++i) {
					uint32_t usage = 0U;
					uint16_t bit = bit_position + (i * global.report_size);
					uint8_t axis_bits = MIN(global.report_size, 32U);

					if (i < local.usage_count) {
						usage = local.usage[i];
					} else if (local.has_usage_range &&
						   (local.usage_min + i <= local.usage_max)) {
						usage = local.usage_min + i;
					}
					if (is_axis_usage(global.usage_page, usage)) {
						add_axis(report, bit, axis_bits, usage,
							 global.logical_min_negative,
							 global.logical_min, global.logical_max,
							 global.report_id, &axis_count);
					}
				}
				bit_position = (uint32_t)next_bit;
			}
			memset(&local, 0, sizeof(local));
		}
		offset += value_size;
	}

	report->axis_count = axis_count;
	report->parsed = axis_count > 0U;
	return report->parsed ? 0 : -ENOENT;
}

int miraczv_hid_report_find_axis(const struct miraczv_hid_report *report, uint32_t usage)
{
	if ((report == NULL) || !report->parsed) {
		return -EINVAL;
	}

	for (uint8_t i = 0U; i < report->axis_count; ++i) {
		if (report->usage[i] == usage) {
			return i;
		}
	}

	return -ENOENT;
}

int miraczv_hid_report_axis(const struct miraczv_hid_report *report, uint8_t index,
			    const uint8_t *data, size_t length, int32_t *value)
{
	uint32_t raw = 0U;
	size_t byte_offset;

	if ((report == NULL) || (data == NULL) || (value == NULL) || !report->parsed ||
	    (index >= report->axis_count)) {
		return -EINVAL;
	}
	if (report->reports[index] != 0U && (length == 0U || data[0] != report->reports[index])) {
		return -ENOENT;
	}
	if (report->reports[index] != 0U) {
		data++;
		length--;
	}
	byte_offset = report->offset[index] / 8U;
	if ((byte_offset + ((report->offset[index] % 8U) + report->bits[index] + 7U) / 8U) >
	    length) {
		return -ENODATA;
	}
	for (uint8_t bit = 0U; bit < report->bits[index]; ++bit) {
		size_t source_bit = report->offset[index] + bit;

		raw |= ((data[source_bit / 8U] >> (source_bit % 8U)) & 1U) << bit;
	}
	if (report->is_signed[index] && (report->bits[index] < 32U) &&
	    (raw & (1U << (report->bits[index] - 1U))) != 0U) {
		raw |= UINT32_MAX << report->bits[index];
	}
	*value = (int32_t)raw;
	return 0;
}
