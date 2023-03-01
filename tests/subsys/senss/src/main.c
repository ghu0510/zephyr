/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/ztest.h>
#include <zephyr/senss/senss.h>
#include <zephyr/senss/senss_sensor.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>
#include "sensor_mgmt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test_senss, LOG_LEVEL_INF);

#define SENSITIVITY_MAX 0xFFFFFFFE
#define INTERVAL_10HZ (100 * USEC_PER_MSEC)
#define INTERVAL_20HZ (50 * USEC_PER_MSEC)
#define INTERVAL_40HZ (25 * USEC_PER_MSEC)
#define INVALID_INTERVAL_US 1 /* 1 us is smaller than the minimal interval */

struct senss_tests_fixture {
	const struct device *dev;
	atomic_t acc_0_samples;
	atomic_t acc_1_samples;
	atomic_t user_0_samples;
	atomic_t user_1_samples;
};

/**
 * @brief Senss Tests setup
 *
 * This test verifies the senss_init.
 */
static void *setup(void)
{
	static struct senss_tests_fixture fixture;

	fixture.dev = DEVICE_DT_GET(DT_INST(0, zephyr_senss_phy_3d_sensor));

	zassert_equal(senss_init(), 0, "senss_init failed");

	return &fixture;
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

/**
 * @brief Test Get Interval
 *
 * This test verifies senss_get_interval
 */
ZTEST(senss_tests, test_senss_get_interval)
{
	int handle = 0;
	uint32_t value;
	int ret;

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	/* positive test */
	ret = senss_get_interval(handle, &value);
	zassert_equal(ret, 0, "Get ACC 0 interval failed");
	zassert_equal(value, 0, "Value is not equal to 0");

	ret = senss_set_interval(handle, INTERVAL_10HZ);
	zassert_equal(ret, 0, "Set ACC 0 interval 100 ms failed");
	value = 0;
	ret = senss_get_interval(handle, &value);
	zassert_equal(ret, 0, "Get ACC 0 interval 100 ms failed");
	zassert_equal(value, INTERVAL_10HZ, "Value is not equal to INTERVAL_10HZ");

	ret = senss_set_interval(handle, INTERVAL_20HZ);
	zassert_equal(ret, 0, "Set ACC 0 interval 50 ms failed");
	value = 0;
	ret = senss_get_interval(handle, &value);
	zassert_equal(ret, 0, "Get ACC 0 interval 50 ms failed");
	zassert_equal(value, INTERVAL_20HZ, "Value is not equal to INTERVAL_20HZ");

	/* negative test */
	ret = senss_get_interval(SENSS_SENSOR_INVALID_HANDLE, &value);
	zassert_true(ret < 0, "Negative test 1 failed");
	/* Pass NULL to uint32_t *value */
	ret = senss_get_interval(handle, NULL);
	zassert_true(ret < 0, "Negative test 2 failed");

	ret = senss_close_sensor(handle);
	zassert_equal(ret, 0, "Close ACC 0 failed");
}

/**
 * @brief Test Set Sensitivity
 *
 * This test verifies senss_set_sensitivity
 */
ZTEST(senss_tests, test_senss_set_sensitivity)
{
	int handle = 0;
	int ret;

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	/* positive test */
	ret = senss_set_sensitivity(handle, 0, 100);
	zassert_equal(ret, 0, "Set ACC 0 index 0 sensitivity 100 failed");

	ret = senss_set_sensitivity(handle, 2, 50);
	zassert_equal(ret, 0, "Set ACC 0 index 2 sensitivity 50 failed");

	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, 100);
	zassert_equal(ret, 0, "Set ACC 0 all index sensitivity 100 failed");

	/* negative test */
	ret = senss_set_sensitivity(SENSS_SENSOR_INVALID_HANDLE, 0, 100);
	zassert_true(ret < 0, "Negative test 1 failed");

	/* For ACC, valid channel: [-1,2] */
	ret = senss_set_sensitivity(handle, -2, 100);
	zassert_true(ret < 0, "Negative test 2 failed");

	ret = senss_set_sensitivity(handle, 3, 100);
	zassert_true(ret < 0, "Negative test 3 failed");

	ret = senss_close_sensor(handle);
	zassert_equal(ret, 0, "Close ACC 0 failed");
}

/**
 * @brief Test Set Sensitivity
 *
 * This test verifies senss_get_sensitivity
 */
