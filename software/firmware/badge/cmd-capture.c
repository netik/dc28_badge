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

#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "badge.h"
#include "badge_console.h"
#include "capture.h"

#include "orchard-app.h"
#include "orchard-events.h"

extern event_source_t orchard_app_key;

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static const struct BaseSequentialStreamVMT * vmt1;
static struct BaseSequentialStreamVMT vmt2;

#define KEY_DOWN	1
#define KEY_UP		2
#define INBUF_LEN	32

#define CAPTURE_CHAR_UP		'Y'
#define CAPTURE_CHAR_DOWN	'X'
#define CAPTURE_CHAR_EXIT	'Z'

extern OrchardAppEvent joyEvent;

static char inbuf[INBUF_LEN + 2];
static int incnt;
static int indir;

#define QUEUE_MAX	4
#define QUEUE_INC(x)	(((x) + 1) & (QUEUE_MAX - 1))

static int queueprod;
static int queuecons;
static int queuecnt;
static uint32_t queue[QUEUE_MAX];
static mutex_t queuemutex;

static void
capture_queue_put (uint32_t code)
{
	bool appEvent = TRUE;

	osalMutexLock (&queuemutex);
	if (queuecnt < QUEUE_MAX) {
		queue[queueprod] = code;
		queuecnt++;
		queueprod = QUEUE_INC(queueprod);
	}
	osalMutexUnlock (&queuemutex);

	switch (CAPTURE_CODE(code)) {
		case CAP_UP:
			joyEvent.key.code = keyAUp;
			break;
		case CAP_DOWN:
			joyEvent.key.code = keyADown;
			break;
		case CAP_LEFT:
			joyEvent.key.code = keyALeft;
			break;
		case CAP_RIGHT:
			joyEvent.key.code = keyARight;
			break;
		case CAP_RETURN:
			joyEvent.key.code = keyASelect;
			break;
		default:
			appEvent = FALSE;
			break;
	}

	if (appEvent == TRUE) {
		joyEvent.type = keyEvent;
		if (CAPTURE_DIR(code) == CAPTURE_KEY_DOWN)
			joyEvent.key.flags = keyPress;
		if (CAPTURE_DIR(code) == CAPTURE_KEY_UP)
			joyEvent.key.flags = keyRelease;
		if (orchard_app_key.next != NULL)
			chEvtBroadcast (&orchard_app_key);
	}
		
	return;
}

int
capture_queue_get (uint32_t * code)
{
	if (code == NULL)
		return (0);

	osalMutexLock (&queuemutex);
	if (queuecnt == 0) {
		osalMutexUnlock (&queuemutex);
		return (0);
	}

	*code = queue[queuecons];
	
	queuecnt--;
	queuecons = QUEUE_INC(queuecons);
	osalMutexUnlock (&queuemutex);

	return (1);
}

void
capture_queue_init (void)
{
	osalMutexObjectInit (&queuemutex);
	return;
}

static size_t
capture_con_read (void * instance, uint8_t * c, size_t n)
{
	size_t r;
	char input;
	uint32_t code;

        (void)instance;

	r = vmt1->read (conin, c, n);

	if (r == 1) {
		input = (char)c[0];
		switch (input) {
			case '\r':
			case CAPTURE_CHAR_EXIT:
				osalMutexLock (&conmutex);
				conin->vmt = vmt1;
				osalMutexUnlock (&conmutex);
				osalMutexLock (&queuemutex);
				queuecnt = 0;
				queueprod = 0;
				queuecons = 0;
				osalMutexUnlock (&queuemutex);
				indir = 0;
				incnt = 0;
				break;
			case CAPTURE_CHAR_UP:
				if (indir == 0) {
					indir = KEY_UP;
					memset (inbuf + 2, 0, INBUF_LEN);
				} else {
					indir = 0;
					incnt = 0;
					code = strtol (inbuf, NULL, 16);
					code |= CAPTURE_KEY_UP;
					/* Send key up event */
					capture_queue_put (code);
				}
				break;
			case CAPTURE_CHAR_DOWN:
				if (indir == 0) {
					indir = KEY_DOWN;
					memset (inbuf + 2, 0, INBUF_LEN);
				} else {
					indir = 0;
					incnt = 0;
					code = strtol (inbuf, NULL, 16);
					code |= CAPTURE_KEY_DOWN;
					/* Send key down event */
					capture_queue_put (code);
				}
				break;
			default:
				/* Don't overflow the input buffer */
				if (incnt == INBUF_LEN)
					incnt = 0;
				/* Don't accept non-digit characters */
				if (isxdigit (input) == 0)
					break;
				inbuf[incnt + 2] = input;
				incnt++;
				break;
		}
		/* This is to fake out the shell so it won't print anything */
		c[0] = 1;
	}

        return (r);
}

static void
cmd_capture (BaseSequentialStream *chp, int argc, char *argv[])
{
	(void)argv;
	(void)chp;
	if (argc > 0) {
		printf ("Usage: capture\r\n");
		return;
	}

	osalMutexLock (&conmutex);
	vmt1 = conin->vmt;
	memcpy (&vmt2, vmt1, sizeof (vmt2));
	vmt2.read = capture_con_read;
	conin->vmt = &vmt2;
	osalMutexUnlock (&conmutex);

	strcpy (inbuf, "0x");

	return;
}

orchard_command("capture", cmd_capture);
