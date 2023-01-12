/*******************************************************************************
 *
 * Intel Myriad-X Vision Processor Driver: PCIe VPU protocol general implementation
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <linux/bitops.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "mxvp.h"
#include "mxvp_pci.h"
#include "mxvp_cmd.h"
#include "mxvp_mem.h"
#include "mxvp_mmio.h"
#include "mxvp_bspec.h"
#include "mxvp_queue.h"
#include "mxvp_print.h"

#define MXVP_PAGE_SIZE (4096u)
#define MXVP_CMD_LIST_ENTRIES (200)
#define MXVP_DMA_LIST_ENTRIES (300)

/*
 * MXVP doorbell parameters
 */
#define MXVP_DOORBELL_ADDR (0xFF0)
#define MXVP_DOORBELL_DATA (0x4E434D44) /* "NCMD" */

#define MXVP_ASYNC_ARGS_SET_POWER_STATE (MXVP_ASYNC_ARGS + 0)

struct mxvp_cmd_list_entry {
    struct mxvp_cmd_list_entry *next;
    struct mxvp_cmd *command;
    mxvp_command_cb  callback;
    void *arg;
};

static int mxvp_bspec_startup_check(struct mxvp *mxvp);

static irqreturn_t mxvp_event_isr(int irq, void *args);
static int  mxvp_events_init(struct mxvp *mxvp);
static void mxvp_events_cleanup(struct mxvp *mxvp);

static int  mxvp_cmd_list_init(struct mxvp_cmd_list *list, size_t entries);
static void mxvp_cmd_list_cleanup(struct mxvp_cmd_list *list);
static void mxvp_cmd_list_flush(struct mxvp_cmd_list *list, u32 reason);
static int  mxvp_cmd_list_add(struct mxvp_cmd_list *list, struct mxvp_cmd *command,
                            mxvp_command_cb callback, void *arg);
static int  mxvp_cmd_list_remove(struct mxvp_cmd_list *list, int position,
                                struct mxvp_cmd_list_entry *entry);
static void mxvp_reply_to_command(struct mxvp_cmd_list *list, struct mxvp_reply *reply);

static void mxvp_dmrq_update_handler(struct work_struct *work);
static void mxvp_retq_update_handler(struct work_struct *work);
static void mxvp_dmaq_preempt_handler(struct work_struct *work);
static void mxvp_cmdq_preempt_handler(struct work_struct *work);
static void mxvp_status_update_handler(struct work_struct *work);

static u32 mxvp_generate_id(struct mxvp *mxvp, int position);
static u32 mxvp_get_seqno(struct mxvp *mxvp);
static int mxvp_id_to_position(u32 id);

int mxvp_bspec_init(struct mxvp *mxvp)
{
    int error;

    if (!mxvp->wq) {
        mxvp_error("missing workqueue\n");
        return -EINVAL;
    }

    if (!mxvp->pci) {
        mxvp_error("missing pci device\n");
        return -EINVAL;
    }

    error = mxvp_pci_init(mxvp);
    if (error) {
        mxvp_error("failed to bring up pci device\n");
        return error;
    }

    error = mxvp_events_init(mxvp);
    if (error) {
        mxvp_error("failed to initialize event dispatcher\n");
        return error;
    }

    error = mxvp_bspec_startup_check(mxvp);
    if (error) {
        return error;
    }

    mxvp_queue_init(&mxvp->cmdq, mxvp->mmio, MXVP_CMDQ_START);
    mxvp_queue_init(&mxvp->retq, mxvp->mmio, MXVP_RETQ_START);
    mxvp_queue_init(&mxvp->dmaq, mxvp->mmio, MXVP_DMAQ_START);
    mxvp_queue_init(&mxvp->dmrq, mxvp->mmio, MXVP_DMRQ_START);

    mxvp->ddr_phys = mxvp_rd64(mxvp->mmio, MXVP_DDR_PHY_ADDRESS);
    mxvp->ddr_size = mxvp_rd32(mxvp->mmio, MXVP_DDR_SIZE) * MXVP_PAGE_SIZE;

    error = mxvp_mem_init(&mxvp->mem, mxvp->ddr_virt, mxvp->ddr_phys, mxvp->ddr_size);
    if (error) {
        mxvp_error("failed to initialize memory manager\n");
        return -1;
    }

    error = mxvp_cmd_list_init(&mxvp->cmd_list, MXVP_CMD_LIST_ENTRIES);
    if (error) {
        mxvp_error("failed to create cmd list with %u entries\n", MXVP_CMD_LIST_ENTRIES);
        return error;
    }

    error = mxvp_cmd_list_init(&mxvp->dma_list, MXVP_DMA_LIST_ENTRIES);
    if (error) {
        mxvp_error("failed to create dma list with %u entries\n", MXVP_DMA_LIST_ENTRIES);
        return error;
    }

    mxvp->async_pending = 0;
    spin_lock_init(&mxvp->async_lock);

    return 0;
}

