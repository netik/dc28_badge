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
#include "badge_console.h"
#include "badge_finder.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

extern int doom_main (int argc, char * argv[]);
extern char __ram7_base__; /* Set by linker */
extern char __ram7_end__; /* Set by linker */

char * net_doom_peer = NULL;
char * net_doom_node = NULL;
uint8_t net_doom_skill;
uint8_t net_doom_episode;
uint8_t net_doom_map;
bool net_doom_deathmatch;
uint32_t net_doom_freq;

static THD_FUNCTION(doomThread, arg)
{
	char * args[11];
	size_t bsslen;

	(void)arg;

	chRegSetThreadName ("DoomThread");

	/* Set the environment */

	setenv ("HOME", "/", TRUE);
	setenv ("DOOMWADDIR", "/doom", TRUE);

	/* Initialize Doom's .bss */

	bsslen = (uintptr_t)&__ram7_end__ - (uintptr_t)&__ram7_base__;
	memset (&__ram7_base__, 0, bsslen);

	badge_concreate (BADGE_CONSOLE_SHARE);

	/*
	 * If the network parameters are set, then launch a networked
	 * game, otherwise just launch a single player game.
	 */

	args[0] = "doom";
	if (net_doom_node != NULL) {
		/*
		 * Switch the radio to GFSK mode and change the
		 * channel before launching Doom. LoRa mode is too
		 * slow for the bandwidth Doom needs.
		 */

		badge_finder_radio_freq_set (net_doom_freq);
		badge_finder_radio_mode_set (FINDER_RADIO_MODE_FAST);

		args[1] = "-port";
		args[2] = "9999";
		args[3] = ""; /* "-dup" */
		args[4] = ""; /* "2" */
		args[5] = "" /*"-extratic"*/;
		args[6] = "-skill";
		args[7] = "3";
		args[8] = "-net";
		args[9] = net_doom_node;
		args[10] = net_doom_peer;
		doom_main (11, args);

		badge_finder_radio_restore ();
	} else
		doom_main (1, args);

	net_doom_node = NULL;

	chSysLock ();
	chThdExitS (MSG_OK);

	return;
}

static uint32_t
doom_init (OrchardAppContext * context)
{
	(void)context;
	return (0);
}

static void
doom_start (OrchardAppContext * context)
{
	(void)context;
	gdispClear (GFX_BLACK);
	orchardAppExit ();
	return;
}

static void
doom_event (OrchardAppContext * context, const OrchardAppEvent * event)
{
	thread_t * pThread;
	void * pWsp;

	(void) context;

	if (event->type == appEvent && event->app.event == appTerminate) {
		i2sWait ();

		/* Doom needs lots of stack space */

		pWsp = memalign (PORT_WORKING_AREA_ALIGN,
		    THD_WORKING_AREA_SIZE(48 * 1024));

		if (pWsp == NULL)
			printf ("Allocating Doom stack failed\n");
		else {
			pThread = chThdCreateStatic (pWsp,
			    THD_WORKING_AREA_SIZE(48 * 1024), ORCHARD_APP_PRIO,
                            doomThread, NULL);

			chThdWait (pThread);

			free (pWsp);
		}
	}

	return;
}

static void
doom_exit (OrchardAppContext * context)
{
	(void) context;
	return;
}

orchard_app("Doom", "icons/doom.rgb", 0, doom_init, doom_start,
    doom_event, doom_exit, 4);
