/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: VPU commands API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXVP_COMMAND_HEADER_
#define MXVP_COMMAND_HEADER_

#include <linux/types.h>

/*
 * Command and Reply versions for header fields
 */
#define MXVP_CMD_HDR_VERSION    (0x01)
#define MXVP_REPLY_HDR_VERSION  (0x01)

/*
 * Command and DMA operations definitions
 */
#define MXVP_CMD_EXE_BUFFER     (0x01)
#define MXVP_CMD_DMA_READ       (0x02)
#define MXVP_CMD_DMA_WRITE      (0x03)
#define MXVP_CMD_MULTI_DMA_READ (0x04)
#define MXVP_CMD_MULTI_DMA_WRITE (0x05)
#define MXVP_CMD_MEM_FILL       (0x06)
#define MXVP_CMD_FENCE          (0x07)

/*
 * Reply status of submitted commands
 */
#define MXVP_STATUS_SUCCESS     (0x00)
#define MXVP_STATUS_PARSING_ERR (0x01)
#define MXVP_STATUS_PROCESS_ERR (0x02)
#define MXVP_STATUS_DISCARDED   (0xDEADDEAD)

/*
 * mxvp_cmd - command header to every entry placed into command and DMA queues
 */
struct mxvp_cmd {
    u32 length;     /* total length of command in bytes, multiple of uint32_t */
    u32 id;         /* sequence number to match in reply */
    u16 version;    /* command header version */
    u16 reserved;
    u32 command;    /* requested operation, use MXVP_CMD definitions */
    u32 payload[];  /* additional payload based on command type */
} __attribute__((packed));

/*
 * mxvp_reply - header to every entry placed into command and DMA queue
 */
struct mxvp_reply {
    u32 length;     /* total length of reply in bytes, multiple of uint32_t */
    u32 id;         /* sequence number to match in reply */
    u16 version;    /* reply header version */
    u16 reserved;
    u32 status;     /* status code of processed command */
    u32 payload[];  /* additional payload based on command */
} __attribute__((packed));

/*
 * mxvp_exec_buffer - payload of MXVP_CMD_EXE_BUFFER command
 */
struct mxvp_exec_buffer {
    u64 addr;   /* VPU physical address - see mxvp_mem_phys */
    u32 size;   /* size in bytes of buffer */
} __attribute__((packed));

/*
 * mxvp_dma_desc - DMA descriptor used in DMA and Multi DMA operations
 */
struct mxvp_dma_desc {
    u64 src;    /* address of source buffer */
    u64 dst;    /* address of destination buffer */
    u32 len;    /* length to transfer in bytes */
} __attribute__((packed));

/*
 * mxvp_dma - payload of MXVP_CMD_DMA_READ/MXVP_CMD_DMA_WRITE commands
 */
struct mxvp_dma {
    u32 ndesc;  /* number of descriptors in payload */
    struct mxvp_dma_desc desc[]; /* variable array of descriptors */
} __attribute__((packed));

/*
 * mxvp_multi_dma - payload of MXVP_CMD_MULTI_DMA_READ/MXVP_CMD_MULTI_DMA_WRITE commands
 */
struct mxvp_multi_dma {
    u64 addr;   /* VPU physical address of list of DMA descriptors */
    u32 size;   /* size in bytes of list of DMA descriptors */
} __attribute__((packed));

/*
 * mxvp_mem_fill - payload of MXVP_CMD_MEM_FILL command
 */
struct mxvp_mem_fill {
    u64 addr;   /* starting VPU physical address */
    u32 size;   /* size in bytes to fill, multiple of uint32_t */
    u32 pattern; /* pattern to repeat */
} __attribute__((packed));

/*
 * @brief Builder function for MXVP_CMD_EXE_BUFFER command
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Allocates memory from GFP_KERNEL, user must free when done
 *  3) Sequence number, "id" field, generated at command queuing
 *
 * @param[in] address - VPU physical address
 * @param[in] size - size in bytes of buffer
 *
 * @return:
 *      NULL - failed to create command/error
 *     !NULL - pointer to command
 */
struct mxvp_cmd *mxvp_cmd_alloc_exe_buffer(u64 address, u32 size);

/*
 * @brief Builder function for MXVP_CMD_DMA_READ command
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Allocates memory from GFP_KERNEL, user must free when done
 *  3) Allocates necessary memory based on ndesc, caller must still
 *     populate descriptors fields
 *
 * @param[in] ndesc - number of descriptors in payload
 *
 * @return:
 *      NULL - failed to create command/error
 *     !NULL - pointer to command
 */
struct mxvp_cmd *mxvp_cmd_alloc_dma_read(u32 ndesc);

/*
 * @brief Builder function for MXVP_CMD_DMA_WRITE command
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Allocates memory from GFP_KERNEL, user must free when done
 *  3) Allocates necessary memory based on ndesc, caller must still
 *     populate descriptors fields
 *
 * @param[in] ndesc - number of descriptors in payload
 *
 * @return:
 *      NULL - failed to create command/error
 *     !NULL - pointer to command
 */
struct mxvp_cmd *mxvp_cmd_alloc_dma_write(u32 ndesc);

/*
 * @brief Builder function for MXVP_CMD_MULTI_DMA_READ command
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Allocates memory from GFP_KERNEL, user must free when done
 *
 * @param[in] buffer - VPU physical address of list of descriptors
 * @param[in] ndesc  - number of descriptors in list
 *
 * @return:
 *      NULL - failed to create command/error
 *     !NULL - pointer to command
 */
struct mxvp_cmd *mxvp_cmd_alloc_multi_dma_read(u64 buffer, u32 ndesc);

/*
 * @brief Builder function for MXVP_CMD_MULTI_DMA_WRITE command
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Allocates memory from GFP_KERNEL, user must free when done
 *
 * @param[in] buffer - VPU physical address of list of descriptors
 * @param[in] ndesc  - number of descriptors in list
 *
 * @return:
 *      NULL - failed to create command/error
 *     !NULL - pointer to command
 */
struct mxvp_cmd *mxvp_cmd_alloc_multi_dma_write(u64 buffer, u32 ndesc);

/*
 * @brief Builder function for MXVP_CMD_MEM_FILL command
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Allocates memory from GFP_KERNEL, user must free when done
 *
 * @param[in] address - VPU physical start address
 * @param[in] size - size in bytes to fill
 * @param[in] pattern - pattern to fill memory with
 *
 * @return:
 *      NULL - failed to create command/error
 *     !NULL - pointer to command
 */
struct mxvp_cmd *mxvp_cmd_alloc_mem_fill(u64 address, u32 size, u32 pattern);

/*
 * @brief Builder function for MXVP_CMD_FENCE command
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Allocates memory from GFP_KERNEL, user must free when done
 *
 * @return:
 *      NULL - failed to create command/error
 *     !NULL - pointer to command
 */
struct mxvp_cmd *mxvp_cmd_alloc_fence(void);

#endif
