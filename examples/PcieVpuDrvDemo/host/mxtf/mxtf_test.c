/*******************************************************************************
 *
 * Intel Myriad-X Test Framework Driver: Test operations implementation
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/semaphore.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/unistd.h>

#include "mxtf.h"
#include "mxtf_print.h"

#include "mxvp_mmio.h"
#include "mxvp_mem.h"
#include "mxvp_cmd.h"
#include "mxvp_bspec.h"

#define NUM_DMA_DESC (16u)
#define DMA_BUFFER_SIZE  (16 *1024u)
#define DMA_THPT_INTERVAL (10000)

static ssize_t mxtf_test_show(struct device *dev,
                            struct device_attribute *attr, char *buf);
static ssize_t mxtf_test_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count);

struct test_entry {
    struct mxtf * mxtf;
    void (* testfunc)(struct mxtf *mxtf);
    struct work_struct work;
};

static void mxtf_test_executor(struct work_struct *work);

static void mxtf_test_dma_loopback(struct mxtf *mxtf);
static void mxtf_test_multi_dma_loopback(struct mxtf *mxtf);
static void mxtf_test_mem_fill(struct mxtf *mxtf);
static void mxtf_test_fence(struct mxtf *mxtf);
static void mxtf_test_exebuf(struct mxtf *mxtf);

static void mxtf_test_async_cmdq_preempt(struct mxtf *mxtf);
static void mxtf_test_async_dmaq_preempt(struct mxtf *mxtf);
static void mxtf_test_async_cmdq_reset(struct mxtf *mxtf);
static void mxtf_test_async_dmaq_reset(struct mxtf *mxtf);
static void mxtf_test_async_dev_reset(struct mxtf *mxtf);
static void mxtf_test_async_set_power(struct mxtf *mxtf);

static void mxtf_test_throughput(struct mxtf *mxtf);

static struct test_entry test_list[] =
{
    [ 0] = { .testfunc = mxtf_test_dma_loopback },
    [ 1] = { .testfunc = mxtf_test_multi_dma_loopback },
    [ 2] = { .testfunc = mxtf_test_mem_fill },
    [ 3] = { .testfunc = mxtf_test_fence },
    [ 4] = { .testfunc = mxtf_test_exebuf },
    [ 5] = { .testfunc = mxtf_test_async_cmdq_preempt },
    [ 6] = { .testfunc = mxtf_test_async_dmaq_preempt },
    [ 7] = { .testfunc = mxtf_test_async_cmdq_reset },
    [ 8] = { .testfunc = mxtf_test_async_dmaq_reset },
    [ 9] = { .testfunc = mxtf_test_async_dev_reset },
    [10] = { .testfunc = mxtf_test_async_set_power },
    [11] = { .testfunc = mxtf_test_throughput }
};

static u64 read_accumulator;
static u64 write_accumulator;
static u64 read_total_len;
static u64 write_total_len;
static u64 read_jiffies;
static u64 write_jiffies;
static struct delayed_work thpt_work;

int mxtf_test_init(struct mxtf *mxtf)
{
    int testno;

    DEVICE_ATTR(test, S_IWUSR | S_IRUGO, mxtf_test_show, mxtf_test_store);
    mxtf->test = dev_attr_test;
    device_create_file(&mxtf->mxvp.pci->dev, &mxtf->test);

    for (testno = 0; testno < ARRAY_SIZE(test_list); testno++) {
        test_list[testno].mxtf = mxtf;
        INIT_WORK(&test_list[testno].work, mxtf_test_executor);
    }

    return 0;
}

void mxtf_test_cleanup(struct mxtf *mxtf)
{
    device_remove_file(&mxtf->mxvp.pci->dev, &mxtf->test);
}

static ssize_t mxtf_test_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    sprintf(buf,
            "[ 0] DMA loopback\n"
            "[ 1] Multi DMA loopback\n"
            "[ 2] Memory Fill\n"
            "[ 3] CMD Fence, DMA Fence\n"
            "[ 4] Execute buffer\n"
            "[ 5] Async CMDQ Preemption\n"
            "[ 6] Async DMAQ Preemption\n"
            "[ 7] Async CMDQ Reset\n"
            "[ 8] Async DMAQ Reset\n"
            "[ 9] Async Device Reset\n"
            "[10] Async Set Power\n"
            "[11] DMA Throughput\n"
            );

    return strlen(buf);
}

static ssize_t mxtf_test_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
    int error;
    unsigned long testno;

    error = kstrtoul(buf, 10, &testno);
    if (error) {
        mxtf_error("unable to parse test number %s", buf);
        return error;
    }

    if (testno < ARRAY_SIZE(test_list)) {
        schedule_work(&test_list[testno].work);
    } else {
        mxtf_error("invalid test number %lu\n", testno);
    }

    return count;
}

static void mxtf_test_executor(struct work_struct *work)
{
    struct test_entry *entry = container_of(work, struct test_entry, work);

    entry->testfunc(entry->mxtf);
}

static void mxtf_test_dma_loopback_cb(struct mxvp_cmd *command,
                                    const struct mxvp_reply *reply, void *arg)
{
    struct semaphore *sema = arg;
    up(sema);
}

static void mxtf_test_dma_loopback(struct mxtf *mxtf)
{
    int frag;
    struct mxvp_cmd *cmd;
    struct mxvp_dma *dma;
    struct mxvp *mxvp;
    struct semaphore sema;

    void *rd_buffers[NUM_DMA_DESC];
    void *wr_buffers[NUM_DMA_DESC];
    dma_addr_t rd_handles[NUM_DMA_DESC];
    dma_addr_t wr_handles[NUM_DMA_DESC];
    mxvp_mem_cookie vp_buffers[NUM_DMA_DESC];

    mxvp = &mxtf->mxvp;
    sema_init(&sema, 0);

    memset(rd_buffers, 0, sizeof(rd_buffers));
    memset(wr_buffers, 0, sizeof(wr_buffers));
    memset(rd_handles, 0, sizeof(rd_handles));
    memset(wr_handles, 0, sizeof(wr_handles));
    memset(vp_buffers, 0, sizeof(vp_buffers));

    /* Create the DMA read command */
    cmd = mxvp_cmd_alloc_dma_read(NUM_DMA_DESC);
    if (!cmd) {
        mxtf_error("failed to create DMA read command");
        return;
    }

    /* Build DMA descriptors */
    dma = (struct mxvp_dma *) cmd->payload;
    for (frag = 0; frag < NUM_DMA_DESC; frag++) {
        /* Allocate VPU memory */
        vp_buffers[frag] = mxvp_mem_alloc(&mxvp->mem, DMA_BUFFER_SIZE);
        if (!vp_buffers[frag]) {
            mxtf_error("failed to allocate VPU memory\n");
            goto error;
        }

        /* Allocate Host memory */
        rd_buffers[frag] = kmalloc(DMA_BUFFER_SIZE, GFP_KERNEL);
        if (!rd_buffers[frag]) {
            mxtf_error("failed to allocate host memory\n");
            goto error;
        }

        /* Fill host memory with random data */
        get_random_bytes(rd_buffers[frag], DMA_BUFFER_SIZE);

        /* Map Host memory */
        rd_handles[frag] = dma_map_single(&mxvp->pci->dev, rd_buffers[frag],
                                            DMA_BUFFER_SIZE, DMA_TO_DEVICE);

        /* Fill descriptor entry */
        dma->desc[frag].dst = mxvp_mem_phys(&mxvp->mem, vp_buffers[frag]);
        dma->desc[frag].src = rd_handles[frag];
        dma->desc[frag].len = DMA_BUFFER_SIZE;
    }

    if (mxvp_send_dma(mxvp, cmd, mxtf_test_dma_loopback_cb, &sema)) {
        mxtf_error("failed to send DMA read command\n");
        goto error;
    }

    mxvp_ring_doorbell(mxvp);

    down(&sema);

    kfree(cmd);