void mxvp_bspec_cleanup(struct mxvp *mxvp)
{
    mxvp_mem_cleanup(&mxvp->mem);
    mxvp_events_cleanup(mxvp);
    mxvp_cmd_list_cleanup(&mxvp->cmd_list);
    mxvp_cmd_list_cleanup(&mxvp->dma_list);
    mxvp_pci_cleanup(mxvp);
}

int mxvp_send_cmd(struct mxvp *mxvp, struct mxvp_cmd *cmd,
                    mxvp_command_cb cb, void *arg)
{
    int error;
    int position;

    if (!cmd) {
        return -EINVAL;
    }

    switch (cmd->command) {
        case MXVP_CMD_EXE_BUFFER :
        case MXVP_CMD_FENCE :
            /* Save command in command list */
            position = mxvp_cmd_list_add(&mxvp->cmd_list, cmd, cb, arg);
            if (position < 0) {
                mxvp_error("failed to enqueue command %d\n", position);
                return position;
            }

            /* Generate the id and enqueue command */
            cmd->id = mxvp_generate_id(mxvp, position);
            error = mxvp_queue_push(&mxvp->cmdq, cmd, cmd->length);
            if (error) {
                mxvp_error("failed to push command to cmdq %d\n", error);
                mxvp_cmd_list_remove(&mxvp->cmd_list, position, NULL);
                return error;
            }
            break;

        default :
            return -EINVAL;
            break;
    }

    return 0;
}

int mxvp_send_dma(struct mxvp *mxvp, struct mxvp_cmd *cmd,
                    mxvp_command_cb cb, void *arg)
{
    int error;
    int position;

    if (!cmd) {
        return -EINVAL;
    }

    switch (cmd->command) {
        case  MXVP_CMD_DMA_READ :
        case  MXVP_CMD_DMA_WRITE :
        case  MXVP_CMD_MULTI_DMA_READ :
        case  MXVP_CMD_MULTI_DMA_WRITE :
        case  MXVP_CMD_MEM_FILL :
        case  MXVP_CMD_FENCE :
            /* Save command in DMA list */
            position = mxvp_cmd_list_add(&mxvp->dma_list, cmd, cb, arg);
            if (position < 0) {
                mxvp_error("failed to enqueue command %d\n", position);
                return position;
            }

            /* Generate the id and enqueue command */
            cmd->id = mxvp_generate_id(mxvp, position);
            error = mxvp_queue_push(&mxvp->dmaq, cmd, cmd->length);
            if (error) {
                mxvp_error("failed to push command to dmaq %d\n", error);
                mxvp_cmd_list_remove(&mxvp->dma_list, position, NULL);
                return error;
            }
            break;

        default:
            return -EINVAL;
            break;
    }

    return 0;
}

