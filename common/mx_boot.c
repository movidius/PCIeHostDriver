/*******************************************************************************
 *
 * MX PCIe boot infrastructure
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mx_boot.h"

#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include "mx_common.h"
#include "mx_pci.h"
#include "mx_mmio.h"
#include "mx_print.h"
#include "mx_second_stage_bl_image.h"

#define MX_BOOT_OBJ_NAME "mx_boot"

#define MF_STATUS_READY     ((u32) 0x00000000)
#define MF_STATUS_PENDING   ((u32) 0xFFFFFFFF)
#define MF_STATUS_STARTING  ((u32) 0x55555555)
#define MF_STATUS_DMA_DONE  ((u32) 0xDDDDDDDD)
#define MF_STATUS_BOOT      ((u32) 0xBBBBBBBB)
#define MF_STATUS_DMA_ERROR ((u32) 0xDEADAAAA)
#define MF_STATUS_INVALID   ((u32) 0xDEADFFFF)

/* Myriad Port Logic DMA registers and configuration values */
#define MX_DMA_READ_ENGINE_EN   (0x99C)
#define MX_DMA_VIEWPORT_SEL     (0xA6C)
#define MX_DMA_CH_CONTROL1      (0xA70)
#define MX_DMA_RD_EN            (0x00000001)
#define MX_DMA_VIEWPORT_CHAN    (0)
#define MX_DMA_VIEWPORT_RD      (0x80000000)
#define MX_CH_CONTROL1_LIE      (0x00000008)

#define priv_to_mx_dev(filp) ((struct mx_dev *)filp->private_data)

static void enable_mx_rdma(struct pci_dev *pci);
static int wait_for_transfer_completion(struct mx_dev *mx_dev,
                                        enum mx_opmode curr_mod);
static int boot_image_transfer(struct mx_dev *mx_dev, void *image,
                               size_t length, bool boot_now);

static void enable_mx_rdma(struct pci_dev *pci)
{
    pci_write_config_dword(pci, MX_DMA_VIEWPORT_SEL,
                           MX_DMA_VIEWPORT_RD | MX_DMA_VIEWPORT_CHAN);
    pci_write_config_dword(pci, MX_DMA_CH_CONTROL1, MX_CH_CONTROL1_LIE);
    pci_write_config_dword(pci, MX_DMA_READ_ENGINE_EN, MX_DMA_RD_EN);
}

void mx_boot_status_update_int_enable(struct mx_dev *mx_dev)
{
    u32 enable;

    enable = field_set(MX_INT_STATUS_UPDATE, 1);

    mx_wr32(mx_dev->mmio, MX_INT_ENABLE, enable);
    mx_wr32(mx_dev->mmio, MX_INT_MASK,  ~enable);
}

void mx_boot_status_update_int_disable(struct mx_dev *mx_dev)
{
    mx_wr32(mx_dev->mmio, MX_INT_ENABLE, 0);
}

void mx_boot_init(struct mx_dev *mx_dev)
{
    /* Workaround for potential bus resets during enumeration process. */
    enable_mx_rdma(mx_dev->pci);

    mutex_init(&mx_dev->transfer_lock);
}

void mx_boot_cleanup(struct mx_dev *mx_dev)
{
    mutex_destroy(&mx_dev->transfer_lock);
}

static int wait_for_transfer_completion(struct mx_dev *mx_dev,
                                        enum mx_opmode curr_mode)
{
    u32 mf_ready;
    int start_tmo = 1500; /* 1500 ms */
    int pending_tmo = 100; /* 100 ms */

    while (1) {
        /* Return if mode changes from boot->loader or loader->app. */
        if (curr_mode != mx_get_opmode(mx_dev)) {
            mx_dbg("opmode changed. DMA complete \n");
            return 0;
        }

        mf_ready = mx_rd32(mx_dev->mmio, MX_MF_READY);
        switch (mf_ready) {
            case MF_STATUS_READY :
                break;
            case MF_STATUS_DMA_DONE :
                mx_dbg("Got DMA_DONE \n");
                return 0;
            case MF_STATUS_PENDING :
                if (pending_tmo--) {
                    msleep(1);
                } else {
                    mx_err("pending status timed out\n");
                    return -ETIME;
                }
                break;
            case MF_STATUS_STARTING :
                if (start_tmo--) {
                    msleep(1);
                } else {
                    mx_err("starting status timed out\n");
                    return -ETIME;
                }
                break;
            case MF_STATUS_DMA_ERROR :
            case MF_STATUS_INVALID :
                mx_err("error status %08X\n", mf_ready);
                return -EPROTO;
            default :
                mx_err("unknown status %08X\n", mf_ready);
                return -EPROTO;
        }
    }
}

