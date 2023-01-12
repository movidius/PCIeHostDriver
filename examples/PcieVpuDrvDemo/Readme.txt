PcieVpuDrvDemo

NOTE: This Readme file only gives information about the Myriad X application.
      For description and instructions regarding the host drivers, see
      host/Readme.txt

Supported Platform
==================

Myriad X - This application is for Myriad X architecture.

Overview
========

This aplication is a basic example demonstrating how the PCIe VPU driver is
supposed to be used and allowing data transfers and commands/replies exchange
between the host platform and the Myriad X device.

This application provides an example host PCIe VPU driver (Linux only) and a
test driver that performs data transfers and commands/replies exchanges.

Software description
====================

The Myriad X application runs on Leon CSS and go through the following steps:
  1. Initialisation of the PCIe VPU driver. Two callbacks are passed to the
     driver:
       - dummy_disp_cb: Triggered upon reception of a new buffer to execute.
         Saves the buffer's size and address into global variables and set a
         flag to interrupt the main loop when a new buffer has been received.
       - dummy_action_cb: Triggered upon reception of a new action to perform.
         Does nothing.
  In an infinite loop:
  2. When a new buffer is received, tell the PCIe VPU driver that the command
     has been processed successfully, using the buffer's size and address as the
     reply's payload.
  If existing the infinite loop for any reason (should not happen in normal test
  runs):
  3. Exit the application.

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
