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

#include "gfx.h"
#include "ides_gfx.h"
#include "src/gdisp/gdisp_driver.h"

#include "video_lld.h"
#include "async_io_lld.h"
#include "stm32sai_lld.h"
#include "hal_stm32_ltdc.h"

#include "badge.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>

#include "jpeglib.h"

static struct jpeg_decompress_struct cinfo;
static struct jpeg_error_mgr jerr;

/*
 * We put the video scanline buffers in the on-board SRAM
 * to save heap space. We can get away with this since the
 * size of this buffer won't vary.
 */

__attribute__((aligned(CACHE_LINE_SIZE)))
static uint8_t frame[320 * 2 * VID_PIXEL_SIZE];

#undef ROTATE

#ifdef ROTATE
__attribute__((aligned(CACHE_LINE_SIZE)))
static uint8_t outframe[320 * 2 * VID_PIXEL_SIZE];
#endif

/*
 * This dummy error exit handler is provided so that the libjpeg
 * library doesn't try to call exit() if it encounters an error
 * while decoding a frame. The newlib exit() function will spin
 * forever; we'd rather avoid that and have the video player
 * routine just return.
 */

static void
videoErrorExit (j_common_ptr cinfo)
{
	(void)cinfo;
	return;
}

#ifdef ROTATE
#define LINE_SHORT	(320 - 240)
#define LINE_FULL	(320)
#define LINE_GAP	(LINE_SHORT / 2)

static void
videoXForm (void)
{
	uint8_t * pSrc1;
	uint8_t * pSrc2;
	uint8_t * pDst1;
	uint8_t * pDst2;
	int i;

	pSrc1 = &frame[(LINE_FULL * VID_PIXEL_SIZE) -
	    (LINE_GAP * VID_PIXEL_SIZE) - VID_PIXEL_SIZE];
	pDst1 = &outframe[0];

	pSrc2 = &frame[(LINE_FULL * VID_PIXEL_SIZE * 2) -
	    (LINE_GAP * VID_PIXEL_SIZE) - VID_PIXEL_SIZE];
	pDst2 = &outframe[VID_PIXEL_SIZE];

	for (i = 0; i < 320 - LINE_GAP; i++) {
		pDst1[0] = pSrc1[0];
		pDst1[1] = pSrc1[1];
		pDst1[2] = pSrc1[2];
		pDst1 += 6;
		pSrc1 -= 3;
		pDst2[0] = pSrc2[0];
		pDst2[1] = pSrc2[1];
		pDst2[2] = pSrc2[2];
		pDst2 += 6;
		pSrc2 -= 3;
	}

	return;
}
#endif

static void
videoFrameDecompress (uint8_t * in, size_t len, GDisplay * g)
{
	unsigned char * buffer_array[2];
	jpeg_mem_src (&cinfo, in, len);
	jpeg_read_header (&cinfo, TRUE);
 	cinfo.out_color_space = JCS_RGB;
	gCoord xlen;

	xlen = gdispGetWidth ();

	jpeg_start_decompress (&cinfo);

	while (cinfo.output_scanline < cinfo.output_height) {

		/*
		 * We can actually get the jpeg library to
		 * reliably process up to 2 scan lines at once,
		 * so we do that here to cut down how many calls
		 * we make to jpeg_read_scalines().
		 */

		buffer_array[0] = frame;
		buffer_array[1] = frame;
		buffer_array[1] += xlen * VID_PIXEL_SIZE;
		jpeg_read_scanlines (&cinfo, buffer_array, 2);

		/*
		 * Note: When we use RGB888 mode, our pixels are
		 * 3 bytes in size, but the uGFX display driver still
		 * thinks we're using RGB565 where pixels are only
		 * 2 bytes in size. Because of this, its cache flush
		 * code doesn't flush the entire frame buffer. We need
		 * to compensate for this here, otherwise portions
		 * of the frame may appear corrupted.
		 */

#ifdef ROTATE
		videoXForm ();

		cacheBufferFlush (outframe + (xlen * 2),
		    (xlen * VID_PIXEL_SIZE) - (xlen * 2));

		gdispGBlitArea (g,
		    /* Start position */
		    LINE_GAP + (cinfo.output_scanline - 2), 0,
		    /* Size of filled area */
		    2, 240,
		    /* Bitmap position to start fill from */
		    0, 0,
		    /* Width of bitmap line */
		    2,
		    /* Bitmap buffer */
		    (gPixel *)outframe);
#else
		cacheBufferFlush (frame + (xlen * 2),
		    (xlen * VID_PIXEL_SIZE) - (xlen * 2));

		gdispGBlitArea (g,
		    0, (cinfo.output_scanline - 2),
		    xlen, 2,
		    0, 0,
		    xlen,
		    (gPixel *)frame);
#endif
	}

	jpeg_finish_decompress (&cinfo);

	return;
}

