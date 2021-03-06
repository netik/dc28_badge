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
#include "ides_gfx.h"

#include "badge.h"

static GListener gl;

static uint32_t
default_init (OrchardAppContext *context)
{
	(void)context;
	return (0);
}

static void
default_start (OrchardAppContext *context)
{
	GSourceHandle gs;

	(void)context;
	gdispClear (GFX_BLACK);

	putImageFile ("icons/badge.rgb", 0, 0);

	gs = ginputGetMouse (0);
	geventListenerInit (&gl);
	geventAttachSource (&gl, gs, GLISTEN_MOUSEMETA);
	geventRegisterCallback (&gl, orchardAppUgfxCallback, &gl);

	badge_lpidle_enable();
	badge_cpu_speed_set (BADGE_CPU_SPEED_SLOW);

	return;
}

static void
default_event (OrchardAppContext *context, const OrchardAppEvent *event)
{
	GEventMouse * me;

	(void) context;

	/*
	 * Put the CPU into slow speed mode when we enter
	 * the idle screen, and put it back in fast mode
	 * when we exit.
	 */

	if (event->type == appEvent) {
		if (event->app.event == appTerminate) {
			i2sPlay ("sound/click.snd");
		}
	}

	if (event->type == ugfxEvent) {
		me = (GEventMouse *)event->ugfx.pEvent;
		if (me->buttons & GMETA_MOUSE_DOWN) {
			orchardAppExit ();
			return;
		}
	}

	return;
}

static void
default_exit(OrchardAppContext *context)
{
	(void) context;

	badge_cpu_speed_set (BADGE_CPU_SPEED_NORMAL);
	badge_lpidle_disable ();

	geventRegisterCallback (&gl, NULL, NULL);
	geventDetachSource (&gl, NULL);

	return;
}

orchard_app("Badge", "icons/badgeico.rgb", 0,
    default_init, default_start,
    default_event, default_exit, 0);
