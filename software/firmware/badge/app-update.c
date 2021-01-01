/*-
 * Copyright (c) 2016-2020
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
#include "fontlist.h"
#include "ides_gfx.h"

#include "nullprot_lld.h"
#include "sx1262_lld.h"

#include "buildtime.h"

#include "../updater/updater.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct _VHandles {
	GListener glCtListener;
	GHandle ghOK;
	GHandle ghCANCEL;
	GHandle ghConsole;
	gFont font;
} VHandles;

typedef struct _buildinfo {
	uint64_t	build_magic;
	uint64_t	build_ver;
} buildinfo;

__attribute__((section(".fwversion")))
static volatile const buildinfo firmware_version = {
	BUILDMAGIC, BUILDVER
};

typedef uint32_t (*pFwUpdate) (void);

static void
gtimerCallback (void * arg)
{
	(void)arg;

	chThdExitS (MSG_OK);

	return;
}

static void
update (VHandles * p)
{
	int f;
	GTimer * t;
	uint32_t * entrypoint = (uint32_t *)(UPDATER_BASE + 4);
	pFwUpdate fwupdatefunc;

	/* Stop the radio */

	sx1262Stop (&SX1262D1);

	/* Stop the gtimer thread */

	chThdSleepMilliseconds (20);

	t = malloc (sizeof(GTimer));
	gtimerInit (t);
	gtimerStart (t, gtimerCallback, NULL, FALSE, 1);

	/* Wait for the LED and gtimer threads to exit */

	chThdSleepMilliseconds (20);

	/*
	 * Turn off the NULL pointer protection. The updater code will
	 * access the vector table to make a copy of it, so we have to
	 * make it readable.
	 */

	nullProtStop ();

	gwinClear (p->ghConsole);
	gwinPrintf (p->ghConsole,
	    "\n\n"
	    " FIRMWARE UPDATE IN PROGRESS\n"
	    " DO NOT POWER OFF THE BADGE\n"
	    " UNTIL IT COMPLETES!\n\n"
	    " THE BADGE WILL REBOOT WHEN\n"
	    " THE UPDATE IS FINISHED");

	chThdSleepMilliseconds (2000);
	f = open (UPDATER_NAME, O_RDONLY, 0);
	(void)read (f, (char *)UPDATER_BASE, UPDATER_SIZE);
	close (f);

	fwupdatefunc = (pFwUpdate)(entrypoint[0]);

	/* Make sure the above operations complete. */

	__ISB();

	/* Turn off the MPU */

	mpuDisable ();

	/* Flush the cache */

	__disable_irq ();

	SCB_CleanInvalidateDCache ();

	/*
	 * If we reached this point, we've loaded an updater image into
	 * RAM and we're about to run it. Once it starts, it will
	 * completely take over the system, and either the firmware
	 * will be updated or the system will hang. Fingers crossed.
	 */

	fwupdatefunc ();

	/* NOTREACHED */

	return;
}

static uint32_t
update_init(OrchardAppContext *context)
{
	(void)context;

	return (0);
}