ZTEST(senss_tests, test_senss_get_sensitivity)
{
	int handle = 0;
	uint32_t value;
	int ret;

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	/* positive test */
	ret = senss_get_sensitivity(handle, 0, &value);
	zassert_equal(ret, 0, "Get ACC 0 index 0 sensitivity failed");
	zassert_equal(value, 0, "Value is not equal to 0");

	ret = senss_set_sensitivity(handle, 0, 100);
	zassert_equal(ret, 0, "Set ACC 0 index 0 sensitivity 100 failed");
	value = 0;
	ret = senss_get_sensitivity(handle, 0, &value);
	zassert_equal(ret, 0, "Get ACC 0 index 0 sensitivity 100 failed");
	zassert_equal(value, 100, "Value is not equal to 100");

	ret = senss_set_sensitivity(handle, 2, 50);
	zassert_equal(ret, 0, "Set ACC 0 index 2 sensitivity 50 failed");
	value = 0;
	ret = senss_get_sensitivity(handle, 2, &value);
	zassert_equal(ret, 0, "Get ACC 0 index 2 sensitivity 50 failed");
	zassert_equal(value, 50, "Value is not equal to 50");

	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, 100);
	zassert_equal(ret, 0, "Set ACC 0 all index sensitivity 100 failed");
	value = 0;
	ret = senss_get_sensitivity(handle, SENSS_INDEX_ALL, &value);
	zassert_equal(ret, 0, "Get ACC 0 all sensitivity 100 failed");
	zassert_equal(value, 100, "Value is not equal to 100");

	/* negative test */
	ret = senss_get_sensitivity(SENSS_SENSOR_INVALID_HANDLE, 0, &value);
	zassert_true(ret < 0, "Negative test 1 failed");

	/* For ACC, valid channel: [-1,2] */
	ret = senss_get_sensitivity(handle, -2, &value);
	zassert_true(ret < 0, "Negative test 2 failed");

	ret = senss_get_sensitivity(handle, 3, &value);
	zassert_true(ret < 0, "Negative test 3 failed");

	/* Pass NULL to uint32_t *value */
	ret = senss_get_sensitivity(handle, 0, NULL);
	zassert_true(ret < 0, "Negative test 4 failed");

	ret = senss_close_sensor(handle);
	zassert_equal(ret, 0, "Close ACC 0 failed");
}

/**
 * @brief Test Senss Read Sample
 *
 * This test verifies senss_read_sample
 */
ZTEST(senss_tests, test_senss_read_sample)
{
	struct senss_sensor_value_3d_int32 sample = {0};
	uint64_t cur_time = get_us();
	uint32_t interval[] = {
		/* set frequency to 10HZ */
		INTERVAL_10HZ,
		/* set frequency to 0 before frequency updated */
		0,
		/* set frequency to 20HZ */
		INTERVAL_20HZ,
		/* increase frequency to 40HZ */
		INTERVAL_40HZ,
		/* reduce frequency to 10HZ */
		INTERVAL_10HZ
	};
	int handle = 0;
	uint64_t i;
	int ret;

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	/* positive test */
	for (i = 0; i < ARRAY_SIZE(interval); i++) {
		ret = senss_set_interval(handle, interval[i]);
		k_sleep(K_SECONDS(2));

		/* when interval equals to 0, skips the sample timestamp check */
		if (interval[i] == 0) {
			continue;
		}

		ret = senss_read_sample(handle, &sample, sizeof(sample));
		zassert_equal(ret, 0, "Read Sample ACC 0 failed");

		cur_time  = get_us();
		LOG_INF("Loop count: %lld Sample timestamp: %lld Cur_time: %lld",
				i, sample.header.base_timestamp, cur_time);
		zassert_between_inclusive(sample.header.base_timestamp,
				cur_time - interval[i], cur_time);
	}

	/* negative test */
	ret = senss_read_sample(SENSS_SENSOR_INVALID_HANDLE, &sample,
			sizeof(sample));
	zassert_true(ret < 0, "Negative test 1 failed");

	ret = senss_read_sample(handle, NULL, sizeof(sample));
	zassert_true(ret < 0, "Negative test 2 failed");

	ret = senss_read_sample(handle, &sample, 0);
	zassert_true(ret < 0, "Negative test 3 failed");

	ret = senss_close_sensor(handle);
	zassert_equal(ret, 0, "Close ACC 0 failed");
}

static int acc_0_callback(int handle, void *buf, int size, void *param)
{
	const struct senss_sensor_info *info = senss_get_sensor_info(handle);
	struct senss_tests_fixture *fixture = param;

	ARG_UNUSED(buf);
	ARG_UNUSED(size);

	zassert_equal(info->type, SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D,
			"Sensor type mismatch");
	zassert_equal(info->sensor_index, 0, "Sensor index mismatch");

	atomic_inc(&fixture->acc_0_samples);

	return 0;
}

/**
 * @brief Test ACC 0
 *
 * This test verifies the ACC 0
 */
ZTEST_F(senss_tests, test_acc_0)
{
	int handle;
	int interval;
	int elapse;
	int expect;
	int ret;

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	ret = senss_register_data_event_callback(handle, acc_0_callback,
			fixture);
	zassert_equal(ret, 0, "ACC 0 register callback failed");

	/* Test 10 Hz */
	interval = INTERVAL_10HZ;
	atomic_set(&fixture->acc_0_samples, 0);
	ret = senss_set_interval(handle, interval);
	zassert_equal(ret, 0, "ACC 0 set interval failed");

	elapse = 10 * USEC_PER_SEC;
	expect = elapse / interval;
	k_sleep(K_SECONDS(10));

	LOG_INF("ACC 0 Samples: %d expect: %d",
			atomic_get(&fixture->acc_0_samples), expect);
	zassert_within(atomic_get(&fixture->acc_0_samples), expect,
			1, "ACC 0 samples num out of range");

	/* Test 20 Hz */
	interval = INTERVAL_20HZ;
	atomic_set(&fixture->acc_0_samples, 0);
	ret = senss_set_interval(handle, interval);
	zassert_equal(ret, 0, "ACC 0 set interval failed");

	elapse = 10 * USEC_PER_SEC;
	expect = elapse / interval;
	k_sleep(K_SECONDS(10));

	LOG_INF("ACC 0 Samples: %d expect: %d",
			atomic_get(&fixture->acc_0_samples), expect);
	zassert_within(atomic_get(&fixture->acc_0_samples), expect,
			1, "ACC 0 samples num out of range");

	ret = senss_set_interval(handle, 0);
	zassert_equal(ret, 0, "ACC 0 stop streaming failed");

	ret = senss_close_sensor(handle);
	zassert_equal(ret, 0, "Close ACC 0 failed");
}

