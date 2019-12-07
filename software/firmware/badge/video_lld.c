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

#include "ff.h"

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
	FIL f;
	UINT br;
	size_t max;
	unsigned char * buffer_array[2];

	if (f_open(&f, path, FA_READ) != FR_OK) {
		printf ("opening [%s] failed\n", path);
		return (-1);
	}

	i2sPlay (NULL);

	ch = &_ch;

	f_read (&f, ch, sizeof(CHUNK_HEADER), &br);

	max = (ch->cur_vid_size + ch->cur_aud_size) + sizeof(CHUNK_HEADER);

	buf = malloc (max * 2);
	frame = malloc (320 * 240 * 2);

	p1 = buf;
	p2 = buf + max;

	/* Create a jpeg decompression context and read the first frame */

	cinfo.err = jpeg_std_error (&jerr); 
	jpeg_create_decompress (&cinfo);

	f_read (&f, p1, (UINT)ch->next_chunk_size, &br);

	while (1) {
		UINT sz;
		ch = (CHUNK_HEADER *)p1;
		sz = ch->next_chunk_size;
		if (sz == 0)
			break;
		asyncIoRead (&f, p2, sz, &br);
		s = p1 + ch->cur_vid_size + sizeof(CHUNK_HEADER);

		/*
		 * Start the audio samples for this frame playing.
		 * The audio playback is done via DMA, so it will occur
		 * at the same time that the CPU is decompressing and
		 * displaying the video for this frame.
		 */

		i2sSamplesPlay (s, (int)ch->cur_aud_size);

		/*
		 * Set the jpeg input sources to the current video
		 * buffer and start decompressing.
		 */

		s = p1 + sizeof(CHUNK_HEADER);

		jpeg_mem_src (&cinfo, s, (unsigned long)ch->cur_vid_size);
		jpeg_read_header (&cinfo, TRUE);
 		cinfo.out_color_space = JCS_RGB565;
		jpeg_start_decompress (&cinfo);

		/* Process scanlines */

		while (cinfo.output_scanline < cinfo.output_height) {

			/*
			 * We can actually get the jpeg library to
			 * reliably process up to 2 scan lines at once,
			 * so we do that here to cut down how many calls
			 * we make to jpeg_read_scalines().
			 */

			buffer_array[0] = (unsigned char *)frame;
			buffer_array[0] += cinfo.output_scanline *
			    cinfo.output_width * 2;
			buffer_array[1] = buffer_array[0];
			buffer_array[1] += cinfo.output_width * 2;
			jpeg_read_scanlines (&cinfo, buffer_array, 2);
                }

		jpeg_finish_decompress (&cinfo);

		/*
		 * Now blit the frame to the screen. Interally the
		 * uGFX library will use the DMA2D engine for this,
		 * so it should be super fast.
		 */

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

	f_close (&f);

	/* Free memory */

	free (buf);
	free (frame);

	return (0);
}
