/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Data transfer engine
 *
 * Copyright (C) 2018 - 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <linux/uaccess.h>
#include <linux/delay.h>

#include "mx_pci.h"
#include "mx_boot.h"
#include "mx_reset.h"

#include "mxlk_char.h"
#include "mxlk_core.h"
#include "mxlk_capabilities.h"
#include "mxlk_ioctl.h"

/* Doorbell parameters. */
#define MXLK_DOORBELL_ADDR (0xFF0)
#define MXLK_DOORBELL_DATA (0x72696E67) /* "RING" */

#define MXLK_CIRCULAR_INC(val, max) (((val) + 1) % (max))

/* Used to avoid processing invalid head and tail pointers that we might read
   when MX device resets itself. */
#define INVALID(qptr) (qptr == 0xFFFFFFFF)

static atomic_t units_found = ATOMIC_INIT(0);

static int rx_pool_size = 5 * 1024 * 1024;
module_param(rx_pool_size, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(rx_pool_size, "receive pool size (default 5MB)");

static int tx_pool_size = 5 * 1024 * 1024;
module_param(tx_pool_size, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(tx_pool_size, "transmit pool size (default 5MB)");


static ssize_t mxlk_debug_show(struct device *dev,
                               struct device_attribute *attr, char *buf);
static ssize_t mxlk_debug_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count);

static int mxlk_version_check(struct mxlk *mxlk);
static void mxlk_set_host_status(struct mxlk *mxlk, int status);
static int mxlk_get_device_status(struct mxlk *mxlk);

static int mxlk_list_init(struct mxlk_list *list);
static void mxlk_list_cleanup(struct mxlk_list *list);
static int mxlk_list_put(struct mxlk_list *list, struct mxlk_buf_desc *bd);
static struct mxlk_buf_desc *mxlk_list_get(struct mxlk_list *list);
static void mxlk_list_info(struct mxlk_list *list, size_t *bytes, size_t *buffers);

static struct mxlk_buf_desc *mxlk_alloc_bd(size_t length);
static void mxlk_free_bd(struct mxlk_buf_desc *bd);
static struct mxlk_buf_desc *mxlk_alloc_rx_bd(struct mxlk *mxlk);
static void mxlk_free_rx_bd(struct mxlk *mxlk, struct mxlk_buf_desc * bd);
static struct mxlk_buf_desc *mxlk_alloc_tx_bd(struct mxlk *mxlk);
static void mxlk_free_tx_bd(struct mxlk *mxlk, struct mxlk_buf_desc * bd);

static int mxlk_all_chrdev_init(struct mxlk *mxlk);
static void mxlk_all_chrdev_cleanup(struct mxlk *mxlk);
static void mxlk_interfaces_init(struct mxlk *mxlk);
static void mxlk_interfaces_cleanup(struct mxlk *mxlk);
static void mxlk_interface_init(struct mxlk *mxlk, int id);
static void mxlk_interface_cleanup(struct mxlk_interface *inf);
static void mxlk_add_bd_to_interface(struct mxlk *mxlk, struct mxlk_buf_desc *bd);

static int mxlk_discover_txrx(struct mxlk *mxlk);
static int mxlk_txrx_init(struct mxlk *mxlk, struct mxlk_cap_txrx *cap);
static void mxlk_txrx_cleanup(struct mxlk *mxlk);

static void mxlk_set_td_address(struct mxlk_transfer_desc *td, u64 address);
static u64  mxlk_get_td_address(struct mxlk_transfer_desc *td);
static void mxlk_set_td_length(struct mxlk_transfer_desc *td, u32 length);
static u32  mxlk_get_td_length(struct mxlk_transfer_desc *td);
static void mxlk_set_td_interface(struct mxlk_transfer_desc *td, u16 interface);
static u16  mxlk_get_td_interface(struct mxlk_transfer_desc *td);
static void mxlk_set_td_status(struct mxlk_transfer_desc *td, u16 status);
static u16  mxlk_get_td_status(struct mxlk_transfer_desc *td);

static void mxlk_set_tdr_head(struct mxlk_pipe *p, u32 head);
static u32  mxlk_get_tdr_head(struct mxlk_pipe *p);
static void mxlk_set_tdr_tail(struct mxlk_pipe *p, u32 tail);
static u32  mxlk_get_tdr_tail(struct mxlk_pipe *p);

static irqreturn_t mxlk_interrupt(int irq, void *args);
static int mxlk_events_init(struct mxlk *mxlk);
static void mxlk_events_cleanup(struct mxlk *mxlk);
static void mxlk_rx_event_handler(struct work_struct *work);
static void mxlk_tx_event_handler(struct work_struct *work);
static void mxlk_status_event_handler(struct work_struct *work);
static void mxlk_send_doorbell_handler(struct work_struct *work);
static void mxlk_start_tx(struct mxlk *mxlk);
static void mxlk_start_rx(struct mxlk *mxlk);
static void mxlk_send_doorbell(struct mxlk *mxlk);
static void mxlk_ring_doorbell(struct mxlk *mxlk);
static int mxlk_comms_init(struct mxlk *mxlk);
static void mxlk_comms_cleanup(struct mxlk *mxlk);

static int mxlk_map_dma(struct mxlk *mxlk, struct mxlk_dma_desc *dd,
                        int direction);
static void mxlk_unmap_dma(struct mxlk *mxlk, struct mxlk_dma_desc *dd,
                           int direction);

static ssize_t mxlk_debug_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
    struct mxlk *mxlk = pci_get_drvdata(pdev);
    struct mxlk_stats new = mxlk->stats;

    snprintf(buf, 4096,
        "tx_krn, pkts %zu (%zu) bytes %zu (%zu)\n"
        "tx_usr, pkts %zu (%zu) bytes %zu (%zu)\n"
        "rx_krn, pkts %zu (%zu) bytes %zu (%zu)\n"
        "rx_usr, pkts %zu (%zu) bytes %zu (%zu)\n"
        "interrupts %zu (%zu) doorbells %zu (%zu)\n"
        "rx runs %zu (%zu) tx runs %zu (%zu)\n",
        new.tx_krn.pkts,   (new.tx_krn.pkts   - mxlk->stats_old.tx_krn.pkts),
        new.tx_krn.bytes,  (new.tx_krn.bytes  - mxlk->stats_old.tx_krn.bytes),
        new.tx_usr.pkts,   (new.tx_usr.pkts   - mxlk->stats_old.tx_usr.pkts),
        new.tx_usr.bytes,  (new.tx_usr.bytes  - mxlk->stats_old.tx_usr.bytes),
        new.rx_krn.pkts,   (new.rx_krn.pkts   - mxlk->stats_old.rx_krn.pkts),
        new.rx_krn.bytes,  (new.rx_krn.bytes  - mxlk->stats_old.rx_krn.bytes),
        new.rx_usr.pkts,   (new.rx_usr.pkts   - mxlk->stats_old.rx_usr.pkts),
        new.rx_usr.bytes,  (new.rx_usr.bytes  - mxlk->stats_old.rx_usr.bytes),
        new.interrupts,    (new.interrupts    - mxlk->stats_old.interrupts),
        new.doorbells,     (new.doorbells     - mxlk->stats_old.doorbells),
        new.rx_event_runs, (new.rx_event_runs - mxlk->stats_old.rx_event_runs),
        new.tx_event_runs, (new.tx_event_runs - mxlk->stats_old.tx_event_runs));

    mxlk->stats_old = new;

    return strlen(buf);
}

