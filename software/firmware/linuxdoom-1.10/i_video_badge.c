// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include "ch.h"
#include "hal.h"

#include "gfx.h"
#include "ides_gfx.h"
#include "hal_stm32_dma2d.h"
#include "hal_stm32_ltdc.h"
#include "badge_console.h"

#include "capture.h"

#include <stdlib.h>
#include <malloc.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

/*
 * Choose only one of these.
 *
 * The software rescaler is really a hack that just duplicates every 5th
 * scanline in the CLUT index buffer generated by Doom prior to rendering.
 * This does fill out the image to 240 lines, but the result isn't
 * exactly correct.
 *
 * The DMA2D accelerated rescaler actually uses color blending to
 * rescale the rendered output image (with RGB565 pixels), and it's
 * much faster than an equivalent software-only implementation, but
 * it's not as fast as the hack software hack rescaler.
 *
 * We prefer to have the cleanest looking result, so we default to
 * the DMA2D accelerated rescaler.
 */

#undef ASPECT_FIXUP_SOFTWARE
#define ASPECT_FIXUP_DMA2D


#ifdef ASPECT_FIXUP_SOFTWARE

#define STRETCHEDHEIGHT 240
__attribute__((section(".ram7")))
static uint8_t * scaled;

static void I_Rescale (void);
static void I_RescaleInit (void);
static void I_RescaleShutdown (void);

#endif


#ifdef ASPECT_FIXUP_DMA2D

#define STRETCHEDHEIGHT 240

typedef struct rescale_val {
	int		line1;
	int		line2;
	uint8_t		alpha1;
	uint8_t		alpha2;
} RESCALE_VAL;

static void I_Rescale (void);
static void I_RescaleInit (void);
static void I_RescaleShutdown (void);

RESCALE_VAL * pRescaleTable;

#endif


__attribute__((section(".ram7")))
static palette_color_t * palettebuf;
__attribute__((section(".ram7")))
static int buttontmp;

/*
 * These are helper functions which are used to copy
 * scanline data 8 bytes at a time. The STM32F746 processor
 * supports single-precision floating point only. However
 * even so, the double-precision registers can be used to
 * load and store 64-bit quantities. This allows us to
 * take advantage of them to copy scanlines with a few
 * less instructions that if we just use a memcpy().
 */

static inline void
copy_scanline (uint8_t * dst, uint8_t * src)
{
	int i;

	for (i = 0; i < (SCREENWIDTH / 8); i++) {
		__asm__ ("vldr	d0, [%0]\n"
			 "vstr	d0, [%1]"
			 : : "r" (src), "r" (dst) : );
		src += 8;
		dst += 8;
	}

	return;
}

#ifdef ASPECT_FIXUP_SOFTWARE
static inline void
duplicate_scanline (uint8_t * dst, uint8_t * src)
{
	int i;

	for (i = 0; i < (SCREENWIDTH / 8); i++) {
		__asm__ ("vldr	d0, [%0]\n"
			 "vstr	d0, [%1]\n"
			 "vstr	d0, [%1,#320]"
			 : : "r" (src), "r" (dst) : );
		src += 8;
		dst += 8;
	}

	return;
}
#endif

//
//  Translates the key 
//