int mxvp_send_async(struct mxvp *mxvp, enum mxvp_async_cmd cmd, void *arg)
{
    volatile u32 done;
    volatile u32 request;

    /* Make sure we have a valid async command */
    if ((cmd < 0) || (cmd >= MXVP_ASYNC_MAX)) {
        return -EINVAL;
    }

    /* Don't allow overlapping async commands of same type */
    if (test_and_set_bit(cmd, &mxvp->async_pending)) {
        return -EBUSY;
    }

    /* Setup any arguments associated with the async command */
    switch (cmd) {
        case MXVP_ASYNC_CMDQ_PREEMPT :
        case MXVP_ASYNC_DMAQ_PREEMPT :
        case MXVP_ASYNC_CMDQ_RESET :
        case MXVP_ASYNC_DMAQ_RESET :
        case MXVP_ASYNC_DEV_RESET :
            break;
        case MXVP_ASYNC_SET_POWER :
            mxvp_wr8(mxvp->mmio,  MXVP_ASYNC_ARGS_SET_POWER_STATE, *(u8 *)arg);
            break;
        default :
            mxvp_error("Invalid async command (%d) detected!\n", cmd);
            return -EINVAL;
    }

    /* Send request and protect request register from concurrent access
     * until requested bit is cleared */
    spin_lock(&mxvp->async_lock);
    mxvp_wr32(mxvp->mmio, MXVP_ASYNC_REQUEST, BIT(cmd));
    mxvp_ring_doorbell(mxvp);
    do
    {
        request = mxvp_rd32(mxvp->mmio, MXVP_ASYNC_REQUEST);
    } while (request & BIT(cmd));
    spin_unlock(&mxvp->async_lock);

    /* Wait for the done bit to be set  */
    do
    {
        done = mxvp_rd32(mxvp->mmio, MXVP_ASYNC_DONE);
    } while (!(done & BIT(cmd)));

    /* Handle any post async actions */
    switch (cmd) {
        case MXVP_ASYNC_CMDQ_PREEMPT :
        case MXVP_ASYNC_CMDQ_RESET :
            mxvp_cmd_list_flush(&mxvp->cmd_list, MXVP_STATUS_DISCARDED);
            break;
        case MXVP_ASYNC_DMAQ_PREEMPT :
        case MXVP_ASYNC_DMAQ_RESET :
            mxvp_cmd_list_flush(&mxvp->dma_list, MXVP_STATUS_DISCARDED);
            break;
        case MXVP_ASYNC_DEV_RESET :
            mxvp_cmd_list_flush(&mxvp->cmd_list, MXVP_STATUS_DISCARDED);
            mxvp_cmd_list_flush(&mxvp->dma_list, MXVP_STATUS_DISCARDED);
            break;
        case MXVP_ASYNC_SET_POWER :
            break;
        default :
            mxvp_error("Invalid async command (%d) detected!\n", cmd);
            return -EINVAL;
    }

    /* Clear command pending bit for future calls */
    test_and_clear_bit(cmd, &mxvp->async_pending);

    return 0;
}

int mxvp_ring_doorbell(struct mxvp *mxvp)
{
    u32 value  = MXVP_DOORBELL_DATA;
    int offset = MXVP_DOORBELL_ADDR;

    return pci_write_config_dword(mxvp->pci, offset, value);
}

s16 mxvp_read_temperature(struct mxvp *mxvp)
{
    return (s16) mxvp_rd16(mxvp->mmio, MXVP_TEMPERATURE);
}

static u32 mxvp_generate_id(struct mxvp *mxvp, int position)
{
    u32 id;

    /* upper 16 bits are unique seqno, lower 16 are used to recover command */
    id  = mxvp_get_seqno(mxvp) << 16;
    id |= (u16) position;

    return id;
}

static u32 mxvp_get_seqno(struct mxvp *mxvp)
{
    return (u32) atomic_fetch_add(1, &mxvp->seqno);
}

static int mxvp_id_to_position(u32 id)
{
    /* position is embedded in lower 16 bits of id */
    return (int) (id & 0xFFFF);
}

static int mxvp_bspec_startup_check(struct mxvp *mxvp)
{
    int retries = 10;
    int error;
    int length;
    u32 date;
    u16 reply_order;
    char main_magic[sizeof(MXVP_MAIN_MAGIC_STR)];

    length = strlen(MXVP_MAIN_MAGIC_STR);

    /* Check for main magic string for 10ms before giving up */
    do {
        msleep(1);
        memset(main_magic, 0, sizeof(main_magic));
        mxvp_rd_buffer(mxvp->mmio, MXVP_MAIN_MAGIC, main_magic, length);
        error = memcmp(main_magic, MXVP_MAIN_MAGIC_STR, sizeof(main_magic));
    } while (error && (retries-- > 0));

    if (error) {
        mxvp_error("failed to read main magic\n");
        return -EIO;
    }

    /* Set one time reply order */
    reply_order  = mxvp_rd16(mxvp->mmio, MXVP_REPLY_ORDER);
    reply_order |= field_set(MXVP_RO_IN_ORDER, 1) | field_set(MXVP_RO_VALID, 1);
    mxvp_wr16(mxvp->mmio, MXVP_REPLY_ORDER, reply_order);

    mxvp_rd_buffer(mxvp->mmio, MXVP_MAIN_MAGIC + 12, &date, sizeof(date));
    mxvp_debug("main magic : %s%04X\n", main_magic, date);

    return 0;
}