static ssize_t mxlk_debug_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
    struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
    struct mxlk *mxlk = pci_get_drvdata(pdev);

    memset(&mxlk->stats_old, 0, sizeof(struct mxlk_stats));
    memset(&mxlk->stats,     0, sizeof(struct mxlk_stats));

    return count;
}

static int mxlk_version_check(struct mxlk *mxlk)
{
    struct mxlk_version version;

    mx_rd_buf(mxlk->mmio, MXLK_MMIO_VERSION, &version, sizeof(version));

    if ((version.major != MXLK_VERSION_MAJOR) ||
        (version.minor != MXLK_VERSION_MINOR)) {
        mx_err("version mismatch, dev : %d.%d.%d, host : %d.%d.%d\n",
               version.major, version.minor, version.build,
               MXLK_VERSION_MAJOR, MXLK_VERSION_MINOR, MXLK_VERSION_BUILD);

        return -EIO;
    }

    mx_info("versions, dev : %d.%d.%d, host : %d.%d.%d\n",
            version.major, version.minor, version.build,
            MXLK_VERSION_MAJOR, MXLK_VERSION_MINOR, MXLK_VERSION_BUILD);

    return 0;
}

static void mxlk_set_host_status(struct mxlk *mxlk, int status)
{
    mxlk->status = status;
    mx_wr32(mxlk->mmio, MXLK_MMIO_HOST_STATUS, status);
}

static int mxlk_get_device_status(struct mxlk *mxlk)
{
    return mx_rd32(mxlk->mmio, MXLK_MMIO_DEV_STATUS);
}

static int mxlk_list_init(struct mxlk_list *list)
{
    spin_lock_init(&list->lock);
    list->bytes = 0;
    list->buffers = 0;
    list->head = NULL;
    list->tail = NULL;

    return 0;
}

static void mxlk_list_cleanup(struct mxlk_list *list)
{
    struct mxlk_buf_desc *bd;

    spin_lock(&list->lock);
    while (list->head) {
        bd = list->head;
        list->head = bd->next;
        mxlk_free_bd(bd);
    }

    list->head = list->tail = NULL;
    spin_unlock(&list->lock);
}

static int mxlk_list_put(struct mxlk_list *list, struct mxlk_buf_desc *bd)
{
    if (!bd) {
        mx_err("attempt to add null buf desc to list!\n");
        return -EINVAL;
    }

    spin_lock(&list->lock);
    if (list->head) {
        list->tail->next = bd;
    } else {
        list->head = bd;
    }
    while (bd) {
        list->tail = bd;
        list->bytes += bd->length;
        list->buffers++;
        bd = bd->next;
    }
    spin_unlock(&list->lock);

    return 0;
}

static struct mxlk_buf_desc *mxlk_list_get(struct mxlk_list *list)
{
    struct mxlk_buf_desc *bd;

    spin_lock(&list->lock);
    bd = list->head;
    if (list->head) {
        list->head = list->head->next;
        if (!list->head) {
            list->tail = NULL;
        }
        bd->next = NULL;
        list->bytes -= bd->length;
        list->buffers--;
    }
    spin_unlock(&list->lock);

    return bd;
}

static void mxlk_list_info(struct mxlk_list *list, size_t *bytes, size_t *buffers)
{
    spin_lock(&list->lock);
    *bytes = list->bytes;
    *buffers = list->buffers;
    spin_unlock(&list->lock);
}

