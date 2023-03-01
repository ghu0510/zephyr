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
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include "sensor_mgmt.h"

#define DT_DRV_COMPAT zephyr_senss

LOG_MODULE_REGISTER(senss, CONFIG_SENSS_LOG_LEVEL);

/* Enumerated sensors from device tree */
static struct senss_sensor_dt_info sensors_dt[] = {
	DT_FOREACH_CHILD(DT_DRV_INST(0), SENSS_SENSOR_INFO)
};

static struct senss_mgmt_context senss_ctx = {0};

K_THREAD_STACK_DEFINE(runtime_stack, CONFIG_SENSS_RUNTIME_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(dispatch_stack, CONFIG_SENSS_DISPATCH_THREAD_STACK_SIZE);


struct senss_mgmt_context *get_senss_ctx(void)
{
	return &senss_ctx;
}

static int cmp_sensor(const void *a, const void *b)
{
	struct senss_sensor *sensor_a = *(struct senss_sensor **)a;
	struct senss_sensor *sensor_b = *(struct senss_sensor **)b;

	return sensor_a->dt_info->ord - sensor_b->dt_info->ord;
}

static void sort_sensors(struct senss_mgmt_context *ctx)
{
	qsort(ctx->sensor_db, ctx->sensor_num, sizeof(struct senss_sensor *), cmp_sensor);
}

static int set_sensor_state(struct senss_sensor *sensor, enum senss_sensor_state state)
{
	__ASSERT(sensor, "set sensor state, senss_sensor is NULL");

	sensor->state = state;

	return 0;
}

static int init_each_connection(struct senss_mgmt_context *ctx,
				struct senss_connection *conn,
				struct senss_sensor *source,
				struct senss_sensor *sink)
{
	__ASSERT(conn, "init each connection, invalid connection");
	conn->source = source;
	conn->sink = sink;
	LOG_INF("%s(%d), sensor:%s, conn:%p, conn-data:%p",
		__func__, __LINE__, source->dev->name, conn, conn->data);
	conn->interval = 0;
	memset(conn->sensitivity, 0x00, sizeof(conn->sensitivity));

	return 0;
}

static int init_sensor(struct senss_mgmt_context *ctx, struct senss_sensor *sensor)
{
	const struct senss_sensor_api *sensor_api;
	struct senss_sensor *reporter;
	struct senss_connection *conn;
	int conn_idx[CONFIG_SENSS_MAX_REPORTER_COUNT] = {0};
	int i, ret = 0;

	__ASSERT(sensor && sensor->dev, "init sensor, sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "init sensor, sensor device sensor_api is NULL");

	sensor->conns_num = sensor->dt_info->reporter_num;

	/* physical sensor has no reporter, conns_num is 0 */
	if (sensor->conns_num == 0) {
		sensor->conns = NULL;
	}

	__ASSERT(sensor->conns_num <= CONFIG_SENSS_MAX_REPORTER_COUNT,
		"connection number:%d exceed max number:%d",
		 sensor->conns_num, CONFIG_SENSS_MAX_REPORTER_COUNT);

	for_each_reporter_conn(i, sensor, conn) {
		/* initial each connection for sensors who has reporters */
		reporter = get_reporter_sensor(ctx, sensor, i);
		__ASSERT(reporter, "sensor's reporter should not be NULL");
		conn->data = malloc(reporter->data_size);
		if (!conn->data) {
			LOG_ERR("malloc memory for connection data error");
			return -ENOMEM;
		}
		ret |= init_each_connection(ctx, conn, reporter, sensor);
		if (ret) {
			LOG_ERR("%s, init_each_connection error:%d", __func__, ret);
			return ret;
		}

		LOG_INF("%s(%d), i:%d, reporter:%s, sensor:%s, conn:%p, conn-data:%p",
			__func__, __LINE__, i, reporter->dev->name, sensor->dev->name, conn, conn->data);

		/* device tree required sensor connection between reporter and client cannot be
		 * opened or closed any more, so it is called fixed connection.
		 */
		conn->index = ctx->fixed_connection_count++;
		LOG_DBG("%s, reporter:%s, client:%s, connection:%d",
			__func__, reporter->dev->name, sensor->dev->name, conn->index);

		__ASSERT(conn->index < CONFIG_SENSS_MAX_CONNECTION_COUNT,
			 "sensor connection number:%d exceed %d",
			 conn->index, CONFIG_SENSS_MAX_CONNECTION_COUNT);

		sensor->conns = conn;
		ctx->conns[conn->index] = conn;
		/* link connection to its repoter's client_list */
		sys_slist_append(&reporter->client_list, &conn->snode);

		conn_idx[i] = conn->index;
	}

	/* physical sensor is working at polling mode by default,
	 * virtual sensor working mode is inherited from its reporter
	 */
	if (is_phy_sensor(sensor)) {
		sensor->mode = SENSOR_TRIGGER_MODE_POLLING;
	}

	return sensor_api->init(sensor->dev, &sensor->dt_info->info, conn_idx, sensor->conns_num);
}

static int fetch_data_and_dispatch(struct senss_mgmt_context *ctx)
{
	struct sensor_data_header *header;
	struct senss_connection *conn;
	uint8_t buf[CONFIG_SENSS_MAX_SENSOR_DATA_SIZE];
	uint32_t wanted_size = sizeof(*header);
	uint32_t ret_size, rd_size = 0;
	uint16_t data_size = 0;
	uint16_t conn_index = 0;
	int ret = 0;

	while ((ret_size = ring_buf_get(&ctx->sensor_ring_buf, buf + rd_size, wanted_size)) > 0) {
		rd_size += ret_size;
		if (rd_size == sizeof(*header)) {
			header = (struct sensor_data_header *)buf;
			data_size = header->data_size;
			__ASSERT(data_size + sizeof(*header) <= CONFIG_SENSS_MAX_SENSOR_DATA_SIZE,
						"invalid data size:%d", header->data_size);
			conn_index = header->conn_index;
			__ASSERT(conn_index < CONFIG_SENSS_MAX_CONNECTION_COUNT,
						"invalid connection index:%d", conn_index);
			/* get data_size from header, and then read sensor data from ring buf */
			wanted_size = data_size;
		} else if (rd_size == sizeof(*header) + wanted_size) {
			/* read next sample header */
			wanted_size = sizeof(*header);
			rd_size = 0;
			conn = ctx->conns[conn_index];
			if (!conn) {
				LOG_WRN("%s, connection is NULL", __func__);
				continue;
			} else if (!conn->data_evt_cb) {
				LOG_WRN("%s, connection:%d event-callback not registered",
							__func__, conn->index);
				continue;
			}
			ret = conn->data_evt_cb((int)conn_index, buf + sizeof(*header),
								data_size, conn->cb_param);
		} else {
			LOG_ERR("%s, invalid ret_size:%d, rd_size:%d", __func__, ret_size, rd_size);
			ret = -EINVAL;
		}
	}

	if (ret_size == 0 && wanted_size != sizeof(*header)) {
		LOG_ERR("%s, ret_size:%d, wanted_size:%d not expected:%d",
			__func__, ret_size, wanted_size, sizeof(*header));
		__ASSERT(wanted_size, "wanted_size:%d", wanted_size);
	}

	return ret;
}

static void senss_dispatch_thread(void *p1, void *p2, void *p3)
{
	struct senss_mgmt_context *ctx = p1;

	LOG_INF("%s start...", __func__);

	do {
		k_sem_take(&ctx->dispatch_sem, K_FOREVER);

		fetch_data_and_dispatch(ctx);
	} while (1);
}

/* create struct senss_sensor *sensor according to sensor device tree */
static struct senss_sensor *create_sensor(struct senss_sensor_dt_info *dt)
{
	struct senss_sensor_ctx *sensor_ctx;
	struct senss_sensor *sensor;
	uint16_t sample_size;

	__ASSERT(dt && dt->dev, "device tree info or senss dev is invalid");
	sensor_ctx = dt->dev->data;
	__ASSERT(sensor_ctx, "senss sensor context is invalid");

	sample_size = sensor_ctx->register_info->sample_size;
	/* total memory to be alloctead for a sensor according to sensor device tree
	 * 1) struct senss_sensor*
	 * 2) sample data point to struct senss_sensor->data_buf
	 */
	sensor = malloc(sizeof(*sensor) + sample_size + dt->reporter_num * sizeof(*sensor->conns));
	if (!sensor) {
		LOG_ERR("malloc memory for struct senss_sensor error");
		return NULL;
	}
	sensor->data_size = sample_size;
	sensor->data_buf = (uint8_t *)sensor + sizeof(*sensor);
	sensor->conns = (struct senss_connection *)((uint8_t *)sensor->data_buf + sample_size);

	sensor->dev = dt->dev;
	/* sensor->dt_info pointed to sensor device tree */
	sensor->dt_info = dt;
	sensor->dt_info->info.flags = sensor_ctx->register_info->flags;
	sensor->dt_info->info.version = sensor_ctx->register_info->version;

	LOG_INF("%s, sensor:%s(%p), min_ri:%d(us)",
		__func__, sensor->dev->name, sensor, sensor->dt_info->info.minimal_interval);

	sensor->interval = 0;
	sensor->sensitivity_count = sensor_ctx->register_info->sensitivity_count;
	__ASSERT(sensor->sensitivity_count <= CONFIG_SENSS_MAX_SENSITIVITY_COUNT,
		 "sensitivity count:%d should not exceed MAX_SENSITIVITY_COUNT",
		 sensor->sensitivity_count);
	memset(sensor->sensitivity, 0x00, sizeof(sensor->sensitivity));

	sensor->state = SENSS_SENOSR_STATE_NOT_READY;
	sys_slist_init(&sensor->client_list);

	sensor_ctx->priv_ptr = sensor;

	return sensor;
}

static void sensor_event_init(struct senss_mgmt_context *ctx)
{
	/* initial semaphore */
	k_sem_init(&ctx->dispatch_sem, 0, 1);
	k_sem_init(&ctx->event_sem, 0, 1);

	/* initial events */
	k_mutex_init(&ctx->conn_mutex);
}

static int senss_bind_conn(struct senss_mgmt_context *ctx, struct senss_connection *conn)
{
	int i = 0;

	__ASSERT(conn && conn->source, "%s, connection or reporter should not be NULL", __func__);
	/* multi applications could bind connection simultaneously, set mutex protect */
	k_mutex_lock(&ctx->conn_mutex, K_FOREVER);
	/* find the first free connection */
	for (i = ctx->fixed_connection_count; i < CONFIG_SENSS_MAX_CONNECTION_COUNT; i++) {
		if (!ctx->conns[i]) {
			break;
		}
	}

	if (i >= CONFIG_SENSS_MAX_CONNECTION_COUNT) {
		LOG_ERR("connection index:%d should less than CONFIG_SENSS_MAX_CONNECTION_COUNT:%d",
			i, CONFIG_SENSS_MAX_CONNECTION_COUNT);
		k_mutex_unlock(&ctx->conn_mutex);
		return -EINVAL;
	}

	conn->index = i;

	ctx->conns[i] = conn;
	/* conn->source is pointer to reporter, add conn to repoter sensor client list */
	sys_slist_append(&conn->source->client_list, &conn->snode);
	k_mutex_unlock(&ctx->conn_mutex);

	return 0;
}

static int senss_unbind_conn(struct senss_mgmt_context *ctx, struct senss_connection *conn)
{
	struct senss_sensor *reporter;

	/* multi applications could unbind connection simultaneously, set mutex protect */
	k_mutex_lock(&ctx->conn_mutex, K_FOREVER);
	__ASSERT(conn && conn->source, "close sensor, connection or reporter not be NULL");
	reporter = conn->source;
	__ASSERT(conn->index < CONFIG_SENSS_MAX_CONNECTION_COUNT,
		"sensor connection number:%d exceed MAX_SENSOR_COUNT", conn->index);

	sys_slist_find_and_remove(&reporter->client_list, &conn->snode);
	LOG_DBG("%s: %s connection:%d complete", __func__, reporter->dev->name, conn->index);

	ctx->conns[conn->index] = NULL;
	k_mutex_unlock(&ctx->conn_mutex);

	return 0;
}

int open_sensor(int type, int sensor_index)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor *reporter;
	struct senss_connection *conn;
	int ret;

	/* get the reporter sensor to be opened */
	reporter = get_sensor_by_type_and_index(ctx, type, sensor_index);
	if (!reporter) {
		LOG_ERR("no sensor match to type:0x%x, index:%d", type, sensor_index);
		return SENSS_SENSOR_INVALID_HANDLE;
	}
	/* allocate struct senss_connection *conn and conn data for application client */
	conn = malloc(sizeof(*conn) + reporter->data_size);
	if (!conn) {
		LOG_ERR("malloc memory for struct senss_connection error");
		return SENSS_SENSOR_INVALID_HANDLE;
	}
	conn->data = (uint8_t *)conn + sizeof(*conn);
	LOG_INF("%s(%d), conn:%p, conn_size:%d", __func__, __LINE__, conn, sizeof(*conn));

	/* create connection for reporter to application(client = NULL) */
	ret = init_each_connection(ctx, conn, reporter, NULL);
	if (ret) {
		LOG_ERR("%s, init_each_connection error:%d", __func__, ret);
		return SENSS_SENSOR_INVALID_HANDLE;
	}

	ret = senss_bind_conn(ctx, conn);
	if (ret) {
		LOG_ERR("%s, senss_bind_conn error:%d", __func__, ret);

		return SENSS_SENSOR_INVALID_HANDLE;
	}

	LOG_INF("%s ready: %s, state:0x%x, conn:%d",
			__func__, reporter->dev->name, reporter->state, conn->index);

	return conn->index;
}

int close_sensor(struct senss_connection *conn)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	int ret;

	ret = senss_unbind_conn(ctx, conn);
	if (ret) {
		LOG_ERR("%s, senss_unbind_conn error:%d", __func__, ret);
		return SENSS_SENSOR_INVALID_HANDLE;
	}

	if (!conn->sink) {
		free(conn);
	} else {
		free(conn->data);
	}

	return 0;
}