static int acc_1_callback(int handle, void *buf, int size, void *param)
{
	const struct senss_sensor_info *info = senss_get_sensor_info(handle);
	struct senss_tests_fixture *fixture = param;

	ARG_UNUSED(buf);
	ARG_UNUSED(size);

	zassert_equal(info->type, SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D,
			"Sensor type mismatch");
	zassert_equal(info->sensor_index, 1, "Sensor index mismatch");

	atomic_inc(&fixture->acc_1_samples);

	return 0;
}

/**
 * @brief Test ACC 0 and 1 parallel
 *
 * This test verifies the ACC 0 and 1 run parallel
 */
ZTEST_F(senss_tests, test_acc_0_and_acc_1)
{
	int acc_0, acc_1;
	int interval_0, interval_1;
	int elapse_0, elapse_1;
	int expect_0, expect_1;
	int ret;

	/* Open ACC 0 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&acc_0);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	ret = senss_register_data_event_callback(acc_0, acc_0_callback,
			fixture);
	zassert_equal(ret, 0, "ACC 0 register callback failed");

	/* Open ACC 1 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 1,
			&acc_1);
	zassert_equal(ret, 0, "Open ACC 1 failed");

	ret = senss_register_data_event_callback(acc_1, acc_1_callback,
			fixture);
	zassert_equal(ret, 0, "ACC 1 register callback failed");

	/* Test 10 Hz */
	interval_0 = INTERVAL_10HZ;
	atomic_set(&fixture->acc_0_samples, 0);
	ret = senss_set_interval(acc_0, interval_0);
	zassert_equal(ret, 0, "ACC 0 set interval failed");

	elapse_0 = 10 * USEC_PER_SEC;
	expect_0 = elapse_0 / interval_0;
	k_sleep(K_SECONDS(10));

	LOG_INF("ACC 0 Samples: %d expect: %d",
			atomic_get(&fixture->acc_0_samples), expect_0);
	zassert_within(atomic_get(&fixture->acc_0_samples), expect_0,
			1, "ACC 0 samples num out of range");

	/* Test 20 Hz */
	interval_1 = INTERVAL_20HZ;
	atomic_set(&fixture->acc_1_samples, 0);
	ret = senss_set_interval(acc_1, interval_1);
	zassert_equal(ret, 0, "ACC 1 set interval failed");

	elapse_1 = 10 * USEC_PER_SEC;
	expect_1 = elapse_1 / interval_1;
	elapse_0 += 10 * USEC_PER_SEC;
	expect_0 = elapse_0 / interval_0;
	k_sleep(K_SECONDS(10));

	LOG_INF("ACC 0 Samples: %d expect: %d",
			atomic_get(&fixture->acc_0_samples), expect_0);
	zassert_within(atomic_get(&fixture->acc_0_samples), expect_0,
			1, "ACC 0 samples num out of range");

	LOG_INF("ACC 1 Samples: %d expect: %d",
			atomic_get(&fixture->acc_1_samples), expect_1);
	zassert_within(atomic_get(&fixture->acc_1_samples), expect_1,
			1, "ACC 0 samples num out of range");

	ret = senss_set_interval(acc_0, 0);
	zassert_equal(ret, 0, "ACC 0 stop streaming failed");

	ret = senss_close_sensor(acc_0);
	zassert_equal(ret, 0, "Close ACC 0 failed");

	ret = senss_set_interval(acc_1, 0);
	zassert_equal(ret, 0, "ACC 1 stop streaming failed");

	ret = senss_close_sensor(acc_1);
	zassert_equal(ret, 0, "Close ACC 1 failed");

	k_sleep(K_SECONDS(2));

	LOG_INF("ACC 0 Samples: %d expect: %d",
			atomic_get(&fixture->acc_0_samples), expect_0);
	zassert_within(atomic_get(&fixture->acc_0_samples), expect_0,
			1, "ACC 0 samples num out of range");

	LOG_INF("ACC 1 Samples: %d expect: %d",
			atomic_get(&fixture->acc_1_samples), expect_1);
	zassert_within(atomic_get(&fixture->acc_1_samples), expect_1,
			1, "ACC 0 samples num out of range");
}

static int user_0_callback(int handle, void *buf, int size, void *param)
{
	const struct senss_sensor_info *info = senss_get_sensor_info(handle);
	struct senss_tests_fixture *fixture = param;

	ARG_UNUSED(buf);
	ARG_UNUSED(size);

	zassert_equal(info->type, SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D,
			"Sensor type mismatch");
	zassert_equal(info->sensor_index, 0, "Sensor index mismatch");

	atomic_inc(&fixture->user_0_samples);

	return 0;
}