static struct mxlk_buf_desc *mxlk_alloc_bd(size_t length)
{
    struct mxlk_buf_desc *bd;

    bd = kzalloc(sizeof(*bd), GFP_KERNEL);
    if (!bd) {
        return NULL;
    }

    bd->head = kzalloc(roundup(length, cache_line_size()), GFP_KERNEL);
    if (!bd->head) {
        kfree(bd);
        return NULL;
    }

    bd->data = bd->head;
    bd->length = bd->true_len = length;
    bd->next = NULL;

    return bd;
}

static void mxlk_free_bd(struct mxlk_buf_desc *bd)
{
    if (bd) {
        kfree(bd->head);
        kfree(bd);
    }
}

static struct mxlk_buf_desc *mxlk_alloc_rx_bd(struct mxlk *mxlk)
{
    struct mxlk_buf_desc *bd;

    bd = mxlk_list_get(&mxlk->rx_pool);
    if (bd) {
        bd->data = bd->head;
        bd->length = bd->true_len;
        bd->next = NULL;
        bd->interface = -1;
    }

    return bd;
}

static void mxlk_free_rx_bd(struct mxlk *mxlk, struct mxlk_buf_desc * bd)
{
    if (bd) {
        mxlk_list_put(&mxlk->rx_pool, bd);
    }
}

static struct mxlk_buf_desc *mxlk_alloc_tx_bd(struct mxlk *mxlk)
{
    struct mxlk_buf_desc *bd;

    bd = mxlk_list_get(&mxlk->tx_pool);
    if (bd) {
        bd->data = bd->head;
        bd->length = bd->true_len;
        bd->next = NULL;
        bd->interface = -1;
    }

    return bd;
}

static void mxlk_free_tx_bd(struct mxlk *mxlk, struct mxlk_buf_desc * bd)
{
    if (bd) {
        mxlk_list_put(&mxlk->tx_pool, bd);
    }
}

static int mxlk_all_chrdev_init(struct mxlk *mxlk)
{
    int error;
    int index;
    struct mxlk_interface *inf;

    for (index = 0; index < MXLK_NUM_INTERFACES; index++) {
        inf = mxlk->interfaces + index;
        /* Set mxlk pointer now because it used by mxlk_chrdev_add(). The rest
         * of the interface structure will be initialized later. */
        inf->mxlk = mxlk;
        error = mxlk_chrdev_add(inf);
        if (error) {
            return error;
        }
    }

    return 0;
}

static void mxlk_all_chrdev_cleanup(struct mxlk *mxlk)
{
    int index;
    struct mxlk_interface *inf;

    for (index = 0; index < MXLK_NUM_INTERFACES; index++) {
        inf = mxlk->interfaces + index;
        mxlk_chrdev_remove(inf);
    }
}

static void mxlk_interfaces_init(struct mxlk *mxlk)
{
    int index;

    mxlk_list_init(&mxlk->write);
    init_waitqueue_head(&mxlk->wr_waitq);
    for (index = 0; index < MXLK_NUM_INTERFACES; index++) {
        mxlk_interface_init(mxlk, index);
    }
}

static void mxlk_interfaces_cleanup(struct mxlk *mxlk)
{
    int index;

    mxlk_list_cleanup(&mxlk->write);
    for (index = 0; index < MXLK_NUM_INTERFACES; index++) {
        mxlk_interface_cleanup(mxlk->interfaces + index);
    }
}

static void mxlk_interface_init(struct mxlk *mxlk, int id)
{
    struct mxlk_interface *inf = mxlk->interfaces + id;

    inf->id = id;
    inf->opened = 0;

    inf->partial_read = NULL;
    mxlk_list_init(&inf->read);
    mutex_init(&inf->rlock);
    mutex_init(&inf->wlock);
    init_waitqueue_head(&inf->rd_waitq);
}

static void mxlk_interface_cleanup(struct mxlk_interface *inf)
{
    struct mxlk_buf_desc *bd;

    inf->opened = 0;
    msleep(10);

    mutex_destroy(&inf->rlock);
    mutex_destroy(&inf->wlock);

    mxlk_free_rx_bd(inf->mxlk, inf->partial_read);
    while ((bd = mxlk_list_get(&inf->read))) {
        mxlk_free_rx_bd(inf->mxlk, bd);
    }
}

static void mxlk_add_bd_to_interface(struct mxlk *mxlk, struct mxlk_buf_desc *bd)
{
    struct mxlk_interface *inf;

    inf = mxlk->interfaces + bd->interface;

    mxlk_list_put(&inf->read, bd);
    /* Wake up read wait queue in case someone is waiting for RX data */
    wake_up(&inf->rd_waitq);
}

static int mxlk_discover_txrx(struct mxlk *mxlk)
{
    int error;
    struct mxlk_cap_txrx *cap;

    cap = mxlk_cap_find(mxlk, 0, MXLK_CAP_TXRX);
    if (cap) {
        error = mxlk_txrx_init(mxlk, cap);
    } else {
        error = -EIO;
    }

    return error;
}

static void mxlk_set_td_address(struct mxlk_transfer_desc *td, u64 address)
{
    mx_wr64(td, offsetof(struct mxlk_transfer_desc, address), address);
}

static u64 mxlk_get_td_address(struct mxlk_transfer_desc *td)
{
    return mx_rd64(td, offsetof(struct mxlk_transfer_desc, address));
}