int
videoPlay (char * path)
{
	CHUNK_HEADER _ch;
	CHUNK_HEADER * ch;
	uint8_t * buf;
	uint8_t * p1;
	uint8_t * p2;
	uint8_t * s;
	int f;
	size_t sz;
	int br;
	size_t max;
	GDisplay * g0;
	GDisplay * g1;
	uint8_t toggle = 0;
	GListener gl;
	GSourceHandle gs;
	GEventMouse * me = NULL;

	f = open (path, O_RDONLY, 0);

	if (f == -1) {
		printf ("opening [%s] failed\n", path);
		return (-1);
	}

	i2sPlay (NULL);

	gs = ginputGetMouse (0);
	geventListenerInit (&gl);
	geventAttachSource (&gl, gs, GLISTEN_MOUSEMETA);

	g0 = (GDisplay *)gdriverGetInstance (GDRIVER_TYPE_DISPLAY, 0);
	g1 = (GDisplay *)gdriverGetInstance (GDRIVER_TYPE_DISPLAY, 1);

	gdispGClear (g0, GFX_BLACK);
	gdispGClear (g1, GFX_BLACK);

	/*
	 * Override the input pixel format used by the DMA2D
	 * engine. The standard libjpeg library doesn't support
	 * RGB565 as an output pixel format. However we can
	 * can use the DMA2D engine to convert from RGB888 to
	 * RGB565 in hardware, and it'll be faster than doing
	 * the RGB565 conversion in software (at the cost of
	 * of using a little more memory).
	 */

	idesDoubleBufferInit (IDES_DB_RGB888);

	ch = &_ch;

	br = read (f, ch, sizeof(CHUNK_HEADER));

	max = (ch->cur_vid_size + ch->cur_aud_size) + sizeof(CHUNK_HEADER);

	buf = memalign (CACHE_LINE_SIZE, (max * 2));

	p1 = buf;
	p2 = buf + max;

	asyncIoInit ();

	/*
	 * Create a jpeg decompression context and read
	 * the first frame from the SD card.
	 */

	cinfo.err = jpeg_std_error (&jerr);
	jerr.error_exit = videoErrorExit;
	jpeg_create_decompress (&cinfo);

	br = read (f, p1, (size_t)ch->next_chunk_size);

	while (1) {
		ch = (CHUNK_HEADER *)p1;
		sz = ch->next_chunk_size;
		if (sz == 0)
			break;
		asyncIoRead (f, p2, sz, &br);

		sz = ch->cur_vid_size;
		s = p1 + sz + sizeof(CHUNK_HEADER);

		/*
		 * Start the audio samples for this frame playing.
		 * The audio playback is done via DMA, so it will occur
		 * at the same time that the CPU is decompressing and
		 * displaying the video for this frame.
		 */

		i2sSamplesPlay (s, (int)ch->cur_aud_size >> 1);

		/*
		 * Now decompress and blit the frame to the screen.
		 * Internally the uGFX library will use the DMA2D engine
		 * for this so it should be super fast.
		 *
	 	 * In order to avoid any flickering artifacts, we
		 * take advantage of the fact that the display controller
		 * has two layers. One layer is set to be visible while
		 * the other is hidden. We always draw the latest frame
		 * to the invisible layer, and then swap them.
		 */

		s = p1 + sizeof(CHUNK_HEADER);

		if (toggle) {
			videoFrameDecompress (s, sz, g1);
			ltdcFgDisableI (&LTDCD1);
			ltdcBgEnableI (&LTDCD1);
			p1 = buf;
			p2 += max;
		} else {
			videoFrameDecompress (s, sz, g0);
			ltdcFgEnableI (&LTDCD1);
			ltdcBgDisableI (&LTDCD1);
			p1 += max;
			p2 = buf;
		}

		/*
		 * Flip the visible layer. The register updates above
		 * won't take effect until we write to the control
		 * register here. The switch will occur during the next
		 * veritical blanking interval. (Doing it immediately
		 * causes flicker.)
	 	 */

		ltdcStartReloadI (&LTDCD1, FALSE);
		toggle = ~toggle;

		asyncIoWait ();

		if (br == 0)
			break;

		/*
		 * Wait for samples to finish playing. Ideally it should
		 * take us a little less time to decompress and display a
		 * frame than it does for us to play the samples. Waiting
		 * for the audio to complete keeps us in sync and keeps
		 * the playback running at the correct rate.
		 */

		i2sSamplesWait ();

		me = (GEventMouse *)geventEventWait (&gl, 0);
		if (me != NULL && me->buttons & GMETA_MOUSE_DOWN)
			break;
	}

	jpeg_destroy_decompress (&cinfo);

	/* Drain any pending I/O */

	i2sSamplesWait ();
	i2sSamplesStop ();

	close (f);

	/* Free memory */

	free (buf);

	idesDoubleBufferStop ();

	geventDetachSource (&gl, NULL);

	if (me != NULL && me->buttons & GMETA_MOUSE_DOWN)
		return (-1);

	return (0);
}
