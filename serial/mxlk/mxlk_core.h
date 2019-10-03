/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Data transfer engine API
 *
 * Copyright (C) 2018 - 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef SERIAL_MXLK_MXLK_CORE_H_
#define SERIAL_MXLK_MXLK_CORE_H_

#include "mxlk.h"
#include "mxlk_ioctl.h"

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
int mxlk_core_init(struct mxlk *mxlk, struct pci_dev *pdev, struct workqueue_struct * wq);

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

/*
 * @brief indicates if there is read data available for a given interface
 *
 * @param[in] inf    - pointer to interface instance
 *
 * @return true if there is data available, false otherwise
 */
bool mxlk_core_read_data_available(struct mxlk_interface *inf);

/*
 * @brief indicates if there are available buffers in the TX pool
 *
 * @param[in] mxlk - pointer to mxlk instance
 *
 * @return true if there are buffers available, false otherwise
 */
bool mxlk_core_write_buffer_available(struct mxlk *mxlk);

/*
 * @brief resets the MX device
 *
 * @param[in] mxlk - pointer to mxlk instance
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxlk_core_reset_dev(struct mxlk *mxlk);

/*
 * @brief loads and boots an MX application image on the MX device
 *
 * @param[in] mxlk - pointer to mxlk instance
 * @param[in] buffer - pointer to userspace buffer containing the MX image
 * @param[in] length - length of the MX image
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxlk_core_boot_dev(struct mxlk *mxlk, const char *buffer, size_t length);

/*
 * @brief Indicates if the MX device has booted or not
 *
 * @param[in] mxlk - pointer to mxlk instance
 * @param[out] fw_status - status of the MX device
 *
 */
void mxlk_get_dev_status(struct mxlk *mxlk, enum mxlk_fw_status *fw_status);

#endif /* SERIAL_MXLK_MXLK_CORE_H_ */
