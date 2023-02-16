/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <zephyr/senss/senss_sensor.h>
#include <zephyr/logging/log.h>
#include "hinge_angle_algo.h"

LOG_MODULE_REGISTER(hinge_angle_algo, CONFIG_SENSS_LOG_LEVEL);

/* HA/ha means HINGE_ANGLE */

#define HA_DEG_0                        0
#define HA_DEG_90                       90
#define HA_DEG_180                      180
#define HA_DEG_360                      360

/* maximum calculable hinge vertical value, unit: mG */
#define HA_HINGE_VERTICAL_MAX           890

#define HA_WEIGHTLESS_TO_1G_SS          \
	(2 * HINGE_ANGLE_MG_IN_1G * HINGE_ANGLE_WEIGHTLESS_ACC_DEVIATION)
#define HA_1G_SS (HINGE_ANGLE_MG_IN_1G * HINGE_ANGLE_MG_IN_1G)

#define HA_COS_TABLE_LEN                91

struct ha_algo_acc_info {
	uint64_t timestamp; /* last sample timestamp */
	int32_t v[HINGE_ANGLE_ACC_AXIS_CNT];
	bool data_in; /* any sensor data in: true, else: false */
};

struct hinge_angle_algo_md_info {
	uint64_t timestamp; /* last sample timestamp */
	int32_t v;
	bool data_in; /* any sensor data in: true, else: false */
};

struct hinge_angle_algo_ctx {
	struct senss_sensor_value_int32 value;
	int32_t result;
	int32_t last_nonoverlap_result;
	bool value_gen; /* value generate: true, else: false */
	bool value_first_process; /* value first output when called process */
	struct ha_algo_acc_info base;
	struct ha_algo_acc_info lid;
};

static const int32_t g_hinge_vector[HINGE_ANGLE_ACC_AXIS_CNT] =
	HINGE_ANGLE_HINGE_VECTOR;

/* 0 ~ 90 deg, s10k means scaled 10000 */
static const int16_t cos_table_scaled10k[HA_COS_TABLE_LEN] = {
	10000, 9998, 9994, 9986, 9976, 9962, 9945, 9925, 9903, 9877, /* 0~9 */
	9848, 9816, 9781, 9744, 9703, 9659, 9613, 9563, 9511, 9455, /* 10~19 */
	9397, 9336, 9272, 9205, 9135, 9063, 8988, 8910, 8829, 8746, /* 20~29 */
	8660, 8572, 8480, 8387, 8290, 8192, 8090, 7986, 7880, 7771, /* 30~39 */
	7660, 7547, 7431, 7314, 7193, 7071, 6947, 6820, 6691, 6561, /* 40~49 */
	6428, 6293, 6157, 6018, 5878, 5736, 5592, 5446, 5299, 5150, /* 50~59 */
	5000, 4848, 4695, 4540, 4384, 4226, 4067, 3907, 3746, 3584, /* 60~69 */
	3420, 3256, 3090, 2924, 2756, 2588, 2419, 2250, 2079, 1908, /* 70~79 */
	1736, 1564, 1392, 1219, 1045, 872, 698, 523, 349, 175, /* 80~89 */
	0 /* 90 */
};

/*
 * x: scaled 10000 (10000 >= x >= -10000, 10000: 0 degree, -10000: 180 degree)
 * return: [0, 180] degree
 */
static int32_t ha_scaled10k_arccos(int32_t x)
{
	int32_t x_abs = abs(x);
	int32_t start = 0;
	int32_t end = HA_COS_TABLE_LEN - 1;
	int32_t mid;
	int32_t table_id = 0;

	if (x >= cos_table_scaled10k[0]) {
		return HA_DEG_0;
	} else if (x <= (-cos_table_scaled10k[0])) {
		return HA_DEG_180;
	} else if (x == cos_table_scaled10k[HA_COS_TABLE_LEN - 1]) {
		return HA_DEG_90;
	}

	while (1) {
		mid = (start + end) >> 1;
		if (cos_table_scaled10k[mid] < x_abs) {
			end = mid;
		} else if (cos_table_scaled10k[mid] > x_abs) {
			if (start == mid) {
				table_id = ((cos_table_scaled10k[start] - x_abs) <
					(x_abs - cos_table_scaled10k[end])) ?
					start : end;
				break;
			}
			start = mid;
		} else {
			/* found */
			table_id = mid;
			break;
		}
	}

	if (x < 0) {
		table_id = HA_DEG_180 - table_id;
	}

	return table_id;
}

