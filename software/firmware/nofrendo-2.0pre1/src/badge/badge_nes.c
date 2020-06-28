
#include <math.h>
#include <string.h>
#include <noftypes.h>
#include <nes_bitmap.h>
#include <nofconfig.h>
#include <event.h>
#include <gui.h>
#include <log.h>
#include <nes.h>
#include <nes_pal.h>
#include <nesinput.h>
#include <osd.h>

#include "ch.h"
#include "hal.h"

#include "gfx.h"
#include "src/gdisp/gdisp_driver.h"
#include "hal_stm32_dma2d.h"
#include "hal_stm32_ltdc.h"

#define roundup(x, y)              ((((x)+((y)-1))/(y))*(y))

#define  DEFAULT_SAMPLERATE   15625
#define  DEFAULT_BPS          16
#define  DEFAULT_FRAGSIZE     1024

#define  DEFAULT_WIDTH        256
#define  DEFAULT_HEIGHT       NES_VISIBLE_HEIGHT

#pragma pack(1)
typedef struct palette_color {
	uint8_t		b;
	uint8_t		g;
	uint8_t		r;
	uint8_t		a;
} palette_color_t;
#pragma pack()

static virtual_timer_t timer;
static vtfunc_t timer_func;
static sysinterval_t timer_delay;

static void timer_cb (void * arg)
{
	chSysLockFromISR();
	timer_func (arg);
	chVTSet (&timer, timer_delay, timer_cb, NULL);
	chSysUnlockFromISR();
}

int
osd_installtimer (int frequency, void *func, int funcsize,
		  void *counter, int countersize)
{
	(void)funcsize;
	(void)counter;
	(void)countersize;

	timer_delay = CH_CFG_ST_FREQUENCY / frequency;
	timer_func = (vtfunc_t)func;
	chVTSet (&timer, timer_delay, timer_cb, NULL);

	return 0;
}


/*
** Audio
*/

void
osd_setsound (void (*playfunc)(void *buffer, int length))
{
	(void)playfunc;
	return;
}

void
osd_getsoundinfo (sndinfo_t *info)
{
#ifdef notdef
	info->sample_rate = sound_samplerate;
	info->bps = sound_bps;
#endif
	info->sample_rate = DEFAULT_SAMPLERATE;
	info->bps = DEFAULT_BPS;
	return;
}

static int
osd_init_sound(void)
{
	return (0);
}

/*
** Video
*/

static int init (int width, int height);
static void shutdown (void);
static int set_mode (int width, int height);
static void set_palette (rgb_t *pal);
static void clear (uint8 color);
static nes_bitmap_t *lock_write(void);
static void free_write(int num_dirties, rect_t *dirty_rects);
static void badge_blit (nes_bitmap_t * , int, rect_t *);

static void * d0;
static void * d1;
static int layer;
static nes_bitmap_t * myBitmap;
static palette_color_t * palettebuf;
static uint8_t fb[1];

viddriver_t sdlDriver =
{
   "Team IDES Badge DirectMedia Layer",         /* name */
   init,          /* init */
   shutdown,      /* shutdown */
   set_mode,      /* set_mode */
   set_palette,   /* set_palette */
   clear,         /* clear */
   lock_write,    /* lock_write */
   free_write,    /* free_write */
   badge_blit,    /* custom_blit */
   false          /* invalidate flag */
};

void
osd_getvideoinfo(vidinfo_t *info)
{
	info->default_width = DEFAULT_WIDTH;
	info->default_height = DEFAULT_HEIGHT;
	info->driver = &sdlDriver;
	return;
}

static int
init (int width, int height)
{
	(void)width;
	(void)height;

	palettebuf = malloc (sizeof(palette_color_t) * 256);

	d0 = (void *)gdriverGetInstance (GDRIVER_TYPE_DISPLAY, 0);
	d1 = (void *)gdriverGetInstance (GDRIVER_TYPE_DISPLAY, 1);

	/* Set pixel format translation */

	dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_L8);

	return (0);
}

