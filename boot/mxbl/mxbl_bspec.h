/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXBL_BSPEC_HEADER_
#define MXBL_BSPEC_HEADER_

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
int mxbl_bspec_init(struct mxbl *mxbl, struct pci_dev *pdev, struct workqueue_struct * wq);

/*
 * @brief Cleanup of mxbl device instance
 *
 * @param[in] mxbl - pointer to mxbl object
 */
void mxbl_bspec_cleanup(struct mxbl *mxbl);

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
#endif
