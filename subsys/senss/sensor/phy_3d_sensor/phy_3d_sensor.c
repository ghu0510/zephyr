/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_senss_phy_3d_sensor

#include <stdlib.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>
#include <zephyr/senss/senss.h>
#include <zephyr/sys/util.h>

#include <senss_sensor.h>
#include "phy_3d_sensor.h"

LOG_MODULE_REGISTER(phy_3d_sensor, CONFIG_SENSS_LOG_LEVEL);

#define PHY_3D_SENSOR_SLOPE_DURATION 2

static const struct phy_3d_sensor_custom custom_accel = {
	.chan_base = SENSOR_CHAN_ACCEL_X,
	.chan_all = SENSOR_CHAN_ACCEL_XYZ,
	.unit_convert_si_to_int32 = sensor_ms2_to_ug,
	.unit_convert_int32_to_si = sensor_ug_to_ms2,
};

static const struct phy_3d_sensor_custom custom_gyro = {
	.chan_base = SENSOR_CHAN_GYRO_X,
	.chan_all = SENSOR_CHAN_GYRO_XYZ,
	.unit_convert_si_to_int32 = sensor_rad_to_10udegrees,
	.unit_convert_int32_to_si = sensor_10udegrees_to_rad,
};

static int phy_3d_sensor_index_to_channel(struct phy_3d_sensor_context *ctx,
		int index, enum sensor_channel *chan)
{
	if (index >= PHY_3D_SENSOR_CHANNEL_NUM || index < SENSS_INDEX_ALL) {
		return -EINVAL;
	}

	if (index == SENSS_INDEX_ALL) {
		*chan = ctx->custom->chan_all;
	} else {
		*chan = ctx->custom->chan_base + index;
	}

	return 0;
}

static void phy_3d_sensor_data_ready_handler(const struct device *dev,
		const struct sensor_trigger *trig)
{
	struct phy_3d_sensor_context *ctx = CONTAINER_OF(trig,
			struct phy_3d_sensor_context,
			trig);

	LOG_DBG("%s: trigger type:%d", dev->name, trig->type);

	senss_sensor_notify_data_ready(ctx->dev);
}

static int phy_3d_sensor_enable_data_ready(struct phy_3d_sensor_context *ctx,
		bool enable)
{
	int ret = 0;

	ctx->trig.type = SENSOR_TRIG_DATA_READY;
	ctx->trig.chan = ctx->custom->chan_all;

	if (enable) {
		if (!ctx->data_ready_enabled) {
			ret = senss_sensor_set_data_ready(ctx->dev, true);
			if (sensor_trigger_set(ctx->hw_dev, &ctx->trig,
					phy_3d_sensor_data_ready_handler) < 0) {
				ret = senss_sensor_set_data_ready(ctx->dev,
						false);
				ctx->data_ready_enabled = false;
				LOG_INF("%s: Configured for polled sampling.",
						ctx->dev->name);
			} else {
				LOG_INF("%s: Configured for triggered sampling.",
						ctx->dev->name);
				ctx->data_ready_enabled = true;
			}
		}
	} else {
		sensor_trigger_set(ctx->hw_dev, &ctx->trig, NULL);
		ret = senss_sensor_set_data_ready(ctx->dev, false);
		ctx->data_ready_enabled = false;
	}

	return ret;
}

static int phy_3d_sensor_init(const struct device *dev,
		const struct senss_sensor_info *info,
		const int *reporter_handles,
		int reporters_count)
{
	struct phy_3d_sensor_context *ctx;
	int ret;

	ARG_UNUSED(reporter_handles);
	ARG_UNUSED(reporters_count);

	ctx = senss_sensor_get_ctx_data(dev);
	ctx->dev = dev;

	switch (ctx->sensor_type) {
	case SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D:
		ctx->custom = &custom_accel;
		break;
	case SENSS_SENSOR_TYPE_MOTION_GYROMETER_3D:
		ctx->custom = &custom_gyro;
		break;
	default:
		LOG_ERR("phy_3d_sensor doesn't support sensor type %d",
				ctx->sensor_type);
		return -ENOTSUP;
	}

	LOG_INF("%s: Underlying device: %s", dev->name, ctx->hw_dev->name);

	/* Try to enable the data ready mode */
	ret = phy_3d_sensor_enable_data_ready(ctx, true);
	if (!ret) {
		ctx->data_ready_support = true;
		ctx->data_ready_enabled = true;
	} else {
		ctx->data_ready_support = false;
		ctx->data_ready_enabled = false;
	}

	return ret;
}

static int phy_3d_sensor_deinit(const struct device *dev)
{
	struct phy_3d_sensor_context *ctx = senss_sensor_get_ctx_data(dev);

	phy_3d_sensor_enable_data_ready(ctx, false);

	return 0;
}

