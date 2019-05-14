/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Main types and defines
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXLK_HEADER_
#define MXLK_HEADER_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/mempool.h>
#include <linux/slab_def.h>
#include <linux/dma-mapping.h>
#include <linux/cache.h>

#include "mxlk_common.h"

#define MXLK_MAX_DEVICES    (1)
#define MXLK_DRIVER_NAME    "mxlk"
#define MXLK_DRIVER_DESC    "Intel(R) MyriadX PCIe xLink"
#define MXLK_MAX_NAME_LEN   (32)

#define MXLK_TO_PCI(mxlk) ((mxlk)->pci)
#define MXLK_TO_DEV(mxlk) (&(mxlk)->pci->dev)

struct mxlk_pipe {
    u32 old;
    u32 ndesc;
    u32 *head;
    u32 *tail;
    struct mxlk_transfer_desc *tdr;
};

struct mxlk_buf_desc {
    struct mxlk_buf_desc *next;
    void  *head;
    size_t true_len;
    void  *data;
    size_t length;
    int interface;
};

struct mxlk_dma_desc {
    struct mxlk_buf_desc *bd;
    dma_addr_t phys;
    size_t     length;
};

struct mxlk_stream {
    int busy;
    size_t frag;
    struct mxlk_pipe pipe;
    struct mxlk_dma_desc *ddr;
};

struct mxlk_list {
    spinlock_t lock;
    size_t bytes;
    size_t buffers;
    struct mxlk_buf_desc *head;
    struct mxlk_buf_desc *tail;
};

struct mxlk_interface {
    int id;
    int opened;
    struct mxlk *mxlk;
    struct cdev cdev;
    struct device *dev;
    struct mutex rlock;
    struct mutex wlock;
    struct mxlk_list read;
    struct mxlk_buf_desc *partial_read;
};

struct mxlk_stats {
    struct {
        size_t pkts;
        size_t bytes;
    }tx_krn, rx_krn, tx_usr, rx_usr;
    size_t doorbells;
    size_t interrupts;
    size_t rx_event_runs;
    size_t tx_event_runs;
};

struct mxlk {
    int status;
    struct pci_dev *pci;    /* pointer to pci device provided by probe */
    void __iomem   *mmio;   /* kernel virtual address to MMIO (BAR2) */

    struct workqueue_struct *wq;

    int  unit;
    char name[MXLK_MAX_NAME_LEN];

    struct mxlk_interface interfaces[MXLK_NUM_INTERFACES];

    size_t fragment_size;
    struct mxlk_cap_txrx *txrx;
    struct mxlk_stream tx;
    struct mxlk_stream rx;

    struct mxlk_list write;
    struct mxlk_list rx_pool;
    struct mxlk_list tx_pool;

    struct work_struct rx_event;
    struct work_struct tx_event;
    struct work_struct send_doorbell;

    struct device_attribute debug;
    struct mxlk_stats stats;
    struct mxlk_stats stats_old;
};

#endif
