/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_MGMT_H_
#define SENSOR_MGMT_H_

#include <senss/senss_datatypes.h>
#include <senss_sensor.h>
#include <sys/slist.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* indicates that this sensor is not polling yet */
#define EXEC_TIME_OFF UINT64_MAX
/* indicates sensor is opened now */
#define SENSOR_INTERVAL_MAX UINT32_MAX
#define SENSOR_SENSITIVITY_MAX UINT32_MAX
#define EXEC_TIME_INIT 0
#define EXEC_TIME_OFF UINT64_MAX

#define SENSS_SENSOR_INFO(node)						\
{									\
	.dev = DEVICE_DT_GET(node),					\
	.info.type = DT_PROP(node, sensor_type),			\
	.info.sensor_index = DT_PROP_OR(node, sensor_index, 0),		\
	.info.vendor = DT_NODE_VENDOR_OR(node, NULL),			\
	.info.model = DT_PROP(node, model),				\
	.info.name = DT_NODE_FULL_NAME(node),				\
	.info.friendly_name = DT_PROP(node, friendly_name),		\
	.info.minimal_interval = DT_PROP(node, minimal_interval),	\
	.reporter_num = PHANDLES_IDX_LIST_LEN(node, reporters),		\
	.reporters = PHANDLES_IDX_LIST(node, reporters),		\
},

#define for_each_sensor(ctx, i, sensor)							\
	for (i = 0; i < ctx->sensor_num &&						\
		(sensor = ctx->sensor_db[ctx->sensor_db[i]->index]) != NULL; i++)	\

#define for_each_sensor_connection(i, sensor, conn)			\
	for (i = 0; i < sensor->conns_num &&				\
		(conn = &sensor->conns[i]) != NULL; i++)		\

#define for_each_sensor_client(sensor, client)				\
	SYS_SLIST_FOR_EACH_CONTAINER(&sensor->client_list, client, snode)

#define for_each_sensor_config(ctx, sensor, tmp)			\
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&ctx->cfg_list, sensor, tmp, cfg_node)

#if CONFIG_SENSS_RUNTIME_THREAD_PRIORITY < -16 || CONFIG_SENSS_RUNTIME_THREAD_PRIORITY >= 15
#error "Invalid runtime thread priority"
#endif

#if CONFIG_SENSS_DISPATCH_THREAD_PRIORITY < -16 || CONFIG_SENSS_DISPATCH_THREAD_PRIORITY >= 15
#error "Invalid dispatch thread priority"
#endif

enum sensor_trigger_mode {
	SENSOR_TRIGGER_MODE_POLLING = 1,
	SENSOR_TRIGGER_MODE_DATA_READY = 2,
};

enum {
	EVENT_DATA_READY,
	EVENT_CONFIG_READY,
};
/**
 * @struct senss_sensor_dt_info
 * @brief Sensor device tree data structure
 */
struct senss_sensor_dt_info {
	const struct device *dev;
	struct senss_sensor_info info;
	uint16_t reporter_num;
	uint16_t reporters[CONFIG_SENSS_MAX_REPORTER_COUNT];
};

/**
 * @struct sensor_config
 * @brief config data to be used by sensor
 */
struct sensor_config {
	uint32_t interval;
	uint8_t sensitivity_count;
	int sensitivity[CONFIG_SENSS_MAX_SENSITIVITY_COUNT];
};

struct sensor_sample {
	uint16_t size;
	void *data;
};

/**
 * @struct connection information
 * @brief connection indicates connection from reporter sensor(source) to client sensor(sink)
 */
struct connection {
	int index;
	struct senss_sensor *source;
	struct senss_sensor *sink;
	/* interval and sensitivity set from client(sink) to reporter(source) */
	uint32_t interval;
	int sensitivity[CONFIG_SENSS_MAX_SENSITIVITY_COUNT];
	/* copy sample to connection from reporter */
	struct sensor_sample sample;
	/* client(sink) next consume time */
	uint64_t next_consume_time;
	/* when new data arrive, set flag to true, after data processing, clear the flag */
	bool new_data_arrive;
	sys_snode_t snode;
	/* dynamic false: connection created during initial according to device tree
	 * dynamic true: connection created/closed from applcation: HID, CHRE, etc
	 */
	bool dynamic;
	/* post data to application */
	senss_data_event_t data_evt_cb;
	void *cb_param;
};

/**
 * @struct senss_sensor
 * @brief Internal sensor instance data structure.
 *
 * Each sensor instance will have its unique data structure for storing all
 * it's related information.
 *
 * Sensor management will enumerate all these instance data structures,
 * build report relationship model base on them, etc.
 */
struct senss_sensor {
	uint16_t index;
	/* device binding to sensor device tree */
	const struct device *dev;
	/* Common Register info */
	struct senss_sensor_dt_info *dt_info;
	int conns_num;
	struct connection *conns;
	/* client handles, updated in senss_init() */
	int clients_count;
	sys_slist_t client_list;
	struct sensor_config cfg;
	sys_snode_t cfg_node;
	enum senss_sensor_state state;
	enum sensor_trigger_mode mode;
	/* runtime info */
	uint64_t start_time;
	uint64_t next_exec_time;
	uint16_t data_size;
	void *data_buf;
};

struct senss_mgmt_context {
	bool senss_initialized;
	int sensor_num;
	struct senss_sensor_info *info;
	int fixed_connection_count;
	uint16_t count;
	struct senss_sensor *sensor_db[CONFIG_SENSS_MAX_SENSOR_COUNT];
	struct connection *conns[CONFIG_SENSS_MAX_HANDLE_COUNT];
	struct k_sem dispatch_sem;
	struct k_sem event_sem;
	atomic_t event_flag;
	struct k_mutex rpt_mutex;
	struct k_mutex cfg_mutex;
	struct k_thread runtime_thread;
	struct k_thread dispatch_thread;
	sys_slist_t cfg_list;
	struct ring_buf sensor_ring_buf;
	uint8_t buf[CONFIG_SENSS_RING_BUF_SIZE];
	bool data_to_ring_buf;
};