int senss_init(void)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor_dt_info *dt_info = &sensors_dt[0];
	struct senss_sensor *sensor;
	enum senss_sensor_state state;
	int ret = 0;
	int i;

	LOG_INF("%s begin...", __func__);

	if (ctx->senss_initialized) {
		LOG_INF("senss is already initialized");
		return 0;
	}

	ctx->sensor_num = ARRAY_SIZE(sensors_dt);
	if (ctx->sensor_num == 0) {
		LOG_WRN("no sensor created yet");
		return 0;
	}

	for (i = 0; i < ctx->sensor_num; i++) {
		sensor = create_sensor(dt_info);
		if (!sensor) {
			LOG_ERR("%s, create_sensor error", __func__);
			return -EINVAL;
		}
		ctx->sensor_db[i] = sensor;
		dt_info++;
	}

	sort_sensors(ctx);

	for_each_sensor(ctx, i, sensor) {
		ret = init_sensor(ctx, sensor);
		if (ret) {
			LOG_ERR("sensor:%s initial error", sensor->dev->name);
		}
		state = (ret ? SENSS_SENOSR_STATE_NOT_READY : SENSS_SENOSR_STATE_READY);
		ret = set_sensor_state(sensor, state);
		if (ret) {
			LOG_ERR("set sensor:%s state:%d error", sensor->dev->name, state);
		}
		LOG_INF("%s, sensor:%s ret:%d", __func__, sensor->dev->name, ret);
	}

	sensor_event_init(ctx);

	ring_buf_init(&ctx->sensor_ring_buf, sizeof(ctx->buf), ctx->buf);

	ctx->senss_initialized = true;

	/* sensor subsystem runtime thread: sensor scheduling and sensor data processing */
	ctx->runtime_id = k_thread_create(&ctx->runtime_thread, runtime_stack,
			CONFIG_SENSS_RUNTIME_THREAD_STACK_SIZE,
			(k_thread_entry_t) senss_runtime_thread, ctx, NULL, NULL,
			CONFIG_SENSS_RUNTIME_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!ctx->runtime_id) {
		LOG_ERR("create runtime thread error");
		return -EAGAIN;
	}

	/* sensor dispatch thread: get sensor data from senss and dispatch data */
	ctx->dispatch_id = k_thread_create(&ctx->dispatch_thread, dispatch_stack,
			CONFIG_SENSS_DISPATCH_THREAD_STACK_SIZE,
			(k_thread_entry_t) senss_dispatch_thread, ctx, NULL, NULL,
			CONFIG_SENSS_DISPATCH_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!ctx->dispatch_id) {
		LOG_ERR("create dispatch thread error");
		return -EAGAIN;
	}

	return ret;
}

