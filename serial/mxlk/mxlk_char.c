/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Character device infrastructure
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include "mxlk_char.h"
#include "mxlk_print.h"
#include "mxlk_core.h"

#define MXLK_DEVICE_NAME MXLK_DRIVER_NAME
#define MXLK_CLASS_NAME  MXLK_DRIVER_NAME

/*
These fields are valid the entire time the module is loaded,
before any PCIe devices are probed and added. So it is OK to have one
static copy
*/
static dev_t mxlk_devno = 0;
static int minors = MXLK_MAX_DEVICES * MXLK_NUM_INTERFACES;
static struct class *mxlk_class = NULL;

#define priv_to_interface(filp) ((struct mxlk_interface *) filp->private_data)

static int mxlk_dev_open(struct inode *inode, struct file* filp)
{
    struct mxlk_interface *inf = container_of(inode->i_cdev, struct mxlk_interface, cdev);
    filp->private_data = inf;

    return mxlk_core_open(inf);
}

static int mxlk_dev_release(struct inode *inode, struct file *filp)
{
    struct mxlk_interface *inf = container_of(inode->i_cdev, struct mxlk_interface, cdev);
    filp->private_data = NULL;

    return mxlk_core_close(inf);
}

static ssize_t mxlk_dev_read(struct file *filp, char *buffer, size_t len, loff_t *offset)
{
    struct mxlk_interface *inf = priv_to_interface(filp);

    return mxlk_core_read(inf, (void *) buffer, len);
}

static ssize_t mxlk_dev_write(struct file *filp, const char *buffer, size_t len, loff_t *offset)
{
    struct mxlk_interface *inf = priv_to_interface(filp);

    return mxlk_core_write(inf, (void *) buffer, len);
}

static struct file_operations mxlk_chrdev_fops = {
    .owner   = THIS_MODULE,
    .open    = mxlk_dev_open,
    .release = mxlk_dev_release,
    .read    = mxlk_dev_read,
    .write   = mxlk_dev_write,
};

char* mxlk_devnode(struct device *dev, umode_t *mode)
{
    if (mode) {
        *mode = 0666;
    }
    return NULL;
}

/* This function initializes the mxlk character device */
int mxlk_chrdev_init(void)
{
    int error;

    mxlk_class = class_create(THIS_MODULE, MXLK_CLASS_NAME);
    if (IS_ERR(mxlk_class)) {
        error = PTR_ERR(mxlk_class);
        mxlk_error("failed to register device class (%d)\n", error);
        return error;
    }

    error = alloc_chrdev_region(&mxlk_devno, 0, minors, MXLK_DRIVER_NAME);
    if (error) {
        mxlk_error("error registering the char device (%d)\n", error);
        return error;
    }

    mxlk_class->devnode = mxlk_devnode;

    return 0;
}

int mxlk_chrdev_add(struct mxlk_interface *i)
{
    int error;
    dev_t devno;

    cdev_init(&i->cdev, &mxlk_chrdev_fops);
    i->cdev.owner = THIS_MODULE;
    kobject_set_name(&i->cdev.kobj, "%s", i->mxlk->name);

    devno = MKDEV(MAJOR(mxlk_devno), (i->mxlk->unit * MXLK_NUM_INTERFACES) + i->id);

    // register the device driver
    i->dev = device_create(mxlk_class, NULL, devno, i, "%s:%d",
                            i->mxlk->name, i->id);
    error = IS_ERR(i->dev);
    if (error) {
        mxlk_error("failed to register the device %s:%d\n", i->mxlk->name, i->id);
        return error;
    }

    error = cdev_add(&i->cdev, devno, 1);
    if (error) {
        mxlk_error("failed to add device %s:%d\n", i->mxlk->name, i->id);
        goto free_device;
    }

    mxlk_debug("device %s:%d created successfully\n", i->mxlk->name, i->id);

    return 0;

free_device:
    device_destroy(mxlk_class, i->dev->devt);
    return -1;
}

void mxlk_chrdev_remove(struct mxlk_interface *i)
{
    device_destroy(mxlk_class, i->dev->devt);
    cdev_del(&i->cdev);
}

int mxlk_chrdev_exit(void)
{
    if (mxlk_devno) {
        unregister_chrdev_region(mxlk_devno, minors);
        mxlk_devno = 0;
    }

    class_destroy(mxlk_class);

    return 0;
}
