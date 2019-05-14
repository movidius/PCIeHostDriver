/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/
#ifndef MXBL_PRINT_HEADER_
#define MXBL_PRINT_HEADER_

#include <linux/module.h>
#include <linux/kernel.h>

#ifdef DEBUG
#define mxbl_debug(fmt, args...) printk("MXBL:DEBUG - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#else
#define mxbl_debug(fmt, args...)
#endif

#define mxbl_error(fmt, args...) printk("MXBL:ERROR - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#define mxbl_info(fmt, args...) printk("MXBL:INFO  - %s(%d) -- "fmt,  __func__, __LINE__, ##args)

#endif
