/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: Standard PCIe management API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXVP_PCI_HEADER_
#define MXVP_PCI_HEADER_

#include "mxvp.h"

/*
 * @brief Initializes mxvp PCI components
 * NOTES:
 *  1) To be called at PCI probe event before using any PCI functionality
 *
 * @param[in] mxvp - pointer to mxvp instance
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int  mxvp_pci_init(struct mxvp *mxvp);

/*
 * @brief Cleanup of mxvp PCI components
 *
 * NOTES:
 *  1) To be called at driver exit
 *  2) PCI related function inoperable after function finishes
 *
 * @param[in] mxvp - pointer to mxvp instance
 */
void mxvp_pci_cleanup(struct mxvp *mxvp);

#endif
