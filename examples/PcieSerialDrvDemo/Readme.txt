PcieSerialDrvDemo

NOTE: This Readme file only gives information about the Myriad X application.
      For description and instructions regarding the host driver and demo
      application, see host/Readme.txt

Supported Platform
==================

Myriad X - This application is for Myriad X architecture.

Overview
========

This aplication is a basic example demonstrating how the PCIe Serial driver is
supposed to be used and allowing data transfers between the host platform and
the Myriad X device.

This application provides a preliminary host PCIe driver and a basic host test
application (Linux only) that are needed to exercise the data transfer feature.

Software description
====================

The Myriad X application runs on Leon CSS and go through the following steps:
  1. General initialisation of the Myriad X chip.
  2. Initialise the PCIe driver.
  3. Create the read/write/throughput calculator tasks.
  4. Wait for the host to be up.
  5. Open the PCIe driver.
  6. Start the read/write/throughput calculator tasks
  7. Infinite loop while tasks are running.
  8. Close the PCIe driver.
  9. Return to step 4.
  Unreachable in in normal test runs:
  10. Exit the application.

Read task:
  While no error detected:
  1. Read a chunk of data.
  2. If error, exit the loop.
  3. Check sequence number in received data.
  4. If error in sequence number, report the error and exit the loop.
  5. Increment number of bytes read.
  Out of the while no error loop:
  6. Report the error (that will kill the other tasks as well).
  7. Exit the task.

Write task:
  While no error detected:
  1. Increment the sequence number of the chunk of data to be sent.
  2. Write a chunk of data.
  3. If error, exit the loop.
  4. Increment number of bytes written.
  Out of the while no error loop:
  5. Report the error (that will kill the other tasks as well).
  6. Exit the task.

Throughput calculator task:
  While no error detected:
  1. Sleep for a given amount of time.
  2. Compute TX/RX throughputs from number of bytes written/read since the last
     iteration of the loop.
  3. Reset number of bytes read/written.
  4. Print the throughput values and the number of sequence number errors
     reported. This print is not accessible in normal runs as application is
     running from flash.
  Out of the while no error loop:
  5. Exit the task.

Build
=====

This project was migrated to the Next Generation build system. You may still use
the legacy build system, but the Next Generation build system is now the
recommended way to build and maintain your project. Please see the sections
below for the build steps corresponding to your choice.

Build using the Next Generation build system
============================================

The Next Generation build system main makefile is named `newMakefile.mk`. Please
use the `-f newMakefile.mk` upon make invocation so that you kick-in the Next
Generation build system for all the targets you may need and not mentioned in
this section. Alternatively, please consider using a SHELL alias in order to
reduce typos.

Please type "make -f newMakefile.mk help" if you want to learn available targets.

Prepare the Next Generation build system:

    $ make -f newMakefile.mk prepare-kconfig

In case of errors, please refer to the log file mentioned on that command's
output. Usually, errors occur when some pre-requisites are not installed.

To inspect the settings and eventually adjust them:

    $ make -f newMakefile.mk [menuconfig|nconfig|qconfig] MV_SOC_REV={Myriad_version}

To build the application:

    $ make -f newMakefile.mk -j4 MV_SOC_REV={Myriad_version}

To clean the build artefacts:

    $ make -f newMakefile.mk clean MV_SOC_REV={Myriad_version}

To completely remove the build artifacts and directories:

    $ rm -rf mvbuild

NOTE:{Myriad_version} can be ma2480 or ma2085.

Build using the legacy build system
===================================

Please type "make help" if you want to learn available targets.
Before cross-compiling make sure you do a "make clean"!

To build the project, please type:
    $ make clean
    $ make all

Setup and running
=================

This application must be flashed and booted through the SPI boot infrastructure
for PCIe applications.

See details and instructions in mdk/common/utils/pcie_app_spi_boot/Readme.txt

Expected output
===============

The debug ouput of the Myriad X application will not be accessible to the user
as the application will be booting from flash.
