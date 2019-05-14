/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: VPU command queues API
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef MXVP_QUEUE_HEADER_
#define MXVP_QUEUE_HEADER_

#include <linux/kernel.h>
#include <linux/spinlock.h>

struct mxvp_queue {
    void __iomem *control;
    void __iomem *memory;
    u32 size;
    spinlock_t lock;
};

/*
 * @brief Initializes queue object
 *
 * @param[in] queue  - pointer to queue object
 * @param[in] base   - pointer to remapped BAR2/MMIO area
 * @param[in] offset - offset within MMIO of queue control fields
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int   mxvp_queue_init(struct mxvp_queue *queue, void *base, size_t offset);

/*
 * @brief Resets queue to known state (empty)
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Function uses
 *
 * @param[in] queue  - pointer to queue object
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int   mxvp_queue_reset(struct mxvp_queue *queue);

/*
 * @brief Enqueues element into queue
 *
 * NOTES:
 *  1) Thread-safe
 *  2) The function will round up length to the next multiple of uint32_t
 *
 * @param[in] queue - pointer to queue object
 * @param[in] element - pointer to message to be copied to queue
 * @param[in] length - size in bytes
 *
 * @return:
 *       0 - success
 *      <0 - linux error code
 */
int   mxvp_queue_push(struct mxvp_queue *queue, void *element, size_t length);

/*
 * @brief Dequeues element into queue
 *
 * NOTES:
 *  1) Thread-safe
 *  2) Function allocates from GFP_ATOMIC
 *  3) Failure to allocate memory will result in no attempt to dequeue
 *  4) Caller is responsible for freeing element returned by function (kfree)
 *
 * @param[in] queue - pointer to queue object
 *
 * @return:
 *       NULL - no element dequeued
 *      !NULL - pointer to dequeued element; following reply format, length of
 *              element in bytes is stored in first uint32_t
 */
void *mxvp_queue_pull(struct mxvp_queue *queue);

/*
 * @brief Checks if queue is empty
 *
 * NOTES:
 *  1) Thread-safe
 *
 * @param[in] queue - pointer to queue object
 *
 * @return:
 *       true : queue is empty
 *      false : queue has pending elements
 */
bool  mxvp_queue_is_empty(struct mxvp_queue *queue);

#endif
