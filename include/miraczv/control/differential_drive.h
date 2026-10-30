/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIRACZV_CONTROL_DIFFERENTIAL_DRIVE_H_
#define MIRACZV_CONTROL_DIFFERENTIAL_DRIVE_H_

#include <miraczv/board.h>
#include <miraczv/control/controller.h>
#include <miraczv/control/pid.h>

struct miraczv_drive_config {
	float wheel_radius_m;
	float track_width_m;
	float max_wheel_speed_mps;
	struct miraczv_pid pid[MIRACZV_WHEEL_COUNT];
};

struct miraczv_drive_state {
	float wheel_setpoint[MIRACZV_WHEEL_COUNT];
	float wheel_speed[MIRACZV_WHEEL_COUNT];
	float wheel_output[MIRACZV_WHEEL_COUNT];
	bool wheel_speed_valid[MIRACZV_WHEEL_COUNT];
	int64_t last_update_ms;
	bool enabled;
	bool command_fresh;
};

int miraczv_drive_init(const struct miraczv_drive_config *config);
const struct miraczv_chassis_config *miraczv_chassis_config_get(void);
void miraczv_drive_set_command(const struct miraczv_command *command);
void miraczv_drive_control_step(int64_t now_ms);
const struct miraczv_drive_state *miraczv_drive_state_get(void);
void miraczv_drive_stop(void);

#endif /* MIRACZV_CONTROL_DIFFERENTIAL_DRIVE_H_ */
