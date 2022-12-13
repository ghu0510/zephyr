/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_senss_phy_3d_sensor

#include <zephyr/ztest.h>
#include <zephyr/senss/senss.h>
#include <zephyr/sys/util.h>
#include "senss_sensor.h"
#include "phy_3d_sensor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test_phy_3d_sensor, LOG_LEVEL_INF);

#define INTERVAL_10HZ (100 * USEC_PER_MSEC)
#define INTERVAL_20HZ (50 * USEC_PER_MSEC)

struct phy_3d_sensor_tests_fixture {
	const struct device *dev;
};

/**
 * @brief Senss Tests setup
 *
 * This test verifies the senss_init
 */
static void *setup(void)
{
	static struct phy_3d_sensor_tests_fixture fixture;

	fixture.dev = DEVICE_DT_GET(DT_DRV_INST(0));

	zassert_equal(senss_init(), 0, "senss_init failed");

	return &fixture;
}

/**
 * @brief Senss Tests teardown
 *
 * This test verifies the senss_deinit
 */
static void teardown(void *ptr)
{
	ARG_UNUSED(ptr);

	zassert_equal(senss_deinit(), 0, "senss_deinit failed");
}

ZTEST_SUITE(phy_3d_sensor_tests, NULL, setup, NULL, NULL, teardown);

/**
 * @brief Test Phy_3d_sensor Init
 *
 * This test verifies the phy_3d_sensor_init
 */
ZTEST_F(phy_3d_sensor_tests, test_phy_3d_sensor_init)
{

	struct phy_3d_sensor_context *ctx;

	ctx = senss_sensor_get_ctx_data(fixture->dev);

	zassert_equal(ctx->dev, fixture->dev, "phy_3d_sensor_init failed");
}

/**
 * @brief Test Phy_3d_sensor Set Interval
 *
 * This test verifies phy_3d_sensor_set_interval
 */
ZTEST_F(phy_3d_sensor_tests, test_phy_3d_sensor_set_interval)
{
	const struct senss_sensor_api *phy_3d_sensor_api;
	struct phy_3d_sensor_context *ctx;
	int ret;

	phy_3d_sensor_api = fixture->dev->api;
	ctx = senss_sensor_get_ctx_data(fixture->dev);

	ret = phy_3d_sensor_api->set_interval(fixture->dev, INTERVAL_10HZ);
	zassert_equal(ret, 0, "Set ACC 0 interval 100 ms failed");
	zassert_equal(ctx->interval, INTERVAL_10HZ, "Interval is not equal to INTERVAL_10HZ");

	ret = phy_3d_sensor_api->set_interval(fixture->dev, INTERVAL_20HZ);
	zassert_equal(ret, 0, "Set ACC 0 interval 50 ms failed");
	zassert_equal(ctx->interval, INTERVAL_20HZ, "Interval is not equal to INTERVAL_20HZ");

	ret = phy_3d_sensor_api->set_interval(fixture->dev, 0);
	zassert_equal(ret, 0, "Set ACC 0 interval 0 ms failed");
	zassert_equal(ctx->interval, 0, "Interval is not equal to 0");
}

/**
 * @brief Test Phy_3d_sensor Get Interval
 *
 * This test verifies phy_3d_sensor_get_interval
 */
ZTEST_F(phy_3d_sensor_tests, test_phy_3d_sensor_get_interval)
{
	const struct senss_sensor_api *phy_3d_sensor_api;
	struct phy_3d_sensor_context *ctx;
	uint32_t value;
	int ret;

	phy_3d_sensor_api = fixture->dev->api;
	ctx = senss_sensor_get_ctx_data(fixture->dev);

	ret = phy_3d_sensor_api->set_interval(fixture->dev, INTERVAL_10HZ);
	zassert_equal(ret, 0, "Set ACC 0 interval 100 ms failed");
	value = 0;
	ret = phy_3d_sensor_api->get_interval(fixture->dev, &value);
	zassert_equal(ret, 0, "Get ACC 0 interval 100 ms failed");
	zassert_equal(value, INTERVAL_10HZ, "Value is not equal to INTERVAL_10HZ");

	ret = phy_3d_sensor_api->set_interval(fixture->dev, INTERVAL_20HZ);
	zassert_equal(ret, 0, "Set ACC 0 interval 50 ms failed");
	value = 0;
	ret = phy_3d_sensor_api->get_interval(fixture->dev, &value);
	zassert_equal(ret, 0, "Get ACC 0 interval 50 ms failed");
	zassert_equal(value, INTERVAL_20HZ, "Value is not equal to INTERVAL_20HZ");

	/* test whether set interval work well when set interval 0 */
	ret = phy_3d_sensor_api->set_interval(fixture->dev, 0);
	zassert_equal(ret, 0, "Set ACC 0 interval 0 ms failed");
	value = 1;
	ret = phy_3d_sensor_api->get_interval(fixture->dev, &value);
	zassert_equal(ret, 0, "Get ACC 0 interval 0 ms failed");
	zassert_equal(value, 0, "Value is not equal to 0");
}

