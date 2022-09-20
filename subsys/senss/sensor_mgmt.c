/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
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

	config->interval = SENSOR_INTERVAL_MAX;
	config->sensitivity_count = get_max_valid_index(type);

	__ASSERT(config->sensitivity_count <= MAX_SENSITIVITY_COUNT,
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
	conn->interval = SENSOR_INTERVAL_MAX;
	for (i = 0; i < MAX_SENSITIVITY_COUNT; i++) {
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
		__ASSERT(!reporter_sensor, "sensor's reporter should not be NULL");
		/* device tree required sensor connection cannot be opened or closed any more,
		 * so it is called fixed connection. dynamic: false
		 */
		init_each_connection(ctx, conn, reporter_sensor, sensor, false);
		conn->index = ctx->fixed_connection_count++;
		__ASSERT(conn->index < MAX_SENSOR_COUNT,
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

	__ASSERT(sensor->conns_num <= MAX_REPORTER_COUNT,
		"connection number:%d exceed max number:%d", sensor->conns_num, MAX_REPORTER_COUNT);

	for_each_sensor_connection(i, sensor, conn) {
		conn_idx[i] = conn->index;
	}
}

static int init_sensor(struct senss_mgmt_context *ctx, struct senss_sensor *sensor)
{
	const struct senss_sensor_api *sensor_api;
	int conn_idx[MAX_REPORTER_COUNT] = {0};

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

	return ret;
}
