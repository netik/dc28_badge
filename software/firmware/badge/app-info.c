/*-
 * Copyright (c) 2019
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

#include "orchard-app.h"
#include "orchard-ui.h"

#include "buildtime.h"

#include "ff.h"
#include "ffconf.h"
#include "diskio.h"

#include "stm32sai_lld.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

extern uint32_t __flash0_start__;
extern uint32_t __ram7_init_text__;

#pragma pack(1)
typedef struct _sd_cid {
	uint8_t		sd_mid;		/* Manufacturer ID */
	char		sd_oid[2];	/* OEM/Application ID */
	char		sd_pnm[5];	/* Product name */
	uint8_t		sd_prv;		/* Product revision */
	uint32_t	sd_psn;		/* Product serial number */
	uint16_t	sd_mdt;		/* Manufacture data */
	uint8_t		sd_crc;		/* 8-bit CRC */
} sd_cid;
#pragma pack()

typedef struct _ihandles {
	GListener	gl;
	GHandle		ghConsole;
} IHandles;

static uint32_t
info_init (OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}

static void
info_start (OrchardAppContext *context)
{
	GSourceHandle gs;
        GWidgetInit wi;
	IHandles * p;
	struct mallinfo m;
	uint64_t disksz;
	DWORD sectors;
	BYTE drv;
	sd_cid cid;
	uint32_t cid_words[4];

	/*
	 * Play a jaunty tune to keep the user entertained.
	 * (ARE YOU NOT ENTERTAINED?!)
	 */

	i2sWait ();
	i2sPlay ("sound/cantina.snd");

	p = malloc (sizeof(IHandles));

	context->priv = p;

	gs = ginputGetMouse (0);
	geventListenerInit (&p->gl);
	geventAttachSource (&p->gl, gs, GLISTEN_MOUSEMETA);
	geventRegisterCallback (&p->gl, orchardAppUgfxCallback, &p->gl);

	/* Create a console widget */

	gwinWidgetClearInit (&wi);
	wi.g.show = FALSE;
	wi.g.x = 0;
	wi.g.y = 0;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight();
	p->ghConsole = gwinConsoleCreate (0, &wi.g);
	gwinSetColor (p->ghConsole, GFX_WHITE);
	gwinSetBgColor (p->ghConsole, GFX_BLUE);
	gwinShow (p->ghConsole);
	gwinClear (p->ghConsole);

	/* Now display interesting info. */

	/* SoC info */

	/* TBD */

	/* CPU core info */
 
	gwinPrintf (p->ghConsole, "CPU Core: ARM (0x%x) Cortex-M7 (0x%x) "
	    "version r%dp%d\n",
	    (SCB->CPUID & SCB_CPUID_IMPLEMENTER_Msk) >>
	      SCB_CPUID_IMPLEMENTER_Pos,
	    (SCB->CPUID & SCB_CPUID_PARTNO_Msk) >>
	      SCB_CPUID_PARTNO_Pos,
	    (SCB->CPUID & SCB_CPUID_VARIANT_Msk) >>
	      SCB_CPUID_VARIANT_Pos,
	    (SCB->CPUID & SCB_CPUID_REVISION_Msk) >>
	      SCB_CPUID_REVISION_Pos);
	gwinPrintf (p->ghConsole, "Debugger: %s\n",
	    CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk ?
	    "connected" : "disconnected");

	/* Firmware info */

	gwinPrintf (p->ghConsole, "ChibiOS kernel version: %s\n",
	    CH_KERNEL_VERSION);
	gwinPrintf (p->ghConsole, "Firmware image size: %d bytes\n",
	    (uint32_t)&__ram7_init_text__ - (uint32_t)&__flash0_start__);
	gwinPrintf (p->ghConsole, "%s\n", BUILDTIME);
	gwinPrintf (p->ghConsole, "Built with: %s\n", PORT_COMPILER_NAME);

	/* Memory info */

	m = mallinfo ();
	gwinPrintf (p->ghConsole, "Heap size: %d bytes  ",
	    HEAP_END - HEAP_BASE);
	gwinPrintf (p->ghConsole, "Arena size: %d bytes\n", m.arena);
	gwinPrintf (p->ghConsole, "Arena in use: %d bytes  ", m.uordblks);
	gwinPrintf (p->ghConsole, "Arena free: %d bytes\n", m.fordblks);

	/* Storage info */

	drv = 0;
	gwinPrintf (p->ghConsole, "SD card: ");
	if (disk_ioctl (drv, GET_SECTOR_COUNT, &sectors) == RES_OK) {
		disksz = sectors * 512ULL;
		disksz /= 1048576ULL;
		gwinPrintf (p->ghConsole, "%d MB ", disksz);

		cid_words[0] = __builtin_bswap32 (SDCD1.cid[3]);
		cid_words[1] = __builtin_bswap32 (SDCD1.cid[2]);
		cid_words[2] = __builtin_bswap32 (SDCD1.cid[1]);
		cid_words[3] = __builtin_bswap32 (SDCD1.cid[0]);

		memcpy (&cid, &cid_words, sizeof(cid_words));

		gwinPrintf (p->ghConsole, "[%c%c] ", cid.sd_oid[0],
		    cid.sd_oid[1]);
		gwinPrintf (p->ghConsole, "[%c%c%c%c%c] ", cid.sd_pnm[0],
		    cid.sd_pnm[1], cid.sd_pnm[2], cid.sd_pnm[3],
		    cid.sd_pnm[4]);
		gwinPrintf (p->ghConsole, "SN: %lx\n", cid.sd_psn);
	} else
		gwinPrintf (p->ghConsole, "not found\n");

	return;
}

static void
info_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	GEventMouse * me;

	(void)event;
	(void)context;

#ifdef notdef
	if (event->type == keyEvent && event->key.flags == keyPress) {
		if (event->key.code == keyASelect ||
		    event->key.code == keyBSelect) {
			i2sPlay ("sound/click.snd");
			orchardAppExit ();
			return;
		}
	}
#endif

	if (event->type == ugfxEvent) {
		me = (GEventMouse *)event->ugfx.pEvent;
		if (me->buttons & GMETA_MOUSE_DOWN) {
			i2sPlay ("sound/click.snd");
			orchardAppExit ();
			return;
		}
	}

	return;
}

static void
info_exit (OrchardAppContext *context)
{
	IHandles * p;

	p = context->priv;

	geventRegisterCallback (&p->gl, NULL, NULL);
	geventDetachSource (&p->gl, NULL);

	gwinDestroy (p->ghConsole);

	free (p);

	return;
}

orchard_app("Badge Info", "icons/search.rgb",
    0, info_init, info_start, info_event, info_exit, 9999);
