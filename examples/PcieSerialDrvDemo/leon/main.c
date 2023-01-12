///
/// @file      main.c
/// @copyright All code copyright Intel Corporation 2018, all rights reserved.
///            For License Warranty see: common/license.txt
///
/// @brief     PCIe Serial driver test application: LeonCSS main source file.
///
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <bsp.h>
#include <rtems.h>

#include "OsDrvCpr.h"
#include "OsDrvInit.h"
#include "OsDrvCmxDma.h"
#include "OsDrvPcieSerial.h"

// System Clock configuration on start-up.
#define DEFAULT_OSC0_KHZ 24000
#define DESIRED_SYS_CLOCK_KHZ 700000
BSP_SET_CLOCK(DEFAULT_OSC0_KHZ, DESIRED_SYS_CLOCK_KHZ, 1, 1,
              DEFAULT_RTEMS_CSS_LOS_CLOCKS | DEV_CSS_PCIE_10GE,
              0, DEFAULT_UPA_CLOCKS, 0, 0);

// PCIe auxiliary clock configuration.
OsDrvCprAuxClockConfig TestAuxClkCfg[] = {
    {OS_DRV_CPR_DEV_CSS_AUX_PCIE, OS_DRV_CPR_CLK_REF0, 1, 1},
    {OS_DRV_CPR_DEV_MAX, 0, 0, 0}
};
// Pass the auxiliary clock configuration as a parameter for CPR init called
// during platform init.
static OsDrvCprConfig TestCprCfg = {
    .ref0InClk = DEFAULT_OSC0_KHZ,
    .auxCfg = TestAuxClkCfg
};
OS_DRV_INIT_CPR_CFG_DEFINE(&TestCprCfg);

#define THPT_INTERVAL (1)
#define BUFLEN (32 * 1024)

static uint32_t writeBuffer[BUFLEN/sizeof(uint32_t)] __attribute__((aligned(64)));
static uint32_t readBuffer[BUFLEN/sizeof(uint32_t)]__attribute__((aligned(64)));
static volatile size_t rx_bytes = 0;
static volatile size_t tx_bytes = 0;
static volatile size_t rx_error = 0;

static volatile int rx_running;
static volatile int tx_running;
static volatile int cc_running;
static volatile int running;

static ssize_t readStream(int dev, void *buffer, size_t length) {
    void *ptr = buffer;
    size_t remaining = length;

    while (remaining) {
        ssize_t copied = OsDrvPcieSerialRead(dev, ptr, remaining, 100);
        if (copied < 0) {
            return copied;
        }
        remaining -= copied;
        ptr += copied;
    }

    return (ssize_t) (length - remaining);
}

static ssize_t writeStream(int dev, void *buffer, size_t length) {
    void *ptr = buffer;
    size_t remaining = length;

    while (remaining) {
        ssize_t copied = OsDrvPcieSerialWrite(dev, ptr, remaining, 100);
        if (copied < 0) {
            return copied;
        }
        remaining -= copied;
        ptr += copied;
    }

    return (int) (length - remaining);
}

static rtems_task reader(rtems_task_argument arg) {
    UNUSED(arg);
    uint32_t actual, expect;

    expect = 0;
    printf("staring reading thread...\n");
    rx_running = 1;
    while (running) {
        ssize_t read = readStream(0, (void *)readBuffer, BUFLEN);
        if (read < 0) {
            break;
        }
        actual = readBuffer[0];
        if (actual != expect) {
            rx_error++;
            printf("seqno mismatch a %08uX e %08uX\n", actual, expect);
            expect = actual;
        }
        expect++;
        rx_bytes += read;
    }
    running = 0;
    rx_running = 0;
}

static rtems_task writer(rtems_task_argument arg) {
    UNUSED(arg);
    u32 seqno;

    seqno = 0;
    printf("staring writing thread...\n");
    tx_running = 1;
    while (running) {
        writeBuffer[0] = seqno++;
        ssize_t written = writeStream(0, (void *)writeBuffer, BUFLEN);
        if (written < 0) {
            break;
        }
        tx_bytes += written;
    }
    running = 0;
    tx_running = 0;
}

static rtems_task calculator(rtems_task_argument arg) {
    UNUSED(arg);
    size_t rx_thpt;
    size_t tx_thpt;
    rtems_interrupt_level irqlevel;

    cc_running = 1;
    while (running) {
        sleep(THPT_INTERVAL);

        rtems_interrupt_disable(irqlevel);
        tx_thpt  = tx_bytes / THPT_INTERVAL;
        rx_thpt  = rx_bytes / THPT_INTERVAL;
        rx_bytes = 0;
        tx_bytes = 0;
        rtems_interrupt_enable(irqlevel);

        printf("rx : %u, tx : %u, er : %u\n", rx_thpt, tx_thpt, rx_error);
    }
    running = 0;
    cc_running = 0;
}

void *POSIX_Init(void *args) {
    UNUSED(args);

    OsDrvCmxDmaInitialize(NULL);
    OsDrvPcieSerialInit();

    rtems_id rxThread;
    rtems_task_create(rtems_build_name('T', 'H', 'P', 'R'),
                                   RTEMS_MINIMUM_PRIORITY + 2,
                                   RTEMS_MINIMUM_STACK_SIZE, RTEMS_DEFAULT_MODES,
                                   RTEMS_DEFAULT_ATTRIBUTES, &rxThread);
    rtems_id txThread;
    rtems_task_create(rtems_build_name('T', 'H', 'P', 'W'),
                                   RTEMS_MINIMUM_PRIORITY + 2,
                                   RTEMS_MINIMUM_STACK_SIZE, RTEMS_DEFAULT_MODES,
                                   RTEMS_DEFAULT_ATTRIBUTES, &txThread);
    rtems_id clcThread;
    rtems_task_create(rtems_build_name('T', 'H', 'P', 'C'),
                                 RTEMS_MINIMUM_PRIORITY + 1,
                                 RTEMS_MINIMUM_STACK_SIZE, RTEMS_DEFAULT_MODES,
                                 RTEMS_DEFAULT_ATTRIBUTES, &clcThread);

restart:
    while (!OsDrvPcieSerialHostIsUp())
        continue;
    OsDrvPcieSerialOpen(0);

    running = 1;
    rtems_task_start(rxThread, reader, 0);
    rtems_task_start(txThread, writer, 0);
    rtems_task_start(clcThread, calculator, 0);

    while (rx_running || tx_running || cc_running)
        continue;

    OsDrvPcieSerialClose(0);
    goto restart;

    exit(0);
}
