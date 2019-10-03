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
 * NOTE: This process must be executed under device lock. It is the caller's
 * responsibility to ensure the device is locked before calling this function.
 *
 * @param[in] mx_dev - pointer to mx_dev device instance.
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mx_reset_device(struct mx_dev *mx_dev);

/*
 * @brief Restore the device and check that it is up and running.
 *
 * NOTE: This is part of the mx_reset_device() process above but is also
 * provided as a standalone function as a recovery mechanism in case the MX
 * device resets itself.
 *
 * @param[in] mx_dev - pointer to mx_dev device instance.
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mx_reset_restore_and_check_device(struct mx_dev *mx_dev);

#endif /* COMMON_MX_RESET_H_ */
