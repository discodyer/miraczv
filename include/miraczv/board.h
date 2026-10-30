/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIRACZV_BOARD_H_
#define MIRACZV_BOARD_H_

#include <miraczv/encoder.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

/** Logical wheel indices for the differential chassis. */
enum miraczv_wheel {
	MIRACZV_WHEEL_LEFT = 0,
	MIRACZV_WHEEL_RIGHT,
	MIRACZV_WHEEL_COUNT,
};

/** Motor wiring for one TB6612 channel. */
struct miraczv_motor {
	struct pwm_dt_spec pwm;
	struct gpio_dt_spec in1;
	struct gpio_dt_spec in2;
	bool invert;
};

/** Complete differential chassis wiring. */
struct miraczv_chassis_config {
	struct miraczv_motor motor[MIRACZV_WHEEL_COUNT];
	struct gpio_dt_spec standby;
	struct miraczv_encoder encoder[MIRACZV_WHEEL_COUNT];
};

/* QEIV2 encoder backend exported by drivers/encoder/qeiv2_backend.c. */
extern const struct miraczv_encoder_ops miraczv_qeiv2_encoder_ops;

int miraczv_chassis_init(const struct miraczv_chassis_config *config);
const struct miraczv_chassis_config *miraczv_chassis_config_get(void);
int miraczv_chassis_drive(enum miraczv_wheel wheel, float normalized);
/** Coast both motors while leaving the TB6612 enabled. */
void miraczv_chassis_stop(void);
/** Coast both motors and put the TB6612 into standby. */
void miraczv_chassis_disable(void);

#endif /* MIRACZV_BOARD_H_ */
