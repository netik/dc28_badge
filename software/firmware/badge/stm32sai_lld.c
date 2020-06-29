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
#include <fcntl.h>

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

#define        roundup(x, y)   ((((x)+((y)-1))/(y))*(y))

SAIDriver SAID2;

static uint16_t * i2sBufOrig;
static uint16_t * i2sBuf;

uint8_t i2sEnabled = TRUE;
 
static char * fname;
static thread_t * pThread = NULL;
static volatile uint8_t play;
static uint8_t i2sloop;

static void i2sThread(void *);

static thread_reference_t i2sThreadReference;
static int i2sState;

static void
saiDmaInt (void * arg, uint32_t flags)
{
	SAIDriver * saip;

	saip = arg;

        osalSysLockFromISR ();
	if (flags & STM32_DMA_ISR_TCIF) {
		i2sState = I2S_STATE_IDLE;
		saip->saiblock->CR1 &= ~SAI_xCR1_DMAEN;
		saip->saiblock->CR2 |= SAI_xCR2_MUTE;
		osalThreadResumeI (&i2sThreadReference, MSG_OK);

	}
        osalSysUnlockFromISR ();

	return;
}

void
saiSend (SAIDriver * saip, void * buf, uint32_t size)
{
	dmaStreamSetMemory0 (saip->dma, buf);
	dmaStreamSetTransactionSize (saip->dma, size / sizeof (uint16_t));
	dmaStreamSetMode (saip->dma, saip->dmamode);
	dmaStreamEnable (saip->dma);

	cacheBufferFlush (buf, size);

	osalSysLock ();
	i2sState = I2S_STATE_BUSY;
	saip->saiblock->CR2 &= ~SAI_xCR2_MUTE;
	saip->saiblock->CR1 |= SAI_xCR1_DMAEN;
	osalThreadResumeI (&i2sThreadReference, MSG_OK);
	osalSysUnlock ();

	return;
}

