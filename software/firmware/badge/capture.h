/*-
 * Copyright (c) 2021
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

#ifndef _CAPTURE_H_

#define CAPTURE_CHAR_UP		'Y'
#define CAPTURE_CHAR_DOWN	'X'
#define CAPTURE_CHAR_EXIT	'Z'

#define CAPTURE_KEY_DOWN	0x08000000
#define CAPTURE_KEY_UP		0x04000000
#define CAPTURE_DIR(x)		((x) & 0x0C000000)
#define CAPTURE_CODE(x)		((x) & 0xF3FFFFFF)

#define CAP_BACKSPACE		0x00000008
#define CAP_TAB			0x00000009
#define CAP_RETURN		0x0000000D
#define CAP_ESCAPE		0x0000001B
#define CAP_MINUS		0x0000002D
#define CAP_EQUALS		0x0000003D
#define CAP_DELETE		0x0000007F

#define CAP_F1			0x4000003A
#define CAP_F2			0x4000003B
#define CAP_F3			0x4000003C
#define CAP_F4			0x4000003D
#define CAP_F5			0x4000003E
#define CAP_F6			0x4000003F
#define CAP_F7			0x40000040
#define CAP_F8			0x40000041
#define CAP_F9			0x40000042
#define CAP_F10			0x40000043
#define CAP_F11			0x40000044
#define CAP_F12			0x40000045
#define CAP_PAUSE		0x40000048
#define CAP_RIGHT		0x4000004F
#define CAP_LEFT		0x40000050
#define CAP_DOWN		0x40000051
#define CAP_UP			0x40000052
#define CAP_KP_MINUS		0x40000056
#define CAP_LSHIFT		0x400000E1
#define CAP_RSHIFT		0x400000E5
#define CAP_LCTRL		0x400000E0
#define CAP_RCTRL		0x400000E4
#define CAP_LALT		0x400000E2
#define CAP_LMETA		0x400000E3
#define CAP_RALT		0x400000E6
#define CAP_RMETA		0x400000E7

extern void capture_queue_init (void);
extern int capture_queue_get (uint32_t * code);

#endif /* _CAPTURE_H_ */
