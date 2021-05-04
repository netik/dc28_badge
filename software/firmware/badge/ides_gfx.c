
/* ides_gfx.c
 *
 * Shared Graphics Routines
 * J. Adams (dc25: 1/2017, dc27: 6/2019)
 */

#include "ch.h"
#include "hal.h"

#include "orchard-app.h"
#include "string.h"
#include "fontlist.h"
#include "ides_gfx.h"

#include "gfx.h"
#include "src/gdisp/gdisp_driver.h"
#include "hal_stm32_dma2d.h"
#include "hal_stm32_ltdc.h"

#include "ffconf.h"
#include "ff.h"

#include "async_io_lld.h"
#include "badge.h"

#include <stdio.h>
#include <stdlib.h>

// WidgetStyle: RedButton, the only button we really use

const GWidgetStyle RedButtonStyle = {
  HTML2COLOR(0xff0000),              // background
  HTML2COLOR(0xff6666),              // focus

  // Enabled color set
  {
    HTML2COLOR(0xffffff),         // text
    HTML2COLOR(0x800000),         // edge
    HTML2COLOR(0xff0000),         // fill
    HTML2COLOR(0x008000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0x808080),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x404040),         // fill
    HTML2COLOR(0x004000),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0xFFFFFF),         // text
    HTML2COLOR(0x800000),         // edge
    HTML2COLOR(0xff6a71),         // fill
    HTML2COLOR(0x008000),         // progress (active area)
  }
};

const GWidgetStyle GreenButtonStyle = {
    HTML2COLOR(0x00ff00),          // background
    HTML2COLOR(0x66ff66),          // focus

  // Enabled color set
  {
    HTML2COLOR(0xffffff),         // text
    HTML2COLOR(0x008000),         // edge
    HTML2COLOR(0x00ff00),         // fill
    HTML2COLOR(0x800000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0x808080),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x404040),         // fill
    HTML2COLOR(0x004000),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0xFFFFFF),         // text
    HTML2COLOR(0x008000),         // edge
    HTML2COLOR(0x6aff71),         // fill
    HTML2COLOR(0x800000),         // progress (active area)
  }
};

const GWidgetStyle DarkPurpleStyle = {
  HTML2COLOR(0x470b67),              // background
  HTML2COLOR(0x2ca0ff),              // focus

  // Enabled color set
  {
    HTML2COLOR(0xc8c8c8),         // text
    HTML2COLOR(0x3d2e44),         // edge
    HTML2COLOR(0x808080),         // fill
    HTML2COLOR(0x008000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0x808080),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x404040),         // fill
    HTML2COLOR(0x004000),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0xFFFFFF),         // text
    HTML2COLOR(0xC0C0C0),         // edge
    HTML2COLOR(0xE0E0E0),         // fill
    HTML2COLOR(0x008000),         // progress (active area)
  }
};

const GWidgetStyle DarkPurpleFilledStyle = {
  HTML2COLOR(0x470b67),              // background
  HTML2COLOR(0x2ca0ff),              // focus

  // Enabled color set
  {
    HTML2COLOR(0xc8c8c8),         // text
    HTML2COLOR(0x3d2e44),         // edge
    HTML2COLOR(0x470b67),         // fill
    HTML2COLOR(0x008000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0x808080),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x404040),         // fill
    HTML2COLOR(0x004000),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0xFFFFFF),         // text
    HTML2COLOR(0xC0C0C0),         // edge
    HTML2COLOR(0xE0E0E0),         // fill
    HTML2COLOR(0x008000),         // progress (active area)
  }
};

