/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: ASYNC CMDs API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXVP_ASYNC_HEADER_
#define MXVP_ASYNC_HEADER_

#include <linux/kernel.h>

/*
 * Myriad-X Asynchronous command definitions
 */
enum mxvp_async_cmd {
    MXVP_ASYNC_CMDQ_PREEMPT = 0,
    MXVP_ASYNC_DMAQ_PREEMPT,
    MXVP_ASYNC_CMDQ_RESET,
    MXVP_ASYNC_DMAQ_RESET,
    MXVP_ASYNC_DEV_RESET,
    MXVP_ASYNC_SET_POWER,
    MXVP_ASYNC_MAX,
};

/*
 * MXVP_ASYNC_SET_POWER command arguments
 */
typedef u8 mxvp_async_pwr_arg;
#define MXVP_ASYNC_PWR_D0_ENTRY (0)
#define MXVP_ASYNC_PWR_D1_ENTRY (1)
#define MXVP_ASYNC_PWR_D2_ENTRY (2)
#define MXVP_ASYNC_PWR_D3_ENTRY (3)

#endif
