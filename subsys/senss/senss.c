/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/senss/senss.h>
#include <zephyr/senss/senss_sensor.h>
#include <stdlib.h>
#include "sensor_mgmt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(senss, CONFIG_SENSS_LOG_LEVEL);

/* senss_open_sensor is normally called by applcation: hid, chre, zephyr main, etc */
int senss_open_sensor(int type, int sensor_index, int *handle)
{
	if (!handle) {
		return -ENODEV;
	}
	/* set connection index directly to handle */
	*handle = open_sensor(type, sensor_index);

	return *handle < 0 ? -EINVAL : 0;
}

/* senss_close_sensor is normally called by applcation: hid, chre, zephyr main, etc */
int senss_close_sensor(int handle)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);
	struct senss_sensor *reporter;
	int ret;

	if (!conn) {
		LOG_ERR("%s, handle:%d get connection error", __func__, handle);
		return -EINVAL;
	}
	__ASSERT(!conn->sink, "only sensor that connection to application could be closed");

	reporter = conn->source;

	ret = close_sensor(conn);
	if (ret) {
		LOG_ERR("close_sensor:%d error, ret:%d", handle, ret);
		return ret;
	}

	return 0;
}

int senss_set_interval(int handle, uint32_t value)
{
	return -ENOTSUP;
}

int senss_get_interval(int handle, uint32_t *value)
{
	return -ENOTSUP;
}

int senss_set_sensitivity(int handle, int index, uint32_t value)
{
	return -ENOTSUP;
}

int senss_get_sensitivity(int handle, int index, uint32_t *value)
{
	return -ENOTSUP;
}

int senss_read_sample(int handle, void *buf, int size)
{
	return -ENOTSUP;
}

int senss_register_data_event_callback(int handle,
		senss_data_event_t callback,
		void *param)
{
	return -ENOTSUP;
}

const struct senss_sensor_info *senss_get_sensor_info(int handle)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!conn) {
		LOG_ERR("%s, handle:%d get connection error", __func__, handle);
		return NULL;
	}

	return get_sensor_info(conn->source);
}

int senss_get_sensor_state(int handle, enum senss_sensor_state *state)
{
	return -ENOTSUP;
}
