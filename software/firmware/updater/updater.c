/*-
 * Copyright (c) 2016-2020
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

#include "hal.h"
#include "chprintf.h"

#include "hal_stm32_ltdc.h"

#include "stm32flash_lld.h"
#include "ff.h"
#include "ffconf.h"

#include "updater.h"
#include "badge.h"

/*
 * Flash configuration
 */
    
STM32FLASHDriver FLASHD1;

/*
 * SDIO configuration.
 */

static const SDCConfig sdccfg =
{
	SDC_MODE_4BIT
};

/* LCD driver configuration */

static const ltdc_window_t ltdc_fullscreen_wincfg = {
	0, 320 - 1, 0, 240 - 1
};

static const ltdc_frame_t ltdc_screen_frmcfg_fg = {
	(void *)FB_BASE0,
	320,
	240,
	320 * 2,
	LTDC_FMT_RGB565,
};

static const ltdc_frame_t ltdc_screen_frmcfg_bg = {
	(void *)FB_BASE1,
	320,
	240,
	320 * 2,
	LTDC_FMT_RGB565,
};

static const ltdc_laycfg_t ltdc_screen_laycfg_fg = {
	&ltdc_screen_frmcfg_fg,
	&ltdc_fullscreen_wincfg,
	0,
	0xFF,
	0,
	NULL,
	0,
	LTDC_BLEND_MOD1_MOD2,
	LTDC_LEF_ENABLE
};

static const ltdc_laycfg_t ltdc_screen_laycfg_bg = {
	&ltdc_screen_frmcfg_bg,
	&ltdc_fullscreen_wincfg,
	0,
	0xFF,
	0,
	NULL,
	0,
	LTDC_BLEND_FIX1_FIX2,
	0
};

static const LTDCConfig ltdc_cfg = {
	/* Display specifications.*/
	480,			/**< Screen pixel width.*/
	272,			/**< Screen pixel height.*/
	41,			/**< Horizontal sync pixel width.*/
	10,			/**< Vertical sync pixel height.*/
	13,			/**< Horizontal back porch pixel width.*/
	2,			/**< Vertical back porch pixel height.*/
	32,			/**< Horizontal front porch pixel width.*/
	4,			/**< Vertical front porch pixel height.*/
	0,			/**< Driver configuration flags.*/

	/* ISR callbacks.*/
	NULL,			/**< Line Interrupt ISR, or @p NULL.*/
	NULL,			/**< Register Reload ISR, or @p NULL.*/
	NULL,			/**< FIFO Underrun ISR, or @p NULL.*/
	NULL,			/**< Transfer Error ISR, or @p NULL.*/

	/* Color and layer settings.*/
	LTDC_COLOR_TEAL,
	&ltdc_screen_laycfg_bg,
	&ltdc_screen_laycfg_fg
};

/* Buffers */

__attribute__((aligned(CACHE_LINE_SIZE)))
static uint8_t buf[UPDATER_SIZE];

static FATFS FatFs;

int
main (void)
{
	FIL f;
	UINT br;
	uint8_t * src = buf;
	uint32_t dst = 0x0;
	FATFS * fs = &FatFs;
	BaseSequentialStream * chp;

	/* Initialize subsystems */

	halInit ();
	osalSysEnable ();

	/* Start serial port (for debugging) */

	sdStart (&SD1, NULL);
	chp = (BaseSequentialStream *)&SD1;

	chprintf (chp, "\r\nFirmware updater started\r\n");

	/*
	 * Try to turn on the LCD
	 * Note that we are taking advantage of the fact that
	 * when the main OS loads the updater, the memory
	 * controller will still be turned on and we'll be
	 * able to make use of the external SDRAM as the
	 * graphics frame buffer memory.
	 */

	ltdcInit ();
	ltdcStart (&LTDCD1, &ltdc_cfg);

	/*
	 * Enable display & backlight
	 * Note: specific to STM32F746 Discovery board.
	 */

	palSetPad (GPIOI, GPIOI_LCD_DISP);
	palSetPad (GPIOK, GPIOK_LCD_BL_CTRL);

	chprintf (chp, "Display enabled\r\n");

	sdcStart (&SDCD1, &sdccfg);

	if (!blkIsInserted (&SDCD1)) {
		chprintf (chp, "No SD card found!\r\n");
		goto done;
	}

	sdcConnect (&SDCD1);

	chprintf (chp, "SD card found\r\n");

	stm32FlashObjectInit (&FLASHD1);
	stm32FlashStart (&FLASHD1);

	chprintf (chp, "Flash initialzed\r\n");

	/* Try to mount the SD card and open the firmware image */

	if (f_mount (fs, "0:", 1) != FR_OK ||
	    f_open (&f, "BADGE.BIN", FA_READ) != FR_OK) {
		chprintf (chp, "Firmware not found!\r\n");
		goto done;
        }

	chprintf (chp, "BADGE.BIN image found\r\n");

	/* Erase the flash */

	chprintf (chp, "Erasing flash... ");

	flashStartEraseAll (&FLASHD1);
	if (flashWaitErase ((void *)&FLASHD1) != FLASH_NO_ERROR) {
		chprintf (chp, "erasing flash failed!\n");
		goto done;
	}

	chprintf (chp, "done!\r\n");
	
	/* Read data from the SD card and copy it to flash. */

	chprintf (chp, "Programming... ");

        while (1) {
                f_read (&f, src, UPDATER_SIZE, &br);
                if (br == 0)
                        break;
                flashProgram (&FLASHD1, dst, br, src);
                dst += br;
        }

	chprintf (chp, "done!\r\n");
	
	/* Close the file */

	f_close (&f);

done:

	chprintf (chp, "Resetting!\r\n");

	osalThreadSleepMilliseconds (100);

	/* Reboot and load the new firmware */

	NVIC_SystemReset ();

	/* NOTREACHED */

	return (0);
}