int senss_deinit(void)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_connection *conn;
	struct senss_sensor *sensor;
	int ret = 0;
	int i;

	/* close all sensors */
	for (i = 0; i < CONFIG_SENSS_MAX_CONNECTION_COUNT; i++) {
		conn = ctx->conns[i];
		if (!conn) {
			continue;
		}
		ret |= close_sensor(conn);
	}

	/* free sensors */
	for_each_sensor(ctx, i, sensor) {
		LOG_INF("%s(%d), sensor:%p", __func__, __LINE__, sensor);
		free(sensor);
	}
	ctx->sensor_num = 0;
	ctx->senss_initialized = false;
	k_thread_abort(ctx->runtime_id);
	k_thread_abort(ctx->dispatch_id);

	LOG_INF("%s complete", __func__);

	return ret;
}

int senss_get_sensors(const struct senss_sensor_info **info)
{
	struct senss_mgmt_context *ctx = (struct senss_mgmt_context *)get_senss_ctx();
	const struct senss_sensor *sensor;
	struct senss_sensor_info *tmp_info;
	int i = 0;

	/* temporary souluction, will modify it after constant sensor information stored in
	 * an iterable section in ROM. https://github.com/zephyrproject-rtos/zephyr/pull/49294
	 */
	if (!ctx->info) {
		ctx->info = malloc(ctx->sensor_num * sizeof(struct senss_sensor_info));
		if (!ctx->info) {
			LOG_ERR("malloc memory for senss_sensor_info error");
			return 0;
		}
		tmp_info = ctx->info;
		for_each_sensor(ctx, i, sensor) {
			*tmp_info++ = sensor->dt_info->info;
		}
	}
	*info = ctx->info;

	return ctx->sensor_num;
}

