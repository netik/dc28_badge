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
#include "shell.h"

#include "badge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * This command demonstrates how to use the wakeup timer to put the
 * CPU into stop mode for a pre-determined amount of time. In stop mode,
 * all internal clocks are disbled. Waking the CPU can only be done via
 * EXTI interrupt, either by GPIO pin or via a small number of peripherals
 * which can be run using separate clock sources. This includes the RTC,
 * which can be set to run using the external low speed timer (a
 * 32.768KHz crystal).
 *
 * We can program the RTC to generate a wakeup timer EXTI event after
 * a certain number of ticks. It's also possible to use the RTC alarm
 * to wake the CPU at a specific date and time, but ideally that requires
 * the RTC to be kept synchronized to actual wall clock time, which we
 * don't do right now.
 */ 

static void
cmd_sleep (BaseSequentialStream * chp, int argc, char * argv[])
{
	RTCWakeup wkup;
	uint32_t sleep;

	(void)argv;
	(void)chp;
	if (argc > 1) {
		printf ("Usage: sleep [seconds]\n");
		return;
	}

	if (argc < 1)
		sleep = 5;
	else 
		sleep = atoi (argv[0]);

	if (sleep == 0)
		return;

	if (sleep > 65535) {
		printf ("sleep interval %ld seconds too long\n", sleep);
		return;
	}

	/* Wait a little while for the serial port to go idle. */

	chThdSleepMilliseconds (10);

	/*
	 * Set the wakeup timer. The RTC clock has been set to the
	 * low speed external clock, which is a 32.768KHz crystal.
	 * We can apply a special 32768 prescaler to this to yield
	 * a 1s tick time, which allows us to delay from 1 to
	 * 65535 seconds.
	 */

	wkup.wutr = sleep - 1;
	wkup.wutr |= (RTC_CR_WUCKSEL_2 << 16);

	rtcSTM32SetPeriodicWakeup (&RTCD1, &wkup);

	/* Enter stop mode - this will block until wakeup. */

	badge_deepsleep_enable ();

	/* Aaaand we're back. Turn off the wakeup timer */

	rtcSTM32SetPeriodicWakeup (&RTCD1, NULL);

	return;
}

static void
cmd_hibernate (BaseSequentialStream * chp, int argc, char * argv[])
{
	(void)argv;
	(void)chp;

	if (argc > 0) {
		printf ("Usage: hibernate\n");
		return;
	}

	/* Wait a little while for the serial port to go idle. */

	chThdSleepMilliseconds (10);

	/* Sleep until there's an interrupt */

	badge_deepsleep_enable ();

	return;
}

static void
cmd_screen (BaseSequentialStream * chp, int argc, char * argv[])
{
	(void)chp;

	if (argc != 1)
		goto usage;

	if (strcmp (argv[0], "on") == 0)
		palSetPad (GPIOI, GPIOI_LCD_DISP);
	else if (strcmp (argv[0], "off") == 0)
		palClearPad (GPIOI, GPIOI_LCD_DISP);
	else {
usage:
		printf ("Usage: screen [off|on]\n");
	}

	return;
}

static void
cmd_backlight (BaseSequentialStream * chp, int argc, char * argv[])
{
	(void)chp;

	if (argc != 1)
		goto usage;

	if (strcmp (argv[0], "on") == 0)
		palSetPad (GPIOK, GPIOK_LCD_BL_CTRL);
	else if (strcmp (argv[0], "off") == 0)
		palClearPad (GPIOK, GPIOK_LCD_BL_CTRL);
	else {
usage:
		printf ("Usage: backlight [off|on]\n");
	}

	return;
}

orchard_command("sleep", cmd_sleep);
orchard_command("hibernate", cmd_hibernate);
orchard_command("screen", cmd_screen);
orchard_command("backlight", cmd_backlight);