static int phy_3d_sensor_read_sample(const struct device *dev,
		void *buf, int size)
{
	struct phy_3d_sensor_context *ctx;
	struct senss_sensor_value_3d_int32 *sample = buf;
	struct sensor_value value[PHY_3D_SENSOR_CHANNEL_NUM];
	int i, ret;

	ctx = senss_sensor_get_ctx_data(dev);

	ret = sensor_sample_fetch(ctx->hw_dev);
	if (ret) {
		LOG_ERR("%s: sample fetch failed: %d", dev->name, ret);
		return ret;
	}

	ret = sensor_channel_get(ctx->hw_dev, ctx->custom->chan_all, value);
	if (ret) {
		LOG_ERR("%s: channel get failed: %d", dev->name, ret);
		return ret;
	}

	for (i = 0; i < PHY_3D_SENSOR_CHANNEL_NUM; ++i) {
		sample->readings[0].v[i] =
			ctx->custom->unit_convert_si_to_int32(&value[i]);
	}

	sample->header.reading_count = 1;

	LOG_DBG("%s: Sample data:\t x: %d, y: %d, z: %d",
			dev->name,
			sample->readings[0].x,
			sample->readings[0].y,
			sample->readings[0].z);

	return ret;
}

static int phy_3d_sensor_sensitivity_test(const struct device *dev,
		int index, uint32_t sensitivity,
		void *last_sample_buf, int last_sample_size,
		void *current_sample_buf, int current_sample_size)
{
	struct senss_sensor_value_3d_int32 *last = last_sample_buf;
	struct senss_sensor_value_3d_int32 *curr = current_sample_buf;
	struct phy_3d_sensor_context *ctx;
	int reached = 0;
	int i;

	ctx = senss_sensor_get_ctx_data(dev);
	if (index >= 0 && index < ARRAY_SIZE(ctx->sensitivity)) {
		reached = abs(curr->readings[0].v[index]
				- last->readings[0].v[index])
			>= sensitivity;
	} else if (index == SENSS_INDEX_ALL) {
		for (i = 0; i < ARRAY_SIZE(ctx->sensitivity); ++i) {
			reached |= abs(curr->readings[0].v[i]
					- last->readings[0].v[i])
				>= sensitivity;
		}
	} else {
		LOG_ERR("%s: test sensitivity: invalid index: %d", dev->name,
				index);
		return -EINVAL;
	}

	return reached;
}

