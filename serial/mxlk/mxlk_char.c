/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Character device infrastructure
 *
 * Copyright (C) 2018 - 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/poll.h>

#include "mxlk_char.h"
#include "mxlk_core.h"
#include "mxlk_ioctl.h"

#define MXLK_DEVICE_NAME MXLK_DRIVER_NAME
#define MXLK_CLASS_NAME  MXLK_DRIVER_NAME

/* These fields are valid the entire time the module is loaded, before any PCIe
 * devices are probed and added. So it is OK to have one copy only. */
static dev_t mxlk_devno = 0;
static int minors = MXLK_MAX_DEVICES * MXLK_NUM_INTERFACES;
static struct class *mxlk_class = NULL;

#define priv_to_interface(filp) ((struct mxlk_interface *)filp->private_data)

static int mxlk_dev_open(struct inode *inode, struct file *filp)
{
    struct mxlk_interface *inf = container_of(inode->i_cdev,
                                              struct mxlk_interface, cdev);
    filp->private_data = inf;

    return mxlk_core_open(inf);
}

static int mxlk_dev_release(struct inode *inode, struct file *filp)
{
    struct mxlk_interface *inf = container_of(inode->i_cdev,
                                              struct mxlk_interface, cdev);
    filp->private_data = NULL;

    return mxlk_core_close(inf);
}

static ssize_t mxlk_dev_read(struct file *filp, char *buffer,
                             size_t len, loff_t *offset)
{
    struct mxlk_interface *inf = priv_to_interface(filp);

    return mxlk_core_read(inf, (void *) buffer, len);
}

static ssize_t mxlk_dev_write(struct file *filp, const char *buffer,
                              size_t len, loff_t *offset)
{
    struct mxlk_interface *inf = priv_to_interface(filp);

    return mxlk_core_write(inf, (void *) buffer, len);
}

static unsigned int mxlk_dev_poll(struct file *filp,
                                  struct poll_table_struct *wait)
{
    struct mxlk_interface *inf = priv_to_interface(filp);
    unsigned int mask = 0;

    /* Add ourselves to the wait */
    poll_wait(filp, &inf->rd_waitq, wait);
    poll_wait(filp, &inf->mxlk->wr_waitq, wait);

    if (mxlk_core_read_data_available(inf)) {
       mask |= POLLIN | POLLRDNORM;
    }

    if (mxlk_core_write_buffer_available(inf->mxlk)) {
       mask |= POLLOUT | POLLWRNORM;
    }

    return mask;
}

static long mxlk_dev_ioctl(struct file *filp, unsigned int cmd,
                           unsigned long arg)
{
    struct mxlk_interface *inf = priv_to_interface(filp);
    struct mxlk_boot_param boot_param;
    enum mxlk_fw_status fw_status = MXLK_FW_STATUS_USER_APP;
    char enumtoStr[][256] = {{"BOOTLOADER"},
                             {"USER_APPLICATION"},
                             {"UNKNOWN_STATE"}};
    int error = 0;

    switch (cmd) {
        case MXLK_RESET_DEV:
            mx_info("resetting MX device %s\n", inf->mxlk->name);
            return mxlk_core_reset_dev(inf->mxlk);
        case MXLK_BOOT_DEV:
            mx_info("loading image to MX device %s\n", inf->mxlk->name);
            error = copy_from_user(&boot_param, (char *)arg, sizeof(boot_param));
            if (error) {
                mx_err("failed to copy from user %d/%zu\n", error, sizeof(boot_param));
            }
            return mxlk_core_boot_dev(inf->mxlk,
                                      boot_param.buffer, boot_param.length);
        case MXLK_STATUS_DEV:
            mxlk_get_dev_status(inf->mxlk, &fw_status);
            mx_info("Device status %s\n", enumtoStr[(int)fw_status]);
            error = copy_to_user((int*)arg, &fw_status, sizeof(fw_status));
            if (error) {
                mx_err("failed to copy to user %d/%zu\n", error, sizeof(fw_status));
            }
            return 0;
        default:
            mx_err("wrong ioctl command (0x%x)\n", cmd);
            return -EPERM;
    }
}

static struct file_operations mxlk_chrdev_fops = {
    .owner   = THIS_MODULE,
    .open    = mxlk_dev_open,
    .release = mxlk_dev_release,
    .read    = mxlk_dev_read,
    .write   = mxlk_dev_write,
    .poll    = mxlk_dev_poll,
    .unlocked_ioctl = mxlk_dev_ioctl,
};

int mxlk_chrdev_add(struct mxlk_interface *i)
{
    int error;
    dev_t devno;

    cdev_init(&i->cdev, &mxlk_chrdev_fops);
    i->cdev.owner = THIS_MODULE;
    kobject_set_name(&i->cdev.kobj, "%s", i->mxlk->name);

    devno = MKDEV(MAJOR(mxlk_devno),
                  (i->mxlk->unit * MXLK_NUM_INTERFACES) + i->id);

    /* register the device driver */
    i->dev = device_create(mxlk_class, NULL, devno, i, "%s:%d",
                           i->mxlk->name, i->id);
    error = IS_ERR(i->dev);
    if (error) {
        mx_err("failed to register the device %s:%d\n", i->mxlk->name, i->id);
        return error;
    }

    error = cdev_add(&i->cdev, devno, 1);
    if (error) {
        mx_err("failed to add device %s:%d\n", i->mxlk->name, i->id);
        goto free_device;
    }

    mx_dbg("device %s:%d created successfully\n", i->mxlk->name, i->id);

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

char *mxlk_devnode(struct device *dev, umode_t *mode)
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
        mx_err("failed to register device class (%d)\n", error);
        return error;
    }

    error = alloc_chrdev_region(&mxlk_devno, 0, minors, MXLK_DRIVER_NAME);
    if (error) {
        mx_err("error registering the char device (%d)\n", error);
        return error;
    }

    mxlk_class->devnode = mxlk_devnode;

    return 0;
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