/**
 * @brief Test Phy_3d_sensor Set Sensitivity
 *
 * This test verifies phy_3d_sensor_set_sensitivity
 */
ZTEST_F(phy_3d_sensor_tests, test_phy_3d_sensor_set_sensitivity)
{
	const struct senss_sensor_api *phy_3d_sensor_api;
	struct phy_3d_sensor_context *ctx;
	int ret;

	phy_3d_sensor_api = fixture->dev->api;
	ctx = senss_sensor_get_ctx_data(fixture->dev);

	ret = phy_3d_sensor_api->set_sensitivity(fixture->dev, 0, 100);
	zassert_equal(ret, 0, "Set ACC 0 index 0 sensitivity 100 failed");
	zassert_equal(ctx->sensitivity[0], 100, "Sensitivity is not equal to 100");

	ret = phy_3d_sensor_api->set_sensitivity(fixture->dev, 2, 50);
	zassert_equal(ret, 0, "Set ACC 0 index 2 sensitivity 50 failed");
	zassert_equal(ctx->sensitivity[2], 50, "Sensitivity is not equal to 50");

	/* test whether set sensitivity work well when set all senss index value */
	ret = phy_3d_sensor_api->set_sensitivity(fixture->dev, SENSS_INDEX_ALL, 100);
	zassert_equal(ret, 0, "Set ACC 0 all index sensitivity 100 failed");
	zassert_equal(ctx->sensitivity[0], 100, "Sensitivity is not equal to 100");
	zassert_equal(ctx->sensitivity[1], 100, "Sensitivity is not equal to 100");
	zassert_equal(ctx->sensitivity[2], 100, "Sensitivity is not equal to 100");

	/* negative test */
	ret = phy_3d_sensor_api->set_sensitivity(fixture->dev, -2, 100);
	zassert_true(ret < 0, "Set ACC 0 invalid index sensitivity not failed");

	ret = phy_3d_sensor_api->set_sensitivity(fixture->dev, 3, 100);
	zassert_true(ret < 0, "Set ACC 0 invalid index sensitivity not failed");
}

/**
 * @brief Test Phy_3d_sensor Get Sensitivity
 *
 * This test verifies phy_3d_sensor_get_sensitivity
 */
ZTEST_F(phy_3d_sensor_tests, test_phy_3d_sensor_get_sensitivity)
{
	const struct senss_sensor_api *phy_3d_sensor_api;
	struct phy_3d_sensor_context *ctx;
	int ret;
	uint32_t value;

	phy_3d_sensor_api = fixture->dev->api;
	ctx = senss_sensor_get_ctx_data(fixture->dev);

	ret = phy_3d_sensor_api->set_sensitivity(fixture->dev, 0, 100);
	zassert_equal(ret, 0, "Set ACC 0 index 0 sensitivity 100 failed");
	value = 0;
	ret = phy_3d_sensor_api->get_sensitivity(fixture->dev, 0, &value);
	zassert_equal(ret, 0, "Get ACC 0 index 0 sensitivity 100 failed");
	zassert_equal(value, 100, "Value is not equal to 100");

	ret = phy_3d_sensor_api->set_sensitivity(fixture->dev, 2, 50);
	zassert_equal(ret, 0, "Set ACC 0 index 2 sensitivity 50 failed");
	value = 0;
	ret = phy_3d_sensor_api->get_sensitivity(fixture->dev, 2, &value);
	zassert_equal(ret, 0, "Get ACC 0 index 2 sensitivity 50 failed");
	zassert_equal(value, 50, "Value is not equal to 100");

	/* test whether get sensitivity work well when get all senss index value */
	value = 0;
	ret = phy_3d_sensor_api->get_sensitivity(fixture->dev, SENSS_INDEX_ALL, &value);
	zassert_true(ret < 0, "Sensitivities are different");

	ret = phy_3d_sensor_api->set_sensitivity(fixture->dev, SENSS_INDEX_ALL, 100);
	zassert_equal(ret, 0, "Set ACC 0 all index sensitivity 100 failed");
	value = 0;
	ret = phy_3d_sensor_api->get_sensitivity(fixture->dev, SENSS_INDEX_ALL, &value);
	zassert_equal(ret, 0, "Get ACC 0 all index sensitivity 100 failed");
	zassert_equal(value, 100, "Value is not equal to 100");

	/* negative test */
	ret = phy_3d_sensor_api->get_sensitivity(fixture->dev, -2, &value);
	zassert_true(ret < 0, "Get ACC 0 invalid index sensitivity not failed");

	ret = phy_3d_sensor_api->get_sensitivity(fixture->dev, 3, &value);
	zassert_true(ret < 0, "Get ACC 0 invalid index sensitivity not failed");
}

