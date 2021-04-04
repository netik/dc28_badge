/*-
 * Copyright (c) 2016, 2019
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

#include <stdio.h>
#include <string.h>

#include "orchard-app.h"
#include "orchard-ui.h"

#include "fontlist.h"
#include "ides_gfx.h"

#include "badge_finder.h"
#include "stm32sai_lld.h"

static OrchardAppRadioEvent radio_evt;
static char buf[128];

typedef struct _NHandles {
	GListener gl;
	GHandle	ghACCEPT;
	GHandle	ghDECLINE;
	gFont	font;
	char * app;
	uint8_t response_accept;
	uint8_t response_decline;
	uint8_t handle;
} NotifyHandles;

static bool
notify_handler (void * arg)
{
	OrchardAppRadioEvent * evt;

	evt = arg;

	if (evt->type != chatEvent &&
	    evt->type != challengeEvent &&
	    evt->type != doomEvent &&
	    evt->type != shoutEvent &&
	    evt->type != fwEvent)
		return (FALSE);

	memcpy (&radio_evt, evt, sizeof (radio_evt));

	orchardAppRun (orchardAppByName ("Radio notification"));

	return (TRUE);
}

static uint32_t
notify_init (OrchardAppContext *context)
{
	(void)context;

	/* This should only happen for auto-init */

	if (context == NULL)
		app_radio_notify = notify_handler;

	return (0);
}

static void
notify_start (OrchardAppContext *context)
{
	NotifyHandles * p;
	GWidgetInit wi;
	int fHeight;
	FINDER_HDR * pHdr;
	FINDER_SHOUT * pShout;

	p = malloc (sizeof (NotifyHandles));

	if (p == NULL) {
		orchardAppExit ();
		return;
	}

	context->priv = p;

	putImageFile ("images/undrattk.rgb", 0, 0);
	i2sPlay ("sound/klaxon.snd");

	/* Find peer addr or name */

	pHdr = (FINDER_HDR *)radio_evt.pkt;

	sprintf (buf, "Badge %s", pHdr->finder_name);

	p->font = gdispOpenFont (FONT_SM);

	fHeight = gdispGetFontMetric (p->font, gFontHeight);

	gdispDrawStringBox (0, 50 -
	    fHeight,
	    gdispGetWidth(), fHeight,
	    buf, p->font, GFX_WHITE, gJustifyCenter);

	if (radio_evt.type == chatEvent) {
		gdispDrawStringBox (0, 50, gdispGetWidth(), fHeight,
		    "WANTS TO CHAT", p->font, GFX_WHITE, gJustifyCenter);
		p->handle = 0;
		p->app = "Radio Chat";
		p->response_accept = 0;
		p->response_decline = 0;
	}

	if (radio_evt.type == challengeEvent) {
		gdispDrawStringBox (0, 50, gdispGetWidth(), fHeight,
		    "IS CHALLENGING YOU", p->font, GFX_WHITE, gJustifyCenter);
		p->handle = 1;
		p->app = "Sea Battle";
		p->response_accept = 111;
		p->response_decline = 111;
	}

	if (radio_evt.type == doomEvent) {
		gdispDrawStringBox (0, 50, gdispGetWidth(), fHeight,
		    "WANTS TO PLAY DOOM YOU", p->font, GFX_WHITE,
		    gJustifyCenter);
		p->handle = 1;
		p->app = "Doom";
		p->response_accept = 111;
		p->response_decline = 111;
	}

	if (radio_evt.type == fwEvent) {
		gdispDrawStringBox (0, 50, gdispGetWidth(), fHeight,
		    "IS OFFERING YOU A", p->font, GFX_WHITE, gJustifyCenter);
		gdispDrawStringBox (0, 50 + fHeight, gdispGetWidth(),
		    fHeight, "FIRMWARE UPDATE", p->font, GFX_WHITE,
		    gJustifyCenter);
		p->handle = 2;
		p->app = "OTA Recv";
		p->response_accept = 222;
		p->response_decline = 222;
	}

	if (radio_evt.type == shoutEvent) {
		pShout = (FINDER_SHOUT *)radio_evt.pkt;
		gdispDrawStringBox (0, 50, gdispGetWidth(), fHeight,
		    "Broadcast message:", p->font, GFX_WHITE, gJustifyCenter);
		gdispDrawStringBox (0, 50 + fHeight, gdispGetWidth(), fHeight,
		    (char *)pShout->finder_payload, p->font, GFX_WHITE,
		    gJustifyCenter);
		gwinSetDefaultStyle (&RedButtonStyle, FALSE);
		gwinSetDefaultFont (p->font);
		gwinWidgetClearInit (&wi);
		wi.g.show = TRUE;
		wi.g.x = 85;
		wi.g.y = 210;
		wi.g.width = 150;
		wi.g.height = 30;
		wi.text = "DISMISS";
		p->ghDECLINE = gwinButtonCreate(0, &wi);
		p->ghACCEPT = NULL;
	} else {
		gwinSetDefaultStyle (&RedButtonStyle, FALSE);
		gwinSetDefaultFont (p->font);
		gwinWidgetClearInit (&wi);
		wi.g.show = TRUE;
		wi.g.x = 0;
		wi.g.y = 210;
		wi.g.width = 150;
		wi.g.height = 30;
		wi.text = "DECLINE";
		p->ghDECLINE = gwinButtonCreate(0, &wi);

		wi.g.x = 170;
		wi.text = "ACCEPT";
		p->ghACCEPT = gwinButtonCreate(0, &wi);
	}

	gwinSetDefaultStyle (&BlackWidgetStyle, FALSE);

	geventListenerInit (&p->gl);
	gwinAttachListener (&p->gl);
	geventRegisterCallback (&p->gl, orchardAppUgfxCallback, &p->gl);

	return;
}

static void
notify_event (OrchardAppContext *context, const OrchardAppEvent *event)
{
	GEvent * pe;
	GEventGWinButton * be;
	NotifyHandles * p;

	p = context->priv;

	if (event->type == ugfxEvent) {
		pe = event->ugfx.pEvent;
		if (pe->type == GEVENT_GWIN_BUTTON) {
			be = (GEventGWinButton *)pe;
			if (be->gwin == p->ghACCEPT) {
				orchardAppRun (orchardAppByName(p->app));
			}
			if (be->gwin == p->ghDECLINE) {
				orchardAppExit ();
			}
		}
	}

	return;
}

static void
notify_exit (OrchardAppContext *context)
{
	NotifyHandles * p;

	p = context->priv;
	context->priv = NULL;

	gdispCloseFont (p->font);
	if (p->ghACCEPT != NULL)
		gwinDestroy (p->ghACCEPT);
	if (p->ghDECLINE != NULL)
		gwinDestroy (p->ghDECLINE);

	geventDetachSource (&p->gl, NULL);
	geventRegisterCallback (&p->gl, NULL, NULL);

	free (p);

	return;
}

orchard_app("Radio notification", NULL, APP_FLAG_HIDDEN|APP_FLAG_AUTOINIT,
	notify_init, notify_start, notify_event, notify_exit, 9999);