static void mxlk_set_td_length(struct mxlk_transfer_desc *td, u32 length)
{
    mx_wr32(td, offsetof(struct mxlk_transfer_desc, length), length);
}

static u32  mxlk_get_td_length(struct mxlk_transfer_desc *td)
{
    return mx_rd32(td, offsetof(struct mxlk_transfer_desc, length));
}

static void mxlk_set_td_interface(struct mxlk_transfer_desc *td, u16 interface)
{
    mx_wr16(td, offsetof(struct mxlk_transfer_desc, interface), interface);
}

static u16  mxlk_get_td_interface(struct mxlk_transfer_desc *td)
{
    return mx_rd16(td, offsetof(struct mxlk_transfer_desc, interface));
}

static void mxlk_set_td_status(struct mxlk_transfer_desc *td, u16 status)
{
    mx_wr16(td, offsetof(struct mxlk_transfer_desc, status), status);
}

static u16  mxlk_get_td_status(struct mxlk_transfer_desc *td)
{
    return mx_rd16(td, offsetof(struct mxlk_transfer_desc, status));
}

static void mxlk_set_tdr_head(struct mxlk_pipe *p, u32 head)
{
    mx_wr32(p->head, 0, head);
}

static u32  mxlk_get_tdr_head(struct mxlk_pipe *p)
{
    return mx_rd32(p->head, 0);
}

static void mxlk_set_tdr_tail(struct mxlk_pipe *p, u32 tail)
{
    mx_wr32(p->tail, 0, tail);
}

static u32  mxlk_get_tdr_tail(struct mxlk_pipe *p)
{
    return mx_rd32(p->tail, 0);
}

static int mxlk_txrx_init(struct mxlk *mxlk, struct mxlk_cap_txrx *cap)
{
    int index;
    int ndesc;
    struct mxlk_stream *tx = &mxlk->tx;
    struct mxlk_stream *rx = &mxlk->rx;

    mxlk->txrx = cap;
    mxlk->fragment_size = mx_rd32(&cap->fragment_size, 0);

    tx->busy = 0;
    tx->pipe.ndesc = mx_rd32(&cap->tx.ndesc, 0);
    tx->pipe.head  = &cap->tx.head;
    tx->pipe.tail  = &cap->tx.tail;
    tx->pipe.old   = mx_rd32(&cap->tx.tail, 0);
    tx->pipe.tdr   = mxlk->mmio + mx_rd32(&cap->tx.ring, 0);

    tx->ddr = kzalloc(sizeof(struct mxlk_dma_desc) * tx->pipe.ndesc, GFP_KERNEL);
    if (!tx->ddr) {
        mx_err("failed to alloc tx dma desc ring\n");
        goto error;
    }

    rx->busy = 0;
    rx->pipe.ndesc = mx_rd32(&cap->rx.ndesc, 0);
    rx->pipe.head  = &cap->rx.head;
    rx->pipe.tail  = &cap->rx.tail;
    rx->pipe.old   = mx_rd32(&cap->tx.head, 0);
    rx->pipe.tdr   = mxlk->mmio + mx_rd32(&cap->rx.ring, 0);

    rx->ddr = kzalloc(sizeof(struct mxlk_dma_desc) * rx->pipe.ndesc, GFP_KERNEL);
    if (!rx->ddr) {
        mx_err("failed to alloc rx dma desc ring\n");
        goto error;
    }

    mxlk_list_init(&mxlk->rx_pool);
    rx_pool_size = roundup(rx_pool_size,  mxlk->fragment_size);
    ndesc = rx_pool_size / mxlk->fragment_size;

    for (index = 0; index < ndesc; index++) {
        struct mxlk_buf_desc *bd = mxlk_alloc_bd(mxlk->fragment_size);
        if (bd) {
            mxlk_list_put(&mxlk->rx_pool, bd);
        } else {
            mx_err("failed to alloc all rx pool descriptors\n");
            goto error;
        }
    }

    mxlk_list_init(&mxlk->tx_pool);
    tx_pool_size = roundup(tx_pool_size,  mxlk->fragment_size);
    ndesc = tx_pool_size / mxlk->fragment_size;

    for (index = 0; index < ndesc; index++) {
        struct mxlk_buf_desc *bd = mxlk_alloc_bd(mxlk->fragment_size);
        if (bd) {
            mxlk_list_put(&mxlk->tx_pool, bd);
        } else {
            mx_err("failed to alloc all tx pool descriptors\n");
            goto error;
        }
    }

    for (index = 0; index < rx->pipe.ndesc; index++) {
        struct mxlk_buf_desc *bd = mxlk_alloc_rx_bd(mxlk);
        struct mxlk_dma_desc *dd = rx->ddr + index;
        struct mxlk_transfer_desc *td = rx->pipe.tdr + index;

        if (!bd) {
            mx_err("failed to alloc rx ring buf desc [%d]\n", index);
            goto error;
        }

        dd->bd = bd;
        if (mxlk_map_dma(mxlk, dd, DMA_FROM_DEVICE)) {
            mx_err("failed to map rx bd\n");
            goto error;
        }

        mxlk_set_td_address(td, dd->phys);
        mxlk_set_td_length(td, dd->length);
    }

    return 0;

error :
    mxlk_txrx_cleanup(mxlk);

    return -1;
}