static int phy_3d_sensor_set_interval(const struct device *dev, uint32_t value)
{
	struct phy_3d_sensor_context *ctx;
	struct sensor_value odr;
	double freq;
	int ret;

	LOG_INF("%s: set report interval %u us", dev->name, value);

	ctx = senss_sensor_get_ctx_data(dev);

	if (value) {
		if (ctx->data_ready_support) {
			phy_3d_sensor_enable_data_ready(ctx, true);
		}

		freq = (double)USEC_PER_SEC / value;
		ret = sensor_value_from_double(&odr, freq);
		if (ret) {
			LOG_ERR("%s: Cannot translate freq %u to sensor value. ret:%d",
					dev->name, (uint32_t)freq, ret);
			return ret;
		}

		ret = sensor_attr_set(ctx->hw_dev, ctx->custom->chan_all,
				SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
		if (ret) {
			LOG_ERR("%s: Cannot set sampling frequency %u. ret:%d",
					dev->name, (uint32_t)freq, ret);
		} else {
			LOG_INF("%s: Set sampling frequency %u.",
					dev->name, (uint32_t)freq);
		}
	} else {
		if (ctx->data_ready_support) {
			phy_3d_sensor_enable_data_ready(ctx, false);
		}
	}

	ctx->interval = value;

	return 0;
}

static int phy_3d_sensor_get_interval(const struct device *dev,
		uint32_t *value)
{
	struct phy_3d_sensor_context *ctx;

	ctx = senss_sensor_get_ctx_data(dev);
	*value = ctx->interval;

	LOG_INF("%s: get report interval %u us", dev->name, *value);

	return 0;
}

static int phy_3d_sensor_set_slope(struct phy_3d_sensor_context *ctx,
		enum sensor_channel chan, uint32_t value)
{
	struct sensor_value attr_value;
	enum sensor_attribute attr;
	int ret = 0;

	ctx->custom->unit_convert_int32_to_si((int32_t)value, &attr_value);
	attr = SENSOR_ATTR_SLOPE_TH;

	ret = sensor_attr_set(ctx->hw_dev, chan, attr, &attr_value);
	if (!ret) {
		/* set slope duration */
		attr_value.val1 = PHY_3D_SENSOR_SLOPE_DURATION;
		attr_value.val2 = 0;
		attr = SENSOR_ATTR_SLOPE_DUR;

		ret = sensor_attr_set(ctx->hw_dev, chan, attr, &attr_value);
		if (!ret) {
			ctx->trig.type = SENSOR_TRIG_DELTA;
			ctx->trig.chan = chan;

			if (value) {
				ret = sensor_trigger_set(ctx->hw_dev,
						&ctx->trig,
						phy_3d_sensor_data_ready_handler);
			} else {
				ret = sensor_trigger_set(ctx->hw_dev,
						&ctx->trig,
						NULL);
			}
		}
	}

	if (ret) {
		LOG_WRN("%s: set slope failed! attr:%d chan:%d ret:%d",
			ctx->hw_dev->name, attr, chan, ret);
	}

	return ret;
}

static int phy_3d_sensor_set_sensitivity(const struct device *dev,
		int index, uint32_t value)
{
	struct phy_3d_sensor_context *ctx;
	enum sensor_channel chan;
	int ret = 0;
	int i;

	ctx = senss_sensor_get_ctx_data(dev);

	if (index >= 0 && index < ARRAY_SIZE(ctx->sensitivity)) {
		ctx->sensitivity[index] = value;
	} else if (index == SENSS_INDEX_ALL) {
		for (i = 0; i < ARRAY_SIZE(ctx->sensitivity); ++i) {
			ctx->sensitivity[i] = value;
		}
	} else {
		LOG_ERR("%s: set sensitivity: invalid index: %d",
				dev->name, index);
		return -EINVAL;
	}

	LOG_INF("%s: set sensitivity index: %d value: %d", dev->name,
			index, value);

	/* Disable data ready before enable any-motion */
	if (ctx->data_ready_support) {
		phy_3d_sensor_enable_data_ready(ctx, false);
	}

	ret = phy_3d_sensor_index_to_channel(ctx, index, &chan);
	if (ret) {
		LOG_WRN("%s: set sensitivity index(%d) to channel failed. ret:%d",
				dev->name, index, ret);
	} else {
		ret = phy_3d_sensor_set_slope(ctx, chan, value);
	}
	if (ret) {
		/* Try to enable data ready if enable any-motion failed */
		if (ctx->data_ready_support) {
			phy_3d_sensor_enable_data_ready(ctx, true);
		}
	}

	return 0;
}

static int phy_3d_sensor_get_sensitivity(const struct device *dev,
		int index, uint32_t *value)
{
	struct phy_3d_sensor_context *ctx;
	int i;

	ctx = senss_sensor_get_ctx_data(dev);

	if (index >= 0 && index < ARRAY_SIZE(ctx->sensitivity)) {
		*value = ctx->sensitivity[index];
	} else if (index == SENSS_INDEX_ALL) {
		for (i = 1; i < ARRAY_SIZE(ctx->sensitivity); ++i) {
			if (ctx->sensitivity[i] != ctx->sensitivity[0]) {
				LOG_ERR("%s: sensitivities are different, please get it one by one",
						dev->name);
				return -ESPIPE;
			};
		}

		*value = ctx->sensitivity[0];
	} else {
		LOG_ERR("%s: get sensitivity: invalid index: %d",
				dev->name, index);
		return -EINVAL;
	}

	LOG_INF("%s: get sensitivity index: %d value: %u",
			dev->name, index, *value);

	return 0;
}

static const struct senss_sensor_api phy_3d_sensor_api = {
	.init = phy_3d_sensor_init,
	.deinit = phy_3d_sensor_deinit,
	.set_interval = phy_3d_sensor_set_interval,
	.get_interval = phy_3d_sensor_get_interval,
	.set_sensitivity = phy_3d_sensor_set_sensitivity,
	.get_sensitivity = phy_3d_sensor_get_sensitivity,
	.read_sample = phy_3d_sensor_read_sample,
	.sensitivity_test = phy_3d_sensor_sensitivity_test,
};

static const struct senss_sensor_register_info phy_3d_sensor_reg = {
	.flags = SENSS_SENSOR_FLAG_REPORT_ON_CHANGE,
	.sample_size = sizeof(struct senss_sensor_value_3d_int32),
	.version.value = SENSS_SENSOR_VERSION(0, 8, 0, 0),
};

#define SENSS_PHY_3D_SENSOR_DT_DEFINE(_inst)				\
	static struct phy_3d_sensor_context _CONCAT(ctx, _inst) = {	\
		.hw_dev = DEVICE_DT_GET(				\
				DT_PHANDLE(DT_DRV_INST(_inst),		\
				underlying_device)),			\
		.sensor_type = DT_PROP(DT_DRV_INST(_inst), sensor_type),\
	};								\
	SENSS_SENSOR_DT_DEFINE(DT_DRV_INST(_inst),			\
		&phy_3d_sensor_reg, &_CONCAT(ctx, _inst),		\
		&phy_3d_sensor_api);

DT_INST_FOREACH_STATUS_OKAY(SENSS_PHY_3D_SENSOR_DT_DEFINE);
