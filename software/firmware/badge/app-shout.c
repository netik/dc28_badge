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
#include "fontlist.h"
#include "ides_gfx.h"
#include "userconfig.h"
#include "stm32sai_lld.h"

#include "crc32.h"

#include "badge_finder.h"

#include <lwip/opt.h>
#include <lwip/sys.h>
#include <lwip/timeouts.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static uint32_t shout_init (OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}

static void shout_start (OrchardAppContext *context)
{
	char * p;
	userconfig * c;
	OrchardUiContext * keyboardUiContext;
        context->instance->uicontext = NULL;

	c = configGet ();

	if (c->cfg_airplane == CONFIG_RADIO_ON) {
		p = malloc (FINDER_PAYLOAD);
		memset (p, 0, FINDER_PAYLOAD);
		keyboardUiContext = malloc (sizeof(OrchardUiContext));
		keyboardUiContext->itemlist =
		    (const char **)malloc(sizeof(char *) * 2);
		keyboardUiContext->itemlist[0] =
		    "Shout something,\npress ENTER to send.\n";
		keyboardUiContext->itemlist[1] = p;
		keyboardUiContext->total = FINDER_MAXPKT - 1;
          
		context->instance->ui = getUiByName ("keyboard");
		context->instance->uicontext = keyboardUiContext;
        
		context->instance->ui->start (context);
        }
        
	return;
}

static void shout_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	OrchardUiContext * keyboardUiContext;
	const OrchardUi * keyboardUi;
	userconfig * c;
	struct sockaddr_in sin;
	FINDER_SHOUT shout;
	int s;

	keyboardUi = context->instance->ui;
	keyboardUiContext = context->instance->uicontext;

	if (event->type == ugfxEvent)
		keyboardUi->event (context, event);

	if (event->type == appEvent && event->app.event == appTerminate)
		return;

	if (event->type == uiEvent) {
		if ((event->ui.code == uiComplete) &&
		    (event->ui.flags == uiOK)) {
			/* Terminate UI */
			/* Send the message */

			c = configGet ();

			memset (&sin, 0, sizeof(sin));
			memset (&shout, 0, sizeof(shout));

			sin.sin_addr.s_addr = inet_addr ("10.255.255.255");
			sin.sin_port = lwip_htons (FINDER_PORT);
			sin.sin_family = AF_INET;

			shout.finder_hdr.finder_type = FINDER_TYPE_SHOUT;
			strcpy (shout.finder_hdr.finder_name, c->cfg_name);
			strcpy ((char *)shout.finder_payload,
			    keyboardUiContext->itemlist[1]);
			shout.finder_salt = (uint32_t)random ();
			shout.finder_csum = crc32_le ((uint8_t *)&shout,
			    sizeof (FINDER_SHOUT) - sizeof(uint32_t), 0);

			s = lwip_socket (AF_INET, SOCK_DGRAM, 0);

			(void)lwip_sendto (s, &shout, sizeof(FINDER_SHOUT), 0,
			    (struct sockaddr *)&sin, sizeof (sin));

			lwip_close (s);

			/* Display a confirmation message */
                        screen_alert_draw (true, "SHOUT SENT");
                        i2sPlay ("sound/alert1.snd");

			/* Wait for a second, then exit */

			chThdSleepMilliseconds (1000);

			orchardAppExit ();
		}
	}
	return;
}

static void shout_exit (OrchardAppContext *context)
{
	OrchardUiContext * keyboardUiContext;
	const OrchardUi * keyboardUi;

	keyboardUi = context->instance->ui;
	keyboardUiContext = context->instance->uicontext;
  
	keyboardUi->exit (context);
	free ((void *)keyboardUiContext->itemlist[1]);

	if (context->instance->uicontext) {   
		free (context->instance->uicontext->itemlist);
		free (context->instance->uicontext);
	}

	return;
}

orchard_app("Radio Shout", "icons/danger.rgb", 0, shout_init,
    shout_start, shout_event, shout_exit, 9999);
