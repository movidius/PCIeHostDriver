VPU Host Drivers
================

NOTE: These drivers have only been tested on Ubuntu 16.04 LTS and only build
      when using Linux kernels v4.15 or above.

For host applications to communicate with the Myriad X PCIe endpoint, the host
requires a Linux PCIe based driver. Under the host/ folder, two Linux kernel
drivers exist:
  - The Myriad X Vision Processor (MXVP) driver exposes the interprocessor
    communication protocol between the host and Myriad X and well as helper
    functions to aid in the creation/sending of commands and accesses to
    PCIe MMIO regions. This driver is not a standalone driver and is intended
    to be built into other drivers.
  - The Myriad X Test Framework (MXTF) driver shows example usages of the MXVP
    driver's functionality. Users execute basic functionality tests from the
    command line and see the results displayed in dmesg.

Each driver folder contains a Makefile:
  - clean: make clean
  - build: make all
  - help : make help

Insert drivers: sudo insmod <driver_name>.ko
Remove drivers: sudo rmmod  <driver_name>

To run MXTF tests:
  - build  driver: make clean all
  - insert driver: sudo insmod mxtf.ko
  - display tests: cat /sys/bus/pci/devices/DDDD:BB:DD.F/test
  - execute tests: sudo bash -c "echo <test_num> > /sys/bus/pci/devices/DDDD:BB:DD.F/test"
  - check results: dmesg

NOTE: The DMA Throughput test runs in a infinite loop. To end the throughput
      test, the user must reboot the system and repeat the steps to load the
      driver.
