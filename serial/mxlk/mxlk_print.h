/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Debug prints API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXLK_PRINT_HEADER_
#define MXLK_PRINT_HEADER_

#include <linux/module.h>
#include <linux/kernel.h>

#ifdef DEBUG
#define mxlk_debug(fmt, args...) printk("MXLK:DEBUG - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#else
#define mxlk_debug(fmt, args...)
#endif

#define mxlk_error(fmt, args...) printk("MXLK:ERROR - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#define mxlk_info(fmt, args...) printk("MXLK:INFO  - %s(%d) -- "fmt,  __func__, __LINE__, ##args)

#endif