static int user_1_callback(int handle, void *buf, int size, void *param)
{
	const struct senss_sensor_info *info = senss_get_sensor_info(handle);
	struct senss_tests_fixture *fixture = param;

	ARG_UNUSED(buf);
	ARG_UNUSED(size);

	zassert_equal(info->type, SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D,
			"Sensor type mismatch");
	zassert_equal(info->sensor_index, 0, "Sensor index mismatch");

	atomic_inc(&fixture->user_1_samples);

	return 0;
}

/**
 * @brief Test multiple instances of ACC 0
 *
 * This test verifies two users use acc 0 at the same time.
 */
ZTEST_F(senss_tests, test_acc_0_with_2_users)
{
	int user_0, user_1;
	int interval_0, interval_1;
	int elapse_0, elapse_1;
	int expect_0, expect_1;
	int ret;

	/* User 0 Open ACC 0 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&user_0);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	ret = senss_register_data_event_callback(user_0, user_0_callback,
			fixture);
	zassert_equal(ret, 0, "ACC 0 register callback failed");

	/* User 1 Open ACC 0 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&user_1);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	ret = senss_register_data_event_callback(user_1, user_1_callback,
			fixture);
	zassert_equal(ret, 0, "ACC 0 register callback failed");

	/* Test 10 Hz */
	interval_0 = INTERVAL_10HZ;
	atomic_set(&fixture->user_0_samples, 0);
	ret = senss_set_interval(user_0, interval_0);
	zassert_equal(ret, 0, "User 0 set interval failed");

	elapse_0 = 10 * USEC_PER_SEC;
	expect_0 = elapse_0 / interval_0;
	k_sleep(K_SECONDS(10));

	LOG_INF("User 0 Samples: %d expect: %d",
			atomic_get(&fixture->user_0_samples), expect_0);
	zassert_within(atomic_get(&fixture->user_0_samples), expect_0,
			1, "Samples num out of range");

	/* Test 20 Hz */
	interval_1 = INTERVAL_20HZ;
	atomic_set(&fixture->user_1_samples, 0);
	ret = senss_set_interval(user_1, interval_1);
	zassert_equal(ret, 0, "User 1 set interval failed");

	elapse_1 = 10 * USEC_PER_SEC;
	expect_1 = elapse_1 / interval_1;
	elapse_0 += 10 * USEC_PER_SEC;
	expect_0 = elapse_0 / interval_0;
	k_sleep(K_SECONDS(10));

	LOG_INF("User 0 Samples: %d expect: %d",
			atomic_get(&fixture->user_0_samples), expect_0);
	zassert_within(atomic_get(&fixture->user_0_samples), expect_0,
			1, "Samples num out of range");

	LOG_INF("User 1 Samples: %d expect: %d",
			atomic_get(&fixture->user_1_samples), expect_1);
	zassert_within(atomic_get(&fixture->user_1_samples), expect_1,
			1, "Samples num out of range");

	ret = senss_set_interval(user_0, 0);
	zassert_equal(ret, 0, "User 0 stop streaming failed");

	ret = senss_close_sensor(user_0);
	zassert_equal(ret, 0, "Close user 0 failed");

	ret = senss_set_interval(user_1, 0);
	zassert_equal(ret, 0, "User 1 stop streaming failed");

	ret = senss_close_sensor(user_1);
	zassert_equal(ret, 0, "Close user 1 failed");

	k_sleep(K_SECONDS(2));

	LOG_INF("User 0 Samples: %d expect: %d",
			atomic_get(&fixture->user_0_samples), expect_0);
	zassert_within(atomic_get(&fixture->user_0_samples), expect_0,
			1, "Samples num out of range");

	LOG_INF("User 1 Samples: %d expect: %d",
			atomic_get(&fixture->user_1_samples), expect_1);
	zassert_within(atomic_get(&fixture->user_1_samples), expect_1,
			1, "Samples num out of range");
}

/**
 * @brief Test set arbitrate interval of ACC 0, ACC1, ACC2
 *
 * This test verifies arbitrate ACC 0, ACC1, ACC2 interval.
 */
ZTEST_F(senss_tests, test_acc_0_1_2_interval_arbitrate)
{
	const struct senss_sensor_api *api;
	int acc_0, acc_1, acc_2;
	uint32_t value;
	int ret;

	/* Open ACC 0 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&acc_0);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	/* Open ACC 1 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&acc_1);
	zassert_equal(ret, 0, "Open ACC 1 failed");

	/* Open ACC 2 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&acc_2);
	zassert_equal(ret, 0, "Open ACC 2 failed");

	/* Set ACC 0, 1, 2 interval 10, 20, 40hz */
	ret = senss_set_interval(acc_0, INTERVAL_10HZ);
	zassert_equal(ret, 0, "ACC 0 set interval failed");

	ret = senss_set_interval(acc_1, INTERVAL_20HZ);
	zassert_equal(ret, 0, "ACC 1 set interval failed");

	ret = senss_set_interval(acc_2, INTERVAL_40HZ);
	zassert_equal(ret, 0, "ACC 2 set interval failed");

	LOG_INF("Sleep a while for senss to arbitrate and set interval");
	k_sleep(K_MSEC(200));

	/* Test ACC 0, 1, 2 arbitrate interval */
	api = fixture->dev->api;
	value = 0;
	ret = api->get_interval(fixture->dev, &value);
	zassert_equal(ret, 0, "Get arbitrate interval failed");
	zassert_equal(value, INTERVAL_40HZ, "Value is not equal to INTERVAL_40HZ");

	/* Test whether arbitrate interval work well when set ACC 2 interval 0 */
	ret = senss_set_interval(acc_2, 0);
	zassert_equal(ret, 0, "ACC 2 set interval failed");

	LOG_INF("Sleep a while for senss to arbitrate and set interval");
	k_sleep(K_MSEC(200));

	ret = api->get_interval(fixture->dev, &value);
	zassert_equal(ret, 0, "Get arbitrate interval failed");
	zassert_equal(value, INTERVAL_20HZ, "Value is not equal to INTERVAL_20HZ");

	/* Test whether arbitrate interval work well when close ACC 1 */
	ret = senss_close_sensor(acc_1);
	zassert_equal(ret, 0, "Close ACC 1 failed");

	LOG_INF("Sleep a while for senss to arbitrate and set interval");
	k_sleep(K_MSEC(200));

	ret = api->get_interval(fixture->dev, &value);
	zassert_equal(ret, 0, "Get arbitrate interval failed");
	zassert_equal(value, INTERVAL_10HZ, "Value is not equal to INTERVAL_10HZ");

	ret = senss_close_sensor(acc_0);
	zassert_equal(ret, 0, "Close ACC 0 failed");

	ret = senss_close_sensor(acc_2);
	zassert_equal(ret, 0, "Close ACC 2 failed");
}

