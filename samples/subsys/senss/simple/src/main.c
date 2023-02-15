/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>
#include <zephyr/senss/senss.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static int acc_data_event_callback(int handle, void *buf, int size, void *param)
{
	return -ENOTSUP;
}

static int hinge_angle_data_event_callback(int handle, void *buf, int size, void *param)
{
	return -ENOTSUP;
}

void main(void)
{
	const struct senss_sensor_info *info = NULL;
	int base_acc;
	int lid_acc;
	int hinge_angle;
	int ret, i, num = 0;

	ret = senss_init();
	if (ret) {
		LOG_ERR("senss init error");
		return;
	}

	num = senss_get_sensors(&info);
	for (i = 0; i < num; ++i) {
		LOG_INF("Sensor %d: name: %s friendly_name: %s type: %d index: %d",
			 i,
			 info->name,
			 info->friendly_name,
			 info->type,
			 info->sensor_index);
		info++;
	}

	LOG_INF("senss run successfully");

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0, &base_acc);
	if (ret) {
		LOG_ERR("senss_open_sensor, type:0x%x index:0 error:%d",
			SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, ret);
	} else {
		senss_register_data_event_callback(base_acc, acc_data_event_callback, NULL);

		ret = senss_set_interval(base_acc, 100 * USEC_PER_MSEC);
		if (ret) {
			LOG_ERR("base_acc senss_set_interval error:%d\n", ret);
		}
	}

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 1, &lid_acc);
	if (ret) {
		LOG_ERR("senss_open_sensor, type:0x%x index:1 error:%d",
			SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, ret);
	} else {
		senss_register_data_event_callback(lid_acc, acc_data_event_callback, NULL);

		ret = senss_set_interval(lid_acc, 100 * USEC_PER_MSEC);
		if (ret) {
			LOG_ERR("lid_acc senss_set_interval error:%d\n", ret);
		}
	}

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 0, &hinge_angle);
	if (ret) {
		LOG_ERR("senss_open_sensor, type:0x%x index:0 error:%d",
			SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, ret);
	} else {
		info = senss_get_sensor_info(hinge_angle);
		senss_register_data_event_callback(hinge_angle,
						   hinge_angle_data_event_callback,
						   NULL);

		ret = senss_set_interval(hinge_angle, info->minimal_interval);
		if (ret) {
			LOG_ERR("hinge_angle senss_set_interval error:%d\n", ret);
		}
	}

	ret = senss_close_sensor(base_acc);
	if (ret) {
		LOG_ERR("senss_close_sensor:%d error:%d", base_acc, ret);
	}

	ret = senss_close_sensor(lid_acc);
	if (ret) {
		LOG_ERR("senss_close_sensor:%d error:%d", lid_acc, ret);
	}

	ret = senss_close_sensor(hinge_angle);
	if (ret) {
		LOG_ERR("senss_close_sensor:%d error:%d", hinge_angle, ret);
	}

	ret = senss_deinit();
	if (ret) {
		LOG_ERR("senss_deinit error:%d", ret);
	}
}
