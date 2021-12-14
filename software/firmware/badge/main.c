/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <stdlib.h>
#include <stdio.h>

#include "ch.h"
#include "hal.h"
#include "shell.h"
#include "portab.h"
#include "usbcfg.h"
#include "badge.h"
#include "badge_finder.h"
#include "badge_console.h"
#include "nullprot_lld.h"

#include "hal_stm32_ltdc.h"
#include "hal_stm32_dma2d.h"
#include "fsmc_sdram.h"

#include "async_io_lld.h"
#include "stm32sai_lld.h"
#include "stm32flash_lld.h"
#include "sddetect_lld.h"
#include "touch_lld.h"
#include "sx1262_lld.h"
#include "usbhid_lld.h"
#include "usbnet_lld.h"
#include "wm8994.h"

#include "gfx.h"

#include "orchard-ui.h"
#include "orchard-app.h"

#include "crc32.h"

#include "userconfig.h"

#include "capture.h"

#include <lwip/opt.h>
#include <lwip/def.h>
#include <lwip/mem.h>
#include <lwip/sys.h>
#include <lwip/tcpip.h>

uint8_t badge_addr[BADGE_ADDR_LEN];

BaseSequentialStream * conin;
BaseSequentialStream * conout;
mutex_t conmutex;

/* linker set for command objects */
orchard_command_start();
orchard_command_end();

/* Resources for UART shell */

static ShellConfig shell_cfg_sd =
{
	(BaseSequentialStream *)&SD1,
	orchard_commands()
};
static thread_t * shell_tp_sd = NULL;

/* Resources for USB shell */

static ShellConfig shell_cfg_usb =
{
	(BaseSequentialStream *)&PORTAB_SDU1,
	orchard_commands()
};
static thread_t * shell_tp_usb = NULL;

thread_reference_t shell_ref_usb;

static event_listener_t shell_el;

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

/*
 * SRAM configuration
 * Not actually used -- we just need a dummy structure to help turn
 * SRAM bank 1 off.
 */

static const SRAMConfig sram_cfg;

#ifndef BOOT_FROM_RAM

/*
 * SDRAM configuration
 */

static const SDRAMConfig sdram_cfg =
{
	/* SDCR register value */

	FMC_ColumnBits_Number_8b |
	FMC_RowBits_Number_12b |
	FMC_SDMemory_Width_16b |
	FMC_InternalBank_Number_4 |
	FMC_CAS_Latency_2 |
	FMC_Write_Protection_Disable |
	FMC_SDClock_Period_2 |
	FMC_Read_Burst_Enable |
	FMC_ReadPipe_Delay_0,

 	/* SDTR register value */

	/* FMC_LoadToActiveDelay = 2 (TMRD: 2 Clock cycles) */
	(2 - 1) |
	/* FMC_ExitSelfRefreshDelay = 7 (TXSR: min=70ns (7x11.11ns)) */
	((7 - 1) <<  4) |
        /* FMC_SelfRefreshTime = 4 (TRAS: min=42ns (4x11.11ns) max=120k (ns))*/
        ((4 - 1) <<  8) |
        /* FMC_RowCycleDelay = 7 (TRC:  min=70 (7x11.11ns)) */
        ((7 - 1) << 12) |
        /* FMC_WriteRecoveryTime = 2 (TWR:  min=1+ 7ns (1+1x11.11ns)) */
        ((2 - 1) << 16) |
        /* FMC_RPDelay = 2 (TRP:  20ns => 2x11.11ns) */
        ((2 - 1) << 20) |
        /* FMC_RCDDelay = 2 (TRCD: 20ns => 2x11.11ns) */
        ((2 - 1) << 24),

	/* SDCMR register */

	(7 << 5) | /* Autorefresh count */
	(FMC_SDCMR_MRD_BURST_LENGTH_1 |
	 FMC_SDCMR_MRD_BURST_TYPE_SEQUENTIAL |
	 FMC_SDCMR_MRD_CAS_LATENCY_2 |
	 FMC_SDCMR_MRD_OPERATING_MODE_STANDARD |
	 FMC_SDCMR_MRD_WRITEBURST_MODE_SINGLE) << 9,

	/* SDTR register */
	(0x603 << 1),
};

#endif

