/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXBL_MMIO_HEADER_
#define MXBL_MMIO_HEADER_

#include <linux/module.h>

/* MMIO field helper macros */
#define field_mask(field)       ((1 << field##_WIDTH) - 1)
#define field_set(field, value) (((value) & field_mask(field)) << field##_SHIFT)
#define field_get(field, value) (((value) >> field##_SHIFT) & field_mask(field))

/* Myriad-X MMIO register offsets and layout */
#define MXBL_MAIN_MAGIC     (0x00)
#define MXBL_MF_READY       (0x10)
#define MXBL_MF_LENGTH         (0x14)
#define MXBL_MF_START          (0x20)
#define MXBL_INT_ENABLE     (0x28)
#define MXBL_INT_MASK       (0x2C)
#define MXBL_INT_IDENTITY   (0x30)
    #define MXBL_INT_DMRQ_UPDATE_SHIFT    (4)
    #define MXBL_INT_DMRQ_UPDATE_WIDTH    (1)
    #define MXBL_INT_RETQ_UPDATE_SHIFT    (3)
    #define MXBL_INT_RETQ_UPDATE_WIDTH    (1)
    #define MXBL_INT_DMAQ_PREEMPTED_SHIFT (2)
    #define MXBL_INT_DMAQ_PREEMPTED_WIDTH (1)
    #define MXBL_INT_CMDQ_PREEMPTED_SHIFT (1)
    #define MXBL_INT_CMDQ_PREEMPTED_WIDTH (1)
    #define MXBL_INT_STATUS_UPDATE_SHIFT  (0)
    #define MXBL_INT_STATUS_UPDATE_WIDTH  (1)
/*
 * @brief Performs platform independent uint8_t read from iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 *
 * @return:
 *       Value read from location as uint8_t
 */
static u8 mxbl_rd8(void __iomem *base, int offset)
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
static u16 mxbl_rd16(void __iomem *base, int offset)
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
static u32 mxbl_rd32(void __iomem *base, int offset)
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
static u64 mxbl_rd64(void __iomem *base, int offset)
{
    u64 low;
    u64 high;

    low  = mxbl_rd32(base, offset);
    high = mxbl_rd32(base, offset + sizeof(u32));

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
static void mxbl_rd_buffer(void __iomem *base, int offset, void *buffer, size_t len)
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
static void mxbl_wr8(void __iomem *base, int offset, u8 value)
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
static void mxbl_wr16(void __iomem *base, int offset, u16 value)
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
static void mxbl_wr32(void __iomem *base, int offset, u32 value)
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
static void mxbl_wr64(void __iomem *base, int offset, u64 value)
{
    mxbl_wr32(base, offset, value);
    mxbl_wr32(base, offset + sizeof(u32), value >> 32);
}

/*
 * @brief Performs platform independent write to iomem
 *
 * @param[in] base - pointer to base of remapped iomem
 * @param[in] offset - offset within iomem to read from
 * @param[in] buffer - pointer to location to write
 * @param[in] len - length in bytes to write into iomem
 */
static void mxbl_wr_buffer(void __iomem *base, int offset, void *buffer, size_t len)
{
    memcpy_toio(base + offset, buffer, len);
}

#endif
