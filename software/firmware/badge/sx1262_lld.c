/* 
 * Copyright (c) 2020
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
#include "osal.h"

#include "sx1262_lld.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * The radio chip is connected to the following pins in addition
 * to the SPI bus:
 *
 * BUSY pin: LINE_ARD_D3 (PB4)
 * RESET pin: LINE_ARD_A0 (PA0)
 * DIO1 pin: LINE_ARD_D5 (PI0)
 * ANT_SW pin: LINE_ARD_D8 (PI2)
 *
 * Note: there are two versions of the schematics for the STM32F746G
 * Discovery board. One is for the rev B board, the other is for rev C.
 * The rev B schematics have an error in them: on the page that shows
 * the Arduino headers, the assignments for the ARD_D5 and ARD_D10
 * pins are swapped. The correct assignment is:
 *
 * ARD_D5: PI0
 * ARD_D10: PA8
 *
 * Oddly, the page that shows all the pin connections on the CPU has
 * this right. Also, the rev C schematics have this error corrected.
 *
 * Unfortunately the error from the rev B schematics was propagated to
 * the ChibiOS BSP for the STM32F746G Discovery board, so it has the
 * pins assigned backwards. As a workaround, we currently use LINE_ARD_D10
 * in this driver.
 */

static THD_WORKING_AREA(waSx1262Thread, 512);

SX1262_Driver SX1262D1;

static void sx1262CmdSend (SX1262_Driver *, void *, uint8_t);
static void sx1262CmdExc (SX1262_Driver *, void *, uint8_t);
static uint8_t sx1262RegRead (SX1262_Driver *, uint16_t);
static void sx1262Reset (SX1262_Driver *);
static void sx1262Calibrate (SX1262_Driver *);
static void sx1262CalImg (SX1262_Driver *);
static bool sx1262IsBusy (SX1262_Driver *);
static void sx1262RegWrite (SX1262_Driver *, uint16_t, uint8_t);
static void sx1262BufRead (SX1262_Driver *, uint8_t *, uint8_t, uint8_t);
static void sx1262BufWrite (SX1262_Driver *, uint8_t *, uint8_t, uint8_t);
static void sx1262Enable (SX1262_Driver *);
static void sx1262Disable (SX1262_Driver *);
static void sx1262Int (void *);
static void sx1262Send (SX1262_Driver *, uint8_t *, uint8_t);
static void sx1262Receive (SX1262_Driver *);
static void sx1262RxHandle (SX1262_Driver *);

static void
sx1262Int (void * arg)
{
	SX1262_Driver * p;

	p = arg;

	osalSysLockFromISR ();
	osalThreadResumeI (&p->sx_threadref, MSG_OK);
	p->sx_service = 1;
	osalSysUnlockFromISR ();

	return;
}

#ifdef debug
static void sx1262Status (SX1262_Driver * p)
{
	SX_GETSTS g;

	g.sx_opcode = SX_CMD_GETSTS;
	sx1262CmdExc (p, &g, sizeof(g));

	printf ("CHIPMODE: %x CMDSTS: %x\n", SX_CHIPMODE(g.sx_status),
	    SX_CMDSTS(g.sx_status));

	return;
}
#endif

static void sx1262Standby (SX1262_Driver * p)
{
	SX_SETSTDBY s;
	SX_SETRXTXFB fb;

	s.sx_opcode = SX_CMD_SETSTDBY;
	s.sx_stdbymode = SX_STDBY_XOSC;
	sx1262CmdSend (p, &s, sizeof(s));

	/* Set fallback mode to match */

	fb.sx_opcode = SX_CMD_SETRXTXFB;
	fb.sx_fbmode = SX_FBMODE_STDBYXOSC;
	sx1262CmdSend (p, &fb, sizeof(fb));

	return;
}

static THD_FUNCTION(sx1262Thread, arg)
{
	SX1262_Driver * p;
	SX_GETIRQ i;
	SX_ACKIRQ a;
	uint16_t irq;

	p = arg;

	chRegSetThreadName ("RadioEvent");

	while (1) {
		osalSysLock ();
		if (p->sx_service == 0)
			osalThreadSuspendS (&p->sx_threadref);
		p->sx_service = 0;
		osalSysUnlock ();

		i.sx_opcode = SX_CMD_GETIRQ;
		sx1262CmdExc (p, &i, sizeof(i));
		irq = __builtin_bswap16(i.sx_irqsts);

		printf ("got a radio interrupt? (%x)\n", irq);

		if (i.sx_irqsts) {
			a.sx_opcode = SX_CMD_ACKIRQ;
			a.sx_irqsts = i.sx_irqsts;
			sx1262CmdSend (p, &a, sizeof(a));
		}

		if (irq & SX_IRQ_RXDONE) {
			sx1262RxHandle (p);
			sx1262Receive (p);
		}

		if (irq & SX_IRQ_TXDONE) {
			sx1262Receive (p);
		}

	}

	/* NOTREACHED */
}

