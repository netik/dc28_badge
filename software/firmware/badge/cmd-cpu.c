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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "badge.h"

static void
cmd_cpu (BaseSequentialStream *chp, int argc, char *argv[])
{
	(void)argv;
	(void)chp;

	if (argc > 0) {
		printf ("Usage: cpu\n");
		return;
	}

	badge_cpu_show ();

	return;
}

static void
cmd_dcache (BaseSequentialStream *chp, int argc, char *argv[])
{
	(void)chp;

	if (argc != 1) {
		printf ("Usage: dcache [on|off]\n");
		return;
	}

	if (strcmp (argv[0], "on") == 0) {
		badge_cpu_dcache (TRUE);
	} else if (strcmp (argv[0], "off") == 0) {
		badge_cpu_dcache (FALSE);
	} else
		printf ("Command [%s] not recognized\n", argv[1]);

	return;
}

static void
cmd_speed (BaseSequentialStream *chp, int argc, char *argv[])
{
	(void)chp;

	if (argc != 1) {
		printf ("Usage: speed [slow|medium|normal]\n");
		return;
	}

	if (strcmp (argv[0], "slow") == 0) {
		badge_cpu_speed_set (BADGE_CPU_SPEED_SLOW);
	} else if (strcmp (argv[0], "medium") == 0) {
		badge_cpu_speed_set (BADGE_CPU_SPEED_MEDIUM);
	} else if (strcmp (argv[0], "normal") == 0) {
		badge_cpu_speed_set (BADGE_CPU_SPEED_NORMAL);
	} else
		printf ("Command [%s] not recognized\n", argv[1]);

	return;
}

orchard_command("dcache", cmd_dcache);
orchard_command("speed", cmd_speed);
orchard_command("cpu", cmd_cpu);
