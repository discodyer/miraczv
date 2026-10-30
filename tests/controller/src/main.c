/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <miraczv/control/pid.h>
#include <zephyr/ztest.h>

ZTEST(miraczv_controller, test_pid_requires_init)
{
	struct miraczv_pid pid = {0};

	zassert_equal(miraczv_pid_update(&pid, 1.0f, 0.0f, 0.01f), 0.0f);
}

ZTEST(miraczv_controller, test_pid_proportional_and_reset)
{
	struct miraczv_pid pid;

	miraczv_pid_init(&pid, 2.0f, 0.0f, 0.0f, -1.0f, 1.0f);
	zassert_within(miraczv_pid_update(&pid, 0.25f, 0.0f, 0.01f), 0.5f, 0.0001f);
	miraczv_pid_reset(&pid);
	zassert_equal(pid.output, 0.0f);
	zassert_equal(pid.integral, 0.0f);
	zassert_equal(pid.previous_measurement, 0.0f);
}

ZTEST(miraczv_controller, test_pid_integral_is_bounded)
{
	struct miraczv_pid pid;

	miraczv_pid_init(&pid, 0.0f, 1.0f, 0.0f, -0.2f, 0.2f);
	for (int i = 0; i < 1000; ++i) {
		(void)miraczv_pid_update(&pid, 1.0f, 0.0f, 0.01f);
	}
	zassert_true(pid.integral <= 0.2f);
}

ZTEST_SUITE(miraczv_controller, NULL, NULL, NULL, NULL, NULL);