////////////////////////////////////////////////////////////////////////////////

    /* Create the DMA write command */
    cmd = mxvp_cmd_alloc_dma_write(NUM_DMA_DESC);
    if (!cmd) {
        mxtf_error("failed to create DMA write command");
        goto error;
    }

    /* Build DMA descriptors */
    dma = (struct mxvp_dma *) cmd->payload;
    for (frag = 0; frag < NUM_DMA_DESC; frag++) {
        /* Allocate Host memory */
        wr_buffers[frag] = kzalloc(DMA_BUFFER_SIZE, GFP_KERNEL);
        if (!wr_buffers[frag]) {
            mxtf_error("failed to allocate host memory\n");
            goto error;
        }

        /* Fill host memory with random data */
        get_random_bytes(wr_buffers[frag], DMA_BUFFER_SIZE);

        /* Map Host memory */
        wr_handles[frag] = dma_map_single(&mxvp->pci->dev, wr_buffers[frag],
                                            DMA_BUFFER_SIZE, DMA_FROM_DEVICE);

        /* Fill descriptor entry */
        dma->desc[frag].dst = wr_handles[frag];
        dma->desc[frag].src = mxvp_mem_phys(&mxvp->mem, vp_buffers[frag]);
        dma->desc[frag].len = DMA_BUFFER_SIZE;
    }

    if (mxvp_send_dma(mxvp, cmd, mxtf_test_dma_loopback_cb, &sema)) {
        mxtf_error("failed to send DMA write command\n");
        goto error;
    }

    mxvp_ring_doorbell(mxvp);

    down(&sema);

    kfree(cmd);

