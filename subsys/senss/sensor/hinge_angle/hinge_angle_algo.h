/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HINGE_ANGLE_ALGO_H_
#define HINGE_ANGLE_ALGO_H_

#include <zephyr/senss/senss_sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/* which axis is the hinge, X: 0, Y: 1, Z: 2 */
#define HINGE_ANGLE_HINGE_AXIS          0
/* which axis is the hinge, which set 1 */
#define HINGE_ANGLE_HINGE_VECTOR         { 1, 0, 0 }

#define HINGE_ANGLE_UG_PER_MG           1000
#define HINGE_ANGLE_MG_PER_G            1000

#define HINGE_ANGLE_MG_IN_1G            HINGE_ANGLE_UG_PER_MG
#define HINGE_ANGLE_MG_IN_8G            8000
#define HINGE_ANGLE_MG_IN_1G_SQUARE     (HINGE_ANGLE_MG_IN_1G * \
	HINGE_ANGLE_MG_IN_1G)
#define HINGE_ANGLE_MG_IN_8G_SQUARE     (HINGE_ANGLE_MG_IN_8G * \
	HINGE_ANGLE_MG_IN_8G)

#define HINGE_ANGLE_SCALED1K            1000
#define HINGE_ANGLE_SCALED10K           10000

#define HINGE_ANGLE_ACC_AXIS_CNT        3

/* acc deviation when weightless state, unit: mG */
#define HINGE_ANGLE_WEIGHTLESS_ACC_DEVIATION      100

/* acc deviation when hypergravity state, unit: mG */
#define HINGE_ANGLE_HYPERGRAVITY_ACC_DEVIATION    100
/* min acc axis value when hypergravity state, unit: mG */
#define HINGE_ANGLE_HYPERGRAVITY_ACC_MIN          \
	(HINGE_ANGLE_MG_IN_1G + HINGE_ANGLE_HYPERGRAVITY_ACC_DEVIATION)

enum hinge_angle_value_info {
	/* value not generated */
	HINGE_ANGLE_VALUE_NOT_GEN = 0,
	/* value first output when called process */
	HINGE_ANGLE_VALUE_FIRST,
	/* value no change */
	HINGE_ANGLE_VALUE_NO_CHANGE,
	/* value is changed */
	HINGE_ANGLE_VALUE_CHANGED
};

void *hinge_angle_algo_create(void);
void hinge_angle_algo_destroy(void *algo_handle);
void hinge_angle_algo_reset(void *algo_handle);
int hinge_angle_algo_collect_data_base_acc(void *algo_handle,
	struct senss_sensor_value_3d_int32 *acc);
int hinge_angle_algo_collect_data_lid_acc(void *algo_handle,
	struct senss_sensor_value_3d_int32 *acc);
int hinge_angle_algo_process(void *algo_handle,
	enum hinge_angle_value_info *value_info,
	struct senss_sensor_value_int32 **value);

#ifdef __cplusplus
	}
#endif

#endif /* HINGE_ANGLE_ALGO_H_ */
