/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_MGMT_H_
#define SENSOR_MGMT_H_

#include <senss/senss_datatypes.h>
#include <sys/slist.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SENSOR_COUNT CONFIG_SENSS_MAX_SENSOR_COUNT
#define MAX_REPORTER_COUNT CONFIG_SENSS_MAX_REPORTER_COUNT
#define MAX_SENSITIVITY_COUNT CONFIG_SENSS_MAX_SENSITIVITY_COUNT
#define MAX_HANDLE_COUNT CONFIG_SENSS_MAX_HANDLE_COUNT

#define SENSOR_INTERVAL_MAX UINT32_MAX
#define SENSOR_SENSITIVITY_MAX UINT32_MAX
#define SENSS_SENSOR_TYPE_COLOR_ALS 0x141

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

enum sensor_trigger_mode {
	SENSOR_TRIGGER_MODE_POLLING = 1,
	SENSOR_TRIGGER_MODE_DATA_READY = 2,
};

/**
 * @struct senss_sensor_dt_info
 * @brief Sensor device tree data structure
 */
struct senss_sensor_dt_info {
	const struct device *dev;
	struct senss_sensor_info info;
	uint16_t reporter_num;
	uint16_t reporters[MAX_REPORTER_COUNT];
};

/**
 * @struct sensor_config
 * @brief config data to be used by sensor
 */
struct sensor_config {
	uint32_t interval;
	uint8_t sensitivity_count;
	int sensitivity[MAX_SENSITIVITY_COUNT];
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
	int sensitivity[MAX_SENSITIVITY_COUNT];
	/* copy sample to connection from reporter */
	struct sensor_sample sample;
	sys_snode_t snode;
	/* dynamic false: connection created during initial according to device tree
	 * dynamic true: connection created/closed from applcation: HID, CHRE, etc
	 */
	bool dynamic;
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
	struct senss_sensor *sensor_db[MAX_SENSOR_COUNT];
	struct connection *conns[MAX_HANDLE_COUNT];
};

struct senss_mgmt_context *get_senss_ctx(void);

static inline struct senss_sensor *get_reporter_sensor(struct senss_mgmt_context *ctx,
						       struct senss_sensor *sensor,
						       int index)
{
	if (!sensor || index >= sensor->conns_num ||
		sensor->dt_info->reporters[index] >= MAX_SENSOR_COUNT) {
		return NULL;
	}

	return ctx->sensor_db[sensor->dt_info->reporters[index]];
}
static inline bool is_phy_sensor(struct senss_sensor *sensor)
{
	return sensor->dt_info->reporter_num == 0;
}
static inline int get_max_valid_index(int32_t type)
{
	int max_valid_index = 1;

	switch (type) {
	case SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D:
	case SENSS_SENSOR_TYPE_MOTION_GYROMETER_3D:
		max_valid_index = 3;
		break;
	case SENSS_SENSOR_TYPE_COLOR_ALS:
		max_valid_index = 5;
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