static void
sx1262CmdSend (SX1262_Driver * p, void * cmd, uint8_t len)
{
	int i;
	uint8_t * c;

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	c = cmd;

	spiAcquireBus (p->sx_spi);
	spiSelect (p->sx_spi);

	spiSend (p->sx_spi, len, cmd);

	spiUnselect (p->sx_spi);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	spiReleaseBus (p->sx_spi);

	if (i == SX_DELAY) {
		printf ("Radio command %x timed out\n", c[0]);
	}

	return;
}

static void
sx1262CmdExc (SX1262_Driver * p, void * cmd, uint8_t len)
{
	int i;
	uint8_t * c;

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	c = cmd;

	spiAcquireBus (p->sx_spi);
	spiSelect (p->sx_spi);

	spiExchange (p->sx_spi, len, cmd, cmd);

	spiUnselect (p->sx_spi);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	spiReleaseBus (p->sx_spi);

	if (i == SX_DELAY) {
		printf ("Radio command exchange %x timed out\n", c[0]);
	}

	return;
}

static uint8_t
sx1262RegRead (SX1262_Driver * p, uint16_t reg)
{
	SX_REGREAD r;

	r.sx_opcode = SX_CMD_REGREAD;
	r.sx_address = __builtin_bswap16(reg);
	r.sx_status = 0;
	r.sx_val = 0;

	sx1262CmdExc (p, &r, sizeof(r));

	return (r.sx_val);
}

static void
sx1262RegWrite (SX1262_Driver * p, uint16_t reg, uint8_t val)
{
	SX_REGWRITE r;

	r.sx_opcode = SX_CMD_REGWRITE;
	r.sx_address = __builtin_bswap16(reg);
	r.sx_val = val;

	sx1262CmdExc (p, &r, sizeof(r));

	return;
}

static void
sx1262BufRead (SX1262_Driver * p, uint8_t * buf, uint8_t off, uint8_t len)
{
	int i;
	SX_BUFREAD b;

	b.sx_opcode = SX_CMD_BUFREAD;
	b.sx_offset = off;

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	spiAcquireBus (p->sx_spi);
	spiSelect (p->sx_spi);

	/* Send buffer read command */
	spiExchange (p->sx_spi, sizeof(b), &b, &b);
	/* Read the resulting buffer data */
	spiExchange (p->sx_spi, len, buf, buf);

	spiUnselect (p->sx_spi);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	spiReleaseBus (p->sx_spi);

	if (i == SX_DELAY) {
		printf ("Buffer read timed out\n");
	}

	return;
}

static void
sx1262BufWrite (SX1262_Driver * p, uint8_t * buf, uint8_t off, uint8_t len)
{
	int i;
	SX_BUFWRITE b;

	b.sx_opcode = SX_CMD_BUFWRITE;
	b.sx_offset = off;

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	spiAcquireBus (p->sx_spi);
	spiSelect (p->sx_spi);

	/* Send buffer write command */
	spiSend (p->sx_spi, sizeof(b), &b);
	/* Send the buffer data */
	spiSend (p->sx_spi, len, buf);

	spiUnselect (p->sx_spi);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	spiReleaseBus (p->sx_spi);

	if (i == SX_DELAY) {
		printf ("Buffer write timed out\n");
	}

	return;
}

static void
sx1262Reset (SX1262_Driver * p)
{
	int i;

	/* Perform a hard reset of the chip. */

	palClearLine (LINE_ARD_A0);
	chThdSleepMilliseconds (50);
	palSetLine (LINE_ARD_A0);
	chThdSleepMilliseconds (50);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	sx1262Standby (p);

	return;
}

static bool
sx1262IsBusy (SX1262_Driver * p)
{
	(void)p;

	if (palReadLine (LINE_ARD_D3) == 1)
		return (TRUE);
	return (FALSE);
}