/**
 * @brief Test set arbitrate sensitivity of ACC 0, ACC1, ACC2
 *
 * This test verifies arbitrate ACC 0, ACC1, ACC2 sensitivity.
 */
ZTEST_F(senss_tests, test_acc_0_1_2_sensitivity_arbitrate)
{
	const struct senss_sensor_api *api;
	int acc_0, acc_1, acc_2;
	uint32_t value;
	int ret;

	/* Open ACC 0 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&acc_0);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	/* Open ACC 1 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&acc_1);
	zassert_equal(ret, 0, "Open ACC 1 failed");

	/* Open ACC 2 */
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&acc_2);
	zassert_equal(ret, 0, "Open ACC 2 failed");

	/* Set ACC 0, 1, 2 interval and sensitivity */
	ret = senss_set_interval(acc_0, INTERVAL_10HZ);
	zassert_equal(ret, 0, "ACC 0 set interval failed");

	ret = senss_set_interval(acc_1, INTERVAL_20HZ);
	zassert_equal(ret, 0, "ACC 1 set interval failed");

	ret = senss_set_interval(acc_2, INTERVAL_40HZ);
	zassert_equal(ret, 0, "ACC 2 set interval failed");

	ret = senss_set_sensitivity(acc_0, 0, 100);
	zassert_equal(ret, 0, "ACC 0 set index 0 sensitivity failed");

	ret = senss_set_sensitivity(acc_1, 0, 200);
	zassert_equal(ret, 0, "ACC 1 set index 0 sensitivity failed");

	ret = senss_set_sensitivity(acc_2, 0, 300);
	zassert_equal(ret, 0, "ACC 2 set index 0 sensitivity failed");

	LOG_INF("Sleep a while for senss to arbitrate and set sensitivity");
	k_sleep(K_MSEC(200));

	/* Test ACC 0, 1, 2 arbitrate sensitivity */
	api = fixture->dev->api;
	value = 0;
	ret = api->get_sensitivity(fixture->dev, 0, &value);
	zassert_equal(ret, 0, "Get arbitrate sensitivity failed");
	zassert_equal(value, 100, "Value is not equal to 100");

	/* Test whether arbitrate sensitivity work well when set ACC 0 interval 0 */
	ret = senss_set_interval(acc_0, 0);
	zassert_equal(ret, 0, "ACC 0 set interval failed");

	LOG_INF("Sleep a while for senss to arbitrate and set sensitivity");
	k_sleep(K_MSEC(200));

	/* Test ACC 0, 1, 2 arbitrate sensitivity */
	api = fixture->dev->api;
	value = 0;
	ret = api->get_sensitivity(fixture->dev, 0, &value);
	zassert_equal(ret, 0, "Get arbitrate sensitivity failed");
	zassert_equal(value, 200, "Value is not equal to 200");

	/* Test whether arbitrate sensitivity work well when close ACC 1 */
	ret = senss_close_sensor(acc_1);
	zassert_equal(ret, 0, "Close ACC 1 failed");

	LOG_INF("Sleep a while for senss to arbitrate and set sensitivity");
	k_sleep(K_MSEC(200));

	ret = api->get_sensitivity(fixture->dev, 0, &value);
	zassert_equal(ret, 0, "Get arbitrate sensitivity failed");
	zassert_equal(value, 300, "Value is not equal to 300");

	ret = senss_close_sensor(acc_0);
	zassert_equal(ret, 0, "Close ACC 0 failed");

	ret = senss_close_sensor(acc_2);
	zassert_equal(ret, 0, "Close ACC 2 failed");
}

static int motion_detector_data_callback(int handle, void *buf, int size, void *param)
{
	struct senss_sensor_value_int32 *sample = (struct senss_sensor_value_int32 *)buf;
	uint32_t *reading_count = (uint32_t *)param;
	uint32_t i;

	for (i = 0; i < sample->header.reading_count; i++) {
		(*reading_count)++;
	}

	return 0;
}

