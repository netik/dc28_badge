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

#include "ch.h"
#include "hal.h"

#include "gfx.h"
#include "src/gwin/gwin_class.h"

#include "badge.h"
#include "badge_console.h"

#include <string.h>

static GHandle ghConsole;
static gFont font;
static uint8_t mode;

static msg_t badge_con_put (void *, uint8_t);

static const struct BaseSequentialStreamVMT * vmt1;
static struct BaseSequentialStreamVMT vmt2;

static msg_t
badge_con_put (void * instance, uint8_t b)
{
	(void)instance;

	gwinPutChar (ghConsole, (char)b);
	if (mode == BADGE_CONSOLE_SHARE)
		vmt1->put (conout, (char)b);

	return (MSG_OK);
}

void
badge_coninit (void) 
{
	return;
}

void
badge_concreate (uint8_t m)
{
	GWidgetInit wi;

	font = gdispOpenFont ("tom_thumb");
	gwinSetDefaultFont (font);

	gwinWidgetClearInit (&wi);
	wi.g.show = FALSE;
	wi.g.x = 0;
	wi.g.y = 0;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight();
	ghConsole = gwinConsoleCreate (0, &wi.g);
	gwinSetColor (ghConsole, GFX_WHITE);
	gwinSetBgColor (ghConsole, GFX_BLUE);
	gwinShow (ghConsole);
	gwinClear (ghConsole);

	mode = m;

	osalMutexLock (&conmutex);
	vmt1 = conout->vmt;
	memcpy (&vmt2, conout->vmt, sizeof(vmt2));
	vmt2.put = badge_con_put;
	conout->vmt = &vmt2;
	osalMutexUnlock (&conmutex);

	return;
}

void
badge_condestroy (void)
{
	gwinDestroy (ghConsole);
	gdispCloseFont (font);

	osalMutexLock (&conmutex);
	conout->vmt = vmt1;
	osalMutexUnlock (&conmutex);

	return;
}
