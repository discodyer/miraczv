/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <miraczv/control/differential_drive.h>
#include <miraczv/encoder.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

static struct miraczv_drive_config drive_config;
static struct miraczv_drive_state drive_state;
static bool initialized;

int miraczv_drive_init(const struct miraczv_drive_config *config)
{
	if ((config == NULL) || (config->wheel_radius_m <= 0.0f) ||
	    (config->track_width_m <= 0.0f) || (config->max_wheel_speed_mps <= 0.0f)) {
		return -EINVAL;
	}

	drive_config = *config;
	for (size_t i = 0; i < MIRACZV_WHEEL_COUNT; ++i) {
		miraczv_pid_init(&drive_config.pid[i], config->pid[i].kp, config->pid[i].ki,
				 config->pid[i].kd, config->pid[i].output_min,
				 config->pid[i].output_max);
	}

	drive_state = (struct miraczv_drive_state){0};
	drive_state.last_update_ms = k_uptime_get();
	initialized = true;
	return 0;
}

void miraczv_drive_set_command(const struct miraczv_command *command)
{
	float half_track;

	if (!initialized || (command == NULL) || !command->valid) {
		return;
	}

	half_track = drive_config.track_width_m * 0.5f;
	drive_state.wheel_setpoint[MIRACZV_WHEEL_LEFT] =
		command->linear_mps - (command->angular_radps * half_track);
	drive_state.wheel_setpoint[MIRACZV_WHEEL_RIGHT] =
		command->linear_mps + (command->angular_radps * half_track);

	for (size_t i = 0; i < MIRACZV_WHEEL_COUNT; ++i) {
		drive_state.wheel_setpoint[i] =
			CLAMP(drive_state.wheel_setpoint[i], -drive_config.max_wheel_speed_mps,
			      drive_config.max_wheel_speed_mps);
	}

	drive_state.command_fresh = true;
}

void miraczv_drive_control_step(int64_t now_ms)
{
	float dt_s = (now_ms - drive_state.last_update_ms) / 1000.0f;
	const struct miraczv_chassis_config *chassis = miraczv_chassis_config_get();
	const struct miraczv_encoder *encoder;
	struct miraczv_encoder_data data;

	if (!initialized) {
		return;
	}

	dt_s = CLAMP(dt_s, 0.001f, 0.1f);
	drive_state.last_update_ms = now_ms;

	for (size_t i = 0; i < MIRACZV_WHEEL_COUNT; ++i) {
		encoder = &chassis->encoder[i];
		drive_state.wheel_speed_valid[i] = false;
		if ((miraczv_encoder_sample_fetch(encoder, &data) != 0) || !data.speed_valid) {
			drive_state.wheel_speed[i] = 0.0f;
			drive_state.wheel_output[i] = 0.0f;
			miraczv_pid_reset(&drive_config.pid[i]);
			miraczv_chassis_drive((enum miraczv_wheel)i, 0.0f);
			continue;
		}

		drive_state.wheel_speed[i] = ((float)data.counts_per_second_milli / 1000.0f) *
					     (2.0f * 3.14159265f * drive_config.wheel_radius_m) /
					     encoder->counts_per_revolution;
		drive_state.wheel_speed_valid[i] = true;
		if (drive_state.wheel_setpoint[i] == 0.0f) {
			miraczv_pid_reset(&drive_config.pid[i]);
			drive_state.wheel_output[i] = 0.0f;
		} else {
			drive_state.wheel_output[i] = miraczv_pid_update(
				&drive_config.pid[i], drive_state.wheel_setpoint[i],
				drive_state.wheel_speed[i], dt_s);
		}
		miraczv_chassis_drive((enum miraczv_wheel)i, drive_state.wheel_output[i]);
	}
}

const struct miraczv_drive_state *miraczv_drive_state_get(void)
{
	return &drive_state;
}

void miraczv_drive_stop(void)
{
	drive_state.wheel_setpoint[MIRACZV_WHEEL_LEFT] = 0.0f;
	drive_state.wheel_setpoint[MIRACZV_WHEEL_RIGHT] = 0.0f;
	drive_state.command_fresh = false;
	for (size_t i = 0; i < MIRACZV_WHEEL_COUNT; ++i) {
		miraczv_pid_reset(&drive_config.pid[i]);
		drive_state.wheel_speed[i] = 0.0f;
		drive_state.wheel_output[i] = 0.0f;
		drive_state.wheel_speed_valid[i] = false;
	}
	miraczv_chassis_stop();
}
