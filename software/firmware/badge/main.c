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
#include "nullprot_lld.h"

#include "hal_fsmc_sdram.h"
#include "fsmc_sdram.h"

#include "async_io_lld.h"
#include "stm32sai_lld.h"
#include "sddetect_lld.h"
#include "touch_lld.h"
#include "sx1262_lld.h"
#include "wm8994.h"

#include "gfx.h"

#include "orchard-ui.h"
#include "orchard-app.h"

#include "crc32.h"

#include <lwip/opt.h>
#include <lwip/def.h>
#include <lwip/mem.h>
#include <lwip/sys.h>
#include <lwip/tcpip.h>

uint8_t badge_addr[BADGE_ADDR_LEN];

BaseSequentialStream * console;

/* linker set for command objects */
orchard_command_start();
orchard_command_end();

extern ShellCommand _orchard_cmd_list_cmd_cpu;

/* Resources for UART shell */
static THD_WORKING_AREA(shell_wa_sd, 3072);
static ShellConfig shell_cfg_sd =
{
	(BaseSequentialStream *)&SD1,
	orchard_commands()
};
static thread_t * shell_tp_sd = NULL;

/* Resources for USB shell */

static THD_WORKING_AREA(shell_wa_usb, 3072);
static ShellConfig shell_cfg_usb =
{
	(BaseSequentialStream *)&SDU1,
	orchard_commands()
};
static thread_t * shell_tp_usb = NULL;

thread_reference_t shell_ref_usb;

static event_listener_t shell_el;

/*
 * Working area for SD card driver.
 */

static uint8_t sd_scratchpad[512];

/*
 * SDIO configuration.
 */
static const SDCConfig sdccfg =
{
	sd_scratchpad,
	SDC_MODE_4BIT
};

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

/*
 * Maximum speed SPI configuration (13.5MHz, CPHA=0, CPOL=0, MSB first).
 *
 * The SPI controller uses the APB2 clock, which is 108MHz, to drive
 * its baud rate generator. The BR divisor bits in the CR1 register
 * control the baud rate (SCK) output. There are 8 divisor values available,
 * from 2 (BR == 0) to 256 (BR == 7). We default BR to 2, which yields
 * a divisor of 8, for an output SCK of 13.5MHz.
 *
 * (The speed of 13.5MHz was chosen because the SemTech SX1262's SPI
 * interface supports a maximum speed of 16MHz, so any of the settings
 * above 13.5MHz would be too fast for reliable data transfer.)
 *
 * The complete list of SCK values is:
 *
 * BR    freq
 * --    ----
 * 000   54MHz
 * 001   27MHz
 * 010   13.5MHz
 * 011   6.75MHz
 * 100   3.375MHz
 * 101   1.6875MHz
 * 110   843.75KHz
 * 111   421.875KHz
 */

static const SPIConfig hs_spicfg =
{
	false,
	NULL,
	GPIOI,
	GPIOI_ARD_D7,
	SPI_CR1_BR_1 | SPI_CR1_SSM | SPI_CR1_SSI,
	SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0
};

/*
 * I2C configuration
 *
 * The ST Micro I2C configuration is a little tricky. They leave it
 * to the system designer to choose a number of critical timing values,
 * all of which have to be calculated based on the I2C clock source.
 *
 * We configre the I2C controller to use PCLK1 (AHB1) which is the
 * low speed bus clock, running at 54MHz.
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
 * We also have a prescaler we can use to divide the 54MHz clock down
 * to something more managable. We use a prescaler value of 1, which yields
 * a clock of 27MHz. The period is 37.037 nanoseconds. When programming
 * the values, we have to subtract 1 (these are all divisors, so a value
 * of 0 represents 'divide by 1').
 *
 * The values we get are:
 *
 * Prescaler: divide by 2, minus 1 == 1
 * SCLL: 1250/37.037 == 33.75, rounded up == 34, minus 1 == 33
 * SCLH: 500/37.037 == 13.5, rounded up == 14, minus 1 == 13
 * SDADEL: 250/37.037 == 6.76, rounded up == 7, minus 1 == 6
 * SCLDEL: 500/37.037 == 13.5, rounded up == 14, minus 1 == 13
 */

