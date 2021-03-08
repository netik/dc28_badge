/*-
 * Copyright (c) 2021
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "badge.h"

#include <string.h>
#include <stdio.h>

#if defined(BOARD_OTG2_USES_ULPI)

/*
 * The STM32F746 manual does not document the existence of the PHYCR
 * register in the USB controller. It exists only for the second controller
 * instance, which has HS capability. The following definitions were
 * found buried in the ST Micro Discovery board SDK.
 */

#define USBULPI_PHYCR		((uint32_t)(0x034))
#define USBULPI_D07		((uint32_t)0x000000FF)
#define USBULPI_START		((uint32_t)0x02000000)
#define USBULPI_RW		((uint32_t)0x00400000)
#define USBULPI_S_BUSY		((uint32_t)0x04000000)
#define USBULPI_S_DONE		((uint32_t)0x08000000)

#define ULPI_TIMEOUT		1000

static bool phy_is_off = FALSE;

static uint32_t
ulpi_read (uint32_t offset)
{
	volatile uint32_t val;
	volatile uint32_t * reg;
	int i;

	reg = (uint32_t *)((uint8_t *)USBHD2.otg + USBULPI_PHYCR);

	*reg = (USBULPI_START | (offset << 16));

	for (i = 0; i < ULPI_TIMEOUT; i++) {
		val = *reg;
		if (val & USBULPI_S_DONE)
			break;
	}

	if (i == ULPI_TIMEOUT) {
		printf ("ULPI PHY read timed out\n");
		return (0);
	}

	return (val & 0xFF);
}

static void
ulpi_write (uint32_t offset, uint32_t val)
{
	volatile uint32_t * reg;
	volatile uint32_t v;
	int i;

	reg = (uint32_t *)((uint8_t *)USBHD2.otg + USBULPI_PHYCR);

	*reg = (USBULPI_START | USBULPI_RW | (offset << 16) | (val & 0xFF));

	for (i = 0; i < ULPI_TIMEOUT; i++) {
		v = *reg;
		if (v & USBULPI_S_DONE)
			break;
	}

	if (i == ULPI_TIMEOUT)
		printf ("ULPI PHY write timed out\n");

	return;
}

static void
ulpi_low (void)
{
	uint32_t regval;

	if (phy_is_off == TRUE)
		return;

	/*
	 * Disable VBUS supply. This should cause
	 * any connected device to disconnect. We wait
	 * for that to happen before proceeding.
	 */

	regval = ulpi_read (0x0A);
	ulpi_write (0x0A, regval & (~0x60));

	chThdSleepMilliseconds (250);

	/* Disable STP pullup */

	regval = ulpi_read (0x07);
	ulpi_write (0x07, regval | 0x80);

	/* Enter low power mode */

	regval = ulpi_read(0x04);
	ulpi_write (0x04, regval & (~0x40));

	phy_is_off = TRUE;

	return;
}

static void
ulpi_high (void)
{
	uint32_t regval;

	if (phy_is_off == FALSE)
		return;

	/* Temporarily change ULPI STP pin to a GPIO */

	palSetLineMode (LINE_ULPI_STP, PAL_MODE_OUTPUT_PUSHPULL);

	/* Force it high for 4ms */

	palSetLine (LINE_ULPI_STP);
	chThdSleepMilliseconds (4);
	palClearLine (LINE_ULPI_STP);

	/* Now put it back to the way it was */

	palSetLineMode (LINE_ULPI_STP, PAL_MODE_ALTERNATE(10) |
	    PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);

	/* Restore STP pullup */

	regval = ulpi_read (0x07);
	ulpi_write (0x07, regval & ~(0x80));

	/* Re-enable VBUS supply */

	regval = ulpi_read (0x0A);
	ulpi_write (0x0A, regval | 0x60);

	phy_is_off = FALSE;

	return;
}
#else
static void
ulpi_low (void)
{
	return;
}

static void
ulpi_high (void)
{
	return;
}
#endif

static void
ulpi_show (void)
{
	char str[64];
	usbh_device_t * dev;

	if (palReadLine (LINE_OTG_FS_VBUS))
		printf ("USB OTG port connected\n");
	else
		printf ("USB OTG port disconnected\n");

	dev = &USBHD2.rootport.device;

	if (dev->status == USBH_DEVSTATUS_DISCONNECTED)
		printf ("USB host port disconnected\n");
	else {
		printf ("USB host port connected\n");
		printf ("Vendor ID: 0x%04X\n", dev->devDesc.idVendor);
		printf ("Device ID: 0x%04X\n", dev->devDesc.idProduct);
		usbhDeviceReadString (dev, str, sizeof(str),
		    dev->devDesc.iManufacturer, dev->langID0);
		printf ("Manufacturer name: [%s]\n", str);
		usbhDeviceReadString (dev, str, sizeof(str),
		    dev->devDesc.iProduct, dev->langID0);
		printf ("Product name: [%s]\n", str);
		usbhDeviceReadString (dev, str, sizeof(str),
		    dev->devDesc.iSerialNumber, dev->langID0);
		printf ("Serial number: [%s]\n", str);
	}

	return;
}

static void
cmd_usb (BaseSequentialStream *chp, int argc, char *argv[])
{
	(void)chp;

	if (argc != 1)
		goto usage;
	if (strcmp (argv[0], "phy-off") == 0)
		ulpi_low ();
	else if (strcmp (argv[0], "phy-on") == 0)
		ulpi_high ();
	else if (strcmp (argv[0], "show") == 0)
		ulpi_show ();
	else {
usage:
		printf ("Usage: usb [phy-off|phy-on|show]\n");
	}

	return;
}

orchard_command("usb", cmd_usb);