static void
sx1262Calibrate (SX1262_Driver * p)
{
	SX_CALIBRATE c;
	SX_GETERRS ge;
	SX_CLRERRS ce;

	ce.sx_opcode = SX_CMD_CLEARERRS;
	ge.sx_opcode = SX_CMD_GETERRS;
	sx1262CmdSend (p, &ce, sizeof(ce));

	c.sx_opcode = SX_CMD_CALIBRATE;
	c.sx_calparam = SX_CALIBRATE_RC64K|SX_CALIBRATE_RC13M|
		SX_CALIBRATE_PLL|SX_CALIBRATE_ADCPULSE|
		SX_CALIBRATE_ADCBULKN|SX_CALIBRATE_ADCBULKP|
		SX_CALIBRATE_IMAGEREJ;

	sx1262CmdSend (p, &c, sizeof(c));

	/* Check for errors */

	sx1262CmdExc (p, &ge, sizeof(ge));

	if (__builtin_bswap16(ge.sx_errs) & 0x3F)
		printf ("Calibration error detected (%x).\n",
		    __builtin_bswap16(ge.sx_errs));

	return;
}


static void
sx1262CalImg (SX1262_Driver * p)
{
	SX_CALIMG c;

	c.sx_opcode = SX_CMD_CALIMG;

	if (p->sx_freq > 900000000) {
		c.sx_freq1 = 0xE1; 
		c.sx_freq2 = 0xE9; 
	} else if (p->sx_freq > 850000000) {
		c.sx_freq1 = 0xD7; 
		c.sx_freq2 = 0xDB; 
	} else if (p->sx_freq > 770000000) {
		c.sx_freq1 = 0xC1; 
		c.sx_freq2 = 0xC5; 
	} else if (p->sx_freq > 460000000) {
		c.sx_freq1 = 0x75; 
		c.sx_freq2 = 0x81; 
	} else if (p->sx_freq > 425000000) {
		c.sx_freq1 = 0x6B; 
		c.sx_freq2 = 0x6F;
	}

	sx1262CmdSend (p, &c, sizeof(c));

	return;
}

