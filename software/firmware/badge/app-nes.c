/*-
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

#include "orchard-app.h"
#include "orchard-ui.h"

#include "stm32sai_lld.h"

#include "ff.h"
#include "ffconf.h"

#include <string.h>
#include <stdlib.h>
#include <malloc.h>

extern int nes_main (int argc, char * argv[]);

typedef struct _NesHandles {
	char **			listitems;
	int			itemcnt;
	OrchardUiContext	uiCtx;
} NesHandles;

static char * nesdir;

static THD_FUNCTION(nesThread, arg)
{
	chRegSetThreadName ("NES Thread");
	nes_main (2, arg); 
 
	chSysLock ();
	chThdExitS (MSG_OK);

        return;
}

static uint32_t
nes_init(OrchardAppContext *context)
{
	(void)context;
	nesdir = "/nes";
	return (0);
}

static void
nes_start (OrchardAppContext *context)
{
	NesHandles * p;
	DIR d;
	FILINFO info;
	int i;

	f_opendir (&d, nesdir);

	i = 0;

	while (1) {
		if (f_readdir (&d, &info) != FR_OK || info.fname[0] == 0)
			break;
		if (strstr (info.fname, ".NES") != NULL)
			i++;
	}

	f_closedir (&d);

	if (i == 0) {
		orchardAppExit ();
		return;
	}

	i += 2;

	p = malloc (sizeof(NesHandles));
	memset (p, 0, sizeof(NesHandles));
	p->listitems = malloc (sizeof(char *) * i);
	p->itemcnt = i;

	p->listitems[0] = "Choose a game";
	p->listitems[1] = "Exit";

	f_opendir (&d, nesdir);

	i = 2;

	while (1) {
		if (f_readdir (&d, &info) != FR_OK || info.fname[0] == 0)
			break;
		if (strstr (info.fname, ".NES") != NULL) {
			p->listitems[i] =
			    malloc (strlen (info.fname) + 1);
			memset (p->listitems[i], 0, strlen (info.fname) + 1);
			strcpy (p->listitems[i], info.fname);
			i++;
		}
	}

	f_closedir (&d);

	p->uiCtx.itemlist = (const char **)p->listitems;
	p->uiCtx.total = p->itemcnt - 1;

	context->instance->ui = getUiByName ("list");
	context->instance->uicontext = &p->uiCtx;
	context->instance->ui->start (context);

	context->priv = p;
	return;
}

static void
nes_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	OrchardUiContext * uiContext;
	const OrchardUi * ui;
	NesHandles * p;
	char nesfn[35];
	char * args[2];
	thread_t * pThread;
	void * pWsp;

	p = context->priv;
	ui = context->instance->ui;
	uiContext = context->instance->uicontext;

	if (event->type == appEvent && event->app.event == appTerminate) {
		if (ui != NULL)
 			ui->exit (context);
	}

	if (event->type == ugfxEvent || event->type == keyEvent)
		if (ui != NULL)
			ui->event (context, event);

	if (event->type == uiEvent && event->ui.code == uiComplete &&
	    event->ui.flags == uiOK && ui != NULL) {
		/*
		 * If this is the list ui exiting, it means we chose a
		 * video to play, or else the user selected exit.
		 */
		ui->exit (context);
		context->instance->ui = NULL;

		/* User chose the "EXIT" selection, bail out. */
		if (uiContext->selected == 0) {
			orchardAppExit ();
			return;
		}

		strcpy (nesfn, nesdir);
		strcat (nesfn, "/");
		strcat (nesfn, p->listitems[uiContext->selected +1]);

		args[0] = "nofrendo";
		args[1] = nesfn;

		pWsp = memalign (PORT_WORKING_AREA_ALIGN,
		    THD_WORKING_AREA_SIZE(8 * 1024));

		if (pWsp == NULL)
			printf ("Allocating NES stack failed!\n");
		else {
			pThread = chThdCreateStatic (pWsp,
			    THD_WORKING_AREA_SIZE(8 * 1024), ORCHARD_APP_PRIO,
			    nesThread, args);

			chThdWait (pThread);

			free (pWsp);
		}
		
		orchardAppExit ();
	}

	return;
}

static void
nes_exit (OrchardAppContext *context)
{
	NesHandles * p;
	int i;

	p = context->priv;

	if (p == NULL)
            return;

	for (i = 2; i < p->itemcnt; i++)
		free (p->listitems[i]);

	free (p->listitems);
	free (p);

	context->priv = NULL;

	return;
}

orchard_app("NES Emulator", "icons/nes.rgb", 0, nes_init, nes_start,
    nes_event, nes_exit, 4);
