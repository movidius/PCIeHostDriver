/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Device capability management
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include "mxlk_capabilities.h"

#define MXLK_CAP_TTL        (32)

void *mxlk_cap_find(struct mxlk *mxlk, u16 start, u16 id)
{
    int ttl = MXLK_CAP_TTL;
    struct mxlk_cap_hdr *hdr;

    /* If user didn't specify start, assume start of mmio */
    if (!start) {
        start = mx_rd32(mxlk->mmio, offsetof(struct mxlk_mmio, cap_offset));
    }

    /* Read header info */
    hdr = (struct mxlk_cap_hdr *) (mxlk->mmio + start);
    /* Check if we still have time to live */
    while (ttl--) {
        /* If cap matches, return header */
        if (hdr->id == id) {
            return hdr;
        }
        /* If cap is NULL, we are at the end of the list */
        else if (hdr->id == MXLK_CAP_NULL) {
            return NULL;
        }
        /* If no match and no end of list, traverse the linked list */
        else {
            hdr = (struct mxlk_cap_hdr *) (mxlk->mmio + hdr->next);
        }
    }

    /* If we reached here, the capability list is corrupted */
    return NULL;
}
