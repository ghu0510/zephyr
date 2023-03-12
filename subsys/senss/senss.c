/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>
#include <zephyr/senss/senss_sensor.h>
#include "sensor_mgmt.h"

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
	LOG_INF("%s, handle:%d, sensor:%s", __func__, handle, conn->source->dev->name);

	ret = close_sensor(conn);
	if (ret) {
		LOG_ERR("close_sensor:%d error", handle);
		return ret;
	}

	LOG_INF("%s(%d) ready, ret:%d", __func__, __LINE__, ret);

	/* connection is unbind, notify sensor mgmt to do updates */
	save_config_and_notify(ctx, reporter);

	return 0;
}

int senss_set_interval(int handle, uint32_t value)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!conn) {
		LOG_ERR("%s, handle:%d get connection error", __func__, handle);
		return -EINVAL;
	}

	LOG_INF("%s, sensor:%s, interval:%d(us)",
		__func__, conn->source->dev->name, value);

	return set_interval(conn, value);
}

int senss_get_interval(int handle, uint32_t *value)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!value) {
		LOG_ERR("%s, invalid interval address", __func__);
		return -EINVAL;
	}

	if (!conn) {
		LOG_ERR("handle:%d get connection error", handle);
		return -EINVAL;
	}

	LOG_INF("%s, sensor:%s", __func__, conn->source->dev->name);

	return get_interval(conn, value);
}

int senss_set_sensitivity(int handle, int index, uint32_t value)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!conn) {
		LOG_ERR("%s, handle:%d get connection error", __func__, handle);
		return -EINVAL;
	}

	LOG_INF("%s, sensor:%s, index:%d, sensitivity:%d",
		__func__, conn->source->dev->name, index, value);

	return set_sensitivity(conn, index, value);
}

int senss_get_sensitivity(int handle, int index, uint32_t *value)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!value) {
		LOG_ERR("%s, invalid sensitivity address", __func__);
		return -EINVAL;
	}

	if (!conn) {
		LOG_ERR("%s, handle:%d get connection error", __func__, handle);
		return -EINVAL;
	}

	LOG_INF("%s, index:%d", __func__, index);

	return get_sensitivity(conn, index, value);
}

int senss_read_sample(int handle, void *buf, int size)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!buf) {
		LOG_ERR("%s, invalid buf to read sample", __func__);
		return -EINVAL;
	}

	if (!conn) {
		LOG_ERR("handle:%d get connection error", handle);
		return -EINVAL;
	}

	return read_sample(conn->source, buf, size);
}

int senss_register_data_event_callback(int handle,
				       senss_data_event_t callback,
				       void *param)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!conn) {
		LOG_ERR("%s, handle:%d get connection error", __func__, handle);
		return -EINVAL;
	}

	__ASSERT(!conn->sink, "only connection to application could register data event callback");

	return register_data_event_callback(conn, callback, param);
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
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!conn) {
		LOG_ERR("%s, handle:%d get connection error", __func__, handle);
		return -EINVAL;
	}

	return get_sensor_state(conn->source, state);
}

int senss_set_report_latency(int handle, uint64_t latency)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn = get_connection_by_handle(ctx, handle);

	if (!conn) {
		LOG_ERR("%s, handle:%d get connection error", __func__, handle);
		return -EINVAL;
	}

	return set_report_latency(conn, latency);
}
