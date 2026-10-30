/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <miraczv/control/controller.h>
#include <string.h>
#include <zephyr/kernel.h>

#define CONTROLLER_MAX 4U

static const struct miraczv_controller_ops *controllers[CONTROLLER_MAX];
static const struct miraczv_controller_ops *active;
static size_t controller_count;

static bool controller_is_registered(const struct miraczv_controller_ops *ops)
{
	for (size_t i = 0; i < controller_count; ++i) {
		if (controllers[i] == ops) {
			return true;
		}
	}

	return false;
}

int miraczv_controller_register(const struct miraczv_controller_ops *ops)
{
	if ((ops == NULL) || (ops->poll == NULL) || (controller_count >= CONTROLLER_MAX) ||
	    controller_is_registered(ops)) {
		return -EINVAL;
	}

	controllers[controller_count++] = ops;
	return 0;
}

int miraczv_controller_poll(struct miraczv_command *command)
{
	if (command == NULL) {
		return -EINVAL;
	}
	if (active == NULL) {
		return -ENODEV;
	}

	memset(command, 0, sizeof(*command));
	return active->poll(command);
}

int miraczv_controller_init(void)
{
	if (active == NULL) {
		return -ENODEV;
	}

	return active->init != NULL ? active->init() : 0;
}

const char *miraczv_controller_name(void)
{
	return active != NULL ? active->name : NULL;
}

int miraczv_controller_activate(const struct miraczv_controller_ops *ops)
{
	if ((ops == NULL) || !controller_is_registered(ops)) {
		return -EINVAL;
	}

	active = ops;
	return 0;
}
