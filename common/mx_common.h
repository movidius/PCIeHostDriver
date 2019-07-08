/*******************************************************************************
 *
 * MX drivers common definitions
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef COMMON_MX_COMMON_H_
#define COMMON_MX_COMMON_H_

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/cdev.h>

/* Myriad X protocol version.
 * For user information only. */
#define MX_BSPEC_REVISION "1.1"

/* Myriad X PCI device ID. */
#define MX_PCI_DEVICE_ID 0x6200

/* DMA transfers performed by Myriad X PCIe DMA must be 16-byte aligned. */
#define MX_DMA_ALIGNMENT 16

/* Max size of contiguous buffer available in standard distributions. */
#define MAX_CONT_BUFFER_SIZE_LINUX (4 * 1024 * 1024)

/* MMIO field helper macros */
#define field_mask(field)       ((1 << field##_WIDTH) - 1)
#define field_set(field, value) (((value) & field_mask(field)) << field##_SHIFT)
#define field_get(field, value) (((value) >> field##_SHIFT) & field_mask(field))

/* Myriad-X boot MMIO register offsets and layout */
#define MX_MAIN_MAGIC     0x00
#define MX_MF_READY       0x10
#define MX_MF_LENGTH      0x14
#define MX_MF_START       0x20
#define MX_INT_ENABLE     0x28
#define MX_INT_MASK       0x2C
#define MX_INT_IDENTITY   0x30
    #define MX_INT_DMRQ_UPDATE_SHIFT    4
    #define MX_INT_DMRQ_UPDATE_WIDTH    1
    #define MX_INT_RETQ_UPDATE_SHIFT    3
    #define MX_INT_RETQ_UPDATE_WIDTH    1
    #define MX_INT_DMAQ_PREEMPTED_SHIFT 2
    #define MX_INT_DMAQ_PREEMPTED_WIDTH 1
    #define MX_INT_CMDQ_PREEMPTED_SHIFT 1
    #define MX_INT_CMDQ_PREEMPTED_WIDTH 1
    #define MX_INT_STATUS_UPDATE_SHIFT  0
    #define MX_INT_STATUS_UPDATE_WIDTH  1

/* Main Magic string to appear at start (bytes[15:0]) of MMIO region */
#define MX_MM_BOOT_STR     "VPUBOOT"
#define MX_MM_LOAD_STR     "VPULOADER"
#define MX_MM_VPUAL_STR    "VPUAL"
#define MX_MM_VPULINK_STR  "VPULINK"
#define MX_MM_LEN          16

/* Operation mode of Myriad-X, based on main magic value */
enum mx_opmode {
    MX_OPMODE_UNKNOWN,
    MX_OPMODE_BOOT,
    MX_OPMODE_LOADER,
    MX_OPMODE_APP_VPUAL,
    MX_OPMODE_APP_VPULINK
};
#define MX_IS_OPMODE_APP(opmode) \
    ((opmode == MX_OPMODE_APP_VPUAL) || (opmode == MX_OPMODE_APP_VPULINK))

/*
 * MX device context.
 * Should be defined once in each device instance control structure.
 */
struct mx_dev {
    struct pci_dev *pci;
    void __iomem *mmio;
    struct mutex transfer_lock;
};

/*
 * @brief Get MX device's operation mode.
 *
 * @param[in] mx_dev - pointer to mx_dev instance.
 *
 * @return Operation mode.
 */
enum mx_opmode mx_get_opmode(struct mx_dev *mx_dev);

#endif /* COMMON_MX_COMMON_H_ */
