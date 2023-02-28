/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>
#include <zephyr/senss/senss_sensor.h>
#include "sensor_mgmt.h"

LOG_MODULE_DECLARE(senss, CONFIG_SENSS_LOG_LEVEL);

static uint64_t calc_next_poll_time(struct senss_mgmt_context *ctx)
{
	struct senss_sensor *sensor;
	uint64_t next_poll = EXEC_TIME_OFF;
	int i;

	for_each_sensor(ctx, i, sensor) {
		if (!is_sensor_state_ready(sensor))
			continue;
		if (!is_sensor_opened(sensor))
			continue;
		if (sensor->next_exec_time == EXEC_TIME_OFF) {
			continue;
		}
		next_poll = MIN(next_poll, sensor->next_exec_time);
	}

	return next_poll;
}

static int calc_sleep_time(struct senss_mgmt_context *ctx, uint64_t cur_time)
{
	uint64_t next_poll_time;
	uint32_t sleep_time;

	next_poll_time = calc_next_poll_time(ctx);
	if (next_poll_time == EXEC_TIME_OFF) {
		/* no sampling request. sleep forever */
		sleep_time = UINT32_MAX;
	} else if (next_poll_time <= cur_time) {
		sleep_time = 0;
	} else {
		sleep_time = (uint32_t)((next_poll_time - cur_time) / USEC_PER_MSEC);
	}

	LOG_DBG("%s, next:%lld, cur:%lld, sleep_time:%d(ms)",
				__func__, next_poll_time, cur_time, sleep_time);

	return sleep_time;
}

static void add_data_to_sensor_ring_buf(struct senss_mgmt_context *ctx,
					struct senss_sensor *sensor,
					struct senss_connection *conn)
{
	struct sensor_data_header *header;
	uint8_t data[CONFIG_SENSS_MAX_SENSOR_DATA_SIZE];
	uint32_t data_size;

	if (ring_buf_space_get(&ctx->sensor_ring_buf) < sizeof(*header) + sensor->data_size) {
		LOG_WRN("ring buffer will overflow, ignore the coming data");
		return;
	}
	__ASSERT(sizeof(*header) + sensor->data_size <= CONFIG_SENSS_MAX_SENSOR_DATA_SIZE,
		"data_size:%d is too large, should enlarge max_sensor_data_size:%d",
		sensor->data_size, CONFIG_SENSS_MAX_SENSOR_DATA_SIZE);

	header = (struct sensor_data_header *)data;
	header->data_size = sensor->data_size;
	header->conn_index = conn->index;

	memcpy(data + sizeof(*header), sensor->data_buf, sensor->data_size);

	data_size = ring_buf_put(&ctx->sensor_ring_buf, data, sizeof(*header) + sensor->data_size);
	__ASSERT(data_size == sizeof(*header) + sensor->data_size,
			"put data size:%d is not expected :%d",
			data_size, sizeof(*header) + sensor->data_size);

	LOG_DBG("%s, sensor:%s, conn_index:%d, data_size:%d",
		__func__, sensor->dev->name, conn->index, sensor->data_size);
}

static int sensor_sensitivity_test(struct senss_sensor *sensor,
				   struct senss_connection *conn)
{
	const struct senss_sensor_api *sensor_api;
	void *last_sample = conn->data;
	void *cur_sample = sensor->data_buf;
	int i;
	int ret = 0;