struct sensor_data_headar {
	uint16_t data_size;
	uint16_t conn_index;
} __packed;

struct senss_mgmt_context *get_senss_ctx(void);
void senss_runtime_thread(void *p1, void *p2, void *p3);
int open_sensor(int type, int instance);
int close_sensor(struct connection *conn);
int set_interval(struct connection *conn, uint32_t value);
int get_interval(struct connection *con, uint32_t *value);
int set_sensitivity(struct connection *conn, int index, uint32_t value);
int get_sensitivity(struct connection *con, int index, uint32_t *value);
int get_sensor_state(struct senss_sensor *sensor, enum senss_sensor_state *state);
const struct senss_sensor_info *get_sensor_info(struct senss_sensor *sensor);
int register_data_event_callback(struct connection *conn,
				 senss_data_event_t callback,
				 void *param);
int read_sample(struct senss_sensor *sensor, void *buf, int size);
void sensor_event_process(struct senss_mgmt_context *ctx, k_timeout_t timeout);

static inline uint64_t get_us(void)
{
	return k_cycle_get_64() * USEC_PER_SEC / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
}

static inline struct senss_sensor *get_reporter_sensor(struct senss_mgmt_context *ctx,
						       struct senss_sensor *sensor,
						       int index)
{
	if (!sensor || index >= sensor->conns_num ||
		sensor->dt_info->reporters[index] >= CONFIG_SENSS_MAX_SENSOR_COUNT) {
		return NULL;
	}

	return ctx->sensor_db[sensor->dt_info->reporters[index]];
}

static inline bool is_phy_sensor(struct senss_sensor *sensor)
{
	return sensor->dt_info->reporter_num == 0;
}

static inline bool is_virtual_sensor(struct senss_sensor *sensor)
{
	return sensor->dt_info->reporter_num > 0;
}

static inline struct senss_sensor *get_sensor_by_dev(const struct device *dev)
{
	return dev ? (struct senss_sensor *)((struct senss_sensor_ctx *)dev->data)->priv_ptr : NULL;
}

static inline struct connection *get_connection_by_handle(struct senss_mgmt_context *ctx,
							  int handle)
{
	if (handle >= CONFIG_SENSS_MAX_HANDLE_COUNT) {
		return NULL;
	}

	return ctx->conns[handle];
}

static inline int find_first_free_connection(struct senss_mgmt_context *ctx)
{
	int i;

	for (i = ctx->fixed_connection_count; i < CONFIG_SENSS_MAX_SENSOR_COUNT; i++) {
		if (!ctx->conns[i]) {
			break;
		}
	}

	return i;
}

static inline bool cfg_list_has_sensor(struct senss_mgmt_context *ctx,
				       struct senss_sensor *sensor)
{
	struct senss_sensor *tmp_sensor, *tmp;

	for_each_sensor_config(ctx, tmp_sensor, tmp) {
		if (!memcmp(tmp_sensor, sensor, sizeof(struct senss_sensor))) {
			return true;
		}
	}

	return false;
}

/* this function is used to decide whether filtering sensitivity checking
 * for example: filter sensitivity checking if sensitivity value is 0.
 */
static inline bool is_filtering_sensitivity(struct sensor_config *cfg)
{
	bool filtering = false;

	for (int i = 0; i < CONFIG_SENSS_MAX_SENSITIVITY_COUNT; i++) {
		if (cfg->sensitivity[i] != 0) {
			filtering = true;
			break;
		}
	}

	return filtering;
}


static inline bool is_sensor_state_ready(struct senss_sensor *sensor)
{
	return (sensor->state == SENSS_SENOSR_STATE_READY);
}

/* when reporter post data to client, new_data_arrive flag will be set,
 * indicates sensor has new data arrive
 */
static inline bool sensor_has_new_data(const struct senss_sensor *sensor)
{
	for (int i = 0; i < sensor->conns_num; i++) {
		if (sensor->conns[i].new_data_arrive)
			return true;
	}

	return false;
}

/* if client has requested data from reporter, interval should be set first */
static inline bool is_client_request_data(struct connection *conn)
{
	return conn->interval != 0;
}

static inline bool is_sensor_opened(struct senss_sensor *sensor)
{
	return sensor->cfg.interval != 0;
}

/* sensor not in polling mode, meanwhile data ready arrived from phyisal driver */
static inline bool is_sensor_data_ready(struct senss_sensor *sensor)
{
	return sensor->mode == SENSOR_TRIGGER_MODE_DATA_READY;
}

static inline int get_max_valid_index(int32_t type)
{
	#define SENSS_SENSOR_TYPE_COLOR_ALS 0x141
	#define SENSS_SENSOR_TYPE_ACC 0x73
	#define SENSS_SENSOR_TYPE_GYRO 0x76

	int max_valid_index = 1;

	switch (type) {
	case SENSS_SENSOR_TYPE_COLOR_ALS:
		max_valid_index = 5;
		break;
	case SENSS_SENSOR_TYPE_ACC:
	case SENSS_SENSOR_TYPE_GYRO:
		max_valid_index = 3;
		break;
	default:
		break;
	}

	return max_valid_index;
}
/**
 * @}
 */

#ifdef __cplusplus
	}
#endif


#endif /* SENSOR_MGNT_H_ */
