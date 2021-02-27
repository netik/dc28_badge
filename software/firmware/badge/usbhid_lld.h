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

#ifndef _USBHID_LLD_H_
#define _USBHID_LLD_H_

#define UKBD_KEYS		6	/* 6 bytes of key data */
#define UKBD_MODIFIERS		8	/* 8 bits of modifier data */
#define UKBD_SCANCODES		256	/* scancodes are only 8 bits wide */

#define UKBD_CODE_RESERVED	0x00
#define UKBD_CODE_ERR_ROLLOVER	0x01
#define UKBD_CODE_ERR_POSTFAIL	0x02
#define UKBD_CODE_ERR_UNDEFINED	0x03

typedef struct _ubkd_report {
	uint8_t		ukbd_modifiers;
	uint8_t		ukbd_reserved;
	uint8_t		ukbd_keys[UKBD_KEYS];
} ukbd_report;

/* Modifier bitmasks */

#define UKBD_MOD_CTRL_L		0x01
#define UKBD_MOD_SHIFT_L	0x02
#define UKBD_MOD_ALT_L		0x04
#define UKBD_MOD_METAL_L	0x08
#define UKBD_MOD_CTRL_R		0x10
#define UKBD_MOD_SHIFT_R	0x20
#define UKBD_MOD_ALT_R		0x40
#define UKBD_MOD_METAL_R	0x80

/* Modifier scancodes */

#define UKBD_KEY_CTRL_L		0xE0
#define UKBD_KEY_SHIFT_L	0xE1
#define UKBD_KEY_ALT_L		0xE2
#define UKBD_KEY_META_L		0xE3
#define UKBD_KEY_CTRL_R		0xE4
#define UKBD_KEY_SHIFT_R	0xE5
#define UKBD_KEY_ALT_R		0xE6
#define UKBD_KEY_META_R		0xE7

extern void usbHostStart (void);

#endif /* _USBHID_LLD_H_ */
