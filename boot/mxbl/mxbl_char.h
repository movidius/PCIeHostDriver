/*******************************************************************************
 *
 * Intel Myriad-X Bootloader Driver
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef BOOT_MXBL_MXBL_CHAR_H_
#define BOOT_MXBL_MXBL_CHAR_H_

#include "mxbl.h"

/*
 * @brief Initializes mxbl char component
 * NOTES:
 *  1) To be called at module init
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int  mxbl_chrdev_init(void);

/*
 * @brief Cleans up mxbl char component
 * NOTES:
 *  1) To be called at module remove
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int  mxbl_chrdev_exit(void);

/*
 * @brief Adds char interface associated with mxbl interface
 *
 * @param[in] mxbl - pointer to mxbl object
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int  mxbl_chrdev_add(struct mxbl *mxbl);

/*
 * @brief Removes char interface associated with mxbl interface
 *
 * @param[in] mxbl - pointer to mxbl object
 *
 */
void mxbl_chrdev_remove(struct mxbl *mxbl);

#endif /* BOOT_MXBL_MXBL_CHAR_H_ */