////////////////////////////////////////////////////////////////////////////////

    /* Compare read buffers vs write buffers */
    for (frag = 0; frag < NUM_DMA_DESC; frag++) {
        void *read  = rd_buffers[frag];
        void *write = wr_buffers[frag];

        dma_unmap_single(&mxvp->pci->dev, rd_handles[frag],
                            DMA_BUFFER_SIZE, DMA_TO_DEVICE);
        dma_unmap_single(&mxvp->pci->dev, wr_handles[frag],
                            DMA_BUFFER_SIZE, DMA_FROM_DEVICE);

        if (memcmp(read, write, DMA_BUFFER_SIZE)) {
            mxtf_error("buffer mismatch at fragment %d\n", frag);
            goto error;
        }
    }

    mxtf_info("DMA loopback test passed \n");

////////////////////////////////////////////////////////////////////////////////

error :
    for (frag = 0; frag < NUM_DMA_DESC; frag++) {
        mxvp_mem_free(&mxvp->mem, vp_buffers[frag]);
        kfree(rd_buffers[frag]);
        kfree(wr_buffers[frag]);
    }
}

static void mxtf_test_multi_dma_loopback(struct mxtf *mxtf)
{
    int frag;
    struct mxvp_cmd *cmd;
    struct mxvp *mxvp;
    struct semaphore sema;
    mxvp_mem_cookie vp_desc_list;
    struct mxvp_dma_desc *list;

    void *rd_buffers[NUM_DMA_DESC];
    void *wr_buffers[NUM_DMA_DESC];
    dma_addr_t rd_handles[NUM_DMA_DESC];
    dma_addr_t wr_handles[NUM_DMA_DESC];
    mxvp_mem_cookie vp_buffers[NUM_DMA_DESC];

    mxvp = &mxtf->mxvp;
    sema_init(&sema, 0);

    memset(rd_buffers, 0, sizeof(rd_buffers));
    memset(wr_buffers, 0, sizeof(wr_buffers));
    memset(rd_handles, 0, sizeof(rd_handles));
    memset(wr_handles, 0, sizeof(wr_handles));
    memset(vp_buffers, 0, sizeof(vp_buffers));

    /* Allocate VPU memory for descriptor list */
    vp_desc_list = mxvp_mem_alloc(&mxvp->mem, NUM_DMA_DESC * sizeof(struct mxvp_dma_desc));
    if (!vp_desc_list) {
        mxtf_error("Failed to allocate VPU memory\n");
        return;
    }

    /* Get virtual handle of list */
    list = mxvp_mem_virt(&mxvp->mem, vp_desc_list);

    /* Allocate multi DMA read command */
    cmd = mxvp_cmd_alloc_multi_dma_read(mxvp_mem_phys(&mxvp->mem, vp_desc_list),
                                        NUM_DMA_DESC);
    if (!cmd) {
        mxtf_error("Failed to create Multi DMA read command");
        goto error;
    }

    /* Build descriptor list */
    for (frag = 0; frag < NUM_DMA_DESC; frag++) {
        /* Allocate VPU memory */
        vp_buffers[frag] = mxvp_mem_alloc(&mxvp->mem, DMA_BUFFER_SIZE);
        if (!vp_buffers[frag]) {
            mxtf_error("failed to allocate VPU memory\n");
            goto error;
        }

        /* Allocate Host memory */
        rd_buffers[frag] = kmalloc(DMA_BUFFER_SIZE, GFP_KERNEL);
        if (!rd_buffers[frag]) {
            mxtf_error("failed to allocate host memory\n");
            goto error;
        }

        /* Fill host memory with random data */
        get_random_bytes(rd_buffers[frag], DMA_BUFFER_SIZE);

        /* Map Host memory */
        rd_handles[frag] = dma_map_single(&mxvp->pci->dev, rd_buffers[frag],
                                                DMA_BUFFER_SIZE, DMA_TO_DEVICE);

        mxvp_wr64(&list[frag].dst, 0, mxvp_mem_phys(&mxvp->mem, vp_buffers[frag]));
        mxvp_wr64(&list[frag].src, 0, rd_handles[frag]);
        mxvp_wr32(&list[frag].len, 0, DMA_BUFFER_SIZE);
    }

    if (mxvp_send_dma(mxvp, cmd, mxtf_test_dma_loopback_cb, &sema)) {
        mxtf_error("failed to send Multi DMA read command\n");
        goto error;
    }

    mxvp_ring_doorbell(mxvp);

    down(&sema);

    kfree(cmd);

////////////////////////////////////////////////////////////////////////////////

    /* Create the DMA write command */
    cmd = mxvp_cmd_alloc_multi_dma_write(mxvp_mem_phys(&mxvp->mem, vp_desc_list),
                                        NUM_DMA_DESC);
    if (!cmd) {
        mxtf_error("failed to create Multi DMA write command");
        goto error;
    }

    /* Build descriptor list */
    for (frag = 0; frag < NUM_DMA_DESC; frag++) {
        /* Allocate Host memory */
        wr_buffers[frag] = kzalloc(DMA_BUFFER_SIZE, GFP_KERNEL);
        if (!wr_buffers[frag]) {
            mxtf_error("failed to allocate host memory\n");
            goto error;
        }

        /* Map Host memory */
        wr_handles[frag] = dma_map_single(&mxvp->pci->dev, wr_buffers[frag],
                                                DMA_BUFFER_SIZE, DMA_TO_DEVICE);

        mxvp_wr64(&list[frag].dst, 0, wr_handles[frag]);
        mxvp_wr64(&list[frag].src, 0, mxvp_mem_phys(&mxvp->mem, vp_buffers[frag]));
        mxvp_wr32(&list[frag].len, 0, DMA_BUFFER_SIZE);
    }

    if (mxvp_send_dma(mxvp, cmd, mxtf_test_dma_loopback_cb, &sema)) {
        mxtf_error("failed to send Multi DMA write command\n");
        goto error;
    }

    mxvp_ring_doorbell(mxvp);

    down(&sema);

    kfree(cmd);

////////////////////////////////////////////////////////////////////////////////

    /* Compare read buffers vs write buffers */
    for (frag = 0; frag < NUM_DMA_DESC; frag++) {
        void *read  = rd_buffers[frag];
        void *write = wr_buffers[frag];

        dma_unmap_single(&mxvp->pci->dev, rd_handles[frag],
                            DMA_BUFFER_SIZE, DMA_TO_DEVICE);
        dma_unmap_single(&mxvp->pci->dev, wr_handles[frag],
                            DMA_BUFFER_SIZE, DMA_FROM_DEVICE);

        if (memcmp(read, write, DMA_BUFFER_SIZE)) {
            mxtf_error("buffer mismatch at fragment %d\n", frag);
            goto error;
        }
    }
    mxtf_info("Multi DMA loopback test passed\n");

////////////////////////////////////////////////////////////////////////////////

error :
    mxvp_mem_free(&mxvp->mem, vp_desc_list);
    for (frag = 0; frag < NUM_DMA_DESC; frag++) {
        mxvp_mem_free(&mxvp->mem, vp_buffers[frag]);
        kfree(rd_buffers[frag]);
        kfree(wr_buffers[frag]);
    }
}

