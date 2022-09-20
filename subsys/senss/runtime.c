/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <senss_sensor.h>
#include <sensor_mgmt.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(senss, CONFIG_SENSS_LOG_LEVEL);

void senss_runtime_thread(void *p1, void *p2, void *p3)
{
	struct senss_mgmt_context *ctx = p1;

	do {
		k_sem_take(&ctx->event_sem, K_FOREVER);
	} while (1);
}
