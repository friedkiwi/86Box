/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Super I/O chip detection code.
 *
 * Version:	@(#)sio_detect.c	1.0.1	2018/11/05
 *
 * Authors:	Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2016-2018 Miran Grca.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "86box.h"
#include "device.h"
#include "io.h"
#include "timer.h"
#include "floppy/fdd.h"
#include "floppy/fdc.h"
#include "sio.h"


typedef struct {
    uint8_t regs[2];
} sio_detect_t;


static void
sio_detect_write(uint16_t port, uint8_t val, void *priv)
{
    sio_detect_t *dev = (sio_detect_t *) priv;

    pclog("sio_detect_write : port=%04x = %02X\n", port, val);

    dev->regs[port & 1] = val;

    return;
}


static uint8_t
sio_detect_read(uint16_t port, void *priv)
{
    sio_detect_t *dev = (sio_detect_t *) priv;

    pclog("sio_detect_read : port=%04x = %02X\n", port, dev->regs[port & 1]);

    return dev->regs[port & 1];
}


static void
sio_detect_close(void *priv)
{
    sio_detect_t *dev = (sio_detect_t *) priv;

    free(dev);
}


static void *
sio_detect_init(const device_t *info)
{
    sio_detect_t *dev = (sio_detect_t *) malloc(sizeof(sio_detect_t));
    memset(dev, 0, sizeof(sio_detect_t));

    device_add(&fdc_at_smc_device);

    io_sethandler(0x0024, 0x0004,
		  sio_detect_read, NULL, NULL, sio_detect_write, NULL, NULL, dev);
    io_sethandler(0x002e, 0x0002,
		  sio_detect_read, NULL, NULL, sio_detect_write, NULL, NULL, dev);
    io_sethandler(0x0044, 0x0004,
		  sio_detect_read, NULL, NULL, sio_detect_write, NULL, NULL, dev);
    io_sethandler(0x004e, 0x0002,
		  sio_detect_read, NULL, NULL, sio_detect_write, NULL, NULL, dev);
    io_sethandler(0x0108, 0x0002,
		  sio_detect_read, NULL, NULL, sio_detect_write, NULL, NULL, dev);
    io_sethandler(0x0250, 0x0002,
		  sio_detect_read, NULL, NULL, sio_detect_write, NULL, NULL, dev);
    io_sethandler(0x0370, 0x0002,
		  sio_detect_read, NULL, NULL, sio_detect_write, NULL, NULL, dev);
    io_sethandler(0x03f0, 0x0002,
		  sio_detect_read, NULL, NULL, sio_detect_write, NULL, NULL, dev);

    return dev;
}


const device_t sio_detect_device = {
    "Super I/O Detection Helper",
    0,
    0,
    sio_detect_init, sio_detect_close, NULL,
    NULL, NULL, NULL,
    NULL
};