/**
 * @brief Test Phy_3d_sensor Test Sensitivity
 *
 * This test verifies phy_3d_sensor_sensitivity_test
 */
ZTEST_F(phy_3d_sensor_tests, test_phy_3d_sensor_sensitivity_test)
{
	struct senss_sensor_value_3d_int32 last_sample = {0};
	struct senss_sensor_value_3d_int32 current_sample = {0};
	const struct senss_sensor_api *phy_3d_sensor_api;
	struct phy_3d_sensor_context *ctx;
	int ret;

	phy_3d_sensor_api = fixture->dev->api;
	ctx = senss_sensor_get_ctx_data(fixture->dev);

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 0, 100, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 0 sensitivity 100 failed");

	/* test x axis sample data sensitivity */
	last_sample.readings[0].x = 100;
	current_sample.readings[0].x = 300;

	/* test whether test sensitivity work well when test senss index 0 value */
	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 0, 100, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 1, "Test ACC 0 index 0 sensitivity 100 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 0, 200, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 1, "Test ACC 0 index 0 sensitivity 200 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 0, 300, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 0 sensitivity 300 failed");

	/* test whether test sensitivity work well when test senss index 1 value */
	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 1, 100, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 1 sensitivity 100 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 1, 200, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 1 sensitivity 200 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 1, 300, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 1 sensitivity 300 failed");

	/* test whether test sensitivity work well when test senss all index value */
	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, SENSS_INDEX_ALL, 100,
			&last_sample, sizeof(last_sample), &current_sample,
			sizeof(current_sample));
	zassert_equal(ret, 1, "Test ACC 0 all index sensitivity 100 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, SENSS_INDEX_ALL, 200,
			&last_sample, sizeof(last_sample), &current_sample,
			sizeof(current_sample));
	zassert_equal(ret, 1, "Test ACC 0 all index sensitivity 200 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, SENSS_INDEX_ALL, 300,
			&last_sample, sizeof(last_sample), &current_sample,
			sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 all index sensitivity 300 failed");

	last_sample.readings[0].x = 0;
	current_sample.readings[0].x = 0;
	/* test y axis sample data sensitivity */
	last_sample.readings[0].y = 300;
	current_sample.readings[0].y = 100;

	/* test whether test sensitivity work well when test senss index 0 value */
	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 0, 100, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 0 sensitivity 100 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 0, 200, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 0 sensitivity 200 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 0, 300, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 0 sensitivity 300 failed");

	/* test whether test sensitivity work well when test senss index 1 value */
	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 1, 100, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 1, "Test ACC 0 index 1 sensitivity 100 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 1, 200, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 1, "Test ACC 0 index 1 sensitivity 200 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 1, 300, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 index 1 sensitivity 300 failed");

	/* test whether test sensitivity work well when test senss all index value */
	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, SENSS_INDEX_ALL, 100,
			&last_sample, sizeof(last_sample), &current_sample,
			sizeof(current_sample));
	zassert_equal(ret, 1, "Test ACC 0 all index sensitivity 100 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, SENSS_INDEX_ALL, 200,
			&last_sample, sizeof(last_sample), &current_sample,
			sizeof(current_sample));
	zassert_equal(ret, 1, "Test ACC 0 all index sensitivity 200 failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, SENSS_INDEX_ALL, 300,
			&last_sample, sizeof(last_sample), &current_sample,
			sizeof(current_sample));
	zassert_equal(ret, 0, "Test ACC 0 all index sensitivity 300 failed");

	/* negative test */
	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, -2, 100, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_true(ret < 0, "Test ACC 0 invalid index sensitivity not failed");

	ret = phy_3d_sensor_api->sensitivity_test(fixture->dev, 3, 100, &last_sample,
			sizeof(last_sample), &current_sample, sizeof(current_sample));
	zassert_true(ret < 0, "Test ACC 0 invalid index sensitivity not failed");
}
