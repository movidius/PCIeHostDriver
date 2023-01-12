PcieSerialDrvDemo - Linux host demo application

Supported Platform
==================

Linux - The driver this application relies on (mxlk) has only been tested on
        Ubuntu 16.04 LTS and only builds when using Linux kernel v4.15 or above.

Software description
====================

The demo application go through the following steps:
  1. Open the PCIe driver.
  2. Create and start the read/write/throughput calculator tasks.
  3. Infinite loop.
  If existing the infinite loop for any reason (should not happen in normal test
  runs):
  4. If the tasks are still running, kill them.
  5. Close the PCIe driver.
  6. Exit the application.

Read task:
  In an infinite loop:
  1. Read a chunk of data.
  2. If error, exit the loop.
  3. Check sequence number in received data.
  4. If error in sequence number, report the error.
  5. Increment number of bytes read.
  Out of the infinite loop:
  6. Exit the task.

Write task:
  In an infinite loop:
  1. Increment the sequence number of the chunk of data to be sent.
  2. Write a chunk of data.
  3. If error, exit the loop.
  4. Increment number of bytes written.
  Out of the infinite loop:
  5. Exit the task.

Throughput calculator task:
  In an infinite loop:
  1. Sleep for a given amount of time.
  2. Compute TX/RX throughputs from number of bytes written/read since the last
     iteration of the loop.
  3. Reset number of bytes read/written.
  4. Print the throughput values and the number of sequence number errors
     reported.
  Out of the while no error loop:
  5. Exit the task.

Host PCIe Serial Driver
=======================

In order to run properly, this application needs the host PCIe Serial driver,
named "mxlk", to be built and installed on the host platform.

This driver can be found in the MDK at the following location:
    "host/linux/pcie/serial/mxlk/"

Follow the instruction in the correspponding Readme file (located in the host
driver's folder) to build and install the driver.

Demo application compilation and running
========================================

Compile the demo application binary using the following command line:
    "make all"
Then, to run the demo application:
    "./demo_app"
If you want to stop the demo application, use CTRL+C keyboard combination.

Expected output
===============

On a succesful run, demo_app application regularly (each second, by default)
prints the following trace:
    "[N] rx: R tx : T er : E"
Where:
  - N is the trace sequence number.
  - R is the RX throughput in bytes per second.
  - T is the TX throughput in bytes per second.
  - E is the number of RX sequence number error detected.

RX and TX throuhgputs have been measured to around 650MBps each on a host having
the following characteristics:
  - 4 year-old desktop PC
  - PCIe 3.0 slot
  - Intel Core I7 quad-core 3.9GHz CPU
  - 32GB RAM
  - Ubuntu 16.04.3 LTS using Linux Kernel v4.15

Given that the transmitted data has to be copied between kernel and user buffers
(either for TX or RX) on the host side, there are a couple of things that can
impact the throughput values measured on a particular PC:
  - Modifying the size and numbers of the kernel buffers or the size of the data
    chunks sent by the application could result in smaller values.
  - Running this application on an already loaded or less powerful PC could
    result in smaller values.
