/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: General infrastructure
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxlk.h"
#include "mxlk_char.h"
#include "mxlk_core.h"

static const struct pci_device_id mxlk_pci_table[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_INTEL, MX_PCI_DEVICE_ID), 0},
    {0}
};

static struct workqueue_struct *mxlk_wq = NULL;

static int mxlk_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int error = 0;
    struct mxlk *mxlk = kzalloc(sizeof(*mxlk), GFP_KERNEL);

    if (!mxlk) {
        mx_err("failed to allocate mxlk for device %s\n", pci_name(pdev));
        return -ENOMEM;
    }

    mx_dbg("mxlk : %px\n", mxlk);
    error = mxlk_core_init(mxlk, pdev, mxlk_wq);
    if (error) {
        goto error_core;
    }

    return 0;

error_core :
    kfree(mxlk);

    return error;
}

static void mxlk_remove(struct pci_dev *pdev)
{
    struct mxlk *mxlk = pci_get_drvdata(pdev);

    mxlk_core_cleanup(mxlk);
    kfree(mxlk);
}

static struct pci_driver mxlk_driver =
{
    .name       = MXLK_DRIVER_NAME,
    .id_table   = mxlk_pci_table,
    .probe      = mxlk_probe,
    .remove     = mxlk_remove
};

static int __init mxlk_init_module(void)
{
    mxlk_wq = alloc_workqueue(MXLK_DRIVER_NAME, WQ_MEM_RECLAIM, 0);
    if (!mxlk_wq) {
        mx_err("failed to allocate workqueue\n");
        return -ENOMEM;
    }

    mxlk_chrdev_init();

    return pci_register_driver(&mxlk_driver);
}

static void __exit mxlk_exit_module(void)
{
    mx_dbg(" Exiting driver ...\n");
    pci_unregister_driver(&mxlk_driver);
    mxlk_chrdev_exit();
}

module_init(mxlk_init_module);
module_exit(mxlk_exit_module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel");
MODULE_DESCRIPTION(MXLK_DRIVER_DESC);
