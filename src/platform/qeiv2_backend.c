/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/sensor/hpmicro_qeiv2.h>
#include <errno.h>
#include <miraczv/encoder.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define QEIV2_SPEED_WINDOW_US     20000U
#define QEIV2_SPEED_WINDOW_PULSES 64U
#define QEIV2_DEFAULT_CLOCK_HZ    24000000U

struct qeiv2_encoder_config {
	const struct device *dev;
	uint32_t counts_per_revolution;
	const struct device *clock_dev;
	clock_control_subsys_t clock_subsys;
};

static const struct qeiv2_encoder_config encoder_config[] = {
	{
		.dev = DEVICE_DT_GET(DT_NODELABEL(qei0)),
		.counts_per_revolution = DT_PROP(DT_NODELABEL(qei0), counts_per_revolution),
		.clock_dev = DEVICE_DT_GET(DT_CLOCKS_CTLR(DT_NODELABEL(qei0))),
		.clock_subsys = (clock_control_subsys_t)DT_CLOCKS_CELL(DT_NODELABEL(qei0), name),
	},
	{
		.dev = DEVICE_DT_GET(DT_NODELABEL(qei1)),
		.counts_per_revolution = DT_PROP(DT_NODELABEL(qei1), counts_per_revolution),
		.clock_dev = DEVICE_DT_GET(DT_CLOCKS_CTLR(DT_NODELABEL(qei1))),
		.clock_subsys = (clock_control_subsys_t)DT_CLOCKS_CELL(DT_NODELABEL(qei1), name),
	},
};

static const struct qeiv2_encoder_config *get_config(const struct device *dev)
{
	for (size_t i = 0U; i < ARRAY_SIZE(encoder_config); ++i) {
		if (encoder_config[i].dev == dev) {
			return &encoder_config[i];
		}
	}

	return NULL;
}

static uint32_t speed_window_cycles(const struct qeiv2_encoder_config *config)
{
	uint32_t clock_hz = QEIV2_DEFAULT_CLOCK_HZ;

	if (clock_control_get_rate(config->clock_dev, config->clock_subsys, &clock_hz) != 0) {
		clock_hz = QEIV2_DEFAULT_CLOCK_HZ;
	}

	return (uint32_t)(((uint64_t)clock_hz * QEIV2_SPEED_WINDOW_US) / 1000000U);
}

static int miraczv_qeiv2_encoder_init(const struct device *dev)
{
	const struct qeiv2_encoder_config *config = get_config(dev);

	if ((config == NULL) || (config->counts_per_revolution < 2U)) {
		return -EINVAL;
	}

	return hpm_qeiv2_configure_speed(dev, 0U, QEIV2_SPEED_WINDOW_PULSES,
					 speed_window_cycles(config));
}

int miraczv_qeiv2_encoder_fetch(const struct device *dev, struct miraczv_encoder_data *out)
{
	const struct device *sensor = dev;
	struct hpm_qeiv2_speed_snapshot snapshot;
	struct sensor_value value;
	const struct qeiv2_encoder_config *config = get_config(sensor);
	int ret;

	if ((config == NULL) || (out == NULL)) {
		return -EINVAL;
	}

	ret = sensor_sample_fetch(sensor);
	if (ret != 0) {
		return ret;
	}

	ret = sensor_channel_get(sensor, SENSOR_CHAN_ENCODER_COUNT, &value);
	if (ret != 0) {
		return ret;
	}
	out->count = value.val1;

	ret = sensor_channel_get(sensor, SENSOR_CHAN_ENCODER_REVOLUTIONS, &value);
	if (ret != 0) {
		return ret;
	}
	out->revolutions = value.val1;

	ret = sensor_channel_get(sensor, SENSOR_CHAN_ROTATION, &value);
	if (ret != 0) {
		return ret;
	}
	out->angle_micro_degrees = sensor_value_to_micro(&value);

	out->speed_valid = false;
	out->rpm_milli = 0;
	out->counts_per_second_milli = 0;
	out->timestamp_ms = k_uptime_get();
	if (hpm_qeiv2_get_speed_snapshot(sensor, 0U, &snapshot) == 0 &&
	    snapshot.duration_based_cycles >= speed_window_cycles(config)) {
		int speed_ret = hpm_qeiv2_speed_to_rpm(sensor, 0U, HPM_QEIV2_SPEED_SOURCE_DURATION,
						       &snapshot, &value);

		if (speed_ret == 0) {
			out->rpm_milli = sensor_value_to_milli(&value);
			out->counts_per_second_milli = (int32_t)(((int64_t)out->rpm_milli *
								  config->counts_per_revolution) /
								 60);
			out->speed_valid = true;
		} else if (snapshot.duration_based_pulses == 0U) {
			out->speed_valid = true;
		} else if (snapshot.pulse_based_cycles > 0U &&
			   hpm_qeiv2_speed_to_rpm(sensor, 0U, HPM_QEIV2_SPEED_SOURCE_PULSE,
						  &snapshot, &value) == 0) {
			out->rpm_milli = sensor_value_to_milli(&value);
			out->counts_per_second_milli = (int32_t)(((int64_t)out->rpm_milli *
								  config->counts_per_revolution) /
								 60);
			out->speed_valid = true;
		}
	}

	return 0;
}

const struct miraczv_encoder_ops miraczv_qeiv2_encoder_ops = {
	.init = miraczv_qeiv2_encoder_init,
	.sample_fetch = miraczv_qeiv2_encoder_fetch,
};
