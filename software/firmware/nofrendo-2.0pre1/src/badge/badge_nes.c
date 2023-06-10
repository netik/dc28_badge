
#include <math.h>
#include <string.h>
#include <malloc.h>
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
#include <vid_drv.h>

#include "ch.h"
#include "hal.h"

#include "gfx.h"
#include "src/gdisp/gdisp_driver.h"
#include "ides_gfx.h"
#include "hal_stm32_dma2d.h"
#include "hal_stm32_ltdc.h"
#include "stm32sai_lld.h"

#include "capture.h"

#define  DEFAULT_SAMPLERATE   22050
#define  DEFAULT_BPS          16
/*
 * The rule is that the time it takes to play all the audio samples
 * for a single frame of video should be slightly longer than the
 * time it takes to draw the frame. The emulator runs at a frame rate
 * of NES_REFRESH_RATE, which is 50 or 60 depending on whether it's
 * set for PAL or NTSC. (We select NTSC.) The amount of audio samples
 * we need to play is therefore roughly equal to the audio sample
 * rate (22050) divided by the frame rate (60). However we need to
 * throw in some extra samples to make the audio play time slighly
 * longer, perhaps by about 500 microseconds (half a millisecond).
 * With a sample rate of 22050, that works out to about 11 extra
 * samples.
 */
#define  DEFAULT_FRAGSIZE     ((DEFAULT_SAMPLERATE/NES_REFRESH_RATE) + 11)

#define  DEFAULT_WIDTH        256
#define  DEFAULT_HEIGHT       NES_VISIBLE_HEIGHT

#define  GPT_FREQ             2000000

static gptcallback_t timer_func;

static void
timer_cb (GPTDriver * gpt)
{
	(void)gpt;
	chSysLockFromISR();
	timer_func (gpt);
	__DSB();
	chSysUnlockFromISR();
}

static const GPTConfig gptcfg =
{
	GPT_FREQ,	/* 2MHz timer clock.*/
	timer_cb,	/* Timer callback function. */
	0,
	0
};

int
osd_installtimer (int frequency, void *func, int funcsize,
		  void *counter, int countersize)
{
	(void)funcsize;
	(void)counter;
	(void)countersize;
	timer_func = (gptcallback_t)func;
	gptStartContinuous (&GPTD5, (GPT_FREQ / frequency) + 1);
	return 0;
}

/*
 * blits a bitmap onto primary buffer
 * This is a custom replacement for the generic vid_blit() function in
 * vid_drv.c. This version uses the DMA2D engine as a hardware blitter.
 */
void vid_blit(nes_bitmap_t *bitmap, int src_x, int src_y,
              int dest_x, int dest_y, int width, int height)
{
	uint8_t * src;
	uint8_t * dst;
	nes_bitmap_t * primary_buffer;

	primary_buffer = vid_getbuffer ();

	src = (uint8_t *)bitmap->line[src_y] + src_x;
	dst = (uint8_t *)primary_buffer->line[dest_y] + dest_x;

	/* Make sure to flush the cache here */

	cacheBufferFlush (src, bitmap->pitch * height);

	dma2dAcquireBusS (&DMA2DD1);

	dma2dFgSetAddressI (&DMA2DD1, (void *)src);
	dma2dFgSetWrapOffsetI (&DMA2DD1, bitmap->pitch - width);
	dma2dOutSetAddressI (&DMA2DD1, (void *)dst);
	dma2dOutSetWrapOffsetI (&DMA2DD1, primary_buffer->pitch - width);
	dma2dOutSetPixelFormatI (&DMA2DD1, DMA2D_FMT_L8);
	dma2dJobSetSizeI (&DMA2DD1, width, height);
	dma2dJobSetModeI (&DMA2DD1, DMA2D_JOB_COPY);
	dma2dJobExecute (&DMA2DD1);

	/* Restore output pixel type. */

	dma2dOutSetPixelFormatI (&DMA2DD1, DMA2D_FMT_RGB565);

	dma2dReleaseBusS (&DMA2DD1);

	return;
}

/*
** Audio
*/

/*
 * We put the sample buffer in the BSS so that it will end
 * up in the DTCM RAM where we won't need to flush/invalidate it.
 * and to hopefully get a small performance improvement in
 * the code that generates the samples.
 */

static uint16_t * audio_samples0;
static uint16_t * audio_samples1;
static uint16_t * audio_samples;
static void (*audio_callback)(void *buffer, int length);

void
osd_setsound (void (*playfunc)(void *buffer, int length))
{
	audio_callback = playfunc;
	return;
}