ZTEST_F(senss_tests, test_motion_detector_open_and_close)
{
	int ret;
	int handle;

	/* positive test */
	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR, 0, &handle);
	zassert_true(ret == 0,
		"test_motion_detector_open_and_close open index 0 ret not expected");
	zassert_true(handle != SENSS_SENSOR_INVALID_HANDLE,
		"test_motion_detector_open_and_close open index 0 handle not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret == 0,
		"test_motion_detector_open_and_close close index 0 ret not expected");

	/* negative test */
	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR, 1, &handle);
	zassert_true(ret != 0,
		"test_motion_detector_open_and_close open index 1 ret not expected");
	zassert_true(handle == SENSS_SENSOR_INVALID_HANDLE,
		"test_motion_detector_open_and_close open index 1 handle not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret != 0,
		"test_motion_detector_open_and_close close index 1 ret not expected");
}

ZTEST_F(senss_tests, test_motion_detector_set_and_get_sensitivity)
{
	int ret;
	int handle;
	uint32_t sensitivity;

	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR, 0, &handle);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_sensitivity open ret not expected");
	zassert_true(handle != SENSS_SENSOR_INVALID_HANDLE,
		"test_motion_detector_set_and_get_sensitivity open handle not expected");

	/* set and get sensitivity 5 */
	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, 5);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_sensitivity 1st set ret not expected");
	sensitivity = SENSITIVITY_MAX;
	ret = senss_get_sensitivity(handle, SENSS_INDEX_ALL, &sensitivity);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_sensitivity 1st get ret not expected");
	zassert_true(sensitivity == 5,
		"test_motion_detector_set_and_get_sensitivity 1st get sensitivity not expected");

	/* set and get sensitivity 0 */
	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, 0);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_sensitivity 2nd set ret not expected");
	sensitivity = SENSITIVITY_MAX;
	ret = senss_get_sensitivity(handle, SENSS_INDEX_ALL, &sensitivity);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_sensitivity 2nd get ret not expected");
	zassert_true(sensitivity == 0,
		"test_motion_detector_set_and_get_sensitivity 2nd get sensitivity not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_sensitivity close ret not expected");
}

ZTEST_F(senss_tests, test_motion_detector_set_and_get_interval)
{
	int ret;
	int handle;
	uint32_t interval;

	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR, 0, &handle);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_interval open ret not expected");
	zassert_true(handle != SENSS_SENSOR_INVALID_HANDLE,
		"test_motion_detector_set_and_get_interval open handle not expected");

	/* set and get interval INTERVAL_10HZ */
	ret = senss_set_interval(handle, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_interval 1st set ret not expected");
	interval = INVALID_INTERVAL_US;
	ret = senss_get_interval(handle, &interval);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_interval 1st get ret not expected");
	zassert_true(interval == INTERVAL_10HZ,
		"test_motion_detector_set_and_get_interval 1st get interval not expected");

	/* set and get interval 0 */
	ret = senss_set_interval(handle, 0);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_interval 2nd set ret not expected");
	interval = INVALID_INTERVAL_US;
	ret = senss_get_interval(handle, &interval);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_interval 2nd get ret not expected");
	zassert_true(interval == 0,
		"test_motion_detector_set_and_get_interval 2nd get interval not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret == 0,
		"test_motion_detector_set_and_get_interval close ret not expected");
}

ZTEST_F(senss_tests, test_motion_detector_1user)
{
	int ret;
	int handle;
	uint32_t reading_count;

	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR, 0, &handle);
	zassert_true(ret == 0,
		"test_motion_detector_1user open ret not expected");
	ret = senss_register_data_event_callback(handle, motion_detector_data_callback,
		&reading_count);
	zassert_true(ret == 0,
		"test_motion_detector_1user register callback ret not expected");

	/* set sensitivity 0, INTERVAL_10HZ, expected reading_count greater than 0 */
	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, 0);
	zassert_true(ret == 0,
		"test_motion_detector_1user 1st set sensitivity ret not expected");
	reading_count = 0;
	ret = senss_set_interval(handle, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_motion_detector_1user 1st set interval ret not expected");
	k_sleep(K_SECONDS(5));
	zassert_true(reading_count > 0,
		"test_motion_detector_1user 1st reading_count not expected");

	/* set interval 0, expected reading_count equal to 0 */
	reading_count = 0;
	ret = senss_set_interval(handle, 0);
	zassert_true(ret == 0,
		"test_motion_detector_1user 2nd set interval 0 ret not expected");
	k_sleep(K_SECONDS(1));
	zassert_true(reading_count == 0,
		"test_motion_detector_1user 2nd reading_count not expected");

	/* set SENSITIVITY_MAX, INTERVAL_10HZ, expected reading_count equal to 1 */
	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, SENSITIVITY_MAX);
	zassert_true(ret == 0,
		"test_motion_detector_1user 3rd set sensitivity ret not expected");
	reading_count = 0;
	ret = senss_set_interval(handle, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_motion_detector_1user 3rd set interval ret not expected");
	k_sleep(K_SECONDS(1));
	zassert_true(reading_count == 1,
		"test_motion_detector_1user 3rd reading_count not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret == 0,
		"test_motion_detector_1user close ret not expected");
}

