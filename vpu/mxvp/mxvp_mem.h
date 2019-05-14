/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: VPU memory management API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXVP_MEMORY_HEADER_
#define MXVP_MEMORY_HEADER_

#include <linux/types.h>
#include <linux/spinlock.h>

/*
 * mxvp_mem_cookie - token representing allocated VPU memory
 */
typedef const void *mxvp_mem_cookie;

/*
 * mxvp_mem - memory manager of VPU memory object
 */
struct mxvp_mem {
    void  *virtual;
    u64    physical;
    size_t size;
    size_t block;
    void **list;
    void **free;
    spinlock_t lock;
};

/*
 * @brief Initializes mxvp memory manager object
 *
 * @param[in] mem - pointer to memory manager object
 * @param[in] virtual - pointer to base kernel virtual address of VPU memory
 * @param[in] physical - base address of VPU DDR physical
 * @param[in] size - size in bytes of memory managed region
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int  mxvp_mem_init(struct mxvp_mem *mem, void * virtual, u64 physical, size_t size);

/*
 * @brief Cleanup of mxvp memory manager object
 *
 * @param[in] mem - pointer to memory manager object
 */
void mxvp_mem_cleanup(struct mxvp_mem *mem);

/*
 * @brief Allocates VPU memory from pool for given size
 *
 * @param[in] mem - pointer to memory manager object
 * @param[in] size - size in bytes of requested memory
 *
 * @return:
 *       NULL - failed to allocate request
 *      !NULL - valid mxvp_mem_cookie token
 */
mxvp_mem_cookie mxvp_mem_alloc(struct mxvp_mem *mem, size_t size);

/*
 * @brief Frees CPU memory back to pool
 *
 * @param[in] mem - pointer to memory manager object
 * @param[in] cookie - cookie returned by allocator function
 */
void mxvp_mem_free(struct mxvp_mem *mem, mxvp_mem_cookie cookie);

/*
 * @brief Token to iomem address
 *
 * NOTES:
 *  1) Pointer not directly accessible, use accessor functions in mxvp_mmio.h
 *
 * @param[in] mem - pointer to memory manager object
 * @param[in] cookie - cookie returned by allocator function
 *
 * @return:
 *      NULL - translation not possible/error
 *     !NULL - pointer to iomem to be used by mmio helper functions
 */
void __iomem *mxvp_mem_virt(struct mxvp_mem *mem, mxvp_mem_cookie cookie);

/*
 * @brief Token to VPU physical address
 *
 * NOTES:
 *  1) Address is _never_ valid to access directly
 *
 * @param[in] mem - pointer to memory manager object
 * @param[in] cookie - cookie returned by allocator function
 *
 * @return:
 *      0 - translation not possible/error
 *     !0 - VPU physical address within the DDR range reported by VPU
 */
u64 mxvp_mem_phys(struct mxvp_mem *mem, mxvp_mem_cookie cookie);

/*
 * @brief Token to memory size
 *
 * @param[in] mem - pointer to memory manager object
 * @param[in] cookie - cookie returned by allocator function
 *
 * @return:
 *      size in bytes of memory represented by cookie
 */
size_t mxvp_mem_size(struct mxvp_mem *mem, mxvp_mem_cookie cookie);

/*
 * @brief Runtime memory usage
 *
 * @param[in] mem - pointer to memory manager object
 * @param[out] free - pointer to store number of free blocks
 * @param[out] free - pointer to store number of total blocks
 */
void mxvp_mem_usage(struct mxvp_mem *mem, u32 *free, u32 *total);

#endif