void save_config_and_notify(struct senss_mgmt_context *ctx, struct senss_sensor *sensor)
{
	LOG_INF("%s, sensor:%s", __func__, sensor->dev->name);

	__ASSERT(sensor, "save config and notify, senss_sensor is NULL");

	atomic_set_bit(&sensor->flag, SENSOR_LATER_CFG_BIT);

	atomic_set_bit(&ctx->event_flag, EVENT_CONFIG_READY);
	k_sem_give(&ctx->event_sem);
}

/* client sensor request to set interval to connection sensor */
int set_interval(struct senss_connection *conn, uint32_t interval)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();

	__ASSERT(conn && conn->source, "%s, connection or reporter should not be NULL", __func__);

	LOG_INF("%s, sensor:%s, conn:%d, interval:%u",
		__func__, conn->source->dev->name, conn->index, interval);

	if (interval > 0 && interval < conn->source->dt_info->info.minimal_interval) {
		LOG_ERR("interval:%d(us) should no less than min interval:%d(us)",
					interval, conn->source->dt_info->info.minimal_interval);
		return -EINVAL;
	}

	conn->interval = interval;
	/* sensor configured with new interval, need to get new sample and restart counting again */
	conn->next_consume_time = EXEC_TIME_INIT;

	/* save current sensor config first, will uniformly config interval after enumerating
	 * all sensors to avoid multiple configuration to certain sensor
	 */
	save_config_and_notify(ctx, conn->source);

	return 0;
}

