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
 * User configuration support module.
 *
 * In previous badge designs, the user configuration data was saved
 * in the CPU's on-board flash. Unfortunately that'd a little difficult
 * to do with the STM32F746's flash design. In the Kinetis KW01 and
 * Nordic nRF52840 chips, the flash was divided up into equal sized
 * sectors (1024 bytes per sector for the KW01 and 4096 bytes for the
 * nRF52840). The ST Micro chip has 1MB of flash, but it's divided
 * up into only 8 sectors. Some are 32KB, some are 256KB and one is
 * 128KB. With the previous designs we could simply reserve one sector
 * at the end of the firmware and use that to store user data, but
 * here the last sector is 256KB in size, so that would be a huge waste.
 *
 * Instead, we elect to keep the user configuration data on the SD card.
 * This greatly simplifies access, since we can just use normal file
 * operations to load and save it.
 *
 * One disadvantage though is that a user can easily eject the SD card
 * from the badge, mount it on their own computer, and then edit the
 * configuration manually. Since the user configuration structure typically
 * contains game stats, this can lead to easy cheating (which is why we
 * wanted to avoid it before). One way around this is to checksum the
 * configuration structure using an algorithm that the user doesn't know,
 * and reject any configuration with a bad checksum.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "ch.h"
#include "hal.h"

#include "crc32.h"
#include "badge.h"

#include "userconfig.h"

/*
 * Handle for userconfig structure. We allocate this from
 * the BSS and then hold onto it for the life of the system.
 */

static userconfig u;
static mutex_t config_mutex;

/*
 * Create a brand new configuration structure with default settings.
 */

static void
init_config (void)
{
	userconfig * new;

	new = &u;
	memset (new, 0, sizeof (userconfig));

	new->cfg_signature = CONFIG_SIGNATURE;
	new->cfg_version = CONFIG_VERSION;
	new->cfg_sound = CONFIG_SOUND_ON;
	new->cfg_airplane = CONFIG_RADIO_ON;

	return;
}

/* Load the configuration from storage and check its signature */

static int
load_config (void)
{
	int fd;
	int len;
	uint32_t crc;
	STM32_ID * pId;

	fd = open (CONFIG_FILE_NAME, O_RDONLY, 0);

	if (fd == -1) {
		perror ("open");
		return (-1);
	}

	len = read (fd, &u, sizeof (userconfig));

	close (fd);

	if (len != sizeof (userconfig))
		return (-1);

	/* Check for a valid signature */

	if (u.cfg_signature != CONFIG_SIGNATURE) {
		printf ("invalid config signature (expected %lx got %lx)\n",
		    CONFIG_SIGNATURE, u.cfg_signature);
		return (-1);
	}

	/* Check for a version mismatch. */

	if (u.cfg_version != CONFIG_VERSION) {
		printf ("config version mismatch (expected %lu got %lu)\n",
		    CONFIG_VERSION, u.cfg_version);
		return (-1);
	}

	/* Now validate the checksum */

	pId = (STM32_ID *)UID_BASE;
	crc = crc32_le ((uint8_t *)pId, sizeof(STM32_ID), 0);
	crc = crc32_le ((uint8_t *)&u,
	    sizeof(userconfig) - sizeof(uint32_t), crc);

	if (crc != u.cfg_csum) {
		printf ("config CRC mismatch\n");
		return (-1);
	}

	return (0);
}

static int
save_config (void)
{
	int fd;
	int len;
	uint32_t crc;
	STM32_ID * pId;

	/*
	 * Each STM32F746 CPU has 96 bits of unique ID information
	 * that's programmed at the factory. We factor this into the
	 * checksum for the config file.
	 *
	 * This effectively locks the config to a specific badge.
	 *
	 * It also makes it hard for the user to tamper with the
	 * config settings because they need to know the chip ID
	 * info to correctly compute the checksum, and there's
	 * no way to figure that out on a production badge unless
	 * you do one of the following:
	 *
	 * - Solder on a header for the on-board UART and look at
	 *   at the debug boot messages to see the chip info
	 * - Solder on a JTAG header and use a debugger to dump
	 *   the UID_BASE region
	 * - Modify the firmware to print out the chip ID info
	 *   using a custom app or command
	 *
	 * Of course if you are able to recompile and reflash the
	 * firmware you can just delete the checksum validation from
	 * the load_config() function in the first place.
	 *
	 * There's no way to completely block the user from hacking
	 * the config since we deliberately want the badge and its
	 * firmware to hackable, but this at least requires the
	 * user to put a little effort into it.
	 */

	pId = (STM32_ID *)UID_BASE;
	crc = crc32_le ((uint8_t *)pId, sizeof(STM32_ID), 0);
	u.cfg_csum = crc32_le ((uint8_t *)&u,
	    sizeof(userconfig) - sizeof(uint32_t), crc);

	fd = open (CONFIG_FILE_NAME, O_WRONLY|O_CREAT, 0);

	if (fd == -1)
		return (-1);

	len = write (fd, &u, sizeof(userconfig));

	close (fd);

	if (len != sizeof (userconfig))
		return (-1);

	return (0);
}

void
configStart (int op)
{
	osalMutexObjectInit (&config_mutex);

	init_config ();

	/* check for SD card */

	if (!blkIsInserted (&SDCD1)) {
		printf ("no SD card found -- using default config\n");
		return;
	}

	switch (op) {
		case CONFIG_LOAD:
			if (load_config () != 0) {
				printf ("loading config failed -- "
				    "using defaults\n");
				init_config ();
				(void) save_config ();
			} else
				printf ("Config OK!\n");
			break;
		case CONFIG_INIT:
			printf ("initializing configuration\n");
			unlink (CONFIG_FILE_NAME);
			if (save_config () != 0)
				printf ("saving config failed\n");
			break;
		default:
			break;
	}

	return; 
}

userconfig *
configGet (void)
{
	if (u.cfg_signature != CONFIG_SIGNATURE)
		return (NULL);

	return (&u);
}

void
configLoad (void)
{
	if (u.cfg_signature != CONFIG_SIGNATURE)
		return;

	osalMutexLock (&config_mutex);
	if (load_config () != 0) {
		printf ("loading config failed -- using defaults");
		init_config ();
	}
	osalMutexUnlock (&config_mutex);

	return;
}

void
configSave (void)
{
	if (u.cfg_signature != CONFIG_SIGNATURE)
		return;

	osalMutexLock (&config_mutex);
	save_config ();
	osalMutexUnlock (&config_mutex);

	return;
}
