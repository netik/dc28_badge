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

#include "gfx.h"
#include "src/gdisp/gdisp.h"
#include "drivers/gdisp/STM32LTDC/stm32_dma2d.h"

#include "ff.h"
#include "ffconf.h"

#include "scroll_lld.h"

#include "badge.h"

#include <stdlib.h>

void
scrollAreaSet (uint16_t TFA, uint16_t BFA)
{
	(void)TFA;
	(void)BFA;
	return;
}

void
scrollCount (int lines)
{
	uint16_t * src;
	uint16_t * dst;
	int i;

	while (DMA2D->CR & DMA2D_CR_START) {
	}

	src = (uint16_t *)FB_BASE;
	src += 1; /* skip to first column */

	dst = (uint16_t *)FB_BASE;

	DMA2D->FGOR = 1;
	DMA2D->OOR = 1;
	DMA2D->NLR = ((gdispGetWidth () - 1) << 16) | gdispGetHeight ();
	DMA2D->FGMAR = (uint32_t)src;
	DMA2D->OMAR = (uint32_t)dst;

	for (i = 0; i < lines; i++) {
		DMA2D->CR = DMA2D_CR_MODE_M2M | DMA2D_CR_START;
		while (DMA2D->CR & DMA2D_CR_START) {
		}
	}

	return;
}

int
scrollImage (char * file, int delay)
{
	int i;
	FIL f;
 	UINT br;
	GDISP_IMAGE * hdr;
	uint16_t h;
	uint16_t w;
	gPixel * buf;
	GEventMouse * me;
	GSourceHandle gs;
	GListener gl;
	int r = 0;

	if (f_open (&f, file, FA_READ) != FR_OK)
		return (-1);

	gs = ginputGetMouse (0);
	geventListenerInit (&gl);
	geventAttachSource (&gl, gs, GLISTEN_MOUSEMETA);

	buf = malloc (sizeof(gPixel) * 240);

	hdr = (GDISP_IMAGE *)buf;
	f_read (&f, hdr, sizeof(GDISP_IMAGE), &br);
	h = hdr->gdi_height_hi << 8 | hdr->gdi_height_lo;
	w = hdr->gdi_width_hi << 8 | hdr->gdi_width_lo;

	me = NULL;

	/* Sanity check for bogus images */

	if (hdr->gdi_id1 != 'N' || hdr->gdi_id2 != 'I' || w > 240)
		goto out;

	for (i = 0; i < h; i++) {
		f_read (&f, buf, sizeof(gPixel) * w, &br);
		gdispBlitAreaEx (319, (240 - w) / 2, 1,
		    w, 0, 0, 1, buf);
		scrollCount (1);
		me = (GEventMouse *)geventEventWait(&gl, 0);
		if (me != NULL &&  me->buttons & GMETA_MOUSE_DOWN) {
			r = -1;
			break;
		}
		chThdSleepMilliseconds (delay);
	}

out:

	geventDetachSource (&gl, NULL);

	free (buf);
	f_close (&f);

	return (r);
}
