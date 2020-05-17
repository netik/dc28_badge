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


/* Uncomment this if your frame buffer device requires flushing */
/* #define GDISP_HARDWARE_FLUSH		GFXON */

#ifdef GDISP_DRIVER_VMT

static void
board_init (GDisplay *g, fbInfo *fbi)
{
	const ltdc_frame_t * f;

	switch (g->controllerdisplay) {
	case 0:
		f = LTDCD1.config->fg_laycfg->frame;
		break;
	case 1:
		f = LTDCD1.config->bg_laycfg->frame;
		break;
	default:
		return;
	}

	g->g.Width = f->width;
	g->g.Height = f->height;
	g->g.Backlight = 100;
	g->g.Contrast = 50;
	fbi->linelen = g->g.Width * sizeof(LLDCOLOR_TYPE); /* bytes per row  */
	fbi->pixels = f->bufferp;

	dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB565);
	dma2dOutSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB565);

	return;
}

#if GDISP_HARDWARE_FLUSH
static void
board_flush (GDisplay *g)
{
	/* Can be an empty function if your hardware doesn't support this */
	(void) g;
}
#endif

#if GDISP_NEED_CONTROL
static void
board_backlight (GDisplay *g, gU8 percent)
{
	(void) g;
	(void) percent;

	if (percent == 100)
		palSetPad (GPIOK, GPIOK_LCD_BL_CTRL);
	else
		palClearPad (GPIOK, GPIOK_LCD_BL_CTRL);

	return;
}

static void
board_contrast (GDisplay *g, gU8 percent)
{
	(void) g;
	(void) percent;
	return;
}

static void
board_power (GDisplay *g, gPowermode pwr)
{
	(void) g;
	(void) pwr;

	if (pwr == gPowerOn)
        	palSetPad (GPIOI, GPIOI_LCD_DISP);
	else
        	palClearPad (GPIOI, GPIOI_LCD_DISP);

	return;
}
#endif

#endif /* GDISP_LLD_BOARD_IMPLEMENTATION */
