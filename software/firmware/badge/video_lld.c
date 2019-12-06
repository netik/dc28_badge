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
#include "ffconf.h"
#include "diskio.h"

#include "hal_fsmc_sdram.h"
#include "fsmc_sdram.h"

#include "video_lld.h"
#include "badge.h"

#include "async_io_lld.h"
#include "stm32sai_lld.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define        roundup(x, y)   ((((x)+((y)-1))/(y))*(y))

#define COALESCE

#ifdef COALESCE
static uint16_t * vp;
#endif
static uint8_t * fp;

#include "tjpgd.h"

static uint16_t 
tjd_input (JDEC * jd, uint8_t * b, uint16_t nd)
{
	(void)jd;

	if (b)
		memcpy (b, fp, nd);

	fp += nd;
	return (nd);
}

static uint16_t
tdj_output (JDEC * jd, void * b, JRECT * r)
{
	gPixel h, w;
#ifdef COALESCE
	gPixel x, y;
	uint32_t i, j, cnt;
	uint32_t * s;
	uint32_t * d;
#endif

	(void)jd;

	w = r->right - r->left + 1;
	h = r->bottom - r->top + 1;

#ifdef COALESCE
	y = r->top;
	x = r->left;

	s = b;
	cnt = 0;
	for (i = 0; i < h; i++) {
		d = (uint32_t *)&vp[(320 * (i + y)) + x];
		for (j = 0; j < (w / 2); j++) {
			d[j] = s[cnt];
			cnt++;
		}
	}
#else
	gdispBlitArea (80 + r->left, 16 + r->top, w, h, b);
#endif

	return (1);
}

static void
videoFrameDecompress (JDEC * jd, uint8_t * buf, uint8_t * work)
{
	fp = buf + sizeof(CHUNK_HEADER);

	jd_prepare (jd, tjd_input, work, VID_JPG_WORKBUF_SIZE, NULL);
	jd_decomp (jd, tdj_output, 0);
#ifdef COALESCE
	gdispBlitArea (80, 16, 320, 240, vp);
#endif

	return;
}

int
videoPlay (char * path)
{
	CHUNK_HEADER _ch;
	CHUNK_HEADER * ch;
	uint8_t * buf_orig;
	uint8_t * buf;
	uint8_t * p1;
	uint8_t * p2;
	uint8_t * workbuf;
	uint8_t * s;
	JDEC jd;
	FIL f;
	UINT br;
	size_t max;

	if (f_open(&f, path, FA_READ) != FR_OK) {
		printf ("opening [%s] failed\n", path);
		return (-1);
	}

	i2sPlay (NULL);

	ch = &_ch;

	f_read (&f, ch, sizeof(CHUNK_HEADER), &br);

	max = (ch->cur_vid_size + ch->cur_aud_size) + sizeof(CHUNK_HEADER);

	buf_orig = malloc ((max * 2) + CACHE_LINE_SIZE);

	buf = (uint8_t *)roundup((uint32_t)buf_orig, CACHE_LINE_SIZE);

#ifdef COALESCE
	vp = malloc (320 * 240 * 2);
#endif
	workbuf = malloc (VID_JPG_WORKBUF_SIZE);

	p1 = buf;
	p2 = buf + max;

	f_read (&f, p1, (UINT)ch->next_chunk_size, &br);

	while (1) {
		UINT sz;
		ch = (CHUNK_HEADER *)p1;
		sz = ch->next_chunk_size;
		if (sz == 0)
			break;
		asyncIoRead (&f, p2, sz, &br);
		s = p1 + ch->cur_vid_size + sizeof(CHUNK_HEADER);
		i2sSamplesPlay (s, 3840);
		videoFrameDecompress (&jd, p1, workbuf);
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
		saiWait ();
	}

	/* Drain any pending I/O */

	i2sSamplesWait ();
	i2sSamplesStop ();

	f_close (&f);

	free (buf_orig);
#ifdef COALESCE
	free (vp);
#endif
	free (workbuf);

	return (0);
}
