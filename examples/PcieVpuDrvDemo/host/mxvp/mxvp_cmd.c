/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: VPU commands generation
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/slab.h>

#include "mxvp_cmd.h"

#define MXVP_CMD_ALLOC_FLAGS    (GFP_KERNEL)
#define MXVP_COMMAND_ALIGNMENT  (sizeof(u32))

static struct mxvp_cmd *mxvp_cmd_alloc_multi_dma_common(u64 address, u32 ndesc);

struct mxvp_cmd *mxvp_cmd_alloc_exe_buffer(u64 address, u32 size)
{
    size_t length;
    struct mxvp_cmd *command;
    struct mxvp_exec_buffer *payload;

    length  = sizeof(struct mxvp_cmd);
    length += sizeof(struct mxvp_exec_buffer);

    command = kzalloc(length, MXVP_CMD_ALLOC_FLAGS);

    if (command) {
        command->length  = roundup(length, MXVP_COMMAND_ALIGNMENT);
        command->version = MXVP_CMD_HDR_VERSION;
        command->command = MXVP_CMD_EXE_BUFFER;

        payload = (struct mxvp_exec_buffer *) command->payload;
        payload->addr = address;
        payload->size = size;
    }

    return command;
}

struct mxvp_cmd *mxvp_cmd_alloc_dma_read(u32 ndesc)
{
    size_t length;
    struct mxvp_cmd *command;
    struct mxvp_dma *payload;

    length  = sizeof(struct mxvp_cmd);
    length += sizeof(struct mxvp_dma);
    length += sizeof(struct mxvp_dma_desc) * ndesc;

    command = kzalloc(length, MXVP_CMD_ALLOC_FLAGS);

    if (command) {
        command->length  = roundup(length, MXVP_COMMAND_ALIGNMENT);
        command->version = MXVP_CMD_HDR_VERSION;
        command->command = MXVP_CMD_DMA_READ;

        payload = (struct mxvp_dma *) command->payload;
        payload->ndesc = ndesc;
    }

    return command;
}

struct mxvp_cmd *mxvp_cmd_alloc_dma_write(u32 ndesc)
{
    size_t length;
    struct mxvp_cmd *command;
    struct mxvp_dma *payload;

    length  = sizeof(struct mxvp_cmd);
    length += sizeof(struct mxvp_dma);
    length += sizeof(struct mxvp_dma_desc) * ndesc;

    command = kzalloc(length, MXVP_CMD_ALLOC_FLAGS);

    if (command) {
        command->length  = roundup(length, MXVP_COMMAND_ALIGNMENT);
        command->version = MXVP_CMD_HDR_VERSION;
        command->command = MXVP_CMD_DMA_WRITE;

        payload = (struct mxvp_dma *) command->payload;
        payload->ndesc = ndesc;
    }

    return command;
}

static struct mxvp_cmd *mxvp_cmd_alloc_multi_dma_common(u64 address, u32 ndesc)
{
    size_t length;
    struct mxvp_cmd *command;
    struct mxvp_multi_dma *payload;

    length  = sizeof(struct mxvp_cmd);
    length += sizeof(struct mxvp_multi_dma);

    command = kzalloc(length, MXVP_CMD_ALLOC_FLAGS);

    if (command) {
        command->length  = roundup(length, MXVP_COMMAND_ALIGNMENT);
        command->version = MXVP_CMD_HDR_VERSION;

        payload = (struct mxvp_multi_dma *) command->payload;
        payload->addr = address;
        payload->size = sizeof(struct mxvp_dma_desc) * ndesc;
    }

    return command;
}

struct mxvp_cmd *mxvp_cmd_alloc_multi_dma_read(u64 buffer, u32 ndesc)
{
    struct mxvp_cmd *command = mxvp_cmd_alloc_multi_dma_common(buffer, ndesc);

    if (command) {
        command->command = MXVP_CMD_MULTI_DMA_READ;
    }

    return command;
}

struct mxvp_cmd *mxvp_cmd_alloc_multi_dma_write(u64 buffer, u32 ndesc)
{
    struct mxvp_cmd *command = mxvp_cmd_alloc_multi_dma_common(buffer, ndesc);

    if (command) {
        command->command = MXVP_CMD_MULTI_DMA_WRITE;
    }

    return command;
}

struct mxvp_cmd *mxvp_cmd_alloc_mem_fill(u64 address, u32 size, u32 pattern)
{
    size_t length;
    struct mxvp_cmd *command;
    struct mxvp_mem_fill *payload;

    length  = sizeof(struct mxvp_cmd);
    length += sizeof(struct mxvp_mem_fill);

    command = kzalloc(length, MXVP_CMD_ALLOC_FLAGS);

    if (command) {
        command->length  = roundup(length, MXVP_COMMAND_ALIGNMENT);
        command->version = MXVP_CMD_HDR_VERSION;
        command->command = MXVP_CMD_MEM_FILL;

        payload = (struct mxvp_mem_fill *) command->payload;
        payload->addr = address;
        payload->size = roundup(size, sizeof(pattern));
        payload->pattern = pattern;
    }

    return command;
}

struct mxvp_cmd *mxvp_cmd_alloc_fence(void)
{
    size_t length;
    struct mxvp_cmd *command;

    length = sizeof(struct mxvp_cmd);

    command = kzalloc(length, MXVP_CMD_ALLOC_FLAGS);

    if (command) {
        command->length  = roundup(length, MXVP_COMMAND_ALIGNMENT);
        command->version = MXVP_CMD_HDR_VERSION;
        command->command = MXVP_CMD_FENCE;
    }

    return command;
}
