/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIRACZV_CONTROL_CONTROLLER_H_
#define MIRACZV_CONTROL_CONTROLLER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Normalized command shared by all future controller sources. */
struct miraczv_command {
	float linear_mps;
	float angular_radps;
	uint32_t sequence;
	int64_t timestamp_ms;
	bool valid;
};

/** Abstract command source. */
struct miraczv_controller_ops {
	const char *name;
	int (*init)(void);
	int (*poll)(struct miraczv_command *command);
};

/** Register a command source with the application. */
int miraczv_controller_register(const struct miraczv_controller_ops *ops);
/** Initialize the active command source. */
int miraczv_controller_init(void);
/** Poll the active command source. */
int miraczv_controller_poll(struct miraczv_command *command);
/** Return the active controller name, or NULL before registration. */
const char *miraczv_controller_name(void);
/** Select a previously registered command source. */
int miraczv_controller_activate(const struct miraczv_controller_ops *ops);

#ifdef __cplusplus
}
#endif

#endif /* MIRACZV_CONTROL_CONTROLLER_H_ */
