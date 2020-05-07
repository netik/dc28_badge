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

#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "badge.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <lwip/opt.h>
#include <lwip/mem.h>
#include <lwip/icmp.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/timeouts.h>
#include <lwip/inet_chksum.h>
#include <lwip/prot/ip4.h>
#include <lwip/prot/ip.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>

#define PING_DATA_SIZE 32
#define PING_ID 0xAFAF
#define PING_RCV_TIMEO 5000
#define PING_MAX_PKT 65535

static err_t
ping_send (int s, ip_addr_t * addr, int len, uint16_t seq)
{
	struct icmp_echo_hdr * iecho;
	struct sockaddr_storage to;
	struct sockaddr_in * to4 = (struct sockaddr_in*)&to;
	size_t ping_size = sizeof(struct icmp_echo_hdr) + len;
	size_t data_len;
	size_t i;
	err_t err;

	iecho = (struct icmp_echo_hdr *)malloc (ping_size);
	if (iecho == NULL)
		return (ERR_MEM);

	ICMPH_TYPE_SET(iecho, ICMP_ECHO);
	ICMPH_CODE_SET(iecho, 0);
	iecho->chksum = 0;
	iecho->id     = PING_ID;
	iecho->seqno  = lwip_htons(seq);

	/* fill the additional data buffer with some data */

	data_len = ping_size - sizeof(struct icmp_echo_hdr);
	for (i = 0; i < data_len; i++)
		((char*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
 
	iecho->chksum = inet_chksum (iecho, ping_size);

	to4->sin_len    = sizeof(to4);
	to4->sin_family = AF_INET;
	inet_addr_from_ip4addr (&to4->sin_addr, ip_2_ip4 (addr));

	err = lwip_sendto (s, iecho, ping_size, 0,
	    (struct sockaddr*)&to, sizeof(to));

	free (iecho);

	return (err ? ERR_OK : ERR_VAL);
}

static void
ping_recv (int s, int size)
{
	char * buf;
	int len;
	ip_addr_t fromaddr;
	struct sockaddr_storage from;
	struct sockaddr_in * from4;
        struct ip_hdr * iphdr;
	struct icmp_echo_hdr * iecho;
	int fromlen = sizeof(from);
	systime_t t1;
	sysinterval_t t2;

	buf = malloc (size);

	t1 = chVTGetSystemTimeX ();

	len = lwip_recvfrom (s, buf, size, 0,
	    (struct sockaddr *)&from, (socklen_t *)&fromlen);

	t2 = chVTTimeElapsedSinceX (t1);

	if (len < 0) {
		printf ("timed out\n");
		return;
	}

	memset(&fromaddr, 0, sizeof(fromaddr));

	if (from.ss_family == AF_INET) {
		from4 = (struct sockaddr_in*)&from;
		inet_addr_to_ip4addr (ip_2_ip4(&fromaddr),
		    &from4->sin_addr);
		IP_SET_TYPE_VAL(fromaddr, IPADDR_TYPE_V4);
	}

	iphdr = (struct ip_hdr *)buf;
       	iecho = (struct icmp_echo_hdr *)(buf + (IPH_HL(iphdr) * 4));

	printf ("%d bytes from %d.%d.%d.%d: ", len,
	    ip4_addr1(&fromaddr), ip4_addr2(&fromaddr),
	    ip4_addr3(&fromaddr), ip4_addr4(&fromaddr));

	printf ("icmp_seq=%d ", lwip_ntohs(iecho->seqno));
	printf ("ttl=%d ", IPH_TTL(iphdr));
	printf ("time=%.3f ms\n", (float)TIME_I2MS(t2) / 1000);

	free (buf);

	return;
}

static void
cmd_ping (BaseSequentialStream *chp, int argc, char *argv[])
{
	int s;
	struct timeval tmo;
	ip_addr_t ping_target;
	int o[4];
	int size;
	int i;
	int hold;

	(void)argv;
	(void)chp;
	if (argc < 1) {
		printf ("Usage: ping <IP address> [size]\n");
		return;
	}

	i = sscanf (argv[0], "%d.%d.%d.%d", &o[0], &o[1], &o[2], &o[3]);

	if (i < 4) {
		printf ("bad IP address\n");
		return;
	}

	for (i = 0; i < 4; i++) {
		if (o[i] <= 0 && o[i] > 255) {
			printf ("bad IP address\n");
			return;
		}
	}

	if (argc < 2)
		size = PING_DATA_SIZE;
	else
		size = atoi (argv[1]);

	if (size > PING_MAX_PKT) {
		printf ("size too large\n");
		return;
	}

	s = lwip_socket (AF_INET, SOCK_RAW, IP_PROTO_ICMP);

	if (s < 0) {
		printf ("failed to create socket\n");
		return;
	}

	tmo.tv_sec = PING_RCV_TIMEO / 1000;
	tmo.tv_usec = (PING_RCV_TIMEO % 1000) * 1000;
	hold = PING_MAX_PKT;

	lwip_setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &tmo, sizeof(tmo));

        lwip_setsockopt (s, SOL_SOCKET, SO_RCVBUF, &hold, sizeof(hold));

	IP4_ADDR(&ping_target, o[0], o[1], o[2], o[3]);

	for (i = 0; i < 5; i++) {
		ping_send (s, &ping_target, size, i);
		ping_recv (s, PING_MAX_PKT);
	}

	lwip_close (s);

	return;
}

orchard_command("ping", cmd_ping);