static void mxlk_txrx_cleanup(struct mxlk *mxlk)
{
    int index;
    struct mxlk_stream *tx = &mxlk->tx;
    struct mxlk_stream *rx = &mxlk->rx;

    if (tx->ddr) {
        for (index = 0; index < tx->pipe.ndesc; index++) {
            struct mxlk_dma_desc *dd = tx->ddr + index;
            if (dd->bd) {
                mxlk_unmap_dma(mxlk, dd, DMA_TO_DEVICE);
                mxlk_free_tx_bd(mxlk, dd->bd);
            }
        }
        kfree(tx->ddr);
    }

    if (rx->ddr) {
        for (index = 0; index < rx->pipe.ndesc; index++) {
            struct mxlk_dma_desc *dd = rx->ddr + index;
            struct mxlk_transfer_desc *td = rx->pipe.tdr + index;
            if (dd->bd) {
                mxlk_unmap_dma(mxlk, dd, DMA_FROM_DEVICE);
                mxlk_free_rx_bd(mxlk, dd->bd);
                mxlk_set_td_address(td, 0);
                mxlk_set_td_length(td, 0);
            }
        }
        kfree(rx->ddr);
    }

    mxlk_list_cleanup(&mxlk->tx_pool);
    mxlk_list_cleanup(&mxlk->rx_pool);
}

static irqreturn_t mxlk_interrupt(int irq, void *args)
{
    struct mxlk *mxlk = args;
    enum mx_opmode opmode;

    opmode = mx_get_opmode(&mxlk->mx_dev);
    if (opmode == MX_OPMODE_APP_VPULINK) {
        mxlk->stats.interrupts++;
        mxlk_start_tx(mxlk);
        mxlk_start_rx(mxlk);
    } else if (opmode == MX_OPMODE_BOOT) {
        mx_wr32(mxlk->mmio, MX_INT_IDENTITY, 0);
    } else {
        mx_err("Unexpected MSI interrupt in operation mode %d\n", opmode);
    }

    return IRQ_HANDLED;
}

#if MXLK_IRQ_VECTORS != 1
#error "mxlk requested irqs mismatch!"
#endif

static int mxlk_events_init(struct mxlk *mxlk)
{
    int error;

    INIT_WORK(&mxlk->rx_event, mxlk_rx_event_handler);
    INIT_WORK(&mxlk->tx_event, mxlk_tx_event_handler);
    INIT_WORK(&mxlk->send_doorbell, mxlk_send_doorbell_handler);

    error = mx_pci_irq_init(&mxlk->mx_dev, MXLK_DRIVER_NAME, mxlk_interrupt, mxlk);
    if (error) {
        return error;
    }

    /* Allow some time for the device to complete initialization after MSI
     * enable handshake. */
    msleep(50);

    if (mx_get_opmode(&mxlk->mx_dev) == MX_OPMODE_BOOT) {
        mx_boot_status_update_int_enable(&mxlk->mx_dev);
    }

    return 0;
}

static void mxlk_events_cleanup(struct mxlk *mxlk)
{
    if (mx_get_opmode(&mxlk->mx_dev) == MX_OPMODE_BOOT) {
       mx_boot_status_update_int_disable(&mxlk->mx_dev);
    }
    mx_pci_irq_cleanup(&mxlk->mx_dev, mxlk);

    cancel_work_sync(&mxlk->send_doorbell);
    cancel_work_sync(&mxlk->rx_event);
    cancel_work_sync(&mxlk->tx_event);
}

static void mxlk_send_doorbell_handler(struct work_struct *work)
{
    struct mxlk *mxlk = container_of(work, struct mxlk, send_doorbell);

    mxlk_ring_doorbell(mxlk);
}

static int mxlk_map_dma(struct mxlk *mxlk, struct mxlk_dma_desc *dd,
                        int direction)
{
    struct device *dev = MXLK_TO_DEV(mxlk);

    dd->phys = dma_map_single(dev, dd->bd->data, dd->bd->length, direction);
    dd->length = dd->bd->length;

    return dma_mapping_error(dev, dd->phys);
}

static void mxlk_unmap_dma(struct mxlk *mxlk, struct mxlk_dma_desc *dd,
                           int direction)
{
    struct device *dev = MXLK_TO_DEV(mxlk);

    dma_unmap_single(dev, dd->phys, dd->length, direction);
}

