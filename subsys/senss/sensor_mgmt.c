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
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include <senss_sensor.h>
#include <sensor_mgmt.h>

#define DT_DRV_COMPAT zephyr_senss

LOG_MODULE_REGISTER(senss, CONFIG_SENSS_LOG_LEVEL);

/* Enumerated sensors from device tree */
static struct senss_sensor_dt_info sensors_dt[] = {
	DT_FOREACH_CHILD(DT_DRV_INST(0), SENSS_SENSOR_INFO)
};
static struct senss_mgmt_context senss_ctx = {0};

K_THREAD_STACK_DEFINE(runtime_stack, CONFIG_SENSS_RUNTIME_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(dispatch_stack, CONFIG_SENSS_DISPATCH_THREAD_STACK_SIZE);

static int fetch_data_and_dispatch(struct senss_mgmt_context *ctx)
{
	struct sensor_data_headar *header;
	struct connection *conn;
	uint8_t buf[CONFIG_SENSS_MAX_SENSOR_DATA_SIZE];
	uint32_t wanted_size = sizeof(*header);
	uint32_t ret_size, rd_size = 0;
	uint16_t data_size = 0;
	uint16_t conn_index = 0;
	int ret = 0;

	while ((ret_size = ring_buf_get(&ctx->sensor_ring_buf, buf + rd_size, wanted_size)) > 0) {
		rd_size += ret_size;
		if (rd_size == sizeof(*header)) {
			header = (struct sensor_data_headar *)buf;
			data_size = header->data_size;
			__ASSERT(data_size + sizeof(*header) <= CONFIG_SENSS_MAX_SENSOR_DATA_SIZE,
						"invalid data size:%d", header->data_size);
			conn_index = header->conn_index;
			__ASSERT(conn_index < CONFIG_SENSS_MAX_HANDLE_COUNT,
						"invalid connection index:%d", conn_index);
			/* get data_size from header, and then read sensor data from ring buf */
			wanted_size = data_size;
		} else if (rd_size == sizeof(*header) + wanted_size) {
			conn = ctx->conns[conn_index];
			if (!conn || !conn->data_evt_cb) {
				LOG_WRN("%s, connection is NULL or event callback isn't registered",
								__func__);
				continue;
			}
			ret = conn->data_evt_cb((int)conn_index, buf + sizeof(*header),
								data_size, conn->cb_param);
			/* read next sample header */
			wanted_size = sizeof(*header);
			rd_size = 0;
		} else {
			LOG_ERR("%s, invalid ret_size:%d, rd_size:%d", __func__, ret_size, rd_size);
			ret = -EINVAL;
		}
	}

	if (ret_size == 0 && wanted_size != sizeof(*header)) {
		LOG_ERR("%s(%d), ret_size:%d, wanted_size:%d",
					__func__, __LINE__, ret_size, wanted_size);
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

struct senss_mgmt_context *get_senss_ctx(void)
{
	return &senss_ctx;
}

/* allocate memory for struct senss_sensor
 * connections number: senss creates a connection between client and its each reporter
 */
static struct senss_sensor *allocate_sensor(uint16_t conns_num, uint16_t sample_size)
{
	struct senss_sensor *sensor;
	int size, sensor_size, conn_size;

	sensor_size = sizeof(*sensor);
	conn_size = (conns_num * sizeof(struct connection));
	/* total memory to be alloctead for a sensor */
	size = sensor_size + conn_size + sample_size;
	sensor = malloc(size);
	__ASSERT(sensor, "alloc memory for senss_sensor error");

	sensor->data_size = sample_size;
	sensor->conns = (struct connection *)(sensor + 1);
	sensor->data_buf = sensor->conns + conns_num;

	LOG_INF("%s, conns_num:%d", __func__, conns_num);

	/* physical sensor has no reporter, connections number is 0 */
	if (conns_num == 0) {
		sensor->conns = NULL;
	}

	sys_slist_init(&sensor->client_list);

	return sensor;
}

static void init_sensor_config(struct sensor_config *config, int type)
{
	int i;

	config->interval = 0;
	config->sensitivity_count = get_max_valid_index(type);

	__ASSERT(config->sensitivity_count <= CONFIG_SENSS_MAX_SENSITIVITY_COUNT,
			"sensitivity count:%d should not exceed MAX_SENSITIVITY_COUNT",
			config->sensitivity_count);

	for (i = 0; i < config->sensitivity_count; i++) {
		config->sensitivity[i] = SENSOR_SENSITIVITY_MAX;
	}
}

/* create struct senss_sensor *sensor according to sensor device tree */
static struct senss_sensor *create_sensor_obj(struct senss_sensor_dt_info *dt)
{
	struct senss_sensor_ctx *sensor_ctx;
	struct senss_sensor *sensor;

	__ASSERT(dt && dt->dev, "device tree info or senss dev is invalid");
	sensor_ctx = dt->dev->data;
	__ASSERT(sensor_ctx, "senss sensor context is invalid");

	sensor = allocate_sensor(dt->reporter_num, sensor_ctx->register_info->sample_size);

	__ASSERT(sensor, "senss_sensor is invalid");

	sensor->dev = dt->dev;
	/* sensor->dt_info pointed to sensor device tree */
	sensor->dt_info = dt;
	sensor->dt_info->info.flags = sensor_ctx->register_info->flags;
	sensor->dt_info->info.version = sensor_ctx->register_info->version;

	LOG_INF("%s, sensor:%s, min_ri:%d(us)",
		__func__, sensor->dev->name, sensor->dt_info->info.minimal_interval);

	init_sensor_config(&sensor->cfg, sensor->dt_info->info.type);

	sensor->state = SENSS_SENOSR_STATE_NOT_READY;

	sensor_ctx->priv_ptr = sensor;

	return sensor;
}

static void switch_sequence(struct senss_mgmt_context *ctx, uint16_t src, uint16_t dest)
{
	uint8_t tmp_index = ctx->sensor_db[src]->index;

	LOG_INF("%s(%d), src:%d,index:%d, dest:%d,index:%d",
		__func__, __LINE__, src, ctx->sensor_db[src]->index,
		dest, ctx->sensor_db[dest]->index);

	ctx->sensor_db[src]->index = ctx->sensor_db[dest]->index;
	ctx->sensor_db[dest]->index = tmp_index;
}

/* sorting rule: (TBD will be replaced by topologic sorting) */
static void sort_sensors(struct senss_mgmt_context *ctx)
{
	struct senss_sensor *sensor;
	uint16_t client_index;
	uint16_t reporter_index;
	int i = 0, j = 0;

	for (i = 0; i < ctx->sensor_num; i++) {
		sensor = ctx->sensor_db[i];
		/* enumerate each sensor connection, if client is sorted before connection,
		 * switch the order to ensure client is sequenced after connection
		 */
		for (j = 0; j < sensor->dt_info->reporter_num; j++) {
			client_index = ctx->sensor_db[i]->index;
			reporter_index = ctx->sensor_db[sensor->dt_info->reporters[j]]->index;
			if (client_index < reporter_index)
				switch_sequence(ctx, client_index, reporter_index);
		}
	}
}

static void init_each_connection(struct senss_mgmt_context *ctx,
				struct connection *conn,
				struct senss_sensor *source,
				struct senss_sensor *sink,
				bool dynamic)
{
	int i;

	__ASSERT(conn, "invalid connection");
	conn->source = source;
	conn->sink = sink;
	conn->dynamic = dynamic;
	/* malloc connection sample buf to copy data from source data buf */
	conn->sample.data = malloc(source->data_size);
	__ASSERT(conn->sample.data, "alloc memory for sample data error");
	conn->sample.size = source->data_size;
	conn->interval = 0;

	for (i = 0; i < CONFIG_SENSS_MAX_SENSITIVITY_COUNT; i++) {
		conn->sensitivity[i] = SENSOR_SENSITIVITY_MAX;
	}
}

/* initial sensor connections: connection between client and its reporter */
static void init_sensor_connections(struct senss_mgmt_context *ctx, struct senss_sensor *sensor)
{
	struct senss_sensor *reporter_sensor;
	struct connection *conn;
	int i;

	sensor->conns_num = sensor->dt_info->reporter_num;
	if (sensor->conns_num == 0) {
		__ASSERT(!sensor->conns, "conns should be NULL if sensor has no connection");
		return;
	}

	/* initial each connection for sensors who has reporters */
	for_each_sensor_connection(i, sensor, conn) {
		reporter_sensor = get_reporter_sensor(ctx, sensor, i);
		__ASSERT(reporter_sensor, "sensor's reporter should not be NULL");
		/* device tree required sensor connection cannot be opened or closed any more,
		 * so it is called fixed connection. dynamic: false
		 */
		init_each_connection(ctx, conn, reporter_sensor, sensor, false);
		conn->index = ctx->fixed_connection_count++;
		LOG_INF("%s, report:%s, client:%s, connection:%d",
			__func__, reporter_sensor->dev->name, sensor->dev->name, conn->index);
		__ASSERT(conn->index < CONFIG_SENSS_MAX_SENSOR_COUNT,
			"sensor connection number:%d exceed MAX_SENSOR_COUNT", conn->index);

		ctx->conns[conn->index] = conn;
		/* link connection to its repoter's client_list */
		sys_slist_append(&reporter_sensor->client_list, &conn->snode);
		reporter_sensor->clients_count++;
	}
}

static void get_connections_index(struct senss_mgmt_context *ctx,
				 struct senss_sensor *sensor,
				 int *conn_idx)
{
	struct connection *conn;
	int i;

	if (sensor->conns_num == 0) {
		__ASSERT(!sensor->conns, "conns should be NULL if sensor has no connection");
		return;
	}

	__ASSERT(sensor->conns_num <= CONFIG_SENSS_MAX_REPORTER_COUNT,
				"connection number:%d exceed max number:%d",
				sensor->conns_num, CONFIG_SENSS_MAX_REPORTER_COUNT);

	for_each_sensor_connection(i, sensor, conn) {
		conn_idx[i] = conn->index;
	}
}

static int init_sensor(struct senss_mgmt_context *ctx, struct senss_sensor *sensor)
{
	const struct senss_sensor_api *sensor_api;
	int conn_idx[CONFIG_SENSS_MAX_REPORTER_COUNT] = {0};

	__ASSERT(sensor && sensor->dev, "sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "sensor device sensor_api is NULL");

	init_sensor_connections(ctx, sensor);

	get_connections_index(ctx, sensor, conn_idx);

	/* physical sensor is working at polling mode by default,
	 * virtual sensor working mode is inherited from its reporter
	 */
	if (is_phy_sensor(sensor)) {
		sensor->mode = SENSOR_TRIGGER_MODE_POLLING;
	}

	return sensor_api->init(sensor->dev, &sensor->dt_info->info, conn_idx, sensor->conns_num);
}

static int set_sensor_state(struct senss_sensor *sensor, enum senss_sensor_state state)
{
	__ASSERT(sensor, "senss_sensor is NULL");

	sensor->state = state;

	return 0;
}

static void sensor_event_init(struct senss_mgmt_context *ctx)
{
	/* initial semaphore */
	k_sem_init(&ctx->dispatch_sem, 0, 1);
	k_sem_init(&ctx->event_sem, 0, 1);

	/* initial events */
	k_mutex_init(&ctx->rpt_mutex);
	k_mutex_init(&ctx->cfg_mutex);

	sys_slist_init(&ctx->cfg_list);
}

static struct senss_sensor *get_sensor_by_type_and_index(struct senss_mgmt_context *ctx,
							 int type,
							 int sensor_index)
{
	struct senss_sensor *sensor;
	int i = 0;

	for_each_sensor(ctx, i, sensor) {
		if (sensor->dt_info->info.type != type ||
			sensor->dt_info->info.sensor_index != sensor_index) {
			continue;
		}
		return sensor;
	}

	return NULL;
}

static void save_config_and_notify(struct senss_mgmt_context *ctx, struct senss_sensor *sensor)
{
	/* add sensor to config list first */
	if (!cfg_list_has_sensor(ctx, sensor)) {
		LOG_INF("%s, sensor:%s, append sensor to cfg_list", __func__, sensor->dev->name);
		/* sensor config may come from multiple applcations, so protect it */
		k_mutex_lock(&ctx->cfg_mutex, K_FOREVER);
		sys_slist_append(&ctx->cfg_list, &sensor->cfg_node);
		k_mutex_unlock(&ctx->cfg_mutex);
	}

	atomic_set_bit(&ctx->event_flag, EVENT_CONFIG_READY);
	k_sem_give(&ctx->event_sem);
}

static int set_reporter_interval(struct senss_mgmt_context *ctx,
				  struct senss_sensor *sensor,
				  struct connection *conn,
				  uint32_t interval)
{
	if (interval > 0 && interval < sensor->dt_info->info.minimal_interval) {
		LOG_ERR("interval:%d should not less than min interval:%d",
					interval, sensor->dt_info->info.minimal_interval);
		return -EINVAL;
	}

	conn->interval = interval;
	/* sensor configured with new interval, need to get new sample and restart counting again */
	conn->next_consume_time = EXEC_TIME_INIT;

	LOG_INF("%s, sensor:%s, mode:%d, dynamic connection:%d, interval:%d",
		__func__, sensor->dev->name, sensor->mode, conn->dynamic, interval);

	/* save current sensor config first, will uniformly config interval after enumerating
	 * all sensors to avoid multiple configuration to certain sensor
	 */
	save_config_and_notify(ctx, sensor);

	return 0;
}

static int set_reporter_sensitivity(struct senss_mgmt_context *ctx,
				    struct senss_sensor *sensor,
				    struct connection *conn,
				    int index,
				    uint32_t sensitivity)
{
	int i;

	LOG_INF("%s, sesnor:%s, index:%d, sensitivity:%d, sensitivity_count:%d",
				__func__, sensor->dev->name, index,
				sensitivity, sensor->cfg.sensitivity_count);

	if (index < SENSS_INDEX_ALL || index >= sensor->cfg.sensitivity_count) {
		LOG_ERR("sensor:%s sensitivity index:%d is invalid", sensor->dev->name, index);
		return -EINVAL;
	}

	if (index == SENSS_INDEX_ALL) {
		for (i = 0; i < sensor->cfg.sensitivity_count; i++) {
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

int senss_init(void)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor_dt_info *dt_info = &sensors_dt[0];
	struct senss_sensor *sensor;
	enum senss_sensor_state state;
	int ret = 0;
	int i;

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
		sensor = create_sensor_obj(dt_info);
		__ASSERT(sensor != NULL, "create sensor object error");
		/* update sensor_db according to sensor device tree sequence firstly,
		 * will topologically sort sensor sequence later
		 */
		sensor->index = i;
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
	}

	sensor_event_init(ctx);

	ring_buf_init(&ctx->sensor_ring_buf, sizeof(ctx->buf), ctx->buf);

	ctx->senss_initialized = true;

	/* sensor subsystem runtime thread: sensor scheduling and sensor data processing */
	k_thread_create(&ctx->runtime_thread, runtime_stack, CONFIG_SENSS_RUNTIME_THREAD_STACK_SIZE,
			(k_thread_entry_t) senss_runtime_thread, ctx, NULL, NULL,
			CONFIG_SENSS_RUNTIME_THREAD_PRIORITY, 0, K_NO_WAIT);

	/* sensor dispatch thread: get sensor data from senss and dispatch data */
	k_thread_create(&ctx->dispatch_thread, dispatch_stack,
			CONFIG_SENSS_DISPATCH_THREAD_STACK_SIZE,
			(k_thread_entry_t) senss_dispatch_thread, ctx, NULL, NULL,
			CONFIG_SENSS_DISPATCH_THREAD_PRIORITY, 0, K_NO_WAIT);

	return ret;
}

int senss_deinit(void)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct connection *conn;
	struct senss_sensor *sensor;
	int ret = 0;
	int i, j;

	/* close all opened sensors */
	for (i = 0; i < CONFIG_SENSS_MAX_HANDLE_COUNT; i++) {
		conn = ctx->conns[i];
		if (!conn) {
			continue;
		}
		ret |= close_sensor(conn);
		if (ret) {
			LOG_ERR("close connection:%d failed", conn->index);
		}
	}

	/* free connection sample data and sensor */
	for_each_sensor(ctx, i, sensor) {
		LOG_INF("%s, free sensor:%s data and senss_sensor", __func__, sensor->dev->name);
		for_each_sensor_connection(j, sensor, conn) {
			free(conn->sample.data);
		}
		free(sensor);
	}

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
		tmp_info = ctx->info;
		for_each_sensor(ctx, i, sensor) {
			*tmp_info++ = sensor->dt_info->info;
		}
	}
	*info = ctx->info;

	return ctx->sensor_num;
}

int open_sensor(int type, int sensor_index)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor *reporter_sensor, *client_sensor;
	struct connection *conn;
	uint16_t conns_num = 1;

	/* get the reporter sensor to be opened */
	reporter_sensor = get_sensor_by_type_and_index(ctx, type, sensor_index);
	if (!reporter_sensor) {
		LOG_ERR("no sensor match to type:0x%x, index:%d", type, sensor_index);
		return SENSS_SENSOR_INVALID_HANDLE;
	}

	/* allocate sensor for application client */
	client_sensor = allocate_sensor(conns_num, 0);
	__ASSERT(client_sensor, "allocate client senss_sensor error");

	init_sensor_config(&client_sensor->cfg, type);

	client_sensor->conns_num = conns_num;
	conn = &client_sensor->conns[0];

	/* create connection between client and reporter */
	init_each_connection(ctx, conn, reporter_sensor, client_sensor, true);

	conn->index = find_first_free_connection(ctx);
	__ASSERT(conn->index < CONFIG_SENSS_MAX_SENSOR_COUNT,
		"connection index:%d should less than MAX_SENSOR_COUNT", conn->index);

	/* multi applications could open sensor simultaneously, mutex protect to global variable */
	k_mutex_lock(&ctx->rpt_mutex, K_FOREVER);
	ctx->conns[conn->index] = conn;
	sys_slist_append(&reporter_sensor->client_list, &conn->snode);
	reporter_sensor->clients_count++;
	k_mutex_unlock(&ctx->rpt_mutex);

	LOG_INF("open_sensor_successfully, sensor:%s, state:0x%x, conn_index:%d",
			reporter_sensor->dev->name, reporter_sensor->state, conn->index);

	return conn->index;
}

int close_sensor(struct connection *conn)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor *reporter, *client;
	sys_snode_t *prev = NULL;
	struct connection *tmp_conn;

	__ASSERT(conn && conn->source && conn->sink,
		"connection or reporter or client should not be NULL");

	reporter = conn->source;
	client = conn->sink;

	/* remove client node from client_list here */
	for_each_sensor_client(reporter, tmp_conn) {
		if (tmp_conn->sink != client) {
			prev = &tmp_conn->snode;
		} else {
			sys_slist_remove(&reporter->client_list, prev, &tmp_conn->snode);
			break;
		}
	}
	__ASSERT(conn->index < CONFIG_SENSS_MAX_SENSOR_COUNT,
		"sensor connection number:%d exceed MAX_SENSOR_COUNT", conn->index);

	if (client->conns != conn) {
		LOG_ERR("cannot find sensor:%d to be closed", conn->index);
		return -ENODEV;
	}

	LOG_INF("%s ready, connection:%d", __func__, conn->index);

	ctx->conns[conn->index] = NULL;
	free(client);
	memset(tmp_conn, 0x00, sizeof(*tmp_conn));

	return 0;
}

/* client sensor request to set interval to connection sensor */
int set_interval(struct connection *conn, uint32_t interval)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor *reporter_sensor;
	struct connection *tmp_conn;

	__ASSERT(conn && conn->source, "connection or reporter should not be NULL");
	reporter_sensor = conn->source;

	LOG_INF("%s, sensor:%s, conn:%d, dynamic_connection:%d, interval:%u",
		__func__, reporter_sensor->dev->name, conn->index, conn->dynamic, interval);

	for_each_sensor_client(reporter_sensor, tmp_conn) {
		if (tmp_conn == conn) {
			return set_reporter_interval(ctx, reporter_sensor, conn, interval);
		}
	}

	LOG_ERR("cannot set sensor:%s interval", reporter_sensor->dev->name);

	return -EINVAL;
}

