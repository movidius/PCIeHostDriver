/*******************************************************************************
 *
 * MX PCI control functions
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef COMMON_MX_PCI_H_
#define COMMON_MX_PCI_H_

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/pci.h>

#include "mx_common.h"

/*
 * @brief Initializes Myriad X PCI components.
 *
 * NOTE: To be called at PCI probe event before using any PCI functionality.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 * @param[in] pci - pointer to pci device instance.
 * @param[in] drv_data - driver control structure.
 * @param[in] drv_name - driver name.
 * @param[out] mmio - pointer to MMIO space.
 *
 * @return:
 *       0 - success.
 *      <0 - linux error code.
 */
int mx_pci_init(struct mx_dev *mx_dev, struct pci_dev *pci, void *drv_data,
                const char *drv_name, void __iomem **mmio);

/*
 * @brief Cleanup of MX PCI components.
 *
 * NOTES:
 *  1. To be called at driver exit.
 *  2. PCI related function inoperable after function finishes.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 */
void mx_pci_cleanup(struct mx_dev *mx_dev);

/*
 * @brief Initializes Myriad X PCI MSI IRQ.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 * @param[in] drv_name - driver name.
 * @param[in] drv_isr - ISR provided by driver.
 * @param[in] drv_data - Data passed to ISR when invoked.
 *
 * @return:
 *       0 - success.
 *      <0 - linux error code.
 */
int mx_pci_irq_init(struct mx_dev *mx_dev, const char *drv_name,
                    irq_handler_t drv_isr, void *drv_isr_data);

/*
 * @brief Cleanup of Myriad X PCI MSI IRQ.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 * @param[in] drv_data - Data passed to ISR when invoked.
 */
void mx_pci_irq_cleanup(struct mx_dev *mx_dev, void *drv_isr_data);

/*
 * @brief Set/clear MSI enable bit.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 * @param[in] enable - 1 to set MSI enable, 0 to clear it.
 */
void mx_pci_msi_set_enable(struct mx_dev *mx_dev, int enable);

/*
 * @brief Lock MX PCI device.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 */
void mx_pci_dev_lock(struct mx_dev *mx_dev);

/*
 * @brief Unlock MX PCI device.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 */
void mx_pci_dev_unlock(struct mx_dev *mx_dev);

/*
 * @brief Wait until all pending PCI transactions to MX device are completed.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 */
void mx_pci_wait_for_pending_transaction(struct mx_dev *mx_dev);

/*
 * @brief Save the PCI device's context.
 *
 * NOTES:
 *  1. Typically, to be called before resetting the device.
 *  2. This function also disables the device (clears Bus Master Enable).
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 */
void mx_pci_dev_ctx_save(struct mx_dev *mx_dev);

/*
 * @brief Restore the PCI device's context.
 *
 * NOTES:
 *  1. Typically, to be called after resetting the device.
 *  2. This function also re-enables the device.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 */
void mx_pci_dev_ctx_restore(struct mx_dev *mx_dev);

/*
 * @brief Check that the device's PCI ID (vendor and device) is valid (must be
 *        an Intel Myriad X device).
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 *
 * @return:
 *       0 - Device's PCI ID is not valid.
 *       1 - Device's PCI ID is valid.
 */
bool mx_pci_dev_id_valid(struct mx_dev *mx_dev);

#endif /* COMMON_MX_PCI_H_ */
