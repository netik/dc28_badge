/*-
 * Copyright (c) 2019
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

#include "badge_finder.h"
#include "userconfig.h"
#include "crc32.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <lwip/opt.h>
#include <lwip/sys.h>
#include <lwip/timeouts.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>

FINDER_PEER * badge_peers;
mutex_t badge_peer_mutex;

static thread_t * pinger_loop_thread;
static thread_t * finder_loop_thread;
static thread_t * ager_loop_thread;

/*
 * See if a peer can be added to the list. If a match
 * is found for an existing peer, reset it.
 */

static void
badge_peer_add (FINDER_MSG * m, struct in_addr * addr)
{
	FINDER_PEER * p;
	int firstfree;
	int i;

	osalMutexLock (&badge_peer_mutex);

	firstfree = -1;

	for (i = 0; i < FINDER_MAXPEERS; i++) {
		p = &badge_peers[i];
		if (p->finder_used == 0 && firstfree == -1)
			firstfree = i;

		/*
		 * If this is a match for an existing peer, fall
		 * though and overwrite/update its slot.
	 	 */

		if (addr->s_addr == p->finder_addr.s_addr) {
			firstfree = i;
			break;
		}
	}

	/*
	 * Not a duplicate, but we're out of room. Note that
	 * this probably can't happen: the peerlist is 512
	 * entries, and we're only manufacturing 500 badges. :)
	 */

	if (firstfree == -1) {
		osalMutexUnlock (&badge_peer_mutex);
		return;
	}

	/* New entry and we have a free slot; populate it */

	p = &badge_peers[firstfree];

	memcpy (&p->finder_msg, m, sizeof (FINDER_MSG));
	memcpy (&p->finder_addr, addr, sizeof (struct in_addr));
	p->finder_ttl = FINDER_TTL;
	p->finder_used = 1;

	osalMutexUnlock (&badge_peer_mutex);

	return;
}

/*
 * Perform an aging pass on the peer list. Peers where the
 * ttl has reached 0 are removed.
 */

static void
badge_peer_age (void)
{
	FINDER_PEER * p;
	int i;

	osalMutexLock (&badge_peer_mutex);

	for (i = 0; i < FINDER_MAXPEERS; i++) {
		p = &badge_peers[i];

		if (p->finder_used == 0)
			continue;

		p->finder_ttl--;

		if (p->finder_ttl == 0)
			memset (p, 0, sizeof (FINDER_MSG));
	}

	osalMutexUnlock (&badge_peer_mutex);

	return;
}

/* Display the current peer list */

void
badge_peer_show (void)
{
	FINDER_PEER * p;
	int i;

	osalMutexLock (&badge_peer_mutex);

	for (i = 0; i < FINDER_MAXPEERS; i++) {
		p = &badge_peers[i];

		if (p->finder_used == 0)
			continue;

		printf ("[%s]: [%s] [%d]\n", inet_ntoa (p->finder_addr),
		    p->finder_msg.finder_name, p->finder_ttl);
	}

	osalMutexUnlock (&badge_peer_mutex);

	return;
}

static
THD_FUNCTION(ager_loop, arg)
{
	(void)arg;

	while (1) {
		sleep (1);
		badge_peer_age ();
	}

	/* NOTREACHED */
	return;
}

/*
 * The pinger loop periodically sends out a broadcast message
 * containing the user's name and other salient info. This allows
 * badges to detect each other.
 */

static
THD_FUNCTION(pinger_loop, arg)
{
	int s;
	struct sockaddr_in sin;
	FINDER_MSG * msg;
	userconfig * c;

	c = configGet ();

	(void)arg;

	memset (&sin, 0, sizeof(sin));

	sin.sin_addr.s_addr = inet_addr ("10.255.255.255");
	sin.sin_port = lwip_htons (12345);
	sin.sin_family = AF_INET;

	s = lwip_socket (AF_INET, SOCK_DGRAM, 0);

	msg = malloc (sizeof(FINDER_MSG));

	memset (msg, 0, sizeof(FINDER_MSG));

	while (1) {
		/*
		 * In case the user changes their name while
		 * the badge is running.
		 */
		strcpy (msg->finder_name, c->cfg_name);

		/* Add a checksum for integrity checking. */

		msg->finder_salt = (uint32_t)random ();
		msg->finder_csum = crc32_le ((uint8_t *)msg,
		    sizeof (FINDER_MSG) - sizeof(uint32_t), 0);

		(void) lwip_sendto (s, msg, sizeof(FINDER_MSG), 0,
		    (struct sockaddr *)&sin, sizeof (sin));

		/*
		 * Pause for 5 seconds, plus some random
		 * amount of milliseconds.
		 */

		usleep (5000000 + ((random () & 0xFFFF)));
	}

	/* NOTREACHED */
	return;
}

static
THD_FUNCTION(finder_loop, arg)
{
	int s, r;
	socklen_t len;
	struct sockaddr_in sin;
	struct sockaddr_in from;
#ifdef FINDER_RX_TIMEOUT
	struct timeval tv;
#endif
	uint8_t * buf;
	uint32_t crc;
	FINDER_MSG * msg;

	(void)arg;

	memset (&sin, 0, sizeof(sin));

	sin.sin_addr.s_addr = inet_addr ("10.255.255.255");
	sin.sin_port = lwip_htons (12345);
	sin.sin_family = AF_INET;

	s = lwip_socket (AF_INET, SOCK_DGRAM, 0);

	(void) lwip_bind (s, (struct sockaddr *)&sin, sizeof(sin));

#ifdef FINDER_RX_TIMEOUT
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	(void) lwip_setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

	buf = malloc (128);
	msg = (FINDER_MSG *)buf;

	while (1) {
		len = sizeof (sin);
		r = lwip_recvfrom (s, buf, 128, 0,
		    (struct sockaddr *)&from, &len);

		if (r == -1)
			continue;

		/* check the length */

		if (r != sizeof (FINDER_MSG))
			continue;

		/* check the checksum */

		crc = crc32_le ((uint8_t *)msg,
		    sizeof (FINDER_MSG) - sizeof(uint32_t), 0);

		if (crc != msg->finder_csum)
			continue;

		badge_peer_add (msg, &from.sin_addr);
	}

	/* NOTREACHED */
	return;
}

void
badge_finder_start (void)
{
	badge_peers = malloc (sizeof(FINDER_PEER) * FINDER_MAXPEERS);
	memset (badge_peers, 0, (sizeof(FINDER_PEER) * FINDER_MAXPEERS));
	osalMutexObjectInit (&badge_peer_mutex);

	ager_loop_thread = chThdCreateFromHeap (NULL,
	    THD_WORKING_AREA_SIZE(128), "agerThread",
	    NORMALPRIO, ager_loop, NULL);

	pinger_loop_thread = chThdCreateFromHeap (NULL,
	    THD_WORKING_AREA_SIZE(1024), "pingerThread",
	    NORMALPRIO, pinger_loop, NULL);

	finder_loop_thread = chThdCreateFromHeap (NULL,
	    THD_WORKING_AREA_SIZE(1024), "finderThread",
	    NORMALPRIO, finder_loop, NULL);

	return;
}
