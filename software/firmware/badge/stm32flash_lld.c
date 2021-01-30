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

/*
 * This module provides support for accessing the STM32F746's internal flash
 * using the ChibiOS flash API. This is intended mainly for firmware update
 * support. The flash is 1MB, but it's partitioned into only 8 sectors, which
 * are not all the same size.
 */

#include <stdlib.h>
#include <string.h>

#include "hal.h"
#include "stm32flash_lld.h"

static const flash_descriptor_t *stm32_get_descriptor(void *instance);
static flash_error_t stm32_read(void *instance, flash_offset_t offset,
                               size_t n, uint8_t *rp);
static flash_error_t stm32_program(void *instance, flash_offset_t offset,
                                  size_t n, const uint8_t *pp);
static flash_error_t stm32_start_erase_all(void *instance);
static flash_error_t stm32_start_erase_sector(void *instance,
                                             flash_sector_t sector);
static flash_error_t stm32_query_erase(void *instance, uint32_t *msec);
static flash_error_t stm32_verify_erase(void *instance, flash_sector_t sector);

static const struct STM32FLASHDriverVMT stm32_vmt = {
	(size_t)0,		/* instance offset */
	stm32_get_descriptor,
	stm32_read,
	stm32_program,
	stm32_start_erase_all,
	stm32_start_erase_sector,
	stm32_query_erase,
	stm32_verify_erase
};

static const flash_sector_descriptor_t stm32_sectors[STM32_SECT_CNT] = {
	{ STM32_SECT0_OFF,	STM32_SECT0_SIZE },
	{ STM32_SECT1_OFF,	STM32_SECT1_SIZE },
	{ STM32_SECT2_OFF,	STM32_SECT2_SIZE },
	{ STM32_SECT3_OFF,	STM32_SECT3_SIZE },
	{ STM32_SECT4_OFF,	STM32_SECT4_SIZE },
	{ STM32_SECT5_OFF,	STM32_SECT5_SIZE },
	{ STM32_SECT6_OFF,	STM32_SECT6_SIZE },
	{ STM32_SECT7_OFF,	STM32_SECT7_SIZE }
};

static flash_descriptor_t stm32_descriptor = {
	0,			/* attributes */
	STM32_SECT0_SIZE,	/* page_size */
	STM32_SECT_CNT,		/* sectors_count */
	stm32_sectors,		/* sectors */
	0U,			/* sectors_size */
	(uint8_t *)FLASH_BASE,	/* address */
	STM32_FLASH_SIZE	/* flash size */
};

static const flash_descriptor_t *
stm32_get_descriptor (void *instance)
{
	STM32FLASHDriver *devp;

	devp = (STM32FLASHDriver *)instance;

	if (devp->state == FLASH_UNINIT ||
	    devp->state == FLASH_STOP)
		return (NULL);

	return (&stm32_descriptor);
}

static flash_error_t
stm32_read (void *instance, flash_offset_t offset, size_t n, uint8_t *rp)
{
	STM32FLASHDriver *devp = (STM32FLASHDriver *)instance;
	uint8_t * p;

	if ((offset + n) > (STM32_SECT7_OFF + STM32_SECT7_SIZE))
		return (FLASH_ERROR_READ);

	osalMutexLock (&devp->mutex);

	if (devp->state != FLASH_READY) {
		osalMutexUnlock (&devp->mutex);
		return (FLASH_ERROR_PROGRAM);
	}

	p = (uint8_t *)stm32_descriptor.address;
	memcpy (rp, p + offset, n);

	osalMutexUnlock (&devp->mutex);

	return (FLASH_NO_ERROR);
}

static flash_error_t
stm32_start_erase_sector (void *instance, flash_sector_t sector)
{
	STM32FLASHDriver *devp = (STM32FLASHDriver *)instance;
	int r = FLASH_ERROR_ERASE;

	osalMutexLock (&devp->mutex);

	if (devp->state != FLASH_READY) {
		osalMutexUnlock (&devp->mutex);
		return (r);
	}

	/* Write key values to activate control register */

	devp->port->KEYR = STM32_FLASH_KEY1;
	devp->port->KEYR = STM32_FLASH_KEY2;

	/* Specify sector */

	devp->port->CR = FLASH_CR_SER | (sector << FLASH_CR_SNB_Pos);

	/* Start erase operation */

	devp->port->CR |= FLASH_CR_STRT;

	devp->state = FLASH_ERASE;

	osalMutexUnlock (&devp->mutex);

	return (r);
}

