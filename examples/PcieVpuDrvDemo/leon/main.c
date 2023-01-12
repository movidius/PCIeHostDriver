///
/// @file      main.c
/// @copyright All code copyright Intel Corporation 2018, all rights reserved.
///            For License Warranty see: common/license.txt
///
/// @brief     PCIe VPU driver demo application: LeonCSS main source file.
///
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <bsp.h>
#include <rtems.h>

#include "OsDrvCpr.h"
#include "OsDrvInit.h"
#include "OsDrvPcieVpu.h"

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

// Size of the exposed DDR space, in number of 4KB pages (448MB).
#define VPU_DDR_SIZE_4KB_PAGES 114688

// Base address of the exposed DDR space. Must be aligned on a 16KB boundary.
#define VPU_DDR_BASE 0x84000000

// Execute buffer reply's payload size (in 32-bit words).
#define EXEC_BUF_REPLY_PAYLOAD_SIZE_WD 2

static volatile uint32_t dispatcher_done = 0;
static uint64_t buffer_address = 0;
static uint64_t buffer_size = 0;
static uint32_t exec_buf_txn_id = 0;

static void dummy_disp_cb(void *arg, uint32_t txn_id,
                          uint32_t buf_addr, uint32_t buf_size) {
    UNUSED(arg);

    buffer_address = buf_addr;
    buffer_size = buf_size;
    exec_buf_txn_id = txn_id;

    dispatcher_done = 1;
    return;
}

static void dummy_disp_action_cb(void *arg, OsDrvPcieVpuDispAction action) {
    UNUSED(arg);
    UNUSED(action);
    return;
}

static void dummy_sys_action_cb(void *arg, OsDrvPcieVpuSysAction action,
                                uint8_t action_arg) {
    UNUSED(arg);
    UNUSED(action);
    UNUSED(action_arg);
    return;
}

void *POSIX_Init(void *args) {
    UNUSED(args);

    OsDrvPcieVpuCbParam cb;
    cb.disp_cb = dummy_disp_cb;
    cb.disp_arg = NULL;
    cb.disp_action_cb = dummy_disp_action_cb;
    cb.disp_action_arg = NULL;
    cb.sys_action_cb = dummy_sys_action_cb;
    cb.sys_action_arg = NULL;
    OsDrvPcieVpuInit(VPU_DDR_BASE, VPU_DDR_SIZE_4KB_PAGES, &cb);

    while (1) {
        if (dispatcher_done) {
            uint64_t arg = (buffer_address) | (buffer_size << 32);
            OsDrvPcieVpuCmdDone(exec_buf_txn_id,
                                OS_DRV_PCIE_VPU_VPUAL_STATUS_SUCCESS,
                                (uint32_t *)&arg,
                                EXEC_BUF_REPLY_PAYLOAD_SIZE_WD);
            dispatcher_done = 0;
        }
    }

    exit(0);
}
