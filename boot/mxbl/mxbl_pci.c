/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxbl_pci.h"

#include "mxbl_print.h"

/* Size of the PCI standard header, in 32-bit words */
#define PCI_HDR_SIZE_DW (PCI_STD_HEADER_SIZEOF / 4)
/* Number of 16-bit control fields in PCIe capability structure (v2) */
#define PCIE_CAP_CTRL_FIELD_NB 7
/* Structure used to save and restore PCI context when resetting the device.
 * The choice of the fields to save is based on the pci_dev_save_and_disable()
 * and pci_dev_restore() kernel functions. We have to re-implement them because
 * there not public and as we cannot use the higher level pci_reset_function()
 * API because of the non-standard method we use to trigger the device reset. */
static struct {
    u32 hdr[PCI_HDR_SIZE_DW];
    u16 pcie[PCIE_CAP_CTRL_FIELD_NB];
} saved_cfg;

static int aspm_enable = 0;
module_param(aspm_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(aspm_enable, "enable ASPM (default 0)");

static void mxbl_pci_set_aspm(struct mxbl *mxbl, int aspm);

int mxbl_pci_init(struct mxbl *mxbl)
{
    int error;

    error = pci_enable_device_mem(mxbl->pci);
    if (error) {
        mxbl_error("Failed to enable pci device\n");
        return error;
    }

    error = pci_request_mem_regions(mxbl->pci, MXBL_DRIVER_NAME);
    if (error) {
        mxbl_error("Failed to request mmio regions\n");
        return error;
    }

    pci_set_drvdata(mxbl->pci, mxbl);

    mxbl->mmio = pci_ioremap_bar(mxbl->pci, 2);
    if (!mxbl->mmio) {
        mxbl_error("Failed to ioremap mmio\n");
        return -1;
    }

    /* set up for high or low dma */
    error = dma_set_mask_and_coherent(&mxbl->pci->dev, DMA_BIT_MASK(64));
    if (error) {
        mxbl_info("Unable to set dma mask for 64bit\n");
        error = dma_set_mask_and_coherent(&mxbl->pci->dev, DMA_BIT_MASK(32));
        if (error) {
            mxbl_error("Failed to set dma mask for 32bit\n");
            return error;
        }
    }

    mxbl_pci_set_aspm(mxbl, aspm_enable);

    pci_set_master(mxbl->pci);

    return 0;
}

void mxbl_pci_cleanup(struct mxbl *mxbl)
{
    iounmap(mxbl->mmio);

    pci_release_mem_regions(mxbl->pci);
    pci_disable_device(mxbl->pci);
}

static void mxbl_pci_set_aspm(struct mxbl *mxbl, int aspm)
{
    u16 link_control;

    pcie_capability_read_word(mxbl->pci, PCI_EXP_LNKCTL, &link_control);
    link_control &= ~(PCI_EXP_LNKCTL_ASPMC);
    link_control |= (aspm & PCI_EXP_LNKCTL_ASPMC);
    pcie_capability_write_word(mxbl->pci, PCI_EXP_LNKCTL, link_control);
}

void mxbl_pci_dev_ctx_save(struct mxbl *mxbl) {
    int i;

    /* Save complete PCI standard header. */
    for (i = 0; i < PCI_HDR_SIZE_DW; i++)
        pci_read_config_dword(mxbl->pci, i * 4, &saved_cfg.hdr[i]);
    /* Save control fields in PCIe capability structure. */
    pcie_capability_read_word(mxbl->pci, PCI_EXP_DEVCTL, &saved_cfg.pcie[0]);
    pcie_capability_read_word(mxbl->pci, PCI_EXP_LNKCTL, &saved_cfg.pcie[1]);
    pcie_capability_read_word(mxbl->pci, PCI_EXP_SLTCTL, &saved_cfg.pcie[2]);
    pcie_capability_read_word(mxbl->pci, PCI_EXP_RTCTL, &saved_cfg.pcie[3]);
    pcie_capability_read_word(mxbl->pci, PCI_EXP_DEVCTL2, &saved_cfg.pcie[4]);
    pcie_capability_read_word(mxbl->pci, PCI_EXP_LNKCTL2, &saved_cfg.pcie[5]);
    pcie_capability_read_word(mxbl->pci, PCI_EXP_SLTCTL2, &saved_cfg.pcie[6]);

    /* Disable the device by clearing the command register, except for legacy
     * interrupts that are disabled. As we saved the command register value
     * before disabling, the device will be naturally re-enabled upon restoring. */
    pci_write_config_word(mxbl->pci, PCI_COMMAND, PCI_COMMAND_INTX_DISABLE);
}

void mxbl_pci_dev_ctx_restore(struct mxbl *mxbl) {
    int i;

    /* Restore control fields in PCIe capability structure. Must be done before
     * re-applying header configuration, as the latter will also re-enable the
     * device. */
    pcie_capability_write_word(mxbl->pci, PCI_EXP_DEVCTL, saved_cfg.pcie[0]);
    pcie_capability_write_word(mxbl->pci, PCI_EXP_LNKCTL, saved_cfg.pcie[1]);
    pcie_capability_write_word(mxbl->pci, PCI_EXP_SLTCTL, saved_cfg.pcie[2]);
    pcie_capability_write_word(mxbl->pci, PCI_EXP_RTCTL, saved_cfg.pcie[3]);
    pcie_capability_write_word(mxbl->pci, PCI_EXP_DEVCTL2, saved_cfg.pcie[4]);
    pcie_capability_write_word(mxbl->pci, PCI_EXP_LNKCTL2, saved_cfg.pcie[5]);
    pcie_capability_write_word(mxbl->pci, PCI_EXP_SLTCTL2, saved_cfg.pcie[6]);
    /* Restore complete PCI standard header. Finish by the command register to
     * ensure the configuration is clean when we re-enable the device. */
    for (i = 4; i < PCI_HDR_SIZE_DW; i++)
        pci_write_config_dword(mxbl->pci, i * 4, saved_cfg.hdr[i]);
    for (i = 0; i <= 3; i++)
        pci_write_config_dword(mxbl->pci, i * 4, saved_cfg.hdr[i]);
}

bool mxbl_pci_dev_id_valid(struct mxbl *mxbl) {
    u32 vend_dev_id = 0;
    pci_read_config_dword(mxbl->pci, PCI_VENDOR_ID, &vend_dev_id);
    return (vend_dev_id == (PCI_VENDOR_ID_INTEL | (MXBL_MX_PCI_DEVICE_ID << 16)));
}
