/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTION_DETECTOR_H_
#define MOTION_DETECTOR_H_

#include <senss_sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

struct motion_detector_context {
	uint32_t interval;
	uint32_t sensitivity;
	int32_t lid_acc_handle;
	void *algo_handle;
};

#ifdef __cplusplus
	}
#endif

#endif /* MOTION_DETECTOR_H_ */
