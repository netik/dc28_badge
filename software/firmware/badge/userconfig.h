/*-
 * Copyright (c) 2020
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

#ifndef _USERCONFIGH_
#define _USERCONFIGH_

#define CONFIG_SIGNATURE	0xdeadbeefUL  // duh
#define CONFIG_VERSION		3UL
#define CONFIG_NAME_MAXLEN	20

#define CONFIG_FILE_NAME	"/userconf.bin"

#define CONFIG_SOUND_ON		1
#define CONFIG_SOUND_OFF	0

#define CONFIG_RADIO_ON		0
#define CONFIG_RADIO_OFF	1

#define CONFIG_ORIENT_PORTRAIT	1
#define CONFIG_ORIENT_LANDSCAPE	0

#define CONFIG_INIT		1
#define CONFIG_LOAD		2

#pragma pack(1)
typedef struct _userconfig {
	uint32_t	cfg_signature;
	uint32_t	cfg_version;

	char		cfg_name[CONFIG_NAME_MAXLEN];	/* User/badge name */
	uint8_t		cfg_sound;			/* Sound on or off */
	uint8_t		cfg_airplane;			/* Radio on or off */
	uint8_t		cfg_orientation;

	uint32_t	cfg_unlocks;			/* unlock bits */

	uint8_t		cfg_touch_data_present;
	float		cfg_touch_data[8];		/* touch cal data */

	uint32_t	cfg_salt;
	uint32_t	cfg_csum;
} userconfig;
#pragma pack()

extern void configStart (int);
extern userconfig * configGet (void);
extern void configSave (void);
extern void configLoad (void);

#endif /* _USERCONFIGH_ */
