/*******************************************************************************
 *
 * Intel Myriad-X Test Framework Driver: Debug prints infrastructure
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXTF_PRINT_HEADER_
#define MXTF_PRINT_HEADER_

#include <linux/module.h>
#include <linux/kernel.h>

#ifdef DEBUG
#define mxtf_debug(fmt, args...) printk("MXTF:DEBUG - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#else
#define mxtf_debug(fmt, args...)
#endif

#define mxtf_error(fmt, args...) printk("MXTF:ERROR - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#define mxtf_info(fmt, args...) printk("MXTF:INFO  - %s(%d) -- "fmt,  __func__, __LINE__, ##args)

#endif
