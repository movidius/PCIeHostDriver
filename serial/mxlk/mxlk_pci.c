/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Standard PCI management
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxlk_pci.h"
#include "mxlk_print.h"

static int aspm_enable = 0;
module_param(aspm_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(aspm_enable, "enable ASPM");

static void mxlk_pci_set_aspm(struct mxlk *mxlk, int aspm);

int mxlk_pci_init(struct mxlk *mxlk, struct pci_dev *pdev)
{
    int error;

    mxlk->pci = pdev;
    pci_set_drvdata(pdev, mxlk);

    error = pci_enable_device_mem(mxlk->pci);
    if (error) {
        mxlk_error("Failed to enable pci device\n");
        return error;
    }

    error = pci_request_mem_regions(mxlk->pci, MXLK_DRIVER_NAME);
    if (error) {
        mxlk_error("Failed to request mmio regions\n");
        goto error_req_mem;
    }

    mxlk->mmio = pci_ioremap_bar(mxlk->pci, 2);
    if (!mxlk->mmio) {
        mxlk_error("Failed to ioremap mmio\n");
        goto error_ioremap;
    }

    /* set up for high or low dma */
    error = dma_set_mask_and_coherent(&mxlk->pci->dev, DMA_BIT_MASK(64));
    if (error) {
        mxlk_info("Unable to set dma mask for 64bit\n");
        error = dma_set_mask_and_coherent(&mxlk->pci->dev, DMA_BIT_MASK(32));
        if (error) {
            mxlk_error("Failed to set dma mask for 32bit\n");
            goto error_dma_mask;
        }
    }

    mxlk_pci_set_aspm(mxlk, aspm_enable);

    pci_set_master(mxlk->pci);

    return 0;

error_dma_mask :
    iounmap(mxlk->mmio);

error_ioremap :
    pci_release_mem_regions(mxlk->pci);

error_req_mem :
    pci_disable_device(mxlk->pci);

    return -1;
}

void mxlk_pci_cleanup(struct mxlk *mxlk)
{
    iounmap(mxlk->mmio);

    pci_release_mem_regions(mxlk->pci);
    pci_disable_device(mxlk->pci);
    pci_set_drvdata(mxlk->pci, NULL);
}

static void mxlk_pci_set_aspm(struct mxlk *mxlk, int aspm)
{
    u8  cap_exp;
    u16 link_control;

    cap_exp = pci_find_capability(mxlk->pci, PCI_CAP_ID_EXP);
    if (!cap_exp) {
        mxlk_error("failed to find pcie capability\n");
        return;
    }

    pci_read_config_word(mxlk->pci, cap_exp + PCI_EXP_LNKCTL, &link_control);
    link_control &= ~(PCI_EXP_LNKCTL_ASPMC);
    link_control |= (aspm & PCI_EXP_LNKCTL_ASPMC);
    pci_write_config_word(mxlk->pci, cap_exp + PCI_EXP_LNKCTL, link_control);
}
