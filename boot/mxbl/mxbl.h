/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef BOOT_MXBL_MXBL_H_
#define BOOT_MXBL_MXBL_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/atomic.h>

#include "mx_common.h"
#include "mx_mmio.h"
#include "mx_print.h"

#define MXBL_DRIVER_NAME "mxbl"
#define MXBL_DRIVER_DESC "Intel(R) Myriad X Bootloader"

#define MXBL_TO_PCI(mxbl) ((mxbl)->pci)
#define MXBL_TO_DEV(mxbl) (&(mxbl)->pci->dev)

#define MXBL_MAX_DEVICES 1

/* Myriad-X Bootloader driver (MXBL) device instance control structure */
struct mxbl {
    struct pci_dev *pci;    /* pointer to pci device provided by probe */
    void __iomem *mmio;     /* kernel virtual address to MMIO (BAR2) */

    struct workqueue_struct *wq;     /* work queue to use for event handling */
    struct work_struct status_update; /* work for handling status updates */

    int unit; /* unit number reserving minor */
    int chrdev_added; /* flag indicating char device exists (for cleanup) */
    struct cdev cdev; /* standard chrdev object */
    struct device *chrdev; /* standard chrdev object */

    struct mx_dev mx_dev; /* MX device control structure */
};

#endif /* BOOT_MXBL_MXBL_H_ */
