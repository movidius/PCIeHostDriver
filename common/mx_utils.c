/*******************************************************************************
 *
 * MX device utility functions
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <linux/string.h>

#include "mx_common.h"
#include "mx_mmio.h"

/* Functions defined in this file are exported through mx_common.h */

enum mx_opmode mx_get_opmode(struct mx_dev *mx_dev)
{
    int error;
    u8 main_magic[MX_MM_LEN] = {0};

    mx_rd_buf(mx_dev->mmio, MX_MAIN_MAGIC, main_magic, MX_MM_LEN);

    error = memcmp(main_magic, MX_MM_BOOT_STR, strlen(MX_MM_BOOT_STR));
    if (!error) {
        return MX_OPMODE_BOOT;
    }

    error = memcmp(main_magic, MX_MM_LOAD_STR, strlen(MX_MM_LOAD_STR));
    if (!error) {
        return MX_OPMODE_LOADER;
    }

    error = memcmp(main_magic, MX_MM_VPUAL_STR, strlen(MX_MM_VPUAL_STR));
    if (!error) {
        return MX_OPMODE_APP_VPUAL;
    }

    error = memcmp(main_magic, MX_MM_VPULINK_STR, strlen(MX_MM_VPULINK_STR));
    if (!error) {
        return MX_OPMODE_APP_VPULINK;
    }

    return MX_OPMODE_UNKNOWN;
}
