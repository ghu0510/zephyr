.. _senss-sample-chre:

Sensor subsystem sample with chre
#######################

Overview
********

A sample that shows how to run senss with chre. It defines four sensors, with
the underlying device bmi160 emulator, and get the sensor data in defined
interval.

The program runs in the following sequence:

#. Define the sensor in the dts

#. run senss

#. run chre

#. start chre sensor nanoapp

#. Run forever and get the sensor data.

Building and Running
********************

This application can be built and executed on native_posix as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/senss/chre
   :host-os: unix
   :board: native_posix
   :goals: run
   :compact:

To build for another board, change "native_posix" above to that board's name.
At the current stage, it only support native posix

Sample Output
=============

.. code-block:: console

    [00:00:00.119,400] <inf> chre: senss run successfully
    [00:00:00.119,400] <inf> chre: CHRE init, version: undefined
    [00:00:00.119,400] <inf> chre: PAL: PalOpen: senssCnt 4 chreCnt 4
    [00:00:00.119,400] <inf> chre: PAL: PalOpen: num 0 name base-accel type 1 index 0 minInterval 625
    [00:00:00.119,400] <inf> chre: PAL: PalOpen: num 1 name lid-accel type 1 index 1 minInterval 625
    [00:00:00.119,400] <inf> chre: PAL: PalOpen: num 2 name motion-detector type 2 index 0 minInterval 100000
    [00:00:00.119,400] <inf> chre: PAL: PalOpen: num 3 name hinge-angle type 36 index 0 minInterval 100000
    [00:00:00.119,400] <inf> chre: PAL: PalOpen: success, version 0x01000000
    [00:00:00.119,400] <dbg> chre: init: Opened Sensor PAL version 0x01030000
    [00:00:00.119,400] <dbg> chre: getSensors: Found sensor: base-accel
    [00:00:00.119,400] <dbg> chre: getSensors: Found sensor: lid-accel
    [00:00:00.119,400] <dbg> chre: getSensors: Found sensor: motion-detector
    [00:00:00.119,400] <dbg> chre: getSensors: Found sensor: hinge-angle
    [00:00:00.119,400] <inf> chre: EventLoop start
    [00:00:00.130,000] <inf> chre: chre run successfully
    [00:00:00.130,000] <dbg> chre: startNanoapp: Instance ID 1 assigned to app ID 0x0123456789000004
    [00:00:00.130,000] <inf> chre: Sensor 0 initialized: true with handle 0
    [00:00:00.130,000] <inf> chre: SensorInfo: base-accel, Type=1 OnChange=0 OneShot=0 Passive=0 minInterval=625nsec
    [00:00:00.130,000] <inf> chre: PAL: PalConfigureSensor: type 1 index 0 mode 3 intervalNs 80000000 latencyNs 4000000000
    [00:00:00.130,000] <inf> chre: Requested data: odr 12 Hz, latency 4 sec, success
    [00:00:00.130,000] <inf> chre: Sensor 1 initialized: true with handle 1
    [00:00:00.130,000] <inf> chre: SensorInfo: lid-accel, Type=1 OnChange=0 OneShot=0 Passive=0 minInterval=625nsec
    [00:00:00.130,000] <inf> chre: PAL: PalConfigureSensor: type 1 index 1 mode 3 intervalNs 100000000 latencyNs 4000000000
    [00:00:00.130,000] <inf> chre: Requested data: odr 10 Hz, latency 4 sec, success
    [00:00:00.130,000] <inf> chre: Sensor 2 initialized: true with handle 2
    [00:00:00.130,000] <inf> chre: SensorInfo: motion-detector, Type=2 OnChange=0 OneShot=1 Passive=0 minInterval=100000nsec
    [00:00:00.130,000] <inf> chre: Sensor 3 initialized: true with handle 3
    [00:00:00.130,000] <inf> chre: SensorInfo: hinge-angle, Type=36 OnChange=1 OneShot=0 Passive=0 minInterval=100000nsec
    [00:00:00.130,000] <inf> chre: PAL: PalConfigureSensor: type 36 index 0 mode 3 intervalNs 100000000 latencyNs 0
    [00:00:00.130,000] <inf> chre: Requested data: odr 10 Hz, latency 0 sec, success
    [00:00:00.130,000] <inf> chre: Sensor 4 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 5 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 6 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 7 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 8 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 9 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 10 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 11 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 12 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 13 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 14 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 15 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 16 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: Sensor 17 initialized: false with handle 0
    [00:00:00.130,000] <inf> chre: sensor app start successfully
    [00:00:00.130,000] <inf> chre: Sampling Change: handle 0, status: interval 80000000 latency 4000000000 enabled 1
    [00:00:00.130,000] <inf> chre: Sampling Change: handle 1, status: interval 100000000 latency 4000000000 enabled 1
    [00:00:00.130,000] <inf> chre: Sampling Change: handle 3, status: interval 100000000 latency 0 enabled 1
    [00:00:00.220,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback info: type 1 index 0 size 28
    [00:00:00.220,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback data: type 1 index 0 readings[0] 60 102017 203974
    [00:00:00.220,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback info: type 36 index 0 size 20
    [00:00:00.220,000] <err> chre: PAL: DataEventCallback failed: type 36 index 0, reading_count 0 wrong
    [00:00:00.220,000] <err> chre: PAL: DataEventCallback failed: type 36 index 0 size 20, ret -22
    [00:00:00.220,000] <inf> chre: base-accel, 1 samples: 60 102017 203974, accuracy: 0, t=0 ms
    [00:00:00.240,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback info: type 1 index 1 size 28
    [00:00:00.240,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback data: type 1 index 1 readings[0] 60 102017 203974
    [00:00:00.240,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback info: type 36 index 0 size 20
    [00:00:00.240,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback data: type 36 index 0 readings[0] -1
    [00:00:00.240,000] <inf> chre: lid-accel, 1 samples: 60 102017 203974, accuracy: 0, t=0 ms
    [00:00:00.240,000] <inf> chre: hinge-angle, 1 samples: -1, accuracy = 0, t=0 ms
    [00:00:00.300,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback info: type 1 index 0 size 28
    [00:00:00.300,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback data: type 1 index 0 readings[0] 60 102017 203974
    [00:00:00.300,000] <inf> chre: base-accel, 1 samples: 60 102017 203974, accuracy: 0, t=0 ms
    [00:00:00.380,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback info: type 1 index 0 size 28
    [00:00:00.380,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback data: type 1 index 0 readings[0] 60 102017 203974
    [00:00:00.380,000] <inf> chre: base-accel, 1 samples: 60 102017 203974, accuracy: 0, t=0 ms
    [00:00:00.430,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback info: type 1 index 1 size 28
    [00:00:00.430,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback data: type 1 index 1 readings[0] 60 102017 203974
    [00:00:00.430,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback info: type 36 index 0 size 20
    [00:00:00.430,000] <dbg> chre: palSystemApiLog: PAL: DataEventCallback data: type 36 index 0 readings[0] -1
    [00:00:00.430,000] <inf> chre: lid-accel, 1 samples: 60 102017 203974, accuracy: 0, t=0 ms
    [00:00:00.430,000] <inf> chre: hinge-angle, 1 samples: -1, accuracy = 0, t=0 ms

Exit by pressing :kbd:`CTRL+C`.
