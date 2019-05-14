/*******************************************************************************
 *
 * Intel Myriad-X Test Framework Driver: General API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXTF_HEADER_
#define MXTF_HEADER_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>

#include "mxvp.h"

#define MXTF_DRIVER_NAME "mxtf"
#define MXTF_DRIVER_DESC "Intel(R) Myriad X Test Framework"

struct mxtf {
    struct mxvp mxvp;
    struct task_struct *task;
    struct device_attribute test;
};

#endif