/* sensor interval is arbitrated by all clients, get_interval() would return interval set by
 * client from struct connection
 */
int get_interval(struct senss_connection *conn, uint32_t *value)
{
	__ASSERT(conn, "get interval, connection not be NULL");
	*value = conn->interval;

	return 0;
}

/* client sensor request to set sensitivity to connection sensor */
int set_sensitivity(struct senss_connection *conn, int index, uint32_t sensitivity)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor *sensor;
	int i;

	__ASSERT(conn && conn->source, "set sensitivity, connection or reporter not be NULL");

	sensor = conn->source;

	LOG_INF("%s, sensor:%s, conn:%d, index:%d, sensitivity:%d, count:%d",
			__func__, sensor->dev->name, conn->index, index,
			sensitivity, sensor->sensitivity_count);

	if (index < SENSS_INDEX_ALL || index >= sensor->sensitivity_count) {
		LOG_ERR("sensor:%s sensitivity index:%d is invalid", sensor->dev->name, index);
		return -EINVAL;
	}

	if (index == SENSS_INDEX_ALL) {
		for (i = 0; i < sensor->sensitivity_count; i++) {
			conn->sensitivity[i] = sensitivity;
		}
	} else {
		conn->sensitivity[index] = sensitivity;
	}

	/* save current sensor config first, will uniformly config sensitivity after enumerating
	 * all sensors to avoid multiple configuration to certain sensor
	 */
	save_config_and_notify(ctx, sensor);

	return 0;
}