static void mxlk_rx_event_handler(struct work_struct *work)
{
    struct mxlk *mxlk = container_of(work, struct mxlk, rx_event);

    int error, restart = 0;
    u16 status, interface;
    u32 head, tail, ndesc, length;
    struct mxlk_stream *rx = &mxlk->rx;
    struct mxlk_buf_desc *bd, *replacement;
    struct mxlk_dma_desc *dd;
    struct mxlk_transfer_desc *td;

    mxlk->stats.rx_event_runs++;

    ndesc =  rx->pipe.ndesc;
    tail  = mxlk_get_tdr_tail(&rx->pipe);
    head  = mxlk_get_tdr_head(&rx->pipe);

    /* Stop processing here in case MX device is down. */
    if (INVALID(head) || INVALID(tail)) {
        return;
    }

    /* clean old entries first */
    while (head != tail) {
        td = rx->pipe.tdr + head;
        dd = rx->ddr + head;

        replacement = mxlk_alloc_rx_bd(mxlk);
        if (!replacement) {
            restart = 1;
            break;
        }

        status = mxlk_get_td_status(td);
        interface = mxlk_get_td_interface(td);
        length = mxlk_get_td_length(td);
        mxlk_unmap_dma(mxlk, dd, DMA_FROM_DEVICE);

        if (unlikely(status != MXLK_DESC_STATUS_SUCCESS)) {
            mxlk_free_rx_bd(mxlk, dd->bd);
        } else {
            bd = dd->bd;
            bd->interface = interface;
            bd->length = length;
            bd->next = NULL;

            if (likely(interface < MXLK_NUM_INTERFACES)) {
                mxlk->stats.rx_krn.pkts++;
                mxlk->stats.rx_krn.bytes += bd->length;
                mxlk_add_bd_to_interface(mxlk, bd);
            } else {
                mxlk_free_rx_bd(mxlk, bd);
            }
        }

        dd->bd = replacement;
        error = mxlk_map_dma(mxlk, dd, DMA_FROM_DEVICE);
        if (error) {
            mx_err("failed to map rx bd (%d)\n", error);
        }

        mxlk_set_td_address(td, dd->phys);
        mxlk_set_td_length(td, dd->length);
        head = MXLK_CIRCULAR_INC(head, ndesc);
    }

    if (mxlk_get_tdr_head(&rx->pipe) != head) {
        mxlk_set_tdr_head(&rx->pipe, head);
        mmiowb();
        mxlk_send_doorbell(mxlk);
    }

    if (unlikely(restart)) {
        msleep(5);
        mxlk_start_rx(mxlk);
    }
}

static void mxlk_tx_event_handler(struct work_struct *work)
{
    struct mxlk *mxlk = container_of(work, struct mxlk, tx_event);

    u16 status;
    u32 head, tail, old, ndesc;
    struct mxlk_stream *tx = &mxlk->tx;
    struct mxlk_buf_desc *bd;
    struct mxlk_dma_desc *dd;
    struct mxlk_transfer_desc *td;
    bool buffer_freed = false;

    mxlk->stats.tx_event_runs++;

    ndesc = tx->pipe.ndesc;
    old   = tx->pipe.old;
    tail  = mxlk_get_tdr_tail(&tx->pipe);
    head  = mxlk_get_tdr_head(&tx->pipe);

    /* Stop processing here in case MX device is down. */
    if (INVALID(head) || INVALID(tail)) {
        return;
    }

    /* clean old entries first */
    while (old != head) {
        dd = tx->ddr + old;
        td = tx->pipe.tdr + old;
        bd = dd->bd;

        status = mxlk_get_td_status(td);
        if (status != MXLK_DESC_STATUS_SUCCESS) {
            mx_err("detected tx desc failure (%u)\n", status);
        }
        mxlk->stats.tx_krn.pkts++;
        mxlk->stats.tx_krn.bytes += bd->length;

        mxlk_unmap_dma(mxlk, dd, DMA_TO_DEVICE);
        mxlk_free_tx_bd(mxlk, dd->bd);
        dd->bd = NULL;
        old = MXLK_CIRCULAR_INC(old, ndesc);
        buffer_freed = true;
    }
    tx->pipe.old = old;

    /* add new entries */
    while (MXLK_CIRCULAR_INC(tail, ndesc) != head) {
        bd = mxlk_list_get(&mxlk->write);
        if (!bd) {
            break;
        }

        dd = tx->ddr + tail;
        td = tx->pipe.tdr + tail;

        dd->bd = bd;
        if (mxlk_map_dma(mxlk, dd, DMA_TO_DEVICE)) {
            mx_err("dma mapping error bd addr %px, size %zu\n",
                   bd->data, bd->length);
            break;
        }

        mxlk_set_td_address(td, dd->phys);
        mxlk_set_td_length(td, dd->length);
        mxlk_set_td_interface(td, bd->interface);
        mxlk_set_td_status(td, MXLK_DESC_STATUS_ERROR);

        tail = MXLK_CIRCULAR_INC(tail, ndesc);
    }

    if (mxlk_get_tdr_tail(&tx->pipe) != tail) {
        mxlk_set_tdr_tail(&tx->pipe, tail);
        wmb();
        mmiowb();
        mxlk_send_doorbell(mxlk);
    }

    if (buffer_freed == true) {
        /* Wake up write wait queue in case someone is waiting for TX buffers */
        wake_up(&mxlk->wr_waitq);
    }
}

static void mxlk_start_tx(struct mxlk *mxlk)
{
    queue_work(mxlk->wq, &mxlk->tx_event);
}

static void mxlk_start_rx(struct mxlk *mxlk)
{
    queue_work(mxlk->wq, &mxlk->rx_event);
}

static void mxlk_send_doorbell(struct mxlk *mxlk)
{
    queue_work(mxlk->wq, &mxlk->send_doorbell);
}

static void mxlk_ring_doorbell(struct mxlk *mxlk)
{
    u32 value  = MXLK_DOORBELL_DATA;
    int offset = MXLK_DOORBELL_ADDR;

    mxlk->stats.doorbells++;
    pci_write_config_dword(mxlk->pci, offset, value);
}

int mxlk_core_init(struct mxlk *mxlk, struct pci_dev *pdev,
                   struct workqueue_struct *wq)
{
    int error;

    mxlk->wq = wq;
    mxlk->pci = pdev;

    mxlk->unit = atomic_fetch_inc(&units_found);
    if (mxlk->unit < MXLK_MAX_DEVICES) {
        scnprintf(mxlk->name, MXLK_MAX_NAME_LEN, MXLK_DRIVER_NAME"%d",
                  mxlk->unit);
    } else {
        return -EPERM;
    }

