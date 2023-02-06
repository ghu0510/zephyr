/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/senss/senss.h>
#include <zephyr/senss/senss_sensor.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include "sensor_mgmt.h"

#define DT_DRV_COMPAT zephyr_senss

LOG_MODULE_REGISTER(senss, CONFIG_SENSS_LOG_LEVEL);

static struct senss_mgmt_context senss_ctx = {0};

struct senss_mgmt_context *get_senss_ctx(void)
{
	return &senss_ctx;
}

int senss_init(void)
{
	return -ENOTSUP;
}

int senss_deinit(void)
{
	return 0;
}

int senss_get_sensors(const struct senss_sensor_info **info)
{
	return 0;
}