static void
sx1262Enable (SX1262_Driver * p)
{
	uint8_t mantissa, exponent, reg;
	/*
	 * You might be thinking: "Hey Bill, instead of defining
	 * each of these structures separately, why not combine them
	 * into a single union so that you use less storage?"
	 *
	 * The answer is: because the way stack space allocation is
	 * done, the compiler will effectively do that anyway 
	 * as part of its optimization pass (assuming you don't turn
	 * optimization off for debugging). And this way the code
	 * is less confusing to read.
	 */
	SX_SETPKT pkt;
	SX_SETPKTPARAM_LORA pkp;
	SX_STOPTIMER t;
	SX_SETLORASYMTO s;
	SX_SETMODPARAM_LORA m;
	SX_SETFREQ f;
	SX_SETDIO2ASRFSW d;
	SX_SETREGMODE r;
	SX_SETBUFBASE b;
	SX_SETPACFG pa;
	SX_SETTXPARAM tp;
	SX_SETDIOIRQ di;

	/* Reset the radio and put it in standby */

	sx1262Reset (p);

	/* Set DIO2 as RF control switch */

	d.sx_opcode = SX_CMD_SETDIO2ASRFSW;
	d.sx_enable = SX_DIO2_RFSW;
	sx1262CmdSend (p, &d, sizeof(d));

	/* Set regulator mode to DCDC */

	r.sx_opcode = SX_CMD_SETREGMODE;
	r.sx_regparam = SX_REGMODE_LDO_DCDC;
	sx1262CmdSend (p, &r, sizeof(r));

	/* Set RX and TX buffer base addresses to 0 */

	b.sx_opcode = SX_CMD_SETBUFBASE;
	b.sx_txbase = 0;
	b.sx_rxbase = 0;
	sx1262CmdSend (p, &b, sizeof(b));

	/* Set transmitter PA config */

	pa.sx_opcode = SX_CMD_SETPACFG;
	pa.sx_padutycycle = 0x4;
	pa.sx_hpmax = 0x7;
	pa.sx_devsel = 0x0; /* 0 == 1262, 1 == 1261 */
	pa.sx_palut = 0x1; /* always 1 */
	sx1262CmdSend (p, &pa, sizeof(pa));

	/* Set overcurrent protection (140mA for whole device) */

	sx1262RegWrite (p, SX_REG_OCPCFG, SX_OCP_1262_140MA);

	/* Set TX params */

	tp.sx_opcode = SX_CMD_SETTXPARAM;
	tp.sx_power = 22;
	tp.sx_ramptime = SX_RAMPTIME_200US;
	sx1262CmdSend (p, &tp, sizeof(tp));

	/*
	 * Setting the TX parameters may cause the radio to enter
	 * the TX state. Put it back into standby state.
	 */

	sx1262Standby (p);

	/* Set up interrupt event signalling -- we use DIO1 */

	di.sx_opcode = SX_CMD_SETDIOIRQ;
	di.sx_irqmask = __builtin_bswap16 (SX_IRQ_RXDONE | SX_IRQ_TXDONE);
	di.sx_dio1mask = di.sx_irqmask;
	di.sx_dio2mask = 0;
	di.sx_dio3mask = 0;
	sx1262CmdSend (p, &di, sizeof(di));

	sx1262CalImg (p);
	sx1262Calibrate (p);

	/* Set channel */

	f.sx_opcode = SX_CMD_SETFREQ;
	f.sx_freq = __builtin_bswap32(SX_FREQ(p->sx_freq));
	sx1262CmdSend (p, &f, sizeof(f));

	/* Perform RX and TX configuration */

	t.sx_opcode = SX_CMD_STOPTIMER;
	t.sx_stoptimerparam = SX_STOPTIMER_SYNCWORD;
	sx1262CmdSend (p, &t, sizeof(t));

	s.sx_opcode = SX_CMD_SETLORASYMTO;
	s.sx_symbnum = p->sx_symtimeout;
	sx1262CmdSend (p, &s, sizeof(s));

	mantissa = p->sx_symtimeout >> 1;
	exponent = 0;
	reg = 0;

	while (mantissa > 31) {
		mantissa >>= 2;
		exponent++;
	}

	reg = exponent + (mantissa << 3);

	sx1262RegWrite (p, SX_REG_SYNCTIMEOUT, reg);

	/* Set the radio to LoRa mode */

	pkt.sx_opcode = SX_CMD_SETPKT;
	pkt.sx_pkttype = SX_PKT_LORA;
	sx1262CmdSend (p, &pkt, sizeof(pkt));

	/* Then set modulation parameters */

	m.sx_opcode = SX_CMD_SETMODPARAM;
	m.sx_sf = SX_LORA_SF7;
	m.sx_bw = SX_LORA_BW_500;
	m.sx_cr = SX_LORA_CR_4_5;
	m.sx_ldopt = SX_LORA_LDOPT_OFF;
	sx1262CmdSend (p, &m, sizeof(m));

	/* Then set the packet parameters */

	pkp.sx_opcode = SX_CMD_SETPKTPARAM;
	pkp.sx_preamlen = __builtin_bswap16(p->sx_preamlen);
	pkp.sx_headertype = SX_LORA_HDR_FIXED;
	pkp.sx_payloadlen = p->sx_pktlen;
	pkp.sx_crctype = SX_LORA_CRCTYPE_ON;
	pkp.sx_invertiq = SX_LORA_IQ_STANDARD;
	sx1262CmdSend (p, &pkp, sizeof(pkp));

	/* Set sync word */

	sx1262RegWrite (p, SX_REG_LORA_SYNC_MSB, p->sx_syncword >> 8);
	sx1262RegWrite (p, SX_REG_LORA_SYNC_LSB, p->sx_syncword & 0xFF);

	/* Apply workaround for I/Q optimization */

	reg = sx1262RegRead (p, SX_REG_IQPOL_SETUP);
	reg |= 0x04;
	sx1262RegWrite (p, SX_REG_IQPOL_SETUP, reg);

	/* Apply workaround for 500KHz operation quality */

	reg = sx1262RegRead (p, SX_REG_TXMOD);
	reg &= 0xFB;
	sx1262RegWrite (p, SX_REG_TXMOD, reg);

	/* Apply workaround for better resistance to antenna mismatch */

	reg = sx1262RegRead (p, SX_REG_TXCLAMPCFG);
	reg |= 0x1E;
	sx1262RegWrite (p, SX_REG_TXCLAMPCFG, reg);

	/* Set RX gain to boosted */

	sx1262RegWrite (p, SX_REG_RXGAIN, SX_RXGAIN_BOOSTED);

	/* Make sure radio is in the right standby mode before we exit */

	sx1262Standby (p);

	return;
}

static void
sx1262Disable (SX1262_Driver * p)
{
	sx1262Reset (p);
	return;
}