    error = mx_pci_init(&mxlk->mx_dev, pdev, mxlk, MXLK_DRIVER_NAME, &mxlk->mmio);
    if (error) {
        return error;
    }

    error = mxlk_events_init(mxlk);
    if (error) {
        goto error_events;
    }

    mx_boot_init(&mxlk->mx_dev);

    /* Initialize character devices immediately: We need them to be alive at all
     * time in order to be able to handle reset and boot operations. */
    error = mxlk_all_chrdev_init(mxlk);
    if (error) {
        goto error_chrdev;
    }

    /* If the MX device is in proper application mode, start communications.
     * If not, then we expect the user to reset the MX device and load a proper
     * MX application before we can start operations. In this case,
     * mxlk_comms_init() will be called at the end of the boot process. */
    if (mx_get_opmode(&mxlk->mx_dev) == MX_OPMODE_APP_VPULINK) {
        error = mxlk_comms_init(mxlk);
        if (error) {
            goto error_comms;
        }
    }

    /* Save PCIe context now so that we have a base for restoring the link if
     * the device ever resets itself. */
    mx_pci_dev_ctx_save(&mxlk->mx_dev);

    return 0;

error_comms:
    mxlk_all_chrdev_cleanup(mxlk);

error_chrdev:
    mxlk_events_cleanup(mxlk);

error_events:
    mx_pci_cleanup(&mxlk->mx_dev);
    mx_err("core failed to init\n");

    return error;
}

void mxlk_core_cleanup(struct mxlk *mxlk)
{
    if (mx_get_opmode(&mxlk->mx_dev) == MX_OPMODE_APP_VPULINK) {
        mxlk_comms_cleanup(mxlk);
    }
    mxlk_all_chrdev_cleanup(mxlk);
    mx_boot_cleanup(&mxlk->mx_dev);
    mxlk_events_cleanup(mxlk);
    mx_pci_cleanup(&mxlk->mx_dev);
}

static int mxlk_comms_init(struct mxlk *mxlk)
{
    int error;
    int status;
    DEVICE_ATTR(debug, S_IWUSR | S_IRUGO, mxlk_debug_show, mxlk_debug_store);

    status = mxlk_get_device_status(mxlk);
    if (status != MXLK_STATUS_RUN) {
        mx_err("device status not RUNNING (%d)\n", status);
        error = -EIO;
        goto error_device_status;
    }

    error = mxlk_version_check(mxlk);
    if (error) {
        goto error_version;
    }

    error = mxlk_discover_txrx(mxlk);
    if (error) {
        goto error_stream;
    }

    mxlk_interfaces_init(mxlk);

    mxlk_set_host_status(mxlk, MXLK_STATUS_RUN);

    mxlk->debug = dev_attr_debug;
    device_create_file(&mxlk->pci->dev, &mxlk->debug);

    memset(&mxlk->stats, 0, sizeof(struct mxlk_stats));
    memset(&mxlk->stats_old, 0, sizeof(struct mxlk_stats));

    mxlk_ring_doorbell(mxlk);

    return 0;

error_stream :
error_version :
error_device_status :
    mx_err("comms failed to init\n");

    return error;
}

static void mxlk_comms_cleanup(struct mxlk *mxlk)
{
    mxlk_set_host_status(mxlk, MXLK_STATUS_UNINIT);
    mdelay(10);

    device_remove_file(MXLK_TO_DEV(mxlk), &mxlk->debug);
    mxlk_interfaces_cleanup(mxlk);
    mxlk_txrx_cleanup(mxlk);
    mxlk_list_cleanup(&mxlk->write);
}

int mxlk_core_open(struct mxlk_interface *inf)
{
    if (inf->opened) {
        return -EACCES;
    }

    inf->opened = 1;

    return 0;
}

int mxlk_core_close(struct mxlk_interface *inf)
{
    if (inf->opened) {
        inf->opened = 0;
    }

    return 0;
}

ssize_t mxlk_core_read(struct mxlk_interface *inf, void *buffer, size_t length)
{
    struct mxlk *mxlk = inf->mxlk;
    size_t remaining = length;
    struct mxlk_buf_desc *bd;

    mutex_lock(&inf->rlock);
    {
        bd = (inf->partial_read) ? inf->partial_read : mxlk_list_get(&inf->read);
        while (remaining && bd) {
            int error;
            size_t bcopy;

            bcopy = min(remaining, bd->length);
            error = copy_to_user(buffer, bd->data, bcopy);
            if (error) {
                mx_err("failed to copy to user %d/%zu\n", error, bcopy);
                break;
            }

            buffer += bcopy;
            remaining -= bcopy;
            bd->data += bcopy;
            bd->length -= bcopy;

            mxlk->stats.rx_usr.bytes += bcopy;
            if (bd->length == 0) {
                mxlk->stats.rx_usr.pkts++;
                mxlk_free_rx_bd(mxlk, bd);
                bd = mxlk_list_get(&inf->read);
            }
        }

        /* save for next time */
        inf->partial_read = bd;
    }
    mutex_unlock(&inf->rlock);

    return (length - remaining);
}

