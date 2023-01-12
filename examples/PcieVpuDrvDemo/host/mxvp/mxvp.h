/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: General API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXVP_HEADER_
#define MXVP_HEADER_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/atomic.h>

#include "mxvp_mem.h"
#include "mxvp_queue.h"

#define MXVP_DRIVER_NAME "mxvp"
#define MXVP_DRIVER_DESC "Intel(R) Myriad X Vision Processor"

typedef struct mxvp_cmd_list_entry *mxvp_command_list;

struct mxvp_cmd_list {
    mxvp_command_list base;
    mxvp_command_list free;
    size_t entries;
    spinlock_t lock;
};

/*
 * Myriad-X Vision Processor (MXVP) device instance control structure
 */
struct mxvp {
    struct pci_dev *pci;    /* pointer to pci device provided by probe */
    void __iomem *mmio;     /* kernel virtual address to MMIO (BAR2) */
    void __iomem *ddr_virt; /* kernel virtual address to VPU DDR (BAR4) */
    u64 ddr_phys;           /* VPU internal DDR start address */
    u32 ddr_size;           /* VPU DDR size in bytes */

    struct mxvp_queue cmdq; /* command queue control object */
    struct mxvp_queue retq; /* reply queue control object */
    struct mxvp_queue dmaq; /* DMA queue control object */
    struct mxvp_queue dmrq; /* DMA reply queue control object */
    struct mxvp_mem   mem;  /* VPU DDR memory manager object */

    struct workqueue_struct *wq;     /* work queue to use for event handling */
    struct work_struct dmrq_update;  /* work object for DMA reply queue event */
    struct work_struct retq_update;  /* work object for reply queue event */
    struct work_struct dmaq_preempt; /* work object for DMA preemption event */
    struct work_struct cmdq_preempt; /* work object for CMD preemption event */
    struct work_struct status_update; /* work object for status update event */

    struct mxvp_cmd_list cmd_list; /* list of command queue entries */
    struct mxvp_cmd_list dma_list; /* list of DMA queue entries */

    long async_pending;    /* pending async command bitfield */
    spinlock_t async_lock; /* spinlock to protect access to async registers */

    atomic_t seqno; /* sequence number count */
};

#endif