static int xlatekey(uint32_t sym)
{
 	int rc;

	switch (sym) {
		case CAP_LEFT:   rc = KEY_LEFTARROW;     break;
		case CAP_RIGHT:  rc = KEY_RIGHTARROW;    break;
		case CAP_DOWN:   rc = KEY_DOWNARROW;     break;
		case CAP_UP:     rc = KEY_UPARROW;       break;
		case CAP_ESCAPE: rc = KEY_ESCAPE;        break;
		case CAP_RETURN: rc = KEY_ENTER;         break;
		case CAP_TAB:    rc = KEY_TAB;           break;
		case CAP_F1:     rc = KEY_F1;            break;
		case CAP_F2:     rc = KEY_F2;            break;
		case CAP_F3:     rc = KEY_F3;            break;
		case CAP_F4:     rc = KEY_F4;            break;
		case CAP_F5:     rc = KEY_F5;            break;
		case CAP_F6:     rc = KEY_F6;            break;
		case CAP_F7:     rc = KEY_F7;            break;
		case CAP_F8:     rc = KEY_F8;            break;
		case CAP_F9:     rc = KEY_F9;            break;
		case CAP_F10:    rc = KEY_F10;           break;
		case CAP_F11:    rc = KEY_F11;           break;
		case CAP_F12:    rc = KEY_F12;           break;
        
		case CAP_BACKSPACE:
		case CAP_DELETE: rc = KEY_BACKSPACE;     break;

		case CAP_PAUSE:  rc = KEY_PAUSE;         break;

		case CAP_EQUALS: rc = KEY_EQUALS;        break;

		case CAP_KP_MINUS:
		case CAP_MINUS:  rc = KEY_MINUS;         break;

		case CAP_LSHIFT:
		case CAP_RSHIFT:
			rc = KEY_RSHIFT;
			break;
        
		case CAP_LCTRL:
		case CAP_RCTRL:
			rc = KEY_RCTRL;
			break;
        
		case CAP_LALT:
		case CAP_LMETA:
		case CAP_RALT:
		case CAP_RMETA:
			rc = KEY_RALT;
			break;
        
		default:
			if (sym & 0x40000000)
				rc = 0;
			else
				rc = sym;
			break;
	}

	return (rc);
}

//
// I_ShutdownGraphics
//
void I_ShutdownGraphics (void)
{
	/*
	 * Grrr. Sometimes this function may be called even though
	 * we haven't set up the graphics yet.
	 */

	if (palettebuf == NULL)
		return;

	idesDoubleBufferStop ();

	free (palettebuf);
	if (screens[0] != NULL)
		chHeapFree (screens[0]);
	screens[0] = NULL;
	palettebuf = NULL;

#if defined (ASPECT_FIXUP_SOFTWARE) || defined (ASPECT_FIXUP_DMA2D)
	I_RescaleShutdown ();
#endif

	return;
}


//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

void I_GetEvent(void)
{
	uint32_t	keysym;
	event_t		event;

	if (capture_queue_get (&keysym)) {
		if (CAPTURE_DIR(keysym) == CAPTURE_KEY_UP)
			event.type = ev_keyup;
		if (CAPTURE_DIR(keysym) == CAPTURE_KEY_DOWN)
			event.type = ev_keydown;
		keysym = CAPTURE_CODE(keysym);
		event.data1 = xlatekey (keysym);
		D_PostEvent (&event);
	}

	return; 
}

//
// I_StartTic
//
void I_StartTic (void)
{
	event_t event;
	uint32_t sts;

	I_GetEvent ();

	/*chThdSleep (50);*/
	sts = palReadLine (LINE_BUTTON_USER);

	if (buttontmp == 2 && sts == 1) {
		buttontmp = 0;
		event.type = ev_keydown;
		event.data1 = 'y';
		D_PostEvent(&event);
		event.type = ev_keyup;
		D_PostEvent(&event);
	}

	if (buttontmp == 0 && sts == 1) {
		buttontmp = 1;
		event.type = ev_keydown;
		event.data1 = KEY_F10;
		D_PostEvent(&event);
		event.type = ev_keyup;
		D_PostEvent(&event);
	}

	if (buttontmp == 1 && sts == 0)
		buttontmp = 2;

	return;
}

#ifdef ASPECT_FIXUP_SOFTWARE

static void
I_RescaleInit (void)
{
	scaled = chHeapAlloc (NULL, SCREENWIDTH * STRETCHEDHEIGHT);
	if (scaled == NULL)
		I_Error ("failed to allocate rescale buffer\n");
	return;
}

static void
I_RescaleShutdown (void)
{
	if (scaled != NULL)
		chHeapFree (scaled);
	scaled = NULL;

	return;
}

