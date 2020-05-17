/*-
 * Copyright (c) 2018
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

#include "gfx.h"
#include "src/gdisp/gdisp_driver.h"

#include "hal_stm32_ltdc.h"
#include "hal_stm32_dma2d.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>

static void
cmd_scroll_right (BaseSequentialStream *chp, int argc, char *argv[])
{
	uint16_t * src;
	uint16_t * dst;
	int i;

	(void)argv;
	(void)chp;

	if (argc > 0) {
		printf ("Usage: scroll-right\n");
		return;
        }

	/* Image is at coordinates x=0, y=0 */

	src = (uint16_t *)LTDCD1.config->fg_laycfg->frame->bufferp;

	dst = (uint16_t *)LTDCD1.config->fg_laycfg->frame->bufferp;
	dst += 1; /* skip to first column */

	dma2dFgSetWrapOffsetI (&DMA2DD1, 1);
	dma2dOutSetWrapOffsetI (&DMA2DD1, 1);
	dma2dJobSetSizeI (&DMA2DD1, (gdispGetWidth () - 1), gdispGetHeight ());
	dma2dFgSetAddressI (&DMA2DD1, src);
	dma2dOutSetAddressI (&DMA2DD1, dst);
	dma2dJobSetModeI (&DMA2DD1, DMA2D_JOB_COPY);

	for (i = 0; i < gdispGetWidth () - 1; i++)
		dma2dJobExecute (&DMA2DD1);

	return;
}

static void
cmd_scroll_left (BaseSequentialStream *chp, int argc, char *argv[])
{
	uint16_t * src;
	uint16_t * dst;
	int i;

	(void)argv;
	(void)chp;

	if (argc > 0) {
		printf ("Usage: scroll-left\n");
		return;
        }

	/* Image is at coordinates x=0, y=0 */

	src = (uint16_t *)LTDCD1.config->fg_laycfg->frame->bufferp;
	src += 1; /* skip to first column */

	dst = (uint16_t *)LTDCD1.config->fg_laycfg->frame->bufferp;

	dma2dFgSetWrapOffsetI (&DMA2DD1, 1);
	dma2dOutSetWrapOffsetI (&DMA2DD1, 1);
	dma2dJobSetSizeI (&DMA2DD1, (gdispGetWidth () - 1), gdispGetHeight ());
	dma2dFgSetAddressI (&DMA2DD1, src);
	dma2dOutSetAddressI (&DMA2DD1, dst);
	dma2dJobSetModeI (&DMA2DD1, DMA2D_JOB_COPY);

	for (i = 0; i < gdispGetWidth () - 1; i++)
		dma2dJobExecute (&DMA2DD1);

	return;
}

static void
cmd_scroll_up (BaseSequentialStream *chp, int argc, char *argv[])
{
	uint16_t * src;
	uint16_t * dst;
	int i;

	(void)argv;
	(void)chp;

	if (argc > 0) {
		printf ("Usage: scroll-down\n");
		return;
        }

	/* Image is at coordinates x=0, y=0 */

	src = (uint16_t *)LTDCD1.config->fg_laycfg->frame->bufferp;
	src += gdispGetWidth (); /* skip 1 line */

	dst = (uint16_t *)LTDCD1.config->fg_laycfg->frame->bufferp;

	dma2dFgSetWrapOffsetI (&DMA2DD1, 0);
	dma2dOutSetWrapOffsetI (&DMA2DD1, 0);
	dma2dJobSetSizeI (&DMA2DD1, (gdispGetWidth () - 1), gdispGetHeight ());
	dma2dFgSetAddressI (&DMA2DD1, src);
	dma2dOutSetAddressI (&DMA2DD1, dst);
	dma2dJobSetModeI (&DMA2DD1, DMA2D_JOB_COPY);

	for (i = 0; i < gdispGetHeight () - 1; i++)
		dma2dJobExecute (&DMA2DD1);
	return;
}

static void
cmd_scroll_down (BaseSequentialStream *chp, int argc, char *argv[])
{
	uint16_t * src;
	uint16_t * dst;
	int i, j;

	(void)argv;
	(void)chp;

	if (argc > 0) {
		printf ("Usage: scroll-down\n");
		return;
        }

	/* Image is at coordinates x=0, y=0 */

	for (i = 0; i < gdispGetHeight () - 1; i++) {
		src = (uint16_t *)LTDCD1.config->fg_laycfg->frame->bufferp;
		/* skip to bottom line of image */
		src += gdispGetWidth () * (gdispGetHeight () - 2);

		dst = (uint16_t *)LTDCD1.config->fg_laycfg->frame->bufferp;
		/* skip to first line of image */
		dst += gdispGetWidth () * (gdispGetHeight () - 1);

		dma2dFgSetWrapOffsetI (&DMA2DD1, 0);
		dma2dOutSetWrapOffsetI (&DMA2DD1, 0);
		dma2dJobSetSizeI (&DMA2DD1, gdispGetWidth (), 1);

		for (j = 0; j < gdispGetHeight () - 1; j++) {
			dma2dFgSetAddressI (&DMA2DD1, src);
			dma2dOutSetAddressI (&DMA2DD1, dst);
			dma2dJobSetModeI (&DMA2DD1, DMA2D_JOB_COPY);
			dma2dJobExecute (&DMA2DD1);
			src -= gdispGetWidth ();
			dst -= gdispGetWidth ();
		}
	}

	return;
}

orchard_command("scroll-right", cmd_scroll_right);
orchard_command("scroll-left", cmd_scroll_left);
orchard_command("scroll-up", cmd_scroll_up);
orchard_command("scroll-down", cmd_scroll_down);