/* sensor sensitivity is arbitrated by all clients, get_sensitivity() would return interval set by
 * client from struct senss_connection
 */
int get_sensitivity(struct senss_connection *conn, int index, uint32_t *value)
{
	struct senss_sensor *sensor;
	int i = 0;

	__ASSERT(conn && conn->source, "get sensitivity, connection or reporter not be NULL");

	sensor = conn->source;

	*value = UINT32_MAX;

	if (index < SENSS_INDEX_ALL || index >= sensor->sensitivity_count) {
		LOG_ERR("sensor:%s sensitivity index:%d is invalid", sensor->dev->name, index);
		return -EINVAL;
	}

	if (index == SENSS_INDEX_ALL) {
		/* each sensitivity index value should be same for global sensitivity */
		for (i = 1; i < sensor->sensitivity_count; i++) {
			if (conn->sensitivity[i] != conn->sensitivity[0]) {
				LOG_ERR("sensitivity[%d]:%d should be same as senstivity:%d",
					i, conn->sensitivity[i], conn->sensitivity[0]);
				return -EINVAL;
			}
		}
		*value = conn->sensitivity[0];
	} else {
		*value = conn->sensitivity[index];
	}

	return 0;
}

int read_sample(struct senss_sensor *sensor, void *buf, int size)
{
	__ASSERT(sensor, "read sample, senss_sensor is NULL");

	if (!sensor) {
		LOG_ERR("cannot find sensor");
		return -ENODEV;
	}

	if (size < sensor->data_size) {
		LOG_ERR("sensor size:%d is not match with required:%d",
						sensor->data_size, size);
		return -ERANGE;
	}
	memcpy(buf, sensor->data_buf, sensor->data_size);

	return 0;
}

