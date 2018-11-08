/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Emulation of the NatSemi PC87306 Super I/O chip.
 *
 * Version:	@(#)sio_pc87306.c	1.0.14	2018/11/05
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *		Copyright 2016-2018 Miran Grca.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "86box.h"
#include "io.h"
#include "device.h"
#include "lpt.h"
#include "mem.h"
#include "pci.h"
#include "rom.h"
#include "serial.h"
#include "disk/hdc.h"
#include "disk/hdc_ide.h"
#include "floppy/fdd.h"
#include "floppy/fdc.h"
#include "sio.h"


typedef struct {
    uint8_t tries,
	    regs[29], gpio[2];
    int cur_reg;
    fdc_t *fdc;
    serial_t *uart[2];
} pc87306_t;


static void
pc87306_gpio_write(uint16_t port, uint8_t val, void *priv)
{
    pc87306_t *dev = (pc87306_t *) priv;

    dev->gpio[port & 1] = val;
}


uint8_t
pc87306_gpio_read(uint16_t port, void *priv)
{
    pc87306_t *dev = (pc87306_t *) priv;

    return dev->gpio[port & 1];
}


static void
pc87306_gpio_remove(pc87306_t *dev)
{
    io_removehandler(dev->regs[0x0f] << 2, 0x0001,
		     pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL, dev);
    io_removehandler((dev->regs[0x0f] << 2) + 1, 0x0001,
		     pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL, dev);
}


static void
pc87306_gpio_init(pc87306_t *dev)
{
    if ((dev->regs[0x12]) & 0x10)
        io_sethandler(dev->regs[0x0f] << 2, 0x0001,
		      pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL, dev);

    if ((dev->regs[0x12]) & 0x20)
        io_sethandler((dev->regs[0x0f] << 2) + 1, 0x0001,
		      pc87306_gpio_read, NULL, NULL, pc87306_gpio_write, NULL, NULL, dev);
}


static void
lpt1_handler(pc87306_t *dev)
{
    int temp;
    uint16_t lptba, lpt_port = 0x378;

    temp = dev->regs[0x01] & 3;
    lptba = ((uint16_t) dev->regs[0x19]) << 2;

    if (dev->regs[0x1b] & 0x10) {
	if (dev->regs[0x1b] & 0x20)
		lpt_port = 0x278;
	else
		lpt_port = 0x378;
    } else {
	switch (temp) {
		case 0:
			lpt_port = 0x378;
			break;
		case 1:
			lpt_port = lptba;
			break;
		case 2:
			lpt_port = 0x278;
			break;
		case 3:
			lpt_port = 0x000;
			break;
	}
    }

    if (lpt_port)
	lpt1_init(lpt_port);
}


static void
serial_handler(pc87306_t *dev, int uart)
{
    int temp;
    uint8_t fer_irq, pnp1_irq;
    uint8_t fer_shift, pnp_shift;
    uint8_t irq;

    temp = (dev->regs[1] >> (2 << uart)) & 3;

    fer_shift = 2 << uart;		/* 2 for UART 1, 4 for UART 2 */
    pnp_shift = 2 + (uart << 2);	/* 2 for UART 1, 6 for UART 2 */

    /* 0 = COM1 (IRQ 4), 1 = COM2 (IRQ 3), 2 = COM3 (IRQ 4), 3 = COM4 (IRQ 3) */
    fer_irq = ((dev->regs[1] >> fer_shift) & 1) ? 3 : 4;
    pnp1_irq = ((dev->regs[0x1c] >> pnp_shift) & 1) ? 4 : 3;

    irq = (dev->regs[0x1c] & 1) ? pnp1_irq : fer_irq;

    switch (temp) {
	case 0:
		serial_setup(dev->uart[uart], SERIAL1_ADDR, irq);
		break;
	case 1:
		serial_setup(dev->uart[uart], SERIAL2_ADDR, irq);
		break;
	case 2:
		switch ((dev->regs[1] >> 6) & 3) {
			case 0:
				serial_setup(dev->uart[uart], 0x3e8, irq);
				break;
			case 1:
				serial_setup(dev->uart[uart], 0x338, irq);
				break;
			case 2:
				serial_setup(dev->uart[uart], 0x2e8, irq);
				break;
			case 3:
				serial_setup(dev->uart[uart], 0x220, irq);
				break;
		}
		break;
	case 3:
		switch ((dev->regs[1] >> 6) & 3) {
			case 0:
				serial_setup(dev->uart[uart], 0x2e8, irq);
				break;
			case 1:
				serial_setup(dev->uart[uart], 0x238, irq);
				break;
			case 2:
				serial_setup(dev->uart[uart], 0x2e0, irq);
				break;
			case 3:
				serial_setup(dev->uart[uart], 0x228, irq);
				break;
		}
		break;
    }
}


static void
pc87306_write(uint16_t port, uint8_t val, void *priv)
{
    pc87306_t *dev = (pc87306_t *) priv;
    uint8_t index, valxor;

    index = (port & 1) ? 0 : 1;

    if (index) {
	dev->cur_reg = val & 0x1f;
	dev->tries = 0;
	return;
    } else {
	if (dev->tries) {
		if ((dev->cur_reg == 0) && (val == 8))
			val = 0x4b;
		valxor = val ^ dev->regs[dev->cur_reg];
		dev->tries = 0;
		if ((dev->cur_reg == 0x19) && !(dev->regs[0x1B] & 0x40))
			return;
		if ((dev->cur_reg <= 28) && (dev->cur_reg != 8)) {
			if (dev->cur_reg == 0)
				val &= 0x5f;
			if (((dev->cur_reg == 0x0F) || (dev->cur_reg == 0x12)) && valxor)
				pc87306_gpio_remove(dev);
			dev->regs[dev->cur_reg] = val;
		} else
			return;
	} else {
		dev->tries++;
		return;
	}
    }

    switch(dev->cur_reg) {
	case 0:
		if (valxor & 1) {
			lpt1_remove();
			if (val & 1)
				lpt1_handler(dev);
		}
		if (valxor & 2) {
			serial_remove(dev->uart[0]);
			if (val & 2)
				serial_handler(dev, 0);
		}
		if (valxor & 4) {
			serial_remove(dev->uart[1]);
			if (val & 4)
				serial_handler(dev, 1);
		}
		if (valxor & 0x28) {
			fdc_remove(dev->fdc);
			if (val & 8)
				fdc_set_base(dev->fdc, (val & 0x20) ? 0x370 : 0x3f0);
		}
		break;
	case 1:
		if (valxor & 3) {
			lpt1_remove();
			if (dev->regs[0] & 1)
				lpt1_handler(dev);
		}
		if (valxor & 0xcc) {
			if (dev->regs[0] & 2)
				serial_handler(dev, 0);
			else
				serial_remove(dev->uart[0]);
		}
		if (valxor & 0xf0) {
			if (dev->regs[0] & 4)
				serial_handler(dev, 1);
			else
				serial_remove(dev->uart[1]);
		}
		break;
	case 2:
		if (valxor & 1) {
			lpt1_remove();
			serial_remove(dev->uart[0]);
			serial_remove(dev->uart[1]);
			fdc_remove(dev->fdc);

			if (!(val & 1)) {
				if (dev->regs[0] & 1)
					lpt1_handler(dev);
				if (dev->regs[0] & 2)
					serial_handler(dev, 0);
				if (dev->regs[0] & 4)
					serial_handler(dev, 1);
				if (dev->regs[0] & 8)
					fdc_set_base(dev->fdc, (dev->regs[0] & 0x20) ? 0x370 : 0x3f0);
			}
		}
		break;
	case 9:
		if (valxor & 0x44) {
			fdc_update_enh_mode(dev->fdc, (val & 4) ? 1 : 0);
			fdc_update_densel_polarity(dev->fdc, (val & 0x40) ? 1 : 0);
		}
		break;
	case 0xF:
		if (valxor)
			pc87306_gpio_init(dev);
		break;
	case 0x12:
		if (valxor & 0x30)
			pc87306_gpio_init(dev);
		break;
	case 0x19:
		if (valxor) {
			lpt1_remove();
			if (dev->regs[0] & 1)
				lpt1_handler(dev);
		}
		break;
	case 0x1B:
		if (valxor & 0x70) {
			lpt1_remove();
			if (!(val & 0x40))
				dev->regs[0x19] = 0xEF;
			if (dev->regs[0] & 1)
				lpt1_handler(dev);
		}
		break;
	case 0x1C:
		if (valxor) {
			if (dev->regs[0] & 2)
				serial_handler(dev, 0);
			if (dev->regs[0] & 4)
				serial_handler(dev, 1);
		}
		break;
    }
}


uint8_t
pc87306_read(uint16_t port, void *priv)
{
    pc87306_t *dev = (pc87306_t *) priv;
    uint8_t ret = 0xff, index;

    index = (port & 1) ? 0 : 1;

    dev->tries = 0;

    if (index)
	ret = dev->cur_reg & 0x1f;
    else {
	if (dev->cur_reg == 8)
		ret = 0x70;
	else if (dev->cur_reg < 28)
		ret = dev->regs[dev->cur_reg];
    }

    return ret;
}


void
pc87306_reset(pc87306_t *dev)
{
    memset(dev->regs, 0, 29);

    dev->regs[0x00] = 0x0B;
    dev->regs[0x01] = 0x01;
    dev->regs[0x03] = 0x01;
    dev->regs[0x05] = 0x0D;
    dev->regs[0x08] = 0x70;
    dev->regs[0x09] = 0xC0;
    dev->regs[0x0b] = 0x80;
    dev->regs[0x0f] = 0x1E;
    dev->regs[0x12] = 0x30;
    dev->regs[0x19] = 0xEF;

    dev->gpio[0] = 0xff;
    dev->gpio[1] = 0xfb;

    /*
	0 = 360 rpm @ 500 kbps for 3.5"
	1 = Default, 300 rpm @ 500,300,250,1000 kbps for 3.5"
    */
    lpt1_remove();
    lpt2_remove();
    lpt1_handler(dev);
    serial_remove(dev->uart[0]);
    serial_remove(dev->uart[1]);
    serial_handler(dev, 0);
    serial_handler(dev, 0);
    fdc_reset(dev->fdc);
    pc87306_gpio_init(dev);
}


static void
pc87306_close(void *priv)
{
    pc87306_t *dev = (pc87306_t *) priv;

    free(dev);
}


static void *
pc87306_init(const device_t *info)
{
    pc87306_t *dev = (pc87306_t *) malloc(sizeof(pc87306_t));
    memset(dev, 0, sizeof(pc87306_t));

    dev->fdc = device_add(&fdc_at_nsc_device);

    dev->uart[0] = device_add_inst(&ns16550_device, 1);
    dev->uart[1] = device_add_inst(&ns16550_device, 2);

    pc87306_reset(dev);

    io_sethandler(0x02e, 0x0002,
		  pc87306_read, NULL, NULL, pc87306_write, NULL, NULL, dev);

    return dev;
}


const device_t pc87306_device = {
    "National Semiconductor PC87306 Super I/O",
    0,
    0,
    pc87306_init, pc87306_close, NULL,
    NULL, NULL, NULL,
    NULL
};
