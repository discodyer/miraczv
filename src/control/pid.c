/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/util.h>
#include <miraczv/control/pid.h>

void miraczv_pid_init(struct miraczv_pid *pid, float kp, float ki, float kd, float output_min,
		      float output_max)
{
	*pid = (struct miraczv_pid){
		.kp = kp,
		.ki = ki,
		.kd = kd,
		.output_min = output_min,
		.output_max = output_max,
	};

	pid->initialized = true;
}

void miraczv_pid_reset(struct miraczv_pid *pid)
{
	pid->integral = 0.0f;
	pid->previous_measurement = 0.0f;
	pid->output = 0.0f;
}

float miraczv_pid_update(struct miraczv_pid *pid, float setpoint, float measurement, float dt_s)
{
	float error;
	float derivative;
	float output;

	if (!pid->initialized || dt_s <= 0.0f) {
		return 0.0f;
	}

	error = setpoint - measurement;
	pid->integral += error * dt_s;

	derivative = (measurement - pid->previous_measurement) / dt_s;
	pid->previous_measurement = measurement;

	output = (pid->kp * error) + (pid->ki * pid->integral) - (pid->kd * derivative);
	output = CLAMP(output, pid->output_min, pid->output_max);

	if (((output == pid->output_max) && (error > 0.0f)) ||
	    ((output == pid->output_min) && (error < 0.0f))) {
		pid->integral -= error * dt_s;
	}

	pid->output = output;
	return output;
}
