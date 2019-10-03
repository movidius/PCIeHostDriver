/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Character device API
 *
 * Copyright (C) 2018 - 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef SERIAL_MXLK_MXLK_CHAR_H_
#define SERIAL_MXLK_MXLK_CHAR_H_

#include "mxlk.h"

/*
 * @brief Initializes mxlk char component
 * NOTES:
 *  1) To be called at module init
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxlk_chrdev_init(void);

/*
 * @brief Cleans up mxlk char component
 * NOTES:
 *  1) To be called at module remove
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxlk_chrdev_exit(void);

/*
 * @brief Adds char interface associated with mxlk interface
 *
 * @param[in] inf - pointer to mxlk interface associated with char instance
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int mxlk_chrdev_add(struct mxlk_interface *inf);

/*
 * @brief Removes char interface associated with mxlk interface
 *
 * @param[in] inf - pointer to mxlk interface associated with char instance
 *
 */
void mxlk_chrdev_remove(struct mxlk_interface *inf);

#endif /* SERIAL_MXLK_MXLK_CHAR_H_ */
