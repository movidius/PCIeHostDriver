/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: VPU memory management
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxvp_mem.h"
#include "mxvp_mmio.h"
#include "mxvp_print.h"
#include <linux/slab.h>

#define MXVP_MEMORY_BLOCK_SIZE (16 * 1024u)

static int  mxvp_mem_create_free_list(struct mxvp_mem *mem);
static bool mxvp_mem_is_cookie(struct mxvp_mem *mem, mxvp_mem_cookie cookie);
static bool mxvp_mem_is_cookie_strict(struct mxvp_mem *mem, mxvp_mem_cookie cookie);

static int mxvp_mem_create_free_list(struct mxvp_mem *mem)
{
    int index;
    size_t nblocks = mem->size / mem->block;

    /* Allocate block list */
    mem->list = kcalloc(nblocks, sizeof(void *), GFP_KERNEL);

    if (!mem->list) {
        mxvp_error("failed to allocate list for memory manager\n");
        return -ENOMEM;
    }

    /* Ser whole list as free */
    mem->free = mem->list;

    /* Create free linked list */
    for (index = 0; index < (nblocks - 1); index++) {
        mem->free[index] = &mem->free[index + 1];
    }
    /* Terminate list */
    mem->free[index] = NULL;

    return 0;
}

static bool mxvp_mem_is_cookie(struct mxvp_mem *mem,
                                     mxvp_mem_cookie cookie)
{
    /* Check that cookie is within bounds */
    return ((cookie >= mem->virtual) &&
            (cookie < (mem->virtual + mem->size)));
}

static bool mxvp_mem_is_cookie_strict(struct mxvp_mem *mem,
                                            mxvp_mem_cookie cookie)
{
    /* Check that cookie is within bound and points to start of block. */
    return ((cookie >= mem->virtual) &&
            (cookie < (mem->virtual + mem->size)) &&
            !((unsigned long) cookie & (mem->block - 1)));
}

int mxvp_mem_init(struct mxvp_mem *mem, void * virtual,
                  u64 physical, size_t size)
{
    memset(mem, 0, sizeof(*mem));
    mem->virtual = virtual;
    mem->physical = physical;
    mem->size = size;
    mem->block = MXVP_MEMORY_BLOCK_SIZE;
    spin_lock_init(&mem->lock);

    return mxvp_mem_create_free_list(mem);
}

void mxvp_mem_cleanup(struct mxvp_mem *mem)
{
    spin_lock(&mem->lock);
    kfree(mem->list);
    mem->free = mem->list = NULL;
    spin_unlock(&mem->lock);
}

mxvp_mem_cookie mxvp_mem_alloc(struct mxvp_mem *mem, size_t size)
{
    void **available = NULL;
    mxvp_mem_cookie cookie = NULL;

    /* Check that requested size doesn't exceed what we can allocate */
    if (size > mem->block) {
        return NULL;
    }

    /* Lock to protect list */
    spin_lock(&mem->lock);
    /* Make sure list hasn't been cleaned up */
    if (mem->list) {
        /* Check if there are any free blocks */
        available = mem->free;
        if (available) {
            /* Available contains pointer to next free block */
            mem->free = (void **) *available;
            /* Cookie points to corresponding host virtual address of block */
            cookie = mem->virtual + (available - mem->list) * mem->block;
        }
    }
    spin_unlock(&mem->lock);

    return cookie;
}

void mxvp_mem_free(struct mxvp_mem *mem, mxvp_mem_cookie cookie)
{
    long index;

    /* Lock to protect list */
    spin_lock(&mem->lock);
    /* Make sure list hasn't been cleaned up */
    if (mem->list) {
        /* Perform strict testing on cookie for validity */
        if (mxvp_mem_is_cookie_strict(mem, cookie)) {
            /* Recover entry in list */
            index = (cookie - mem->virtual) / mem->block;
            /* Insert entry in free list */
            mem->list[index] = mem->free;
            mem->free = &mem->list[index];
        } else {
            mxvp_debug("invalid cookie freed!\n");
        }
    }
    spin_unlock(&mem->lock);
}

void __iomem *mxvp_mem_virt(struct mxvp_mem *mem, mxvp_mem_cookie cookie)
{
    /* Cookie already represents virtual address, but check if valid */
    return (mxvp_mem_is_cookie(mem, cookie)) ? (void __iomem *) cookie : NULL;
}

u64 mxvp_mem_phys(struct mxvp_mem *mem, mxvp_mem_cookie cookie)
{
    /* Calculate physical address from cookie (virtual address) */
    if (mxvp_mem_is_cookie(mem, cookie)) {
        return (cookie - mem->virtual) + mem->physical;
    }
    return 0;
}

size_t mxvp_mem_size(struct mxvp_mem *mem, mxvp_mem_cookie cookie)
{
    return (size_t) mem->block;
}

void mxvp_mem_usage(struct mxvp_mem *mem, u32 *free, u32 *total)
{
    void **next;
    u32 tally = 0;

    spin_lock(&mem->lock);
    next = mem->free;
    while (next) {
        tally++;
        next = (void **) *next;
    }
    spin_unlock(&mem->lock);

    *free = tally;
    *total = mem->size / mem->block;
}