int register_data_event_callback(struct senss_connection *conn,
				 senss_data_event_t callback,
				 void *param)
{
	__ASSERT(conn, "register data event callback, connection not be NULL");

	conn->data_evt_cb = callback;
	conn->cb_param = param;

	return 0;
}

const struct senss_sensor_info *get_sensor_info(struct senss_sensor *sensor)
{
	__ASSERT(sensor, "get sensor info, senss_sensor is NULL");

	return &sensor->dt_info->info;
}

int get_sensor_state(struct senss_sensor *sensor, enum senss_sensor_state *state)
{
	__ASSERT(sensor, "get sensor state, senss_sensor is NULL");
	*state = sensor->state;

	return 0;
}

/* sensor_later_config including interval/sensitiviyt arbitrate and set,
 * which is called in rumtime thread
 */
static uint32_t arbitrate_interval(struct senss_sensor *sensor)
{
	struct senss_connection *conn;
	uint32_t min_interval = UINT32_MAX;
	uint32_t interval;

	/* search from all clients, arbitrate the interval */
	for_each_client_conn(sensor, conn) {
		LOG_INF("%s, for each client, sensor:%s, conn:%d, interval:%d(us)",
			__func__, sensor->dev->name, conn->index, conn->interval);
		if (!is_client_request_data(conn)) {
			continue;
		}
		if (conn->interval < min_interval) {
			min_interval = conn->interval;
		}
	}
	/* min_interval == UINT32_MAX means sensor is not opened by any clients
	 * if no client open the sensor, interval should be 0
	 */
	interval = (min_interval == UINT32_MAX ? 0 : min_interval);

	LOG_INF("%s, sensor:%s, interval:%d(us), next_exec_time:%lld",
		__func__, sensor->dev->name, interval, sensor->next_exec_time);

	/* interval == 0 means sensor is not opened by any client
	 */
	if (interval == 0) {
		/* sensor is closed by all clients, reset next_exec_time as EXEC_TIME_OFF
		 * open -> close: next_exec_time = EXEC_TIME_OFF
		 */
		sensor->next_exec_time = EXEC_TIME_OFF;
	} else {
		/* sensor is still closed last time, set next_exec_time as EXEC_TIME_INIT
		 * close -> open: next_exec_time = EXEC_TIME_INIT
		 */
		if (sensor->next_exec_time == EXEC_TIME_OFF) {
			sensor->next_exec_time = EXEC_TIME_INIT;
		}
	}

	return interval;
}

