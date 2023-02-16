/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTION_DETECTOR_H_
#define MOTION_DETECTOR_H_

#include <zephyr/senss/senss_sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_DETECTOR_ACC_INTERVAL_US      100000

struct motion_detector_context {
	uint32_t interval;
	int32_t acc_handle;
	void *algo_handle;
};

#ifdef __cplusplus
	}
#endif

#endif /* MOTION_DETECTOR_H_ */
