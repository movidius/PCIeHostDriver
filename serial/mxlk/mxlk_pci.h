/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Standard PCI management API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXLK_PCI_HEADER_
#define MXLK_PCI_HEADER_

#include "mxlk.h"

/*
 * @brief Initializes mxlk PCI components
 * NOTES:
 * 	1) To be called at PCI probe event before using any PCI functionality
 *
 * @param[in] mxlk - pointer to mxlk instance
 *
 * @return:
 *  	 0 - success
 *  	<0 - linux error code
 */
int  mxlk_pci_init(struct mxlk *mxlk, struct pci_dev *pdev);

/*
 * @brief Cleanup of mxlk PCI components
 *
 * NOTES:
 * 	1) To be called at driver exit
 * 	2) PCI related function inoperable after function finishes
 *
 * @param[in] mxlk - pointer to mxlk instance
 */
void mxlk_pci_cleanup(struct mxlk *mxlk);

#endif
