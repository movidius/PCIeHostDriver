/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/
#include <linux/bitops.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "mxbl.h"
#include "mxbl_pci.h"
#include "mxbl_char.h"
#include "mxbl_bspec.h"
#include "mxbl_print.h"
#ifdef EMBEDDED_BINARY
    #include "mxbl_first_stage_image.h"
#endif

#define MF_STATUS_READY     ((u32) 0x00000000)
#define MF_STATUS_PENDING   ((u32) 0xFFFFFFFF)
#define MF_STATUS_STARTING  ((u32) 0x55555555)
#define MF_STATUS_DMA_ERROR ((u32) 0xDEADAAAA)
#define MF_STATUS_INVALID   ((u32) 0xDEADFFFF)

/* Myriad Port Logic DMA registers and configuration values */
#define MXBL_DMA_READ_ENGINE_EN   (0x99C)
#define MXBL_DMA_VIEWPORT_SEL     (0xA6C)
#define MXBL_DMA_CH_CONTROL1      (0xA70)
#define MXBL_DMA_RD_EN            (0x00000001)
#define MXBL_DMA_VIEWPORT_CHAN    (0)
#define MXBL_DMA_VIEWPORT_RD      (0x80000000)
#define MXBL_CH_CONTROL1_LIE      (0x00000008)
/* Myriad Port Logic Vendor Specific DLLP register and configuration value */
#define MXBL_VENDOR_SPEC_DLLP     (0x704)
#define MXBL_RESET_DEV            (0xDEADDEAD)

/* Time given to MX device to detect and perform reset, in milliseconds.
 * This value may have to be updated if the polling task's period on device side
 * is changed. */
#define MXBL_DEV_RESET_TIME_MS (1000)

static void mxbl_dev_enable_rdma(struct mxbl *mxbl);

static enum mxbl_opmode mxbl_get_opmode(struct mxbl *mxbl);

static irqreturn_t mxbl_event_isr(int irq, void *args);
static int  mxbl_events_init(struct mxbl *mxbl);
static void mxbl_events_cleanup(struct mxbl *mxbl);
static void mxbl_status_update_handler(struct work_struct *work);

static int mxbl_wait_for_transfer_completion(struct mxbl *mxbl);

static int mxbl_reset_device(struct mxbl *mxbl);

static atomic_t units_found = ATOMIC_INIT(0);

static void mxbl_dev_enable_rdma(struct mxbl *mxbl)
{
    pci_write_config_dword(MXBL_TO_PCI(mxbl), MXBL_DMA_VIEWPORT_SEL,
                           MXBL_DMA_VIEWPORT_RD | MXBL_DMA_VIEWPORT_CHAN);
    pci_write_config_dword(MXBL_TO_PCI(mxbl), MXBL_DMA_CH_CONTROL1,
                           MXBL_CH_CONTROL1_LIE);
    pci_write_config_dword(MXBL_TO_PCI(mxbl), MXBL_DMA_READ_ENGINE_EN,
                           MXBL_DMA_RD_EN);
}

static irqreturn_t mxbl_event_isr(int irq, void *args)
{
    struct mxbl *mxbl = args;
    u32 identity = mxbl_rd32(mxbl->mmio, MXBL_INT_IDENTITY);

    /* Check for source of interrupt */
    if (field_get(MXBL_INT_STATUS_UPDATE, identity)) {
        queue_work(mxbl->wq, &mxbl->status_update);
    }

    /* Report all events handled to VPU */
    mxbl_wr32(mxbl->mmio, MXBL_INT_IDENTITY, 0);

    /* Report MSI handled */
    return IRQ_HANDLED;
}

static int mxbl_events_init(struct mxbl *mxbl)
{
    int irq;
    int error;
    u32 enable;

    INIT_WORK(&mxbl->status_update, mxbl_status_update_handler);

    error = pci_alloc_irq_vectors(MXBL_TO_PCI(mxbl), 1, 1, PCI_IRQ_MSI);
    if (error < 0) {
        mxbl_error("failed to allocate %d MSI vectors - %d\n", 1, error);
        return error;
    }

    irq = pci_irq_vector(MXBL_TO_PCI(mxbl), 0);
    error = request_irq(irq, mxbl_event_isr, 0, MXBL_DRIVER_NAME, mxbl);
    if (error) {
        mxbl_error("failed to request irqs - %d\n", error);
        return error;
    }

    enable = field_set(MXBL_INT_STATUS_UPDATE, 1);

    mxbl_wr32(mxbl->mmio, MXBL_INT_ENABLE, enable);
    mxbl_wr32(mxbl->mmio, MXBL_INT_MASK,  ~enable);

    return 0;
}

