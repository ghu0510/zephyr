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
	const struct senss_sensor_info *info = senss_get_sensor_info(handle);
	struct senss_sensor_value_3d_int32 *sample =
		(struct senss_sensor_value_3d_int32 *)buf;

	ARG_UNUSED(size);
	ARG_UNUSED(param);

	LOG_INF("Sensor %s data:\t x: %d, y: %d, z: %d", info->name,
			sample->readings[0].x,
			sample->readings[0].y,
			sample->readings[0].z);

	return 0;
}
#if 0
static int motion_detector_data_event_callback(int handle, void *buf, int size,
	void *param)
{
	const struct senss_sensor_info *info = senss_get_sensor_info(handle);
	struct senss_sensor_value_int32 *sample =
		(struct senss_sensor_value_int32 *)buf;

	ARG_UNUSED(size);
	ARG_UNUSED(param);

	LOG_INF("Sensor %s data:\t v: %d", info->name,
		sample->readings[0].v);

	return 0;
}

static int hinge_angle_data_event_callback(int handle, void *buf, int size,
	void *param)
{
	const struct senss_sensor_info *info = senss_get_sensor_info(handle);
	struct senss_sensor_value_int32 *sample =
		(struct senss_sensor_value_int32 *)buf;

	ARG_UNUSED(size);
	ARG_UNUSED(param);

	LOG_INF("Sensor %s data:\t v: %d", info->name,
		sample->readings[0].v);

	return 0;
}
#endif

void main(void)
{
	int base_acc;
	int ret;

	ret = senss_init();
	if (ret) {
		LOG_ERR("senss init error");
		return;
	}

	LOG_INF("senss run successfully");

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&base_acc);
	if (ret) {
		LOG_ERR("senss_open_sensor, type:0x%x index:0 error",
			SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D);
	} else {
		senss_register_data_event_callback(base_acc,
				acc_data_event_callback, NULL);

		ret = senss_set_interval(base_acc, 100 * USEC_PER_MSEC);
		if (ret) {
			LOG_ERR("base_acc senss_set_interval error\n");
		}
		/* set report latency as 2s*/
		ret = senss_set_report_latency(base_acc, 2 * USEC_PER_MSEC * MSEC_PER_SEC);
		if (ret) {
			LOG_ERR("base_acc senss_set_report_latency error\n");
		}
	}
#if 0
	const struct senss_sensor_info *info = NULL;
	int lid_acc;
	int motion_detector;
	int hinge_angle;

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 1,
			&lid_acc);
	if (ret) {
		LOG_ERR("senss_open_sensor, type:0x%x index:1 error",
			SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D);
	} else {
		senss_register_data_event_callback(lid_acc,
				acc_data_event_callback, NULL);

		ret = senss_set_interval(lid_acc, 100 * USEC_PER_MSEC);
		if (ret) {
			LOG_ERR("lid_acc senss_set_interval error\n");
		}
	}

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR, 0,
		&motion_detector);
	if (ret) {
		LOG_ERR("senss_open_sensor, type:0x%x index:0 error",
			SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR);
	} else {
		info = senss_get_sensor_info(motion_detector);
		senss_register_data_event_callback(motion_detector,
			motion_detector_data_event_callback, NULL);

		ret = senss_set_interval(motion_detector,
			info->minimal_interval);
		if (ret) {
			LOG_ERR("motion_detector senss_set_interval error\n");
		}
	}

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 0,
		&hinge_angle);
	if (ret) {
		LOG_ERR("senss_open_sensor, type:0x%x index:0 error",
			SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE);
	} else {
		info = senss_get_sensor_info(hinge_angle);
		senss_register_data_event_callback(hinge_angle,
			hinge_angle_data_event_callback, NULL);

		ret = senss_set_interval(hinge_angle, info->minimal_interval);
		if (ret) {
			LOG_ERR("hinge_angle senss_set_interval error\n");
		}
	}
#endif
}
