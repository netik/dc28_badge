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
#include "badge.h"

#include <lwip/opt.h>
#include <lwip/def.h>
#include <lwip/mem.h>
#include <lwip/pbuf.h>
#include <lwip/sys.h>
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include <lwip/tcpip.h>
#include <netif/etharp.h>
#include <lwip/netifapi.h>
#include <lwip/inet.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

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
 * the ChibiOS BSP for the STM32F746G Discovery board. Versions of
 * ChibiOS prior to 20.3.1 have this bug in it, so for those versions
 * it's necessary to workaround it by using LINE_ARD_D10 in place of
 * LIND_ARD_D5 where applicable. Fortunately we're now using version
 * 20.3.3 where this is fixed.
 */

SX1262_Driver SX1262D1;

static void sx1262CmdSend (SX1262_Driver *, void *, uint8_t);
static void sx1262CmdExc (SX1262_Driver *, void *, uint8_t);
static uint8_t sx1262RegRead (SX1262_Driver *, uint16_t);
static void sx1262RegWrite (SX1262_Driver *, uint16_t, uint8_t);
static void sx1262BufRead (SX1262_Driver *, uint8_t *, uint8_t, uint8_t);
static void sx1262BufWrite (SX1262_Driver *, uint8_t *, uint8_t, uint8_t);
static void sx1262GfskConfig (SX1262_Driver * p);
static void sx1262LoraConfig (SX1262_Driver * p);
static void sx1262Reset (SX1262_Driver *);
static void sx1262ChanSet (SX1262_Driver *);
static void sx1262Calibrate (SX1262_Driver *);
static void sx1262CalImg (SX1262_Driver *);
static bool sx1262IsBusy (SX1262_Driver *);
static void sx1262Int (void *);
static void sx1262Timeout (void *);
static void sx1262Send (SX1262_Driver *, uint8_t *, uint8_t);
static void sx1262ReceiveSet (SX1262_Driver *);
static void sx1262TransmitSet (SX1262_Driver *);
static void sx1262StandbySet (SX1262_Driver *);
#ifdef SX_MANUAL_CAD
static void sx1262CadSet (SX1262_Driver *);
#endif
static err_t sx1262EthernetInit (struct netif *);
static err_t sx1262LinkOutput (struct netif *, struct pbuf *);
static err_t sx1262Output (struct netif *, struct pbuf *, const ip4_addr_t *);
static void sx1262RxHandle (SX1262_Driver *);
#ifdef debug
static void sx1262Status (SX1262_Driver *);
#endif

static void
sx1262Int (void * arg)
{
	SX1262_Driver * p;

	p = arg;

	osalSysLockFromISR ();
	p->sx_service |= 0x00000001;
	if (p->sx_dispatch)
		osalThreadResumeI (&p->sx_threadref, MSG_OK);
	else
		osalThreadResumeI (&p->sx_txwait, MSG_OK);
	osalSysUnlockFromISR ();

	return;
}

static void
sx1262Timeout (void * arg)
{
	SX1262_Driver * p;

	p = arg;

	osalSysLockFromISR ();
	p->sx_service |= 0x00000002;
	osalThreadResumeI (&p->sx_txwait, MSG_OK);
	osalSysUnlockFromISR ();

	return;
}

#ifdef debug
static void
sx1262Status (SX1262_Driver * p)
{
	SX_GETSTS g;

	g.sx_opcode = SX_CMD_GETSTS;
	sx1262CmdExc (p, &g, sizeof(g));

	printf ("CHIPMODE: %x CMDSTS: %x (%x)\n", SX_CHIPMODE(g.sx_status),
	    SX_CMDSTS(g.sx_status), g.sx_status);

	return;
}
#endif

static void
sx1262StandbySet (SX1262_Driver * p)
{
	SX_SETSTDBY s;
	SX_SETRXTXFB fb;

	/* Set to XOSC standby mode */

	s.sx_opcode = SX_CMD_SETSTDBY;
	s.sx_stdbymode = SX_STDBY_XOSC;
	sx1262CmdSend (p, &s, sizeof(s));

	/* Set fallback mode to match */

	fb.sx_opcode = SX_CMD_SETRXTXFB;
	fb.sx_fbmode = SX_FBMODE_STDBYXOSC;
	sx1262CmdSend (p, &fb, sizeof(fb));

	return;
}

#ifdef SC_MANUAL_CAD
static void
sx1262CadSet (SX1262_Driver * p)
{
	SX_SETSTDBY c;
	SX_SETSTDBY s;

	s.sx_opcode = SX_CMD_SETSTDBY;
	s.sx_stdbymode = SX_STDBY_XOSC;
	sx1262CmdSend (p, &s, sizeof(s));

	c.sx_opcode = SX_CMD_SETCAD;
	sx1262CmdSend (p, &c, sizeof(c));

	return;
}
#endif

