/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/senss/senss_sensor.h>

#include "sensor_mgmt.h"

LOG_MODULE_DECLARE(senss, CONFIG_SENSS_LOG_LEVEL);

int senss_sensor_post_data(const struct device *dev, void *buf, int size)
{
	struct senss_sensor *sensor = get_sensor_by_dev(dev);

	__ASSERT(sensor, "senss sensor post data, senss_sensor is NULL");

	if (size < sensor->data_size) {
		LOG_ERR("post size:%d should be same as sensor data size:%d",
			size, sensor->data_size);
		return -EINVAL;
	}
	LOG_INF("%s, sensor:%s, data_size:%d", __func__, sensor->dev->name, sensor->data_size);

	memcpy(sensor->data_buf, buf, sensor->data_size);

	return 0;
}

int senss_sensor_notify_data_ready(const struct device *dev)
{
	struct senss_sensor *sensor = get_sensor_by_dev(dev);
	struct senss_mgmt_context *ctx = get_senss_ctx();

	__ASSERT(sensor, "senss sensor notify data ready, senss_sensor is NULL");

	LOG_INF("%s: %s data ready, sensor_mode:%d", __func__, sensor->dev->name, sensor->mode);

	if (sensor->mode != SENSOR_TRIGGER_MODE_DATA_READY) {
		LOG_ERR("sensor:%s is not in data ready mode", sensor->dev->name);
		return -EINVAL;
	}

	atomic_set_bit(&sensor->flag, SENSOR_DATA_READY_BIT);
	atomic_set_bit(&ctx->event_flag, EVENT_DATA_READY);
	k_sem_give(&ctx->event_sem);

	return 0;
}

int senss_sensor_set_data_ready(const struct device *dev, bool data_ready)
{
	struct senss_sensor *sensor = get_sensor_by_dev(dev);

	__ASSERT(sensor, "senss sensor set data ready, senss_sensor is NULL");

	sensor->mode = data_ready ? SENSOR_TRIGGER_MODE_DATA_READY : SENSOR_TRIGGER_MODE_POLLING;
	LOG_INF("%s, sensor:%s, data_ready:%d, trigger_mode:%d",
			__func__, sensor->dev->name, data_ready, sensor->mode);

	return 0;
}