ZTEST_F(senss_tests, test_motion_detector_2users)
{
	int ret;
	int handle1, handle2;
	uint32_t reading_count1, reading_count2;

	/* handle1 open, register callback, set sensitivity, set interval */
	handle1 = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR, 0, &handle1);
	zassert_true(ret == 0,
		"test_motion_detector_2users handle1 open ret not expected");
	ret = senss_register_data_event_callback(handle1, motion_detector_data_callback,
		&reading_count1);
	zassert_true(ret == 0,
		"test_motion_detector_2users handle1 register callback ret not expected");
	reading_count1 = 0;
	ret = senss_set_sensitivity(handle1, SENSS_INDEX_ALL, 0);
	zassert_true(ret == 0,
		"test_motion_detector_2users handle1 set sensitivity ret not expected");
	ret = senss_set_interval(handle1, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_motion_detector_2users handle1 set interval ret not expected");

	/* handle2 open, register callback, set sensitivity, set interval */
	handle2 = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_MOTION_DETECTOR, 0, &handle2);
	zassert_true(ret == 0,
		"test_motion_detector_2users handle2 open ret not expected");
	ret = senss_register_data_event_callback(handle2, motion_detector_data_callback,
		&reading_count2);
	zassert_true(ret == 0,
		"test_motion_detector_2users handle2 register callback ret not expected");
	reading_count2 = 0;
	ret = senss_set_sensitivity(handle2, SENSS_INDEX_ALL, 0);
	zassert_true(ret == 0,
		"test_motion_detector_2users handle2 set sensitivity ret not expected");
	ret = senss_set_interval(handle2, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_motion_detector_2users handle2 set interval ret not expected");

	/* handle2 closes 1 second later than handle1 and expected more reading_count */
	k_sleep(K_SECONDS(5));
	ret = senss_close_sensor(handle1);
	zassert_true(ret == 0,
		"test_motion_detector_2users close handle1 ret not expected");
	k_sleep(K_SECONDS(1));
	ret = senss_close_sensor(handle2);
	zassert_true(ret == 0,
		"test_motion_detector_2users close handle2 ret not expected");
	zassert_true(reading_count1 > 0,
		"test_motion_detector_2users reading_count1 ret not expected");
	zassert_true(reading_count2 > reading_count1,
		"test_motion_detector_2users reading_count2 ret not expected");
}

static int hinge_angle_data_callback(int handle, void *buf, int size, void *param)
{
	struct senss_sensor_value_int32 *sample = (struct senss_sensor_value_int32 *)buf;
	uint32_t *reading_count = (uint32_t *)param;
	uint32_t i;

	LOG_INF("%s(%d),rd_cnt:%d", __func__, __LINE__, sample->header.reading_count);

	for (i = 0; i < sample->header.reading_count; i++) {
		(*reading_count)++;
	}

	return 0;
}

ZTEST_F(senss_tests, test_hinge_angle_open_and_close)
{
	int ret;
	int handle;

	/* positive test */
	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 0, &handle);
	zassert_true(ret == 0,
		"test_hinge_angle_open_and_close open index 0 ret not expected");
	zassert_true(handle != SENSS_SENSOR_INVALID_HANDLE,
		"test_hinge_angle_open_and_close open index 0 handle not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret == 0,
		"test_hinge_angle_open_and_close close index 0 ret not expected");

	/* negative test */
	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 1, &handle);
	zassert_true(ret != 0,
		"test_hinge_angle_open_and_close open index 1 ret not expected");
	zassert_true(handle == SENSS_SENSOR_INVALID_HANDLE,
		"test_hinge_angle_open_and_close open index 1 handle not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret != 0,
		"test_hinge_angle_open_and_close close index 1 ret not expected");
}

ZTEST_F(senss_tests, test_hinge_angle_set_and_get_sensitivity)
{
	int ret;
	int handle;
	uint32_t sensitivity;

	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 0, &handle);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_sensitivity open ret not expected");
	zassert_true(handle != SENSS_SENSOR_INVALID_HANDLE,
		"test_hinge_angle_set_and_get_sensitivity open handle not expected");

	/* set and get sensitivity 5 */
	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, 5);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_sensitivity 1st set ret not expected");
	sensitivity = SENSITIVITY_MAX;
	ret = senss_get_sensitivity(handle, SENSS_INDEX_ALL, &sensitivity);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_sensitivity 1st get ret not expected");
	zassert_true(sensitivity == 5,
		"test_hinge_angle_set_and_get_sensitivity 1st get sensitivity not expected");

	/* set and get sensitivity 0 */
	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, 0);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_sensitivity 2nd set ret not expected");
	sensitivity = SENSITIVITY_MAX;
	ret = senss_get_sensitivity(handle, SENSS_INDEX_ALL, &sensitivity);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_sensitivity 2nd get ret not expected");
	zassert_true(sensitivity == 0,
		"test_hinge_angle_set_and_get_sensitivity 2nd get sensitivity not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_sensitivity close ret not expected");
}