static flash_error_t
stm32_query_erase (void *instance, uint32_t *msec)
{
	STM32FLASHDriver *devp = (STM32FLASHDriver *)instance;
	int r = FLASH_ERROR_ERASE;

	osalMutexLock (&devp->mutex);

	if (devp->state != FLASH_ERASE) {
		osalMutexUnlock (&devp->mutex);
		return (FLASH_ERROR_PROGRAM);
	}

	/* Check for completion */

	if (devp->port->SR & FLASH_SR_BSY) {
		if (msec != NULL)
			*msec = 1U;
		r = FLASH_BUSY_ERASING;
	} else {

		/* Check status */

		if ((devp->port->SR & FLASH_SR_ERSERR) == 0)
			r = FLASH_NO_ERROR;

		/* Clear status */

		devp->port->SR = 0xFFFFFFFF;

		/* Lock the flash */

		devp->port->CR |= FLASH_CR_LOCK;

		devp->state = FLASH_READY;
	}

	osalMutexUnlock (&devp->mutex);

	return (r);
}

static flash_error_t
stm32_verify_erase (void *instance, flash_sector_t sector)
{
	(void)instance;
	(void)sector;

	return (FLASH_ERROR_ERASE);
}

static flash_error_t
stm32_start_erase_all(void *instance)
{
	STM32FLASHDriver *devp = (STM32FLASHDriver *)instance;
	int r = FLASH_ERROR_ERASE;

	osalMutexLock (&devp->mutex);

	if (devp->state != FLASH_READY) {
		osalMutexUnlock (&devp->mutex);
		return (r);
	}

	/* Write key values to activate control register */

	devp->port->KEYR = STM32_FLASH_KEY1;
	devp->port->KEYR = STM32_FLASH_KEY2;

	/* Initiate mass erase */

	devp->port->CR = FLASH_CR_MER;
	devp->port->CR |= FLASH_CR_STRT;

	devp->state = FLASH_ERASE;

	osalMutexUnlock (&devp->mutex);

	return (r);
}

static flash_error_t
stm32_program (void *instance, flash_offset_t offset,
               size_t n, const uint8_t *pp)
{
	STM32FLASHDriver *devp = (STM32FLASHDriver *)instance;
	int r = FLASH_NO_ERROR;
	volatile uint8_t * p;
	size_t i;

	if ((offset + n) > (STM32_SECT7_OFF + STM32_SECT7_SIZE))
		return (FLASH_ERROR_PROGRAM);

	osalMutexLock (&devp->mutex);

	if (devp->state != FLASH_READY) {
		osalMutexUnlock (&devp->mutex);
		return (FLASH_ERROR_PROGRAM);
	}

	p = (volatile uint8_t *)stm32_descriptor.address;
	p += offset;

	/* Write key values to activate control register */

	devp->port->KEYR = STM32_FLASH_KEY1;
	devp->port->KEYR = STM32_FLASH_KEY2;

	/* Start program operation */

	for (i = 0; i < n; i++) {
		devp->port->CR |= FLASH_CR_PG;
		p[i] = pp[i];
		/* Wait for completion */

        	while (devp->port->SR & FLASH_SR_BSY)
			__DSB();

		if (devp->port->SR & (FLASH_SR_PGPERR | FLASH_SR_PGAERR)) {
			r = FLASH_ERROR_PROGRAM;
			break;
		}
	}

	/* Clear status */

	devp->port->SR = 0xFFFFFFFF;

	/* Lock the flash */

	devp->port->CR |= FLASH_CR_LOCK;

	osalMutexUnlock (&devp->mutex);

	return (r);
}

void
stm32FlashObjectInit(STM32FLASHDriver *devp)
{
	devp->vmt = &stm32_vmt;
	devp->state = FLASH_STOP;
	devp->port = FLASH;
	osalMutexObjectInit (&devp->mutex);

	return;
}

void
stm32FlashStart(STM32FLASHDriver *devp)
{
	/* Make sure controller is idle */

	while (devp->port->SR & FLASH_SR_BSY)
		__DSB();

	/* Clear status */

	devp->port->SR = 0xFFFFFFFF;

	/* Make sure control register is locked */

	devp->port->CR |= FLASH_CR_LOCK;

	devp->state = FLASH_READY;

	return;
}

void
stm32FlashStop(STM32FLASHDriver *devp)
{
	(void)devp;
	return;
}
