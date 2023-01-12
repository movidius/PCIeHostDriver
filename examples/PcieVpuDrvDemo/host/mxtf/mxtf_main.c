/*******************************************************************************
 *
 * Intel Myriad-X Test Framework Driver: General infrastructure
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxtf.h"
#include "mxtf_test.h"
#include "mxtf_print.h"

#include "mxvp_bspec.h"

static struct workqueue_struct *mxtf_wq;

static const struct pci_device_id mxtf_pci_table[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x6200), 0},
    {0}
};

static int mxtf_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int error;
    struct mxtf *mxtf = kzalloc(sizeof(*mxtf), GFP_KERNEL);

    mxtf_info("pci device : %s\n", pci_name(pdev));

    if (!mxtf) {
        mxtf_error("Failed to allocate mxtf for pci device %s\n", pci_name(pdev));
        return -ENOMEM;
    }

    mxtf->mxvp.pci = pdev;
    mxtf->mxvp.wq  = mxtf_wq;
    error = mxvp_bspec_init(&mxtf->mxvp);
    if (error) {
        mxtf_error("Failed to init bspec\n");
        return error;
    }

    return mxtf_test_init(mxtf);
}

static void mxtf_remove(struct pci_dev *pdev)
{
    struct mxvp *mxvp = pci_get_drvdata(pdev);
    struct mxtf *mxtf = container_of(mxvp, struct mxtf, mxvp);

    mxtf_test_cleanup(mxtf);
    mxvp_bspec_cleanup(&mxtf->mxvp);
    kfree(mxtf);
}

static struct pci_driver mxtf_driver =
{
    .name = MXTF_DRIVER_NAME,
    .id_table = mxtf_pci_table,
    .probe = mxtf_probe,
    .remove = mxtf_remove
};

static int __init mxtf_init_module(void)
{
    mxtf_info("%s - %s\n", MXTF_DRIVER_NAME, MXTF_DRIVER_DESC);

    mxtf_wq = alloc_workqueue(MXTF_DRIVER_NAME, WQ_MEM_RECLAIM, 0);
    if (!mxtf_wq) {
        mxtf_error("Failed to create workqueue\n");
        return -ENOMEM;
    }

    return pci_register_driver(&mxtf_driver);
}

static void __exit mxtf_exit_module(void)
{
    pci_unregister_driver(&mxtf_driver);
    destroy_workqueue(mxtf_wq);
}

module_init(mxtf_init_module);
module_exit(mxtf_exit_module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel");
MODULE_DESCRIPTION(MXTF_DRIVER_NAME);
