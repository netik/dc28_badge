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

#include "stm32flash_lld.h"
#include "ff.h"
#include "ffconf.h"

#include "updater.h"

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

	chprintf (chp, "BOOT.BIN image found\r\n");

	/* Erase the flash */

	chprintf (chp, "Erasing flash... ");

	flashStartEraseAll (&FLASHD1);

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