static void mxtf_test_dma_memfill_cb(struct mxvp_cmd *command,
                                    const struct mxvp_reply *reply, void *arg)
{
    struct semaphore *sema = arg;
    up(sema);
}

void mxtf_test_mem_fill(struct mxtf *mxtf)
{
    int index;
    mxvp_mem_cookie vp_cookie = NULL;
    struct mxvp_cmd *cmd = NULL;
    u32 pattern;
    u32 *expected = NULL;
    u32 *actual   = NULL;
    struct mxvp *mxvp;
    struct semaphore sema;
    dma_addr_t handle;

    mxvp = &mxtf->mxvp;
    sema_init(&sema, 0);

    /* Allocate VPU memory */
    vp_cookie = mxvp_mem_alloc(&mxvp->mem, DMA_BUFFER_SIZE);
    if (!vp_cookie) {
        mxtf_error("failed to allocate VPU memory\n");
        goto error;
    }

    /* Allocate host buffer to compare against */
    expected = kmalloc(DMA_BUFFER_SIZE, GFP_KERNEL);
    if (!expected) {
        mxtf_error("failed to allocate expected host memory\n");
        goto error;
    }

    actual = kzalloc(DMA_BUFFER_SIZE, GFP_KERNEL);
    if (!actual) {
        mxtf_error("failed to allocate actual host memory\n");
        goto error;
    }

    /* Get random pattern and fill buffer */
    get_random_bytes(&pattern, sizeof(pattern));
    for (index = 0; index < (DMA_BUFFER_SIZE / sizeof(pattern)); index++) {
        expected[index] = pattern;
    }

    /* Allocate memfill command */
    cmd = mxvp_cmd_alloc_mem_fill(mxvp_mem_phys(&mxvp->mem, vp_cookie),
                                    DMA_BUFFER_SIZE, pattern);
    if (!cmd) {
        mxtf_error("failed to allocate memfill command\n");
        goto error;
    }

    /* Map actual buffer to read from */
    handle = dma_map_single(&mxvp->pci->dev, actual, DMA_BUFFER_SIZE, DMA_FROM_DEVICE);

    if (mxvp_send_dma(mxvp, cmd, mxtf_test_dma_memfill_cb, &sema)) {
        mxtf_error("Failed to send read write command\n");
        return;
    }

    mxvp_ring_doorbell(mxvp);

    down(&sema);

    mxvp_rd_buffer(mxvp_mem_virt(&mxvp->mem, vp_cookie),0, actual, DMA_BUFFER_SIZE);

    dma_unmap_single(&mxvp->pci->dev, handle, DMA_BUFFER_SIZE, DMA_FROM_DEVICE);

    /* Compare expect vs actual buffers for errors */
    if (memcmp(expected, actual, DMA_BUFFER_SIZE)) {
        mxtf_error("Mismatch in memfill comparison\n");
        goto error;
    }

    mxtf_info("Memory Fill test passed\n");

error :
    kfree(cmd);
    kfree(expected);
    kfree(actual);
    mxvp_mem_free(&mxvp->mem, vp_cookie);
}

