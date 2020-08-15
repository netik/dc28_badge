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

#ifndef _BADGE_FINDERH_
#define _BADGE_FINDERH_

#include <lwip/inet.h>
#include "userconfig.h"

#define FINDER_NAME_MAXLEN	CONFIG_NAME_MAXLEN
#define FINDER_MAXPEERS		512
#define FINDER_TTL		16

/*
 * This message structure contains all the user info we
 * want to advertise in a badge broadcast. For now it
 * contains mainly just the user's name. Right now it's 
 * unclear what additional data to send (probably some
 * inter-badge game stats.
 */

#pragma pack(1)
typedef struct finder_msg {
	char		finder_name[FINDER_NAME_MAXLEN];
	uint32_t	finder_dummy;
	uint32_t	finder_salt;
	uint32_t	finder_csum;
} FINDER_MSG;
#pragma pack()

typedef struct finder_peer {
	struct in_addr	finder_addr;
	uint8_t		finder_ttl;
	uint8_t		finder_used;
	FINDER_MSG	finder_msg;
} FINDER_PEER;

extern FINDER_PEER * badge_peers;
extern mutex_t badge_peer_mutex;

extern void badge_finder_start (void);
extern void badge_peer_show (void);
extern void multiSend (uint8_t *, size_t);

#endif /* _BADGE_FINDERH_ */
