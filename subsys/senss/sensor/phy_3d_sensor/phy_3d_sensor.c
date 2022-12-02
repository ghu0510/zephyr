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
#include <zephyr/sys/util.h>

#include <senss_sensor.h>

LOG_MODULE_REGISTER(phy_3d_sensor, CONFIG_SENSS_LOG_LEVEL);

#define PHY_3D_SENSOR_SLOPE_DURATION 2

struct phy_3d_sensor_context {
	const struct device *dev;
	const struct device *hw_dev;
	struct sensor_trigger trig;
	uint32_t interval;
	uint32_t samples;
	uint32_t sensitivity[3];
};

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
	ctx->trig.chan = SENSOR_CHAN_ACCEL_XYZ;

	if (enable) {
		ret = senss_sensor_set_data_ready(ctx->dev, true);
		if (sensor_trigger_set(ctx->hw_dev, &ctx->trig,
					phy_3d_sensor_data_ready_handler) < 0) {
			ret = senss_sensor_set_data_ready(ctx->dev, false);
			LOG_INF("%s: Configured for polled sampling.",
					ctx->dev->name);
		} else {
			LOG_INF("%s: Configured for triggered sampling.",
					ctx->dev->name);
		}
	} else {
		sensor_trigger_set(ctx->hw_dev, &ctx->trig, NULL);
		ret = senss_sensor_set_data_ready(ctx->dev, false);
	}

	return ret;
}