static void mxtf_test_fence_cb(struct mxvp_cmd *command,
                                    const struct mxvp_reply *reply, void *arg)
{
    struct semaphore *sema = arg;
    up(sema);
}

void mxtf_test_fence(struct mxtf *mxtf)
{
    struct mxvp_cmd * cmd = NULL;
    struct mxvp *mxvp = &mxtf->mxvp;
    struct semaphore sema;

    sema_init(&sema, 0);

    /* Create the FENCE command */
    cmd = mxvp_cmd_alloc_fence();
    if (!cmd) {
        mxtf_error("Failed to create FENCE command");
        goto error;
    }

    /* Submit FENCE in command queue */
    if (mxvp_send_cmd(mxvp, cmd, mxtf_test_fence_cb, &sema)) {
        mxtf_error("Failed to send DMA FENCE command\n");
        goto error;
    }

    mxvp_ring_doorbell(mxvp);

    down(&sema);

////////////////////////////////////////////////////////////////////////////////
    sema_init(&sema, 0);

    /* Submit FENCE in DMA queue */
    if (mxvp_send_dma(mxvp, cmd, mxtf_test_fence_cb, &sema)) {
        mxtf_error("Failed to send CMD FENCE command\n");
        return;
    }

    mxvp_ring_doorbell(mxvp);

    down(&sema);

    mxtf_info("FENCE test passed\n");

error :
    kfree(cmd);
}

static void mxtf_test_exebuf_cb(struct mxvp_cmd *command,
                                    const struct mxvp_reply *reply, void *arg)
{
    struct semaphore *sema = arg;
    up(sema);
}

