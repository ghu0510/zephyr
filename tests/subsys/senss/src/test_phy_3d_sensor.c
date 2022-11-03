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
