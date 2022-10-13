.. _senss-sample:

Sensor subsystem sample
#######################

Overview
********

A simple sample that shows how to use the sensors with senss APIs. It defines
four sensors, with the underlying device bmi160 emulator, and get the sensor
data in defined interval.

The program runs in the following sequence:

#. Define the sensor in the dts

#. Open the sensor

#. Register call back.

#. Set sample interval

#. Run forever and get the sensor data.

Building and Running
********************

This application can be built and executed on native_posix as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/senss/simple
   :host-os: unix
   :board: native_posix
   :goals: run
   :compact:

To build for another board, change "native_posix" above to that board's name.
At the current stage, it only support native posix

Sample Output
=============

.. code-block:: console

    [00:00:00.000,000] <inf> emul: Registering 1 emulator(s) for i2c@100
    [00:00:00.000,000] <inf> i2c_emul_ctlr: Register emulator 'bmi@68' at I2C addr 68
    [00:00:00.000,000] <inf> emul: Registering 1 emulator(s) for spi@200
    [00:00:00.000,000] <inf> spi_emul_ctlr: Register emulator 'bmi@3' at cs 3
    [00:00:00.000,000] <inf> bosch_bmi160: write 7e = b6
    [00:00:00.000,000] <inf> bosch_bmi160:    * soft reset
    [00:00:00.001,000] <inf> bosch_bmi160: read 7f =
    [00:00:00.001,000] <inf> bosch_bmi160:    * Bus start
    [00:00:00.001,000] <inf> bosch_bmi160:        = 0
    [00:00:00.001,150] <inf> bosch_bmi160: read 0 =
    [00:00:00.001,150] <inf> bosch_bmi160:    * get chipid
    [00:00:00.001,150] <inf> bosch_bmi160:        = d1
    [00:00:00.001,150] <inf> bosch_bmi160: write 7e = 18
    [00:00:00.001,150] <inf> bosch_bmi160:    * pmu mag = 0, new status 0
    [00:00:00.001,500] <inf> bosch_bmi160: read 3 =
    [00:00:00.001,500] <inf> bosch_bmi160:    * get pmu
    [00:00:00.001,500] <inf> bosch_bmi160:        = 0
    [00:00:00.001,500] <inf> bosch_bmi160: write 7e = 11
    [00:00:00.001,500] <inf> bosch_bmi160:    * pmu acc = 1, new status 10
    [00:00:00.004,700] <inf> bosch_bmi160: read 3 =
    [00:00:00.004,700] <inf> bosch_bmi160:    * get pmu
    [00:00:00.004,700] <inf> bosch_bmi160:        = 10
    [00:00:00.004,700] <inf> bosch_bmi160: write 7e = 15
    [00:00:00.004,700] <inf> bosch_bmi160:    * pmu gyr = 1, new status 4
    [00:00:00.059,700] <inf> bosch_bmi160: read 3 =
    [00:00:00.059,700] <inf> bosch_bmi160:    * get pmu
    [00:00:00.059,700] <inf> bosch_bmi160:        = 4
    [00:00:00.059,700] <inf> bosch_bmi160: read 40 =
    [00:00:00.059,700] <inf> bosch_bmi160:    * acc conf
    [00:00:00.059,700] <inf> bosch_bmi160:        = 0
    [00:00:00.059,700] <inf> bosch_bmi160: write 40 = 0
    [00:00:00.059,700] <inf> bosch_bmi160:    * acc conf
    [00:00:00.059,700] <inf> bosch_bmi160: write 41 = 3
    [00:00:00.059,700] <inf> bosch_bmi160:    * acc range
    [00:00:00.059,700] <inf> bosch_bmi160: write 43 = 0
    [00:00:00.059,700] <inf> bosch_bmi160:    * gyr range
    [00:00:00.059,700] <inf> bosch_bmi160: read 40 =
    [00:00:00.059,700] <inf> bosch_bmi160:    * acc conf
    [00:00:00.059,700] <inf> bosch_bmi160:        = 0
    [00:00:00.059,700] <inf> bosch_bmi160: write 40 = 8
    [00:00:00.059,700] <inf> bosch_bmi160:    * acc conf
    [00:00:00.059,700] <inf> bosch_bmi160: read 42 =
    [00:00:00.059,700] <inf> bosch_bmi160:    * gyr conf
    [00:00:00.059,700] <inf> bosch_bmi160:        = 0
    [00:00:00.059,700] <inf> bosch_bmi160: write 42 = 8
    [00:00:00.059,700] <inf> bosch_bmi160:    * gyr conf
    [00:00:00.059,700] <inf> bosch_bmi160: write 7e = b6
    [00:00:00.059,700] <inf> bosch_bmi160:    * soft reset
    [00:00:00.060,700] <inf> bosch_bmi160: read 7f =
    [00:00:00.060,700] <inf> bosch_bmi160:    * Bus start
    [00:00:00.060,700] <inf> bosch_bmi160:        = 0
    [00:00:00.060,850] <inf> bosch_bmi160: read 0 =
    [00:00:00.060,850] <inf> bosch_bmi160:    * get chipid
    [00:00:00.060,850] <inf> bosch_bmi160:        = d1
    [00:00:00.060,850] <inf> bosch_bmi160: write 7e = 18
    [00:00:00.060,850] <inf> bosch_bmi160:    * pmu mag = 0, new status 0
    [00:00:00.061,200] <inf> bosch_bmi160: read 3 =
    [00:00:00.061,200] <inf> bosch_bmi160:    * get pmu
    [00:00:00.061,200] <inf> bosch_bmi160:        = 0
    [00:00:00.061,200] <inf> bosch_bmi160: write 7e = 11
    [00:00:00.061,200] <inf> bosch_bmi160:    * pmu acc = 1, new status 10
    [00:00:00.064,400] <inf> bosch_bmi160: read 3 =
    [00:00:00.064,400] <inf> bosch_bmi160:    * get pmu
    [00:00:00.064,400] <inf> bosch_bmi160:        = 10
    [00:00:00.064,400] <inf> bosch_bmi160: write 7e = 15
    [00:00:00.064,400] <inf> bosch_bmi160:    * pmu gyr = 1, new status 4
    [00:00:00.119,400] <inf> bosch_bmi160: read 3 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * get pmu
    [00:00:00.119,400] <inf> bosch_bmi160:        = 4
    [00:00:00.119,400] <inf> bosch_bmi160: read 40 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160:        = 0
    [00:00:00.119,400] <inf> bosch_bmi160: write 40 = 0
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160: write 41 = 3
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc range
    [00:00:00.119,400] <inf> bosch_bmi160: write 43 = 0
    [00:00:00.119,400] <inf> bosch_bmi160:    * gyr range
    [00:00:00.119,400] <inf> bosch_bmi160: read 40 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160:        = 0
    [00:00:00.119,400] <inf> bosch_bmi160: write 40 = 8
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160: read 42 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * gyr conf
    [00:00:00.119,400] <inf> bosch_bmi160:        = 0
    [00:00:00.119,400] <inf> bosch_bmi160: write 42 = 8
    [00:00:00.119,400] <inf> bosch_bmi160:    * gyr conf
    *** Booting Zephyr OS build zephyr-3.1.99-intel-20220912-20-g5688f21b42e9  ***
    [00:00:00.119,400] <inf> senss: allocate_sensor, conns_num:0
    [00:00:00.119,400] <inf> senss: create_sensor_obj, sensor:base-accel, min_ri:625(us)
    [00:00:00.119,400] <inf> senss: allocate_sensor, conns_num:0
    [00:00:00.119,400] <inf> senss: create_sensor_obj, sensor:lid-accel, min_ri:625(us)
    [00:00:00.119,400] <inf> senss: allocate_sensor, conns_num:1
    [00:00:00.119,400] <inf> senss: create_sensor_obj, sensor:motion-detector, min_ri:100000(us)
    [00:00:00.119,400] <inf> senss: allocate_sensor, conns_num:2
    [00:00:00.119,400] <inf> senss: create_sensor_obj, sensor:hinge-angle, min_ri:100000(us)
    [00:00:00.119,400] <inf> phy_acc: base-accel: Underlying device: bmi@68
    [00:00:00.119,400] <inf> senss: senss_sensor_set_data_ready, sensor:base-accel, data_ready:1, polling:2
    [00:00:00.119,400] <inf> senss: senss_sensor_set_data_ready, sensor:base-accel, data_ready:0, polling:1
    [00:00:00.119,400] <inf> phy_acc: base-accel: Configured for polled sampling.
    [00:00:00.119,400] <inf> phy_acc: lid-accel: Underlying device: bmi@3
    [00:00:00.119,400] <inf> senss: senss_sensor_set_data_ready, sensor:lid-accel, data_ready:1, polling:2
    [00:00:00.119,400] <inf> senss: senss_sensor_set_data_ready, sensor:lid-accel, data_ready:0, polling:1
    [00:00:00.119,400] <inf> phy_acc: lid-accel: Configured for polled sampling.
    [00:00:00.119,400] <inf> motion_detector: [md_init] name: motion-detector
    [00:00:00.119,400] <inf> senss: senss_get_sensor_info, conn:0xf61017a4
    [00:00:00.119,400] <inf> motion_detector: [md_init] reporter_handles[0] 0, type 0x73 index 1
    [00:00:00.119,400] <inf> hinge_angle: [hinge_init] name: hinge-angle
    [00:00:00.119,400] <inf> senss: senss_get_sensor_info, conn:0xf6101874
    [00:00:00.119,400] <inf> hinge_angle: [hinge_init] reporter_handles[0] 1, type 0x73 index 0
    [00:00:00.119,400] <inf> senss: senss_get_sensor_info, conn:0xf61018c0
    [00:00:00.119,400] <inf> hinge_angle: [hinge_init] reporter_handles[1] 2, type 0x73 index 1
    [00:00:00.119,400] <inf> senss: senss_runtime_thread start...
    [00:00:00.119,400] <inf> senss: sensor:base-accel need to execute, next_exec_time:0, mode:1, interval:-1
    [00:00:00.119,400] <inf> senss: sensor base-accel not in polling mode:1 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:lid-accel need to execute, next_exec_time:0, mode:1, interval:-1
    [00:00:00.119,400] <inf> senss: sensor lid-accel not in polling mode:1 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:motion-detector need to execute, next_exec_time:0, mode:0, interval:-1
    [00:00:00.119,400] <inf> senss: sensor motion-detector not in polling mode:0 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:hinge-angle need to execute, next_exec_time:0, mode:0, interval:-1
    [00:00:00.119,400] <inf> senss: sensor hinge-angle not in polling mode:0 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: calc_sleep_time, next:-1, cur:119400, sleep_time:-1(ms)
    [00:00:00.119,400] <inf> senss: senss_mgmt_thread start...
    [00:00:00.119,400] <inf> main: senss run successfully
    [00:00:00.119,400] <inf> senss: allocate_sensor, conns_num:1
    [00:00:00.119,400] <inf> senss: open_sensor_successfully, sensor:base-accel, state:0x2, conn_index:3
    [00:00:00.119,400] <inf> senss: senss_set_interval, dynamic connection:1, sensor:base-accel, interval:100000
    [00:00:00.119,400] <inf> senss: set_interval, conn:3, sensor:base-accel, dynamic_connection:1, interval:100000
    [00:00:00.119,400] <inf> senss: set_reporeter_interval, sensor:base-accel, mode:1, dynamic connection:1, interval:100000
    [00:00:00.119,400] <inf> senss: save_config_and_notify, sensor:base-accel, append sensor to cfg_list
    [00:00:00.119,400] <inf> senss: senss_runtime_thread, config_ready
    [00:00:00.119,400] <inf> senss: sensor_later_config, config virtual sensor first
    [00:00:00.119,400] <inf> senss: sensor_later_config, then config physical sensor
    [00:00:00.119,400] <inf> senss: arbitrate_interval, sensor:base-accel, interval:100000, min_ri:100000, next_exec_time:-1
    [00:00:00.119,400] <inf> senss: set_arbitrate_interval, interval:100000, next_exec_time:0
    [00:00:00.119,400] <inf> phy_acc: base-accel: set report interval 100000 us
    [00:00:00.119,400] <inf> bosch_bmi160: read 40 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160:        = 8
    [00:00:00.119,400] <inf> bosch_bmi160: write 40 = 5
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> phy_acc: base-accel: Set sampling frequency 10 for accelerometer.
    [00:00:00.119,400] <inf> senss: arbitrate_sensivitity, min_sensitivity:-1
    [00:00:00.119,400] <inf> senss: config_sensitivity, sensor:base-accel, index:0, sensitivity:-1
    [00:00:00.119,400] <inf> senss: sensitivity is not set by any client, ignore
    [00:00:00.119,400] <wrn> senss: sensor:base-accel config sensitivity index:0 error
    [00:00:00.119,400] <inf> senss: sensor:base-accel need to execute, next_exec_time:0, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:base-accel first time exe, cur time:119400, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel need to execute, next_exec_time:-1, mode:1, interval:-1
    [00:00:00.119,400] <inf> senss: sensor lid-accel not in polling mode:1 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:motion-detector need to execute, next_exec_time:-1, mode:0, interval:-1
    [00:00:00.119,400] <inf> senss: sensor motion-detector not in polling mode:0 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:hinge-angle need to execute, next_exec_time:-1, mode:0, interval:-1
    [00:00:00.119,400] <inf> senss: sensor hinge-angle not in polling mode:0 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: calc_sleep_time, next:219400, cur:119400, sleep_time:100(ms)
    [00:00:00.119,400] <inf> senss: allocate_sensor, conns_num:1
    [00:00:00.119,400] <inf> senss: open_sensor_successfully, sensor:lid-accel, state:0x2, conn_index:4
    [00:00:00.119,400] <inf> senss: senss_set_interval, dynamic connection:1, sensor:lid-accel, interval:100000
    [00:00:00.119,400] <inf> senss: set_interval, conn:4, sensor:lid-accel, dynamic_connection:1, interval:100000
    [00:00:00.119,400] <inf> senss: set_reporeter_interval, sensor:lid-accel, mode:1, dynamic connection:1, interval:100000
    [00:00:00.119,400] <inf> senss: save_config_and_notify, sensor:lid-accel, append sensor to cfg_list
    [00:00:00.119,400] <inf> senss: senss_runtime_thread, config_ready
    [00:00:00.119,400] <inf> senss: sensor_later_config, config virtual sensor first
    [00:00:00.119,400] <inf> senss: sensor_later_config, then config physical sensor
    [00:00:00.119,400] <inf> senss: arbitrate_interval, sensor:lid-accel, interval:100000, min_ri:100000, next_exec_time:-1
    [00:00:00.119,400] <inf> senss: set_arbitrate_interval, interval:100000, next_exec_time:0
    [00:00:00.119,400] <inf> phy_acc: lid-accel: set report interval 100000 us
    [00:00:00.119,400] <inf> bosch_bmi160: read 40 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160:        = 8
    [00:00:00.119,400] <inf> bosch_bmi160: write 40 = 5
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> phy_acc: lid-accel: Set sampling frequency 10 for accelerometer.
    [00:00:00.119,400] <inf> senss: arbitrate_sensivitity, min_sensitivity:-1
    [00:00:00.119,400] <inf> senss: config_sensitivity, sensor:lid-accel, index:0, sensitivity:-1
    [00:00:00.119,400] <inf> senss: sensitivity is not set by any client, ignore
    [00:00:00.119,400] <wrn> senss: sensor:lid-accel config sensitivity index:0 error
    [00:00:00.119,400] <inf> senss: sensor:base-accel need to execute, next_exec_time:219400, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel need to execute, next_exec_time:0, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel first time exe, cur time:119400, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:motion-detector need to execute, next_exec_time:-1, mode:0, interval:-1
    [00:00:00.119,400] <inf> senss: sensor motion-detector not in polling mode:0 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:hinge-angle need to execute, next_exec_time:-1, mode:0, interval:-1
    [00:00:00.119,400] <inf> senss: sensor hinge-angle not in polling mode:0 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: calc_sleep_time, next:219400, cur:119400, sleep_time:100(ms)
    [00:00:00.119,400] <inf> senss: allocate_sensor, conns_num:1
    [00:00:00.119,400] <inf> senss: open_sensor_successfully, sensor:motion-detector, state:0x2, conn_index:5
    [00:00:00.119,400] <inf> senss: senss_get_sensor_info, conn:0xf6101cf4
    [00:00:00.119,400] <inf> senss: senss_set_interval, dynamic connection:1, sensor:motion-detector, interval:100000
    [00:00:00.119,400] <inf> senss: set_interval, conn:5, sensor:motion-detector, dynamic_connection:1, interval:100000
    [00:00:00.119,400] <inf> senss: set_reporeter_interval, sensor:motion-detector, mode:0, dynamic connection:1, interval:100000
    [00:00:00.119,400] <inf> senss: save_config_and_notify, sensor:motion-detector, append sensor to cfg_list
    [00:00:00.119,400] <inf> senss: senss_runtime_thread, config_ready
    [00:00:00.119,400] <inf> senss: sensor_later_config, config virtual sensor first
    [00:00:00.119,400] <inf> senss: arbitrate_interval, sensor:motion-detector, interval:100000, min_ri:100000, next_exec_time:-1
    [00:00:00.119,400] <inf> senss: set_arbitrate_interval, interval:100000, next_exec_time:0
    [00:00:00.119,400] <inf> motion_detector: [md_set_interval] name: motion-detector, value:100000
    [00:00:00.119,400] <inf> senss: senss_set_interval, dynamic connection:0, sensor:lid-accel, interval:100000
    [00:00:00.119,400] <inf> senss: set_interval, conn:0, sensor:lid-accel, dynamic_connection:0, interval:100000
    [00:00:00.119,400] <inf> senss: set_reporeter_interval, sensor:lid-accel, mode:1, dynamic connection:0, interval:100000
    [00:00:00.119,400] <inf> senss: save_config_and_notify, sensor:lid-accel, append sensor to cfg_list
    [00:00:00.119,400] <inf> senss: arbitrate_sensivitity, min_sensitivity:-1
    [00:00:00.119,400] <inf> senss: config_sensitivity, sensor:motion-detector, index:0, sensitivity:-1
    [00:00:00.119,400] <inf> senss: sensitivity is not set by any client, ignore
    [00:00:00.119,400] <wrn> senss: sensor:motion-detector config sensitivity index:0 error
    [00:00:00.119,400] <inf> senss: sensor_later_config, then config physical sensor
    [00:00:00.119,400] <inf> senss: arbitrate_interval, sensor:lid-accel, interval:100000, min_ri:100000, next_exec_time:219400
    [00:00:00.119,400] <inf> senss: set_arbitrate_interval, interval:100000, next_exec_time:0
    [00:00:00.119,400] <inf> phy_acc: lid-accel: set report interval 100000 us
    [00:00:00.119,400] <inf> bosch_bmi160: read 40 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160:        = 5
    [00:00:00.119,400] <inf> bosch_bmi160: write 40 = 5
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> phy_acc: lid-accel: Set sampling frequency 10 for accelerometer.
    [00:00:00.119,400] <inf> senss: arbitrate_sensivitity, min_sensitivity:-1
    [00:00:00.119,400] <inf> senss: config_sensitivity, sensor:lid-accel, index:0, sensitivity:-1
    [00:00:00.119,400] <inf> senss: sensitivity is not set by any client, ignore
    [00:00:00.119,400] <wrn> senss: sensor:lid-accel config sensitivity index:0 error
    [00:00:00.119,400] <inf> senss: sensor:base-accel need to execute, next_exec_time:219400, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel need to execute, next_exec_time:0, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel first time exe, cur time:119400, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:motion-detector need to execute, next_exec_time:0, mode:0, interval:100000
    [00:00:00.119,400] <inf> senss: sensor motion-detector not in polling mode:0 or sensor interval:100000 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:hinge-angle need to execute, next_exec_time:-1, mode:0, interval:-1
    [00:00:00.119,400] <inf> senss: sensor hinge-angle not in polling mode:0 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: calc_sleep_time, next:219400, cur:119400, sleep_time:100(ms)
    [00:00:00.119,400] <inf> senss: senss_runtime_thread, config_ready
    [00:00:00.119,400] <inf> senss: sensor:base-accel need to execute, next_exec_time:219400, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel need to execute, next_exec_time:219400, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:motion-detector need to execute, next_exec_time:-1, mode:0, interval:100000
    [00:00:00.119,400] <inf> senss: sensor motion-detector not in polling mode:0 or sensor interval:100000 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:hinge-angle need to execute, next_exec_time:-1, mode:0, interval:-1
    [00:00:00.119,400] <inf> senss: sensor hinge-angle not in polling mode:0 or sensor interval:-1 not opened yet
    [00:00:00.119,400] <inf> senss: calc_sleep_time, next:219400, cur:119400, sleep_time:100(ms)
    [00:00:00.119,400] <inf> senss: allocate_sensor, conns_num:1
    [00:00:00.119,400] <inf> senss: open_sensor_successfully, sensor:hinge-angle, state:0x2, conn_index:6
    [00:00:00.119,400] <inf> senss: senss_get_sensor_info, conn:0xf6101dd4
    [00:00:00.119,400] <inf> senss: senss_set_interval, dynamic connection:1, sensor:hinge-angle, interval:100000
    [00:00:00.119,400] <inf> senss: set_interval, conn:6, sensor:hinge-angle, dynamic_connection:1, interval:100000
    [00:00:00.119,400] <inf> senss: set_reporeter_interval, sensor:hinge-angle, mode:0, dynamic connection:1, interval:100000
    [00:00:00.119,400] <inf> senss: save_config_and_notify, sensor:hinge-angle, append sensor to cfg_list
    [00:00:00.119,400] <inf> senss: senss_runtime_thread, config_ready
    [00:00:00.119,400] <inf> senss: sensor_later_config, config virtual sensor first
    [00:00:00.119,400] <inf> senss: arbitrate_interval, sensor:hinge-angle, interval:100000, min_ri:100000, next_exec_time:-1
    [00:00:00.119,400] <inf> senss: set_arbitrate_interval, interval:100000, next_exec_time:0
    [00:00:00.119,400] <inf> hinge_angle: [hinge_set_interval] name: hinge-angle, value:100000
    [00:00:00.119,400] <inf> senss: senss_set_interval, dynamic connection:0, sensor:base-accel, interval:100000
    [00:00:00.119,400] <inf> senss: set_interval, conn:1, sensor:base-accel, dynamic_connection:0, interval:100000
    [00:00:00.119,400] <inf> senss: set_reporeter_interval, sensor:base-accel, mode:1, dynamic connection:0, interval:100000
    [00:00:00.119,400] <inf> senss: save_config_and_notify, sensor:base-accel, append sensor to cfg_list
    [00:00:00.119,400] <inf> senss: senss_set_interval, dynamic connection:0, sensor:lid-accel, interval:100000
    [00:00:00.119,400] <inf> senss: set_interval, conn:2, sensor:lid-accel, dynamic_connection:0, interval:100000
    [00:00:00.119,400] <inf> senss: set_reporeter_interval, sensor:lid-accel, mode:1, dynamic connection:0, interval:100000
    [00:00:00.119,400] <inf> senss: save_config_and_notify, sensor:lid-accel, append sensor to cfg_list
    [00:00:00.119,400] <inf> senss: arbitrate_sensivitity, min_sensitivity:-1
    [00:00:00.119,400] <inf> senss: config_sensitivity, sensor:hinge-angle, index:0, sensitivity:-1
    [00:00:00.119,400] <inf> senss: sensitivity is not set by any client, ignore
    [00:00:00.119,400] <wrn> senss: sensor:hinge-angle config sensitivity index:0 error
    [00:00:00.119,400] <inf> senss: sensor_later_config, then config physical sensor
    [00:00:00.119,400] <inf> senss: arbitrate_interval, sensor:base-accel, interval:100000, min_ri:100000, next_exec_time:219400
    [00:00:00.119,400] <inf> senss: set_arbitrate_interval, interval:100000, next_exec_time:0
    [00:00:00.119,400] <inf> phy_acc: base-accel: set report interval 100000 us
    [00:00:00.119,400] <inf> bosch_bmi160: read 40 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160:        = 5
    [00:00:00.119,400] <inf> bosch_bmi160: write 40 = 5
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> phy_acc: base-accel: Set sampling frequency 10 for accelerometer.
    [00:00:00.119,400] <inf> senss: arbitrate_sensivitity, min_sensitivity:-1
    [00:00:00.119,400] <inf> senss: config_sensitivity, sensor:base-accel, index:0, sensitivity:-1
    [00:00:00.119,400] <inf> senss: sensitivity is not set by any client, ignore
    [00:00:00.119,400] <wrn> senss: sensor:base-accel config sensitivity index:0 error
    [00:00:00.119,400] <inf> senss: arbitrate_interval, sensor:lid-accel, interval:100000, min_ri:100000, next_exec_time:219400
    [00:00:00.119,400] <inf> senss: set_arbitrate_interval, interval:100000, next_exec_time:0
    [00:00:00.119,400] <inf> phy_acc: lid-accel: set report interval 100000 us
    [00:00:00.119,400] <inf> bosch_bmi160: read 40 =
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> bosch_bmi160:        = 5
    [00:00:00.119,400] <inf> bosch_bmi160: write 40 = 5
    [00:00:00.119,400] <inf> bosch_bmi160:    * acc conf
    [00:00:00.119,400] <inf> phy_acc: lid-accel: Set sampling frequency 10 for accelerometer.
    [00:00:00.119,400] <inf> senss: arbitrate_sensivitity, min_sensitivity:-1
    [00:00:00.119,400] <inf> senss: config_sensitivity, sensor:lid-accel, index:0, sensitivity:-1
    [00:00:00.119,400] <inf> senss: sensitivity is not set by any client, ignore
    [00:00:00.119,400] <wrn> senss: sensor:lid-accel config sensitivity index:0 error
    [00:00:00.119,400] <inf> senss: sensor:base-accel need to execute, next_exec_time:0, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:base-accel first time exe, cur time:119400, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel need to execute, next_exec_time:0, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel first time exe, cur time:119400, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:motion-detector need to execute, next_exec_time:-1, mode:0, interval:100000
    [00:00:00.119,400] <inf> senss: sensor motion-detector not in polling mode:0 or sensor interval:100000 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:hinge-angle need to execute, next_exec_time:0, mode:0, interval:100000
    [00:00:00.119,400] <inf> senss: sensor hinge-angle not in polling mode:0 or sensor interval:100000 not opened yet
    [00:00:00.119,400] <inf> senss: calc_sleep_time, next:219400, cur:119400, sleep_time:100(ms)
    [00:00:00.119,400] <inf> senss: senss_runtime_thread, config_ready
    [00:00:00.119,400] <inf> senss: sensor:base-accel need to execute, next_exec_time:219400, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:lid-accel need to execute, next_exec_time:219400, mode:1, interval:100000
    [00:00:00.119,400] <inf> senss: sensor:motion-detector need to execute, next_exec_time:-1, mode:0, interval:100000
    [00:00:00.119,400] <inf> senss: sensor motion-detector not in polling mode:0 or sensor interval:100000 not opened yet
    [00:00:00.119,400] <inf> senss: sensor:hinge-angle need to execute, next_exec_time:-1, mode:0, interval:100000
    [00:00:00.119,400] <inf> senss: sensor hinge-angle not in polling mode:0 or sensor interval:100000 not opened yet
    [00:00:00.119,400] <inf> senss: calc_sleep_time, next:219400, cur:119400, sleep_time:100(ms)
    [00:00:00.220,000] <inf> senss: sensor:base-accel need to execute, next_exec_time:219400, mode:1, interval:100000
    [00:00:00.220,000] <inf> bosch_bmi160: read 1b =
    [00:00:00.220,000] <inf> bosch_bmi160:    * status
    [00:00:00.220,000] <inf> bosch_bmi160:        = 40
    [00:00:00.220,000] <inf> bosch_bmi160: Sample read
    [00:00:00.220,000] <inf> senss: sensor:lid-accel need to execute, next_exec_time:219400, mode:1, interval:100000
    [00:00:00.220,000] <inf> bosch_bmi160: read 1b =
    [00:00:00.220,000] <inf> bosch_bmi160:    * status
    [00:00:00.220,000] <inf> bosch_bmi160:        = 40
    [00:00:00.220,000] <inf> bosch_bmi160: Sample read
    [00:00:00.220,000] <inf> senss: sensor:motion-detector need to execute, next_exec_time:0, mode:1, interval:100000
    [00:00:00.220,000] <inf> senss: sensor:motion-detector first time exe, cur time:220000, interval:100000
    [00:00:00.220,000] <inf> senss: sensor:hinge-angle need to execute, next_exec_time:0, mode:1, interval:100000
    [00:00:00.220,000] <inf> senss: sensor:hinge-angle first time exe, cur time:220000, interval:100000
    [00:00:00.220,000] <inf> hinge_angle_algo: [hinge_angle_algo_process] value first -1, base 0 102 203 lid 0 102 203
    [00:00:00.220,000] <inf> senss: senss_sensor_post_data, sensor:hinge-angle, data_size:20
    [00:00:00.220,000] <inf> senss: calc_sleep_time, next:319400, cur:220000, sleep_time:99(ms)
    [00:00:00.220,000] <inf> senss: senss_get_sensor_info, conn:0xf6101b34
    [00:00:00.220,000] <inf> main: Sensor base-accel data:	 x: 60, y: 102017, z: 203974
    [00:00:00.220,000] <inf> senss: senss_get_sensor_info, conn:0xf6101c14
    [00:00:00.220,000] <inf> main: Sensor lid-accel data:	 x: 60, y: 102017, z: 203974
    [00:00:00.220,000] <inf> senss: senss_get_sensor_info, conn:0xf6101cf4
    [00:00:00.220,000] <inf> main: Sensor motion-detector data:	 v: 0
    [00:00:00.220,000] <inf> senss: senss_get_sensor_info, conn:0xf6101dd4
    [00:00:00.220,000] <inf> main: Sensor hinge-angle data:	 v: -1

Exit by pressing :kbd:`CTRL+C`.
