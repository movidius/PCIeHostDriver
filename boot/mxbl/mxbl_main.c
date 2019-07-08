/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxbl.h"
#include "mxbl_char.h"
#include "mxbl_core.h"

static struct workqueue_struct *mxbl_wq;

static const struct pci_device_id mxbl_pci_table[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_INTEL, MX_PCI_DEVICE_ID), 0},
    {0}
};

static int mxbl_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct mxbl *mxbl = kzalloc(sizeof(*mxbl), GFP_KERNEL);

    if (!mxbl) {
        mx_err("Failed to allocate mxbl for pci device %s\n", pci_name(pdev));
        return -ENOMEM;
    }

    return mxbl_core_init(mxbl, pdev, mxbl_wq);
}

static void mxbl_remove(struct pci_dev *pdev)
{
    struct mxbl *mxbl = pci_get_drvdata(pdev);

    mxbl_core_cleanup(mxbl);
    kfree(mxbl);
}

static struct pci_driver mxbl_driver =
{
    .name = MXBL_DRIVER_NAME,
    .id_table = mxbl_pci_table,
    .probe = mxbl_probe,
    .remove = mxbl_remove
};

static int __init mxbl_init_module(void)
{
    mx_info("%s - %s\n", MXBL_DRIVER_NAME, MXBL_DRIVER_DESC);

    mxbl_wq = alloc_workqueue(MXBL_DRIVER_NAME, WQ_MEM_RECLAIM, 0);
    if (!mxbl_wq) {
        mx_err("Failed to create workqueue\n");
        return -ENOMEM;
    }

#ifndef EMBEDDED_BINARY
    mxbl_chrdev_init();
#endif

    return pci_register_driver(&mxbl_driver);
}

static void __exit mxbl_exit_module(void)
{
    pci_unregister_driver(&mxbl_driver);
    destroy_workqueue(mxbl_wq);
#ifndef EMBEDDED_BINARY
    mxbl_chrdev_exit();
#endif
}

module_init(mxbl_init_module);
module_exit(mxbl_exit_module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel");
MODULE_DESCRIPTION(MXBL_DRIVER_NAME);