void
saiStop (SAIDriver * saip)
{
	osalSysLock ();
	i2sState = I2S_STATE_IDLE;
	saip->saiblock->CR2 &= ~SAI_xCR2_MUTE;
	saip->saiblock->CR1 &= ~SAI_xCR1_DMAEN;
	osalSysUnlock ();

	dmaStreamDisable (saip->dma);

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
saiStereo (SAIDriver * saip, bool enable)
{
	/*i2sSamplesWait ();*/
	/*saiWait ();*/

	chThdSleepMilliseconds (200);

	osalSysLock ();
	if (enable == TRUE) {
		saip->saiblock->CR1 &= ~SAI_xCR1_MONO;
	} else {
		saip->saiblock->CR1 |= SAI_xCR1_MONO;
	}
	osalSysUnlock ();

	chThdSleepMilliseconds (200);

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

	/* Configure SAI2 sublock A for I2S mode */

	saip->saiblock->CR1 =
			2 << SAI_xCR1_MCKDIV_Pos |  /* div by 4 */
			4 << SAI_xCR1_DS_Pos |      /* 16 data bits */
			0 << SAI_xCR1_CKSTR_Pos |   /* clkstrobe rising edge */
			0 << SAI_xCR1_PRTCFG_Pos |  /* free protocol */
			0 << SAI_xCR1_SYNCEN_Pos |  /* asynchronous */
			0 << SAI_xCR1_MONO_Pos |    /* stereo (mono == 0) */
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

	/* Allocate memory for playback buffer */

	i2sBufOrig = chHeapAlloc (NULL, (I2S_SAMPLES * sizeof(uint16_t) * 2) +
	    CACHE_LINE_SIZE);
	i2sBuf = (uint16_t *)roundup ((uintptr_t)i2sBufOrig, CACHE_LINE_SIZE);

        /* Launch the player thread. */

	pThread = chThdCreateFromHeap (NULL, THD_WORKING_AREA_SIZE(1024),
            "I2SEvent", I2S_THREAD_PRIO, i2sThread, NULL);

	return;
}

static
THD_FUNCTION(i2sThread, arg)
{
        int f;
        int br;
        uint16_t * p;
        thread_t * th;
        char * file = NULL;

	(void)arg;

	while (1) {
		if (play == 0) {
			th = chMsgWait ();
			file = fname;
			play = 1;
			chMsgRelease (th, MSG_OK);
		}

		if (i2sEnabled == FALSE) {
			play = 0;
			file = NULL;
			continue;
		}

		f = open (file, O_RDONLY, 0);
		if (f == -1) {
			play = 0;
			continue;
		}

		/* Load the first block of samples. */

		p = i2sBuf;
		br = read (f, p, I2S_BYTES);
		if (br == 0 || br == -1) {
			close (f);
			play = 0;
			continue;
		}

		while (1) {

			/* Start the samples playing */

			i2sSamplesPlay (p, br >> 1);

			/* Swap buffers and load the next block of samples */

			if (p == i2sBuf)
        			p += I2S_SAMPLES;
			else
        			p = i2sBuf;

			br = read (f, p, I2S_BYTES);

			/*
			 * Wait until the current block of samples
			 * finishes playing before playing the next
			 * block.
			 */

			i2sSamplesWait ();

			/* If we read 0 bytes, we reached end of file. */

			if (br == 0 || br == -1)
				break;

			/* If told to stop, exit the loop. */

			if (play == 0)
        			break;
		}

		/* We're done, close the file. */

		i2sSamplesStop ();
		if (br == 0 && i2sloop == I2S_PLAY_ONCE) {
			file = NULL;
			play = 0;
		}

		close (f);
	}

	/* NOTREACHED */
	return;
}

/******************************************************************************
*
* i2sSamplesPlay - play specific sample buffer
*
* This function can be used to play an arbitrary buffer of samples provided
* by the caller via the pointer argument <p>. The <cnt> argument indicates
* the number of samples (which should be equal to the total number of bytes
* in the buffer divided by 2, since samples are stored as 16-bit quantities).
*
* This function is used primarily by the video player module.
*
* RETURNS: N/A
*/

void
i2sSamplesPlay (void * buf, int cnt)
{
	if (cnt == 0)
		return;
	saiSend (&SAID2, buf, cnt * 2);
	return;
}

/******************************************************************************
*
* i2sSamplesWait - wait for audio sample buffer to finish playing
*
* This function can be used to test when the current block of samples has
* finished playing. It is used primarily by callers of i2sSamplesPlay() to
* keep pace with the audio playback. The function sleeps until an interrupt
* occurs indicating that the current batch of samples has been sent to
* the codec chip.
*
* RETURNS: N/A
*/

void
i2sSamplesWait (void)
{
	saiWait ();
	return;
}

/******************************************************************************
*
* i2sSamplesStop - stop sample playback
*
* This function must be called after samples are sent to the codec using
* i2sSamplesPlay() and there are no more samples left to send. The nature
* of the CS4344 codec is such that we can't just stop the I2S controller
* because that cuts off the clock signals to it, and it will take time for
* it to re-synchronize when the clocks are started again. Instead we gate
* the SCK signal which stops it from trying to decode any audio data.
*
* RETURNS: N/A
*/

void
i2sSamplesStop (void)
{
	saiStop (&SAID2);
	return;
}

/******************************************************************************
*
* i2sWait - wait for current audio file to finish playing
*
* This function can be used to test when the current audio sample file has
* finished playing. It can be used to pause the current thread until a
* sound effect finishes playing.
*
* RETURNS: The number of ticks we had to wait until the playback finished.
*/

int
i2sWait (void)
{
        int waits = 0;

        while (play != 0) {
                chThdSleep (1);
                waits++;
        }

        return (waits);
}

/******************************************************************************
*
* i2sPlay - play an audio file
*
* This is a shortcut version of i2sLoopPlay() that always plays a file just
* once. As with i2sLoopPlay(), playback can be halted by calling i2sPlay()
* with <file> set to NULL.
*
* RETURNS: N/A
*/

void
i2sPlay (char * file)
{
        i2sLoopPlay (file, I2S_PLAY_ONCE);
        return;
}

/******************************************************************************
*
* i2sLoopPlay - play an audio file
*
* This function cues up an audio file to be played by the background I2S
* thread. The file name is specified by <file>. If <loop> is I2S_PLAY_ONCE,
* the sample file is played once and then the player thread will go idle
* again. If <loop> is I2S_PLAY_LOOP, the same file will be played over and
* over again.
*
* Playback of any sample file (including one that is looping) can be halted
* at any time by calling i2sLoopPlay() with <file> set to NULL.
*
* RETURNS: N/A
*/

void
i2sLoopPlay (char * file, uint8_t loop)
{
        play = 0;
        i2sloop = loop;
        fname = file;
        chMsgSend (pThread, MSG_OK);

        return;
}
