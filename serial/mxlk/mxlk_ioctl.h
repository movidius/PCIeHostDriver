/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: IOCTL commands and parameters
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#ifndef SERIAL_MXLK_MXLK_IOCTL_H_
#define SERIAL_MXLK_MXLK_IOCTL_H_

/* The MXLK driver creates one character for each interface of each MX device
 * probed by the kernel. These characters device provide the following IOCTL
 * commands:
 *    - MXLK_RESET_DEV: Reset the MX device. To allow this operation, the MX
 *      device must be in application operation mode. At the end of the process,
 *      the device will be in boot mode.
 *      Alternatively, this command can also detect if the device has reset
 *      itself (e.g. watchdog reset) and, in this case, perform recovery
 *      operations to bring it back into boot mode.
 *      In both cases, an MX application must then be re-loaded in order to
 *      start communications over the PCIe serial link again.
 *    - MXLK_BOOT_DEV: Load an MX application image and boot the MX device. To
 *      allow this operation, the MX device must be in boot operation mode,
 *      either on first boot or after having been reset.
 *    - MXLK_STATUS_DEV: Get the status (MX application image loaded or not) of
 *      the MX device.
 *
 * NOTE: These commands can be triggered using the character device of any
 * interface but they have effect on the whole device. Typically, when using the
 * reset command on a given interface, all the other interfaces of the device
 * will be unusable until an MX application is reloaded. */

/* IOCTL commands IDs. */
#define IOC_MAGIC 'Z'
#define MXLK_RESET_DEV      _IO(IOC_MAGIC, 0x80)
#define MXLK_BOOT_DEV       _IOW(IOC_MAGIC, 0x81, struct mxlk_boot_param)
#define MXLK_STATUS_DEV     _IOR(IOC_MAGIC, 0x82, enum mxlk_fw_status)

struct mxlk_boot_param {
    /* Buffer containing the MX application image (MVCMD format). */
    const char *buffer;
    /* Size of the image in bytes. */
    size_t length;
};

/* State of Myriad X device. */
enum mxlk_fw_status {
    /* MX waiting for FW to be loaded from host */
    MXLK_FW_STATE_BOOTLOADER,
    /* MX running FW loaded from host. */
    MXLK_FW_STATUS_USER_APP,
    /* MX context is not restored or device is lost*/
    MXLK_FW_STATUS_UNKNOWN_STATE,
};

#endif /* SERIAL_MXLK_MXLK_IOCTL_H_ */
