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
#include "drivers/gdisp/STM32LTDC/stm32_dma2d.h"

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

	src = (uint16_t *)FB_BASE;

	dst = (uint16_t *)FB_BASE;
	dst += 1; /* skip to first column */

	DMA2D->FGOR = 480 - 319;
	DMA2D->OOR = 480 - 319;
	DMA2D->NLR = (319 << 16) | 240;
	DMA2D->FGMAR = (uint32_t)src;
	DMA2D->OMAR = (uint32_t)dst;

	for (i = 0; i < 319; i++) {
		DMA2D->CR = DMA2D_CR_MODE_M2M | DMA2D_CR_START;
		while (DMA2D->CR & DMA2D_CR_START) {
		}
	}

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

	src = (uint16_t *)FB_BASE;
	src += 1; /* skip to first column */

	dst = (uint16_t *)FB_BASE;

	DMA2D->FGOR = 480 - 319;
	DMA2D->OOR = 480 - 319;
	DMA2D->NLR = (319 << 16) | 240;
	DMA2D->FGMAR = (uint32_t)src;
	DMA2D->OMAR = (uint32_t)dst;

	for (i = 0; i < 319; i++) {
		DMA2D->CR = DMA2D_CR_MODE_M2M | DMA2D_CR_START;
		while (DMA2D->CR & DMA2D_CR_START) {
		}
	}

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

	src = (uint16_t *)FB_BASE;
	src += 480; /* skip 1 line */

	dst = (uint16_t *)FB_BASE;

	DMA2D->FGOR = 480 - 320;
	DMA2D->OOR = 480 - 320;
	DMA2D->NLR = (320 << 16) | 239;

	DMA2D->FGMAR = (uint32_t)src;
	DMA2D->OMAR = (uint32_t)dst;

	for (i = 0; i < 239; i++) {
		DMA2D->CR = DMA2D_CR_MODE_M2M | DMA2D_CR_START;
		while (DMA2D->CR & DMA2D_CR_START) {
		}
	}

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

	for (i = 0; i < 239; i++) {
		src = (uint16_t *)FB_BASE;
		src += 480 * (238); /* skip to bottom line of image */

		dst = (uint16_t *)FB_BASE;
		dst += 480 * (239); /* skip to first line of image */

		DMA2D->FGOR = 480 - 320;
		DMA2D->OOR = 480 - 320;
		DMA2D->NLR = (320 << 16) | 1;

		for (j = 0; j < 239; j++) {
			DMA2D->FGMAR = (uint32_t)src;
			DMA2D->OMAR = (uint32_t)dst;
			DMA2D->CR = DMA2D_CR_MODE_M2M | DMA2D_CR_START;
			while (DMA2D->CR & DMA2D_CR_START) {
			}
			src -= 480;
			dst -= 480;
		}
	}

	return;
}

orchard_command("scroll-right", cmd_scroll_right);
orchard_command("scroll-left", cmd_scroll_left);
orchard_command("scroll-up", cmd_scroll_up);
orchard_command("scroll-down", cmd_scroll_down);
