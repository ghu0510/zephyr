/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SENSS_H_
#define ZEPHYR_INCLUDE_SENSS_H_

/**
 * @defgroup senss Sensor Subsystem APIs
 * @defgroup senss_api API
 * @ingroup senss
 * @defgroup senss_sensor_types  Sensor Types
 * @ingroup senss
 * @defgroup senss_datatypes Data Types
 * @ingroup senss
 */

#include <zephyr/senss/senss_datatypes.h>
#include <zephyr/senss/senss_sensor_types.h>

/**
 * @brief Sensor Subsystem API
 * @addtogroup senss_api
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @struct senss_sensor_version
 * @brief Sensor Version
 */
struct senss_sensor_version {
	union {
		uint32_t value;
		struct {
			uint8_t major;
			uint8_t minor;
			uint8_t hotfix;
			uint8_t build;
		};
	};
};

#define SENSS_SENSOR_INVALID_HANDLE (-1)
#define SENSS_INDEX_ALL (-1)

#define SENSS_SENSOR_VERSION(_major, _minor, _hotfix, _build)           \
	((_major) << 24 | (_minor) << 16 | (_hotfix) << 8 | (_build))


/**
 * @brief Sensor flag indicating if this sensor is on event reporting data.
 *
 * Reporting sensor data when the sensor event occurs, such as a motion detect sensor reporting
 * a motion or motionless detected event.
 */
#define SENSS_SENSOR_FLAG_REPORT_ON_EVENT			BIT(0)

/**
 * @brief Sensor flag indicating if this sensor is on change reporting data.
 *
 * Reporting sensor data when the sensor data changes.
 *
 * Exclusive with  \ref SENSS_SENSOR_FLAG_REPORT_ON_EVENT
 */
#define SENSS_SENSOR_FLAG_REPORT_ON_CHANGE			BIT(1)


/**
 * @brief Sensor state.
 *
 */
enum senss_sensor_state {
	SENSS_SENOSR_STATE_NOT_READY = 1,
	SENSS_SENOSR_STATE_READY = 2,
};


/**
 * @struct senss_sensor_info
 * @brief Sensor basic constant information
 *
 */
struct senss_sensor_info {
	/** Name of the sensor instance */
	const char *name;

	/** Friendly name of the sensor instance */
	const char *friendly_name;

	/** Vendor name of the sensor instance */
	const char *vendor;

	/** Model name of the sensor instance */
	const char *model;

	/** Sensor type */
	int32_t type;

	/** Sensor index for multiple sensors under same sensor type */
	int32_t sensor_index;

	/** Sensor flags */
	uint32_t flags;

	/** Minimal report interval in micro seconds  */
	uint32_t minimal_interval;

	/** Sensor version */
	struct senss_sensor_version version;
};



/**
 * @brief Initialize sensor subsystem runtime.
 *
 * This will allocate internal resources, such as create internal schedule
 * task, create internal management data structures.
 *
 * It will also enumerate all registered sensor instances in sensor subsystem,
 * build sensors report relational data model, if a virtual sensor's required dependency
 * sensor type not found, will return error.
 * Then call each sensor instances' init callback API.
 *
 * @return 0 on success or negative error value on failure.
 *
 */
int senss_init(void);

 /**
  * @brief De-initialize sensor subsystem runtime.
  *
  * This will release all dynamically allocated internal resources, and call
  * all sensor instances' deinit callback API.
  *
  * @return 0 on success or negative error value on failure.
  */
int senss_deinit(void);

 /**
  * @brief Get all supported sensor instances' information.
  *
  * This API just returned read only information of sensor instances,
  * no side effect to sensor instances.
  *
  * @param **info For receiving sensor instances' information array pointer.
  *
  * @return Number of sensor instances, 0 returned if no sensors.
  */
int senss_get_sensors(const struct senss_sensor_info **info);


/**
 * @brief Open sensor instance by sensor type and sensor index.
 *
 * Application clients use it to open a sensor instance and get its handle.
 * Support multiple Application clients for open same sensor instance,
 * in this case, the returned handle will different for different clients.
 *
 * @param type The sensor type which need to open.
 *
 * @param sensor_index The index which need to open.
 *
 * @param *handle The opened instance handle, if failed will be set to -1.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_open_sensor(
		int type, int sensor_index,
		int *handle);

/**
 * @brief Close sensor instance.
 *
 * @param handle The sensor instance handle need to close.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_close_sensor(
		int handle);


/**
 * @brief Get current report interval of given client handle.
 *
 * @param handle The sensor instance handle.
 *
 * @param value The data buffer to receive value (micro seconds unit).
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_get_interval(
		int handle,
		uint32_t *value);

/**
 * @brief Set current report interval of given client handle.
 *
 * @param handle The sensor instance handle.
 *
 * @param value The value to be set (micro seconds unit).
 *
 * @return 0 on success or negative error value on failure, not support etc.
 */
