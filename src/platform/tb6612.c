/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <miraczv/board.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/util.h>

static const struct miraczv_chassis_config chassis_config = {
	.motor = {
		{
			.pwm = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(chassis), 0),
			.in1 = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(chassis), left_in1_gpios, 0),
			.in2 = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(chassis), left_in2_gpios, 0),
		},
		{
			.pwm = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(chassis), 1),
			.in1 = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(chassis), right_in1_gpios, 0),
			.in2 = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(chassis), right_in2_gpios, 0),
			.invert = DT_PROP(DT_NODELABEL(chassis), right_invert),
		},
	},
	.standby = GPIO_DT_SPEC_GET(DT_NODELABEL(chassis), standby_gpios),
	.encoder = {
		{
			.dev = DEVICE_DT_GET(DT_NODELABEL(qei0)),
			.ops = &miraczv_qeiv2_encoder_ops,
			.counts_per_revolution = DT_PROP(DT_NODELABEL(qei0), counts_per_revolution),
			.invert = true,
		},
		{
			.dev = DEVICE_DT_GET(DT_NODELABEL(qei1)),
			.ops = &miraczv_qeiv2_encoder_ops,
			.counts_per_revolution = DT_PROP(DT_NODELABEL(qei1), counts_per_revolution),
		},
	},
};

PINCTRL_DT_DEFINE(DT_NODELABEL(chassis));

static bool ready;

BUILD_ASSERT(DT_PWMS_PERIOD_BY_IDX(DT_NODELABEL(chassis), 0) ==
		     PWM_USEC(CONFIG_MIRACZV_PWM_PERIOD_US),
	     "Left motor PWM period does not match CONFIG_MIRACZV_PWM_PERIOD_US");
BUILD_ASSERT(DT_PWMS_PERIOD_BY_IDX(DT_NODELABEL(chassis), 1) ==
		     PWM_USEC(CONFIG_MIRACZV_PWM_PERIOD_US),
	     "Right motor PWM period does not match CONFIG_MIRACZV_PWM_PERIOD_US");

static int motor_write(const struct miraczv_motor *motor, int32_t permille)
{
	uint32_t pulse;
	bool forward;

	permille = CLAMP(permille, -1000, 1000);
	forward = permille >= 0;
	if (motor->invert) {
		forward = !forward;
	}
	if (permille < 0) {
		permille = -permille;
	}
	if (permille < 10) {
		permille = 0;
	}

	pulse = (uint32_t)(((uint64_t)motor->pwm.period * (uint32_t)permille) / 1000U);
	if (pulse == 0U) {
		(void)gpio_pin_set_dt(&motor->in1, 0);
		(void)gpio_pin_set_dt(&motor->in2, 0);
	} else {
		(void)gpio_pin_set_dt(&motor->in1, forward);
		(void)gpio_pin_set_dt(&motor->in2, !forward);
	}

	return pwm_set_pulse_dt(&motor->pwm, pulse);
}

static int motor_configure(const struct miraczv_motor *motor)
{
	int ret;

	if (!pwm_is_ready_dt(&motor->pwm) || !gpio_is_ready_dt(&motor->in1) ||
	    !gpio_is_ready_dt(&motor->in2)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&motor->in1, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		return ret;
	}

	ret = gpio_pin_configure_dt(&motor->in2, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		return ret;
	}

	return pwm_set_pulse_dt(&motor->pwm, 0U);
}

int miraczv_chassis_init(const struct miraczv_chassis_config *config)
{
	int ret;

	if ((config == NULL) || !gpio_is_ready_dt(&config->standby)) {
		return -ENODEV;
	}

	ret = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(chassis)),
				  PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		return ret;
	}

	for (size_t i = 0; i < MIRACZV_WHEEL_COUNT; ++i) {
		ret = motor_configure(&config->motor[i]);
		if (ret != 0) {
			return ret;
		}

		ret = miraczv_encoder_init(&config->encoder[i]);
		if (ret != 0) {
			return ret;
		}
	}

	ret = gpio_pin_configure_dt(&config->standby, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		return ret;
	}
	ready = true;
	ret = gpio_pin_set_dt(&config->standby, 1);
	if (ret != 0) {
		ready = false;
		return ret;
	}
	return 0;
}

const struct miraczv_chassis_config *miraczv_chassis_config_get(void)
{
	return &chassis_config;
}

int miraczv_chassis_drive(enum miraczv_wheel wheel, float normalized)
{
	const struct miraczv_motor *motor;

	if (!ready || (wheel >= MIRACZV_WHEEL_COUNT)) {
		return -ENODEV;
	}

	motor = &miraczv_chassis_config_get()->motor[wheel];
	normalized = CLAMP(normalized, -1.0f, 1.0f);
	return motor_write(motor, (int32_t)(normalized * 1000.0f));
}

void miraczv_chassis_stop(void)
{
	if (!ready) {
		return;
	}

	(void)miraczv_chassis_drive(MIRACZV_WHEEL_LEFT, 0.0f);
	(void)miraczv_chassis_drive(MIRACZV_WHEEL_RIGHT, 0.0f);
}

void miraczv_chassis_disable(void)
{
	if (!ready) {
		return;
	}

	miraczv_chassis_stop();
	(void)gpio_pin_set_dt(&chassis_config.standby, 0);
	ready = false;
}
