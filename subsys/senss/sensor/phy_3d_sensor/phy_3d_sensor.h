/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SPHY_3D_SENSOR_H_
#define ZEPHYR_INCLUDE_SPHY_3D_SENSOR_H_

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#define PHY_3D_SENSOR_CHANNEL_NUM 3

struct phy_3d_sensor_custom {
	const enum sensor_channel chan_base;
	const enum sensor_channel chan_all;
	int32_t (*unit_convert_si_to_int32)(const struct sensor_value *si);
	void (*unit_convert_int32_to_si)(int32_t val, struct sensor_value *si);
};

struct phy_3d_sensor_context {
	const struct device *dev;
	const struct device *hw_dev;
	const int32_t sensor_type;
	const struct phy_3d_sensor_custom *custom;
	struct sensor_trigger trig;
	bool data_ready_enabled;
	bool data_ready_support;
	uint32_t interval;
	uint32_t samples;
	uint32_t sensitivity[PHY_3D_SENSOR_CHANNEL_NUM];
};

#endif