static const I2CConfig i2cconfig =
{
	STM32_TIMINGR_PRESC(1U) |
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

THD_FUNCTION(shellSdThreadStub, p)
{
	chRegSetThreadName ("UartShell");
	shellThread (p);
}

THD_FUNCTION(shellUsbThreadStub, p)
{
	uint8_t c;

	chRegSetThreadName ("UsbShell");

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

	if (SDU1.config->usbp->state != USB_ACTIVE) {
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

		if (streamRead ((BaseSequentialStream *)&SDU1, &c, 1) == 0) {
			shellExit (MSG_OK);
			return;
		}
	}

	console = (BaseSequentialStream *)&SDU1;

	shellThread (p);
}

/*
 * Application entry point.
 */

int
main (void)
{
	STM32_ID * pId;
	uint32_t crc;

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
	 * Enable NULL pointer protection. We use the MPU to make the first
	 * 256 bytes of the address space unreadable and unwritable. If someone
	 * tries to dereference a NULL pointer, it will result in a load
	 * or store at that location, and we'll get a memory manager trap
	 * right away instead of possibly dying somewhere else further
	 * on down the line.
	 */

	nullProtStart ();

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

#ifdef notdef
	/* Enable deep sleep mode when we execute a WFI */

	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
#endif

	/*
	 * Enable FPU interrupts. These signal events like underflow and
	 * overflow. Technically it's safe to ignore them, but we really
	 * should at mimimum clear them when they occur. If we don't,
	 * by leaving the interrupt outstanding, we won't be able to
	 * enter low power sleep mode.
	 */

	NVIC_SetPriority (FPU_IRQn, 6);
	NVIC_EnableIRQ (FPU_IRQn);

	/*
	 * Activates the serial driver 1 using the driver default
	 * configuration (115200 8N1).
	 */

	sdStart (&SD1, NULL);
	console = (BaseSequentialStream *)&SD1;

	/* Enable SDRAM */

	fsmcSdramInit ();
	fsmcSdramStart (&SDRAMD, &sdram_cfg);

	/*
	 * By default, the SDRAM region won't be cached. We want
	 * it to be because this improves performance.
	 */

 	mpuConfigureRegion (MPU_REGION_4, FSMC_Bank5_MAP_BASE,
	    MPU_RASR_ATTR_AP_RW_RW | MPU_RASR_ATTR_CACHEABLE_WB_WA |
            MPU_RASR_SIZE_8M | MPU_RASR_ENABLE);

	/*
	 * The portion of the SDRAM that's used for the
	 * graphics frame buffer should be uncached. (Per the
	 * Cortex-M7 manual, when two MPU regions overlap, the
	 * one with the highest number takes precedence.)
	 */

 	mpuConfigureRegion (MPU_REGION_5, FB_BASE,
	    MPU_RASR_ATTR_AP_RW_RW | MPU_RASR_ATTR_SHARED_DEVICE |
            MPU_RASR_SIZE_512K | MPU_RASR_ENABLE);

	/* Initialize newlib (libc) facilities. */

	newlibStart ();

	/* Safe to use printf() from here on. */

	printf ("\n\nUntitled Ides of DEF CON 28 Badge Game\n\n");

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

	printf ("Debugger: %s\n",
	    CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk ?
	    "connected" : "disconnected");

	_orchard_cmd_list_cmd_cpu.sc_function (NULL, 0, NULL);

	/* Enable timer */

	gptStart (&GPTD5, &gptcfg);

	printf ("Timer TIM5 enabled\n");

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

	/* Enable random number generator */

	trngStart (&TRNGD1, NULL);

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

	printf ("USB controller enabled\n");

	/*
	 * Initializes the SDIO drivers.
	 */

	sdcStart(&SDCD1, &sdccfg);

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

	/* Initialize uGFX subsystem */

	gfxInit ();

	/* Enable layer 0, since that's the default display */

	LTDC_Layer1->CR = LTDC_LxCR_LEN;
	LTDC_Layer2->CR = 0;
	LTDC->SRCR = LTDC_SRCR_VBR;

	printf ("Main screen turn on\n");

	/* Initialize touch controller event thread */

	touchStart ();

	printf ("LCD touch panel enabled\n");

	/* Initialize SD card sensor */

	sdDetectStart ();

	printf ("SD card detect enabled\n");

	badge_deepsleep_init ();
	badge_sleep_enable ();

	printf ("Power management enabled\n");

	/* Initialize TCP/IP stack */

	tcpip_init (NULL, NULL);

	printf ("lwIP TCP/IP stack enabled\n");

	/* Initialize radio (and lwIP network interface) */

	sx1262Start (&SX1262D1);

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
			shell_tp_sd = chThdCreateStatic (shell_wa_sd,
			    sizeof(shell_wa_sd), NORMALPRIO + 5,
 			    shellSdThreadStub, (void *)&shell_cfg_sd);
		}

		if (shell_tp_usb == NULL) {
			shell_tp_usb = chThdCreateStatic (shell_wa_usb,
			    sizeof(shell_wa_usb), NORMALPRIO + 5,
 			    shellUsbThreadStub, (void *)&shell_cfg_usb);
		}

		chEvtWaitAny (EVENT_MASK(0));

		if (chThdTerminatedX (shell_tp_sd)) {
			chThdRelease (shell_tp_sd);
			shell_tp_sd = NULL;
		}

		if (chThdTerminatedX (shell_tp_usb)) {
			chThdRelease (shell_tp_usb);
			shell_tp_usb = NULL;
			console = (BaseSequentialStream *)&SD1;
		}
	}

	/* NOTREACHED */
}
