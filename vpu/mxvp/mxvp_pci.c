/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: Standard PCIe management
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxvp_pci.h"
#include "mxvp_print.h"

static int aspm_enable = 0;
module_param(aspm_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(aspm_enable, "enable ASPM:\n \t[0] disabled\n\t[1] L0s\n\t[2] L1\n\t[3] L0s & L1");

static void mxvp_pci_set_aspm(struct mxvp *mxvp, int aspm);

int mxvp_pci_init(struct mxvp *mxvp)
{
    int error;

    error = pci_enable_device_mem(mxvp->pci);
    if (error) {
        mxvp_error("Failed to enable pci device\n");
        return error;
    }

    error = pci_request_mem_regions(mxvp->pci, MXVP_DRIVER_NAME);
    if (error) {
        mxvp_error("Failed to request mmio regions\n");
        return error;
    }

    pci_set_drvdata(mxvp->pci, mxvp);

    mxvp->mmio = pci_ioremap_bar(mxvp->pci, 2);
    if (!mxvp->mmio) {
        mxvp_error("Failed to ioremap mmio\n");
        return -1;
    }

    mxvp->ddr_virt = pci_ioremap_bar(mxvp->pci, 4);
    if (!mxvp->ddr_virt) {
        mxvp_error("Failed to ioremap ddr_virt\n");
        return -1;
    }

    /* set up for high or low dma */
    error = dma_set_mask_and_coherent(&mxvp->pci->dev, DMA_BIT_MASK(64));
    if (error) {
        mxvp_info("Unable to set dma mask for 64bit\n");
        error = dma_set_mask_and_coherent(&mxvp->pci->dev, DMA_BIT_MASK(32));
        if (error) {
            mxvp_error("Failed to set dma mask for 32bit\n");
            return error;
        }
    }

    mxvp_pci_set_aspm(mxvp, aspm_enable);

    pci_set_master(mxvp->pci);

    return 0;
}

void mxvp_pci_cleanup(struct mxvp *mxvp)
{
    iounmap(mxvp->mmio);
    iounmap(mxvp->ddr_virt);

    pci_release_mem_regions(mxvp->pci);
    pci_disable_device(mxvp->pci);
}

static void mxvp_pci_set_aspm(struct mxvp *mxvp, int aspm)
{
    u8  cap_exp;
    u16 link_control;

    cap_exp =  pci_find_capability(mxvp->pci, PCI_CAP_ID_EXP);
    if (!cap_exp) {
        mxvp_error("failed to find pcie capability\n");
        return;
    }

    pci_read_config_word(mxvp->pci, cap_exp + PCI_EXP_LNKCTL, &link_control);
    link_control &= ~(PCI_EXP_LNKCTL_ASPMC);
    link_control |= (aspm & PCI_EXP_LNKCTL_ASPMC);
    pci_write_config_word(mxvp->pci, cap_exp + PCI_EXP_LNKCTL, link_control);
}
