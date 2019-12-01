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

#include "stm32sai_lld.h"

#include "badge.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * Enabling the SAI module is tricky. There are four things you need
 * to worry about:
 *
 * 1) Selecting the SAI clock. For some reason, using the SAIPLL option
 *    with SAI2 doesn't seem to work, so we set it for I2SPLL instead.
 *    The the VCO output is set for 256MHz which we divide down by the
 *    the PLLI2SQ of 8 (to 32MHz) and then by the PLLI2SDIVQ of 2 to
 *    get 16MHz.
 *
 * 2) Setting up the GPIO pins correctly. We need to set the MCK, SCK
 *    SDA and FSA pins for alternate mode 10 (for SAI2 block A).
 *
 * 3) Setting up a DMA channel. We must use DMA channel 2,4 for SAI2.
 *
 * 4) Setting up the SAI configuration for I2S.
 *
 * Note that we divide the 16MHz SAI input clock by 4 to get an MCLK
 * value of 4MHz for the codec. This is divided by a fixed factor of
 * 256 to get a sample rate of 15625Hz.
 *
 * ST Micro makes it very hard to figure out how to configure all these
 * things because their SDK suffers from the all-too-common problem of
 * excessive abstraction. Everything is broken up into layers that
 * can only be combined by compiling a complete project, which you can't
 * do out of the box since the Makefiles must be generated using a GUI.
 */

SAIDriver SAID1;

static thread_reference_t i2sThreadReference;
static int i2sState;

static void
saiDmaInt (void * arg, uint32_t flags)
{
	SAIDriver * saip;

	saip = arg;

        osalSysLock ();
	if (flags & STM32_DMA_ISR_TCIF) {
		i2sState = I2S_STATE_IDLE;
		saip->saiblock->CR1 &= ~SAI_xCR1_DMAEN;
		saip->saiblock->CR2 |= SAI_xCR2_MUTE;
		osalThreadResumeI (&i2sThreadReference, MSG_OK);

	}
        osalSysUnlock ();

	return;
}

void
saiSend (SAIDriver * saip, void * buf, uint32_t size)
{
	dmaStreamSetMemory0 (saip->dma, buf);
	dmaStreamSetTransactionSize (saip->dma, size / sizeof (uint16_t));
	dmaStreamEnable (saip->dma);

	cacheBufferFlush (buf, size);

	osalSysLock ();
	i2sState = I2S_STATE_BUSY;
	saip->saiblock->CR2 &= ~SAI_xCR2_MUTE;
	saip->saiblock->CR1 |= SAI_xCR1_DMAEN;
	osalSysUnlock ();

	return;
}

void
saiWait (void)
{
	osalSysLock ();

	/* If we've finished playing samples already, just return. */

	if (i2sState == I2S_STATE_IDLE) {
		osalSysUnlock ();
		return;
	}

	/* Sleep until the current batch of samples has been sent. */

	osalThreadSuspendS (&i2sThreadReference);
	osalSysUnlock ();

	return;
}

void
saiStart (SAIDriver * saip)
{
	/* Set the handles for SAI2 sub block A */

	saip->sai = SAI2;
	saip->saiblock = SAI2_Block_A;

	/* Configure pins */

	palSetPadMode (GPIOI, GPIOI_SAI2_MCLKA, PAL_MODE_ALTERNATE(10));
	palSetPadMode (GPIOI, GPIOI_SAI2_SCKA, PAL_MODE_ALTERNATE(10));
	palSetPadMode (GPIOI, GPIOI_SAI2_SDA, PAL_MODE_ALTERNATE(10));
	palSetPadMode (GPIOI, GPIOI_SAI2_FSA, PAL_MODE_ALTERNATE(10));

	/* Enable clock and reset peripheral */

	rccEnableAPB2 (RCC_APB2ENR_SAI2EN, TRUE);
	rccResetAPB2 (RCC_APB2RSTR_SAI2RST);

	/*
	 * Allocate DMA stream.
	 * Note: mode is always memory-to-peripheral since we only
	 * support audio playback on this channel.
	 */

	saip->dmamode = STM32_DMA_CR_CHSEL(STM32_SAI2_A_DMA_CHANNEL) |
			STM32_DMA_CR_PL(STM32_SAI2_DMA_PRIORITY) |
			STM32_DMA_CR_PSIZE_HWORD |
			STM32_DMA_CR_MSIZE_HWORD |
			STM32_DMA_CR_MINC |
			STM32_DMA_CR_DIR_M2P |
			STM32_DMA_CR_PBURST_SINGLE |
			STM32_DMA_CR_MBURST_SINGLE |
			STM32_DMA_CR_TCIE;

	saip->dma = dmaStreamAlloc (STM32_SAI2_A_DMA_STREAM,
	    STM32_SAI2_DMA_IRQ_PRIORITY, saiDmaInt, saip);
	dmaStreamSetPeripheral (saip->dma, &saip->saiblock->DR);
	dmaStreamSetMode (saip->dma, saip->dmamode);

	/* Configure SAI2 sublock A for I2S mode */

	saip->saiblock->CR1 =
			2 << SAI_xCR1_MCKDIV_Pos |  /* div by 4 */
			4 << SAI_xCR1_DS_Pos |      /* 16 data bits */
			0 << SAI_xCR1_CKSTR_Pos |   /* clkstrobe rising edge */
			0 << SAI_xCR1_PRTCFG_Pos |  /* free protocol */
			0 << SAI_xCR1_SYNCEN_Pos |  /* asynchronous */
			0 << SAI_xCR1_OUTDRIV_Pos | /* outdrive disable */
			0 << SAI_xCR1_LSBFIRST_Pos |/* MSB first */
			0 << SAI_xCR1_MODE_Pos;	    /* master xmit mode */

	saip->saiblock->CR2 =
			0 << SAI_xCR2_FTH_Pos;      /* FIFO threshold empty */

	saip->saiblock->SLOTR =
			3 << SAI_xSLOTR_SLOTEN_Pos |/* slots 0:1 */
			1 << SAI_xSLOTR_NBSLOT_Pos |/* 2 slots */
			0 << SAI_xSLOTR_SLOTSZ_Pos |/* slot size == data size*/
			0 << SAI_xSLOTR_FBOFF_Pos;  /* 1st bit off == 0 */

	saip->saiblock->FRCR =
			1 << SAI_xFRCR_FSOFF_Pos |  /* before first bit */
			1 << SAI_xFRCR_FSDEF_Pos |  /* + channel ident */
			1 << SAI_xFRCR_FSPOL_Pos |  /* active low */
			31 << SAI_xFRCR_FSALL_Pos | /* active length */
			63 << SAI_xFRCR_FRL_Pos;    /* frame length */

	/*
	 * Enable the SAI block but keep it muted until we're
	 * ready to play some samples.
	 */

	saip->saiblock->CR2 |= SAI_xCR2_MUTE;
	saip->saiblock->CR1 |= SAI_xCR1_SAIEN;
	i2sState = I2S_STATE_IDLE;

	/* Give the codec some time to sense the master clock */

	chThdSleepMilliseconds (200);

	return;
}