/* sensor interval is arbitrated by all clients, get_interval() would return interval set by
 * client from struct connection
 */
int get_interval(struct connection *conn, uint32_t *value)
{
	__ASSERT(conn, "connection should not be NULL");
	*value = conn->interval;

	return 0;
}

/* client sensor request to set sensitivity to connection sensor
 * 1) struct senss_sensor *sensor: conn sensor, whose sensitivity to be set
 * 2) struct senss_sensor *sensor: client sensor, who will set sensitivity to its conn
 * 3) index: which sensitivity item to be set
 * 4) value: sensitiviyt value to be set
 */
int set_sensitivity(struct connection *conn, int index, uint32_t value)
{
	struct senss_mgmt_context *ctx = get_senss_ctx();
	struct senss_sensor *reporter_sensor;
	struct connection *tmp_conn;

	__ASSERT(conn && conn->source, "connection or reporter should not be NULL");

	reporter_sensor = conn->source;

	LOG_INF("%s, sensor:%s, conn:%d, dynamic_connection:%d, index:%d, sensitivity:%u",
		__func__, reporter_sensor->dev->name, conn->index, conn->dynamic, index, value);

	for_each_sensor_client(reporter_sensor, tmp_conn) {
		if (tmp_conn == conn) {
			return set_reporter_sensitivity(ctx, reporter_sensor, conn, index, value);
		}
	}

	LOG_ERR("cannot set sensor:%s sensitivity", reporter_sensor->dev->name);

	return -EINVAL;
}