/*
 * Maximum speed SPI configuration (13.5MHz, CPHA=0, CPOL=0, MSB first).
 *
 * The SPI2 controller uses the APB1 clock, which is 27MHz, to drive
 * its baud rate generator. The BR divisor bits in the CR1 register
 * control the baud rate (SCK) output. There are 8 divisor values available,
 * from 2 (BR == 0) to 256 (BR == 7). We default BR to 1, which yields
 * a divisor of 4, for an output SCK of 6.75MHz.
 *
 * We select the speed based on the maxium that the SemTech SX1262
 * radio's SPI interface can handle. According to the manual, the
 * minimum SCK period is 62.5nS, which works out to 16MHz. In theory
 * that means a speed of 13.5MHz should work. However experimentation
 * has shown that communication with the chip is not stable at that
 * frequency. Looking at the sample LoRaWAN code from SemTech, they
 * always program the SPI bus clock for 10MHz instead, which suggests
 * that the datasheets may be a little overzealous.
 *
 * Unfortunately the ST Micro SPI controller doesn't provide enough
 * granularity to select a 10MHz clock, so we have to settle for
 * 6.75MHz, which is the next increment down.
 *
 * The complete list of SCK values is:
 *
 * BR    freq
 * --    ----
 * 000   13.5MHz
 * 001   6.75MHz
 * 010   3.375MHz
 * 011   1.6875MHz
 * 100   843.75KHz
 * 101   421.875KHz
 * 110   210.9375KHz
 * 111   105.46875KHz  
 */

static const SPIConfig hs_spicfg =
{
	false,
	NULL,
	GPIOI,
	GPIOI_ARD_D7,
	SPI_CR1_BR_0 | SPI_CR1_SSM | SPI_CR1_SSI,
	SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0
};

/*
 * I2C configuration
 *
 * The ST Micro I2C configuration is a little tricky. They leave it
 * to the system designer to choose a number of critical timing values,
 * all of which have to be calculated based on the I2C clock source.
 *
 * We configre the I2C controller to use PCLK1 (APB1) which is the
 * low speed bus clock, running at 27MHz.
 *
 * The critical values we need to choose depend on the I2C output clock
 * speed that we want. There are four choices: 10KHz, 100KHz, 400KHz
 * and 1MHz. 10KHz and 100KHz are considered standard modes. 400KHz
 * is fast mode. 1MHz is Fast Mode Plus.
 *
 * The values we need to shoot for are:
 *
 *		10KHz		100KHz		400KHz		1MHz
 * tSCLL	50uS		5.0uS		1250nS		312.5nS
 * tSCLH	49uS		4.0uS		500nS		187.5nS
 * tSDADEL	500nS		500nS		250nS		0nS
 * tSCLDEL	1250nS		1250nS		500nS		187.5nS
 *
 * We also have a prescaler we can use to divide the 27MHz clock down
 * to something more managable. We use a prescaler value of 1, which yields
 * a clock of 27MHz. The period is 37.037 nanoseconds. When programming
 * the values, we have to subtract 1 (these are all divisors, so a value
 * of 0 represents 'divide by 1').
 *
 * The values we get are:
 *
 * Prescaler: divide by 1, minus 1 == 0
 * SCLL: 1250/37.037 == 33.75, rounded up == 34, minus 1 == 33
 * SCLH: 500/37.037 == 13.5, rounded up == 14, minus 1 == 13
 * SDADEL: 250/37.037 == 6.76, rounded up == 7, minus 1 == 6
 * SCLDEL: 500/37.037 == 13.5, rounded up == 14, minus 1 == 13
 */

static const I2CConfig i2cconfig =
{
	STM32_TIMINGR_PRESC(0U) |
	STM32_TIMINGR_SCLDEL(13U) | STM32_TIMINGR_SDADEL(6U) |
	STM32_TIMINGR_SCLH(13U)  | STM32_TIMINGR_SCLL(33U),
	0,
	0
};

/*
 * Timer configuration
 * The STM32F746 supports two high precision (32-bit) timers. One is
 * used as the system clock. We use the other (TIM5) as a general
 * purpose timer. (There are several low precision (16-bit) timers too.)
 */

