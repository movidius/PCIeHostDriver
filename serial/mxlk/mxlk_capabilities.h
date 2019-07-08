/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Device capability management API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef SERIAL_MXLK_MXLK_CAPABILITIES_H_
#define SERIAL_MXLK_MXLK_CAPABILITIES_H_

#include "mxlk.h"
#include "mxlk_common.h"

/*
 * @brief Searches the mxlk mmio space for a capability
 * NOTES:
 *  1) start parameter controls where to start the search. If 0 is used, the
 *     search starts at the capability start offset. Otherwise it uses the
 *     user supplied start at user's own peril
 *
 * @param[in] mxlk  - pointer to mxlk
 * @param[in] start - offset within the mmio to start search
 * @param[in] cap   - pointer to mxlk interface associated with char instance
 *
 * @return:
 *       0 - not found
 *      !0 - kernel virtual pointer to first capability of desired type found
 */
void *mxlk_cap_find(struct mxlk *mxlk, u16 start, u16 cap);

#endif /* SERIAL_MXLK_MXLK_CAPABILITIES_H_ */
