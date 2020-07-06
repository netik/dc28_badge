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

#include <stdio.h>

#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "badge.h"
#include "stm32adc_lld.h"

static void
cmd_temp (BaseSequentialStream *chp, int argc, char *argv[])
{
	float temp;

	(void)argv;
	(void)chp;
	if (argc > 0) {
		printf ("Usage: temp\n");
		return;
	}

	temp = stm32TempGet ();

	printf ("CPU temperature: %f C (%f F)\n", temp,
	    (temp * ((float)9/(float)5)) + (float)32);

	return;
}

static void
cmd_batt (BaseSequentialStream *chp, int argc, char *argv[])
{
	(void)argv;
	(void)chp;
	if (argc > 0) {
		printf ("Usage: batt\n");
		return;
	}

	printf ("VBAT voltage: %f\n", stm32VbatGet ());

	return;
}

static void
cmd_vref (BaseSequentialStream *chp, int argc, char *argv[])
{
	(void)argv;
	(void)chp;
	if (argc > 0) {
		printf ("Usage: vref\n");
		return;
	}

	printf ("CPU internal reference voltage: %f\n", stm32VrefGet ());

	return;
}

orchard_command("batt", cmd_batt);
orchard_command("vref", cmd_vref);
orchard_command("temp", cmd_temp);
