/*******************************************************************************
 *
 * Intel Myriad-X PCIe Serial Driver: Demo application
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>

#define REPORTING_INTERVAL (1)
#define BUFLEN (32 * 1024)

uint32_t readBuffer[BUFLEN / sizeof(uint32_t)];
uint32_t writeBuffer[BUFLEN / sizeof(uint32_t)];

static volatile bool running;
static pthread_t readThread;
static pthread_t writeThread;
static pthread_t calcThread;
static size_t tx_bytes = 0;
static size_t rx_bytes = 0;
static size_t rx_error = 0;

static ssize_t readStream(int dev, void *buffer, size_t length)
{
    void *ptr = buffer;
    size_t remaining = length;

    while (remaining) {
        ssize_t copied = read(dev, ptr, remaining);
        if (copied < 0) {
            printf("read failure (%zd)\n", copied);
            break;
        }
        remaining -= copied;
        ptr += copied;
    }

    return (ssize_t) (length - remaining);
}

static ssize_t writeStream(int dev, void *buffer, size_t length)
{
    void *ptr = buffer;
    size_t remaining = length;

    while (remaining) {
        ssize_t copied = write(dev, ptr, remaining);
        if (copied < 0) {
            printf("write failure (%zd)\n", copied);
            break;
        }
        remaining -= copied;
        ptr += copied;
    }

    return (ssize_t) (length - remaining);
}

static void *reader(void *args)
{
    uint32_t expect = 0;
    uint32_t actual = 0;
    int dev = *(int *) args;

    printf("starting reader thread...\n");
    while (running) {
        ssize_t read = readStream(dev, readBuffer, sizeof(readBuffer));
        if (read < 0) {
            break;
        }

        actual = readBuffer[0];
        if (actual != expect) {
            rx_error++;
            printf("rx seqno mismatch a %u e %u\n", actual, expect);
            expect = actual;
        }
        expect++;
        rx_bytes += read;
    }

    return NULL;
}

static void *writer(void *args)
{
    uint32_t seqno = 0;
    int dev = *(int *) args;

    printf("starting writer thread...\n");
    while (running) {
        writeBuffer[0] = seqno++;
        ssize_t written = writeStream(dev, writeBuffer, sizeof(writeBuffer));
        if (written < 0) {
            printf("write error (%zd), exiting...\n", written);
            break;
        }
        tx_bytes += written;
    }

    return NULL;
}

static void *calculator(void *args)
{
    size_t count = 0;

    while (running) {
        uint64_t rx_thpt, tx_thpt;

        sleep(REPORTING_INTERVAL);
        rx_thpt  = rx_bytes / REPORTING_INTERVAL;
        tx_thpt  = tx_bytes / REPORTING_INTERVAL;
        rx_bytes = tx_bytes = 0;
        printf("[%zu] rx : %zu tx : %zu er : %zu\n", count, rx_thpt, tx_thpt, rx_error);

        count += REPORTING_INTERVAL;
    }

    return NULL;
}

int main(int argc, char ** argv)
{
    int device = open ("/dev/mxlk0:0", O_RDWR);
    if (device < 0) {
        printf("failed to open device...\n");
        return -1;
    }

    running = 1;
    pthread_create(&readThread, NULL, reader, &device);
    pthread_create(&writeThread, NULL, writer, &device);
    pthread_create(&calcThread, NULL, calculator, NULL);

    while(1);

    if(running)
    {
        running = false;
        pthread_cancel(readThread);
        pthread_cancel(writeThread);
        pthread_cancel(calcThread);
        pthread_join(readThread, NULL);
        pthread_join(writeThread, NULL);
        pthread_join(calcThread, NULL);
    }

    close(device);

    printf("exited...\n");

    return 0;
}
