/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <senss_sensor.h>
#include <zephyr/logging/log.h>
#include "motion_detector_algo.h"

LOG_MODULE_REGISTER(motion_detector_algo, CONFIG_SENSS_LOG_LEVEL);

/* MD/md means MOTION_DETECTOR */

#define MD_US_PER_SEC                   1000000
/* at least samples per second */
#define MD_MOTIONLESS_SAMPLES_PER_SEC   2
#define MD_MOTIONLESS_TIME_US      (MOTION_DETECTOR_MOTIONLESS_SECONDS * MD_US_PER_SEC)
#define MD_MOTIONLESS_SAMPLES_NEED (MOTION_DETECTOR_MOTIONLESS_SECONDS * \
	MD_MOTIONLESS_SAMPLES_PER_SEC)

struct md_algo_acc_info {
	uint64_t start_timestamp;
	uint64_t timestamp; /* last sample timestamp */
	int32_t v[MOTION_DETECTOR_ACC_AXIS_CNT];
	int32_t magnitude_min;
	int32_t magnitude_max;
	uint32_t sample_cnt;
	enum senss_motion_detector_result result;
	bool value_gen; /* value generate: true, else: false */
	bool value_first_process; /* value first output when called process */
};

struct md_algo_ctx {
	struct senss_sensor_value_int32 value;
	struct md_algo_acc_info acc;
};

static int32_t md_int_sqrt(int64_t x)
{
	int32_t end = INT32_MAX;
	int32_t start = 0;
	int32_t mid = 0;
	int64_t mid_square;

	if (x <= 0) {
		return 0;
	}

	if (x <= MOTION_DETECTOR_MG_IN_1G_SQUARE) {
		end = MOTION_DETECTOR_MG_IN_1G;
	} else if (x <= MOTION_DETECTOR_MG_IN_8G_SQUARE) {
		end = MOTION_DETECTOR_MG_IN_8G;
	} else if (x <= INT32_MAX) {
		end = 46340; /* sqrt(INT32_MAX) ~= 46340.95 */
	} else if (x <= (int64_t)INT32_MAX * INT32_MAX) {
		end = INT32_MAX;
	}

	if (x >= (int64_t)end * end) {
		return end;
	}

	while (1) {
		mid = start + (end - start) / 2;
		mid_square = (int64_t)mid * mid;

		if (mid_square > x) {
			end = mid;
		} else if (mid_square < x) {
			if (start == mid) {
				return mid;
			}

			start = mid;
		} else {
			return mid;
		}
	}

	return mid;
}

/* parameter v is an array of size 3 */
static int32_t md_vector_magnitude(const int32_t *v)
{
	int64_t sum = (int64_t)v[0] * v[0] + (int64_t)v[1] * v[1] +
		(int64_t)v[2] * v[2];

	return md_int_sqrt(sum);
}

static void md_algo_acc_calc(struct md_algo_acc_info *acc_info, int32_t *data)
{
	int32_t v[MOTION_DETECTOR_ACC_AXIS_CNT];
	int32_t magnitude;
	bool recount = false;

	v[0] = data[0] / MOTION_DETECTOR_UG_PER_MG;
	v[1] = data[1] / MOTION_DETECTOR_UG_PER_MG;
	v[2] = data[2] / MOTION_DETECTOR_UG_PER_MG;

	/* hypergravity, in moving */
	if (v[0] > MOTION_DETECTOR_HYPERGRAVITY_ACC_MIN ||
		v[1] > MOTION_DETECTOR_HYPERGRAVITY_ACC_MIN ||
		v[2] > MOTION_DETECTOR_HYPERGRAVITY_ACC_MIN) {
		acc_info->result = SENSS_MOTION_DETECTOR_RESULT_MOVING;
		acc_info->value_gen = true;
		acc_info->sample_cnt = 0;
		return;
	}

	/* first sample, only record */
	if (!acc_info->sample_cnt) {
		acc_info->v[0] = v[0];
		acc_info->v[1] = v[1];
		acc_info->v[2] = v[2];
		acc_info->sample_cnt = 1;
		acc_info->start_timestamp = acc_info->timestamp;
		return;
	}

	/* axis value changes too much, in moving */
	if (abs(v[0] - acc_info->v[0]) >
			MOTION_DETECTOR_MOTIONLESS_ACC_DEVIATION ||
		abs(v[1] - acc_info->v[1]) >
			MOTION_DETECTOR_MOTIONLESS_ACC_DEVIATION ||
		abs(v[2] - acc_info->v[2]) >
			MOTION_DETECTOR_MOTIONLESS_ACC_DEVIATION) {
		acc_info->result = SENSS_MOTION_DETECTOR_RESULT_MOVING;
		acc_info->value_gen = true;
		acc_info->v[0] = v[0];
		acc_info->v[1] = v[1];
		acc_info->v[2] = v[2];
		acc_info->sample_cnt = 1;
		acc_info->start_timestamp = acc_info->timestamp;
		return;
	}

	/* fill the first sample magnitude */
	if (acc_info->sample_cnt == 1) {
		acc_info->magnitude_min = md_vector_magnitude(acc_info->v);
		acc_info->magnitude_max = acc_info->magnitude_min;
	}

	/* record the current sample */
	acc_info->v[0] = v[0];
	acc_info->v[1] = v[1];
	acc_info->v[2] = v[2];
	acc_info->sample_cnt++;

	magnitude = md_vector_magnitude(v);
	if (magnitude < acc_info->magnitude_min) {
		acc_info->magnitude_min = magnitude;
	} else if (magnitude > acc_info->magnitude_max) {
		acc_info->magnitude_max = magnitude;
	}

	if ((acc_info->magnitude_max - acc_info->magnitude_min) >
		MOTION_DETECTOR_MOTIONLESS_ACC_DEVIATION) {
		/* cumulative magnitude diff exceeds the range, in moving */
		acc_info->result = SENSS_MOTION_DETECTOR_RESULT_MOVING;
		acc_info->value_gen = true;
		recount = true;
	} else if ((acc_info->timestamp - acc_info->start_timestamp) >=
		MD_MOTIONLESS_TIME_US) {
		/* enough samples are needed to judge as motionless */
		if (acc_info->sample_cnt >= MD_MOTIONLESS_SAMPLES_NEED) {
			acc_info->result =
				SENSS_MOTION_DETECTOR_RESULT_MOTIONLESS;
			acc_info->value_gen = true;
		}

		recount = true;
	}

	if (recount) {
		acc_info->sample_cnt = 1;
		acc_info->start_timestamp = acc_info->timestamp;
	}
}

