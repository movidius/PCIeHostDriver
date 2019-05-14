/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Data transfer engine API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXLK_CORE_HEADER_
#define MXLK_CORE_HEADER_

#include "mxlk.h"

/*
 * @brief Initializes mxlk core component
 * NOTES:
 *  1) To be called at PCI probe event
 *
 * @param[in] mxlk - pointer to mxlk instance
 * @param[in] pdev - pointer to pci dev instance
 * @param[in] wq   - pointer to work queue to use
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int  mxlk_core_init(struct mxlk *mxlk, struct pci_dev *pdev, struct workqueue_struct * wq);

/*
 * @brief cleans up mxlk core component
 * NOTES:
 *  1) To be called at PCI remove event
 *
 * @param[in] mxlk - pointer to mxlk instance
 *
 */
void mxlk_core_cleanup(struct mxlk *mxlk);

/*
 * @brief opens mxlk interface
 *
 * @param[in] inf - pointer to interface instance
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxlk_core_open(struct mxlk_interface *inf);

/*
 * @brief closes mxlk interface
 *
 * @param[in] inf - pointer to interface instance
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxlk_core_close(struct mxlk_interface *inf);

/*
 * @brief read buffer from mxlk interface
 *
 * @param[in] inf    - pointer to interface instance
 * @param[in] buffer - pointer to userspace buffer
 * @param[in] length - max bytes to copy into buffer
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
ssize_t mxlk_core_read(struct mxlk_interface *inf, void *buffer, size_t length);

/*
 * @brief writes buffer to mxlk interface
 *
 * @param[in] inf    - pointer to interface instance
 * @param[in] buffer - pointer to userspace buffer
 * @param[in] length - length of buffer to copy from
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
ssize_t mxlk_core_write(struct mxlk_interface *inf, void *buffer, size_t length);

#endif
