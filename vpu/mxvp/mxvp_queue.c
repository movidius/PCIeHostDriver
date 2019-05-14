/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: VPU command queues management
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/slab.h>

#include "mxvp_mmio.h"
#include "mxvp_queue.h"

/* Offsets within the queue's control structure, to match bspec definition */
#define QCONTROL_START (0x00)
#define QCONTROL_SIZE  (0x04)
#define QCONTROL_HEAD  (0x08)
#define QCONTROL_TAIL  (0x0C)

#define QUEUE_WRAP_MARKER    ((u32) -1)
#define QUEUE_ALIGNMENT      (sizeof(u32))

#define diff(a,b) ({ \
    typeof(a) _a = a; typeof(b) _b = b; \
    ((_a > _b) ? _a - _b : _b - _a); })


static bool is_empty(struct mxvp_queue *queue)
{
    u32 head = mxvp_rd32(queue->control, QCONTROL_HEAD);
    u32 tail = mxvp_rd32(queue->control, QCONTROL_TAIL);
    return (tail == head);
}

static void enqueue(struct mxvp_queue *queue, u32 tail, void *element, u32 length)
{
    mxvp_wr_buffer(queue->memory, tail, element, length);

    tail += length;
    mxvp_wr32(queue->control, QCONTROL_TAIL, tail);
}

static void dequeue(struct mxvp_queue *queue, u32 head, void *element, u32 length)
{
    mxvp_rd_buffer(queue->memory, head, element, length);

    head += length;
    mxvp_wr32(queue->control, QCONTROL_HEAD, head);
}

int mxvp_queue_init(struct mxvp_queue *queue, void *base, size_t offset)
{
    queue->control = base + offset;
    queue->memory  = base + mxvp_rd32(queue->control, QCONTROL_START);
    queue->size    = mxvp_rd32(queue->control, QCONTROL_SIZE);
    spin_lock_init(&queue->lock);

    return 0;
}

int mxvp_queue_reset(struct mxvp_queue *queue)
{
    spin_lock(&queue->lock);
    mxvp_wr32(queue->control, QCONTROL_TAIL, 0);
    mxvp_wr32(queue->control, QCONTROL_HEAD, 0);
    spin_unlock(&queue->lock);

    return 0;
}

int mxvp_queue_push(struct mxvp_queue *queue, void *element, size_t length)
{
    int err = 0;
    u32 head;
    u32 tail;
    u32 space_req;

    length = roundup(length, QUEUE_ALIGNMENT);
    space_req = (u32) length + sizeof(QUEUE_WRAP_MARKER);

    spin_lock(&queue->lock);
    head = mxvp_rd32(queue->control, QCONTROL_HEAD);
    tail = mxvp_rd32(queue->control, QCONTROL_TAIL);
    if (tail >= head) {
        /* Is there enough space left at end of queue */
        if (diff(tail, queue->size) > space_req)  {
            enqueue(queue, tail, element, length);
        /* If not enough space left at end, check start */
        } else if (head > space_req) {
            mxvp_wr32(queue->memory , tail, QUEUE_WRAP_MARKER);
            enqueue(queue, 0, element, length);
        /* No room at start or end, return failed */
        } else {
            err = -ENOSPC;
        }
    } else {
        if (diff(head, tail) > space_req) {
            enqueue(queue, tail, element, length);
        } else {
            err = -ENOSPC;
        }
    }
    spin_unlock(&queue->lock);

    return err;
}

void *mxvp_queue_pull(struct mxvp_queue *queue)
{
    u32 head;
    u32 length;
    void *element = NULL;

    spin_lock(&queue->lock);
retry :
    if (!is_empty(queue)) {
        head   = mxvp_rd32(queue->control, QCONTROL_HEAD);
        length = mxvp_rd32(queue->memory, head);
        if (length == QUEUE_WRAP_MARKER) {
            mxvp_wr32(queue->control, QCONTROL_HEAD, 0);
            goto retry;
        }
        element = kmalloc(length, GFP_ATOMIC);
        if (element) {
            dequeue(queue, head, element, length);
        }
    }
    spin_unlock(&queue->lock);

    return element;
}

bool mxvp_queue_is_empty(struct mxvp_queue *queue)
{
    bool empty;

    spin_lock(&queue->lock);
    empty = is_empty(queue);
    spin_unlock(&queue->lock);

    return empty;
}
