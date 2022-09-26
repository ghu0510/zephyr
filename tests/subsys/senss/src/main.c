/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/senss/senss.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test_senss, LOG_LEVEL_INF);

#define INTERVAL_10HZ (100 * USEC_PER_MSEC)
#define INTERVAL_20HZ (50 * USEC_PER_MSEC)
#define INVALID_INTERVAL_US 1 /* 1 us is smaller than the minimal interval */

/**
 * @brief Senss Tests setup
 *
 * This test verifies the senss_init.
 */
static void *setup(void)
{
	zassert_equal(senss_init(), 0, "senss_init failed");

	return NULL;
}

/**
 * @brief Senss Tests teardown
 *
 * This test verifies the senss_deinit.
 */
static void teardown(void *ptr)
{
	ARG_UNUSED(ptr);

	zassert_equal(senss_deinit(), 0, "senss_deinit failed");
}

ZTEST_SUITE(senss_tests, NULL, setup, NULL, NULL, teardown);

#define DT_DRV_COMPAT zephyr_senss
#define DT_SENSOR_TYPE_INDEX(node)				\
	{							\
		.type = DT_PROP(node, sensor_type),		\
		.index = DT_PROP_OR(node, sensor_index, 0),	\
	}

struct sensor_info_t {
	int32_t type;
	int32_t index;
};

static struct sensor_info_t sensors[] = {
	DT_FOREACH_CHILD_SEP(DT_DRV_INST(0), DT_SENSOR_TYPE_INDEX, (,))
};

static bool lookup_sensor_in_dt(int32_t type, int32_t index)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sensors); ++i) {
		if (sensors[i].type == type &&
				sensors[i].index == index) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Test Get Sensors
 *
 * This test verifies senss_get_sensors
 */
ZTEST(senss_tests, test_senss_get_sensors)
{
	const struct senss_sensor_info *infos;
	int num = senss_get_sensors(&infos);
	int i = 0;

	/* Check the return value */
	zassert_equal(num, ARRAY_SIZE(sensors), "Sensors num mismatch");
	zassert_not_null(infos, "Sensors infos is null");

	for (i = 0; i < num; ++i) {
		/* Check the content */
		zassert_true(lookup_sensor_in_dt(infos[i].type,
					infos[i].sensor_index),
				"sensor info mismatch");
	}
}

#define INVALID_SENSOR_TYPE (-1)
#define INVALID_SENSOR_INDEX (-1)

/**
 * @brief Test Open Sensor
 *
 * This test verifies senss_open_sensors
 */
ZTEST(senss_tests, test_senss_open_sensors)
{
	int handle = 0;
	int ret;

	/* positive test */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");
	zassert_not_equal(handle, SENSS_SENSOR_INVALID_HANDLE,
			"Open ACC 0 failed");

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 1,
			&handle);
	zassert_equal(ret, 0, "Open ACC 1 failed");
	zassert_not_equal(handle, SENSS_SENSOR_INVALID_HANDLE,
			"Open ACC 1 failed");

	/* negative test */
	ret = senss_open_sensor(INVALID_SENSOR_TYPE, 0, &handle);
	zassert_true(ret < 0, "Negative test 2 failed");
	zassert_equal(handle, SENSS_SENSOR_INVALID_HANDLE,
			"Negative test 1 failed");

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D,
			INVALID_SENSOR_INDEX, &handle);
	zassert_true(ret < 0, "Negative test 2 failed");
	zassert_equal(handle, SENSS_SENSOR_INVALID_HANDLE,
			"Negative test 2 failed");
}

/**
 * @brief Test Close Sensor
 *
 * This test verifies senss_close_sensors
 */
ZTEST(senss_tests, test_senss_close_sensors)
{
	int handle = 0;
	int ret;

	/* positive test */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");
	zassert_not_equal(handle, SENSS_SENSOR_INVALID_HANDLE,
			"Open ACC 0 failed");

	ret = senss_close_sensor(handle);
	zassert_equal(ret, 0, "Close ACC 0 failed");

	/* negative test */
	ret = senss_close_sensor(SENSS_SENSOR_INVALID_HANDLE);
	zassert_true(ret < 0, "Negative test failed");
}

/**
 * @brief Test Set Interval
 *
 * This test verifies senss_set_interval
 */
ZTEST(senss_tests, test_senss_set_interval)
{
	int handle = 0;
	int ret;

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");
	zassert_not_equal(handle, SENSS_SENSOR_INVALID_HANDLE,
			"Open ACC 0 failed");

	/* positive test */
	ret = senss_set_interval(handle, INTERVAL_10HZ);
	zassert_equal(ret, 0, "Set ACC 0 interval 100 ms failed");

	ret = senss_set_interval(handle, INTERVAL_20HZ);
	zassert_equal(ret, 0, "Set ACC 0 interval 50 ms failed");

	/* negative test */
	ret = senss_set_interval(handle, INVALID_INTERVAL_US);
	zassert_true(ret < 0, "Negative test 1 failed");

	ret = senss_set_interval(SENSS_SENSOR_INVALID_HANDLE,
			INTERVAL_10HZ);
	zassert_true(ret < 0, "Negative test 2 failed");

	ret = senss_close_sensor(handle);
	zassert_equal(ret, 0, "Close ACC 0 failed");
}
