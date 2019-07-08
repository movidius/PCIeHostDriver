/*******************************************************************************
 *
 * MX PCIe boot API
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef COMMON_MX_BOOT_H_
#define COMMON_MX_BOOT_H_

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/pci.h>

#include "mx_common.h"

/*
 * @brief Initialize MX boot component
 *
 * NOTE The calling driver must enable MSI of the MX device before calling this
 * function.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 *
 */
void mx_boot_init(struct mx_dev *mx_dev);

/*
 * @brief Clean up MX boot component
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 *
 */
void mx_boot_cleanup(struct mx_dev *mx_dev);

/*
 * @brief Enable STATUS UPDATE interrupt.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 */
void mx_boot_status_update_int_enable(struct mx_dev *mx_dev);

/*
 * @brief Disable STATUS UPDATE interrupt.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 */
void mx_boot_status_update_int_disable(struct mx_dev *mx_dev);

/*
 * @brief Load an MX application image into the MX device
 *
 * NOTE: If the user uses the character device provided by the MX boot component,
 * it is not necessary to call this function directly as it will done by writing
 * into the corresponding character device.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 * @param[in] buffer - pointer to memory containing image.
 * @param[in] length - length of buffer in bytes.
 * @param[in] user_mem_buffer - true if the buffer is stored in user memory,
 *            false if it is stored in kernel memory.
 *
 * @return:
 *      - if success: length of the image loaded
 *      - if error: -1
 */
int mx_boot_load_image(struct mx_dev *mx_dev, const char *buffer, size_t length,
                       bool user_mem_buffer);

#endif /* COMMON_MX_BOOT_H_ */
