.. _senss_test:

Sensor Subsystem Test Sample
############################

Overview
********

This test sample is used to test the senss(Sensor Subsystem) APIs. It bases on
bmi160 emulator on native_posix platform.

Building and Testing
********************

This application can be built and executed on native_posix as follows:

.. code-block:: console

   $ ./scripts/twister -p native_posix -T tests/subsys/senss

To build for another board, change "native_posix" above to that board's name.
At the current stage, it only supports native_posix.
