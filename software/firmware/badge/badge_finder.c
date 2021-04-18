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

#include "badge.h"

#include "orchard-app.h"

#include "sx1262_lld.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <lwip/opt.h>
#include <lwip/sys.h>
#include <lwip/timeouts.h>
#include <lwip/netifapi.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

FINDER_PEER * badge_peers;
mutex_t badge_peer_mutex;

static thread_t * pinger_loop_thread;
static thread_t * finder_loop_thread;
static thread_t * ager_loop_thread;

/*
 * The 900MHz ISM band extends from 902MHz to 928MHz. We split this
 * up into 500KHz channels, for a total of 52 different frequencies.
 * One frequency is used as the beacon channel.
 */

uint32_t badge_finder_channels[FINDER_CHANNELS] = {
	902000000, 902500000,
	903000000, 903500000,
	904000000, 904500000,
	905000000, 905500000,
	906000000, 906500000,
	907000000, 907500000,
	908000000, 908500000,
	909000000, 909500000,
	910000000, 910500000,
	911000000, 911500000,
	912000000, 912500000,
	913000000, 913500000,
	914000000, 914500000,
	915000000, 915500000,
	916000000, 916500000,
	917000000, 917500000,
	918000000, 918500000,
	919000000, 919500000,
	920000000, 920500000,
	921000000, 921500000,
	922000000, 922500000,
	923000000, 923500000,
	924000000, 924500000,
	925000000, 926500000,
	926000000, 927500000,
	927000000, 928500000
};

/*
 * See if a peer can be added to the list. If a match
 * is found for an existing peer, reset it.
 */

static void
badge_peer_add (FINDER_PING * m, struct in_addr * addr)
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

	memcpy (&p->finder_msg, m, sizeof (FINDER_PING));
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
			memset (p, 0, sizeof (FINDER_PING));
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
		    p->finder_msg.finder_hdr.finder_name, p->finder_ttl);
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
	FINDER_PING * msg;
	userconfig * c;
	uint32_t ur;
	uint32_t ut;

	c = configGet ();

	(void)arg;

	memset (&sin, 0, sizeof(sin));

	sin.sin_addr.s_addr = inet_addr ("10.255.255.255");
	sin.sin_port = lwip_htons (FINDER_PORT);
	sin.sin_family = AF_INET;

	s = lwip_socket (AF_INET, SOCK_DGRAM, 0);

	msg = malloc (sizeof(FINDER_PING));

	memset (msg, 0, sizeof(FINDER_PING));

	while (1) {
		/*
		 * In case the user changes their name while
		 * the badge is running.
		 */
		strcpy (msg->finder_hdr.finder_name, c->cfg_name);
		msg->finder_hdr.finder_type = FINDER_TYPE_PING;

		/* Add a checksum for integrity checking. */

		msg->finder_salt = (uint32_t)random ();
		msg->finder_csum = crc32_le ((uint8_t *)msg,
		    sizeof (FINDER_PING) - sizeof(uint32_t), 0);

		(void) lwip_sendto (s, msg, sizeof(FINDER_PING), 0,
		    (struct sockaddr *)&sin, sizeof (sin));

		/*
		 * Pause for 5 seconds, plus some random
		 * amount of milliseconds.
		 */

		ur = ((unsigned long)random () & 0xFFFF);
		ut = 5000000 + ur;

		usleep (ut);
	}

	/* NOTREACHED */
	return;
}

static void
badge_handle_ping (FINDER_PING * p, int len, struct sockaddr_in * from)
{
	uint32_t crc;

	/* check the length */

	if (len != sizeof (FINDER_PING))
		return;

	/* check the checksum */

	crc = crc32_le ((uint8_t *)p,
	    sizeof (FINDER_PING) - sizeof(uint32_t), 0);

	if (crc != p->finder_csum)
		return;

	badge_peer_add (p, &from->sin_addr);

	return;
}

static void
badge_handle_shout (FINDER_SHOUT * p, int len, struct sockaddr_in * from)
{
	uint32_t crc;

	(void)from;

	/* check the length */

	if (len != sizeof (FINDER_SHOUT))
		return;

	/* check the checksum */

	crc = crc32_le ((uint8_t *)p,
	    sizeof (FINDER_SHOUT) - sizeof(uint32_t), 0);

	if (crc != p->finder_csum)
		return;

	orchardAppRadioCallback (shoutEvent, 0, p, sizeof(FINDER_SHOUT));

	return;
}

static void
badge_handle_doom (FINDER_DOOM * p, int len, struct sockaddr_in * from)
{
	uint32_t crc;

	/* check the length */

	if (len != sizeof (FINDER_DOOM))
		return;

	/* check the checksum */

	crc = crc32_le ((uint8_t *)p,
	    sizeof (FINDER_DOOM) - sizeof(uint32_t), 0);

	if (crc != p->finder_csum)
		return;

	memcpy (&p->finder_doom_peer, &from->sin_addr, sizeof(struct in_addr));

	orchardAppRadioCallback (doomEvent, 0, p, sizeof(FINDER_DOOM));

	return;
}

