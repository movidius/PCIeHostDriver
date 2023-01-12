/*******************************************************************************
 *
 * Intel Myriad-X Test Framework Driver: Test operations API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXTF_TEST_HEADER_
#define MXTF_TEST_HEADER_

#include "mxtf.h"

/*
 * @brief Initialize mxtf device instance
 *
 * @param[in] mxtf - pointer to mxtf object
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int  mxtf_test_init(struct mxtf *mxtf);

/*
 * @brief Cleanup of mxtf device instance
 *
 * @param[in] mxtf - pointer to mxtf object
 */
void mxtf_test_cleanup(struct mxtf *mxtf);

#endif
