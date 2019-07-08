/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef BOOT_MXBL_MXBL_CORE_H_
#define BOOT_MXBL_MXBL_CORE_H_

#include "mxbl.h"

/*
 * @brief Initialize mxbl device instance
 *
 * @param[in] mxbl - pointer to mxbl object
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxbl_core_init(struct mxbl *mxbl, struct pci_dev *pdev, struct workqueue_struct * wq);

/*
 * @brief Cleanup of mxbl device instance
 *
 * @param[in] mxbl - pointer to mxbl object
 */
void mxbl_core_cleanup(struct mxbl *mxbl);

/*
 * @brief Performs first stage boot
 *
 * @param[in] mxbl   - pointer to mxbl object
 * @param[in] buffer - pointer to kernel memory containing image
 * @param[in] length - length of buffer in bytes
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxbl_first_stage_transfer(struct mxbl *mxbl, void *buffer, size_t length);

#endif /* BOOT_MXBL_MXBL_CORE_H_ */
