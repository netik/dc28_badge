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
#include <malloc.h>

#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "badge.h"

static void
cmd_mem (BaseSequentialStream *chp, int argc, char *argv[])
{
	(void)argv;
	(void)chp;
	if (argc > 0) {
		printf ("Usage: mem\n");
		return;
	}

	printf ("total heap size  = %10u\n", HEAP_END - HEAP_BASE);
	malloc_stats ();

	return;
}

static void
cmd_memtest (BaseSequentialStream *chp, int argc, char *argv[])
{
	uint8_t * p;
	uint32_t i;

	(void)argv;
	(void)chp;
	if (argc > 0) {
		printf ("Usage: memtest\n");
		return;
	}

	p = malloc (65536 * 16);
	printf ("allocated at %p\n", p);
	for (i = 0; i < (65536 * 16); i++) {
		p[i] = 0xa5;
		if (p[i] != 0xa5) {
			printf ("error at %p\n", &p[i]);
		}
	}
	free (p);

	return;
}

orchard_command("mem-sdram", cmd_mem);
orchard_command("memtest-sdram", cmd_memtest);