void mxtf_test_exebuf(struct mxtf *mxtf)
{
    int length = 256;
    mxvp_mem_cookie vp_cookie = NULL;
    struct mxvp_cmd * cmd = NULL;
    struct mxvp *mxvp = &mxtf->mxvp;
    struct semaphore sema;

    sema_init(&sema, 0);

    vp_cookie = mxvp_mem_alloc(&mxvp->mem, length);
    if (!vp_cookie) {
        mxtf_error("failed to allocate VPU memory\n");
        goto error;
    }

    /* Create the execute buffer command */
    cmd = mxvp_cmd_alloc_exe_buffer(mxvp_mem_phys(&mxvp->mem, vp_cookie), length);
    if (!cmd) {
        mxtf_error("failed to create execute buffer command");
        goto error;
    }

    /* Send command */
    if (mxvp_send_cmd(mxvp, cmd, mxtf_test_exebuf_cb, &sema)) {
        mxtf_error("failed to send execute buffer command\n");
        goto error;
    }

    mxvp_ring_doorbell(mxvp);

    down(&sema);

    mxtf_info("Execute buffer test passed\n");

error :
    mxvp_mem_free(&mxvp->mem, vp_cookie);
    kfree(cmd);
}

void mxtf_test_async_cmdq_preempt(struct mxtf *mxtf)
{
    int error;
    struct mxvp *mxvp = &mxtf->mxvp;

    error = mxvp_send_async(mxvp, MXVP_ASYNC_CMDQ_PREEMPT, 0);
    if (error) {
        mxtf_error("failed to send MXVP_ASYNC_CMDQ_PREEMPT\n");
        return;
    }

    mxtf_info("CMDQ Preemption test passed\n");
}

void mxtf_test_async_dmaq_preempt(struct mxtf *mxtf)
{
    int error;
    struct mxvp *mxvp = &mxtf->mxvp;

    error = mxvp_send_async(mxvp, MXVP_ASYNC_DMAQ_PREEMPT, 0);
    if (error) {
        mxtf_error("failed to send MXVP_ASYNC_DMAQ_PREEMPT\n");
        return;
    }

    mxtf_info("DMAQ Preemption test passed\n");
}

void mxtf_test_async_cmdq_reset(struct mxtf *mxtf)
{
    int error;
    struct mxvp *mxvp = &mxtf->mxvp;

    mxvp = &mxtf->mxvp;

    error = mxvp_send_async(mxvp, MXVP_ASYNC_CMDQ_RESET, 0);
    if (error) {
        mxtf_error("failed to send MXVP_ASYNC_CMDQ_RESET\n");
        return;
    }

    mxtf_info("CMDQ Reset test passed\n");
}

void mxtf_test_async_dmaq_reset(struct mxtf *mxtf)
{
    int error;
    struct mxvp *mxvp = &mxtf->mxvp;

    mxvp = &mxtf->mxvp;

    error = mxvp_send_async(mxvp, MXVP_ASYNC_DMAQ_RESET, 0);
    if (error) {
        mxtf_error("failed to send MXVP_ASYNC_DMAQ_RESET\n");
        return;
    }

    mxtf_info("DMAQ Reset test passed\n");
}

void mxtf_test_async_dev_reset(struct mxtf *mxtf)
{
    int error;
    struct mxvp *mxvp = &mxtf->mxvp;

    error = mxvp_send_async(mxvp, MXVP_ASYNC_DEV_RESET, 0);
    if (error) {
        mxtf_error("failed to send MXVP_ASYNC_DEV_RESET (%d)\n", error);
        return;
    }

    mxtf_info("Device Reset test passed\n");
}

void mxtf_test_async_set_power(struct mxtf *mxtf)
{
    int error;
    struct mxvp *mxvp = &mxtf->mxvp;
    mxvp_async_pwr_arg arg;

    arg = MXVP_ASYNC_PWR_D0_ENTRY;
    error = mxvp_send_async(mxvp, MXVP_ASYNC_SET_POWER, &arg);
    if (error) {
        mxtf_error("failed to send MXVP_ASYNC_SET_POWER\n");
        return;
    }

    mxtf_info("Set Power test passed\n");
}

