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
#include "../../../src/ginput/ginput_driver_mouse.h"

#include "touch_lld.h"

#include <stdio.h>

/*
 * This module provides support for asynchronous event handling from
 * from the touch controller chip. By default, uGFX will periodically
 * poll the touch controller using the gtimer thread. This works, but
 * ideally we'd like the CPU to stay completely asleep when the system
 * is idle, and the polling forces it to wake up all the time. If we
 * use the touch notification pin instead, we can keep the CPU asleep
 * until someone actually touches the screen.
 *
 * Note that the FT5336 captouch controller on the STM32F746 Discovery
 * board supports both a standard interrupt mode, where the interrupt
 * pin remains asserted as long as someone has their finger on the screen,
 * and continuous trigger interrupt mode, where it will pulse the interrupt
 * pin while someone is touching the screen instead. However the XPT2046
 * resistive touch screen only supports the standard mode.
 *
 * For compatibility, we use the standard mode here. The touch event
 * thread will loop and continue to call the mouse event handler as
 * long as the interrupt pin is asserted.
 */

static THD_WORKING_AREA(waTouchThread, 256);
static thread_reference_t touchThreadReference;
static volatile uint8_t touchService = 0;
static volatile uint8_t plevel = 1;

static void
touchInt (void * arg)
{
	(void)arg;

	osalSysLockFromISR ();
	touchService = ~touchService;
	plevel = palReadLine (LINE_LCD_INT);
	osalThreadResumeI (&touchThreadReference, MSG_OK);
	osalSysUnlockFromISR ();

	return;
}

static THD_FUNCTION(touchThread, arg)
{
	GMouse * mouse;

	mouse = arg;

	chRegSetThreadName ("TouchEvent");

	while (1) {
		osalSysLock ();
		if (touchService == 0 && plevel == 1)
			osalThreadSuspendS (&touchThreadReference);
		touchService = 0;
		osalSysUnlock ();

		chThdSleepMilliseconds (5);

		_gmouseWakeup (mouse);
	}

	/* NOTREACHED */

	return;
}

void
touchStart (void)
{
	GMouse * mouse = NULL;

	/*
	 * Set the LCD interrupt pin as an input and turn on the
	 * internal pullup resistor, then enable interrupt events
	 * for it. We want both rising and falling edges so we can
	 * detect both touch and release events.
	 */

	palSetPadMode (GPIOI, GPIOI_LCD_INT, PAL_STM32_PUPDR_PULLUP | 
	    PAL_STM32_MODE_INPUT);

	palSetLineCallback (LINE_LCD_INT, touchInt, NULL);
	palEnableLineEvent (LINE_LCD_INT, PAL_EVENT_MODE_BOTH_EDGES);

	/* Launch the event handler thread. */

	mouse = (GMouse*)gdriverGetInstance (GDRIVER_TYPE_MOUSE, 0);

	chThdCreateStatic (waTouchThread, sizeof(waTouchThread),
	    NORMALPRIO - 1, touchThread, mouse);

	return;
}
