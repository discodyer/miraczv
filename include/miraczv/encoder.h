/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIRACZV_ENCODER_H_
#define MIRACZV_ENCODER_H_

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generic encoder measurement data.
 *
 * The position fields are signed and referenced to an arbitrary startup
 * zero when the backend has no index calibration. The speed fields are valid
 * only when @ref speed_valid is true.
 */
struct miraczv_encoder_data {
	/** Raw signed phase count within one revolution. */
	int32_t count;
	/** Signed number of complete revolutions. */
	int32_t revolutions;
	/** Signed position angle in microdegrees. */
	int64_t angle_micro_degrees;
	/** Signed speed in milli-RPM. */
	int32_t rpm_milli;
	/** Signed speed in milli-counts per second. */
	int32_t counts_per_second_milli;
	/** Measurement timestamp in milliseconds. */
	int64_t timestamp_ms;
	/** True when the speed field is a complete, valid measurement. */
	bool speed_valid;
};

/** Generic encoder operations implemented by an encoder backend. */
struct miraczv_encoder_ops {
	int (*init)(const struct device *dev);
	int (*sample_fetch)(const struct device *dev, struct miraczv_encoder_data *data);
};

/** Generic encoder handle. */
struct miraczv_encoder {
	const struct device *dev;
	const struct miraczv_encoder_ops *ops;
	/** Mechanical counts per output-shaft revolution. */
	uint32_t counts_per_revolution;
	/** Invert the reported direction and position. */
	bool invert;
};

/**
 * @brief Initialize an encoder backend.
 *
 * @param encoder Encoder handle.
 *
 * @retval 0 on success or if the backend has no init operation.
 */
static inline int miraczv_encoder_init(const struct miraczv_encoder *encoder)
{
	if ((encoder == NULL) || (encoder->ops == NULL)) {
		return -EINVAL;
	}

	return encoder->ops->init != NULL ? encoder->ops->init(encoder->dev) : 0;
}

/**
 * @brief Fetch one coherent encoder measurement.
 *
 * @param encoder Encoder handle.
 * @param data Output measurement.
 *
 * @retval 0 on success.
 * @retval -EINVAL for a null argument.
 */
static inline int miraczv_encoder_sample_fetch(const struct miraczv_encoder *encoder,
					       struct miraczv_encoder_data *data)
{
	int ret;

	if ((encoder == NULL) || (encoder->ops == NULL) || (data == NULL)) {
		return -EINVAL;
	}

	ret = encoder->ops->sample_fetch(encoder->dev, data);
	if ((ret == 0) && encoder->invert) {
		data->count = -data->count;
		data->revolutions = -data->revolutions;
		data->angle_micro_degrees = -data->angle_micro_degrees;
		data->rpm_milli = -data->rpm_milli;
		data->counts_per_second_milli = -data->counts_per_second_milli;
	}

	return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* MIRACZV_ENCODER_H_ */