static void mxtf_test_thpt_calculator(struct work_struct *work)
{
    struct delayed_work *dw = to_delayed_work(work);
    u64 rbps = read_accumulator;
    u64 wbps = write_accumulator;
    u64 now  = get_jiffies_64();

    /* Zero out accumulators */
    read_accumulator = 0;
    write_accumulator = 0;

    /* Calculate throughput in bytes per second */
    rbps = (rbps / jiffies_to_msecs(now - read_jiffies)) * 1000;
    wbps = (wbps / jiffies_to_msecs(now - write_jiffies)) * 1000;

    /* Record time for next calculator */
    read_jiffies = write_jiffies = now;

    mxtf_info("r : %llu w : %llu\n", rbps, wbps);

    /* Schedule next calculator for interval */
    schedule_delayed_work(dw, msecs_to_jiffies(DMA_THPT_INTERVAL));
}

static void mxtf_test_throughput_cb(struct mxvp_cmd *command,
                                    const struct mxvp_reply *reply, void *arg)
{
    struct mxvp *mxvp = (struct mxvp *) arg;

    /* Tally consumed buffer */
    switch (command->command) {
        case MXVP_CMD_DMA_READ :
        case MXVP_CMD_MULTI_DMA_READ :
            read_accumulator += read_total_len;
            break;
        case MXVP_CMD_DMA_WRITE :
        case MXVP_CMD_MULTI_DMA_WRITE :
            write_accumulator += write_total_len;
            break;
        default :
            return;
    }

    /* Queue up same command */
    if (mxvp_send_dma(mxvp, command, mxtf_test_throughput_cb, mxvp)) {
        mxtf_error("failed to send DMA command\n");
        return;
    }

    /* Notify VPU */
    mxvp_ring_doorbell(mxvp);
}

void mxtf_test_throughput(struct mxtf *mxtf)
{
    int frag;
    int ndesc = 16;
    int length = 16 * 1024;
    struct mxvp_cmd *rcmd;
    struct mxvp_cmd *wcmd;
    struct mxvp_dma *dma;
    struct mxvp *mxvp;

    INIT_DELAYED_WORK(&thpt_work, mxtf_test_thpt_calculator);
    read_accumulator = 0;
    write_accumulator = 0;
    mxvp = &mxtf->mxvp;

    /* Create the DMA commands */
    rcmd = mxvp_cmd_alloc_dma_read(ndesc);
    wcmd = mxvp_cmd_alloc_dma_write(ndesc);

    /* Build the descriptors for each command */
    for (frag = 0; frag < ndesc; frag++) {
        mxvp_mem_cookie cookie;
        void *buffer;
        dma_addr_t phys;

        cookie = mxvp_mem_alloc(&mxvp->mem, length);
        buffer = kmalloc(length, GFP_KERNEL);
        phys = dma_map_single(&mxvp->pci->dev, buffer, length, DMA_BIDIRECTIONAL);

        dma = (struct mxvp_dma *) rcmd->payload;
        dma->desc[frag].dst = mxvp_mem_phys(&mxvp->mem, cookie);
        dma->desc[frag].src = phys;
        dma->desc[frag].len = length;

        dma = (struct mxvp_dma *) wcmd->payload;
        dma->desc[frag].dst = phys;
        dma->desc[frag].src = mxvp_mem_phys(&mxvp->mem, cookie);
        dma->desc[frag].len = length;
    }

    write_total_len = read_total_len = ndesc * length;

    /* Send Read and Write commands plus backups to keep it running */
    if (mxvp_send_dma(mxvp, rcmd, mxtf_test_throughput_cb, mxvp)) {
        mxtf_error("failed to send DMA read command\n");
        return;
    }

    if (mxvp_send_dma(mxvp, wcmd, mxtf_test_throughput_cb, mxvp)) {
        mxtf_error("failed to send DMA write command\n");
        return;
    }

    if (mxvp_send_dma(mxvp, rcmd, mxtf_test_throughput_cb, mxvp)) {
        mxtf_error("failed to send DMA read command\n");
        return;
    }

    if (mxvp_send_dma(mxvp, wcmd, mxtf_test_throughput_cb, mxvp)) {
        mxtf_error("failed to send DMA write command\n");
        return;
    }

    /* Notify VPU of these commands */
    mxvp_ring_doorbell(mxvp);

    /* Record time for calculator */
    write_jiffies = read_jiffies = get_jiffies_64();
    /* Schedule the calculator */
    schedule_delayed_work(&thpt_work, msecs_to_jiffies(DMA_THPT_INTERVAL));
}