ssize_t mxlk_core_write(struct mxlk_interface *inf, void *buffer, size_t length)
{
    size_t remaining = length;
    struct mxlk *mxlk = inf->mxlk;
    struct mxlk_buf_desc *bd, *head;

    mutex_lock(&inf->wlock);
    {
        bd = head = mxlk_alloc_tx_bd(mxlk);
        while (remaining && bd) {
            int error;
            size_t bcopy;

            bcopy = min(bd->length, remaining);
            error = copy_from_user(bd->data, buffer, bcopy);
            if (error) {
                mx_err("failed to copy from user %d/%zu\n", error, bcopy);
                break;
            }

            buffer += bcopy;
            remaining -= bcopy;
            bd->length = bcopy;
            bd->interface = inf->id;

            mxlk->stats.tx_usr.pkts++;
            mxlk->stats.tx_usr.bytes += bcopy;

            if (remaining) {
                bd->next = mxlk_alloc_tx_bd(mxlk);
                bd = bd->next;
            }
        }

        if (head) {
            mxlk_list_put(&inf->mxlk->write, head);
            mxlk_start_tx(mxlk);
        }
    }
    mutex_unlock(&inf->wlock);

    return (length - remaining);
}

bool mxlk_core_read_data_available(struct mxlk_interface *inf)
{
    size_t bytes, buffers;

    mxlk_list_info(&inf->read, &bytes, &buffers);
    return (inf->partial_read || (buffers != 0));
}

bool mxlk_core_write_buffer_available(struct mxlk *mxlk)
{
    size_t bytes, buffers;

    mxlk_list_info(&mxlk->tx_pool, &bytes, &buffers);
    return (buffers != 0);
}

int mxlk_core_reset_dev(struct mxlk *mxlk)
{
    int error;
    enum mx_opmode opmode;
    bool need_reset = true;

    opmode = mx_get_opmode(&mxlk->mx_dev);
    if (opmode == MX_OPMODE_BOOT) {
        /* MX device has already been reset - nothing to do */
        return 0;
    } else if (opmode == MX_OPMODE_UNKNOWN) {
        /* Looks like device may have reset itself already.
         * Perform post reset operations to see if we can resume proper
         * communications. */
        error = mx_reset_restore_and_check_device(&mxlk->mx_dev);
        if (error) {
            mx_err("Context restoring failed with error: %d\n", error);
            return error;
        }
        need_reset = false;
    } else if (!MX_IS_OPMODE_APP(opmode)) {
        /* PCIe reset driver is not accessible... */
        return -EPERM;
    }

    mxlk_comms_cleanup(mxlk);
    mxlk_events_cleanup(mxlk);

    if (need_reset) {
        mx_pci_dev_lock(&mxlk->mx_dev);
        error = mx_reset_device(&mxlk->mx_dev);
        mx_pci_dev_unlock(&mxlk->mx_dev);
        if (error) {
            return error;
        }
    }

    error = mxlk_events_init(mxlk);
    if (error) {
        return error;
    }

    /* Kernel discards saved context after restoration so save it again now in
     * order to be ready in case a device-initiated reset happens. */
    mx_pci_dev_ctx_save(&mxlk->mx_dev);

    return 0;
}

int mxlk_core_boot_dev(struct mxlk *mxlk, const char *buffer, size_t length)
{
    int error;
    enum mx_opmode opmode;

    /* Make sure we are in the correct operational mode and the device is ready. */
    opmode = mx_get_opmode(&mxlk->mx_dev);
    if (opmode != MX_OPMODE_BOOT) {
        return -EPERM;
    }

    mxlk_events_cleanup(mxlk);

    error = mx_boot_load_image(&mxlk->mx_dev, buffer, length, true);
    if (error < 0) {
        return error;
    }

    error = mxlk_events_init(mxlk);
    if (error) {
        return error;
    }

    opmode = mx_get_opmode(&mxlk->mx_dev);
    if (opmode == MX_OPMODE_APP_VPULINK) {
        error = mxlk_comms_init(mxlk);
        if (error) {
            return error;
        }
    } else {
        mx_err("unexpected operation mode (%d) after MX FW loading!\n", opmode);
    }

    return 0;
}

static char* convertOpModeToStr(const enum mx_opmode opmode) {
    switch (opmode) {
        case MX_OPMODE_UNKNOWN:     return "MX_OPMODE_UNKNOWN";
        case MX_OPMODE_BOOT:        return "MX_OPMODE_BOOT";
        case MX_OPMODE_LOADER:      return "MX_OPMODE_LOADER";
        case MX_OPMODE_APP_VPUAL:   return "MX_OPMODE_APP_VPUAL";
        case MX_OPMODE_APP_VPULINK: return "MX_OPMODE_APP_VPULINK";
        default: return "";
    }
}

void mxlk_get_dev_status(struct mxlk *mxlk, enum mxlk_fw_status *fw_status)
{
    enum mx_opmode opmode;

    opmode = mx_get_opmode(&mxlk->mx_dev);
    mx_info("%s Device opmode: %s\n", mxlk->name, convertOpModeToStr(opmode));
    if (opmode == MX_OPMODE_BOOT) {
        /* Device in Boot mode */
        *fw_status = MXLK_FW_STATE_BOOTLOADER;
    } else if (opmode == MX_OPMODE_UNKNOWN) {
        /* Device is lost or require configuration restoration */
        *fw_status = MXLK_FW_STATUS_UNKNOWN_STATE;
    } else {
        /* In case of of all other modes, assumed myriad is running some mvcmd */
        *fw_status = MXLK_FW_STATUS_USER_APP;
    }
}