static irqreturn_t mxvp_event_isr(int irq, void *args)
{
    struct mxvp *mxvp = args;
    u32 pending = mxvp_rd32(mxvp->mmio, MXVP_INTR_PENDING);

    /* Check which events were generated and schedule work */
    if (field_get(MXVP_INTR_DMRQ_UPDATE, pending)) {
        queue_work(mxvp->wq, &mxvp->dmrq_update);
    }

    if (field_get(MXVP_INTR_RETQ_UPDATE, pending)) {
        queue_work(mxvp->wq, &mxvp->retq_update);
    }

    if (field_get(MXVP_INTR_DMAQ_PREEMPTED, pending)) {
        queue_work(mxvp->wq, &mxvp->dmaq_preempt);
    }

    if (field_get(MXVP_INTR_CMDQ_PREEMPTED, pending)) {
        queue_work(mxvp->wq, &mxvp->cmdq_preempt);
    }

    if (field_get(MXVP_INTR_STATUS_UPDATE, pending)) {
        queue_work(mxvp->wq, &mxvp->status_update);
    }

    /* Report all events handled to VPU */
    mxvp_wr32(mxvp->mmio, MXVP_INTR_STATUS, pending);
    mxvp_wr32(mxvp->mmio, MXVP_INTR_ACK, 0);
    mxvp_ring_doorbell(mxvp);

    /* Report MSI handled */
    return IRQ_HANDLED;
}

static int mxvp_events_init(struct mxvp *mxvp)
{
    int irq;
    int error;
    u32 enable;

    error = pci_alloc_irq_vectors(mxvp->pci, 1, 1, PCI_IRQ_MSI);
    if (error < 0) {
        mxvp_error("failed to allocate %d MSI vectors - %d\n", 1, error);
        return error;
    }

    irq = pci_irq_vector(mxvp->pci, 0);
    error = request_irq(irq, mxvp_event_isr, 0, MXVP_DRIVER_NAME, mxvp);
    if (error) {
        mxvp_error("failed to request irqs - %d\n", error);
        return error;
    }

    INIT_WORK(&mxvp->dmrq_update, mxvp_dmrq_update_handler);
    INIT_WORK(&mxvp->retq_update, mxvp_retq_update_handler);
    INIT_WORK(&mxvp->dmaq_preempt, mxvp_dmaq_preempt_handler);
    INIT_WORK(&mxvp->cmdq_preempt, mxvp_cmdq_preempt_handler);
    INIT_WORK(&mxvp->status_update, mxvp_status_update_handler);

    enable = field_set(MXVP_INTR_DMRQ_UPDATE, 1) |
             field_set(MXVP_INTR_RETQ_UPDATE, 1) |
             field_set(MXVP_INTR_DMAQ_PREEMPTED, 1) |
             field_set(MXVP_INTR_CMDQ_PREEMPTED, 1) |
             field_set(MXVP_INTR_STATUS_UPDATE, 1);

    mxvp_wr32(mxvp->mmio, MXVP_INTR_STATUS, 0);
    mxvp_wr32(mxvp->mmio, MXVP_INTR_ACK, 0);
    mxvp_wr32(mxvp->mmio, MXVP_INTR_ENABLE, enable);

    return 0;
}

static void mxvp_events_cleanup(struct mxvp *mxvp)
{
    int irq = pci_irq_vector(mxvp->pci, 0);

    mxvp_wr32(mxvp->mmio, MXVP_INTR_ENABLE, 0);
    mxvp_wr32(mxvp->mmio, MXVP_INTR_STATUS, 0);
    mxvp_wr32(mxvp->mmio, MXVP_INTR_ACK, 0);

    cancel_work_sync(&mxvp->dmrq_update);
    cancel_work_sync(&mxvp->retq_update);
    cancel_work_sync(&mxvp->dmaq_preempt);
    cancel_work_sync(&mxvp->cmdq_preempt);
    cancel_work_sync(&mxvp->status_update);

    synchronize_irq(irq);
    free_irq(irq, mxvp);
    pci_free_irq_vectors(mxvp->pci);
}

static int mxvp_cmd_list_init(struct mxvp_cmd_list *list, size_t entries)
{
    int index;
    struct mxvp_cmd_list_entry *entry;
    size_t length = sizeof(struct mxvp_cmd_list_entry) * entries;

    list->base = kzalloc(length, GFP_KERNEL);
    if (!list->base) {
        return -ENOMEM;
    }
    list->entries = entries;
    entry = list->free = list->base;
    for (index = 0; index < (entries - 1); index++) {
        entry[index].next = &entry[index + 1];
    }
    entry[index].next = NULL;
    spin_lock_init(&list->lock);

    return 0;
}