	__ASSERT(sensor && sensor->dev, "sensor sensitivity test, sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "sensor sensitivity test, sensor device sensor_api is NULL");

	if (!sensor_api->sensitivity_test) {
		LOG_ERR("sensor:%s not register sensitivity callback", sensor->dev->name);
		return -ENODEV;
	}
	for (i = 0; i < sensor->sensitivity_count; i++) {
		ret |= sensor_api->sensitivity_test(sensor->dev, i, sensor->sensitivity[i],
				last_sample, sensor->data_size, cur_sample, sensor->data_size);
	}

	return ret;
}

/* check whether the new sample passes the sensitivity test, send to client if passed */
static bool sensor_test_sensitivity(struct senss_sensor *sensor, struct senss_connection *conn)
{
	int ret = 0;

	/* always send the first sample to client */
	if (conn->next_consume_time == EXEC_TIME_INIT) {
		return true;
	}

	/* skip checking if sensitivity equals to 0 */
	if (!is_filtering_sensitivity(&sensor->sensitivity[0])) {
		return true;
	}

	/* call sensor sensitivity_test, ret:
	 * <0: means call sensor_sensitivity_test() failed
	 * 0: means sample delta less than sensitivity threshold
	 * 1: means sample data over sensitivity threshold
	 */
	ret = sensor_sensitivity_test(sensor, conn);
	if (ret > 0) {
		return true;
	}

	return false;
}

/* check whether it is right time for client to consume this sample */
static bool sensor_test_consume_time(struct senss_sensor *sensor,
				     struct senss_connection *conn,
				     uint64_t cur_time)
{
	uint64_t sample_time = ((struct senss_sensor_value_header *)
					sensor->data_buf)->base_timestamp;

	if (conn->next_consume_time <= sample_time)
		return true;


	LOG_DBG("sensor:%s data not ready, next_consume_time:%lld sample_time:%lld, cur_time:%lld",
			sensor->dev->name, conn->next_consume_time, sample_time, cur_time);

	return false;
}

static void update_client_consume_time(struct senss_sensor *sensor, struct senss_connection *conn)
{
	uint32_t interval = conn->interval;
	uint64_t sample_time = ((struct senss_sensor_value_header *)
						sensor->data_buf)->base_timestamp;

	LOG_DBG("%s, sensor:%s, conn:%d, next_consume:%lld, interval:%d, sample_time:%lld",
			__func__, sensor->dev->name, conn->index,
			conn->next_consume_time, interval, sample_time);

	if (conn->next_consume_time == 0 || conn->next_consume_time + interval < sample_time) {
		/* three cases start counting from last sample:
		 * 1) first sample arrived, next_consume_time is set to 0
		 * 2) samples dropped
		 * 3) data ready mode is also processed this way to avoid error accumulation
		 */
		conn->next_consume_time = sample_time + interval;
	} else {
		/* regular flow */
		conn->next_consume_time += interval;
	}
}

/* send data to clients based on interval and sensitivity */
static int send_data_to_clients(struct senss_mgmt_context *ctx,
				struct senss_sensor *sensor,
				uint64_t cur_time)
{
	struct senss_sensor *client;
	struct senss_connection *conn;
	bool sensi_pass = false;

	for_each_client_conn(sensor, conn) {
		client = conn->sink;
		LOG_INF("%s(%d), sensor:%s, connection:%d",
			__func__, __LINE__, conn->source->dev->name, conn->index);
		if (!is_client_request_data(conn)) {
			continue;
		}
		/* check whether time is ready or not:
		 * true: currently, it's time for client consume the data
		 * false: client time not arrived yet, not consume the data
		 */
		if (!sensor_test_consume_time(sensor, conn, cur_time)) {
			continue;
		}
		/* check sensitivity threshold passing or not, sensi_pass:
		 * true: sensitivity checking pass, could post the data
		 * false: sensitivity checking not pass, return
		 */
		sensi_pass = sensor_test_sensitivity(sensor, conn);

		update_client_consume_time(sensor, conn);

		if (!sensi_pass) {
			continue;
		}
		if (conn->sink) {
			/* pass the sensor mode to its client */
			client->mode = sensor->mode;
			/* if client sensor switch to polling mode, reset next_execute_time */
			if (client->mode == SENSOR_TRIGGER_MODE_POLLING &&
							client->next_exec_time == EXEC_TIME_OFF) {
				client->next_exec_time = EXEC_TIME_INIT;
			}
		}

		conn->new_data_arrive = true;

		memcpy(conn->data, sensor->data_buf, sensor->data_size);
		if (!conn->sink) {
			add_data_to_sensor_ring_buf(ctx, sensor, conn);
			ctx->data_to_ring_buf = true;
		}
	}

	/* notify dispatch thread to dispatch data to applicaiton */
	if (ctx->data_to_ring_buf) {
		k_sem_give(&ctx->dispatch_sem);
		ctx->data_to_ring_buf = false;
	}

	return 0;
}

static int process_streaming_data(struct senss_sensor *sensor, uint64_t cur_time)
{
	const struct senss_sensor_api *sensor_api;
	uint64_t next_time;
	uint64_t *sample_time;
	int ret = 0;

	__ASSERT(sensor && sensor->dev, "process streaming data, sensor or sensor device is NULL");

	sample_time = &((struct senss_sensor_value_header *)sensor->data_buf)->base_timestamp;
	/* sample time equals to 0 is for first sample,
	 * update sample time according to current time
	 */
	next_time = (*sample_time == 0 ? cur_time :
				MIN(cur_time, *sample_time + sensor->interval));

	LOG_INF("%s, sensor:%s, cur:%lld, sampe:%lld, ri:%d(us), next:%lld", __func__,
		sensor->dev->name, cur_time, *sample_time, sensor->interval, next_time);

	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "process streaming data, sensor device sensor_api is NULL");
	ret = sensor_api->read_sample(sensor->dev, sensor->data_buf, sensor->data_size);
	if (ret) {
		return ret;
	}
	/* update data sample time */
	*sample_time = next_time;

	return 0;
}

static int physical_sensor_process_data(struct senss_sensor *sensor, uint64_t cur_time)
{
	int ret = 0;

	ret = process_streaming_data(sensor, cur_time);
	if (ret)
		return ret;
	/* TODO: process fifo data*/

	return 0;
}

