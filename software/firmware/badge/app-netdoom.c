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

#include "orchard-app.h"
#include "orchard-ui.h"
#include "stm32sai_lld.h"
#include "badge_finder.h"
#include "sx1262_lld.h"
#include "ides_gfx.h"
#include "crc32.h"
#include "badge.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <lwip/opt.h>
#include <lwip/sys.h>
#include <lwip/timeouts.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>

typedef struct _DoomHandles {
	char *			peernames[FINDER_MAXPEERS + 2];
	struct in_addr		peeraddrs[FINDER_MAXPEERS + 2];
	OrchardUiContext	uiCtx;
	int			peers;
	int			timeout;
	GListener		gl;
} DoomHandles;

static void
netdoom_create_uilist (OrchardAppContext * context)
{
	int i;
	FINDER_PEER * peer;
	DoomHandles * p;

	p = context->priv;

	osalMutexLock (&badge_peer_mutex);

	p->peers = 2;

	for (i = 0; i < FINDER_MAXPEERS; i++) {
		peer = &badge_peers[i];
		if (peer->finder_used == 0)
			continue;
		p->peernames[p->peers] =
		    strdup (peer->finder_msg.finder_hdr.finder_name);
		memcpy (&p->peeraddrs[p->peers], &peer->finder_addr,
		    sizeof (struct in_addr));
		p->peers++;
	}

	osalMutexUnlock (&badge_peer_mutex);

	p->peernames[0] = "Choose a partner...";
	p->peernames[1] = "Exit";
	p->uiCtx.total = p->peers - 1;
	p->uiCtx.itemlist = (const char **)p->peernames;

	context->instance->ui = getUiByName ("list");
	context->instance->uicontext = &p->uiCtx;
	context->instance->ui->start (context);

	return;
}

static void
netdoom_send_request (OrchardAppContext * context)
{
	DoomHandles * p;
	OrchardUiContext * uiContext;
	userconfig * c;
	struct sockaddr_in sin;
	FINDER_DOOM doom;
	int s;
	int peer;

	p = context->priv;
	uiContext = context->instance->uicontext;
	peer = uiContext->selected + 1;

	memset (&sin, 0, sizeof(sin));
	memset (&doom, 0, sizeof(doom));

	memcpy (&sin.sin_addr, &p->peeraddrs[peer], sizeof(struct in_addr));
	sin.sin_port = lwip_htons (FINDER_PORT);
	sin.sin_family = AF_INET;

	c = configGet ();

	doom.finder_hdr.finder_type = FINDER_TYPE_DOOM;
	strcpy (doom.finder_hdr.finder_name, c->cfg_name);
	net_doom_freq = doom.finder_freq = badge_finder_radio_chan_alloc ();
	net_doom_skill = doom.finder_doom_skill = 1;
	net_doom_episode = doom.finder_doom_episode = 1;
	net_doom_map = doom.finder_doom_map = 1;
	net_doom_deathmatch = doom.finder_doom_deathmatch = 0;
	doom.finder_salt = (uint32_t)random ();
	doom.finder_csum = crc32_le ((uint8_t *)&doom,
	    sizeof (FINDER_DOOM) - sizeof(uint32_t), 0);

	s = lwip_socket (AF_INET, SOCK_DGRAM, 0);
	(void)lwip_sendto (s, &doom, sizeof(FINDER_DOOM), 0,
	    (struct sockaddr *)&sin, sizeof (sin));
	lwip_close (s);

	return;
}

static uint32_t
netdoom_init (OrchardAppContext * context)
{
	(void)context;
	return (0);
}

static void
netdoom_start (OrchardAppContext * context)
{
	DoomHandles * p;

	gdispClear (GFX_BLACK);

	p = malloc (sizeof(DoomHandles));

	context->priv = p;

	/* Add some ambiance. */

	i2sWait ();
	i2sPlay ("sound/doome1m1.snd");
	i2sIgnore = TRUE;

	netdoom_create_uilist (context);

	return;
}

