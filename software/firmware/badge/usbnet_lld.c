/*
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

/*
 * This is an LWIP shim driver designed to operate on top of a USB CDC
 * network connection between two badges. It uses the same IP address as
 * the radio interface, but is only active when a peer badge is plugged
 * in. Once a CDC link is brought up, the radio interface is disabled,
 * and IP traffic should flow through the USB link instead. When the
 * USB link is disconnected, the radio will become the default network
 * interface again.
 *
 * On the host side, the usbcdc_lld driver is used, while on the target
 * side, the USB virtual COM port driver (via the shell) is used.
 */

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "usbnet_lld.h"
#include "sx1262_lld.h"
#include "badge.h"

#include <lwip/opt.h>
#include <lwip/def.h>
#include <lwip/mem.h>
#include <lwip/pbuf.h>
#include <lwip/sys.h>
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include <lwip/tcpip.h>
#include <netif/etharp.h>
#include <lwip/netifapi.h>
#include <lwip/inet.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

USBNet_Driver USBNETD1;

static err_t usbnetEthernetInit (struct netif *);
static err_t usbnetOutput (struct netif *, struct pbuf *, const ip4_addr_t *);

void
usbnetReceive (uint8_t * rxbuf, uint16_t len)
{
	struct netif * netif;
	struct pbuf * q;
	struct pbuf * pbuf;

	netif = USBNETD1.usb_netif;

	pbuf = pbuf_alloc (PBUF_RAW, len, PBUF_POOL);

	if (pbuf != NULL) {
		len = 0;
		for (q = pbuf; q != NULL; q = q->next) {
			memcpy (q->payload, rxbuf + len, q->len);
			len += q->len;
		}
		MIB2_STATS_NETIF_ADD(netif, ifinoctets, pbuf->tot_len);
		MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
		netif->input (pbuf, netif);
	} else {
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		MIB2_STATS_NETIF_INC(netif, ifindiscards);
	}

	return;
}

static err_t
usbnetOutput (struct netif * netif, struct pbuf * pbuf, const ip4_addr_t * i)
{
	USBNet_Driver * p;

	(void) i;

	p = netif->state;

	pbuf_copy_partial (pbuf, p->usb_tx_buf, pbuf->tot_len, 0);
	(*p->usb_tx_cb)(p->usb_tx_buf, p->usb_mtu);

	MIB2_STATS_NETIF_ADD(netif, ifoutoctets, pbuf->tot_len);
	MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
	LINK_STATS_INC(link.xmit);

        return (ERR_OK);
}

static err_t
usbnetEthernetInit (struct netif * netif)
{
	USBNet_Driver * p;

	p = netif->state;
        
	MIB2_INIT_NETIF(netif, snmp_ifType_other, 0);
	netif->name[0] = 'u';
	netif->name[1] = 's';
	netif->output = usbnetOutput;
	netif->mtu = p->usb_mtu;
	netif->flags = NETIF_FLAG_BROADCAST |
	    NETIF_FLAG_IGMP | NETIF_FLAG_LINK_UP;

	return (ERR_OK);
}

void
usbnetStart (USBNet_Driver * p)
{
	p->usb_netif = malloc (sizeof(struct netif));
	ip_addr_t ip, gateway, netmask;

	IP4_ADDR(&gateway, 10, 0, 0, 1);
	IP4_ADDR(&netmask, 255, 0, 0, 0);
	IP4_ADDR(&ip, 10, badge_addr[3], badge_addr[4], badge_addr[5]);

	p->usb_mtu = CDC_MTU;
	p->usb_tx_buf = memalign (CACHE_LINE_SIZE, CDC_MTU);

	netif_add (p->usb_netif, &ip, &netmask, &ip,
	    p, usbnetEthernetInit, tcpip_input);
	netif_set_down (p->usb_netif);

	return;
}

void
usbnetEnable (USBNet_Driver * p)
{
#ifdef BADGE_USBNET_NOSLEEP
	badge_sleep_disable ();
#endif
	netif_set_down (SX1262D1.sx_netif);
	netif_set_up (p->usb_netif);
	printf ("USB network attatched\n");
	return;
}

void
usbnetDisable (USBNet_Driver * p)
{
	netif_set_down (p->usb_netif);
	netif_set_up (SX1262D1.sx_netif);
#ifdef BADGE_USBNET_NOSLEEP
	badge_sleep_enable ();
#endif
	printf ("USB network detached\n");
	return;
}
