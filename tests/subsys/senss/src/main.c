/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/senss/senss.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test_senss, LOG_LEVEL_INF);

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