static void
netdoom_event (OrchardAppContext * context, const OrchardAppEvent * event)
{
	DoomHandles * p;
	const OrchardUi * ui;
	OrchardUiContext * uiContext;
	FINDER_ACTION * pAct;
	GSourceHandle gs;
	GEventMouse * me;
	char buf[40];

	p = context->priv;
	ui = context->instance->ui;
	uiContext = context->instance->uicontext;

	if (event->type == appEvent) {

	}

        if (event->type == ugfxEvent) {
        }

	if (event->type == ugfxEvent || event->type == keyEvent) {

		me = (GEventMouse *)event->ugfx.pEvent;
		if (me != NULL && me->buttons & GMETA_MOUSE_DOWN) {
			i2sIgnore = FALSE;
			i2sPlay ("sound/click.snd");
			orchardAppExit ();
			return;
		}

		if (ui != NULL)
			ui->event (context, event);
	}

	if (event->type == uiEvent &&
	    event->ui.code == uiComplete &&
	    event->ui.flags == uiOK) {

		if (ui != NULL) {
			ui->exit (context);

			/* User decided to just exit; bail out */
			if (uiContext->selected == 0) {
				i2sIgnore = FALSE;
				i2sPlay ("sound/click.snd");
				orchardAppExit ();
				return;
			}

			gs = ginputGetMouse (0);
			geventListenerInit (&p->gl);
			geventAttachSource (&p->gl, gs, GLISTEN_MOUSEMETA);
			geventRegisterCallback (&p->gl,
			    orchardAppUgfxCallback, &p->gl);

			putImageFile ("images/doom.rgb", 0, 0);
			netdoom_send_request (context);

			context->instance->ui = NULL;

			screen_alert_draw (false, "WAITING FOR RESPONSE: 10");

			/* Start a timeout timer in case we get no reply. */

			p->timeout = 10;
			orchardAppTimer (context, 1000000, false);
		}
		
	}

	if (event->type == radioEvent && event->radio.type == actionEvent) {
		pAct = (FINDER_ACTION *)event->radio.pkt;
		if (pAct->finder_action == FINDER_ACTION_ACCEPT) {
			int peer;

			peer = uiContext->selected + 1;
			net_doom_peer = inet_ntoa (p->peeraddrs[peer]);
			net_doom_node = "1";

			orchardAppRun (orchardAppByName ("Doom"));
		} else {
			screen_alert_draw (false, "REQUEST WAS DECLINED");
			i2sIgnore = FALSE;
			i2sPlay ("sound/alert1.snd");
			i2sWait ();
			orchardAppExit ();
		}
	}

	if (event->type == timerEvent) {
		if (p->timeout > 0) {
			p->timeout--;
			sprintf (buf, "WAITING FOR RESPONSE: %d\n",
			    p->timeout);
			screen_alert_draw (false, buf);
			orchardAppTimer (context, 1000000, false);
		} else {
			screen_alert_draw (false, "REQUEST TIMED OUT");
			i2sIgnore = FALSE;
			i2sPlay ("sound/wilhelm.snd");
			i2sWait ();
			orchardAppExit ();
		}
	}

	return;
}

static void
netdoom_exit (OrchardAppContext * context)
{
	DoomHandles * p;
	int i;

	p = context->priv;

	geventRegisterCallback (&p->gl, NULL, NULL);
	geventDetachSource (&p->gl, NULL);

	for (i = 2; i < p->peers; i++) {
		free (p->peernames[i]);
		p->peernames[i] = NULL;
	}

	free (context->priv);
	context->priv = NULL;

	/* Stop music if it's still going */

	i2sIgnore = FALSE;
	i2sPlay (NULL);

	return;
}

orchard_app("Net Doom", "icons/doom.rgb", 0, netdoom_init, netdoom_start,
    netdoom_event, netdoom_exit, 4);