int senss_set_interval(
		int handle,
		uint32_t value);

/**
 * @brief Set current data change sensitivity of given client handle.
 *
 * Since each sensor type may have multiple data fields for it's value, this
 * API support set separated sensitivity for each data field, or global
 * sensitivity for all data fields.
 *
 * @param handle The sensor instance handle.
 *
 * @param index The value fields index to be set, -1 for all fields (global).
 *
 * @param value The value to be set.
 *
 * @return 0 on success or negative error value on failure, not support etc.
 */
int senss_set_sensitivity(
		int handle,
		int index, uint32_t value);

/**
 * @brief Get current data change sensitivity of given client handle.
 *
 * Since each sensor type may have multiple data fields for it's value, this
 * API support get separated sensitivity for each data field, or global
 * sensitivity for all data fields.
 *
 * @param handle The sensor instance handle.
 *
 * @param index The value fields index to be set, -1 for all fields (global).
 *
 * @param value The data buffer to receive value.
 *
 * @return 0 on success or negative error value on failure, not support etc.
 */
int senss_get_sensitivity(
		int handle,
		int index, uint32_t *value);

/**
 * @brief Read a data sample
 *
 * This API will trigger target sensor read a sample each time.
 *
 * @param handle The sensor instance handle.
 *
 * @param buf The data buffer to receive sensor data sample.
 *
 * @param size The buffer size in bytes.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_read_sample(
		int handle,
		void *buf, int size);

/**
 * @brief Sensor data event receive callback.
 *
 * @param handle The sensor instance handle.
 *
 * @param buf The data buffer with sensor data.
 *
 * @param size The buffer size in bytes.
 *
 * @param param The user private parameter.
 *
 * @return 0 on success or negative error value on failure.
 */
typedef int (*senss_data_event_t)(
		int handle,
		void *buf, int size, void *param);

/**
 * @brief Register sensor data event receive callback.
 *
 * User can use this API to register a data event receive callback function.
 *
 * @param handle The sensor instance handle.
 *
 * @param callback The data event receive callback function.
 *
 * @param param The user private parameter.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_register_data_event_callback(
		int handle,
		senss_data_event_t callback, void *param);


/**
 * @brief Set report latency of a sensor instance.
 *
 * Report latency controls how long the sensor data events of the sensor instance will be buffered
 * in the batching buffer before deliver to its client.
 *
 * For 0 latency value, it's means sensor data event will not buffered, and will report to its
 * client immediately.
 *
 * The maximum sensor data events of this sensor instance is calculated as: latency /
 * report interval.
 *
 * @param handle The sensor instance handle.
 *
 * @param latency The latency in micro seconds.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_set_report_latency(
		int handle, uint64_t latency);

/**
 * @brief Get current report latency value of a sensor instance.
 *
 * @param handle The sensor instance handle.
 *
 * @param latency The value buffer to receive report latency in micro seconds.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_get_report_latency(
		int handle, uint64_t *latency);

/**
 * @brief Flush batching buffer for a sensor instance.
 *
 * @param handle The sensor instance handle.
 *
 * If \p handle is -1, will flush all buffer.
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_batching_flush(
		int handle);

/**
 * @brief Get sensor information from sensor instance handle.
 *
 * @param handle The sensor instance hande.
 *
 * @return a const pointer to \ref senss_sensor_info on success or NULL on failure.
 */
const struct senss_sensor_info *senss_get_sensor_info(
		int handle);

/**
 * @brief Get sensor instance's state
 *
 * @param handle The sensor instance hande.
 *
 * @param state Returned sensor state value
 *
 * @return 0 on success or negative error value on failure.
 */
int senss_get_sensor_state(
		int handle,
		enum senss_sensor_state *state);


#ifdef __cplusplus
}
#endif

/**
 * @}
 */


#endif /*ZEPHYR_INCLUDE_SENSS_H_*/
