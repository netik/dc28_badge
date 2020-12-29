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

#ifndef STM32FLASH_LLD_H
#define STM32FLASH_LLD_H

#include "hal_flash.h"

/*
 * The STM32F746 has 1MB of flash, but it's divided up into only 8 sectors,
 * with non-uniform sizes. I have no ideas why ST Micro thought this
 * was a good idea.
 */

#define STM32_SECT0_OFF		0x0
#define STM32_SECT0_SIZE	0x8000

#define STM32_SECT1_OFF		0x8000
#define STM32_SECT1_SIZE	0x8000

#define STM32_SECT2_OFF		0x10000
#define STM32_SECT2_SIZE	0x8000

#define STM32_SECT3_OFF		0x18000
#define STM32_SECT3_SIZE	0x8000

#define STM32_SECT4_OFF		0x20000
#define STM32_SECT4_SIZE	0x20000

#define STM32_SECT5_OFF		0x40000
#define STM32_SECT5_SIZE	0x40000

#define STM32_SECT6_OFF		0x80000
#define STM32_SECT6_SIZE	0x40000

#define STM32_SECT7_OFF		0xC0000
#define STM32_SECT7_SIZE	0x40000

#define STM32_SECT_CNT		8
#define STM32_FLASH_SIZE	0x100000

#define STM32_FLASH_KEY1	0x45670123
#define STM32_FLASH_KEY2	0xCDEF89AB

struct STM32FLASHDriverVMT {
	_base_flash_methods
};

typedef struct {
	const struct STM32FLASHDriverVMT *	vmt;
	_base_flash_data
	FLASH_TypeDef *				port;
	mutex_t					mutex;
} STM32FLASHDriver;

extern STM32FLASHDriver FLASHD1;

extern void stm32FlashObjectInit (STM32FLASHDriver *devp);
extern void stm32FlashStart (STM32FLASHDriver *devp);
extern void stm32FlashStop (STM32FLASHDriver *devp);

#endif /* STM32FLASH_LLD_H */
