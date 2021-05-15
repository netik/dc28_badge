/*-
 * Copyright (c) 2017
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "ch.h"
#include "hal.h"

#include "async_io_lld.h"

static thread_t * pThread;

static void * async_buf;
static int async_f;
static int async_btr;
static volatile int async_br = (int)ASYNC_THD_READY;
static volatile bool read_start;

static volatile int * saved_br;

static thread_reference_t fsReference;
static thread_reference_t wakeReference;

static THD_FUNCTION(asyncIoThread, arg)
{
	int br = (int)ASYNC_THD_READY;

	(void) arg;

	while (1) {
		osalSysLock ();
		async_br = br;
		osalThreadResumeS (&wakeReference, MSG_OK);
		if (read_start == FALSE)
			osalThreadSuspendS (&fsReference);
		read_start = FALSE;
		osalSysUnlock ();
		if (async_br == (int)ASYNC_THD_EXIT)
			break;
		br = read (async_f, async_buf, async_btr);
		if (br == -1) {
			br = 0;
		}
	}

	chThdExitS (MSG_OK);

        return;
}

void
asyncIoInit (void)
{
	async_br = (int)ASYNC_THD_READY;
	return;
}

void
asyncIoRead (int f, void * buf, size_t btr, int * br)
{
	if (async_br != (int)ASYNC_THD_READY)
		return;

	async_f = f;
	async_buf = buf;
	async_btr = btr;

	saved_br = br;

	osalSysLock ();
	read_start = TRUE;
	async_br = (int)ASYNC_THD_READ;
	osalThreadResumeS (&fsReference, MSG_OK);
	osalSysUnlock ();

	return;
}

void
asyncIoWait (void)
{
	osalSysLock ();
	while (async_br == (int)ASYNC_THD_READ ||
	       async_br == (int)ASYNC_THD_READY)
		osalThreadSuspendS (&wakeReference);
	osalSysUnlock ();

	*saved_br = async_br;
	async_br = (int)ASYNC_THD_READY;

	return;
}

void
asyncIoStart (void)
{
	pThread = chThdCreateFromHeap (NULL, THD_WORKING_AREA_SIZE(512),
	    "AsyncIO", NORMALPRIO, asyncIoThread, NULL);

	async_br = (int)ASYNC_THD_READY;

	return;
}

void
asyncIoStop (void)
{
	osalSysLock ();
	async_br = (int)ASYNC_THD_EXIT;
	osalThreadResumeS (&fsReference, MSG_OK);
	osalSysUnlock ();

	chThdWait (pThread);

	pThread = NULL;

	return;
}
