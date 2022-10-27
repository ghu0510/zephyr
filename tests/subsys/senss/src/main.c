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

struct senss_tests_fixture {
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
	int handle = 0;
	struct senss_sensor_value_3d_int32 sample = {0};
	int ret;

	ret = senss_open_sensor(SENSS_SENSOR_TYPE_MOTION_ACCELEROMETER_3D, 0,
			&handle);
	zassert_equal(ret, 0, "Open ACC 0 failed");

	/* positive test */
	ret = senss_read_sample(handle, &sample, sizeof(sample));
	zassert_equal(ret, 0, "Read Sample ACC 0 failed");

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
	zassert_equal(ret, 0, "ACC 0 stop steaming failed");

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
	zassert_equal(ret, 0, "ACC 0 stop steaming failed");

	ret = senss_close_sensor(acc_0);
	zassert_equal(ret, 0, "Close ACC 0 failed");

	ret = senss_set_interval(acc_1, 0);
	zassert_equal(ret, 0, "ACC 1 stop steaming failed");

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
 * @brief Test multiple instanses of ACC 0
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
	zassert_equal(ret, 0, "User 0 stop steaming failed");

	ret = senss_close_sensor(user_0);
	zassert_equal(ret, 0, "Close user 0 failed");

	ret = senss_set_interval(user_1, 0);
	zassert_equal(ret, 0, "User 1 stop steaming failed");

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