void sx1262Send (SX1262_Driver * p, uint8_t * buf, uint8_t len)
{
	SX_SETTX t;

	sx1262BufWrite (p, buf, 0, len);

	t.sx_opcode = SX_CMD_SETTX;
	t.sx_timeo0 = 0;
	t.sx_timeo1 = 0;
	t.sx_timeo2 = 0;

	sx1262CmdSend (p, &t, sizeof (t));

	return;
	
}

void sx1262Receive (SX1262_Driver * p)
{
	SX_SETRX r;

	r.sx_opcode = SX_CMD_SETRX;
	r.sx_timeo0 = 0;
	r.sx_timeo1 = 0;
	r.sx_timeo2 = 0;

	sx1262CmdSend (p, &r, sizeof (r));

	return;
}

void sx1262RxHandle (SX1262_Driver * p)
{
	SX_GETRXBUFSTS s;
	SX_GETPKTSTS_LORA sp;

	sp.sx_opcode = SX_CMD_GETPKTSTS;
	sx1262CmdExc (p, &sp, sizeof(sp));

	s.sx_opcode = SX_CMD_GETRXBUFSTS;
	sx1262CmdExc (p, &s, sizeof(s));

	sx1262BufRead (p, p->sx_rxbuf, s.sx_rxstbufptr, s.sx_rxpaylen);

printf ("RX payload: %d offset: %d ", s.sx_rxpaylen, s.sx_rxstbufptr);
printf ("SnR: %x RSSI1: %x RSSI2: %x\n", sp.sx_snrpkt, sp.sx_rssipkt,
    sp.sx_rssipkt);

printf ("PAYLOAD: [%s]\n", p->sx_rxbuf);

	return;
}

void
sx1262Start (void)
{
	uint8_t ocp;

	/* Set the SPI interface */

	SX1262D1.sx_spi = &SPID2;
	SX1262D1.sx_freq = 916000000;
	SX1262D1.sx_syncword = SX_LORA_SYNC_PRIVATE;
	SX1262D1.sx_symtimeout = 0;
	SX1262D1.sx_pktlen = 64;
	SX1262D1.sx_preamlen = 8;
	SX1262D1.sx_service = 0;

	/* Configure I/O pins */

	/* Reset */
	palSetLineMode (LINE_ARD_A0, PAL_STM32_PUPDR_PULLUP |
	     PAL_STM32_MODE_OUTPUT);

	/* Busy */
	palSetLineMode (LINE_ARD_D3, PAL_STM32_PUPDR_PULLDOWN |
	     PAL_STM32_MODE_INPUT);

	/* Antenna switch */
	palSetLineMode (LINE_ARD_D8, PAL_STM32_PUPDR_PULLUP |
	     PAL_STM32_MODE_OUTPUT);
	palSetLine (LINE_ARD_D8);

	/* DIO1 */
	palSetLineMode (LINE_ARD_D10, PAL_STM32_PUPDR_PULLDOWN |
	     PAL_STM32_MODE_INPUT);
	palSetLineCallback (LINE_ARD_D10, sx1262Int, &SX1262D1);
	palEnableLineEvent (LINE_ARD_D10, PAL_EVENT_MODE_RISING_EDGE);

	/* Reset the chip */

	sx1262Reset (&SX1262D1);

	/*
	 * Try to probe for the radio chip. We do this by
	 * reading the overcurrent protection register and
	 * checking that its reset default value makes sense.
	 */

	ocp = sx1262RegRead (&SX1262D1, SX_REG_OCPCFG);

	if (ocp == SX_OCP_1261_60MA)
		printf ("SemTech SX1262 radio enabled\n");
	else {
		printf ("No radio detected\n");
		return;
	}

	/* Set up interrupt event thread */

	SX1262D1.sx_thread = chThdCreateStatic (waSx1262Thread,
	    sizeof(waSx1262Thread), NORMALPRIO - 1, sx1262Thread, &SX1262D1);

	/* Enable the radio */

	sx1262Enable (&SX1262D1);

	/* Send 5 packets as a quick test */

	{
		char * b;
		int i;

		b = malloc (64);

		for (i = 0; i < 5; i++) {
			sprintf (b, "testing%d...\n", i);
			sx1262Send (&SX1262D1, (uint8_t *)b, 64);
			chThdSleepMilliseconds (1000);
		}

		free (b);
		sx1262Receive (&SX1262D1);
	}

	return;
}

void
sx1262Stop (SX1262_Driver * p)
{
	sx1262Disable (p);
	return;
}