/* sensor sensitivity is arbitrated by all clients, get_sensitivity() would return interval set by
 * client from struct connection
 */
int get_sensitivity(struct connection *conn, int index, uint32_t *value)
{
	struct senss_sensor *sensor;
	int i = 0;

	__ASSERT(conn && conn->source, "connection or reporter should not be NULL");

	sensor = conn->source;

	*value = UINT32_MAX;

	if (index < SENSS_INDEX_ALL || index >= sensor->cfg.sensitivity_count) {
		LOG_ERR("sensor:%s sensitivity index:%d is invalid", sensor->dev->name, index);
		return -EINVAL;
	}

	if (index == SENSS_INDEX_ALL) {
		/* each sensitivity index value should be same for global sensitivity */
		for (i = 1; i < sensor->cfg.sensitivity_count; i++) {
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
	__ASSERT(sensor, "senss_sensor is NULL");

	if (!sensor) {
		LOG_ERR("cannot find sensor");
		return -ENODEV;
	}

	if (size != sensor->data_size) {
		LOG_ERR("sensor size:%d is not match with required:%d",
						sensor->data_size, size);
		return -ERANGE;
	}
	memcpy(buf, sensor->data_buf, size);

	return 0;
}

int register_data_event_callback(struct connection *conn,
				 senss_data_event_t callback,
				 void *param)
{
	__ASSERT(conn, "connection should not be NULL");

	conn->data_evt_cb = callback;
	conn->cb_param = param;

	return 0;
}

const struct senss_sensor_info *get_sensor_info(struct senss_sensor *sensor)
{
	__ASSERT(sensor, "senss_sensor is NULL");

	return &sensor->dt_info->info;
}

int get_sensor_state(struct senss_sensor *sensor, enum senss_sensor_state *state)
{
	__ASSERT(sensor, "senss_sensor is NULL");
	*state = sensor->state;

	return 0;
}

int senss_sensor_post_data(const struct device *dev, void *buf, int size)
{
	struct senss_sensor *sensor = get_sensor_by_dev(dev);

	__ASSERT(sensor, "senss_sensor is NULL");

	if (size != sensor->data_size) {
		LOG_ERR("post size:%d should be same as sensor data size:%d",
			size, sensor->data_size);
		return -EINVAL;
	}
	LOG_DBG("%s, sensor:%s, data_size:%d", __func__, sensor->dev->name, sensor->data_size);

	memcpy(sensor->data_buf, buf, size);

	return 0;
}

int senss_sensor_notify_data_ready(const struct device *dev)
{
	struct senss_sensor *sensor = get_sensor_by_dev(dev);
	struct senss_mgmt_context *ctx = get_senss_ctx();

	__ASSERT(sensor, "senss_sensor is NULL");

	LOG_INF("%s: %s data ready", __func__, sensor->dev->name);

	if (sensor->mode != SENSOR_TRIGGER_MODE_DATA_READY) {
		LOG_ERR("sensor:%s is not in data ready mode", sensor->dev->name);
		return -EINVAL;
	}

	atomic_set_bit(&ctx->event_flag, EVENT_DATA_READY);
	k_sem_give(&ctx->event_sem);

	return 0;
}

int senss_sensor_set_data_ready(const struct device *dev, bool data_ready)
{
	struct senss_sensor *sensor = get_sensor_by_dev(dev);

	__ASSERT(sensor, "senss_sensor is NULL");

	sensor->mode = !data_ready ? SENSOR_TRIGGER_MODE_POLLING : SENSOR_TRIGGER_MODE_DATA_READY;
	LOG_INF("%s, sensor:%s, data_ready:%d, trigger_mode:%d",
			__func__, sensor->dev->name, data_ready, sensor->mode);

	return 0;
}


/* sensor_later_config including interval/sensitiviyt arbitrate and set,
 * which is called in rumtime thread
 */
static uint32_t arbitrate_interval(struct senss_sensor *sensor)
{
	struct connection *conn;
	uint32_t min_interval = SENSOR_INTERVAL_MAX;
	uint32_t interval;

	/* search from all clients, arbitrate the interval */
	for_each_sensor_client(sensor, conn) {
		LOG_DBG("%s, for each client, conn:%d, sensor:%s, mode:%d, interval:%d",
			__func__, conn->index, sensor->dev->name, sensor->mode, conn->interval);
		if (conn->interval != 0 && conn->interval < min_interval) {
			min_interval = conn->interval;
		}
	}
	/* min_interval == SENSOR_INTERVAL_MAX means sensor is not opened by any clients
	 * if no client open the sensor, interval should be 0
	 */
	interval = (min_interval == SENSOR_INTERVAL_MAX ? 0 : min_interval);

	LOG_INF("%s, sensor:%s, interval:%d, min_interval:%d, next_exec_time:%lld",
		__func__, sensor->dev->name, interval, min_interval, sensor->next_exec_time);

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

	__ASSERT(sensor && sensor->dev, "sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "sensor device sensor_api is NULL");

	sensor->cfg.interval = interval;
	/* reset sensor next_exec_time as soon as sensor interval is changed */
	sensor->next_exec_time = interval > 0 ? EXEC_TIME_INIT : EXEC_TIME_OFF;

	LOG_INF("%s, interval:%d, next_exec_time:%lld", __func__, interval, sensor->next_exec_time);

	if (!sensor_api->set_interval) {
		LOG_WRN("sensor:%s set_interval callback is not set yet", sensor->dev->name);
		return -ENODEV;
	}

	return sensor_api->set_interval(sensor->dev, interval);
}

static int config_interval(struct senss_sensor *sensor)
{
	uint32_t interval = arbitrate_interval(sensor);

	return set_arbitrate_interval(sensor, interval);
}

static uint32_t arbitrate_sensivitity(struct senss_sensor *sensor, int index)
{
	struct connection *conn;
	uint32_t min_sensitivity = SENSOR_SENSITIVITY_MAX;

	/* search from all clients, arbitrate the sensitivity */
	for_each_sensor_client(sensor, conn) {
		LOG_INF("%s, for each client, sensor:%s, conn:%d, index:%d, sens:%d, min_sen:%d",
					__func__, sensor->dev->name, conn->index, index,
					conn->sensitivity[index], min_sensitivity);
		if (conn->sensitivity[index] < min_sensitivity) {
			min_sensitivity = conn->sensitivity[index];
		}
	}
	LOG_INF("%s, min_sensitivity:%d", __func__, min_sensitivity);

	/* min_sensitivity == SENSOR_SENSITIVITY_MAX means sensitivity is not configured
	 * by any client, in this case, return sensitivity 0
	 */
	return (min_sensitivity == SENSOR_SENSITIVITY_MAX ? 0 : min_sensitivity);
}

static int set_arbitrate_sensitivity(struct senss_sensor *sensor, int index, uint32_t sensitivity)
{
	const struct senss_sensor_api *sensor_api;

	__ASSERT(sensor && sensor->dev, "sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "sensor device sensor_api is NULL");

	/* update sensor sensitivity */
	sensor->cfg.sensitivity[index] = sensitivity;

	if (!sensor_api->set_sensitivity) {
		LOG_WRN("sensor:%s set_sensitivity callback is not set", sensor->dev->name);
		return -ENODEV;
	}
	if (sensitivity == SENSOR_SENSITIVITY_MAX) {
		LOG_INF("sensitivity is not set by any client, ignore");
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

	for (i = 0; i < sensor->cfg.sensitivity_count; i++) {
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
	struct senss_sensor *sensor, *tmp;
	int virtual_cfg_cnt = 0;

	if (sys_slist_is_empty(&ctx->cfg_list)) {
		return;
	}

	LOG_INF("%s, config virtual sensor first", __func__);

	/* enumerate all virtual sensors first */
	do {
		virtual_cfg_cnt = 0;
		for_each_sensor_config(ctx, sensor, tmp) {
			if (is_phy_sensor(sensor)) {
				continue;
			}
			config_sensor(sensor);
			sys_slist_find_and_remove(&ctx->cfg_list, &sensor->cfg_node);
			virtual_cfg_cnt++;
		}
	} while (virtual_cfg_cnt);

	LOG_INF("%s, then config physical sensor", __func__);

	/* enumerate all physical sensors and config sensor */
	for_each_sensor_config(ctx, sensor, tmp) {
		if (is_virtual_sensor(sensor)) {
			continue;
		}
		config_sensor(sensor);
		sys_slist_find_and_remove(&ctx->cfg_list, &sensor->cfg_node);
	}
}

void sensor_event_process(struct senss_mgmt_context *ctx, k_timeout_t timeout)
{
	int ret;

	ret = k_sem_take(&ctx->event_sem, timeout);
	if (!ret) {
		if (atomic_test_and_clear_bit(&ctx->event_flag, EVENT_CONFIG_READY)) {
			LOG_INF("%s, config_ready", __func__);
			sensor_later_config(ctx);
		}
		if (atomic_test_and_clear_bit(&ctx->event_flag, EVENT_DATA_READY)) {
			LOG_INF("%s, data_ready", __func__);
		}
	}
}
