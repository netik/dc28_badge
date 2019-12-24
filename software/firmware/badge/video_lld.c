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
#include "src/gdisp/gdisp_driver.h"
#include "drivers/gdisp/STM32LTDC/stm32_dma2d.h"

#include "video_lld.h"
#include "async_io_lld.h"
#include "stm32sai_lld.h"

#include "badge.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "jpeglib.h"

static struct jpeg_decompress_struct cinfo;
static struct jpeg_error_mgr jerr;

static void
videoFrameDecompress (uint8_t * in, uint16_t * out, size_t len)
{
	unsigned char * buffer_array[2];

	jpeg_mem_src (&cinfo, in, len);
	jpeg_read_header (&cinfo, TRUE);
 	cinfo.out_color_space = JCS_RGB;

	jpeg_start_decompress (&cinfo);

	while (cinfo.output_scanline < cinfo.output_height) {

		/*
		 * We can actually get the jpeg library to
		 * reliably process up to 2 scan lines at once,
		 * so we do that here to cut down how many calls
		 * we make to jpeg_read_scalines().
		 */

		buffer_array[0] = (unsigned char *)out;
		buffer_array[0] += cinfo.output_scanline *
		    cinfo.output_width * VID_PIXEL_SIZE;
		buffer_array[1] = buffer_array[0];
		buffer_array[1] += cinfo.output_width * VID_PIXEL_SIZE;
		jpeg_read_scanlines (&cinfo, buffer_array, 2);
	}

	jpeg_finish_decompress (&cinfo);

	return;
}

int
videoPlay (char * path)
{
	CHUNK_HEADER _ch;
	CHUNK_HEADER * ch;
	uint16_t * frame;
	uint8_t * buf;
	uint8_t * p1;
	uint8_t * p2;
	uint8_t * s;
	int f;
	size_t sz;
	int br;
	size_t max;

	f = open (path, O_RDONLY, 0);

	if (f == -1) {
		printf ("opening [%s] failed\n", path);
		return (-1);
	}

	i2sPlay (NULL);

	/*
	 * Override the input pixel format used by the DMA2D
	 * engine. It turns out that the libjpeg-turbo library
	 * performs faster when we tell it to generate RGB pixels
	 * instead of RGB565 pixels. We can use the DMA2D to
	 * translate the pixel format in hardware and it'll be
	 * faster than doing the RGB565 conversion in software
	 * (at the cost of using a little more memory).
	 */

	DMA2D->FGPFCCR = FGPFCCR_CM_RGB888;

	ch = &_ch;

	br = read (f, ch, sizeof(CHUNK_HEADER));

	max = (ch->cur_vid_size + ch->cur_aud_size) + sizeof(CHUNK_HEADER);

	buf = malloc ((max * 2) + CACHE_LINE_SIZE);
	frame = malloc ((320 * 240 * VID_PIXEL_SIZE) + CACHE_LINE_SIZE);

	p1 = buf;
	p2 = buf + max;

	/*
	 * Create a jpeg decompression context and read
	 * the first frame from the SD card.
	 */

	cinfo.err = jpeg_std_error (&jerr); 
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

		i2sSamplesPlay (s, (int)ch->cur_aud_size);

		/* Now decompress the frame */

		s = p1 + sizeof(CHUNK_HEADER);

		videoFrameDecompress (s, frame, sz);

		/*
		 * Now blit the frame to the screen. Interally the
		 * uGFX library will use the DMA2D engine for this,
		 * so it should be super fast.
		 *
		 * Note: When we use RGB888 mode, our pixels are
		 * 3 bytes in size, but the uGFX display driver still
		 * thinks we're using RGB565 where pixels are only
		 * 2 bytes in size. Because of this, its cache flush
		 * code doesn't flush the entire frame buffer. We need
		 * to compensate for this here, otherwise the last few
		 * lines in the frame may appear corrupted.
		 */

		cacheBufferFlush (frame + (320 * 240),
		    (320 * 80 * VID_PIXEL_SIZE));
		gdispBlitArea (80, 16, 320, 240, frame);

		if (br == 0)
			break;

		if (p1 == buf) {
			p1 += max;
			p2 = buf;
		} else {
			p1 = buf;
			p2 += max;
		}

		asyncIoWait ();

		/*
		 * Wait for samples to finish playing. Ideally it should
		 * take us a little less time to decompress and display a
		 * frame than it does for us to play the samples. Waiting
		 * for the audio to complete keeps us in sync and keeps
		 * the playback running at the correct rate.
		 */

		i2sSamplesWait ();
	}

	jpeg_destroy_decompress (&cinfo);

	/* Drain any pending I/O */

	i2sSamplesWait ();
	i2sSamplesStop ();

	close (f);

	/* Free memory */

	free (buf);
	free (frame);

	/* Restore the DMA2D input pixel format. */

	DMA2D->FGPFCCR = FGPFCCR_CM_RGB565;

	return (0);
}
