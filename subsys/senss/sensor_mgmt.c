/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/senss/senss.h>
#include <zephyr/logging/log.h>
#include <sensor_mgmt.h>

#define DT_DRV_COMPAT zephyr_senss

LOG_MODULE_REGISTER(senss, CONFIG_SENSS_LOG_LEVEL);

/* Enumerated sensors from device tree */
static struct senss_sensor_dt_info sensors_dt[] = {
	DT_FOREACH_CHILD(DT_DRV_INST(0), SENSS_SENSOR_INFO)
};

int senss_init(void)
{
	int sensor_num = ARRAY_SIZE(sensors_dt);

	LOG_INF("sensor num:%d", sensor_num);
	if (sensor_num == 0) {
		LOG_WRN("no sensor created yet");
		return -EINVAL;
	}

	return 0;
}