// WidgetStyle: Ivory
const GWidgetStyle DarkGFX_GREYStyle = {
  HTML2COLOR(0xffefbe),              // background
  HTML2COLOR(0x2A8FCD),              // focus

  // Enabled color set
  {
    HTML2COLOR(0x000000),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0xffefbe),         // background
    HTML2COLOR(0x00E000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0xC0C0C0),         // text
    HTML2COLOR(0x808080),         // edge
    HTML2COLOR(0xE0E0E0),         // fill
    HTML2COLOR(0xC0E0C0),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0x404040),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x808080),         // fill
    HTML2COLOR(0x00E000),         // progress (active area)
  }
};

static int
putGifImage (char *name, int16_t x, int16_t y)
{
	gdispImage img;

	if (gdispImageOpenFile (&img, name) == GDISP_IMAGE_ERR_OK) {
		gdispImageDraw (&img, x, y, img.width, img.height, 0, 0);
		gdispImageClose (&img);
		return (1);
	}

	return (0);
}

static int
putRgbImage (char *name, int16_t x, int16_t y)
{
	gdispImage img;

	if (gdispImageOpenFile (&img, name) == GDISP_IMAGE_ERR_OK) {
		gdispImageDraw (&img, x, y, img.width, img.height, 0, 0);
		gdispImageClose (&img);
		return (1);
 	}

	printf ("can't load image %s!!\n", name);
	return (0);
}

int
putImageFile (char *name, int16_t x, int16_t y)
{
	int r;

	if (strstr (name, ".gif") != NULL)
		r = putGifImage (name, x, y);
	else
		r = putRgbImage (name, x, y);

	return (r);
}

void
screen_alert_draw (uint8_t clear, char *msg)
{
	uint16_t middle;
	gFont fontFF;

	middle = (gdispGetHeight () >> 1);
	fontFF = gdispOpenFont (FONT_FIXED);


	if (clear) {
		gdispClear(GFX_BLACK);
	} else {
		/* just black out the drawing area. */
		gdispFillArea( 0, middle - 20,
		    gdispGetWidth(), 40, GFX_BLACK);
	}

	gdispDrawThickLine (0, middle - 20, gdispGetWidth (),
	    middle - 20, GFX_BLUE, 2, FALSE);
	gdispDrawThickLine (0, middle + 20, gdispGetWidth (),
	    middle +20, GFX_BLUE, 2, FALSE);

	gdispDrawStringBox (0,
	    middle - (gdispGetFontMetric(fontFF, gFontHeight) >> 1),
	    gdispGetWidth (), gdispGetFontMetric (fontFF, gFontHeight),
	    msg, fontFF, GFX_YELLOW, gJustifyCenter);

	gdispCloseFont (fontFF);

	return;
}


void drawProgressBar(gCoord x, gCoord y,
                     gCoord width, gCoord height,
                     int32_t maxval, int32_t currentval,
                     uint8_t use_leds, uint8_t reverse) {

  (void)use_leds;

  // draw a bar if reverse is true, we draw right to left vs left to
  // right

  // WARNING: if x+w > screen_width or y+height > screen_height,
  // unpredicable things will happen in memory. There is no protection
  // for overflow here.

  gColor c = GFX_LIME;
  int16_t remain;
  float remain_f;

  if (currentval < 0)
    currentval = 0; // never overflow

  if (currentval > maxval) {
    // prevent bar overflow
    remain_f = 1;
  } else {
    remain_f = (float)currentval / (float)maxval;
  }

  remain = width * remain_f;

  if (remain_f >= 0.8) {
    c = GFX_LIME;
  } else if (remain_f >= 0.5) {
    c = GFX_YELLOW;
  } else {
    c = GFX_RED;
  }

  /*
   * Clear the clip variables otherwise
   * gdispGFillArea() might fail.
   */

  GDISP->clipx0 = 0;
  GDISP->clipy0 = 0;
  GDISP->clipx1 = gdispGetWidth ();
  GDISP->clipy1 = gdispGetHeight ();

  if (reverse) {
    gdispFillArea(x,y+1,(width - remain)-1,height-2, GFX_BLACK);
    gdispFillArea((x+width)-remain,y,remain,height, c);
  } else {
    gdispFillArea(x + remain,y+1,(width - remain)-1,height-2, GFX_BLACK);
    gdispFillArea(x,y,remain,height, c);
  }

  gdispDrawBox (x, y, width, height, c);
  gdispDrawBox (x - 1, y - 1, width + 2, height + 2, GFX_BLACK);
}