static void mxvp_cmd_list_flush(struct mxvp_cmd_list *list, u32 reason)
{
    int index;
    struct mxvp_reply reply;
    struct mxvp_cmd_list_entry entry;

    reply.length  = sizeof(reply);
    reply.version = MXVP_REPLY_HDR_VERSION;
    reply.status  = reason;
    for (index = 0; index < list->entries; index++) {
        mxvp_cmd_list_remove(list, index, &entry);
        if (entry.command) {
            if (entry.callback) {
                reply.id = entry.command->id;
                entry.callback(entry.command, &reply, entry.arg);
            }
        }
    }
}

static void mxvp_cmd_list_cleanup(struct mxvp_cmd_list *list)
{
    mxvp_cmd_list_flush(list, MXVP_STATUS_DISCARDED);
    kfree(list->base);
}

static int mxvp_cmd_list_add(struct mxvp_cmd_list *list,
                            struct mxvp_cmd *command,
                            mxvp_command_cb callback, void *arg)
{
    struct mxvp_cmd_list_entry *entry;

    spin_lock(&list->lock);
    entry = list->free;
    if (entry) {
        list->free = entry->next;
        spin_unlock(&list->lock);
    } else {
        spin_unlock(&list->lock);
        return -ENOSPC;
    }

    entry->command = command;
    entry->callback = callback;
    entry->arg = arg;

    return entry - list->base;
}

static int mxvp_cmd_list_remove(struct mxvp_cmd_list *list,
                                int position,
                                struct mxvp_cmd_list_entry *entry)
{
    struct mxvp_cmd_list_entry *lower = list->base;
    struct mxvp_cmd_list_entry *upper = list->base + list->entries;
    struct mxvp_cmd_list_entry *swap  = list->base + position;

    spin_lock(&list->lock);
    if ((swap >= lower) && (swap < upper)) {
        if (entry) {
            *entry = *swap;
            entry->next = NULL;
        }
        memset(swap, 0, sizeof(*swap));
        swap->next = list->free;
        list->free = swap;
        spin_unlock(&list->lock);
    } else {
        spin_unlock(&list->lock);
        mxvp_error("failed to recover entry %p - lower %p - upper %p\n",
                    swap, lower, upper);
        return -EFAULT;
    }

    return 0;
}

static void mxvp_reply_to_command(struct mxvp_cmd_list *list,
                                  struct mxvp_reply *reply)
{
    int error;
    int position = mxvp_id_to_position(reply->id);
    struct mxvp_cmd_list_entry entry;

    error = mxvp_cmd_list_remove(list, position, &entry);
    if (!error) {
        if (entry.callback) {
            entry.callback(entry.command, reply, entry.arg);
        }
    }
}

static void mxvp_dmrq_update_handler(struct work_struct *work)
{
    int budget = 20;
    struct mxvp *mxvp = container_of(work, struct mxvp, dmrq_update);

    while (budget--) {
        if (!mxvp_queue_is_empty(&mxvp->dmrq)) {
            struct mxvp_reply *reply = mxvp_queue_pull(&mxvp->dmrq);
            mxvp_reply_to_command(&mxvp->dma_list, reply);
            kfree(reply);
        } else {
            return;
        }
    }
    queue_work(mxvp->wq, &mxvp->dmrq_update);
}

static void mxvp_retq_update_handler(struct work_struct *work)
{
    int budget = 20;
    struct mxvp *mxvp = container_of(work, struct mxvp, retq_update);

    while (budget--) {
        if (!mxvp_queue_is_empty(&mxvp->retq)) {
            struct mxvp_reply *reply = mxvp_queue_pull(&mxvp->retq);
            mxvp_reply_to_command(&mxvp->cmd_list, reply);
            kfree(reply);
        } else {
            return;
        }
    }
    queue_work(mxvp->wq, &mxvp->retq_update);
}

static void mxvp_dmaq_preempt_handler(struct work_struct *work)
{

}

static void mxvp_cmdq_preempt_handler(struct work_struct *work)
{

}

static void mxvp_status_update_handler(struct work_struct *work)
{

}
