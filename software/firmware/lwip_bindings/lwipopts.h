/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Simon Goldschmidt
 *
 */
#ifndef LWIP_HDR_LWIPOPTS_H__
#define LWIP_HDR_LWIPOPTS_H__

/* Fixed settings mandated by the ChibiOS integration.*/
#include "static_lwipopts.h"

#define X8_F "02x"

#define LWIP_RAW			1
#define LWIP_SO_RCVTIMEO		1
#define LWIP_SO_SNDTIMEO		1
#define LWIP_IGMP			1
#define LWIP_SOCKET			1
#define LWIP_DNS			1

#define LWIP_NETIF_LOOPBACK		1

#define MEM_LIBC_MALLOC			1
#define MEMP_MEM_MALLOC			1

#include <stdlib.h>
#define LWIP_RAND()			rand()

#define IP_REASS_MAX_PBUFS		128
#define MEMP_NUM_NETCONN		16
#define MEMP_NUM_FRAG_PBUF		64
#define MEMP_NUM_PBUF			256
#define PBUF_POOL_SIZE			256

#define LWIP_DEBUG                      1
#define NETIF_DEBUG                     LWIP_DBG_ON
#include <stdio.h>
#define LWIP_PLATFORM_DIAG(x)           printf x

#define TCPIP_THREAD_NAME		"lwIPEvent"
/*
 * *sigh* Thanks for making these so hard to find.
 */

#define TCPIP_MBOX_SIZE			MEMP_NUM_PBUF
#define DEFAULT_RAW_RECVMBOX_SIZE 	MEMP_NUM_PBUF
#define DEFAULT_UDP_RECVMBOX_SIZE	MEMP_NUM_PBUF
#define DEFAULT_TCP_RECVMBOX_SIZE	MEMP_NUM_PBUF
#define DEFAULT_ACCEPTMBOX_SIZE		MEMP_NUM_PBUF

/* Optional, application-specific settings.*/
#if !defined(TCPIP_MBOX_SIZE)
#define TCPIP_MBOX_SIZE                 MEMP_NUM_PBUF
#endif
#if !defined(TCPIP_THREAD_STACKSIZE)
#define TCPIP_THREAD_STACKSIZE          1024
#endif

/* Use ChibiOS specific priorities. */
#if !defined(TCPIP_THREAD_PRIO)
#define TCPIP_THREAD_PRIO               (NORMALPRIO + 10) /*(LOWPRIO + 1)*/
#endif
#if !defined(LWIP_THREAD_PRIORITY)
#define LWIP_THREAD_PRIORITY            (NORMALPRIO + 11) /*(LOWPRIO)*/
#endif

#endif /* LWIP_HDR_LWIPOPTS_H__ */