void
osd_getsoundinfo (sndinfo_t *info)
{
	info->sample_rate = DEFAULT_SAMPLERATE;
	info->bps = DEFAULT_BPS;
	return;
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

static nes_bitmap_t * myBitmap;
static palette_color_t * palettebuf;
static uint8_t fb[1];

viddriver_t sdlDriver =
{
	"Team IDES Badge DirectMedia Layer",	/* name */
	init,					/* init */
	shutdown,				/* shutdown */
	set_mode,				/* set_mode */
	set_palette,				/* set_palette */
	clear,					/* clear */
	lock_write,				/* lock_write */
	free_write,				/* free_write */
	badge_blit,				/* custom_blit */
	false					/* invalidate flag */
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

	idesDoubleBufferInit (IDES_DB_L8);

	return (0);
}

static void
shutdown (void)
{
	idesDoubleBufferStop ();

	free (palettebuf);

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
	(void)dirty_rects;

	if (num_dirties == -1) {
		idesDoubleBufferBlit (32, 0, primary->width,
		    primary->height, primary->data);
	}

	/* Grab the audio samples and start them playing */

	audio_callback (audio_samples, DEFAULT_FRAGSIZE);
	i2sSamplesWait ();
	i2sSamplesPlay (audio_samples, DEFAULT_FRAGSIZE);

	if (audio_samples == audio_samples0)
		audio_samples = audio_samples1;
	else
		audio_samples = audio_samples0;

	return;
}

/* copy nes palette over to hardware */

static void
set_palette (rgb_t * pal)
{
	int i;

	for (i = 0; i < 256; i++) {
		palettebuf[i].a = 0;
		palettebuf[i].r = pal[i].r;
		palettebuf[i].g = pal[i].g;
		palettebuf[i].b = pal[i].b;
	}

	idesDoubleBufferPaletteLoad (palettebuf);

	return;
}

/* clear all frames to a particular color */
static void
clear (uint8 color)
{
	void * d;

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
	d = (void *)gdriverGetInstance (GDRIVER_TYPE_DISPLAY, 0);
	gdispGClear (d, GFX_BLACK);
	d = (void *)gdriverGetInstance (GDRIVER_TYPE_DISPLAY, 1);
	gdispGClear (d, GFX_BLACK);
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

static int buttontmp = 0;

#define CAP_LAST 256

static int key_array[CAP_LAST];
#define LOAD_KEY(key, def_key) \
key_array[key & 0xFFFF] = def_key

static void
osd_initinput (void)
{
	uint32_t sym;

	buttontmp = 0;

	LOAD_KEY(CAP_ESCAPE, event_quit);
   
	LOAD_KEY(CAP_F1, event_soft_reset);
	LOAD_KEY(CAP_F2, event_hard_reset);
	LOAD_KEY(CAP_F3, event_gui_toggle_fps);
	LOAD_KEY(CAP_F4, event_snapshot);
	LOAD_KEY(CAP_F5, event_state_save);
	LOAD_KEY(CAP_F6, event_toggle_sprites);
	LOAD_KEY(CAP_F7, event_state_load);
	LOAD_KEY(CAP_F8, event_none);
	LOAD_KEY(CAP_F9, event_none);
	LOAD_KEY(CAP_F10, event_osd_1);
	LOAD_KEY(CAP_F11, event_none);
	LOAD_KEY(CAP_F12, event_none);

	LOAD_KEY(CAP_BACKSPACE, event_gui_display_info);

	LOAD_KEY(CAP_LCTRL, event_joypad1_b);
	LOAD_KEY(CAP_RCTRL, event_joypad1_b);
	LOAD_KEY(CAP_LALT, event_joypad1_a);
	LOAD_KEY(CAP_RALT, event_joypad1_a);
	LOAD_KEY(CAP_LSHIFT, event_joypad1_a);
	LOAD_KEY(CAP_RSHIFT, event_joypad1_a);

	LOAD_KEY(CAP_UP, event_joypad1_up);
	LOAD_KEY(CAP_DOWN, event_joypad1_down);
	LOAD_KEY(CAP_LEFT, event_joypad1_left);
	LOAD_KEY(CAP_RIGHT, event_joypad1_right);

	LOAD_KEY('z', event_joypad1_b);
	LOAD_KEY('x', event_joypad1_a);
	LOAD_KEY('c', event_joypad1_select);
	LOAD_KEY('v', event_joypad1_start);
	LOAD_KEY('b', event_joypad2_b);
	LOAD_KEY('n', event_joypad2_a);
	LOAD_KEY('m', event_joypad2_select);

	/* Make sure the input queue is empty. */

	while (capture_queue_get (&sym)) {
	}

	return;
}

void
osd_getinput (void)
{
	event_t ev;
	uint32_t sym;
	int code;

	/* Process virtual keyboard events */

	if (capture_queue_get (&sym)) {
		if (CAPTURE_DIR(sym) == CAPTURE_KEY_UP)
			code = INP_STATE_BREAK;
		if (CAPTURE_DIR(sym) == CAPTURE_KEY_DOWN)
			code = INP_STATE_MAKE;
		ev = event_get (key_array[sym & 0xFFFF]);
		if (ev)
			ev (code);
	}

	if (buttontmp) {
		buttontmp = 0;
#ifdef NES_SIMULATE_START
		ev = event_get (event_joypad1_start);
#else
		ev = event_get (event_quit);
#endif
		ev (INP_STATE_BREAK);
	}

	if (buttontmp == 0 && palReadLine(LINE_BUTTON_USER) == 1) {
		buttontmp = 1;
#ifdef NES_SIMULATE_START
		ev = event_get (event_joypad1_start);
#else
		ev = event_get (event_quit);
#endif
		ev (INP_STATE_MAKE);
	}

	return;
}
void
osd_getmouse (int *x, int *y, int *button)
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
osd_shutdown (void)
{
	gptStopTimer (&GPTD5);
	gptStop (&GPTD5);
	saiStereo (&SAID2, TRUE);
	saiSpeed (&SAID2, I2S_SPEED_NORMAL);

	free (audio_samples0);
	free (audio_samples1);

	return;
}

/*
** Startup
*/

int
osd_init (void)
{
	audio_samples0 = memalign (CACHE_LINE_SIZE,
	    DEFAULT_FRAGSIZE * sizeof(uint16_t));
	audio_samples1 = memalign (CACHE_LINE_SIZE,
	    DEFAULT_FRAGSIZE * sizeof(uint16_t));

	audio_samples = audio_samples0;

	saiStereo (&SAID2, FALSE);
	saiSpeed (&SAID2, I2S_SPEED_NES);
	gptStop (&GPTD5);
	gptStart (&GPTD5, &gptcfg);

	osd_initinput();

	return 0;
}
