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
#include "hinge_angle_algo.h"
#include "hinge_angle.h"

LOG_MODULE_REGISTER(hinge_angle, CONFIG_SENSS_LOG_LEVEL);

static struct senss_sensor_register_info hinge_reg = {
	.flags = SENSS_SENSOR_FLAG_REPORT_ON_CHANGE,
	.sample_size = sizeof(struct senss_sensor_value_int32),
	.version.value = SENSS_SENSOR_VERSION(1, 0, 0, 0),
};

static int hinge_init(const struct device *dev,
	const struct senss_sensor_info *info, const int32_t *reporter_handles,
	int32_t reporters_count)
{
	int i;
	struct hinge_angle_context *ctx = senss_sensor_get_ctx_data(dev);
	const struct senss_sensor_info *rpt_info = NULL;

	LOG_INF("[%s] name: %s", __func__, dev->name);

	ctx->base_acc_handle = SENSS_SENSOR_INVALID_HANDLE;
	ctx->lid_acc_handle = SENSS_SENSOR_INVALID_HANDLE;

	for (i = 0; i < reporters_count; i++) {
		rpt_info = senss_get_sensor_info(reporter_handles[i]);

		LOG_INF("[%s] reporter_handles[%d] %d, type 0x%x index %d",
			__func__, i, reporter_handles[i], rpt_info->type,
			rpt_info->sensor_index);

		if (rpt_info->type ==
			SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D) {
			if (strcmp(rpt_info->name, "base-accel") == 0) {
				ctx->base_acc_handle = reporter_handles[i];
				continue;
			} else if (strcmp(rpt_info->name, "lid-accel") == 0) {
				ctx->lid_acc_handle = reporter_handles[i];
				continue;
			}
		}

		LOG_WRN("[%s] unused reporter_handles[%d] %d, type 0x%x index %d",
			__func__, i, reporter_handles[i], rpt_info->type,
			rpt_info->sensor_index);
	}

	/* base acc and lid acc are necessary */
	if ((ctx->base_acc_handle == SENSS_SENSOR_INVALID_HANDLE) ||
		(ctx->lid_acc_handle == SENSS_SENSOR_INVALID_HANDLE)) {
		LOG_ERR("[%s] error, base_acc_handle %d lid_acc_handle %d",
			__func__, ctx->base_acc_handle, ctx->lid_acc_handle);
		return -EINVAL;
	}

	/* create algorithm */
	if (!ctx->algo_handle) {
		ctx->algo_handle = hinge_angle_algo_create();
		if (!ctx->algo_handle) {
			LOG_ERR("[%s] algo create error", __func__);
			return -ENOMEM;
		}
	}

	return 0;
}

static int hinge_reset(const struct device *dev)
{
	struct hinge_angle_context *ctx = senss_sensor_get_ctx_data(dev);

	hinge_angle_algo_reset(ctx->algo_handle);

	return 0;
}

static int hinge_deinit(const struct device *dev)
{
	struct hinge_angle_context *ctx = senss_sensor_get_ctx_data(dev);

	hinge_angle_algo_destroy(ctx->algo_handle);
	ctx->algo_handle = NULL;

	return 0;
}

static int hinge_set_interval(const struct device *dev, uint32_t value)
{
	int ret;
	struct hinge_angle_context *ctx = senss_sensor_get_ctx_data(dev);

	LOG_INF("[%s] name: %s, value:%d", __func__, dev->name, value);

	ret = senss_set_interval(ctx->base_acc_handle, value);
	if (ret) {
		return ret;
	}

	ret = senss_set_interval(ctx->lid_acc_handle, value);
	if (ret) {
		return ret;
	}

	ctx->interval = value;

	return 0;
}

static int hinge_get_interval(const struct device *dev, uint32_t *value)
{
	struct hinge_angle_context *ctx = senss_sensor_get_ctx_data(dev);

	LOG_INF("[%s] name: %s", __func__, dev->name);

	if (value) {
		*value = ctx->interval;
	}

	return 0;
}

static int hinge_process(const struct device *dev, int32_t reporter, void *buf,
	int32_t size)
{
	int ret = 0;
	struct hinge_angle_context *ctx = senss_sensor_get_ctx_data(dev);
	enum hinge_angle_value_info info = HINGE_ANGLE_VALUE_NOT_GEN;
	struct senss_sensor_value_int32 *value = NULL;

	if (reporter == ctx->base_acc_handle) {
		ret = hinge_angle_algo_collect_data_base_acc(ctx->algo_handle,
			(struct senss_sensor_value_3d_int32 *)buf);
	} else if (reporter == ctx->lid_acc_handle) {
		ret = hinge_angle_algo_collect_data_lid_acc(ctx->algo_handle,
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

	/* only calc angle when lid acc input */
	if (reporter != ctx->lid_acc_handle)
		return 0;

	ret = hinge_angle_algo_process(ctx->algo_handle, &info, &value);
	if (ret) {
		LOG_ERR("[%s] process error, reporter %d ret %d",
			__func__, reporter, ret);
		return ret;
	}

	if ((info == HINGE_ANGLE_VALUE_NOT_GEN) || (!value)) {
		return 0;
	}

	return senss_sensor_post_data(dev, value, sizeof(*value));
}

static int hinge_sensitivity_test(const struct device *dev, int index,
	uint32_t sensitivity, void *last_sample_buf, int last_sample_size,
	void *current_sample_buf, int current_sample_size)
{
	return 1;
}

static struct senss_sensor_api hinge_api = {
	.init = hinge_init,
	.reset = hinge_reset,
	.deinit = hinge_deinit,
	.get_interval = hinge_get_interval,
	.set_interval = hinge_set_interval,
	.process = hinge_process,
	.sensitivity_test = hinge_sensitivity_test,
};

#define DT_DRV_COMPAT zephyr_senss_hinge_angle
#define SENSS_HINGE_ANGLE_DT_DEFINE(_inst) \
	static struct hinge_angle_context _CONCAT(hinge_ctx, _inst) = { 0 }; \
	SENSS_SENSOR_DT_DEFINE(DT_DRV_INST(_inst), &hinge_reg, \
		&_CONCAT(hinge_ctx, _inst), &hinge_api);

DT_INST_FOREACH_STATUS_OKAY(SENSS_HINGE_ANGLE_DT_DEFINE);
