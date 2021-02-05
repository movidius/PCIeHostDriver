/*******************************************************************************
 *
 * MX device reset infrastructure
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mx_reset.h"

#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "mx_common.h"
#include "mx_pci.h"
#include "mx_print.h"

/* Time given to MX device to detect and perform reset, in milliseconds.
 * This value may have to be updated if the polling task's period on device side
 * is changed. */
#define MX_DEV_RESET_TIME_MS 1000

/* Myriad Port Logic Vendor Specific DLLP register and configuration value */
#define MX_VENDOR_SPEC_DLLP     (0x704)
#define MX_RESET_DEV            (0xDEADDEAD)

int mx_reset_restore_and_check_device(struct mx_dev *mx_dev) {
    /* Restore the device's context. */
    mx_pci_dev_ctx_restore(mx_dev);

    /* Re-enable the device. */
    mx_pci_dev_enable(mx_dev);

    /* Check that the MX device is back in boot mode.
     * This implies setting MSI enable because the MX is waiting for this to
     * complete part of its initialization process needed to be able to read
     * MMIO space (and thus the operation mode). But the device will not start
     * sending MSIs before other parts of its initialization sequence (implying
     * other handshakes handled by higher layers) are completed. */
    mx_pci_msi_set_enable(mx_dev, 1);
    msleep(1);
    if (mx_get_opmode(mx_dev) != MX_OPMODE_BOOT) {
        return -EIO;
    }

    return 0;
}

int mx_reset_device(struct mx_dev *mx_dev) {
    int error;

    /* Save the device's context because its PCIe controller will be reset in
     * the process. */
    mx_pci_dev_ctx_save(mx_dev);

    /* Disable the device to put an end to all operations. */
    mx_pci_dev_disable(mx_dev);

    /* Ensure there are no transactions pending. */
    mx_pci_wait_for_pending_transaction(mx_dev);

    /* Write the magic into Vendor Specific DLLP register to trigger the device
     * reset. */
    pci_write_config_dword(mx_dev->pci, MX_VENDOR_SPEC_DLLP, MX_RESET_DEV);

    /* Give some time to the device to trigger and complete the reset. */
    msleep(MX_DEV_RESET_TIME_MS);

    /* Check that the device is up again before restoring the full PCI context. */
    if (!mx_pci_dev_id_valid(mx_dev)) {
        return -EIO;
    }

    /* Restore the full PCI context and check the device is up and running. */
    error = mx_reset_restore_and_check_device(mx_dev);
    if (error) {
        return error;
    }

    return 0;
}
