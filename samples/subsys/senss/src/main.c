/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/senss/senss.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

void main(void)
{
	int ret;

	ret = senss_init();
	if (ret) {
		LOG_ERR("senss init error");
		return;
	}

	LOG_INF("senss run successfully");
}