static void
I_Rescale (void)
{
	uint8_t *	pDst;
	uint8_t *	pSrc;

	pDst = scaled;
	pSrc = screens[0];

	/* Duplicate every 5th scanline */

	for (i = 0; i < SCREENHEIGHT; i++) {
		if ((i % 5) == 0) {
			duplicate_scanline (pDst, pSrc);
			pDst += SCREENWIDTH;
		} else {
			copy_scanline (pDst, pSrc);
		}
		pDst += SCREENWIDTH;
		pSrc += SCREENWIDTH;
	}

	return;
}

#endif

#ifdef ASPECT_FIXUP_DMA2D

static void
I_RescaleInit (void)
{
	int i;
	float interp_line;
	RESCALE_VAL * pCur;
	float alpha;

	/*
	 * Many of the values we need for the rescale operation
	 * are the same for every frame, so we pre-calculate
	 * them here to save a little overhead.
	 */

	pRescaleTable = malloc (sizeof(RESCALE_VAL) * STRETCHEDHEIGHT);
	pCur = pRescaleTable;

	for (i = 0; i < STRETCHEDHEIGHT; i++) {
		interp_line = (float)SCREENHEIGHT * i / STRETCHEDHEIGHT;
		pCur->line1 = floorf (interp_line) * SCREENWIDTH;
		pCur->line2 = ceilf (interp_line) * SCREENWIDTH;
		alpha = interp_line - floorf (interp_line);
		pCur->alpha1 = alpha * 255;
		pCur->alpha2 = (1 - alpha) * 255;
		pCur++;
	}

	/*
	 * Fix up the DMA2D engine with some settings that won't change
	 * while we're running. Normally we only use the foreground
	 * transfer channel for rendering, so we can pre-init the
	 * background channel's pixel format to RGB565. Also, we
	 * normally don't use the alpha channel feature either, so
	 * we can pre-initialize that for both transfer channels
	 * as well.
	 */

	dma2dBgSetPixelFormatI (&DMA2DD1, DMA2D_FMT_RGB565);
	dma2dBgSetAlphaModeI (&DMA2DD1, DMA2D_ALPHA_MODULATE);
	dma2dFgSetAlphaModeI (&DMA2DD1, DMA2D_ALPHA_MODULATE);

	return;
}

static void
I_RescaleShutdown (void)
{
	if (pRescaleTable != NULL) {
		free (pRescaleTable);
		pRescaleTable = NULL;
	}

	return;
}