static void
shutdown (void)
{
	/* Restore pixel format translation */

	dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB565);

	/* Re-enable the right layer */

	ltdcFgEnableI (&LTDCD1);
	ltdcBgDisableI (&LTDCD1);

	free (palettebuf);

	gdispGClear (d0, GFX_BLACK);
	gdispGClear (d1, GFX_BLACK);

	return;
}

/* set a video mode */
static int
set_mode (int width, int height)
{
	(void)width;
	(void)height;
	return 0;
}

static void
badge_blit (nes_bitmap_t * primary, int num_dirties, rect_t *dirty_rects)
{
	GDisplay * g;

	(void)dirty_rects;

	if (num_dirties == -1) {

		if (layer == 0) {
			g = (GDisplay *)d1;
			ltdcFgDisableI (&LTDCD1);
			ltdcBgEnableI (&LTDCD1);
		} else {
			g = (GDisplay *)d0;
			ltdcFgEnableI (&LTDCD1);
			ltdcBgDisableI (&LTDCD1);
		}

		cacheBufferFlush (primary->data,
		    primary->width * primary->height);

		gdispGBlitArea (g,
			/* Start position */
			32, 0,
			/* Size of filled area */
			primary->width, primary->height,
			/* Bitmap position to start fill from */
			0, 0,
			/* Width of bitmap line */
			primary->width,
			/* Bitmap buffer */
			(gPixel *)primary->data);

		/* Trigger frame swap on next vertical refresh. */

		ltdcStartReloadI (&LTDCD1, FALSE);

		/* Swap layers */

		layer ^= layer;
	}

	return;
}

/* copy nes palette over to hardware */
static void
set_palette (rgb_t * pal)
{
	int i;
	dma2d_palcfg_t palcfg;

	for (i = 0; i < 256; i++) {
		palettebuf[i].a = 0;
		palettebuf[i].r = pal[i].r;
		palettebuf[i].g = pal[i].g;
		palettebuf[i].b = pal[i].b;
	}

	cacheBufferFlush (palettebuf, sizeof(palette_color_t) * 256);

	palcfg.length = 256;
	palcfg.fmt = DMA2D_FMT_ARGB8888;
	palcfg.colorsp = palettebuf;

	dma2dFgSetPalette (&DMA2DD1, &palcfg);

	return;
}

/* clear all frames to a particular color */
static void
clear (uint8 color)
{
	(void)color;

	/*
	 * This is a bit of a hack. We really should use the color
	 * provided to us, but Nofrendo is written so that it doesn't
	 * load the palette until after the clear routine is called.
	 * From testing it's evident that the clear() routine is
	 * only called when starting the emulator, and we always
	 * clear the screen to black, so we assume that's what
	 * will always happen here.
	 */

	dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_RGB565);
	gdispGClear (d0, GFX_BLACK);
	gdispGClear (d1, GFX_BLACK);
	dma2dFgSetPixelFormat (&DMA2DD1, DMA2D_FMT_L8);

	return;
}

/* acquire the directbuffer for writing */
static nes_bitmap_t *
lock_write (void)
{
	myBitmap = bmp_createhw (fb, DEFAULT_WIDTH, DEFAULT_HEIGHT,
	    DEFAULT_WIDTH);
	return myBitmap;
}

/* release the resource */
static void
free_write (int num_dirties, rect_t *dirty_rects)
{
	(void)num_dirties;
	(void)dirty_rects;
	bmp_destroy (&myBitmap);
	return;
}

/*
** Input
*/

static void
osd_initinput(void)
{
	return;
}

void
osd_getinput(void)
{
	event_t ev;

	if (palReadLine(LINE_BUTTON_USER)) {
		ev = event_get (event_quit);
		ev (INP_STATE_MAKE);
	}

	return;
}
void
osd_getmouse(int *x, int *y, int *button)
{
	(void)x;
	(void)y;
	(void)button;
	return;
}

/*
** Shutdown
*/

/* this is at the bottom, to eliminate warnings */
void
osd_shutdown()
{
	chVTReset (&timer);
	return;
}

/*
** Startup
*/

int
osd_init()
{
	if (osd_init_sound())
		return -1;

	osd_initinput();

	chVTObjectInit (&timer);
	chVTReset (&timer);

	return 0;
}
