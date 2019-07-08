/*******************************************************************************
 *
 * MX device reset API
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef COMMON_MX_RESET_H_
#define COMMON_MX_RESET_H_

#include <linux/pci.h>

#include "mx_common.h"

/*
 * @brief Reset MX device
 *
 * NOTES:
 *  1. This process must be executed under device lock. It is the caller's
 *     responsibility to ensure the device is locked before calling this
 *     function.
 *  2. This function can be called directly from kernel drivers. User land
 *     applications can perform MX reset using the sysfs entry created by
 *     mx_reset_sysfs_init().
 *
 * @param[in] pci - pointer to pci device instance.
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mx_reset_device(struct mx_dev *mx_dev);

#endif /* COMMON_MX_RESET_H_ */
