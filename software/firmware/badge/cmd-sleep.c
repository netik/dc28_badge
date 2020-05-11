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

static void
cmd_sleep (BaseSequentialStream * chp, int argc, char * argv[])
{
	RTCAlarm alarmspec;
	RTCDateTime rtctm;
	struct tm tim;
	uint32_t tv_sec;
	uint32_t sleep;

	(void)argv;
	(void)chp;
	if (argc > 1) {
		printf ("Usage: sleep [seconds]\r\n");
		return;
	}

	rtcGetTime (&RTCD1, &rtctm);
	rtcConvertDateTimeToStructTm (&rtctm, &tim, NULL);
	tv_sec = tim.tm_sec;

	if (argc < 1)
		sleep = 5;
	else 
		sleep = atoi (argv[0]);

	if (sleep > 59) {
		printf ("sleep interval %ld seconds too long\n", sleep);
		return;
	}

	tv_sec += sleep;
	if (tv_sec > 59)
		tv_sec -= 59;

	rtcSetAlarm (&RTCD1, 0, NULL);

	alarmspec.alrmr =
	    /* Don't care about dats/hours/minutes match */
	    RTC_ALRMAR_MSK4 | RTC_ALRMAR_MSK3 | RTC_ALRMAR_MSK2 |
	    /* Tens of seconds */
	    ((tv_sec / 10) << 4) |
	    /* Single seconds */
	    (tv_sec % 10);

	rtcSetAlarm (&RTCD1, 0, &alarmspec);

	/* Wait a little while for the serial port to go idle. */

	chThdSleepMilliseconds (10);

	badge_sleep_enable ();
	badge_deepsleep_enable ();

	return;
}

orchard_command("sleep", cmd_sleep);
