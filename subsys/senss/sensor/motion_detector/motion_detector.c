/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <senss_sensor.h>
#include "motion_detector_algo.h"
#include "motion_detector.h"

LOG_MODULE_REGISTER(motion_detector, CONFIG_SENSS_LOG_LEVEL);

static struct senss_sensor_register_info md_reg = {
	.flags = SENSS_SENSOR_FLAG_REPORT_ON_EVENT,
	.sample_size = sizeof(struct senss_sensor_value_int32),
	.sensitivity_count = 1,
	.version.value = SENSS_SENSOR_VERSION(1, 0, 0, 0),
};

static int md_init(const struct device *dev,
	const struct senss_sensor_info *info, const int32_t *reporter_handles,
	int32_t reporters_count)
{
	int32_t i;
	struct motion_detector_context *ctx = senss_sensor_get_ctx_data(dev);
	const struct senss_sensor_info *rpt_info = NULL;

	LOG_INF("[%s] name: %s", __func__, dev->name);

	ctx->acc_handle = SENSS_SENSOR_INVALID_HANDLE;

	for (i = 0; i < reporters_count; i++) {
		rpt_info = senss_get_sensor_info(reporter_handles[i]);

		LOG_INF("[%s] reporter_handles[%d] %d, type 0x%x index %d",
			__func__, i, reporter_handles[i], rpt_info->type,
			rpt_info->sensor_index);

		if (rpt_info->type ==
			SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D) {
			if (strcmp(rpt_info->name, "lid-accel") == 0) {
				ctx->acc_handle = reporter_handles[i];
				continue;
			} else if (strcmp(rpt_info->name, "base-accel") == 0) {
				/* only one acc is supported, and lid acc takes precedence */
				if (ctx->acc_handle == SENSS_SENSOR_INVALID_HANDLE) {
					ctx->acc_handle = reporter_handles[i];
				}
				continue;
			}
		}

		LOG_WRN("[%s] unused reporter_handles[%d] %d, type 0x%x index %d",
			__func__, i, reporter_handles[i], rpt_info->type,
			rpt_info->sensor_index);
	}

	if (ctx->acc_handle == SENSS_SENSOR_INVALID_HANDLE) {
		LOG_ERR("[%s] error, acc_handle is invalid", __func__);
		return -EINVAL;
	}

	/* create algorithm */
	if (!ctx->algo_handle) {
		ctx->algo_handle = motion_detector_algo_create();
		if (!ctx->algo_handle) {
			LOG_ERR("[%s] algo create error", __func__);
			return -ENOMEM;
		}
	}

	return 0;
}

static int md_reset(const struct device *dev)
{
	struct motion_detector_context *ctx = senss_sensor_get_ctx_data(dev);

	motion_detector_algo_reset(ctx->algo_handle);

	return 0;
}

static int md_deinit(const struct device *dev)
{
	struct motion_detector_context *ctx = senss_sensor_get_ctx_data(dev);

	motion_detector_algo_destroy(ctx->algo_handle);
	ctx->algo_handle = NULL;

	return 0;
}

static int md_set_interval(const struct device *dev, uint32_t value)
{
	int ret;
	struct motion_detector_context *ctx = senss_sensor_get_ctx_data(dev);
	uint32_t acc_interval = value ? MOTION_DETECTOR_ACC_INTERVAL_US : 0;

	ret = senss_set_interval(ctx->acc_handle, acc_interval);
	if (ret) {
		LOG_ERR("[%s] error, value %d acc_interval %d ret %d", __func__,
			value, acc_interval, ret);
		return -ENOSYS;
	}

	ctx->interval = value;
	LOG_INF("[%s] name: %s, value %d acc_interval %d", __func__, dev->name,
		value, acc_interval);

	return 0;
}

static int md_get_interval(const struct device *dev, uint32_t *value)
{
	struct motion_detector_context *ctx = senss_sensor_get_ctx_data(dev);

	if (!value) {
		return -EINVAL;
	}

	*value = ctx->interval;
	LOG_INF("[%s] name: %s interval %d", __func__, dev->name, ctx->interval);

	return 0;
}

static int md_process(const struct device *dev, int32_t reporter, void *buf,
	int32_t size)
{
	int ret = 0;
	struct motion_detector_context *ctx = senss_sensor_get_ctx_data(dev);
	enum motion_detector_value_info info = MOTION_DETECTOR_VALUE_NOT_GEN;
	struct senss_sensor_value_int32 *value = NULL;

	if (reporter == ctx->acc_handle) {
		ret = motion_detector_algo_collect_data_acc(ctx->algo_handle,
			(struct senss_sensor_value_3d_int32 *)buf);
	} else {
		LOG_WRN("[%s] unexpected reporter %d, size %d",
			__func__, reporter, size);
		return 0;
	}

	if (ret) {
		LOG_ERR("[%s] collect data error, reporter %d ret %d",
			__func__, reporter, ret);
		return ret;
	}

	ret = motion_detector_algo_process(ctx->algo_handle, &info,
		&value);
	if (ret) {
		LOG_ERR("[%s] process error, reporter %d ret %d",
			__func__, reporter, ret);
		return ret;
	}

	if ((info == MOTION_DETECTOR_VALUE_NOT_GEN) || (!value)) {
		return 0;
	}

	return senss_sensor_post_data(dev, value, sizeof(*value));
}

static int md_sensitivity_test(const struct device *dev, int index,
	uint32_t sensitivity, void *last_sample_buf, int last_sample_size,
	void *current_sample_buf, int current_sample_size)
{
	struct senss_sensor_value_int32 *last = last_sample_buf;
	struct senss_sensor_value_int32 *curr = current_sample_buf;

	return (abs(curr->readings[0].v - last->readings[0].v) >= sensitivity) ? 1 : 0;
}

static struct senss_sensor_api md_api = {
	.init = md_init,
	.reset = md_reset,
	.deinit = md_deinit,
	.get_interval = md_get_interval,
	.set_interval = md_set_interval,
	.process = md_process,
	.sensitivity_test = md_sensitivity_test,
};

#define DT_DRV_COMPAT zephyr_senss_motion_detector
#define SENSS_MOTION_DETECTOR_DT_DEFINE(_inst) \
	static struct motion_detector_context _CONCAT(md_ctx, _inst) = { 0 }; \
	SENSS_SENSOR_DT_DEFINE(DT_DRV_INST(_inst), &md_reg, \
		&_CONCAT(md_ctx, _inst), &md_api);

DT_INST_FOREACH_STATUS_OKAY(SENSS_MOTION_DETECTOR_DT_DEFINE);
