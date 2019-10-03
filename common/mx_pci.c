/*******************************************************************************
 *
 * MX PCI control functions
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mx_pci.h"

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>

#include "mx_common.h"
#include "mx_print.h"

static int aspm_enable = 0;
module_param(aspm_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(aspm_enable, "enable ASPM (default 0)");

static void set_aspm(struct pci_dev *pci, int aspm);

static void set_aspm(struct pci_dev *pci, int aspm)
{
    u16 link_control;

    pcie_capability_read_word(pci, PCI_EXP_LNKCTL, &link_control);
    link_control &= ~(PCI_EXP_LNKCTL_ASPMC);
    link_control |= (aspm & PCI_EXP_LNKCTL_ASPMC);
    pcie_capability_write_word(pci, PCI_EXP_LNKCTL, link_control);
}

int mx_pci_init(struct mx_dev *mx_dev, struct pci_dev *pci, void *drv_data,
                const char *drv_name, void __iomem **mmio)
{
    int error;

    mx_dev->pci = pci;
    pci_set_drvdata(pci, drv_data);

    error = pci_enable_device_mem(pci);
    if (error) {
        mx_err("Failed to enable pci device\n");
        goto error_enable;
    }

    error = pci_request_mem_regions(pci, drv_name);
    if (error) {
        mx_err("Failed to request mmio regions\n");
        goto error_req_mem;
    }

    *mmio = pci_ioremap_bar(pci, 2);
    if (!(*mmio)) {
        mx_err("Failed to ioremap mmio\n");
        goto error_ioremap;
    }
    mx_dev->mmio = *mmio;

    error = dma_set_mask_and_coherent(&pci->dev, DMA_BIT_MASK(64));
    if (error) {
        mx_info("Unable to set dma mask for 64bit\n");
        error = dma_set_mask_and_coherent(&pci->dev, DMA_BIT_MASK(32));
        if (error) {
            mx_err("Failed to set dma mask for 32bit\n");
            goto error_dma_mask;
        }
    }

    set_aspm(pci, aspm_enable);

    pci_set_master(pci);

    return 0;

error_dma_mask:
    iounmap(mmio);
error_ioremap:
    pci_release_mem_regions(pci);
error_req_mem:
    pci_disable_device(pci);
error_enable:

    return -1;
}

void mx_pci_cleanup(struct mx_dev *mx_dev)
{
    iounmap(mx_dev->mmio);

    pci_release_mem_regions(mx_dev->pci);
    pci_disable_device(mx_dev->pci);
}

int mx_pci_irq_init(struct mx_dev *mx_dev, const char *drv_name,
                    irq_handler_t drv_isr, void *drv_isr_data)
{
    int irq;
    int error;

    error = pci_alloc_irq_vectors(mx_dev->pci, 1, 1, PCI_IRQ_MSI);
    if (error < 0) {
        mx_err("failed to allocate %d MSI vectors - %d\n", 1, error);
        return error;
    }

    irq = pci_irq_vector(mx_dev->pci, 0);
    error = request_irq(irq, drv_isr, 0, drv_name, drv_isr_data);
    if (error) {
        mx_err("failed to request irqs - %d\n", error);
        return error;
    }

    return 0;
}

void mx_pci_irq_cleanup(struct mx_dev *mx_dev, void *drv_isr_data)
{
    int irq;

    irq = pci_irq_vector(mx_dev->pci, 0);
    synchronize_irq(irq);
    free_irq(irq, drv_isr_data);
    pci_free_irq_vectors(mx_dev->pci);
}

void mx_pci_dev_enable(struct mx_dev *mx_dev)
{
    pci_set_master(mx_dev->pci);
}

void mx_pci_dev_disable(struct mx_dev *mx_dev)
{
    pci_clear_master(mx_dev->pci);
}

void mx_pci_msi_set_enable(struct mx_dev *mx_dev, int enable)
{
    int msi_ctrl_offset = mx_dev->pci->msi_cap + PCI_MSI_FLAGS;
    u16 msi_ctrl;

    pci_read_config_word(mx_dev->pci, msi_ctrl_offset, &msi_ctrl);
    if (enable) {
        msi_ctrl |= PCI_MSI_FLAGS_ENABLE;
    } else {
        msi_ctrl &= ~PCI_MSI_FLAGS_ENABLE;
    }
    pci_write_config_word(mx_dev->pci, msi_ctrl_offset, msi_ctrl);
}

void mx_pci_dev_lock(struct mx_dev *mx_dev) {
    pci_cfg_access_lock(mx_dev->pci);
    device_lock(&mx_dev->pci->dev);
}

void mx_pci_dev_unlock(struct mx_dev *mx_dev) {
    device_unlock(&mx_dev->pci->dev);
    pci_cfg_access_unlock(mx_dev->pci);
}

void mx_pci_wait_for_pending_transaction(struct mx_dev *mx_dev) {
    pci_wait_for_pending_transaction(mx_dev->pci);
}

void mx_pci_dev_ctx_save(struct mx_dev *mx_dev)
{
    pci_save_state(mx_dev->pci);
}

void mx_pci_dev_ctx_restore(struct mx_dev *mx_dev)
{
    pci_restore_state(mx_dev->pci);
}

bool mx_pci_dev_id_valid(struct mx_dev *mx_dev)
{
    u32 vend_dev_id = 0;
    pci_read_config_dword(mx_dev->pci, PCI_VENDOR_ID, &vend_dev_id);
    return (vend_dev_id == (PCI_VENDOR_ID_INTEL | (MX_PCI_DEVICE_ID << 16)));
}
