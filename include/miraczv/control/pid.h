/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIRACZV_CONTROL_PID_H_
#define MIRACZV_CONTROL_PID_H_

#include <stdbool.h>

struct miraczv_pid {
	float kp;
	float ki;
	float kd;
	float output_min;
	float output_max;
	float integral;
	float previous_measurement;
	float output;
	bool initialized;
};

void miraczv_pid_init(struct miraczv_pid *pid, float kp, float ki, float kd, float output_min,
		      float output_max);
void miraczv_pid_reset(struct miraczv_pid *pid);
float miraczv_pid_update(struct miraczv_pid *pid, float setpoint, float measurement, float dt_s);

#endif /* MIRACZV_CONTROL_PID_H_ */
