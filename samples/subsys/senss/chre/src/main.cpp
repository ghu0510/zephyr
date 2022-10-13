/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/senss/senss.h>
#include "apps.hpp"
#include "chre/core/event_loop_manager.h"
#include "chre/target_platform/init.h"
#include "chre/util/nanoapp/app_id.h"

#define WAIT_CHRE_RUN_MAX_MS 500

int main(void)
{
	int ret;
	int i;

	ret = senss_init();
	if (ret) {
		LOG_ERR("senss init error");
		return -1;
	}

	LOG_INF("senss run successfully");

	auto sensor_app = chre::initializeStaticNanoappSensorWorld();
	auto& event_loop =
		chre::EventLoopManagerSingleton::get()->getEventLoop();

	if (chre::zephyr::init()) {
		LOG_ERR("chre init error");
		return -1;
	}

	for (i = 0; i < WAIT_CHRE_RUN_MAX_MS; i++) {
		k_msleep(1);
		/* post system event return true means chre is running */
		if (event_loop.postSystemEvent(static_cast<uint16_t>(
			chre::SystemCallbackType::FirstCallbackType), NULL,
			[](uint16_t type, void *data, void *extraData) { },
			NULL)) {
			break;
		}
	}

	if (i == WAIT_CHRE_RUN_MAX_MS) {
		LOG_ERR("chre run timeout, waited %d ms",
			WAIT_CHRE_RUN_MAX_MS);
		return -1;
	}

	LOG_INF("chre run successfully");

	if (!event_loop.startNanoapp(sensor_app)) {
		LOG_ERR("chre sensor nanoapp start error");
		return -1;
	}

	LOG_INF("chre sensor nanoapp start successfully");
	return 0;
}