static const GPTConfig gptcfg =
{
	4000000,	/* 4MHz timer clock.*/
	NULL,		/* Timer callback function. */
	0,
	0
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

/* DMA2D configuration */

static const DMA2DConfig dma2d_cfg = {
	/* ISR callbacks.*/
	NULL,		/**< Configuration error, or @p NULL.*/
	NULL,		/**< Palette transfer done, or @p NULL.*/
	NULL,		/**< Palette access error, or @p NULL.*/
	NULL,		/**< Transfer watermark, or @p NULL.*/
	NULL,		/**< Transfer complete, or @p NULL.*/
	NULL		/**< Transfer error, or @p NULL.*/
};

THD_FUNCTION(shellSdThreadStub, p)
{
	shellThread (p);

	return;
}

THD_FUNCTION(shellUsbThreadStub, p)
{
	uint8_t c;

	/*
	 * We launch the USB shell thread right away,
	 * but we want to pause it until the USB serial
	 * port is actually connected. If we don't, the
	 * thread will just exit right away because
	 * calling streamRead() on the USB serial device
	 * before it's actually connected will just return
	 * error, which will cause the thread to exit and
	 * be respawned over and over again (until the
	 * cable is actually plugged in).
	 */

	if (PORTAB_SDU1.config->usbp->state != USB_ACTIVE)
		osalThreadSuspendS (&shell_ref_usb);

	/*
	 * This works around what looks like a bug
	 * in the FreeBSD USB CDC driver. If we print out
 	 * data immediately upon USB connection, it will
	 * fill the RX buffer on the host side with some
 	 * data. But there won't be any program running
 	 * to consume the data yet. Once we do finally
	 * connect a terminal program (kermit, tip, etc...),
	 * the RX and TX paths will appear out of sync.
	 * In most cases, a USB modem stays idle when
	 * first connected (instead of printing anything
 	 * right away, it waits for the host to send an
	 * AT command.) To emulate this, we wait here for
	 * the other side to send a character before we
	 * send anything ourselves.
	 */

	if (streamRead ((BaseSequentialStream *)&PORTAB_SDU1, &c, 1) == 0) {
		shellExit (MSG_OK);
		return;
	}

	osalMutexLock (&conmutex);
	conin = (BaseSequentialStream *)&PORTAB_SDU1;
	conout = (BaseSequentialStream *)&PORTAB_SDU1;
	osalMutexUnlock (&conmutex);

	shellThread (p);

	return;
}

/*
 * Application entry point.
 */

int
main (void)
{
	STM32_ID * pId;
	uint32_t crc;
	unsigned seed;
	const flash_descriptor_t * pFlash;
	userconfig * cfg;
	uint32_t i;
#ifdef BOOT_FROM_RAM
	uint32_t sdmap = 0xFFFFFFFF;
#endif

	/*
	 * System initializations.
	 * - HAL initialization, this also initializes the configured
	 *   device drivers and performs the board-specific initializations.
	 * - Kernel initialization, the main() function becomes a thread and
	 *   the RTOS is active.
	 */

	halInit();
	chSysInit();

	/*
	 * Enable division by 0 traps. We don't enable unaligned access
	 * traps because there are some pieces of code in the OS that trigger
	 * unaligned accesses.
	 */
    
	SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk /*| SCB_CCR_UNALIGN_TRP_Msk*/;

	/*
	 * Enable memory management, usage and bus fault exceptions, so that
	 * we don't always end up diverting through the hard fault handler.
	 * Note: the memory management fault only applies if the MPU is
	 * enabled, which it currently is (for stack guard pages).
	 */

	SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk |
	    SCB_SHCSR_BUSFAULTENA_Msk |
	    SCB_SHCSR_MEMFAULTENA_Msk;

	/*
	 * Enable FPU interrupts. These signal events like underflow and
	 * overflow. Technically it's safe to ignore them, but we really
	 * should at mimimum clear them when they occur. If we don't,
	 * by leaving the interrupt outstanding, we won't be able to
	 * enter low power sleep mode.
	 */

	nvicEnableVector (FPU_IRQn, 6);

	/*
	 * Activates the serial driver 1 using the driver default
	 * configuration (115200 8N1).
	 */

	sdStart (&SD1, NULL);
	conin = (BaseSequentialStream *)&SD1;
	conout = (BaseSequentialStream *)&SD1;
	osalMutexObjectInit (&conmutex);

	/* Configure memory mappings. */

	__disable_irq ();

	/* Delete any existing MPU configuration. */

	for (i = 0; i < MPU_TYPE_DREGION(MPU->TYPE); i++) {
#ifdef BOOT_FROM_RAM
		MPU->RNR = ((uint32_t)i);

		/*
		 * If we've been booted from U-Boot, we'll be running
		 * from SDRAM, and U-Boot will have set up an MPU mapping
		 * for the SDRAM window. If so, avoid clobbering it.
		 * According to section 2.2.3 of the Cortex-M7 Generic
		 * User's Guide, default behavior for the region from
		 * 0xA0000000 to 0xDFFFFFFF is eXecute Never (XN), and
		 * the SDRAM bank starts at 0xC0000000. So if we turn
		 * off that MPU entry and we're running out of RAM,
		 * we will die screaming. We will create our own mapping
		 * below though, and at that point we can safely remove
		 * the previous one.
		 */

		if ((MPU->RBAR & MPU_RBAR_ADDR_MASK) == FSMC_Bank5_MAP_BASE) {
			sdmap = i;
			continue;
		}
#endif
		mpuConfigureRegion (i, 0, 0);
	}

	/*
	 * By default, the SDRAM region won't be cached. We want
	 * it to be because this improves performance.
	 */

	mpuConfigureRegion (MPU_REGION_3, FSMC_Bank5_MAP_BASE,
	    MPU_RASR_ATTR_AP_RW_RW | MPU_RASR_ATTR_CACHEABLE_WB_WA |
	    MPU_RASR_SIZE_8M | MPU_RASR_ENABLE);

#ifdef BOOT_FROM_RAM

	/*
	 * If we were booted by U-Boot, there will be a pre-existing
	 * mapping for the SRAM in slot 0. We can delete it now.
	 */

	if (sdmap != 0xFFFFFFFF)
		mpuConfigureRegion (sdmap, 0, 0);

#endif

	/*
	 * The portion of the SDRAM that's used for the
	 * graphics frame buffer should be uncached. (Per the
	 * Cortex-M7 manual, when two MPU regions overlap, the
	 * one with the highest number takes precedence, so the
	 * two entries below will override the one above.)
	 *
	 * To try to gain maybe a little bit of performance,
	 * we map the graphics frame buffers as write-through
	 * cached. This means that writes to the frame buffer
	 * will be written out immediately, and reads will be
	 * cached.
	 */

	mpuConfigureRegion (MPU_REGION_4, FB_BASE,
	    MPU_RASR_ATTR_AP_RW_RW | MPU_RASR_ATTR_CACHEABLE_WT_NWA |
	    MPU_RASR_SIZE_64K | MPU_RASR_ENABLE);

	mpuConfigureRegion (MPU_REGION_5, FB_BASE + 0x10000,
	    MPU_RASR_ATTR_AP_RW_RW | MPU_RASR_ATTR_CACHEABLE_WT_NWA |
	    MPU_RASR_SIZE_256K | MPU_RASR_ENABLE);

	SCB_CleanInvalidateDCache ();

	/*
	 * Enable NULL pointer protection. We use the MPU to make the first
	 * 256 bytes of the address space unreadable and unwritable. If someone
	 * tries to dereference a NULL pointer, it will result in a load
	 * or store at that location, and we'll get a memory manager trap
	 * right away instead of possibly dying somewhere else further
	 * on down the line.
	 */

	nullProtStart ();

	/* If guard pages aren't turned on, we need to do this ourselves. */

	mpuEnable (MPU_CTRL_PRIVDEFENA);

	__enable_irq ();

	/* Enable SDRAM */

	sdramInit ();

#ifdef BOOT_FROM_RAM
	SDRAMD1.state = SDRAM_READY;
#else
	sdramStart (&SDRAMD1, &sdram_cfg);
#endif

	/*
	 * Per the manual, SRAM/PSRAM/NOR bank 1 is always enabled
	 * after reset. Presumably this is done so that executable
	 * code in NOR flash can be available as soon as the chip
	 * comes up. However we aren't using this bank, so we really
	 * should shut it off. Ideally the FSMC driver should do this
	 * for us, but it doesn't. So we pretend to have SRAM bank 1
	 * present just so that we can use the FSMC driver to shut
	 * it off.
	 *
	 * Why does this matter? I noticed that Doom and the touch tone
	 * dialer app would exhibit glitchy graphics behavior. It's hard
	 * to explain, but basically if we're using graphics and sound
	 * at about the same time (and possibly also in conjunction
	 * with heavy CPU activity), the screen would sometimes
	 * appear jumpy. A good way to see this is to take out the line
	 * below, then run the tone dialer app and randomly press some
	 * buttons very quickly. You may notice the screen become
	 * distorted for a split second as soon as a button is tapped.
	 * It looks like the LCD controller basically paints a garbled
	 * version of the screen for a frame or two, but eventually
	 * redraws it correctly.
	 *
	 * With Doom, you could see this effect while the Demo is
	 * running. Every so often the display would appear "jumpy,"
	 * again only for a few frames at a time. This effect would go
	 * away if sound was disabled.
	 *
	 * I discovered that completely disabling the data cache
	 * seemed to mitigate this, but at the cost of reduced
	 * performance (which was mainly noticeable when running
	 * the Nintendo emulator; the sound was too slow).
	 *
	 * After further experimentation, I discovered I could also
	 * work around the problem by using the MPU to map the region at
	 * 0x60000000 as uncached. Per the manual, this corresponds to
	 * SRAM/PSRAM/NOR Bank 1 in the FSMC controller. From there I
	 * tracked down the part of the manual that says that this
	 * bank is enabled by default at POR.
	 *
	 * Exactly what goes wrong if you don't turn it off isn't clear,
	 * but note that we are using external SDRAM via the FSMC
	 * controller for the graphics frame buffer memory. It's possible
	 * that having the SRAM bank enabled when there isn't really
	 * any SRAM connected somehow causes glitchy behavior inside
	 * the FSMC memory controller during heavy SDRAM usage.
	 *
 	 * In any case, disabling the SRAM bank seems to correct the
	 * problem at the source.
	 */

	sramInit ();
	sramStart (&SRAMD1, &sram_cfg);
	sramStop (&SRAMD1);

	/* Initialize newlib (libc) facilities. */

	newlibStart ();

	/* Safe to use printf() from here on. */

	printf ("\n\nUntitled Ides of DEF CON 30 Badge Game\n\n");

	/*
	 * The STM32F746 has a 96-bit unique device ID. It's actually
	 * derived from three things:
	 *
 	 * 1) The chip's manufacture lot number (7-byte ASCII string)
	 * 2) The silicon wafer number in the lot (1 byte)
	 * 3) The chip's X/Y coordinates on the wafer (2 bytes)
	 *
	 * The complete set of values is guaranteed to be different
	 * for every chip.
	 */
	
	pId = (STM32_ID *)UID_BASE;
	printf ("Device ID: ");
	printf ("Wafer X/Y: %d/%d ", pId->stm32_wafer_x, pId->stm32_wafer_y);
	printf ("Wafer num: %d ", pId->stm32_wafernum);
	printf ("Lot number: %c%c%c%c%c%c%c\n",
	    pId->stm32_lotnum[0], pId->stm32_lotnum[1], pId->stm32_lotnum[2],
	    pId->stm32_lotnum[3], pId->stm32_lotnum[4], pId->stm32_lotnum[5],
	    pId->stm32_lotnum[6]);

	/*
	 * Hash the unique ID and create a MAC address out of it.
	 * We use CRC32 as a hash routine for now, and take the
	 * lower 24 bits as the device portion of the address.
	 */

	crc = crc32_le ((const uint8_t *)pId, sizeof (STM32_ID), 0x0);
	badge_addr[0] = BADGE_OUI_0;
	badge_addr[1] = BADGE_OUI_1;
	badge_addr[2] = BADGE_OUI_2;
	badge_addr[3] = crc & 0xFF;
	badge_addr[4] = (crc & 0xFF00) >> 8;
	badge_addr[5] = (crc & 0xFF0000) >> 16;

	printf ("Station address: %02X:%02X:%02X:%02X:%02X:%02X\n",
	    badge_addr[0], badge_addr[1], badge_addr[2],
	    badge_addr[3], badge_addr[4], badge_addr[5]);

	printf ("Flash size: %d KB\n", *(uint16_t *)FLASHSIZE_BASE);

	printf ("CPU speed: %lu MHz\n", badge_cpu_speed_get ());

	printf ("Debugger: %s\n",
	    CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk ?
	    "connected" : "disconnected");

	/* Enable flash driver */

	stm32FlashObjectInit (&FLASHD1);
	stm32FlashStart (&FLASHD1);
	pFlash = flashGetDescriptor (&FLASHD1);

	if (pFlash->sectors_count > 0) {
		printf ("On-board STM32F746 flash detected: "
		    "%ld KB mapped at %p\n", pFlash->size / 1024,
		    pFlash->address);
	}

	/* Enable timer */

	gptStart (&GPTD5, &gptcfg);

	printf ("Timer TIM5 enabled\n");

	/* Enable RTC */

	rtcInit ();

	printf ("RTC enabled\n");

	/* Enable SPI */

	palSetLineMode (LINE_ARD_D11, PAL_MODE_ALTERNATE(5) |
	    PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
	palSetLineMode (LINE_ARD_D12, PAL_MODE_ALTERNATE(5) |
	    PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
	palSetLineMode (LINE_ARD_D13, PAL_MODE_ALTERNATE(5) |
	    PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
	palSetLineMode (LINE_ARD_D7, PAL_MODE_OUTPUT_PUSHPULL);

	spiStart(&SPID2, &hs_spicfg);

	printf ("SPI bus 2 enabled\n");

	/* Enable I2C instance 1 (arduino pins) */

	palSetPadMode (GPIOB, GPIOB_ARD_D14, PAL_MODE_ALTERNATE(4));
	palSetPadMode (GPIOB, GPIOB_ARD_D15, PAL_MODE_ALTERNATE(4));

	i2cStart (&I2CD1, &i2cconfig);

	printf ("I2C bus 1 enabled\n");

	/*
	 * Enable I2C instance 3 (audio codec)
	 * Note: GPIO pins are already configured in board.c/board.h
	 */

	i2cStart (&I2CD3, &i2cconfig);

	printf ("I2C bus 3 enabled\n");

	/* Initialize audio subsystem */

	wm8994Init ();
	saiStart (&SAID2);

	printf ("SAI2 block A enabled\n");

	/* Enable random number generator, and seed rand()/random() with it */

	trngStart (&TRNGD1, NULL);
        trngGenerate (&TRNGD1, sizeof (seed), (uint8_t *)&seed);
	srand (seed);
        trngGenerate (&TRNGD1, sizeof (seed), (uint8_t *)&seed);
	srandom (seed);

	printf ("Random number generator enabled\n");

	/* Initialize Analog to Digital converter */

	adcStart (&ADCD1, NULL);

	printf ("Analog to digital converter ADC1 enabled\n");

	/*
	 * Initialize serial-over-USB CDC driver.
	 */

	sduObjectInit (&PORTAB_SDU1);
	sduStart (&PORTAB_SDU1, &serusbcfg);

	printf ("USB CDC enabled\n");

	/*
	 * Activate the USB driver and then the USB bus pull-up on D+.
	 */

	usbDisconnectBus (serusbcfg.usbp);
	usbStart (serusbcfg.usbp, &usbcfg);
	usbConnectBus (serusbcfg.usbp);

	printf ("USB client controller enabled\n");

	/*
	 * Initialize the SDIO driver.
	 */

	sdcStart (&SDCD1, &sdccfg);

	printf ("SD card controller enabled\n");

	if (!blkIsInserted (&SDCD1))
		printf ("SD card not present.\n");
	else {
		printf ("SD card inserted - ");
		sdcConnect (&SDCD1);
		printf("Capacity: %ldMB\r\n", SDCD1.capacity / 2048);
	}

	/* Initialize async I/O module */

	asyncIoStart ();

	printf ("Async I/O subsystem enabled\n");

	/* Initialize the LCD display and DMA2D engine */

	ltdcInit ();
	ltdcStart (&LTDCD1, &ltdc_cfg);

	printf ("LCD display controller enabled\n");

	dma2dInit ();
	dma2dStart (&DMA2DD1, &dma2d_cfg);

	printf ("DMA2D acceleration engine enabled\n");

	/* Activate the display and backlight */

	palSetPad (GPIOI, GPIOI_LCD_DISP);
	palSetPad (GPIOK, GPIOK_LCD_BL_CTRL);

	printf ("Main screen turn on\n");

	/* Initialize uGFX subsystem */

	gfxInit ();

	printf ("uGFX graphics enabled\n");

	/* Initialize SD card insert/remove sensor */

	sdDetectStart ();

	printf ("SD card detect enabled\n");

	/* Initialize touch controller event thread */

	touchStart ();

	printf ("LCD touch panel enabled\n");

	badge_deepsleep_init ();
	badge_sleep_enable ();

	printf ("Power management enabled\n");

	/* Initialize TCP/IP stack */

	tcpip_init (NULL, NULL);

	printf ("lwIP TCP/IP stack enabled\n");

	/* Initialize radio (and lwIP network interface) */

	sx1262Start (&SX1262D1);

	/* Load user configuration */

	if (palReadLine (LINE_BUTTON_USER) == 1) {
		configStart (CONFIG_INIT);
		i2sPlay ("sound/wilhelm.snd");
		i2sWait ();
	} else
		configStart (CONFIG_LOAD);

	/* Take into account and custom radio settings */

	sx1262Disable (&SX1262D1);

	cfg = configGet ();

	if (cfg->cfg_radio_mode == CONFIG_RADIO_MODE_GFSK)
		SX1262D1.sx_mode = SX_MODE_GFSK;

	if (cfg->cfg_radio_power == CONFIG_RADIO_PWR_HI)
		SX1262D1.sx_tx_power = SX_TX_POWER_22DB;

	if (cfg->cfg_airplane == CONFIG_RADIO_ON)
		sx1262Enable (&SX1262D1);

	/* Start the badge finder/pinger service. */

	badge_finder_start ();

	/* Initialize the GUI output console */

	badge_coninit ();
	capture_queue_init ();

	/* Initialize usb serial network interface */

	usbnetStart (&USBNETD1);

	/* Initialize host USB controller */

	usbhStart (&USBHD2);
	usbHostStart ();

	printf ("USB host controller enabled\n");

	/*
	 * Initialize orchard subsystem
	 * Note that at boot time, we temporarily set the default
	 * app to the "Intro" app, which plays the Dragnet video.
	 * After this, the default app will be the launcher.
	 */

	uiStart ();
 	orchardAppInit ();
	instance.app = orchardAppByName ("Intro");
	orchardAppRestart ();

	/* Initialize shell subsystem */

	shellInit ();
	chEvtRegister (&shell_terminated, &shell_el, 0);

	/*
	 * Normal main() thread activity. Start and monitor
	 * shell threads. The USB shell thread will only wake
	 * up and run when a USB cable is connected. At that
	 * point, the USB shell becomes the "console" device
	 * until it's unplugged again.
	 */

	while (true) {
		if (shell_tp_sd == NULL) {
			shell_tp_sd = chThdCreateFromHeap (NULL,
			    THD_WORKING_AREA_SIZE(3072), "UartShell",
			    NORMALPRIO + 5, shellSdThreadStub,
			    (void *)&shell_cfg_sd);
		}

		if (shell_tp_usb == NULL) {
			shell_tp_usb = chThdCreateFromHeap (NULL,
			    THD_WORKING_AREA_SIZE(3072), "UsbShell",
			    NORMALPRIO + 5, shellUsbThreadStub,
			    (void *)&shell_cfg_usb);
		}

		chEvtWaitAny (EVENT_MASK(0));

		if (chThdTerminatedX (shell_tp_sd)) {
			chThdRelease (shell_tp_sd);
			shell_tp_sd = NULL;
		}

		if (chThdTerminatedX (shell_tp_usb)) {
			chThdRelease (shell_tp_usb);
			shell_tp_usb = NULL;
			osalMutexLock (&conmutex);
			conin = (BaseSequentialStream *)&SD1;
			conout = (BaseSequentialStream *)&SD1;
			osalMutexUnlock (&conmutex);
		}
	}

	/* NOTREACHED */
}
