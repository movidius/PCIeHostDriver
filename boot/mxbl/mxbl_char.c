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

#include "mx_boot.h"

#define MXBL_CLASS_NAME MXBL_DRIVER_NAME

static dev_t mxbl_devno = 0;
static int minors = MXBL_MAX_DEVICES;
static struct class *mxbl_class = NULL;

#define priv_to_mxbl(filp) ((struct mxbl *) filp->private_data)

static int mxbl_dev_open(struct inode *inode, struct file *filp)
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
    char main_magic[MX_MM_LEN] = {0};
    struct mxbl *mxbl = priv_to_mxbl(filp);

    mx_rd_buf(mxbl->mmio, MX_MAIN_MAGIC, main_magic, sizeof(main_magic));
    length = min(length, sizeof(main_magic));
    int error = copy_to_user(buffer, main_magic, length);
    if (error) {
        mx_err("failed to copy to user %d/%zu\n", error, length);
    }

    return (ssize_t) length;
}

static ssize_t mxbl_dev_write(struct file *filp, const char *buffer,
                              size_t length, loff_t *offset)
{
    struct mxbl *mxbl = priv_to_mxbl(filp);
    int error;

    error = mx_boot_load_image(&mxbl->mx_dev, buffer, length, true);
    if (error) {
        return error;
    }

    return (ssize_t) length;
}

static struct file_operations mxbl_chrdev_fops = {
    .owner   = THIS_MODULE,
    .open    = mxbl_dev_open,
    .release = mxbl_dev_release,
    .read    = mxbl_dev_read,
    .write   = mxbl_dev_write,
};

char *mxbl_devnode(struct device *dev, umode_t *mode)
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
        mx_err("failed to register device class (%d)\n", error);
        return error;
    }

    error = alloc_chrdev_region(&mxbl_devno, 0, minors, MXBL_DRIVER_NAME);
    if (error) {
        mx_err("error registering the char device (%d)\n", error);
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
            mx_err("failed to register the device %s:%s (%d)\n",
                   MXBL_DRIVER_NAME, pci_name(MXBL_TO_PCI(mxbl)), error);
            return error;
        }

        error = cdev_add(&mxbl->cdev, devno, 1);
        if (error) {
            mx_err("failed to add device %s:%s\n",
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