static void mxbl_events_cleanup(struct mxbl *mxbl)
{
    int irq;
    enum mxbl_opmode opmode;

    opmode = mxbl_get_opmode(mxbl);
    if (opmode == MXBL_OPMODE_BOOT) {
        mxbl_wr32(mxbl->mmio, MXBL_INT_ENABLE, 0);
    }

    cancel_work_sync(&mxbl->status_update);

    irq = pci_irq_vector(MXBL_TO_PCI(mxbl), 0);
    synchronize_irq(irq);
    free_irq(irq, mxbl);
    pci_free_irq_vectors(MXBL_TO_PCI(mxbl));
}

static void mxbl_status_update_handler(struct work_struct *work)
{
    enum mxbl_opmode opmode;
    struct mxbl *mxbl = container_of(work, struct mxbl, status_update);

    opmode = mxbl_get_opmode(mxbl);
    if (opmode == MXBL_OPMODE_BOOT) {
#ifdef EMBEDDED_BINARY
        int error;
        void *image;
        size_t length;
        struct kmem_cache *cachep;

        length = sizeof(mxbl_first_stage_image);
        cachep = kmem_cache_create(MXBL_DRIVER_NAME, length,
                                   MXBL_DMA_ALIGNMENT, 0, NULL);
        if (!cachep) {
            goto error_failed_cache_create;
        }

        image = kmem_cache_alloc(cachep, GFP_KERNEL);
        if (!image) {
            goto error_failed_cache_alloc;
        }

        memcpy(image, mxbl_first_stage_image, length);

        mutex_lock(&mxbl->transfer_lock);
        error = mxbl_first_stage_transfer(mxbl, image, length);
        if (error) {
            goto error_failed_transfer;
        }
        mutex_unlock(&mxbl->transfer_lock);

        kmem_cache_free(cachep, image);
        kmem_cache_destroy(cachep);

        return;

        error_failed_transfer:
            kmem_cache_free(cachep, image);
        error_failed_cache_alloc:
            kmem_cache_destroy(cachep);
        error_failed_cache_create:
            mxbl_error("failed to perform transfer\n");
#else
        mxbl_chrdev_add(mxbl);
#endif
    }
}

static enum mxbl_opmode mxbl_get_opmode(struct mxbl *mxbl)
{
    int error;
    u8 main_magic[16] = {0};

    mxbl_rd_buffer(mxbl->mmio, MXBL_MAIN_MAGIC, main_magic, sizeof(main_magic));

    error = memcmp(main_magic, MXBL_MM_BOOT_STR, strlen(MXBL_MM_BOOT_STR));
    if (!error) {
        return MXBL_OPMODE_BOOT;
    }

    error = memcmp(main_magic, MXBL_MM_LOAD_STR, strlen(MXBL_MM_LOAD_STR));
    if (!error) {
        return MXBL_OPMODE_LOADER;
    }

    error = memcmp(main_magic, MXBL_MM_MAIN_STR, strlen(MXBL_MM_MAIN_STR));
    if (!error) {
        return MXBL_OPMODE_APP;
    }

    return MXBL_OPMODE_UNKNOWN;
}

static int mxbl_wait_for_transfer_completion(struct mxbl *mxbl)
{
    u32 mf_ready;
    enum mxbl_opmode opmode;
    int start_tmo = 1500; /* 1500 ms */
    int pending_tmo = 100; /* 100 ms */

    while (1) {
        opmode = mxbl_get_opmode(mxbl);
        if (opmode != MXBL_OPMODE_BOOT) {
            return 0;
        }

        mf_ready = mxbl_rd32(mxbl->mmio, MXBL_MF_READY);
        switch (mf_ready) {
            case MF_STATUS_READY :
                break;
            case MF_STATUS_PENDING :
                if (pending_tmo--) {
                    msleep(1);
                } else {
                    mxbl_error("pending status timed out\n");
                    return -ETIME;
                }
                break;
            case MF_STATUS_STARTING :
                if (start_tmo--) {
                    msleep(1);
                } else {
                    mxbl_error("starting status timed out\n");
                    return -ETIME;
                }
                break;
            case MF_STATUS_DMA_ERROR :
            case MF_STATUS_INVALID :
                mxbl_error("error status %08X\n", mf_ready);
                return -EPROTO;
            default :
                mxbl_error("unknown status %08X\n", mf_ready);
                return -EPROTO;
        }
    }
}

int mxbl_bspec_init(struct mxbl *mxbl, struct pci_dev *pdev,
                    struct workqueue_struct * wq)
{
    int error = 0;

    if (!wq) {
        return -EINVAL;
    }

    if (!pdev) {
        return -EINVAL;
    }

    mxbl->wq  = wq;
    mxbl->pci = pdev;