static void
I_Rescale (void)
{
	int y;
	uint16_t * pDst;
	uint16_t * pSrc;
	RESCALE_VAL * pCur;

	/*
	 * So, at every tic, Doom's graphics engine produces a 320x200
	 * buffer of 8-bit color lookup table (CLUT) indexes. We pass
	 * this buffer along with the associated palette (the lookup
	 * table itself) to the DMA2D engine, which magically produces
	 * a frame of RGB565 pixels that's deposited in one of the two
	 * available frame buffers.
	 *
	 * The problem is that we want to rescale this 320x200 frame
	 * into a 320x240 frame. Sadly, the DMA2D engine doesn't
	 * support hardware rescaling. But it does support blending,
	 * so there is a way to take advantage of it.
	 *
	 * Rescaling would work like this:
	 *
	 * - You have an input buffer with 200 lines and an output
	 *   buffer with 240 lines
	 * - Each line is 320 pixels long
	 * - For each line in the output buffer, choose the two closest
	 *   lines in the input buffer.
	 * - Then for all 320 offsets, take the pixels of the two input
	 *   lines and blend them together, then put the resulting pixel
	 *   at the same offset in the output line.
	 *
	 * In other words, for every output line, you average together
	 * the pixels from the two closes input lines.
	 *
	 * We accomplish this using the following steps:
	 *
 	 * - First, we perform the usual DMA2D render step to draw
	 *   a frame of 320x200 RGB565 pixels in the display frame buffer
	 *   that is not currently visible. We use a special blit function		 *   for this that moves the data, but which does not swap frame
	 *   buffers just yet.
	 *
	 * - The frame is rendered at the bottom of the frame buffer. This
	 *   means it extends from lines 40 to 239. The first 40 lines
	 *   are left untouched.
	 *
	 * - We set the DMA2D forground buffer pointer register to the
	 *   address of line 40 and the background buffer pointer register
	 *   to the address of line 41. We also set the output buffer
	 *   pointer register to the address of line 0.
	 *
	 * - Then we trigger a blend operation, which merges lines 40
	 *   and 41 together and puts the result at line 0.
	 *
	 * - We then repeat this process to generate 240 output lines.
	 *
	 * The rescaled image is built starting from the top of the
	 * frame buffer and working down, consuming the input lines
	 * starting at line 40 as it goes. This allows the rescaling
	 * to be done in place and avoids the need to allocate a
	 * separate output buffer.
	 *
	 * Later, once this operation is complete, we trigger the
	 * display buffer switch which will make the newly rescaled
	 * image visible during the next vertical refresh interval.
	 */

	dma2dAcquireBusS (&DMA2DD1);

	/* These only need to be set once */

	dma2dJobSetSizeI (&DMA2DD1, SCREENWIDTH, 1);
	dma2dJobSetModeI (&DMA2DD1, DMA2D_JOB_BLEND);
	dma2dFgSetPixelFormatI (&DMA2DD1, DMA2D_FMT_RGB565);

	pCur = pRescaleTable;
	pSrc = pDst = idesDoubleBufferAddr ();
	pSrc += SCREENWIDTH * (STRETCHEDHEIGHT - SCREENHEIGHT);

	/* Don't bother rescaling the last 2 lines, there's no point. */

	for (y = 0; y < STRETCHEDHEIGHT - 2; y++) {
		while (dma2dJobIsExecutingI (&DMA2DD1))
			chSchDoYieldS ();

		dma2dOutSetAddressI (&DMA2DD1, pDst);
		dma2dFgSetAddressI (&DMA2DD1, pSrc + pCur->line1);
		dma2dBgSetAddressI (&DMA2DD1, pSrc + pCur->line2);
		dma2dFgSetConstantAlphaI (&DMA2DD1, pCur->alpha2);
		dma2dBgSetConstantAlphaI (&DMA2DD1, pCur->alpha1);

		dma2dJobStartI (&DMA2DD1);

		pDst += SCREENWIDTH;
		pCur++;
	}

	while (dma2dJobIsExecutingI (&DMA2DD1))
		chSchDoYieldS ();

	dma2dFgSetPixelFormatI (&DMA2DD1, DMA2D_FMT_L8);

	dma2dReleaseBusS (&DMA2DD1);

	return;
}

#endif


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
__attribute__((section(".ram7")))
	static int	lasttic;
	int		tics;
	int		i;

	if (palettebuf == NULL)
		return;

	/* draws little dots on the bottom of the screen */

	if (devparm) {
		i = I_GetTime();
		tics = i - lasttic;
		lasttic = i;
		if (tics > 20) tics = 20;

		for (i=0 ; i<tics*2 ; i+=2)
	    		screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
		for ( ; i<20*2 ; i+=2)
	    		screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    	}

	/*
	 * Back in the day, the MS-DOS version of Doom used a VGA display
	 * mode known as VGA mode 13h (also known as mode Y, which
	 * allowed for a resolution of 320x200 pixels and 256 colors.
	 * This is why Doom's engine uses this resolution when producing
	 * graphics frames.
	 *
	 * This resolution implies a somewhat oddball aspect ratio of 4:2.5.
	 * But VGA CRT displays had an actual physical aspect ratio of 4:3.
	 * Despite this apparent mismatch, a 320x200 frame in VGA mode 13h
	 * would still fill the entire screen. This happened because in
	 * this mode, the displayed pixels were not square: they were a
	 * little taller than they were wide. The result was that the
	 * displayed image was stretched a little vertically to fit the
	 * whole screen.
	 *
	 * It turns out that the various monster sprites and other
	 * graphics in Doom (like Doomguy's face in the HUD) were drawn to
	 * take this into account. That is, they really only look correct
	 * with the pixel stretching applied.
	 *
	 * Meanwhile, our display hardware supports a resolution of 320x240
	 * with perfectly square pixels. This means our screen does have
 	 * a 4:3 aspect ratio just like an old school VGA display, but if we
	 * simply draw the 320x200 frames produced by Doom's renderer on it,
	 * they will be 40 pixels too short and appear a little squashed.
	 *
	 * We would prefer to correct this so that Doom can truly be
	 * appreciated in the original Klingon, as it were.
	 *
	 * There are two possible solutions. The first is a quick hack,
	 * which is to take the 320x200 CLUT index buffer output by Doom
	 * and copy it into a new buffer, duplicating every 5th scan line.
	 * We try to me a little clever by using the 64-bit FPU registers
	 * to speed up the copying from the original display buffer into
	 * the rescale buffer. This is fast, but the results are less than
	 * ideal. It also requires allocating extra memory for the 320x240
	 * CLUT index buffer.
	 *
	 * The other solution is to take advantage of the fact that the
	 * DMA2D engine in the STM32F746 CPU can do color blending. Using
	 * this we can fix up the RGB565 pixels in the fully rendered
	 * output buffer in a way that's faster than if we were to do
	 * the same thing entirely in software. We can also use a bit of
	 * a hack to rescale the image in place so that we don't need to
	 * allocate any extra frame buffer memory. It's still not as fast
	 * as the quick hack method, but it's fast enough that it doesn't
	 * slow down the game, and the result is much nicer.
	 */

