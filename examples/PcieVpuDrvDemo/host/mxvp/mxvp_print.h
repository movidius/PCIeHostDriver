/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: Debug prints infrastructure
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXVP_PRINT_HEADER_
#define MXVP_PRINT_HEADER_

#include <linux/module.h>
#include <linux/kernel.h>

#ifdef DEBUG
#define mxvp_debug(fmt, args...) printk("MXVP:DEBUG - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#else
#define mxvp_debug(fmt, args...)
#endif

#define mxvp_error(fmt, args...) printk("MXVP:ERROR - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#define mxvp_info(fmt, args...) printk("MXVP:INFO  - %s(%d) -- "fmt,  __func__, __LINE__, ##args)

#endif