static
THD_FUNCTION(sx1262Thread, arg)
{
	SX1262_Driver * p;
	SX_GETIRQ i;
	SX_ACKIRQ a;
	uint16_t irq;

	p = arg;

	while (1) {
		osalSysLock ();
		if (p->sx_service == 0)
			osalThreadSuspendS (&p->sx_threadref);
		p->sx_service = 0;
		osalSysUnlock ();

		osalMutexLock (&p->sx_mutex);

		i.sx_opcode = SX_CMD_GETIRQ;
		i.sx_irqsts = 0;
		sx1262CmdExc (p, &i, sizeof(i));
		irq = __builtin_bswap16 (i.sx_irqsts);

		if (i.sx_irqsts) {
			a.sx_opcode = SX_CMD_ACKIRQ;
			a.sx_irqsts = i.sx_irqsts;
			sx1262CmdSend (p, &a, sizeof(a));
		}

#ifdef debug
		printf ("got a radio interrupt? (%x)\n", irq);
#endif

		if (irq & SX_IRQ_RXDONE) {
			/* Ignore packets with bad CRC */
			if (!(irq & (SX_IRQ_CRCERR | SX_IRQ_HDRERR)))
				sx1262RxHandle (p);
			else
				sx1262ReceiveSet (p);
		}

#ifdef SX_MANUAL_CAD
		if (irq & SX_IRQ_CADDONE) {
			if (irq & SX_IRQ_CADDET)
				sx1262CadSet (p);
			else
				sx1262TransmitSet (p);
		}
#endif

		osalMutexUnlock (&p->sx_mutex);
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
		chThdSleep (1);
	}

	memcpy (p->sx_cmdbuf, cmd, len);
	c = cmd;

	spiAcquireBus (p->sx_spi);

	spiSelect (p->sx_spi);

	badge_lpidle_suspend ();

	spiSend (p->sx_spi, len, p->sx_cmdbuf);

	badge_lpidle_resume ();

	spiUnselect (p->sx_spi);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleep (1);
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
	uint8_t c;

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleep (1);
	}

	memcpy (p->sx_cmdbuf, cmd, len);
	c = *((uint8_t *)cmd);

	spiAcquireBus (p->sx_spi);

	spiSelect (p->sx_spi);

	badge_lpidle_suspend ();

	spiExchange (p->sx_spi, len, p->sx_cmdbuf, p->sx_cmdbuf);

	badge_lpidle_resume ();

	spiUnselect (p->sx_spi);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleep (1);
	}

	spiReleaseBus (p->sx_spi);

	memcpy (cmd, p->sx_cmdbuf, len);

	if (i == SX_DELAY) {
		printf ("Radio command exchange %x timed out\n", c);
	}

	return;
}

static uint8_t
sx1262RegRead (SX1262_Driver * p, uint16_t reg)
{
	SX_REGREAD r;

	r.sx_opcode = SX_CMD_REGREAD;
	r.sx_address = __builtin_bswap16 (reg);
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
	r.sx_address = __builtin_bswap16 (reg);
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
		chThdSleep (1);
	}

	spiAcquireBus (p->sx_spi);
	spiSelect (p->sx_spi);

	/* Send buffer read command */

	badge_lpidle_suspend ();

	spiExchange (p->sx_spi, sizeof(b), &b, &b);

	/* Read the resulting buffer data */

	spiExchange (p->sx_spi, len, buf, buf);

	badge_lpidle_resume ();

	spiUnselect (p->sx_spi);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleep (1);
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
		chThdSleep (1);
	}

	spiAcquireBus (p->sx_spi);
	spiSelect (p->sx_spi);

	badge_lpidle_suspend ();

	/* Send buffer write command */
	spiSend (p->sx_spi, sizeof(b), &b);
	/* Send the buffer data */
	spiSend (p->sx_spi, len, buf);

	badge_lpidle_resume ();

	spiUnselect (p->sx_spi);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleep (1);
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
	chThdSleepMilliseconds (150);
	palSetLine (LINE_ARD_A0);
	chThdSleepMilliseconds (150);

	for (i = 0; i < SX_DELAY; i++) {
		if (sx1262IsBusy (p) == FALSE)
			break;
		chThdSleepMilliseconds (1);
	}

	return;
}

static bool
sx1262IsBusy (SX1262_Driver * p)
{
	(void)p;

	__DSB();
	if (palReadLine (LINE_ARD_D3) == 1)
		return (TRUE);
	return (FALSE);
}