static int phy_3d_sensor_init(const struct device *dev,
		const struct senss_sensor_info *info,
		const int *reporter_handles,
		int reporters_count)
{
	struct phy_3d_sensor_context *ctx;

	ARG_UNUSED(reporter_handles);
	ARG_UNUSED(reporters_count);

	ctx = senss_sensor_get_ctx_data(dev);
	ctx->dev = dev;

	LOG_INF("%s: Underlying device: %s", dev->name, ctx->hw_dev->name);

	/* Try to enable the data ready mode */
	return phy_3d_sensor_enable_data_ready(ctx, true);
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
	struct sensor_value accel[3];
	int ret;

	ctx = senss_sensor_get_ctx_data(dev);

	ret = sensor_sample_fetch(ctx->hw_dev);
	if (ret) {
		LOG_ERR("%s: sample fetch failed: %d", dev->name, ret);
		return ret;
	}

	ret = sensor_channel_get(ctx->hw_dev, SENSOR_CHAN_ACCEL_XYZ,
			accel);
	if (ret) {
		LOG_ERR("%s: channel get failed: %d", dev->name, ret);
		return ret;
	}

	sample->readings[0].x = sensor_ms2_to_ug(&accel[0]);
	sample->readings[0].y = sensor_ms2_to_ug(&accel[1]);
	sample->readings[0].z = sensor_ms2_to_ug(&accel[2]);
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
		freq = (double)USEC_PER_SEC / value;
	} else {
		freq = 0;
	}

	ret = sensor_value_from_double(&odr, freq);
	if (ret) {
		LOG_ERR("%s: Cannot translate freq %u to sensor value. ret:%d",
				dev->name, (uint32_t)freq, ret);
		return ret;
	}

	ret = sensor_attr_set(ctx->hw_dev, SENSOR_CHAN_ACCEL_XYZ,
			SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	if (ret) {
		LOG_ERR("%s: Cannot set sampling frequency %u for accelerometer. ret:%d",
				dev->name, (uint32_t)freq, ret);
	} else {
		LOG_INF("%s: Set sampling frequency %u for accelerometer.",
				dev->name, (uint32_t)freq);
		ctx->interval = value;
	}

	return ret;
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

	attr_value.val1 = value / 1000000;
	attr_value.val2 = value % 1000000;
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
		chan = SENSOR_CHAN_ACCEL_X + index;
	} else if (index == SENSS_INDEX_ALL) {
		for (i = 0; i < ARRAY_SIZE(ctx->sensitivity); ++i) {
			ctx->sensitivity[i] = value;
		}

		chan = SENSOR_CHAN_ACCEL_XYZ;
	} else {
		LOG_ERR("%s: set sensitivity: invalid index: %d",
				dev->name, index);
		return -EINVAL;
	}

	LOG_INF("%s: set sensitivity index: %d value: %d", dev->name,
			index, value);

	/* Disable data ready before enable any-motion */
	phy_3d_sensor_enable_data_ready(ctx, false);

	ret = phy_3d_sensor_set_slope(ctx, chan, value);
	if (ret) {
		/* Try to enable data ready if enable any-motion failed */
		phy_3d_sensor_enable_data_ready(ctx, true);
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

static int phy_3d_sensor_set_range(const struct device *dev,
		int index, uint32_t value)
{
	struct phy_3d_sensor_context *ctx;
	struct sensor_value attr_value;
	enum sensor_attribute attr;
	enum sensor_channel chan;
	int ret = 0;

	LOG_INF("%s: set range index: %d value: %u", dev->name, index, value);

	ctx = senss_sensor_get_ctx_data(dev);
	attr = SENSOR_ATTR_FULL_SCALE;

	if (index >= 0 && index <= SENSOR_CHAN_ACCEL_Z - SENSOR_CHAN_ACCEL_X) {
		chan = SENSOR_CHAN_ACCEL_X + index;
	} else if (index == SENSS_INDEX_ALL) {
		chan = SENSOR_CHAN_ACCEL_XYZ;
	} else {
		LOG_ERR("%s: set range: invalid index: %d", dev->name, index);
		return -EINVAL;
	}

	sensor_g_to_ms2(value / 1000000, &attr_value);

	ret = sensor_attr_set(ctx->hw_dev, chan, attr, &attr_value);
	if (ret) {
		LOG_WRN("%s: Cannot set full scale for accelerometer. err:%d",
				dev->name, ret);
	}

	return ret;
}

static int phy_3d_sensor_get_range(const struct device *dev,
		int index, uint32_t *value)
{
	struct phy_3d_sensor_context *ctx;
	struct sensor_value attr_value;
	enum sensor_attribute attr;
	enum sensor_channel chan;
	int ret = 0;

	ctx = senss_sensor_get_ctx_data(dev);
	attr = SENSOR_ATTR_FULL_SCALE;

	if (index >= 0 && index <= SENSOR_CHAN_ACCEL_Z - SENSOR_CHAN_ACCEL_X) {
		chan = SENSOR_CHAN_ACCEL_X + index;
	} else if (index == SENSS_INDEX_ALL) {
		chan = SENSOR_CHAN_ACCEL_XYZ;
	} else {
		LOG_ERR("%s: get range: invalid index: %d", dev->name, index);
		return -EINVAL;
	}

	ret = sensor_attr_get(ctx->hw_dev, chan, attr, &attr_value);
	if (ret) {
		LOG_ERR("%s: Cannot get full scale for accelerometer. ret: %d",
				dev->name, ret);
		return -ENOSYS;
	}

	*value = sensor_ms2_to_ug(&attr_value);

	LOG_INF("%s: get range index: %d value: %u", dev->name, index, *value);

	return ret;
}

static int phy_3d_sensor_set_fifo(const struct device *dev,
		uint32_t samples)
{
	struct phy_3d_sensor_context *ctx;

	ctx = senss_sensor_get_ctx_data(dev);

	ctx->samples = samples;

	return -ENOTSUP;
}

static int phy_3d_sensor_get_fifo(const struct device *dev,
		uint32_t *samples)
{
	struct phy_3d_sensor_context *ctx;

	ctx = senss_sensor_get_ctx_data(dev);

	*samples = ctx->samples;

	return -ENOTSUP;
}

static int phy_3d_sensor_set_offset(const struct device *dev,
		int index, int32_t value)
{
	struct phy_3d_sensor_context *ctx;
	struct sensor_value attr_value;
	enum sensor_attribute attr;
	enum sensor_channel chan;
	int ret = 0;

	LOG_DBG("%s: set offset index: %d value: %d", dev->name, index, value);

	ctx = senss_sensor_get_ctx_data(dev);
	attr = SENSOR_ATTR_OFFSET;

	if (index >= 0 && index <= SENSOR_CHAN_ACCEL_Z - SENSOR_CHAN_ACCEL_X) {
		chan = SENSOR_CHAN_ACCEL_X + index;
	} else if (index == SENSS_INDEX_ALL) {
		chan = SENSOR_CHAN_ACCEL_XYZ;
	} else {
		LOG_ERR("%s: set offset: invalid index: %d", dev->name, index);
		return -EINVAL;
	}

	attr_value.val1 = value;
	attr_value.val2 = 0;

	ret = sensor_attr_set(ctx->hw_dev, chan, attr, &attr_value);
	if (ret) {
		LOG_ERR("%s: Cannot set offset %d.", dev->name, ret);
	}

	return ret;
}

static int phy_3d_sensor_get_offset(const struct device *dev,
		int index, int32_t *value)
{
	struct phy_3d_sensor_context *ctx;
	struct sensor_value attr_value = {0};
	enum sensor_attribute attr;
	enum sensor_channel chan;
	int ret = 0;

	ctx = senss_sensor_get_ctx_data(dev);
	attr = SENSOR_ATTR_OFFSET;

	if (index >= 0 && index <= SENSOR_CHAN_ACCEL_Z - SENSOR_CHAN_ACCEL_X) {
		chan = SENSOR_CHAN_ACCEL_X + index;
	} else if (index == SENSS_INDEX_ALL) {
		chan = SENSOR_CHAN_ACCEL_XYZ;
	} else {
		LOG_ERR("%s: Get offset: invalid index: %d", dev->name, index);
		return -EINVAL;
	}

	ret = sensor_attr_get(ctx->hw_dev, chan, attr, &attr_value);
	if (ret) {
		LOG_ERR("%s: Cannot get offset %d.", dev->name, ret);
	}

	*value = attr_value.val1;

	LOG_DBG("%s: get offset index: %d value: %d", dev->name,
			index, *value);

	return ret;
}

static const struct senss_sensor_api phy_3d_sensor_api = {
	.init = phy_3d_sensor_init,
	.deinit = phy_3d_sensor_deinit,
	.set_interval = phy_3d_sensor_set_interval,
	.get_interval = phy_3d_sensor_get_interval,
	.set_range = phy_3d_sensor_set_range,
	.get_range = phy_3d_sensor_get_range,
	.set_offset = phy_3d_sensor_set_offset,
	.get_offset = phy_3d_sensor_get_offset,
	.set_fifo = phy_3d_sensor_set_fifo,
	.get_fifo = phy_3d_sensor_get_fifo,
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
	};								\
	SENSS_SENSOR_DT_DEFINE(DT_DRV_INST(_inst),			\
		&phy_3d_sensor_reg, &_CONCAT(ctx, _inst),		\
		&phy_3d_sensor_api);

DT_INST_FOREACH_STATUS_OKAY(SENSS_PHY_3D_SENSOR_DT_DEFINE);