static int virtual_sensor_process_data(struct senss_sensor *sensor)
{
	const struct senss_sensor_api *sensor_api;
	struct senss_connection *conn;
	int ret = 0;
	int i;

	__ASSERT(sensor && sensor->dev,
			"virtual_sensorprocess_data, sensor or sensor device is NULL");
	sensor_api = sensor->dev->api;
	__ASSERT(sensor_api, "virtual sensor process data, sensor device sensor_api is NULL");

	/* enumerate each connection, and call process data for each connection,
	 * after data processing, clear new_data_arrvie flag
	 */
	for_each_reporter_conn(i, sensor, conn) {
		if (!conn->new_data_arrive) {
			continue;
		}
		ret |= sensor_api->process(sensor->dev,
					conn->index,
					conn->data,
					sensor->data_size);
		conn->new_data_arrive = false;
	}

	return ret;
}

static int sensor_process_data(struct senss_sensor *sensor, uint64_t cur_time)
{
	__ASSERT(sensor, "sensor process data, senss_sensor is NULL");

	if (is_virtual_sensor(sensor)) {
		return virtual_sensor_process_data(sensor);
	} else {
		return physical_sensor_process_data(sensor, cur_time);
	}

	return 0;
}

static int sensor_exec(struct senss_mgmt_context *ctx,
		       struct senss_sensor *sensor,
		       uint64_t cur_time)
{
	int ret = 0;

	ret = sensor_process_data(sensor, cur_time);
	if (ret)
		return ret;

	return send_data_to_clients(ctx, sensor, cur_time);
}

/* check sensor need to poll data or not, if need poll, update execute time
 * when time arriving
 */
static bool sensor_need_poll(struct senss_sensor *sensor, uint64_t cur_time)
{
	bool poll = false;

	/* sensor is not in polling mode or sensor interval still not set yet,
	 * no need to poll, return directly
	 */
	if (sensor->mode != SENSOR_TRIGGER_MODE_POLLING || sensor->interval == 0) {
		LOG_DBG("sensor %s not in polling mode:%d or sensor interval:%d not opened yet",
			sensor->dev->name, sensor->mode, sensor->interval);
		sensor->next_exec_time = EXEC_TIME_OFF;
		return false;
	}

	/* sensor is in polling mode, first time execute, will poll data at next interval */
	if (sensor->next_exec_time == EXEC_TIME_INIT) {
		LOG_INF("sensor:%s first time exe, cur time:%lld, interval:%d(us)",
				sensor->dev->name, cur_time, sensor->interval);
		sensor->next_exec_time = cur_time + sensor->interval;
		return false;
	}

	/* execute time arrived, excute this polling data, meanwhile calculate next execute time */
	if (sensor->next_exec_time <= cur_time) {
		poll = true;
		sensor->next_exec_time += sensor->interval;
	}

	LOG_INF("%s, sensor:%s, need_poll:%u, cur:%llu, next_exec_time:%llu, mode:%d",
		__func__, sensor->dev->name, poll, cur_time, sensor->next_exec_time, sensor->mode);

	return poll;
}

/* check whether sensor needs to be executed/processed */
static bool sensor_need_exec(struct senss_sensor *sensor, uint64_t cur_time)
{
	LOG_DBG("sensor:%s need to execute, next_exec_time:%lld, sensor_mode:%d, interval:%d",
		sensor->dev->name, sensor->next_exec_time, sensor->mode, sensor->interval);

	if (!is_sensor_opened(sensor)) {
		return false;
	}
	/* 1) check whether time arrived in polling mode, also update next execute time if arrived
	 * 2) async data (such as physical sensor data ready) mode
	 * 3) virtual sensor has new data arrived from repoter
	 */
	if (sensor_need_poll(sensor, cur_time) ||
			is_sensor_data_ready(sensor) ||
				sensor_has_new_data(sensor)) {
		return true;
	}

	return false;
}

static int sensor_loop(struct senss_mgmt_context *ctx, uint64_t cur_time)
{
	struct senss_sensor *sensor;
	int i = 0;
	int ret = 0;

	for_each_sensor(ctx, i, sensor) {
		if (!sensor_need_exec(sensor, cur_time)) {
			continue;
		}
		ret = sensor_exec(ctx, sensor, cur_time);
		if (ret) {
			LOG_WRN("sensor_exec error:%d", ret);
		}
	}

	return ret;
}

void senss_runtime_thread(void *p1, void *p2, void *p3)
{
	struct senss_mgmt_context *ctx = p1;
	k_timeout_t timeout;
	uint64_t cur_time;
	int sleep_time;

	LOG_INF("%s start...", __func__);

	do {
		cur_time = get_us();

		sensor_loop(ctx, cur_time);

		sleep_time = calc_sleep_time(ctx, cur_time);

		timeout = (sleep_time == UINT32_MAX ? K_FOREVER : K_MSEC(sleep_time));
		sensor_event_process(ctx, timeout);
	} while (1);
}