/*
 * Read a block of pixels from the screen
 * x,y == location of box
 * cx,cy == dimensions of box
 * buf == pointer to pixel buffer
 * Note: buf must be big enough to hold the resulting data
 *       (cx x cy x sizeof(gPixel))
 * Reading pixels from the display is a bit painful on the ILI9341, because
 * it returns 24-bit RGB color data instead of 16-bit RGB565 pixel data.
 * The gdisp_lld_read_color() does conversion in software.
 */

void
getPixelBlock (gCoord x, gCoord y, gCoord cx, gCoord cy, gPixel * buf)
{
  (void)x;
  (void)y;
  (void)cx;
  (void)cy;
  (void)buf;

#ifdef notdef
  int i;

  GDISP->p.x = x;
  GDISP->p.y = y;
  GDISP->p.cx = cx;
  GDISP->p.cy = cy;

  gdisp_lld_read_start (GDISP);

  for (i = 0; i < (cx * cy); i++)
    buf[i] = gdisp_lld_read_color (GDISP);

  gdisp_lld_read_stop (GDISP);
#endif

  return;
}

/*
 * Write a block of pixels to the screen
 * x,y == location of box
 * cx,cy == dimensions of box
 * buf == pointer to pixel buffer
 * Note: buf must be big enough to hold the resulting data
 *       (cx*cy*sizeof(gPixel))
 * This is basically a blitter function, but it's a little more
 * efficient than the one in uGFX. We bypass the gdisp_lld_write_color()
 * function and dump the pixels straight into the SPI bus.
 * Note: Pixel data must be in big endian form, since the Nordic SPI
 * controller doesn't support 16-bit accesses (like the NXP one did).
 * This means we're really using RGB565be format.
 */

void
putPixelBlock (gCoord x, gCoord y, gCoord cx, gCoord cy, gPixel * buf)
{
  gdispBlitArea (x, y, cx, cy, buf);
  return;
}

// this allows us to write text to the screen and remember the background
void drawBufferedStringBox(
  gPixel **fb,
  gCoord x,
  gCoord y,
  gCoord cx,
  gCoord cy,
  const char* str,
  gFont font,
  gColor color,
  gJustify justify) {
    // if this is the first time through, init the buffer and
    // remember the background
    if (*fb == NULL) {
      *fb = (gPixel *) malloc(cx * cy * sizeof(gPixel));
      // get the pixels
      getPixelBlock (x, y, cx, cy, *fb);
    } else {
      // paint it back.
      putPixelBlock (x, y, cx, cy, *fb);
    }

    // paint the text
    gdispDrawStringBox(x,y,cx,cy,str,font,color,justify);
}

static int layer;
static void * d0;
static void * d1;

