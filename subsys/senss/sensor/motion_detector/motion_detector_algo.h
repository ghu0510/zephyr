/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTION_DETECTOR_ALGO_H_
#define MOTION_DETECTOR_ALGO_H_

#include <zephyr/senss/senss_sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_DETECTOR_UG_PER_MG       1000
#define MOTION_DETECTOR_MG_PER_G        1000

#define MOTION_DETECTOR_MG_IN_1G        MOTION_DETECTOR_UG_PER_MG
#define MOTION_DETECTOR_MG_IN_8G        8000
#define MOTION_DETECTOR_MG_IN_1G_SQUARE (MOTION_DETECTOR_MG_IN_1G * \
	MOTION_DETECTOR_MG_IN_1G)
#define MOTION_DETECTOR_MG_IN_8G_SQUARE (MOTION_DETECTOR_MG_IN_8G * \
	MOTION_DETECTOR_MG_IN_8G)

#define MOTION_DETECTOR_ACC_AXIS_CNT    3

/* max acc deviation when motionless state, unit: mG */
#define MOTION_DETECTOR_MOTIONLESS_ACC_DEVIATION  40
/* so long if the max diff meets the condition, judged as motionless */
#define MOTION_DETECTOR_MOTIONLESS_SECONDS        3

/* acc deviation when hypergravity state, unit: mG */
#define MOTION_DETECTOR_HYPERGRAVITY_ACC_DEVIATION          100
/* min acc axis value when hypergravity state, unit: mG */
#define MOTION_DETECTOR_HYPERGRAVITY_ACC_MIN                \
	(MOTION_DETECTOR_MG_IN_1G + MOTION_DETECTOR_HYPERGRAVITY_ACC_DEVIATION)

enum motion_detector_value_info {
	/* value not generated */
	MOTION_DETECTOR_VALUE_NOT_GEN = 0,
	/* value first output when called process */
	MOTION_DETECTOR_VALUE_FIRST,
	/* value no change */
	MOTION_DETECTOR_VALUE_NO_CHANGE,
	/* value is changed */
	MOTION_DETECTOR_VALUE_CHANGED
};

void *motion_detector_algo_create(void);
void motion_detector_algo_destroy(void *algo_handle);
void motion_detector_algo_reset(void *algo_handle);
int motion_detector_algo_collect_data_acc(void *algo_handle,
	struct senss_sensor_value_3d_int32 *acc);
int motion_detector_algo_process(void *algo_handle,
	enum motion_detector_value_info *value_info,
	struct senss_sensor_value_int32 **value);

#ifdef __cplusplus
	}
#endif

#endif /* MOTION_DETECTOR_ALGO_H_ */
