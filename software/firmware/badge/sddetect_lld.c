/*-
 * Copyright (c) 2019
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
#include "osal.h"

#include "gfx.h"

#include "sddetect_lld.h"

/*
 * On the STM32F764 Discovery board, the LINE_SD_DETECT pin is PC13.
 */

static THD_WORKING_AREA(waSdDetectThread, 256);
static thread_reference_t sdDetectThreadReference;
static volatile uint8_t sdDetectService = 0;

static mutex_t sdMutex;
static volatile uint8_t sdMounted = FALSE;

static void
sdDetectInt (void * arg)
{
	(void)arg;

	osalSysLockFromISR ();
	sdDetectService = 1;
	osalThreadResumeI (&sdDetectThreadReference, MSG_OK);
	osalSysUnlockFromISR ();

	return;
}

static void
sdCheck (void)
{
	chThdSleepMilliseconds (1);

	osalMutexLock (&sdMutex);

	if (palReadLine (LINE_SD_DETECT)) {
		if (sdMounted == FALSE)
			goto skip;
		gfileUnmount ('F', "0:");
		sdcDisconnect (&SDCD1);
		sdMounted = FALSE;
	} else {
		if (sdMounted == TRUE)
			goto skip;
		sdcConnect (&SDCD1);
		gfileMount ('F', "0:");
		sdMounted = TRUE;
	}

skip:

	osalMutexUnlock (&sdMutex);

	return;
}

static THD_FUNCTION(sdDetectThread, arg)
{
	(void)arg;

	chRegSetThreadName ("SDCardEvent");

	while (1) {
		osalSysLock ();
		if (sdDetectService == 0)
			osalThreadSuspendS (&sdDetectThreadReference);
		sdDetectService = 0;
		osalSysUnlock ();

		sdCheck ();
	}

	/* NOTREACHED */

	return;
}

void
sdDetectStart (void)
{
	/*
	 * Set the SD detect pin as an input and turn on the
	 * internal pullup resistor, then enable interrupt events
	 * for it. We want both rising and falling edges so we can
	 * detect both insert and eject events.
	 */

	palSetLineMode (LINE_SD_DETECT, PAL_STM32_PUPDR_PULLUP | 
	    PAL_STM32_MODE_INPUT);

	palSetLineCallback (LINE_SD_DETECT, sdDetectInt, NULL);
	palEnableLineEvent (LINE_SD_DETECT, PAL_EVENT_MODE_BOTH_EDGES);

	osalMutexObjectInit (&sdMutex);

	/* Launch the event handler thread. */

	chThdCreateStatic (waSdDetectThread, sizeof(waSdDetectThread),
	    NORMALPRIO - 1, sdDetectThread, NULL);

	sdCheck ();

	return;
}
