/*-
 * Copyright (c) 2017-2019
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

#include "hal_stm32_ltdc.h"
#include "hal_stm32_dma2d.h"

#include "gfx.h"

#if GFX_USE_GDISP

#define GDISP_DRIVER_VMT			GDISPVMT_STM32FB
#include "gdisp_lld_config.h"
#include "../../../src/gdisp/gdisp_driver.h"

typedef struct fbInfo {
	void *	pixels;		/* The pixel buffer */
	gCoord	linelen;	/* The number of bytes per display line */
} fbInfo;

#include "board_STM32FB.h"

typedef struct fbPriv {
	fbInfo	fbi;		/* Display information */
} fbPriv;

/*===========================================================================*/
/* Driver local routines.                                                    */
/*===========================================================================*/

#define PIXEL_POS(g, x, y) \
  ((y) * ((fbPriv *)(g)->priv)->fbi.linelen + (x) * sizeof(LLDCOLOR_TYPE))
#define PIXEL_ADDR(g, pos) \
  ((LLDCOLOR_TYPE *)(((char *)((fbPriv *)(g)->priv)->fbi.pixels)+pos))

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

LLDSPEC gBool
gdisp_lld_init (GDisplay *g)
{
	/* Initialize the private structure */

	if (!(g->priv = gfxAlloc (sizeof(fbPriv)))) {
		gfxHalt ("GDISP Framebuffer: "
		    "Failed to allocate private memory");
	}

	((fbPriv *)g->priv)->fbi.pixels = 0;
	((fbPriv *)g->priv)->fbi.linelen = 0;

	/* Initialize the GDISP structure */

	g->g.Orientation = gOrientation0;
	g->g.Powermode = gPowerOn;
	g->board = 0;	/* preinitialize */
	board_init (g, &((fbPriv *)g->priv)->fbi);

	return (gTrue);
}

#if GDISP_HARDWARE_FLUSH
LLDSPEC void gdisp_lld_flush(GDisplay *g)
{
	board_flush (g);
}
#endif

LLDSPEC void
gdisp_lld_draw_pixel (GDisplay *g)
{
	unsigned	pos;

#if GDISP_NEED_CONTROL
	switch(g->g.Orientation) {
	case gOrientation0:
	default:
		pos = PIXEL_POS(g, g->p.x, g->p.y);
		break;
	case gOrientation90:
		pos = PIXEL_POS(g, g->p.y, g->g.Width-g->p.x-1);
		break;
	case gOrientation180:
		pos = PIXEL_POS(g, g->g.Width-g->p.x-1, g->g.Height-g->p.y-1);
		break;
	case gOrientation270:
		pos = PIXEL_POS(g, g->g.Height-g->p.y-1, g->p.x);
		break;
	}
#else
	pos = PIXEL_POS(g, g->p.x, g->p.y);
#endif

#if GDISP_LLD_PIXELFORMAT == GDISP_PIXELFORMAT_RGB888
	PIXEL_ADDR(g, pos)[0] = gdispColor2Native(g->p.color) ^ 0xFF000000;
#else
	PIXEL_ADDR(g, pos)[0] = gdispColor2Native(g->p.color);
#endif

	return;
}

LLDSPEC	gColor
gdisp_lld_get_pixel_color (GDisplay *g)
{
	unsigned	pos;
	LLDCOLOR_TYPE	color;

#if GDISP_NEED_CONTROL
	switch(g->g.Orientation) {
	case gOrientation0:
	default:
		pos = PIXEL_POS(g, g->p.x, g->p.y);
		break;
	case gOrientation90:
		pos = PIXEL_POS(g, g->p.y, g->g.Width-g->p.x-1);
		break;
	case gOrientation180:
		pos = PIXEL_POS(g, g->g.Width-g->p.x-1, g->g.Height-g->p.y-1);
		break;
	case gOrientation270:
		pos = PIXEL_POS(g, g->g.Height-g->p.y-1, g->p.x);
		break;
	}
#else
	pos = PIXEL_POS(g, g->p.x, g->p.y);
#endif

#if GDISP_LLD_PIXELFORMAT == GDISP_PIXELFORMAT_RGB888
	color = PIXEL_ADDR(g, pos)[0] ^ 0xFF000000;
#else
	color = PIXEL_ADDR(g, pos)[0];
#endif

	return gdispNative2Color(color);
}

