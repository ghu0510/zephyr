/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HINGE_ANGLE_H_
#define HINGE_ANGLE_H_

#include <senss_sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hinge_angle_context {
	uint32_t interval;
	uint32_t sensitivity;
	int32_t base_acc_handle;
	int32_t lid_acc_handle;
	void *algo_handle;
};

#ifdef __cplusplus
	}
#endif

#endif /* HINGE_ANGLE_H_ */