static int boot_image_transfer(struct mx_dev *mx_dev, void *image,
                               size_t length, bool boot_now)
{
    int error;
    u32 ready;
    dma_addr_t start;
    enum mx_opmode curr_mode;

    /* Get the operational mode before initiating the transfer */
    curr_mode = mx_get_opmode(mx_dev);
    /* Check MF_READY to make sure VPU is ready. Initial state of MF_READY is
     * MF_STATUS_READY. While loading multiple chunks it is MF_STATUS_DMA_DONE */
    ready = mx_rd32(mx_dev->mmio, MX_MF_READY);
    if ((ready != MF_STATUS_READY) && (ready != MF_STATUS_DMA_DONE)) {
        mx_err("boot_image_transfer: not in ready mode %x\n", ready);
        return -EIO;
    }

    /* Get DMA address */
    start = dma_map_single(&mx_dev->pci->dev, image, length, DMA_TO_DEVICE);
    error = dma_mapping_error(&mx_dev->pci->dev, start);
    if (error) {
        mx_err("boot_image_transfer: error mapping dma %d\n", error);
        return error;
    }

    /* Setup MMIO registers for transfer */
    mx_wr64(mx_dev->mmio, MX_MF_START,  (u64) start);
    mx_wr32(mx_dev->mmio, MX_MF_LENGTH, (u32) length);
    mx_wr32(mx_dev->mmio, MX_MF_READY,  MF_STATUS_PENDING);

    /* Block until complete or error */
    error = wait_for_transfer_completion(mx_dev, curr_mode);
    dma_unmap_single(&mx_dev->pci->dev, start, length, DMA_TO_DEVICE);

    if ((true == boot_now) && (curr_mode == MX_OPMODE_LOADER)) {
        /* Setup MMIO registers for boot for secondary loader */
        mx_wr32(mx_dev->mmio, MX_MF_READY,  MF_STATUS_BOOT);
    }

    return error;
}

int mx_boot_load_image(struct mx_dev *mx_dev, const char *buffer, size_t length,
                       bool user_land_buffer)
{
    int error;
    void *image;
    char *usr_image;
    size_t size_left;
    size_t chunk_size;
    int boot_now;
    struct kmem_cache *cachep;
    size_t bl_length;
    enum mx_opmode op_mode;

    if (length > MAX_CONT_BUFFER_SIZE_LINUX) {
        /* If the buffer passed by the user is greater than 4MB, then need to
         * boot the Myriad X with a second stage loader, that accepts the boot
         * image in chunks over PCIe */
        mx_dbg("Boot image size greater than 4MB; loading the secondary "
               "bootloader\n");
        bl_length = sizeof(mx_second_stage_bl_image);
        error = mx_boot_load_image(mx_dev, (void*)mx_second_stage_bl_image,
                                   bl_length, false);
        if (error != bl_length) {
            mx_err("Failed to load second stage bootloader\n");
            goto error_failed_second_stage_bl;
        }
        /* Confirm the MyriadX is in Loader mode before proceeding.
         * wait_for_transfer_completion ensures Myriad is in a mode other than
         * BOOT mode. If the current mode is not LOADER mode, then something bad
         * has happened. This should never happen unless the secondary
         * boot-loader image has changed */
        op_mode = mx_get_opmode(mx_dev);
        if (MX_OPMODE_LOADER != op_mode) {
            mx_err("Opmode not in LOADER mode, current mode is %d\n", op_mode);
            goto error_failed_second_stage_bl;
        }
    }

    size_left = length;
    usr_image = (char*)buffer;
    while(size_left > 0) {
        chunk_size = (size_left > MAX_CONT_BUFFER_SIZE_LINUX) ?
                     MAX_CONT_BUFFER_SIZE_LINUX : size_left;

        /* In case of secondary loader, boot in case the last chunk */
        boot_now = (size_left - chunk_size) ? 0 : 1;

        cachep = kmem_cache_create(MX_BOOT_OBJ_NAME, chunk_size,
                                   MX_DMA_ALIGNMENT, 0, NULL);
        if (!cachep) {
            goto error_failed_cache_create;
        }

        image = kmem_cache_alloc(cachep, GFP_KERNEL);
        if (!image) {
            goto error_failed_cache_alloc;
        }

        if (user_land_buffer) {
            copy_from_user(image, usr_image, chunk_size);
        } else {
            memcpy(image, usr_image, chunk_size);
        }

        mx_dbg("Transferring %ld of %ld bytes \n", chunk_size, size_left);
        mutex_lock(&mx_dev->transfer_lock);
        error = boot_image_transfer(mx_dev, image, chunk_size, boot_now);
        mutex_unlock(&mx_dev->transfer_lock);
        if (error) {
            goto error_failed_transfer;
        }

        kmem_cache_free(cachep, image);
        kmem_cache_destroy(cachep);

        usr_image += chunk_size;
        size_left -= chunk_size;
    }

    return (ssize_t) length;

    error_failed_transfer:
        kmem_cache_free(cachep, image);
    error_failed_cache_alloc:
        kmem_cache_destroy(cachep);
    error_failed_second_stage_bl:
    error_failed_cache_create:
        mx_err("failed to perform transfer\n");

    return -1;
}
