/*******************************************************************************
 *
 * MX drivers debug prints infrastructure
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef COMMON_MX_PRINT_H_
#define COMMON_MX_PRINT_H_

#include <linux/module.h>
#include <linux/kernel.h>

#ifdef DEBUG
#define mx_dbg(fmt, args...) printk("MX:DEBUG - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#else
#define mx_dbg(fmt, args...)
#endif

#define mx_err(fmt, args...) printk("MX:ERROR - %s(%d) -- "fmt,  __func__, __LINE__, ##args)
#define mx_info(fmt, args...) printk("MX:INFO  - %s(%d) -- "fmt,  __func__, __LINE__, ##args)

#endif /* COMMON_MX_PRINT_H_ */