void
idesDoubleBufferInit (uint8_t pixfmt)
{
	/* Get screen handles */

	d0 = (void *)gdriverGetInstance (GDRIVER_TYPE_DISPLAY, 0);
	d1 = (void *)gdriverGetInstance (GDRIVER_TYPE_DISPLAY, 1);

	layer = 0;

	/*
	 * Ensure correct initial display frame is selected:
	 * foreground (display 0) on, background (display 1) off.
	 */

	ltdcFgEnableI (&LTDCD1);
	ltdcBgDisableI (&LTDCD1);
	ltdcReload (&LTDCD1, FALSE);

	/*
	 * Set the desired DMA2D pixel format. There are three
	 * choices:
	 *
	 * IDES_DB_DEFAULT: default setting, RGB565
	 * IDES_DB_RGB888: RGB888, used by video player
	 * IDES_DB_L8: 8-bit lookup table, uses 8-bit color with
	 * hardware color look-up table (CLUT)
	 *
	 * For IDES_DB_L8, the caller must also use
	 * esDoubleBufferPaleteLoad() to install a palette into
	 * the DMA2D engine.
	 */

	dma2dAcquireBusS (&DMA2DD1);

	switch (pixfmt) {
		case IDES_DB_RGB888:
			dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB888);
			dma2dBgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB888);
			break;
		case IDES_DB_L8:
			dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_L8);
			dma2dBgSetPixelFormat (&DMA2DD1, DMA2D_FMT_L8);
			break;
		case IDES_DB_DEFAULT:
		default:
			dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB565);
			dma2dBgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB565);
			break;
	}

	dma2dReleaseBusS (&DMA2DD1);

	gdispGClear (d0, GFX_BLACK);
	gdispGClear (d1, GFX_BLACK);

	return;
}

void
idesDoubleBufferPaletteLoad (palette_color_t * palette)
{
	dma2d_palcfg_t palcfg;

	cacheBufferFlush (palette, sizeof(palette_color_t) * 256);

	/*
	 * The assumption is that we will always use the L8 CLUT
	 * format, so there will always be 256 colors, and for
	 * simplicity we always use the ARGB8888 palette format.
	 */

	palcfg.length = 256;
	palcfg.fmt = DMA2D_FMT_ARGB8888;
	palcfg.colorsp = palette;

	/* Load the palette into the DMA2D engine */

	dma2dAcquireBusS (&DMA2DD1);

	dma2dFgSetPalette (&DMA2DD1, &palcfg);
	dma2dBgSetPalette (&DMA2DD1, &palcfg);

	dma2dReleaseBusS (&DMA2DD1);

	return;
}

void
idesDoubleBufferStop (void)
{
	/*
	 * Restore the DMA2D input pixel format to the default.
	 * This is in case the caller is using something besides
	 * the default.
	 */

	dma2dAcquireBusS (&DMA2DD1);

	dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB565);
	dma2dBgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB565);

	dma2dReleaseBusS (&DMA2DD1);

	/*
	 * Ensure correct display frame is restored:
	 * foreground (display 0) on, background (display 1) off.
	 */

	ltdcFgEnableI (&LTDCD1);
	ltdcBgDisableI (&LTDCD1);
	ltdcReload (&LTDCD1, FALSE);

	gdispGClear (d0, GFX_BLACK);
	gdispGClear (d1, GFX_BLACK);

	d0 = d1 = NULL;

	return;
}

void
idesDoubleBufferBlit (gCoord x, gCoord y, gCoord cx, gCoord cy, void * buf)
{
	GDisplay * g;

	/* Make sure we've been initialized */

	if (d0 == NULL || d1 == NULL)
		return;

	/*
	 * Select the correct display frame based on the 'layer'
	 * toggle. Note that we don't actually trigger a reload
	 * yet: that will be done after we do the blit. The
	 * currently displayed frame won't change until the
	 * reload occurs.
	 */

	if (layer == 0) {
		g = (GDisplay *)d1;
		ltdcFgDisableI (&LTDCD1);
		ltdcBgEnableI (&LTDCD1);
	} else {
		g = (GDisplay *)d0;
		ltdcFgEnableI (&LTDCD1);
		ltdcBgDisableI (&LTDCD1);
	}

	/* Blit the data to the screen */

	gdispGBlitArea (g,
		/* Start position */
		x, y,
		/* Size of filled area */
		cx, cy,
		/* Bitmap position to start fill from */
		0, 0,
		/* Width of bitmap line */
		cx,
		/* Bitmap buffer */
		(gPixel *)buf);

	/* Trigger frame swap on next vertical refresh. */

	ltdcStartReloadI (&LTDCD1, FALSE);

	/* Exchange layers */

	layer ^= 1;

	return;
}
