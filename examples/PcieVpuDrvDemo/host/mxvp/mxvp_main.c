/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: General infrastructure
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxvp.h"
#include "mxvp_bspec.h"
#include "mxvp_print.h"

static struct workqueue_struct *mxvp_wq;

static const struct pci_device_id mxvp_pci_table[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x6200), 0},
    {0}
};

static int mxvp_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct mxvp *mxvp = kzalloc(sizeof(*mxvp), GFP_KERNEL);

    if (!mxvp) {
        mxvp_error("Failed to allocate mxvp for pci device %s\n", pci_name(pdev));
        return -ENOMEM;
    }

    mxvp->pci = pdev;
    mxvp->wq  = mxvp_wq;
    return mxvp_bspec_init(mxvp);
}

static void mxvp_remove(struct pci_dev *pdev)
{
    struct mxvp *mxvp = pci_get_drvdata(pdev);
    mxvp_bspec_cleanup(mxvp);
    kfree(mxvp);
}

static struct pci_driver mxvp_driver =
{
    .name = MXVP_DRIVER_NAME,
    .id_table = mxvp_pci_table,
    .probe = mxvp_probe,
    .remove = mxvp_remove
};

static int __init mxvp_init_module(void)
{
    mxvp_info("%s - %s\n", MXVP_DRIVER_NAME, MXVP_DRIVER_DESC);

    mxvp_wq = alloc_workqueue(MXVP_DRIVER_NAME, WQ_MEM_RECLAIM, 0);
    if (!mxvp_wq) {
        mxvp_error("Failed to create workqueue\n");
        return -ENOMEM;
    }

    return pci_register_driver(&mxvp_driver);
}

static void __exit mxvp_exit_module(void)
{
    pci_unregister_driver(&mxvp_driver);
    destroy_workqueue(mxvp_wq);
}

module_init(mxvp_init_module);
module_exit(mxvp_exit_module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel");
MODULE_DESCRIPTION(MXVP_DRIVER_NAME);