#if defined(ASPECT_FIXUP_SOFTWARE)

	I_Rescale ();

	idesDoubleBufferBlit (0, 0, SCREENWIDTH, STRETCHEDHEIGHT, scaled);

#elif defined(ASPECT_FIXUP_DMA2D)

	idesDoubleBufferNoReloadBlit (0, 40, SCREENWIDTH,
	    SCREENHEIGHT, screens[0]);

	I_Rescale ();
 
	idesDoubleBufferReload ();

#else /* No rescaling, just letterboxing */

	idesDoubleBufferBlit (0, 20, SCREENWIDTH, SCREENHEIGHT, screens[0]);

#endif

	return;
}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
	int		i;
	uint8_t *	pSrc;
	uint8_t *	pDst;

	pDst = scr;
	pSrc = screens[0];

	/* Use the fast copy scheme here */

	for (i = 0; i < SCREENHEIGHT; i++) {
		copy_scanline (pDst, pSrc);
		pDst += SCREENWIDTH;
		pSrc += SCREENWIDTH;
	}

	return;
}

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
	int i;
	int c;

	if (palettebuf == NULL) {
		/* Go home Doom, you're drunk. */
		return;
	}

	for (i = 0; i < 256; i++) {
		palettebuf[i].a = 0;
		c = gammatable[usegamma][*palette++];
		palettebuf[i].r = c;
		c = gammatable[usegamma][*palette++];
		palettebuf[i].g = c;
		c = gammatable[usegamma][*palette++];
		palettebuf[i].b = c;
	}

	idesDoubleBufferPaletteLoad (palettebuf);

	return;
}


void I_InitGraphics(void)
{
	uint32_t sym;

	badge_condestroy ();

	/* Allocate pallete */

	palettebuf = malloc (sizeof(palette_color_t) * 256);

	idesDoubleBufferInit (IDES_DB_L8);

	buttontmp = 0;

	/*
	 * Allocate the foreground screen buffer. We put it
	 * in the CPU's internal SRAM for better performance,
	 * since access time for the SRAM is faster than that of
	 * the external SDRAM.
	 */

	screens[0] = chHeapAlloc (NULL, (SCREENWIDTH * SCREENHEIGHT));
	if (screens[0] == NULL)
		I_Error ("failed to allocate foreground buffer\n");
	memset (screens[0], 0, (SCREENWIDTH * SCREENHEIGHT));

#if defined (ASPECT_FIXUP_SOFTWARE) || defined (ASPECT_FIXUP_DMA2D)
	I_RescaleInit ();
#endif

	/* Drain the input queue */

	while (capture_queue_get (&sym)) {
	}

	return;
}
