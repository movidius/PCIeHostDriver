/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxbl_char.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "mxbl_bspec.h"
#include "mxbl_print.h"

#define MXBL_CLASS_NAME  MXBL_DRIVER_NAME

static dev_t mxbl_devno = 0;
static int minors = MXBL_MAX_DEVICES;
static struct class *mxbl_class = NULL;

#define priv_to_mxbl(filp) ((struct mxbl *) filp->private_data)

static int mxbl_dev_open(struct inode *inode, struct file* filp)
{
    struct mxbl *mxbl = container_of(inode->i_cdev, struct mxbl, cdev);
    filp->private_data = mxbl;

    return 0;
}

static int mxbl_dev_release(struct inode *inode, struct file *filp)
{
    filp->private_data = NULL;

    return 0;
}

static ssize_t mxbl_dev_read(struct file *filp, char *buffer,
                                size_t length, loff_t *offset)
{
    char main_magic[16] = {0};
    struct mxbl *mxbl = priv_to_mxbl(filp);

    mxbl_rd_buffer(mxbl->mmio, MXBL_MAIN_MAGIC, main_magic, sizeof(main_magic));
    length = min(length, sizeof(main_magic));
    copy_to_user(buffer, main_magic, length);

    return (ssize_t) length;
}

static ssize_t mxbl_dev_write(struct file *filp, const char *buffer,
                                size_t length, loff_t *offset)
{
    int error;
    void *image;
    struct kmem_cache *cachep;
    struct mxbl *mxbl = priv_to_mxbl(filp);

    cachep = kmem_cache_create(MXBL_DRIVER_NAME, length,
                               MXBL_DMA_ALIGNMENT, 0, NULL);
    if (!cachep) {
        goto error_failed_cache_create;
    }

    image = kmem_cache_alloc(cachep, GFP_KERNEL);
    if (!image) {
        goto error_failed_cache_alloc;
    }

    copy_from_user(image, buffer, length);

    mutex_lock(&mxbl->transfer_lock);
    error = mxbl_first_stage_transfer(mxbl, image, length);
    if (error) {
        goto error_failed_transfer;
    }
    mutex_unlock(&mxbl->transfer_lock);

    kmem_cache_free(cachep, image);
    kmem_cache_destroy(cachep);

    return (ssize_t) length;

error_failed_transfer:
    kmem_cache_free(cachep, image);
error_failed_cache_alloc:
    kmem_cache_destroy(cachep);
error_failed_cache_create:
    mxbl_error("failed to perform transfer\n");

    return -1;
}

static struct file_operations mxbl_chrdev_fops = {
    .owner   = THIS_MODULE,
    .open    = mxbl_dev_open,
    .release = mxbl_dev_release,
    .read    = mxbl_dev_read,
    .write   = mxbl_dev_write,
};

char* mxbl_devnode(struct device *dev, umode_t *mode)
{
    if (mode) {
        *mode = 0666;
    }
    return NULL;
}

int mxbl_chrdev_init(void)
{
    int error;

    mxbl_class = class_create(THIS_MODULE, MXBL_CLASS_NAME);
    error = IS_ERR(mxbl_class);
    if (error) {
        error = PTR_ERR(mxbl_class);
        mxbl_error("failed to register device class (%d)\n", error);
        return error;
    }

    error = alloc_chrdev_region(&mxbl_devno, 0, minors, MXBL_DRIVER_NAME);
    if (error) {
        mxbl_error("error registering the char device (%d)\n", error);
        return error;
    }

    mxbl_class->devnode = mxbl_devnode;

    return 0;
}

int mxbl_chrdev_add(struct mxbl *mxbl)
{
    if (!mxbl->chrdev_added) {
        int error;
        dev_t devno;

        cdev_init(&mxbl->cdev, &mxbl_chrdev_fops);
        mxbl->cdev.owner = THIS_MODULE;
        kobject_set_name(&mxbl->cdev.kobj, "%s", MXBL_DRIVER_NAME);

        devno = MKDEV(MAJOR(mxbl_devno), mxbl->unit);

        mxbl->chrdev = device_create(mxbl_class, NULL, devno, mxbl, "%s:%s",
                                  MXBL_DRIVER_NAME, pci_name(MXBL_TO_PCI(mxbl)));
        error = IS_ERR(mxbl->chrdev);
        if (error) {
            error = PTR_ERR(mxbl->chrdev);
            mxbl_error("failed to register the device %s:%s (%d)\n",
                        MXBL_DRIVER_NAME, pci_name(MXBL_TO_PCI(mxbl)), error);
            return error;
        }

        error = cdev_add(&mxbl->cdev, devno, 1);
        if (error) {
            mxbl_error("failed to add device %s:%s\n",
                        MXBL_DRIVER_NAME, pci_name(MXBL_TO_PCI(mxbl)));
            goto free_device;
        }

        mxbl->chrdev_added = 1;

        return 0;
    }

    return -EEXIST;

free_device:
    device_destroy(mxbl_class, mxbl->chrdev->devt);
    return -1;
}

void mxbl_chrdev_remove(struct mxbl *mxbl)
{
    if (mxbl->chrdev_added) {
        mxbl->chrdev_added = 0;
        device_destroy(mxbl_class, mxbl->chrdev->devt);
        cdev_del(&mxbl->cdev);
    }
}

int mxbl_chrdev_exit(void)
{
    if (mxbl_devno) {
        unregister_chrdev_region(mxbl_devno, minors);
        mxbl_devno = 0;
    }

    class_destroy(mxbl_class);

    return 0;
}
