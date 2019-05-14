/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Types and defines shared with device
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXLK_COMMON_HEADER_
#define MXLK_COMMON_HEADER_

#include <linux/types.h>

/*
 * Number of MSIs used between the device and host
 */
#define MXLK_IRQ_VECTORS    (1)

/*
 * Number of interfaces to statically allocate resources for
 */
#define MXLK_NUM_INTERFACES (1)

/*
 * Alignment restriction on buffers passed between device and host
 */
#define MXLK_DMA_ALIGNMENT  (16)

////////////////////////////////////////////////////////////////////////////////

/*
 * Status encoding of the transfer descriptors
 */
#define MXLK_DESC_STATUS_SUCCESS    ( 0)
#define MXLK_DESC_STATUS_ERROR      (-1)

/*
 * Layout transfer descriptors used by device and host
 */
struct mxlk_transfer_desc {
    uint64_t address;
    uint32_t length;
    uint16_t status;
    uint16_t interface;
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////////////

/*
 * Version to be exposed by both device and host
 */
#define MXLK_VERSION_MAJOR  (1)
#define MXLK_VERSION_MINOR  (2)
#define MXLK_VERSION_BUILD  (0)
#define MXLK_MMIO_SIZE      (16 * 1024)

struct mxlk_version {
    uint8_t  major;
    uint8_t  minor;
    uint16_t build;
} __attribute__((packed));

/*
 * Status encoding of both device and host
 */
#define MXLK_STATUS_ERROR   (-1)
#define MXLK_STATUS_UNINIT  ( 0)
#define MXLK_STATUS_BOOT    ( 1)
#define MXLK_STATUS_RUN     ( 2)


/*
 * MMIO layout and offsets shared between device and host
 */
struct mxlk_mmio {
    struct mxlk_version version;
    uint32_t device_status;
    uint32_t host_status;
    uint32_t cap_offset;
} __attribute__((packed));

#define MXLK_MMIO_VERSION       (offsetof(struct mxlk_mmio, version))
#define MXLK_MMIO_DEV_STATUS    (offsetof(struct mxlk_mmio, device_status))
#define MXLK_MMIO_HOST_STATUS   (offsetof(struct mxlk_mmio, host_status))
#define MXLK_MMIO_CAPABILITES   (offsetof(struct mxlk_mmio, cap_offset))

////////////////////////////////////////////////////////////////////////////////

/*
 * Defined capabilities located in mmio space
 */
#define MXLK_CAP_NULL   (0)
#define MXLK_CAP_BOOT   (1)
#define MXLK_CAP_STATS  (2)
#define MXLK_CAP_TXRX   (3)

/*
 * Header at the beginning of each capability to define and link to next
 */
struct mxlk_cap_hdr {
    uint16_t id;
    uint16_t next;
} __attribute__((packed));

#define MXLK_CAP_HDR_ID    (offsetof(struct mxlk_cap_hdr, id))
#define MXLK_CAP_HDR_NEXT  (offsetof(struct mxlk_cap_hdr, next))

/*
 * Bootloader capability - to be defined later
 */
struct mxlk_cap_boot {
    struct mxlk_cap_hdr hdr;
} __attribute__((packed));

/*
 * Stat collection - to be defined later
 */
struct mxlk_cap_stats {
    struct mxlk_cap_hdr hdr;
} __attribute__((packed));

/*
 * Simplex used by txrx cap
 */
struct mxlk_cap_pipe {
    uint32_t ring;
    uint32_t ndesc;
    uint32_t head;
    uint32_t tail;
} __attribute__((packed));

/*
 * Transmit and Receive capability
 */
struct mxlk_cap_txrx {
    struct mxlk_cap_hdr hdr;
    u32 fragment_size;
    struct mxlk_cap_pipe tx;
    struct mxlk_cap_pipe rx;
} __attribute__((packed));

#endif