static void
sx1262ChanSet (SX1262_Driver * p)
{
	SX_SETFREQ f;

	sx1262CalImg (p);

	f.sx_opcode = SX_CMD_SETFREQ;
	f.sx_freq = __builtin_bswap32 (SX_FREQ(p->sx_freq));
	sx1262CmdSend (p, &f, sizeof(f));

	return;
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

	if (__builtin_bswap16 (ge.sx_errs) & 0x3F)
		printf ("Calibration error detected (%x).\n",
		    __builtin_bswap16 (ge.sx_errs));

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
sx1262LoraConfig (SX1262_Driver * p)
{
	uint8_t mantissa, exponent, reg;
	SX_SETPKT pkt;
	SX_SETPKTPARAM_LORA pkp;
	SX_SETLORASYMTO s;
	SX_SETMODPARAM_LORA m;
	SX_SETCADPARAMS cad;

	s.sx_opcode = SX_CMD_SETLORASYMTO;
	s.sx_symbnum = p->sx_lora.sx_symtimeout;
	sx1262CmdSend (p, &s, sizeof(s));

	mantissa = p->sx_lora.sx_symtimeout >> 1;
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

	/* Then set modulation parameters. */

	m.sx_opcode = SX_CMD_SETMODPARAM;
	m.sx_sf = p->sx_lora.sx_spreadfactor;
	m.sx_bw = p->sx_lora.sx_bandwidth;
	m.sx_cr = p->sx_lora.sx_coderate;
	m.sx_ldopt = SX_LORA_LDOPT_OFF;
	sx1262CmdSend (p, &m, sizeof(m));

	/*
	 * Then set the packet parameters.
	 *
	 * A note about CRCs:
	 *
	 * LoRa frames may or may not include a header. When there
	 * is no header, it's assumed that the stations communicating
	 * via LoRa have previously agreed on a frame payload size and
	 * coding rate, and all frames will be of the same fixed length.
	 * An optional CRC may be appended at the end of of the frame.
	 * This is called implicit header mode.
	 *
	 * If there is a header, then the header will specify the
	 * frame length and coding rate of the payload, and indicate
	 * whether or not the payload includes a checksum. The header
	 * will also contain a checksum of its own. This is called
	 * explicit header mode.
	 *
	 * Unfortunately the SX1261/SX1262 reference manual is unclear
	 * as to if or when the chip will insert or check for valid
	 * checksums and indicate checksum errors.
	 *
	 * The "set packet parameters" command lets you specify if you
	 * want to use implicit (fixed) or explicit (variable) header
	 * mode, and lets you specify a "CRC type" of on or off.
	 *
	 * But there's some confusion: for explicit (variable) header
	 * mode, there can be two checksums (one for the header and
	 * one for the payload), but there is only one "CRC mode"
	 * control. Does it control the inclusion of both the header
	 * and payload CRCs, or just one? And if it's just one, then
	 * which one?
	 *
	 * Also, explict (variable) header mode implies that each time
	 * a station tranmits a frame, it can be of arbitrary size. So
	 * how does software specify the TX packet size? The "set packet
	 * parameters" command seems to be the only way to specify any
	 * packet size, but does that means we have to issue a new "set
	 * packet parameters" command every time we want to send a packet?
	 * (I mean, we can do that, but it seems inefficient.)
	 *
	 * Experimentation has shown that with fixed (implicit) header
	 * mode, the "CRC type" field does indeed control whether or
	 * not a checksum is included with the frame, and it also
	 * controls whether or not the chip will validate the checksum
	 * on receive. So with SX_LORA_HDR_FIXED and SX_LORA_CRCTYPE_ON,
	 * we will get an SX_IRQ_CRCERR interrupt indication when a
	 * packet with a bad payload checksum is received.
	 *
	 * The situation with variable (explicit) header mode isn't as
	 * clear. With SX_LORA_HDR_VARIABLE, if one radio is set to
	 * SX_LORA_CRCTYPE_OFF and the other is set to SX_LORA_CRCTYPE_ON,
	 * both radios are still able to successfully exhange packets
	 * in both directions (no SX_IRQ_CRCERR or SX_IRQ_HDRERR errors
	 * are flagged). This seems to imply that when using explicit
	 * headers, the transmitter always generates both the header
	 * and payload checksums.
	 *
	 * However there is some evidence that the "CRC type" setting
	 * does control the receiver behavior with variable (explicit)
	 * headers. When leaving the radio in continuous receive mode,
	 * it will sometimes generate false packet receive events. (One
	 * assumes this is due to the modem incorrectly interpreting
	 * radio interference as a packet.) With SX_LORA_CRCTYPE_ON,
	 * the radio seems to signal SX_IRQ_HDRERR errors on these
	 * bogus frames and proceeds no further. However when using
	 * SX_LORA_CRCTYPE_OFF, the receiver has been observed to signal
	 * both SX_IRQ_HDRERR and SX_IRQ_CRCERR events, but still also
	 * generate SX_IRQ_RXDONE (packet received) events as well.
	 *
	 * So from this I've inferred the following:
	 *
	 *                     SX_LORA_HDR_FIXED:
	 *
	 *     SX_LORA_CRCTYPE_ON            SX_LORA_CRCTYPE_OFF
	 *     ----------------------------  ---------------------------
	 *     TX: PL checksum sent          TX: no checksum sent
	 *     RX: PL checksum checked       RX: checksum not checked
	 *
	 *                     SX_LORA_HDR_VARIABLE:
	 *
	 *     SX_LORA_CRCTYPE_ON            SX_LORA_CRCTYPE_OFF
	 *     ----------------------------  ---------------------------
	 *     TX: HDR+PL checksums sent     TX: HDR+PL checksums sent
	 *     RX: HDR+PL checksums checked  RX: no checksums checked
	 */

	pkp.sx_opcode = SX_CMD_SETPKTPARAM;
	pkp.sx_preamlen = __builtin_bswap16 (p->sx_lora.sx_preamlen);
	pkp.sx_headertype = SX_LORA_HDR_VARIABLE;
	pkp.sx_payloadlen = p->sx_pktlen;
	pkp.sx_crctype = SX_LORA_CRCTYPE_ON;
	pkp.sx_invertiq = SX_LORA_IQ_STANDARD;
	sx1262CmdSend (p, &pkp, sizeof(pkp));

	/* Then set CAD parameters */

	cad.sx_opcode = SX_CMD_SETCADPARAM;
	cad.sx_cadsymnum = SX_CAD_SYMB_8;
	cad.sx_caddetpeak = p->sx_lora.sx_spreadfactor + 13;
	cad.sx_caddetmin = 10;
	cad.sx_cadexitmode = SX_CAD_EXITMODE_CADONLY;
	cad.sx_cadtimeo[0] = 0;
	cad.sx_cadtimeo[1] = 0;
	cad.sx_cadtimeo[2] = 0;
	sx1262CmdSend (p, &cad, sizeof(cad));

	/* Set sync word */

	sx1262RegWrite (p, SX_REG_LORA_SYNC_MSB, p->sx_lora.sx_syncword >> 8);
	sx1262RegWrite (p, SX_REG_LORA_SYNC_LSB, p->sx_lora.sx_syncword & 0xFF);

	return;
}

static void
sx1262GfskConfig (SX1262_Driver * p)
{
	SX_SETPKT pkt;
	SX_SETMODPARAM_GFSK m;
	SX_SETPKTPARAM_GFSK pkp;
	uint32_t val;

	/*
	 * According to the manual, we must perform the following
	 * operations in order: set packet type, set modulation
	 * parameters, then finally set packet parameters. (In spite
	 * of this, some SemTech does the packet parameter setup
	 * before the modulation parameters.)
	 */

	/* Set the radio to GFSK mode */

	pkt.sx_opcode = SX_CMD_SETPKT;
	pkt.sx_pkttype = SX_PKT_GFSK;
	sx1262CmdSend (p, &pkt, sizeof(pkt));

	/* Then set modulation parameters. */

	m.sx_opcode = SX_CMD_SETMODPARAM;
	val = SX_GFSKBR(p->sx_gfsk.sx_bitrate);
	m.sx_gfskbr[0] = (val >> 16) & 0xFF;
	m.sx_gfskbr[1] = (val >> 8) & 0xFF;
	m.sx_gfskbr[2] = val & 0xFF;
	m.sx_bw = p->sx_gfsk.sx_bandwidth;
	m.sx_pshape = SX_PSHAPE_GAUSSBT1;
	val = SX_GFSKDEV(p->sx_gfsk.sx_deviation);
	m.sx_fdev[0] = (val >> 16) & 0xFF;
	m.sx_fdev[1] = (val >> 8) & 0xFF;
	m.sx_fdev[2] = val & 0xFF;
	sx1262CmdSend (p, &m, sizeof(m));

	/*
	 * Then set packet parameters.
	 * Note: preamble length and syncword length are specified
	 * to the chip in bits, so we multiply the supplied byte
	 * values by 8.
	 */

	val = p->sx_gfsk.sx_preamlen * 8;
	pkp.sx_opcode = SX_CMD_SETPKTPARAM;
	pkp.sx_preamlen = __builtin_bswap16 (val);
	pkp.sx_preamdetlen = SX_GFSK_PREAM_32BITS;
	pkp.sx_syncwordlen = p->sx_gfsk.sx_syncwordlen * 8;
	pkp.sx_addrcomp = SX_GFSK_ADDRCOMP_OFF;
	pkp.sx_pkttype = SK_GFSK_PKTTYPE_VARIABLE;
	pkp.sx_paylen = p->sx_pktlen;
	pkp.sx_crctype = SK_GFSK_CRCTYPE_2BYTEIN;
	pkp.sx_whitening = SK_GFSK_WHITENING_ON;
	sx1262CmdSend (p, &pkp, sizeof(pkp));

	/* Set sync word */

	for (val = 0; val < p->sx_gfsk.sx_syncwordlen; val++)
		sx1262RegWrite (p, SX_REG_SYNCWORD_0 + val,
		    p->sx_gfsk.sx_syncword[val]);

	/*
	 * Set whitening seed
	 *
	 * Note: In the register table, with regards to the whitening
	 * initial value MSB register, the manual says "the user
	 * should not change the value of the 7 MSB of this register."
	 *
	 * In reality, this "should" should be a "must." You must read
	 * whatever value is currently in this register and preserve
	 * the 7 most significant bits when setting the whitening seed.
	 *
	 * If you don't do this, the receiver will actually lock up the
	 * first time you receive a valid GFSK packet. Curiously though,
	 * the transmitter will work just fine.
	 */

	val = SX_WHITENING_SEED;
	val &= 0x01FF;
	val |= (sx1262RegRead (p, SX_REG_WHITEINIT_MSB) & 0xFE) << 8;

	sx1262RegWrite (p, SX_REG_WHITEINIT_MSB, (val >> 8) & 0xFF);
	sx1262RegWrite (p, SX_REG_WHITEINIT_LSB, val & 0xFF);

	/* Set CRC seed */

	sx1262RegWrite (p, SX_REG_CRCINIT_MSB, SX_CRC_SEED_CCITT >> 8);
	sx1262RegWrite (p, SX_REG_CRCINIT_LSB, SX_CRC_SEED_CCITT & 0xFF);

	/* Set CRC polynomial */

	sx1262RegWrite (p, SX_REG_CRCPOLY_MSB, SX_CRC_POLY_CCITT >> 8);
	sx1262RegWrite (p, SX_REG_CRCPOLY_LSB, SX_CRC_POLY_CCITT & 0xFF);

	/* Set node and broadcast address */

	sx1262RegWrite (p, SX_REG_NODEADDR, 0xFF);
	sx1262RegWrite (p, SX_REG_BCASTADDR, 0xFF);

	return;
}

void
sx1262Enable (SX1262_Driver * p)
{
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
	SX_STOPTIMER t;
	SX_SETDIO2ASRFSW d;
	SX_SETREGMODE r;
	SX_SETBUFBASE b;
	SX_SETPACFG pa;
	SX_SETTXPARAM tp;
	SX_SETDIOIRQ di;
	uint8_t reg;

	/* Reset the radio and put it in standby */

	sx1262Reset (p);

	sx1262StandbySet (p);

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

	if (p->sx_tx_power == SX_TX_POWER_22DB) {
		pa.sx_padutycycle = 0x4;
		pa.sx_hpmax = 0x7;
	} else {
		pa.sx_padutycycle = 0x2;
		pa.sx_hpmax = 0x2;
	}

	pa.sx_devsel = 0x0; /* 0 == 1262, 1 == 1261 */
	pa.sx_palut = 0x1; /* always 1 */
	sx1262CmdSend (p, &pa, sizeof(pa));

	/* Set overcurrent protection (140mA for whole device) */

	sx1262RegWrite (p, SX_REG_OCPCFG, SX_OCP_1262_140MA);

	/*
	 * Set TX params
	 * Power can be as high as 22dB, but current draw
	 * at that level can be as high as 100mA, so for now
	 * we keep it down to 14dB.
	 */

	tp.sx_opcode = SX_CMD_SETTXPARAM;

	if (p->sx_tx_power == SX_TX_POWER_22DB)
		tp.sx_power = 22;
	else
		tp.sx_power = 14;

	tp.sx_ramptime = SX_RAMPTIME_200US;
	sx1262CmdSend (p, &tp, sizeof(tp));

	/*
	 * Set up interrupt event signalling -- we use DIO1.
	 * Note that we unmask several different IRQ events,
	 * but we only program DIO1 to toggle for RX and TX
	 * completions, to limit how often we actually get
	 * woken up. For RX, we want to be notified if the
	 * receiver thinks it's actually received a packet,
	 * but once that happens we also need to check if
	 * the packet is actually valid by looking for CRC
	 * or header errors.
	 */

	di.sx_opcode = SX_CMD_SETDIOIRQ;
	di.sx_irqmask = __builtin_bswap16 (SX_IRQ_RXDONE |
	    SX_IRQ_TXDONE | SX_IRQ_CRCERR | SX_IRQ_HDRERR |
	    SX_IRQ_CADDONE | SX_IRQ_CADDET | SX_IRQ_TIMEO);
	di.sx_dio1mask = __builtin_bswap16(SX_IRQ_RXDONE |
#ifdef SX_MANUAL_CAD
	    SX_IRQ_CADDONE | SX_IRQ_CADDET |
#endif
	    SX_IRQ_TXDONE);
	di.sx_dio2mask = 0;
	di.sx_dio3mask = 0;
	sx1262CmdSend (p, &di, sizeof(di));

	t.sx_opcode = SX_CMD_STOPTIMER;
	t.sx_stoptimerparam = SX_STOPTIMER_SYNCWORD;
	sx1262CmdSend (p, &t, sizeof(t));

	/* Perform calibration */

	sx1262Calibrate (p);

	/* Set channel */

	sx1262ChanSet (p);

	/* Apply workaround for LoRa I/Q optimization */

	if (p->sx_mode == SX_MODE_LORA) {
		reg = sx1262RegRead (p, SX_REG_IQPOL_SETUP);
		reg |= 0x04;
		sx1262RegWrite (p, SX_REG_IQPOL_SETUP, reg);
	}

	/* Apply workaround for LoRa 500KHz operation quality */

	reg = sx1262RegRead (p, SX_REG_TXMOD);
	if (p->sx_mode == SX_MODE_LORA &&
	    p->sx_lora.sx_bandwidth == SX_LORA_BW_500)
		reg &= 0xFB;
	else
		reg |= 0x04;
	sx1262RegWrite (p, SX_REG_TXMOD, reg);

	/* Apply workaround for better resistance to antenna mismatch */

	reg = sx1262RegRead (p, SX_REG_TXCLAMPCFG);
	reg |= 0x1E;
	sx1262RegWrite (p, SX_REG_TXCLAMPCFG, reg);

	/* Set RX gain to boosted */

	sx1262RegWrite (p, SX_REG_RXGAIN, SX_RXGAIN_BOOSTED);

	/* Perform RX and TX configuration */

	if (p->sx_mode == SX_MODE_LORA)
		sx1262LoraConfig (p);
	if (p->sx_mode == SX_MODE_GFSK)
		sx1262GfskConfig (p);

	p->sx_service = 0;
	p->sx_txdone = TRUE;
	p->sx_dispatch = TRUE;
	chVTReset (&p->sx_timer);

	/* Enable receiver */

	sx1262ReceiveSet (p);

	return;
}

void
sx1262Disable (SX1262_Driver * p)
{
	sx1262Reset (p);

	return;
}

static void
sx1262Send (SX1262_Driver * p, uint8_t * buf, uint8_t len)
{
	/* Set sync word */
	sx1262BufWrite (p, buf, 0, len);

#ifdef SX_MANUAL_CAD
	sx1262CadSet (p);
#else
	sx1262TransmitSet (p);
#endif

	return;
}

static void
sx1262TransmitSet (SX1262_Driver * p)
{
	SX_SETTX t;

	t.sx_opcode = SX_CMD_SETTX;
	t.sx_timeo0 = 0;
	t.sx_timeo1 = 0;
	t.sx_timeo2 = 0;

	sx1262CmdSend (p, &t, sizeof (t));

	return;
}

static void
sx1262ReceiveSet (SX1262_Driver * p)
{
	SX_SETRX r;

	r.sx_opcode = SX_CMD_SETRX;
	r.sx_timeo0 = 0;
	r.sx_timeo1 = 0;
	r.sx_timeo2 = 0;

	sx1262CmdSend (p, &r, sizeof (r));

	return;
}

static void
sx1262RxHandle (SX1262_Driver * p)
{
	SX_GETRXBUFSTS s;
	uint16_t len;
	struct pbuf * q;
	struct pbuf * pbuf;
	struct netif * netif;
	ip_addr_t * a;
	struct ip_hdr * i;

	s.sx_opcode = SX_CMD_GETRXBUFSTS;
	sx1262CmdExc (p, &s, sizeof(s));

	/* Sanity check */

	if (s.sx_rxpaylen == 0)
		return;

	sx1262BufRead (p, p->sx_rxbuf, s.sx_rxstbufptr, s.sx_rxpaylen);
	sx1262ReceiveSet (p);

	len = s.sx_rxpaylen;

	netif = p->sx_netif;

	/*
	 * If the packet isn't for us and it's not a broadcast, ignore it.
	 * This is our half-baked attempt at RX filtering, since the
	 * hardware doesn't know how to do it for us.
	 */

	i = (struct ip_hdr *)p->sx_rxbuf;
	a = &netif->ip_addr;

	if (ntohl(i->dest.addr) != ntohl(a->addr) &&
	    ntohl(i->dest.addr) != ntohl(inet_addr ("10.255.255.255"))) {
		return;
	}

	/* We allocate a pbuf chain of pbufs from the pool. */
	pbuf = pbuf_alloc (PBUF_RAW, len, PBUF_POOL);

	if (pbuf != NULL) {
		len = 0;
		for (q = pbuf; q != NULL; q = q->next) {
			memcpy (q->payload, p->sx_rxbuf + len, q->len);
			len += q->len;
		}

		MIB2_STATS_NETIF_ADD(netif, ifinoctets, pbuf->tot_len);

		if (ntohl(i->dest.addr) ==
		    ntohl(inet_addr ("10.255.255.255"))) {
			/* broadcast or multicast packet*/
			MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
		} else {
			/* unicast packet*/
			MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
		}
		netif->input (pbuf, netif);
	} else {
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		MIB2_STATS_NETIF_INC(netif, ifindiscards);
	}

	return;
}

static err_t
sx1262Output (struct netif * netif, struct pbuf * p, const ip4_addr_t * i)
{
	(void) i;
	return (sx1262LinkOutput (netif, p));
}

static err_t
sx1262LinkOutput (struct netif * netif, struct pbuf * p)
{
	SX1262_Driver * d;
	struct ip_hdr * ip;
	SX_ACKIRQ a;
	SX_GETIRQ i;
	uint16_t irq;
	uint32_t service;

	d = netif->state;

	/* There may be many sending threads */

	osalMutexLock (&d->sx_mutex);

        /* Get contiguous copy of data */

        pbuf_copy_partial (p, d->sx_rxbuf, p->tot_len, 0);

	d->sx_txdone = FALSE;

	sx1262StandbySet (d);

	/* Check for any RX events */

	i.sx_opcode = SX_CMD_GETIRQ;
	i.sx_irqsts = 0;
	sx1262CmdExc (d, &i, sizeof(i));
	irq = __builtin_bswap16 (i.sx_irqsts);

	if (irq & SX_IRQ_RXDONE)
		sx1262RxHandle (d);

	if (irq) {
		a.sx_opcode = SX_CMD_ACKIRQ;
		a.sx_irqsts = __builtin_bswap16 (irq);
		sx1262CmdExc (d, &a, sizeof(a));
	}

	/*
	 * It turns out there are two magic undocumented registers for
	 * changing the packet payload size. The first, SX_REG_PAYLEN,
	 * is for LoRa mode only and seems to only affect the transmit
	 * payload size. The other, SX_REG_RXTX_PAYLOAD_LEN, is for
	 * GFSK mode and as the name implies it affects both RX and
	 * TX, so we have to put it back to its original value before
	 * we turn the receiver back on.
	 *
	 * The SX_REG_PAYLEN register is used in the SemTeck LoraMac
	 * code, and SX_REG_RXTX_PAYLOAD_LEN is used in the sample
	 * code that goes with SemTech application node AN1200.53 (which
	 * shows how to exchange packets larger than 255 bytes with
	 * the SX126x in GFSK mode).
	 */
retry:

	if (d->sx_mode == SX_MODE_GFSK)
		sx1262RegWrite (d, SX_REG_RXTX_PAYLOAD_LEN, p->tot_len);
	else if (d->sx_mode == SX_MODE_LORA)
		sx1262RegWrite (d, SX_REG_PAYLEN, p->tot_len);

	osalSysLock ();
	d->sx_service = 0;
	d->sx_dispatch = FALSE;
	osalSysUnlock ();

	sx1262Send (d, d->sx_rxbuf, p->tot_len);

	osalSysLock ();

	/* Set a timeout in case the transmitter gets stuck. */

	if (d->sx_mode == SX_MODE_LORA)
		chVTSetI (&d->sx_timer, TIME_MS2I(100), sx1262Timeout, d);
	else
		chVTSetI (&d->sx_timer, TIME_MS2I(20), sx1262Timeout, d);

	/* Wait for TX to complete */

	if (d->sx_service == 0)
                osalThreadSuspendS (&d->sx_txwait);
	service = d->sx_service;
	d->sx_service = 0;
	d->sx_dispatch = TRUE;
	chVTResetI (&d->sx_timer);

	osalSysUnlock ();

	/* Acknowledge interrupt */

	if (service & 0x00000001) {
		i.sx_opcode = SX_CMD_GETIRQ;
		i.sx_irqsts = 0;
		sx1262CmdExc (d, &i, sizeof(i));
		irq = __builtin_bswap16 (i.sx_irqsts);
		a.sx_opcode = SX_CMD_ACKIRQ;
		a.sx_irqsts = __builtin_bswap16 (irq);
		sx1262CmdExc (d, &a, sizeof(a));
	}

	/* If the timeout expired, reset the radio. */

	if (service & 0x00000002) {
		sx1262Enable (d);
		goto retry;
	} else if (d->sx_mode == SX_MODE_GFSK)
		sx1262RegWrite (d, SX_REG_RXTX_PAYLOAD_LEN, SX_PKTLEN_DEFAULT);

	/* Turn receiver back on */

	sx1262ReceiveSet (d);

	osalMutexUnlock (&d->sx_mutex);

        MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);

	ip = (struct ip_hdr *)d->sx_rxbuf;

	if (ntohl(ip->dest.addr) == ntohl(inet_addr ("10.255.255.255"))) {
                /* broadcast or multicast packet*/
                MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
        } else {
                /* unicast packet */
                MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
        }
        LINK_STATS_INC(link.xmit);

	return (ERR_OK);
}