void *motion_detector_algo_create(void)
{
	void *algo_handle = malloc(sizeof(struct md_algo_ctx));

	if (!algo_handle) {
		return NULL;
	}

	motion_detector_algo_reset(algo_handle);

	return algo_handle;
}

void motion_detector_algo_destroy(void *algo_handle)
{
	if (!algo_handle) {
		return;
	}

	free(algo_handle);
}

void motion_detector_algo_reset(void *algo_handle)
{
	struct md_algo_ctx *ctx = (struct md_algo_ctx *)algo_handle;

	if (!algo_handle) {
		return;
	}

	memset(ctx, 0, sizeof(struct md_algo_ctx));
	ctx->acc.result = SENSS_MOTION_DETECTOR_RESULT_UNKNOWN;
	ctx->value.header.reading_count = 1;
}

int motion_detector_algo_collect_data_acc(void *algo_handle,
	struct senss_sensor_value_3d_int32 *acc)
{
	struct md_algo_ctx *ctx = (struct md_algo_ctx *)algo_handle;
	uint32_t i;

	if ((!algo_handle) || (!acc) || (acc->header.reading_count < 1)) {
		return -EINVAL;
	}

	ctx->acc.timestamp = acc->header.base_timestamp;

	for (i = 0; i < acc->header.reading_count; i++) {
		md_algo_acc_calc(&ctx->acc, acc->readings[i].v);
		ctx->acc.timestamp += acc->readings[i].timestamp_delta;
	}

	return 0;
}

int motion_detector_algo_process(void *algo_handle,
	enum motion_detector_value_info *value_info,
	struct senss_sensor_value_int32 **value)
{
	struct md_algo_ctx *ctx = (struct md_algo_ctx *)algo_handle;

	if ((!algo_handle) || (!value_info) || (!value)) {
		return -EINVAL;
	}

	if (!ctx->acc.value_gen) {
		*value_info = MOTION_DETECTOR_VALUE_NOT_GEN;
	} else {
		if (!ctx->acc.value_first_process) {
			ctx->acc.value_first_process = true;
			*value_info = MOTION_DETECTOR_VALUE_FIRST;

			LOG_INF("[%s] value first %d, acc %d %d %d",
				__func__, ctx->acc.result, ctx->acc.v[0],
				ctx->acc.v[1], ctx->acc.v[2]);
		} else if (ctx->acc.result == ctx->value.readings[0].v) {
			*value_info = MOTION_DETECTOR_VALUE_NO_CHANGE;
		} else {
			*value_info = MOTION_DETECTOR_VALUE_CHANGED;

			LOG_INF("[%s] value changed from %d to %d, acc %d %d %d",
				__func__, ctx->value.readings[0].v,
				ctx->acc.result, ctx->acc.v[0],
				ctx->acc.v[1], ctx->acc.v[2]);
		}
	}

	ctx->value.readings[0].v = ctx->acc.result;
	ctx->value.header.base_timestamp = ctx->acc.timestamp;
	*value = (struct senss_sensor_value_int32 *)&ctx->value;

	return 0;
}