ZTEST_F(senss_tests, test_hinge_angle_set_and_get_interval)
{
	int ret;
	int handle;
	uint32_t interval;

	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 0, &handle);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_interval open ret not expected");
	zassert_true(handle != SENSS_SENSOR_INVALID_HANDLE,
		"test_hinge_angle_set_and_get_interval open handle not expected");

	/* set and get interval INTERVAL_10HZ */
	ret = senss_set_interval(handle, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_interval 1st set ret not expected");
	interval = INVALID_INTERVAL_US;
	ret = senss_get_interval(handle, &interval);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_interval 1st get ret not expected");
	zassert_true(interval == INTERVAL_10HZ,
		"test_hinge_angle_set_and_get_interval 1st get interval not expected");

	/* set and get interval 0 */
	ret = senss_set_interval(handle, 0);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_interval 2nd set ret not expected");
	interval = INVALID_INTERVAL_US;
	ret = senss_get_interval(handle, &interval);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_interval 2nd get ret not expected");
	zassert_true(interval == 0,
		"test_hinge_angle_set_and_get_interval 2nd get interval not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret == 0,
		"test_hinge_angle_set_and_get_interval close ret not expected");
}

ZTEST_F(senss_tests, test_hinge_angle_1user)
{
	int ret;
	int handle;
	uint32_t reading_count;

	handle = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 0, &handle);
	zassert_true(ret == 0,
		"test_hinge_angle_1user open ret not expected");
	ret = senss_register_data_event_callback(handle, hinge_angle_data_callback,
		&reading_count);
	zassert_true(ret == 0,
		"test_hinge_angle_1user register callback ret not expected");

	/* set sensitivity 0, INTERVAL_10HZ, expected reading_count greater than 0 */
	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, 0);
	zassert_true(ret == 0,
		"test_hinge_angle_1user 1st set sensitivity ret not expected");
	reading_count = 0;
	ret = senss_set_interval(handle, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_hinge_angle_1user 1st set interval ret not expected");
	k_sleep(K_SECONDS(1));
	zassert_true(reading_count > 0,
		"test_hinge_angle_1user 1st reading_count not expected");

	/* set interval 0, expected reading_count equal to 0 */
	reading_count = 0;
	ret = senss_set_interval(handle, 0);
	zassert_true(ret == 0,
		"test_hinge_angle_1user 2nd set interval 0 ret not expected");
	k_sleep(K_SECONDS(1));
	zassert_true(reading_count == 0,
		"test_hinge_angle_1user 2nd reading_count not expected");

	/* set SENSITIVITY_MAX, INTERVAL_10HZ, expected reading_count equal to 1 */
	ret = senss_set_sensitivity(handle, SENSS_INDEX_ALL, SENSITIVITY_MAX);
	zassert_true(ret == 0,
		"test_hinge_angle_1user 3rd set sensitivity ret not expected");
	reading_count = 0;
	ret = senss_set_interval(handle, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_hinge_angle_1user 3rd set interval ret not expected");
	k_sleep(K_SECONDS(1));
	zassert_true(reading_count == 1,
		"test_hinge_angle_1user 3rd reading_count not expected");

	ret = senss_close_sensor(handle);
	zassert_true(ret == 0,
		"test_hinge_angle_1user close ret not expected");
}

ZTEST_F(senss_tests, test_hinge_angle_2users)
{
	int ret;
	int handle1, handle2;
	uint32_t reading_count1, reading_count2;

	/* handle1 open, register callback, set sensitivity, set interval */
	handle1 = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 0, &handle1);
	zassert_true(ret == 0,
		"test_hinge_angle_2users handle1 open ret not expected");
	ret = senss_register_data_event_callback(handle1, hinge_angle_data_callback,
		&reading_count1);
	zassert_true(ret == 0,
		"test_hinge_angle_2users handle1 register callback ret not expected");
	reading_count1 = 0;
	ret = senss_set_sensitivity(handle1, SENSS_INDEX_ALL, 0);
	zassert_true(ret == 0,
		"test_hinge_angle_2users handle1 set sensitivity ret not expected");
	ret = senss_set_interval(handle1, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_hinge_angle_2users handle1 set interval ret not expected");

	/* handle2 open, register callback, set sensitivity, set interval */
	handle2 = SENSS_SENSOR_INVALID_HANDLE;
	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_HINGE_ANGLE, 0, &handle2);
	zassert_true(ret == 0,
		"test_hinge_angle_2users handle2 open ret not expected");
	ret = senss_register_data_event_callback(handle2, hinge_angle_data_callback,
		&reading_count2);
	zassert_true(ret == 0,
		"test_hinge_angle_2users handle2 register callback ret not expected");
	reading_count2 = 0;
	ret = senss_set_sensitivity(handle2, SENSS_INDEX_ALL, 0);
	zassert_true(ret == 0,
		"test_hinge_angle_2users handle2 set sensitivity ret not expected");
	ret = senss_set_interval(handle2, INTERVAL_10HZ);
	zassert_true(ret == 0,
		"test_hinge_angle_2users handle2 set interval ret not expected");

	/* handle2 closes 1 second later than handle1 and expected more reading_count */
	k_sleep(K_SECONDS(1));
	ret = senss_close_sensor(handle1);
	zassert_true(ret == 0,
		"test_hinge_angle_2users close handle1 ret not expected");
	k_sleep(K_SECONDS(1));
	ret = senss_close_sensor(handle2);
	zassert_true(ret == 0,
		"test_hinge_angle_2users close handle2 ret not expected");
	zassert_true(reading_count1 > 0,
		"test_hinge_angle_2users reading_count1 ret not expected");
	zassert_true(reading_count2 > reading_count1,
		"test_hinge_angle_2users reading_count2 ret not expected");
}