static err_t
sx1262EthernetInit (struct netif * netif)
{
	SX1262_Driver * p;

	p = netif->state;

	MIB2_INIT_NETIF(netif, snmp_ifType_other, 0);
	netif->name[0] = 's';
	netif->name[1] = 'x';
	netif->output = sx1262Output;
	netif->mtu = p->sx_pktlen;
	netif->flags = NETIF_FLAG_BROADCAST |
	    NETIF_FLAG_IGMP | NETIF_FLAG_LINK_UP;

	return (ERR_OK);
}

void
sx1262Start (SX1262_Driver * p)
{
	uint8_t ocp;
	ip_addr_t ip, gateway, netmask;

	/* Set the SPI interface */

	p->sx_spi = &SPID2;

	/* Set generic parameters */

	p->sx_freq = SX_FREQ_DEFAULT;
	p->sx_tx_power = SX_POWER_DEFAULT;
	p->sx_mode = SX_MODE_DEFAULT;
	p->sx_pktlen = SX_PKTLEN_DEFAULT;

	/* Set LoRa parameters */

	p->sx_lora.sx_syncword = SX_LORA_SYNC_PRIVATE;
	p->sx_lora.sx_symtimeout = 0;
	p->sx_lora.sx_preamlen = 8;
	p->sx_lora.sx_spreadfactor = SX_LORA_SF7;
	p->sx_lora.sx_bandwidth = SX_LORA_BW_500;
	p->sx_lora.sx_coderate = SX_LORA_CR_4_5;

	/* Set GFSK parameters */

	p->sx_gfsk.sx_bitrate = 500000;
	p->sx_gfsk.sx_deviation = 175000;
	p->sx_gfsk.sx_bandwidth = SX_RX_BW_467000;
	p->sx_gfsk.sx_preamlen = 8;
	p->sx_gfsk.sx_syncwordlen = 6;
	p->sx_gfsk.sx_syncword[0] = 0x90;
	p->sx_gfsk.sx_syncword[1] = 0x4e;
	p->sx_gfsk.sx_syncword[2] = 0xde;
	p->sx_gfsk.sx_syncword[3] = 0xad;
	p->sx_gfsk.sx_syncword[4] = 0xbe;
	p->sx_gfsk.sx_syncword[5] = 0xef;
	p->sx_gfsk.sx_syncword[6] = 0x00;
	p->sx_gfsk.sx_syncword[7] = 0x00;

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
	palSetLineMode (LINE_ARD_D5, PAL_STM32_PUPDR_PULLDOWN |
	    PAL_STM32_MODE_INPUT);
	palSetLineCallback (LINE_ARD_D5, sx1262Int, p);
	palEnableLineEvent (LINE_ARD_D5, PAL_EVENT_MODE_RISING_EDGE);

	p->sx_rxbuf = memalign (SX_ALIGNMENT, SX_MAX_PKT + SX_MAX_CMD);
	p->sx_cmdbuf = p->sx_rxbuf + SX_MAX_PKT;

	/* Reset the chip */

	sx1262Reset (p);

	/*
	 * Try to probe for the radio chip. We do this by
	 * reading the overcurrent protection register and
	 * checking that its reset default value makes sense.
	 */

	ocp = sx1262RegRead (p, SX_REG_OCPCFG);

	if (ocp == SX_OCP_1261_60MA)
		printf ("SemTech SX1262 radio enabled\n");
	else {
		free (p->sx_rxbuf);
		printf ("No radio detected\n");
		return;
	}

	/* Set up interrupt event thread */

	p->sx_thread = chThdCreateFromHeap (NULL, THD_WORKING_AREA_SIZE(512),
            "RadioEvent", LWIP_THREAD_PRIORITY, sx1262Thread, p);

	osalMutexObjectInit (&p->sx_mutex);

	/* Enable the radio */

	sx1262Enable (p);

	p->sx_netif = malloc (sizeof(struct netif));

	/* Add this interface to the TCP/IP stack */

	IP4_ADDR(&gateway, 10, 0, 0, 1);
	IP4_ADDR(&netmask, 255, 0, 0, 0);
	IP4_ADDR(&ip, 10, badge_addr[3], badge_addr[4], badge_addr[5]);

	netif_add (p->sx_netif, &ip, &netmask, &gateway,
	    p, sx1262EthernetInit, tcpip_input);
	netif_set_default (p->sx_netif);
	netif_set_up (p->sx_netif);

	return;
}

void
sx1262Stop (SX1262_Driver * p)
{
	sx1262Disable (p);
	netifapi_netif_remove (p->sx_netif);
	free (p->sx_netif);
	free (p->sx_rxbuf);

	return;
}

uint32_t
sx1262Random (SX1262_Driver * p)
{
	uint32_t r = 0;

	r = sx1262RegRead (p, SX_REG_RNG_0);
	r |= sx1262RegRead (p, SX_REG_RNG_1) << 8;
	r |= sx1262RegRead (p, SX_REG_RNG_2) << 16;
	r |= sx1262RegRead (p, SX_REG_RNG_3) << 24;

	return (r);
}