static void
update_start(OrchardAppContext *context)
{
	int f;
	GHandle ghConsole;
	GWidgetInit wi;
	gFont font;
	struct stat finfo;
	buildinfo buildinfo;
	VHandles * p;

	font = gdispOpenFont (FONT_FIXED);
	gwinSetDefaultFont (font);

	wi.g.show = FALSE;
	wi.g.x = 0;
	wi.g.y = 0;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight();
	ghConsole = gwinConsoleCreate (0, &wi.g);
	gwinSetColor (ghConsole, GFX_YELLOW);
	gwinSetBgColor (ghConsole, GFX_BLUE);
	gwinShow (ghConsole);
	gwinClear (ghConsole);

	if (stat (UPDATER_NAME, &finfo) != 0) {
		gwinPrintf (ghConsole,
		    "\n\n\n"
		    "    UPDATER.BIN image was not\n"
		    "    found on the SD card!");
		goto done;
	}

	if (stat ("BADGE.BIN", &finfo) != 0) {
		gwinPrintf (ghConsole,
		    "\n\n\n"
		    "    BADGE.BIN image was not\n"
		    "    found on the SD card!");
		goto done;
	}

	/*
	 * Check that the firmware version in the file is newer than
	 * the current version. The firmware_version structure is
	 * stored at a fixed location within the image using some
	 * link script magic. The offset currently is 0x100. In order
	 * to avoid hard-coding this value, we use the address of
	 * the firmware_version in the current firmware as the file
	 * offset where we need to search for the corresponding
	 * structure in the new firmware.
	 */

	f = open ("BADGE.BIN", O_RDONLY, 0);
	(void)lseek (f, (uint32_t)&firmware_version & 0xFFFFF, SEEK_SET);
	(void)read (f, (char *)&buildinfo, sizeof (buildinfo));
	close (f);

	if (buildinfo.build_magic != BUILDMAGIC) {
		gwinPrintf (ghConsole,
		    "\n\n\n"
		    "    Invalid firmware image!\n"
		    "    Firmware magic: %x\n"
		    "    Expected magic: %x",
		    (uint32_t)buildinfo.build_magic,
		    (uint32_t)BUILDMAGIC);
		goto done;
	}

	if (buildinfo.build_ver <= firmware_version.build_ver) {
		gwinPrintf (ghConsole,
		    "\n\n\n"
		    "    Firmware on the SD card\n"
		    "    is older or same as the\n"
		    "    currently running\n"
		    "    version!");
		goto done;
	}

	p = malloc (sizeof(VHandles));
	memset (p, 0, sizeof(VHandles));
	context->priv = p;

	p->font = font;
	p->ghConsole = ghConsole;

 	gdispDrawStringBox (0, 40, 320, 20, "UPDATE FIRMWARE",
	    font, GFX_WHITE, gJustifyCenter);

 	gdispDrawStringBox (0, 100, 320, 20, "ARE YOU SURE",
	    font, GFX_RED, gJustifyCenter);

	gwinSetDefaultStyle (&RedButtonStyle, FALSE);
	gwinWidgetClearInit (&wi);
	wi.g.show = TRUE;
	wi.g.x = 0;
	wi.g.y = 210;
	wi.g.width = 150;
	wi.g.height = 30;
	wi.text = "CANCEL";
	p->ghCANCEL = gwinButtonCreate(0, &wi);
  
	wi.g.x = 170;
	wi.text = "CONFIRM";
	p->ghOK = gwinButtonCreate(0, &wi);

	gwinSetDefaultStyle (&BlackWidgetStyle, FALSE);

	geventListenerInit (&p->glCtListener);
	gwinAttachListener (&p->glCtListener);
	geventRegisterCallback (&p->glCtListener, orchardAppUgfxCallback,
	    &p->glCtListener);

	return;

done:

	chThdSleepMilliseconds (3000);

	gwinDestroy (ghConsole);
	gdispCloseFont (font);
	orchardAppExit ();

	return;
}

static void
update_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	VHandles * p;
	GEvent * pe;
	GEventGWinButton * be;

	p = context->priv;

	if (event->type == ugfxEvent) {
		pe = event->ugfx.pEvent;
		if (pe->type == GEVENT_GWIN_BUTTON) {
			be = (GEventGWinButton *)pe;
			if (be->gwin == p->ghOK)
				update (p);
			if (be->gwin == p->ghCANCEL)
				orchardAppExit ();
		}
	}

	return;
}

static
void update_exit(OrchardAppContext *context)
{
	VHandles * p;

	p = context->priv;

	if (p == NULL)
		return;

	gdispCloseFont (p->font);
	gwinDestroy (p->ghOK);
	gwinDestroy (p->ghCANCEL);
	gwinDestroy (p->ghConsole);

	geventDetachSource (&p->glCtListener, NULL);
	geventRegisterCallback (&p->glCtListener, NULL, NULL);
   
	free (p);
	context->priv = NULL;

	return;
}

orchard_app("Update FW", "icons/rov.rgb", 0,
            update_init, update_start, update_event, update_exit, 9999);