static int set_arbitrate_interval(struct senss_sensor *sensor, uint32_t interval)
{
	const struct senss_sensor_api *sensor_api;

	__ASSERT(sensor && sensor->dev, "set arbitrate interval, sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "set arbitrate interval, sensor device sensor_api is NULL");

	sensor->interval = interval;
	/* reset sensor next_exec_time and sample timestamp as soon as sensor interval is changed */
	sensor->next_exec_time = interval > 0 ? EXEC_TIME_INIT : EXEC_TIME_OFF;

	LOG_INF("%s, interval:%d, next_exec_time:%lld, sensor:%s, data_buf:%p",
		__func__, interval, sensor->next_exec_time, sensor->dev->name, sensor->data_buf);

	((struct senss_sensor_value_header *)sensor->data_buf)->base_timestamp = 0;

	LOG_INF("%s, interval:%d, next_exec_time:%lld", __func__, interval, sensor->next_exec_time);

	if (!sensor_api->set_interval) {
		LOG_ERR("sensor:%s set_interval callback is not set yet", sensor->dev->name);
		return -ENODEV;
	}

	return sensor_api->set_interval(sensor->dev, interval);
}

static int config_interval(struct senss_sensor *sensor)
{
	uint32_t interval = arbitrate_interval(sensor);

	LOG_INF("%s(%d), interval:%d", __func__, __LINE__, interval);

	return set_arbitrate_interval(sensor, interval);
}

static uint32_t arbitrate_sensivitity(struct senss_sensor *sensor, int index)
{
	struct senss_connection *conn;
	uint32_t min_sensitivity = UINT32_MAX;

	/* search from all clients, arbitrate the sensitivity */
	for_each_client_conn(sensor, conn) {
		LOG_INF("%s, each sensor:%s, conn:%d, interval:%d, index:%d, sens:%d, min_sen:%d",
				__func__, sensor->dev->name, conn->index, conn->interval, index,
				conn->sensitivity[index], min_sensitivity);
		if (!is_client_request_data(conn)) {
			continue;
		}
		if (conn->sensitivity[index] < min_sensitivity) {
			min_sensitivity = conn->sensitivity[index];
		}
	}
	LOG_INF("%s, sensor:%s, min_sensitivity:%d", __func__, sensor->dev->name, min_sensitivity);

	/* min_sensitivity == UINT32_MAX means no client is requesting to open sensor,
	 * by any client, in this case, return sensitivity 0
	 */

	return (min_sensitivity == UINT32_MAX ? 0 : min_sensitivity);
}

static int set_arbitrate_sensitivity(struct senss_sensor *sensor, int index, uint32_t sensitivity)
{
	const struct senss_sensor_api *sensor_api;

	__ASSERT(sensor && sensor->dev,
			"set arbitrate sensitivity, sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "set arbitrate sensitivity, sensor device sensor_api is NULL");

	/* update sensor sensitivity */
	sensor->sensitivity[index] = sensitivity;

	if (!sensor_api->set_sensitivity) {
		LOG_WRN("sensor:%s set_sensitivity callback is not set", sensor->dev->name);
		/* sensor driver may not set sensivity callback, so no need to return error here */
		return 0;
	}

	return sensor_api->set_sensitivity(sensor->dev, index, sensitivity);
}

static int config_sensitivity(struct senss_sensor *sensor, int index)
{
	uint32_t sensitivity = arbitrate_sensivitity(sensor, index);

	LOG_INF("%s, sensor:%s, index:%d, sensitivity:%d",
		__func__, sensor->dev->name, index, sensitivity);

	return set_arbitrate_sensitivity(sensor, index, sensitivity);
}

static int config_sensor(struct senss_sensor *sensor)
{
	int ret;
	int i = 0;

	ret = config_interval(sensor);
	if (ret) {
		LOG_WRN("sensor:%s config interval error", sensor->dev->name);
	}

	for (i = 0; i < sensor->sensitivity_count; i++) {
		ret = config_sensitivity(sensor, i);
		if (ret) {
			LOG_WRN("sensor:%s config sensitivity index:%d error",
					sensor->dev->name, i);
		}
	}

	return ret;
}

static void sensor_later_config(struct senss_mgmt_context *ctx)
{
	struct senss_sensor *sensor;
	int i = 0;

	for_each_sensor_reverse(ctx, i, sensor) {
		if (atomic_test_and_clear_bit(&sensor->flag, SENSOR_LATER_CFG_BIT)) {
			LOG_INF("%s, reverse_index:%d, sensor:%s", __func__, i, sensor->dev->name);
			config_sensor(sensor);
		}
	}
}

void sensor_event_process(struct senss_mgmt_context *ctx, k_timeout_t timeout)
{
	int ret;

	ret = k_sem_take(&ctx->event_sem, timeout);
	if (!ret) {
		if (atomic_test_and_clear_bit(&ctx->event_flag, EVENT_CONFIG_READY)) {
			LOG_INF("%s, event_config ready", __func__);
			sensor_later_config(ctx);
		}
		if (atomic_test_and_clear_bit(&ctx->event_flag, EVENT_DATA_READY)) {
			LOG_INF("%s, event_data ready", __func__);
		}
	}
}
