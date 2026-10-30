/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <miraczv/board.h>
#include <miraczv/control/controller.h>
#include <miraczv/control/differential_drive.h>
#ifdef CONFIG_MIRACZV_HID_HOST
#include <miraczv/platform/hid_controller.h>
#endif
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(miraczv, LOG_LEVEL_INF);

static struct miraczv_command last_command;
static int64_t last_command_ms;

static void command_timeout_check(int64_t now_ms)
{
	if (last_command.valid &&
	    ((now_ms - last_command_ms) > CONFIG_MIRACZV_COMMAND_TIMEOUT_MS)) {
		last_command.valid = false;
		miraczv_drive_stop();
		LOG_WRN("Controller command timeout, motors coasting");
	}
}

int main(void)
{
	const struct miraczv_chassis_config *chassis = miraczv_chassis_config_get();
	struct miraczv_drive_config drive_config = {
		.wheel_radius_m = CONFIG_MIRACZV_WHEEL_RADIUS_MM / 1000.0f,
		.track_width_m = CONFIG_MIRACZV_TRACK_WIDTH_MM / 1000.0f,
		.max_wheel_speed_mps = CONFIG_MIRACZV_MAX_WHEEL_MPS / 1000.0f,
	};
	struct miraczv_command command;
	int ret;

	for (size_t i = 0; i < MIRACZV_WHEEL_COUNT; ++i) {
		drive_config.pid[i] = (struct miraczv_pid){
			.kp = 1.1f,
			.ki = 2.0f,
			.kd = 0.0f,
			.output_min = -1.0f,
			.output_max = 1.0f,
		};
	}

	if (!device_is_ready(chassis->encoder[MIRACZV_WHEEL_LEFT].dev) ||
	    !device_is_ready(chassis->encoder[MIRACZV_WHEEL_RIGHT].dev)) {
		LOG_ERR("Encoder device not ready");
		return 0;
	}

	ret = miraczv_chassis_init(chassis);
	if (ret != 0) {
		LOG_ERR("Chassis initialization failed: %d", ret);
		return 0;
	}

	ret = miraczv_drive_init(&drive_config);
	if (ret != 0) {
		LOG_ERR("Drive initialization failed: %d", ret);
		return 0;
	}

#ifdef CONFIG_MIRACZV_HID_HOST
	ret = miraczv_controller_register(&miraczv_hid_controller_ops);
	if (ret == 0) {
		ret = miraczv_controller_activate(&miraczv_hid_controller_ops);
	}
	if (ret == 0) {
		ret = miraczv_controller_init();
	}
	if (ret != 0) {
		LOG_ERR("HID controller initialization failed: %d", ret);
	}
#endif

	LOG_INF("MiracZV ready: controller=%s",
		miraczv_controller_name() != NULL ? miraczv_controller_name() : "none");
	last_command_ms = k_uptime_get();

	while (true) {
		int64_t now_ms = k_uptime_get();

		command = (struct miraczv_command){0};
		ret = miraczv_controller_poll(&command);
		if (ret == 0 && command.valid) {
			last_command = command;
			last_command_ms = now_ms;
			miraczv_drive_set_command(&command);
		} else if (ret != -EAGAIN && ret != -ENODEV && ret != -ENODATA) {
			LOG_DBG("Controller poll failed: %d", ret);
		}

		command_timeout_check(now_ms);
		miraczv_drive_control_step(now_ms);
		k_sleep(K_MSEC(CONFIG_MIRACZV_CONTROL_PERIOD_MS));
	}
}
