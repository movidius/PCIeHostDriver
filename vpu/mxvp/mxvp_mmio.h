/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: VPU MMIO definitions and access functions
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXVP_MMIO_HEADER_
#define MXVP_MMIO_HEADER_

#include <linux/module.h>

/*
 * MMIO field helper macros
 */
#define field_mask(field)       ((1 << field##_WIDTH) - 1)
#define field_set(field, value) (((value) & field_mask(field)) << field##_SHIFT)
#define field_get(field, value) (((value) >> field##_SHIFT) & field_mask(field))

/*
 * Myriad-X IPC version
 */
#define MXVP_BSPEC_REVISION  "1.0"

/*
 * Main Magic string to appear at start (bytes[15:0]) of MMIO region
 */
#define MXVP_MAIN_MAGIC_STR  "VPUAL-000000"

/*
 * Myriad-X MMIO register offsets and layout
 */
#define MXVP_MAIN_MAGIC       (0x0000)
#define MXVP_REPLY_ORDER      (0x0010)
    #define MXVP_RO_IN_ORDER_SHIFT   (0)
    #define MXVP_RO_IN_ORDER_WIDTH   (1)
    #define MXVP_RO_VALID_SHIFT      (1)
    #define MXVP_RO_VALID_WIDTH      (1)
#define MXVP_TEMPERATURE      (0x0012)
#define MXVP_DDR_SIZE         (0x0014)
#define MXVP_DDR_PHY_ADDRESS  (0x0018)
#define MXVP_DEBUG            (0x0020)
#define MXVP_CMDQ_START       (0x1020)
#define MXVP_CMDQ_SIZE        (0x1024)
#define MXVP_CMDQ_HEAD        (0x1028)
#define MXVP_CMDQ_TAIL        (0x102C)
#define MXVP_RETQ_START       (0x1030)
#define MXVP_RETQ_SIZE        (0x1034)
#define MXVP_RETQ_HEAD        (0x1038)
#define MXVP_RETQ_TAIL        (0x103C)
#define MXVP_DMAQ_START       (0x1040)
#define MXVP_DMAQ_SIZE        (0x1044)
#define MXVP_DMAQ_HEAD        (0x1048)
#define MXVP_DMAQ_TAIL        (0x104C)
#define MXVP_DMRQ_START       (0x1050)
#define MXVP_DMRQ_SIZE        (0x1054)
#define MXVP_DMRQ_HEAD        (0x1058)
#define MXVP_DMRQ_TAIL        (0x105C)
#define MXVP_ASYNC_REQUEST    (0x1060)
#define MXVP_ASYNC_PENDING    (0x1064)
#define MXVP_ASYNC_DONE       (0x1068)
#define MXVP_ASYNC_ARGS       (0x106C)
#define MXVP_INTR_ENABLE      (0x1074)
#define MXVP_INTR_PENDING     (0x1078)
#define MXVP_INTR_PENDING2    (0x107C)
#define MXVP_INTR_STATUS      (0x1080)
#define MXVP_INTR_ACK         (0x1084)
    #define MXVP_INTR_DMRQ_UPDATE_SHIFT    (4)
    #define MXVP_INTR_DMRQ_UPDATE_WIDTH    (1)
    #define MXVP_INTR_RETQ_UPDATE_SHIFT    (3)
    #define MXVP_INTR_RETQ_UPDATE_WIDTH    (1)
    #define MXVP_INTR_DMAQ_PREEMPTED_SHIFT (2)
    #define MXVP_INTR_DMAQ_PREEMPTED_WIDTH (1)
    #define MXVP_INTR_CMDQ_PREEMPTED_SHIFT (1)
    #define MXVP_INTR_CMDQ_PREEMPTED_WIDTH (1)
    #define MXVP_INTR_STATUS_UPDATE_SHIFT  (0)
    #define MXVP_INTR_STATUS_UPDATE_WIDTH  (1)

/*
 * @brief Performs platform independent uint8_t read from iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 *
 * @return:
 *       Value read from location as uint8_t
 */
static u8 mxvp_rd8(void __iomem *base, int offset)
{
    return ioread8(base + offset);
}

/*
 * @brief Performs platform independent uint16_t read from iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 *
 * @return:
 *       Value read from location as uint16_t
 */
static u16 mxvp_rd16(void __iomem *base, int offset)
{
    return ioread16(base + offset);
}

/*
 * @brief Performs platform independent uint32_t read from iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 *
 * @return:
 *       Value read from location as uint32_t
 */
static u32 mxvp_rd32(void __iomem *base, int offset)
{
    return ioread32(base + offset);
}

/*
 * @brief Performs platform independent uint64_t read from iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 *
 * @return:
 *       Value read from location as uint64_t
 */
static u64 mxvp_rd64(void __iomem *base, int offset)
{
    u64 low;
    u64 high;

    low  = mxvp_rd32(base, offset);
    high = mxvp_rd32(base, offset + sizeof(u32));

    return low | (high << 32);
}

/*
 * @brief Performs platform independent read from iomem
 *
 * @param[in]  base - pointer to base of remapped iomem
 * @param[in]  offset - offset within iomem to read from
 * @param[out] buffer - pointer to location to store values
 * @param[in]  len - length in bytes to read into buffer
 */
static void mxvp_rd_buffer(void __iomem *base, int offset, void *buffer, size_t len)
{
    memcpy_fromio(buffer, base + offset, len);
}

/*
 * @brief Performs platform independent uint8_t write to iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 * @param[in] value - uint8_t value to write
 */
static void mxvp_wr8(void __iomem *base, int offset, u8 value)
{
    iowrite8(value, base + offset);
}

/*
 * @brief Performs platform independent uint16_t write to iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 * @param[in] value - uint16_t value to write
 */
static void mxvp_wr16(void __iomem *base, int offset, u16 value)
{
    iowrite16(value, base + offset);
}

/*
 * @brief Performs platform independent uint32_t write to iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 * @param[in] value - uint32_t value to write
 */
static void mxvp_wr32(void __iomem *base, int offset, u32 value)
{
    iowrite32(value, base + offset);
}

/*
 * @brief Performs platform independent uint64_t write to iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 * @param[in] value - uint64_t value to write
 */
static void mxvp_wr64(void __iomem *base, int offset, u64 value)
{
    mxvp_wr32(base, offset, value);
    mxvp_wr32(base, offset + sizeof(u32), value >> 32);
}

/*
 * @brief Performs platform independent write to iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 * @param[in] buffer - pointer to location to write
 * @param[in] len - length in bytes to write into iomem
 */
static void mxvp_wr_buffer(void __iomem *base, int offset, void *buffer, size_t len)
{
    memcpy_toio(base + offset, buffer, len);
}

#endif
