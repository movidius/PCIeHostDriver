/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXBL_PCI_HEADER_
#define MXBL_PCI_HEADER_

#include "mxbl.h"

/*
 * @brief Initializes mxbl PCI components
 *
 * NOTES:
 *  1) To be called at PCI probe event before using any PCI functionality
 *
 * @param[in] mxbl - pointer to mxbl instance
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int  mxbl_pci_init(struct mxbl *mxbl);

/*
 * @brief Cleanup of mxbl PCI components
 *
 * NOTES:
 *  1) To be called at driver exit
 *  2) PCI related function inoperable after function finishes
 *
 * @param[in] mxbl - pointer to mxbl instance
 */
void mxbl_pci_cleanup(struct mxbl *mxbl);

/*
 * @brief Save the PCI device's context
 *
 * NOTES:
 *  1) Typically, to be called before resetting the device
 *  2) This function also disables the device (clears Bus Master Enable)
 *
 * @param[in] mxbl - pointer to mxbl instance
 */
void mxbl_pci_dev_ctx_save(struct mxbl *mxbl);

/*
 * @brief Restore the PCI device's context
 *
 * NOTES:
 *  1) Typically, to be called after resetting the device
 *  2) This function also re-enables the device
 *
 * @param[in] mxbl - pointer to mxbl instance
 */
void mxbl_pci_dev_ctx_restore(struct mxbl *mxbl);

/*
 * @brief Check that the device's PCI ID (vendor and device) is valid (must be
 *        an Intel Myriad X device).
 *
 * @param[in] mxbl - pointer to mxbl instance
 *
 * @return:
 *       0 - Device's PCI ID is not valid
 *       1 - Device's PCI ID is valid
 */
bool mxbl_pci_dev_id_valid(struct mxbl *mxbl);

#endif
