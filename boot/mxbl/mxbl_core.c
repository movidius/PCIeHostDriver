/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxbl_core.h"

#include <linux/bitops.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "mx_pci.h"
#include "mx_boot.h"
#include "mx_reset.h"

#include "mxbl.h"
#include "mxbl_char.h"

#ifdef EMBEDDED_BINARY
    #include "mxbl_boot_image.h"
#endif

static irqreturn_t mxbl_event_isr(int irq, void *args);
static int  mxbl_events_init(struct mxbl *mxbl);
static void mxbl_events_cleanup(struct mxbl *mxbl);
static void mxbl_status_update_handler(struct work_struct *work);

static atomic_t units_found = ATOMIC_INIT(0);

static irqreturn_t mxbl_event_isr(int irq, void *args)
{
    struct mxbl *mxbl = args;
    u32 identity = mx_rd32(mxbl->mmio, MX_INT_IDENTITY);

    /* Check for source of interrupt */
    if (field_get(MX_INT_STATUS_UPDATE, identity)) {
        queue_work(mxbl->wq, &mxbl->status_update);
    }

    /* Report all events handled to VPU */
    mx_wr32(mxbl->mmio, MX_INT_IDENTITY, 0);

    /* Report MSI handled */
    return IRQ_HANDLED;
}

static int mxbl_events_init(struct mxbl *mxbl)
{
    int error;

    INIT_WORK(&mxbl->status_update, mxbl_status_update_handler);

    error = mx_pci_irq_init(&mxbl->mx_dev, MXBL_DRIVER_NAME, mxbl_event_isr, mxbl);
    if (error) {
        return error;
    }

    mx_boot_status_update_int_enable(&mxbl->mx_dev);

    return 0;
}

static void mxbl_events_cleanup(struct mxbl *mxbl)
{
    if (mx_get_opmode(&mxbl->mx_dev) == MX_OPMODE_BOOT) {
        mx_boot_status_update_int_disable(&mxbl->mx_dev);
    }

    cancel_work_sync(&mxbl->status_update);

    mx_pci_irq_cleanup(&mxbl->mx_dev, mxbl);
}

static void mxbl_status_update_handler(struct work_struct *work)
{
    struct mxbl *mxbl = container_of(work, struct mxbl, status_update);

    if (mx_get_opmode(&mxbl->mx_dev) == MX_OPMODE_BOOT) {
#ifdef EMBEDDED_BINARY
        size_t length = sizeof(mxbl_boot_image);
        mx_boot_load_image(&mxbl->mx_dev, mxbl_boot_image, length, false);
#else
        mxbl_chrdev_add(mxbl);
#endif
    }
}

int mxbl_core_init(struct mxbl *mxbl, struct pci_dev *pdev,
                    struct workqueue_struct * wq)
{
    int error = 0;

    if (!wq) {
        return -EINVAL;
    }

    if (!pdev) {
        return -EINVAL;
    }

    mxbl->wq  = wq;
    mxbl->pci = pdev;

    error = mx_pci_init(&mxbl->mx_dev, pdev, mxbl, MXBL_DRIVER_NAME, &mxbl->mmio);
    if (error) {
        goto error_pci_init;
    }

    error = mxbl_events_init(mxbl);
    if (error) {
        goto error_events_init;
    }

    mx_boot_init(&mxbl->mx_dev);

    if (mx_get_opmode(&mxbl->mx_dev) != MX_OPMODE_BOOT) {
        /* Device is not in boot mode: Some application was already running. */
        /* In this case we need to reset the device to bring it back to boot
         * mode. */

        /* As MSI configuration will be reset in the process, we will have to
         * re-do events initialization after the reset. So do a proper cleanup
         * now to ensure the later events initialization is performed properly. */
         mxbl_events_cleanup(mxbl);
         /* Reset the device.
          * Device is locked already as we are inside the probing process. */
         error = mx_reset_device(&mxbl->mx_dev);
         if (error) {
             goto error_reset;
         }
         /* Re-initialize events. */
         mxbl_events_init(mxbl);
    }

    /* In case the driver is unloaded and loaded again, the interrupt will not be triggered,
       in such cases the loop below will kick in and add  */ 
    if ( (mx_get_opmode(&mxbl->mx_dev) == MX_OPMODE_BOOT) && 
         (!mxbl->chrdev_added) ){
#ifdef EMBEDDED_BINARY
        size_t length = sizeof(mxbl_boot_image);
        mx_boot_load_image(&mxbl->mx_dev, mxbl_boot_image, length, false);
#else
        mxbl_chrdev_add(mxbl);
#endif
    }

    mxbl->unit = atomic_fetch_add(1, &units_found);
    if (mxbl->unit >= MXBL_MAX_DEVICES) {
        goto error_max_devices;
    }

    return 0;

error_max_devices:
    mxbl_events_cleanup(mxbl);
    /* Try to clean up chrdev as well in case we received an interrupt */
    mxbl_chrdev_remove(mxbl);

error_events_init:
error_reset:
    mx_pci_cleanup(&mxbl->mx_dev);

error_pci_init:
    mx_err("failed to init (%d)\n", error);
    return error;
}

void mxbl_core_cleanup(struct mxbl *mxbl)
{
    mxbl_chrdev_remove(mxbl);
    mx_boot_cleanup(&mxbl->mx_dev);
    mxbl_events_cleanup(mxbl);
    mx_pci_cleanup(&mxbl->mx_dev);
}
