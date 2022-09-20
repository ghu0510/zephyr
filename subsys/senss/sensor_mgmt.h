/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_MGMT_H_
#define SENSOR_MGMT_H_

#include <senss/senss_datatypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_REPORTER_COUNT CONFIG_SENSS_MAX_REPORTER_COUNT

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
	.reporter_num = PHANDLES_IDX_LIST_LEN(node, reporters),	\
	.reporters = PHANDLES_IDX_LIST(node, reporters),		\
},

/**
 * @struct senss_sensor_dt_info
 * @brief Sensor device tree data structure
 */
struct senss_sensor_dt_info {
	const char *label;
	struct senss_sensor_info info;
	uint16_t reporter_num;
	uint16_t reporters[MAX_REPORTER_COUNT];
};

/**
 * @}
 */

#ifdef __cplusplus
	}
#endif


#endif /* SENSOR_MGNT_H_ */