    error = mxbl_pci_init(mxbl);
    if (error) {
        goto error_pci_init;
    }

    error = mxbl_events_init(mxbl);
    if (error) {
        goto error_events_init;
    }

    if (mxbl_get_opmode(mxbl) == MXBL_OPMODE_BOOT) {
        /* Device is in boot mode already: First application load. */

        /* Because of the device potentially having its PCIe controller reset
         * during the host boot process, we need to re-apply the DMA settings
         * through the device's configuration space. Needed on first load only. */
        mxbl_dev_enable_rdma(mxbl);
    } else {
        /* Device is not in boot mode: Some application was already running. */
        /* In this case we need to reset the device to bring it back to boot
         * mode. */

        /* Clean up events before the reset as we will have to re-init them
         * after the reset so that we can read operation mode again and check
         * that the reset was successful. */
        mxbl_events_cleanup(mxbl);
        /* Reset the device.
         * Device is locked already as we are inside the probing process. */
        error = mxbl_reset_device(mxbl);
        if (error) {
            goto error_reset;
        }
        /* Re-init events so that we can read operation mode again. */
        mxbl_events_init(mxbl);
        /* The device should now be back to boot mode. */
        if (mxbl_get_opmode(mxbl) != MXBL_OPMODE_BOOT) {
            error = -EIO;
            goto error_opmode;
        }
    }

    mxbl->unit = atomic_fetch_add(1, &units_found);
    if (mxbl->unit >= MXBL_MAX_DEVICES) {
        goto error_max_devices;
    }

    mutex_init(&mxbl->transfer_lock);

    return 0;

error_max_devices:
error_opmode:
    mxbl_events_cleanup(mxbl);
    /* Try to clean up chrdev as well in case we received an interrupt */
    mxbl_chrdev_remove(mxbl);

error_events_init:
error_reset:
   mxbl_pci_cleanup(mxbl);

error_pci_init:
    mxbl_error("failed to init (%d)\n", error);
    return error;
}

void mxbl_bspec_cleanup(struct mxbl *mxbl)
{
    mxbl_chrdev_remove(mxbl);
    mxbl_events_cleanup(mxbl);
    mxbl_pci_cleanup(mxbl);
}

int mxbl_first_stage_transfer(struct mxbl *mxbl, void *image, size_t length)
{
    int error;
    u32 ready;
    dma_addr_t start;
    enum mxbl_opmode opmode;

    /* Make sure we are in the correct operational mode */
    opmode = mxbl_get_opmode(mxbl);
    if (opmode != MXBL_OPMODE_BOOT) {
        return -EPERM;
    }

    /* Check MF_READY to make sure VPU is ready */
    ready = mxbl_rd32(mxbl->mmio, MXBL_MF_READY);
    if (ready != MF_STATUS_READY) {
        return -EIO;
    }

    /* Get DMA address */
    start = dma_map_single(MXBL_TO_DEV(mxbl), image, length, DMA_TO_DEVICE);
    error = dma_mapping_error(MXBL_TO_DEV(mxbl), start);
    if (error) {
        return error;
    }

    /* Setup MMIO registers for transfer */
    mxbl_wr64(mxbl->mmio, MXBL_MF_START,  (u64) start);
    mxbl_wr32(mxbl->mmio, MXBL_MF_LENGTH, (u32) length);
    mxbl_wr32(mxbl->mmio, MXBL_MF_READY,  MF_STATUS_PENDING);

    /* Block until complete or error */
    error = mxbl_wait_for_transfer_completion(mxbl);
    dma_unmap_single(MXBL_TO_DEV(mxbl), start, length, DMA_TO_DEVICE);

    return error;
}

/* NOTE: This process must be executed under device lock. It is the caller's
 * responsibility to ensure the device is locked before calling this function. */
static int mxbl_reset_device(struct mxbl *mxbl) {
    /* Save the device's context because its PCIe controller will be reset in
     * the process. */
    mxbl_pci_dev_ctx_save(mxbl);

    /* We assume there are no transactions pending as the host driver
     * corresponding to the device application currently has now been removed,
     * putting an end to all potential transactions. */

    /* Write the magic into Vendor Specific DLLP register to trigger the device
     * reset. */
    pci_write_config_dword(MXBL_TO_PCI(mxbl), MXBL_VENDOR_SPEC_DLLP,
                           MXBL_RESET_DEV);

    /* Give some time to the device to trigger and complete the reset. */
    msleep(MXBL_DEV_RESET_TIME_MS);

    /* Check that the device is up again before restoring the full PCI context. */
    if (!mxbl_pci_dev_id_valid(mxbl)) {
        return -EIO;
    }

    /* Restore the device's context. */
    mxbl_pci_dev_ctx_restore(mxbl);

    return 0;
}