static int32_t ha_int_sqrt(int64_t x)
{
	int32_t end = INT32_MAX;
	int32_t start = 0;
	int32_t mid = 0;
	int64_t mid_square;

	if (x <= 0) {
		return 0;
	}

	if (x <= HINGE_ANGLE_MG_IN_1G_SQUARE) {
		end = HINGE_ANGLE_MG_IN_1G;
	} else if (x <= HINGE_ANGLE_MG_IN_8G_SQUARE) {
		end = HINGE_ANGLE_MG_IN_8G;
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
static int64_t ha_sum_of_squares(const int32_t *v)
{
	return (int64_t)v[0] * v[0] + (int64_t)v[1] * v[1] +
		(int64_t)v[2] * v[2];
}

/* parameter v is an array of size 3 */
static int32_t ha_vector_magnitude(const int32_t *v)
{
	int64_t sum = (int64_t)v[0] * v[0] + (int64_t)v[1] * v[1] +
		(int64_t)v[2] * v[2];

	return ha_int_sqrt(sum);
}

/* parameter v1 v2 and v are an array of size 3 */
static void ha_cross_product(const int32_t *v1, const int32_t *v2,
	int32_t *v)
{
	v[0] = (int64_t)v1[1] * v2[2] - (int64_t)v1[2] * v2[1];
	v[1] = (int64_t)v1[2] * v2[0] - (int64_t)v1[0] * v2[2];
	v[2] = (int64_t)v1[0] * v2[1] - (int64_t)v1[1] * v2[0];
}

/* parameter v1 and v2 are an array of size 3 */
static int64_t ha_dot_product(const int32_t *v1, const int32_t *v2)
{
	return (int64_t)v1[0] * v2[0] + (int64_t)v1[1] * v2[1] +
		(int64_t)v1[2] * v2[2];
}

/* parameter v1 and v2 are an array of size 3 */
static inline bool ha_is_hypergravity_state(const int32_t *v1,
	const int32_t *v2)
{
	return (abs(v1[0]) > HINGE_ANGLE_HYPERGRAVITY_ACC_MIN ||
		abs(v1[1]) > HINGE_ANGLE_HYPERGRAVITY_ACC_MIN ||
		abs(v1[2]) > HINGE_ANGLE_HYPERGRAVITY_ACC_MIN ||
		abs(v2[0]) > HINGE_ANGLE_HYPERGRAVITY_ACC_MIN ||
		abs(v2[1]) > HINGE_ANGLE_HYPERGRAVITY_ACC_MIN ||
		abs(v2[2]) > HINGE_ANGLE_HYPERGRAVITY_ACC_MIN);
}

static int32_t ha_algo_calc_angle(struct hinge_angle_algo_ctx *ctx,
	int32_t *hinge_angle)
{
	int32_t angle_cos;
	int32_t angle_deg;
	int64_t dot_product;
	int64_t compensate;
	int64_t base_ss, lid_ss; /* ss: sum of squares */
	int32_t base_magnitude, lid_magnitude;
	int32_t base_project[HINGE_ANGLE_ACC_AXIS_CNT];
	int32_t lid_project[HINGE_ANGLE_ACC_AXIS_CNT];
	int32_t cross[HINGE_ANGLE_ACC_AXIS_CNT];

	if (ha_is_hypergravity_state(ctx->base.v, ctx->lid.v)) {
		LOG_DBG("[%s] hypergravity state, base %d %d %d lid %d %d %d",
			__func__,
			ctx->base.v[0], ctx->base.v[1], ctx->base.v[2],
			ctx->lid.v[0], ctx->lid.v[1], ctx->lid.v[2]);
		return -1;
	}

	base_ss = ha_sum_of_squares(ctx->base.v);
	lid_ss = ha_sum_of_squares(ctx->lid.v);
	if ((((int64_t)HA_1G_SS - base_ss) > (int64_t)HA_WEIGHTLESS_TO_1G_SS) ||
		(((int64_t)HA_1G_SS - lid_ss) >
		(int64_t)HA_WEIGHTLESS_TO_1G_SS)) {
		LOG_DBG("[%s] weightless state, base %d %d %d lid %d %d %d",
			__func__,
			ctx->base.v[0], ctx->base.v[1], ctx->base.v[2],
			ctx->lid.v[0], ctx->lid.v[1], ctx->lid.v[2]);
		return -1;
	}

	if (abs(ctx->base.v[HINGE_ANGLE_HINGE_AXIS]) > HA_HINGE_VERTICAL_MAX ||
		abs(ctx->lid.v[HINGE_ANGLE_HINGE_AXIS]) >
		HA_HINGE_VERTICAL_MAX) {
		LOG_DBG("[%s] hinge axis too vertical, base %d %d %d lid %d %d %d",
			__func__,
			ctx->base.v[0], ctx->base.v[1], ctx->base.v[2],
			ctx->lid.v[0], ctx->lid.v[1], ctx->lid.v[2]);
		return -1;
	}

	memcpy(base_project, ctx->base.v, sizeof(base_project));
	memcpy(lid_project, ctx->lid.v, sizeof(lid_project));
	base_project[HINGE_ANGLE_HINGE_AXIS] = 0;
	lid_project[HINGE_ANGLE_HINGE_AXIS] = 0;
	base_magnitude = ha_vector_magnitude(base_project);
	lid_magnitude = ha_vector_magnitude(lid_project);
	if ((!base_magnitude) || (!lid_magnitude)) {
		LOG_DBG("[%s] magnitude is 0, base %d %d %d lid %d %d %d",
			__func__,
			ctx->base.v[0], ctx->base.v[1], ctx->base.v[2],
			ctx->lid.v[0], ctx->lid.v[1], ctx->lid.v[2]);
		return -1;
	}

	dot_product = ha_dot_product(base_project, lid_project);
	compensate = dot_product ?
		(((int64_t)base_magnitude * lid_magnitude) / 2) : 0;
	if (dot_product < 0) {
		compensate = -compensate;
	}

	angle_cos = (dot_product * HINGE_ANGLE_SCALED10K + compensate) /
		((int64_t)base_magnitude * lid_magnitude);
	if (angle_cos > HINGE_ANGLE_SCALED10K) {
		angle_cos = HINGE_ANGLE_SCALED10K;
	} else if (angle_cos < -HINGE_ANGLE_SCALED10K) {
		angle_cos = -HINGE_ANGLE_SCALED10K;
	}

	angle_deg = ha_scaled10k_arccos(angle_cos);
	/* need in the case the 2 z-axes are the same when expand and flatten */
	angle_deg = HA_DEG_180 - angle_deg;

	ha_cross_product(base_project, lid_project, cross);
	dot_product = ha_dot_product(cross, g_hinge_vector);
	if (dot_product > 0) {
		angle_deg = HA_DEG_360 - angle_deg;
	}

	*hinge_angle = angle_deg;

	return 0;
}

void *hinge_angle_algo_create(void)
{
	void *algo_handle = malloc(sizeof(struct hinge_angle_algo_ctx));

	if (!algo_handle) {
		return NULL;
	}

	hinge_angle_algo_reset(algo_handle);

	return algo_handle;
}

void hinge_angle_algo_destroy(void *algo_handle)
{
	if (!algo_handle) {
		return;
	}

	free(algo_handle);
}

void hinge_angle_algo_reset(void *algo_handle)
{
	struct hinge_angle_algo_ctx *ctx =
		(struct hinge_angle_algo_ctx *)algo_handle;

	if (!algo_handle) {
		return;
	}

	memset(ctx, 0, sizeof(struct hinge_angle_algo_ctx));
	ctx->result = SENSS_HINGE_ANGLE_RESULT_UNKNOWN;
	ctx->value.header.reading_count = 1;
}

static int ha_algo_collect_data_acc(struct ha_algo_acc_info *acc_info,
	struct senss_sensor_value_3d_int32 *acc)
{
	uint32_t i;

	acc_info->timestamp = acc->header.base_timestamp;

	for (i = 0; i < acc->header.reading_count; i++)
		acc_info->timestamp += acc->readings[i].timestamp_delta;

	i = acc->header.reading_count - 1;
	acc_info->v[0] = acc->readings[i].v[0] / HINGE_ANGLE_UG_PER_MG;
	acc_info->v[1] = acc->readings[i].v[1] / HINGE_ANGLE_UG_PER_MG;
	acc_info->v[2] = acc->readings[i].v[2] / HINGE_ANGLE_UG_PER_MG;

	if (!acc_info->data_in) {
		acc_info->data_in = true;
	}

	return 0;
}

int hinge_angle_algo_collect_data_base_acc(void *algo_handle,
	struct senss_sensor_value_3d_int32 *acc)
{
	struct hinge_angle_algo_ctx *algo_ctx =
		(struct hinge_angle_algo_ctx *)algo_handle;

	if ((!algo_handle) || (!acc) || (acc->header.reading_count < 1)) {
		return -EINVAL;
	}

	return ha_algo_collect_data_acc(&algo_ctx->base, acc);
}

int hinge_angle_algo_collect_data_lid_acc(void *algo_handle,
	struct senss_sensor_value_3d_int32 *acc)
{
	struct hinge_angle_algo_ctx *algo_ctx =
		(struct hinge_angle_algo_ctx *)algo_handle;

	if ((!algo_handle) || (!acc) || (acc->header.reading_count < 1)) {
		return -EINVAL;
	}

	return ha_algo_collect_data_acc(&algo_ctx->lid, acc);
}

int hinge_angle_algo_process(void *algo_handle,
	enum hinge_angle_value_info *value_info,
	struct senss_sensor_value_int32 **value)
{
	int32_t ret;
	struct hinge_angle_algo_ctx *ctx =
		(struct hinge_angle_algo_ctx *)algo_handle;
	int32_t hinge_angle = HA_DEG_0;

	if ((!algo_handle) || (!value_info) || (!value)) {
		return -EINVAL;
	}

	if (ctx->base.data_in && ctx->lid.data_in) {
		ret = ha_algo_calc_angle(ctx, &hinge_angle);
		if (!ret) {
			if (hinge_angle == HA_DEG_0 ||
				hinge_angle == HA_DEG_360) {
				hinge_angle = (ctx->last_nonoverlap_result >
					HA_DEG_180) ? HA_DEG_360 : HA_DEG_0;
			} else {
				ctx->last_nonoverlap_result = hinge_angle;
			}

			ctx->result = hinge_angle;
		} else {
			ctx->result = SENSS_HINGE_ANGLE_RESULT_UNKNOWN;
		}

		ctx->value_gen = true;
	}

	if (!ctx->value_gen) {
		*value_info = HINGE_ANGLE_VALUE_NOT_GEN;
	} else {
		if (!ctx->value_first_process) {
			ctx->value_first_process = true;
			*value_info = HINGE_ANGLE_VALUE_FIRST;

			LOG_INF("[%s] value first %d, base %d %d %d lid %d %d %d",
				__func__, ctx->result, ctx->base.v[0],
				ctx->base.v[1], ctx->base.v[2],
				ctx->lid.v[0], ctx->lid.v[1],
				ctx->lid.v[2]);
		} else if (ctx->result == ctx->value.readings[0].v) {
			*value_info = HINGE_ANGLE_VALUE_NO_CHANGE;
		} else {
			*value_info = HINGE_ANGLE_VALUE_CHANGED;

			LOG_INF("[%s] value changed from %d to %d, base %d %d %d lid %d %d %d",
				__func__, ctx->value.readings[0].v,
				ctx->result, ctx->base.v[0], ctx->base.v[1],
				ctx->base.v[2], ctx->lid.v[0],
				ctx->lid.v[1], ctx->lid.v[2]);
		}
	}

	ctx->value.readings[0].v = ctx->result;
	ctx->value.header.base_timestamp = ctx->lid.timestamp;
	*value = (struct senss_sensor_value_int32 *)&ctx->value;

	return 0;
}