static void
badge_handle_action (FINDER_ACTION * p, int len, struct sockaddr_in * from)
{
	uint32_t crc;

	(void)from;

	/* check the length */

	if (len != sizeof (FINDER_ACTION))
		return;

	/* check the checksum */

	crc = crc32_le ((uint8_t *)p,
	    sizeof (FINDER_ACTION) - sizeof(uint32_t), 0);

	if (crc != p->finder_csum)
		return;

	orchardAppRadioCallback (actionEvent, 0, p, sizeof(FINDER_ACTION));

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
	FINDER_HDR * hdr;

	(void)arg;

	memset (&sin, 0, sizeof(sin));

	sin.sin_addr.s_addr = lwip_htonl(INADDR_ANY);
	sin.sin_port = lwip_htons (FINDER_PORT);
	sin.sin_family = AF_INET;

	s = lwip_socket (AF_INET, SOCK_DGRAM, 0);

	(void) lwip_bind (s, (struct sockaddr *)&sin, sizeof(sin));

#ifdef FINDER_RX_TIMEOUT
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	(void) lwip_setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

	buf = malloc (FINDER_MAXPKT);
	hdr = (FINDER_HDR *)buf;

	while (1) {
		len = sizeof (sin);
		r = lwip_recvfrom (s, buf, FINDER_MAXPKT, 0,
		    (struct sockaddr *)&from, &len);

		if (r == -1)
			continue;

		if (hdr->finder_type == FINDER_TYPE_SHOUT)
			badge_handle_shout ((FINDER_SHOUT *)buf, r, &from);

		if (hdr->finder_type == FINDER_TYPE_DOOM)
			badge_handle_doom ((FINDER_DOOM *)buf, r, &from);

		if (hdr->finder_type == FINDER_TYPE_ACTION)
			badge_handle_action ((FINDER_ACTION *)buf, r, &from);

		if (hdr->finder_type == FINDER_TYPE_PING)
			badge_handle_ping ((FINDER_PING *)buf, r, &from);
	}

	/* NOTREACHED */
	return;
}

void
badge_finder_radio_mode_set (uint8_t mode)
{
	if (SX1262D1.sx_mode == mode)
		return;

	if (mode != FINDER_RADIO_MODE_SLOW &&
	    mode != FINDER_RADIO_MODE_FAST)
		return;

	netif_set_down (SX1262D1.sx_netif);

	osalMutexLock (&SX1262D1.sx_mutex);
	if (mode == FINDER_RADIO_MODE_SLOW)
		SX1262D1.sx_mode = SX_MODE_LORA;
	if (mode == FINDER_RADIO_MODE_FAST)
		SX1262D1.sx_mode = SX_MODE_GFSK;
	sx1262Disable (&SX1262D1);
	sx1262Enable (&SX1262D1);
	osalMutexUnlock (&SX1262D1.sx_mutex);

	netif_set_up (SX1262D1.sx_netif);

	return;
}

void
badge_finder_radio_freq_set (uint32_t freq)
{
	if (SX1262D1.sx_freq == freq)
		return;

	if (freq < FINDER_FREQ_MIN || freq > FINDER_FREQ_MAX)
		return;

	netif_set_down (SX1262D1.sx_netif);

	osalMutexLock (&SX1262D1.sx_mutex);
	SX1262D1.sx_freq = freq;
	sx1262Disable (&SX1262D1);
	sx1262Enable (&SX1262D1);
	osalMutexUnlock (&SX1262D1.sx_mutex);

	netif_set_up (SX1262D1.sx_netif);

	return;
}

void
badge_finder_radio_restore (void)
{
	if (SX1262D1.sx_mode == SX_MODE_DEFAULT &&
	    SX1262D1.sx_freq == SX_FREQ_DEFAULT)
		return;

	netif_set_down (SX1262D1.sx_netif);

	osalMutexLock (&SX1262D1.sx_mutex);
	SX1262D1.sx_mode = SX_MODE_DEFAULT;
	SX1262D1.sx_freq = SX_FREQ_DEFAULT;
	sx1262Disable (&SX1262D1);
	sx1262Enable (&SX1262D1);
	osalMutexUnlock (&SX1262D1.sx_mutex);

	netif_set_up (SX1262D1.sx_netif);

	return;
}

uint32_t
badge_finder_radio_chan_alloc (void)
{
	unsigned seed;
	int i;

        trngGenerate (&TRNGD1, sizeof (seed), (uint8_t *)&seed);
	srand (seed);

	i = rand () % (FINDER_CHANNELS + 1);

	/* don't return the beacon channel */

	if (i == 0)
		i++;

	return (badge_finder_channels[i]);
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
