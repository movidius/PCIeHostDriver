mxbl - Linux host PCIe Boot driver

Supported Platform
==================

Linux - This driver has only been tested on Ubuntu 16.04 LTS and only builds
        when using Linux kernel v4.15 or above.

Overview
========

This is the PCIe Boot driver for Linux hosts. This driver allows the host
platform to communicate with the Myriad X device using the protocol designed to
transfer Myriad X application binaries over PCIe before booting.

Limitation
==========

This driver only allows the loading of Myriad X application binaries that are
smaller than 4 MB.

Driver build
============

The mxbl driver provides two different methods to load the Myriad X application:
  - Method 1: The mxbl driver creates, upon insertion, a character device a user
    application can write the Myriad X application binary into, in order to
    start the boot process.
  - Method 2: The mxbl driver turns the Myriad X application binary into an
    array that is embedded in the driver's binary upon compilation. In this case
    the Myriad X application binary is loaded automatically upon insertion of
    the mxbl driver.

The choice between methods 1 and 2 is done at build time, as detailed below.

NOTE: The user will need the kernel-devel packages installed on the Linux host
      to be able to build the mxbl driver.

To setup method 1, build the driver using the following command:
    "make all"

To setup method 2, build the driver using the following command:
    "make all BINARY=mx_app_bin_path"
Where "mx_app_bin_path" is the path to the Myriad X application binary.

NOTE: The Myriad X application binary must be in MVCMD binary format.

To clean the driver object files, use the following command:
    "make clean"

Driver loading/unloading
========================

The driver can then be inserted using the following command:
    "sudo insmod mxbl.ko"

The driver can be removed using the following command:
    "sudo rmmod mxbl"

NOTE: Removing the driver once the loading of the Myriad X application has been
performed successfully is necessary if another host PCIe driver must be inserted
to handle the communication with the Myriad X device running under the
application just loaded.

The debug output of the driver can be accessed using the dmesg command.