/* Uses p.x,p.y  p.cx,p.cy  p.color */
LLDSPEC void
gdisp_lld_fill_area (GDisplay* g)
{
	gU32 pos;
	gU32 lineadd;

#if GDISP_NEED_CONTROL
	switch(g->g.Orientation) {
	case gOrientation0:
	default:
		pos = PIXEL_POS(g, g->p.x, g->p.y);
		lineadd = g->g.Width - g->p.cx;
		break;
	case gOrientation90:
		pos = PIXEL_POS(g, g->p.y, g->g.Width-g->p.x-g->p.cx);
		lineadd = g->g.Height - g->p.cy;
		break;
	case gOrientation180:
		pos = PIXEL_POS(g, g->g.Width-g->p.x-g->p.cx,
		    g->g.Height-g->p.y-g->p.cy);
		lineadd = g->g.Width - g->p.cx;
		break;
	case gOrientation270:
		pos = PIXEL_POS(g, g->g.Height-g->p.y-g->p.cy, g->p.x);
		lineadd = g->g.Height - g->p.cy;
		break;
	}
#else
	pos = PIXEL_POS(g, g->p.x, g->p.y);
	lineadd = g->g.Width - g->p.cx;
#endif

	dma2dOutSetAddressI (&DMA2DD1, (void *)PIXEL_ADDR(g, pos));
	dma2dOutSetWrapOffsetI (&DMA2DD1, lineadd);
	dma2dJobSetSizeI (&DMA2DD1, g->p.cx, g->p.cy);
#if GDISP_LLD_PIXELFORMAT == GDISP_PIXELFORMAT_RGB888
	dma2dOutSetDefaultColorI (&DMA2DD1,
	    gdispColor2Native(g->p.color)) ^ 0xFF000000);
#else
	dma2dOutSetDefaultColorI (&DMA2DD1, gdispColor2Native(g->p.color));
#endif
	dma2dJobSetModeI (&DMA2DD1, DMA2D_JOB_CONST);
	dma2dJobExecute (&DMA2DD1);

	return;
}


/*
 * Uses p.x,p.y  p.cx,p.cy  p.x1,p.y1 (=srcx,srcy)
 *  p.x2 (=srccx), p.ptr (=buffer)
 */

LLDSPEC void
gdisp_lld_blit_area (GDisplay* g)
{
	gU32	srcstart, dststart;
	gU32	len;

#define LTDC_PIXELBYTES 2

	srcstart = LTDC_PIXELBYTES * ((gU32)g->p.x2 * g->p.y1 * + g->p.x1) + (gU32)g->p.ptr;
	dststart = (gU32)PIXEL_ADDR(g, PIXEL_POS(g, g->p.x, g->p.y));

	/* Flush the source area */

	len = (g->p.cy > 1 ? ((gU32)g->p.x2 * g->p.cy) :
	    (gU32)g->p.cx) * LTDC_PIXELBYTES;
	len += CACHE_LINE_SIZE;
	cacheBufferFlush ((void *)srcstart, len);

	dma2dFgSetAddressI (&DMA2DD1, (void *)srcstart);
	dma2dFgSetWrapOffsetI (&DMA2DD1, g->p.x2 - g->p.cx);
	dma2dOutSetAddressI (&DMA2DD1, (void *)dststart);
	dma2dOutSetWrapOffsetI (&DMA2DD1, g->g.Width - g->p.cx);
	dma2dJobSetSizeI (&DMA2DD1, g->p.cx, g->p.cy);
	dma2dJobSetModeI (&DMA2DD1, DMA2D_JOB_CONVERT);
	dma2dJobExecute (&DMA2DD1);

	return;
}

#if GDISP_NEED_CONTROL
LLDSPEC void
gdisp_lld_control (GDisplay *g)
{
	switch (g->p.x) {
	case GDISP_CONTROL_POWER:
		if (g->g.Powermode == (gPowermode)g->p.ptr)
			break;

		switch((gPowermode)g->p.ptr) {
			case gPowerOff:
			case gPowerOn:
			case gPowerSleep:
			case gPowerDeepSleep:
				board_power (g, (gPowermode)g->p.ptr);
				break;
			default:
				return;
		}

		g->g.Powermode = (gPowermode)g->p.ptr;
		return;

	case GDISP_CONTROL_ORIENTATION:
		if (g->g.Orientation == (gOrientation)g->p.ptr)
			break;

		switch ((gOrientation)g->p.ptr) {
		case gOrientation0:
		case gOrientation180:
			if (g->g.Orientation == gOrientation90 ||
			    g->g.Orientation == gOrientation270) {
				gCoord	tmp;

				tmp = g->g.Width;
				g->g.Width = g->g.Height;
				g->g.Height = tmp;
			}
			break;
		case gOrientation90:
		case gOrientation270:
			if (g->g.Orientation == gOrientation0 ||
			    g->g.Orientation == gOrientation180) {
				gCoord	tmp;

				tmp = g->g.Width;
				g->g.Width = g->g.Height;
				g->g.Height = tmp;
			}
			break;
		default:
			break;
		}

		g->g.Orientation = (gOrientation)g->p.ptr;
		break;

		case GDISP_CONTROL_BACKLIGHT:
			if ((unsigned)g->p.ptr > 100) g->p.ptr = (void *)100;
				board_backlight (g, (unsigned)g->p.ptr);
			g->g.Backlight = (unsigned)g->p.ptr;
			break;

		case GDISP_CONTROL_CONTRAST:
			if ((unsigned)g->p.ptr > 100) g->p.ptr = (void *)100;
				board_contrast (g, (unsigned)g->p.ptr);
			g->g.Contrast = (unsigned)g->p.ptr;
			break;
		}

	return;
	}
#endif

#endif /* GFX_USE_GDISP */
