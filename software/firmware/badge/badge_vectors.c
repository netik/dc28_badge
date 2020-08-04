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

#include "hal_fsmc_sdram.h"

#include "badge.h"

#include <stdio.h>
#include <stdint.h>

/*
 * Information on fault handling comes from the Cortex-M4 Generic User Guide
 * http://infocenter.arm.com/help/topic/com.arm.doc.dui0553b/DUI0553.pdf
 */

#define HARD_FAULT	0
#define BUS_FAULT	1
#define USAGE_FAULT	2
#define MEMMANAGE_FAULT	3

/*
 * Cortex-M4 exception frame, including floating point state.
 */

typedef struct exc_frame {
	uint32_t		exc_R[4];
	uint32_t		exc_R12;
	uint32_t		exc_LR;
	uint32_t		exc_PC;
	uint32_t		exc_xPSR;
	uint32_t		exc_fpregs[16];
	uint32_t		exc_FPSCR;
	uint32_t		exc_dummy;
} EXC_FRAME;

/*
 * Cortex-M4 exception types.
 */

static char * exc_msg[] = {
	"Instruction access violation",
	"Data access violation",
	NULL,
	"Memory management fault on unstacking during exception return",
	"Memory management fault on stacking during exception entry",
	"Memory management fault during floating point context save",
	NULL,
	NULL,
	"Instruction bus error",
	"Precise data bus error",
	"Imprecise data bus error",
	"Bus error on unstacking during exception return",
	"Bus error on stacking during exception entry",
	"Bus error during floating point context save",
	NULL,
	NULL,
	"Illegal instruction",
	"Invalid EPSR state",
	"Invalid PC load via EXC_RETURN",
	"Unsupported coprocessor",
	NULL,
	NULL,
	NULL,
	NULL,
	"Unaligned access trap",
	"Division by zero",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static char exc_msgbuf[80];

/*
 * This global controls whether or not the idle thread will execute
 * a WFI instruction to put the CPU to sleep when we're idle. This
 * is generally a good idea for power saving.
 */

static volatile uint8_t badge_sleep = FALSE;
static volatile uint8_t badge_lpidle = FALSE;
static volatile uint8_t badge_cpuspeed = BADGE_CPU_SPEED_NORMAL;

/*
 * A few notes on deep sleep power management.
 *
 * We want to be able to put the CPU into STOP mode cleanly. To achieve
 * this, we use a power manager thread whose job is to enable the deepsleep
 * bit in the CPU's system control register and disable certain peripherals
 * and/or put them into low power state. It turn executes a WFI instruction
 * to enter STOP mode. At this point, all the CPU's peripheral clocks will
 * be turned off.
 *
 * The power manager thread runs at the highest thread priority. The
 * reason we use this scheme is so that ChibiOS will be forced to
 * temporarily pause other threads while the power manager thread runs.
 * This (hopefully) prevents any additional I/O operations from being
 * started until the system wakes up again.
 *
 * The CPU can only be woken up from this state by an EXTI interrupt. This
 * can be an external interrupt pin event like a radio or touch screen event,
 * or an EXTI interrupt generated using the RTC wakeup timer. We need to
 * restore the system ASAP once one of these interrupts triggers, so
 * we install a custom interrupt prologue hook into ChibiOS that checks to
 * see if we're waking from sleep mode, and if so it turns things back on
 * again. The prologue hook must not block.
 *
 * Note that for best power savings, we put the external SDRAM into
 * self-refresh mode. We must put it back into normal mode before we can
 * access any data stored in SDRAM again (including the graphics frame
 * buffer). When the CPU is operating in STOP mode, the internal SRAM is
 * still powered on. It's therefore important that we put the power
 * management variables and power thread stack into the internal SRAM
 * rather than the external SDRAM. Fortunately the linker always uses
 * the internal SRAM for static data allocations.
 */

static volatile uint8_t badge_deep_sleep = FALSE;
static thread_t * badge_power_thread;
static thread_reference_t badge_power_thread_reference;

/*
 * FPU interrupt
 * Floating point exceptions can occur for things like overflow
 * and underflow, which are not treated as CPU exceptions like
 * divide by zero. When they occur, this triggers the FPU interrupt
 * in the NVIC. Normally this interrupt is not enabled, so we don't
 * notice when these events occur, but we really should handle them.
 * For one thing, it's not possible to put the CPU into low power
 * modes while FPU events are pending.
 *
 * We do not set the FPSCR register directly. When an FPU interrupt
 * occurs, the CPU pushes the current CPU state, including the FPU
 * context, onto the stack. The FPU context includes an FPSCR word,
 * and this word will be loaded back into the FPSCR register from
 * the stack when the interrupt service routine exits. So modify
 * the stashed FPSCR word on the stack and the FPSCR register instead
 * of just the FPSCR register itself.
 */

#define FPU_EXCEPTION_MASK 0x0000009F

OSAL_IRQ_HANDLER(Vector184)
{
	EXC_FRAME * exc;

	OSAL_IRQ_PROLOGUE();

	/*
	 * The Cortex-M4F supports a feature called lazy stacking.
	 * FPU state is something which should, in some cases,
	 * be preserved when an interrupt occurs. However saving the
	 * FPU state to the stack adds some extra overhead which can
	 * affect interrupt latency. The rule is that if the CPU uses a
	 * floating point instruction and an interrupt occurs, extra
	 * space will be reserved on the stack to hold the floating
	 * point context. However with lazy stacking, that extra
	 * space is not actually filled with the FPU register state
	 * right away. (Allocating the extra space takes no extra
	 * work since all the CPU has to do is set the SP register
	 * to a slightly different value. Actually writing out the
	 * FPU register contents to the stack is another matter.)
	 *
	 * To force the CPU to write the FPU state, we need to perform
	 * some kind of floating point operation on entry to the ISR.
	 * Reading the FPSCR register counts as a "floating point
	 * operation" and has the added benefit that it doesn't also
	 * change the the FPU state. So reading it here forces the
	 * CPU to put the FPU registers onto the stack, including
	 * the current FPSCR contents itself.
	 */

	(void)__get_FPSCR();

	/*
	 * Get the exception stack frame pointer. Note that using
	 * the PSP assumes we will only get a floating point exception
	 * from thread-mode code. This should be true since we
	 * should never be using floating point instructions in
	 * an interrupt handler.
	 */

	exc = (EXC_FRAME *)__get_PSP();

	/*
	 * Clear floating point exceptions. We directly update the
	 * FPSCR register and the FPSCR word in the exception frame.
	 * This prevents the FPSCR status bits from possibly being
	 * changed back to their previous state later when we exit
	 * the ISR.
	 */

	exc->exc_FPSCR &= ~(FPU_EXCEPTION_MASK);
	__set_FPSCR(exc->exc_FPSCR);

	OSAL_IRQ_EPILOGUE();

	return;
}

void
badge_delay (uint16_t ms)
{
	volatile uint32_t freq;
	uint16_t i;

	/* Calculate 1ms delay count based on current CPU frequency */

	freq = badge_cpuspeed * 1000;

	for (i = 0; i < ms; i++) {
		SysTick->LOAD = freq;
		SysTick->VAL = 0;
		SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
		    SysTick_CTRL_ENABLE_Msk;

		while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
			__ISB();
	}

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	return;
}

void
badge_idle (void)
{
	if (badge_sleep == TRUE) {

		/*
		 * If I understand correctly, when going into STOP
		 * mode (deep sleep), the CPU will automatically
		 * issue a self-refresh command to the SDRAM before
		 * the memory controller clock is stopped. But when
		 * going into SLEEP mode (low power idle), and we
		 * have enabled automatic clock gating for the memory
		 * controller, the CPU will turn off the memory
		 * controller clock when we execute a WFI, but it will
		 * not send the self-refresh command.
		 *
		 * This is a problem because a) it means the SDRAM
		 * chip contents might degrade if the chip is idle
		 * for a long time, and b) we want to put the chip
		 * in self-refresh when idle since then it will only
		 * draw about 2mA, as opposed to about 100mA when
	 	 * in normal operation.
		 *
		 * So we manually issue the self-refresh command
		 * here.
		 *
		 * Note that since we have the LCD frame buffers
		 * stored in SDRAM, this means we really need to
		 * also turn off the LCD controller clock since
		 * screen updates won't work correctly while the
		 * SDRAM controller clock is shut down.
		 */

		if (badge_lpidle) {
			__disable_irq ();
			fsmcSdramSelfRefresh (&SDRAMD);
		}
		__DSB();
		__ISB();
		__WFI();
		if (badge_lpidle) {
			fsmcSdramNormal (&SDRAMD);
			__enable_irq ();
		}
	}
	return;
}

void
badge_wakeup (void)
{
	/*
	 * If we entered an ISR and the deepsleep bit is set,
	 * then it means we're waking up from deepsleep mode.
	 * Turn it off and re-enable all the clocks.
	 */

	if (badge_deep_sleep == TRUE && SCB->SCR & SCB_SCR_SLEEPDEEP_Msk) {
		__disable_irq ();
		fsmcSdramNormal (&SDRAMD);
		badge_deep_sleep = FALSE;
		SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

		/* Reset these bits */

		PWR->CSR1 |= PWR_CSR1_UDRDY;

		stm32_clock_init ();

		/*
		 * stm32_clock_init () will reinitialize the power controller			 * CR1 register. It expects that the "disable backup
		 * domain write protection" bit will be set by the halInit()
		 * path, but we don't want to call halInit() since it resets
		 * extra things that we don't want to change. But we need to
		 * make sure the DBP bit is turned back on otherwise we won't
		 * be able to access the RTC registers again.
		 */

	        PWR->CR1 |= PWR_CR1_DBP;

		/*
		 * stm32_clock_init () will also reinitialize the CPU
		 * to high speed. Restore whatever speed was set when
		 * we went to sleep.
		 */

		badge_cpu_speed_set (badge_cpuspeed);

		/*
	 	 * Now restart the clocks on all the peripherals.
		 * Strictly speaking, it looks like mainly just the
		 * timers and SPI bus clocks really need to be reset,
		 * but we may as well do the others to be safe.
		 */

		rccEnableTIM2 (TRUE);
		rccEnableTIM5 (TRUE);
		rccEnableSPI2 (TRUE);

		rccEnableUSART1 (TRUE);
		rccEnableSDMMC1 (TRUE);
		RCC->APB2ENR |= RCC_APB2ENR_LTDCEN;
		rccEnableDMA2D (TRUE);
		rccEnableAPB2 (RCC_APB2ENR_SAI2EN, TRUE);
		rccEnableDMA1 (TRUE);
		rccEnableDMA2 (TRUE);
		rccEnableOTG_FS (TRUE);
		rccEnableRNG (TRUE);
		rccEnableI2C1 (TRUE);
		rccEnableI2C3 (TRUE);
		rccEnableADC1 (TRUE);

		__enable_irq ();
	}

	return;
}

void
badge_sleep_enable (void)
{
	osalSysLock ();
	badge_sleep = TRUE;
	osalSysUnlock ();
	return;
}

void
badge_sleep_disable (void)
{
	osalSysLock ();
	badge_sleep = FALSE;
	osalSysUnlock ();
	return;
}

void
badge_lpidle_enable (void)
{
	osalSysLock ();
	badge_lpidle = TRUE;
	rccEnableFSMC (FALSE);
	rccEnableLTDC (FALSE);
	osalSysUnlock ();
	return;
}

void
badge_lpidle_disable (void)
{
	osalSysLock ();
	badge_lpidle = FALSE;
	rccEnableFSMC (TRUE);
	rccEnableLTDC (TRUE);
	osalSysUnlock ();
	return;
}

uint8_t
badge_lpidle_get (void)
{
	return (badge_lpidle);
}

static
THD_FUNCTION(badge_power_loop, arg)
{
	(void)arg;

	while (1) {
		osalSysLock ();
		osalThreadSuspendS (&badge_power_thread_reference);
		osalSysUnlock ();

		/*
	 	 * Once we wake up, we're the prettiest princess--
		 * er.., we're the highest priority thread in the
		 * system. We control the vertical. We control the
		 * horizontal.
		 *
		 * It's important that we wait for any pending I/O
		 * to complete before we put the CPU into deep sleep
		 * mode. That means we need to wait for all DMA
		 * transfers involving any of the peripherals to
		 * finish, and we don't want to let any tasks start
		 * any new transfers too.
		 *
		 * The best way to do that is to hog the CPU for a
		 * bit. But we can't just use chThdSleep() because that
		 * will just let other tasks take over the CPU again
		 * while we're sleeping.
		 *
	 	 * So instead, we busy wait for a few milliseconds,
		 * using the ARM core's SysTick timer as a stopwatch.
		 */

		badge_delay (100);

		__disable_irq ();

		/* Flush the cache, why not. */

		SCB_CleanInvalidateDCache ();

		badge_deep_sleep = TRUE;
		
		/* Enable deep sleep mode in the Cortex-M7 core. */

		SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

		/*
		 * Enable the following low power options:
		 * - Disable backup domain write protection
	 	 * - Flash power down in stop mode
		 * - Low power regulator in stop mode.
		 * - Low power regulator in under drive mode during
		 *   stop mode.and flash
		 * - Underdrive when in stop mode
		 */

		PWR->CR1 |= PWR_CR1_DBP | PWR_CR1_FPDS | PWR_CR1_LPDS |
		   PWR_CR1_LPUDS | PWR_CR1_UDEN;

		/*
		 * When the CPU enters deep sleep mode, it will
		 * turn off the clock to the LCD controller, which
		 * will produce this weird "melting" effect on the
		 * screen once the controller stops refreshing it.
		 * It's harmless but looks weird, so we turn off
		 * the screen here so the user doesn't have to see it.
		 */

		palClearPad (GPIOI, GPIOI_LCD_DISP);
		palClearPad (GPIOK, GPIOK_LCD_BL_CTRL);

		/*
		 * Executing a WFI here will immediately put us to sleep.
		 * This instruction shouldn't complete until a wakeup event
		 * happens, and by that point we the interrupt prologue hook
		 * should have re-enabled all of the clocks.
		 */

		fsmcSdramSelfRefresh (&SDRAMD);
		__DSB();
		__ISB();
		__WFI();
		__enable_irq ();

		/*
		 * Once we get here, we know we're awake again. Turn the
		 * screen back on. We pause very briefly between the time
		 * we reactivate the display and the time we turn the
		 * backlight on, otherwise the user will see a white
		 * flash.
		 */

		palSetPad (GPIOI, GPIOI_LCD_DISP);
		chThdSleepMilliseconds (180);
		palSetPad (GPIOK, GPIOK_LCD_BL_CTRL);
	}

	/* NOTREACHED */
}

void badge_deepsleep_init (void)
{
	badge_power_thread = chThdCreateFromHeap (NULL,
	    THD_WORKING_AREA_SIZE(256), "PowerEvent",
	    HIGHPRIO, badge_power_loop, NULL);

	return;
}

void
badge_deepsleep_enable (void)
{
	osalSysLock ();
	osalThreadResumeS (&badge_power_thread_reference, MSG_OK);
	osalSysUnlock ();

	return;
}

void
badge_deepsleep_timed (uint16_t delay)
{
	RTCWakeup wkup;

	/*
	 * Set the wakeup timer. The RTC clock has been set to the
	 * low speed external clock, which is a 32.768KHz crystal.
	 * We can apply a special 32768 prescaler to this to yield
	 * a 1s tick time, which allows us to delay from 1 to
	 * 65535 seconds.
	 */

	wkup.wutr = delay - 1;
	wkup.wutr |= (RTC_CR_WUCKSEL_2 << 16);

	rtcSTM32SetPeriodicWakeup (&RTCD1, &wkup);

	/* Enter stop mode - this will block until wakeup. */

	badge_deepsleep_enable ();

	/* Aaaand we're back. Turn off the wakeup timer */

	rtcSTM32SetPeriodicWakeup (&RTCD1, NULL);

	return;
}

void
badge_cpu_show (void)
{
	uint32_t freq;
	uint32_t pll_pll_divisor;
	uint32_t pll_multiplier;
	uint32_t pll_sysclk_divisor;

	/*
	 * The external crystal frequency is defined in board.h.
	 * For the Discovery reference board, it's 25MHz. (The low
	 * speed crystal (STM32_LSECLK) is 32.768KHz.)
	 */

	freq = STM32_HSECLK / 1000000;

	printf ("System clock source is: ");
	switch (RCC->CFGR & RCC_CFGR_SW_Msk) {
		case RCC_CFGR_SW_HSI:
			printf ("High speed internal 16MHz oscillator\n");
			break;
		case RCC_CFGR_SW_HSE:
			printf ("High speed external %luMHz clock\n",
			    freq);
			break;
		case RCC_CFGR_SW_PLL:
			printf ("PLL output\n");
			break;
		default:
			printf ("<unknown>\n");
			return;
			/* NOTREACHED */
			break;
	}

	if ((RCC->CFGR & RCC_CFGR_SW_Msk) == RCC_CFGR_SW_PLL) {
		printf ("PLL source: ");
		switch (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC_Msk) {
			case RCC_PLLCFGR_PLLSRC_HSE:
				printf ("High speed external %luMHz clock\n",
				    freq);
				break;
			case RCC_PLLCFGR_PLLSRC_HSI:
				printf ("High speed internal "
				    "16MHz oscillator\n");
				freq = 16;
				break;
			default:
				break;
		}

		printf ("PLL configuration: 0x%lX\n", RCC->PLLCFGR);

		/* Get PLL input divisor */

		pll_pll_divisor = RCC->PLLCFGR & RCC_PLLCFGR_PLLM_Msk;
		pll_pll_divisor >>= RCC_PLLCFGR_PLLM_Pos;

		printf ("PLL input divisor: %ld\n", pll_pll_divisor);

		/* Get PLL multiplier */

		pll_multiplier = RCC->PLLCFGR & RCC_PLLCFGR_PLLN_Msk;
		pll_multiplier >>= RCC_PLLCFGR_PLLN_Pos;

		printf ("PLL multiplier: %ld\n", pll_multiplier);

		/* Get system clock divisor */

		pll_sysclk_divisor = RCC->PLLCFGR & RCC_PLLCFGR_PLLP_Msk;
		pll_sysclk_divisor >>= RCC_PLLCFGR_PLLP_Pos;

		pll_sysclk_divisor += 1;
		pll_sysclk_divisor *= 2;

		printf ("PLL system clock divisor: %ld\n", pll_sysclk_divisor);

		freq *= 1000000;
		freq /= pll_pll_divisor;
		freq *= pll_multiplier;
		freq /= pll_sysclk_divisor;
		freq /= 1000000;

		printf ("CPU speed: %ldMHz\n", freq);
	}

	return;
}

uint32_t
badge_cpu_speed_get (void)
{
	uint32_t freq;
	uint32_t pll_pll_divisor;
	uint32_t pll_multiplier;
	uint32_t pll_sysclk_divisor;

	/*
	 * The external crystal frequency is defined in board.h.
	 * For the Discovery reference board, it's 25MHz. (The low
	 * speed crystal (STM32_LSECLK) is 32.768KHz.)
	 */

	freq = STM32_HSECLK / 1000000;

	switch (RCC->CFGR & RCC_CFGR_SW_Msk) {
		case RCC_CFGR_SW_HSI:
			freq = 16;
			break;
		case RCC_CFGR_SW_HSE:
			break;
		case RCC_CFGR_SW_PLL:
			break;
		default:
			return (0);
			/* NOTREACHED */
			break;
	}

	if ((RCC->CFGR & RCC_CFGR_SW_Msk) == RCC_CFGR_SW_PLL) {
		switch (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC_Msk) {
			case RCC_PLLCFGR_PLLSRC_HSE:
				break;
			case RCC_PLLCFGR_PLLSRC_HSI:
				freq = 16;
				break;
			default:
				break;
		}

		/* Get PLL input divisor */

		pll_pll_divisor = RCC->PLLCFGR & RCC_PLLCFGR_PLLM_Msk;
		pll_pll_divisor >>= RCC_PLLCFGR_PLLM_Pos;

		/* Get PLL multiplier */

		pll_multiplier = RCC->PLLCFGR & RCC_PLLCFGR_PLLN_Msk;
		pll_multiplier >>= RCC_PLLCFGR_PLLN_Pos;

		/* Get system clock divisor */

		pll_sysclk_divisor = RCC->PLLCFGR & RCC_PLLCFGR_PLLP_Msk;
		pll_sysclk_divisor >>= RCC_PLLCFGR_PLLP_Pos;

		pll_sysclk_divisor += 1;
		pll_sysclk_divisor *= 2;

		freq *= 1000000;
		freq /= pll_pll_divisor;
		freq *= pll_multiplier;
		freq /= pll_sysclk_divisor;
		freq /= 1000000;
	}

	return (freq);
}

void
badge_cpu_dcache (bool on)
{

	__disable_irq ();

       	SCB_CleanInvalidateDCache ();

	if (on == TRUE)
		SCB_EnableDCache();
	else
		SCB_DisableDCache();

	__enable_irq ();

	return;
}

void
badge_cpu_speed_set (int speed)
{
	badge_cpu_dcache (FALSE);

	__disable_irq ();

	/*
	 * As a hack, when we're in slow speed mode, we divide the
	 * SPI bus clock by a factor of instead of 4. This reduces
	 * the bus SPI clock speed from 6.75MHz to 3.375MHz. This
	 * is to compensate for the fact that while we're able to
	 * maintain the same APB bus speed for the SPI controller,
	 * we proportionally reduce the AHB bus clock for the DMA
	 * DMA controller, and at the slow CPU speed (54MHz) the
	 * DMA controller seems too slow to keep up with the
	 * required SPI signalling rate, and we end up triggering
	 * DMA errors. This causes problems using the radio when
	 * operating in slow CPU mode.
	 *
	 * It might have been possible to avoid this if only
	 * ST Micro had used SPI bus 1 for the Arduino headers
	 * on the STM32F746 Discovery board, since that controller
	 * is on ABP2 instead of ABP1. *sigh*
	 *
	 * Note that we have to do this before we turn off the
	 * SPI2 clock below, since the controller won't respond to
	 * any register accesses if its clock is disabled.
	 */

	SPI2->CR1 &= ~(SPI_CR1_SPE | SPI_CR1_BR);
	if (speed == BADGE_CPU_SPEED_SLOW)
		SPI2->CR1 |= SPI_CR1_BR_1;
	else
		SPI2->CR1 |= SPI_CR1_BR_0;
	SPI2->CR1 |= SPI_CR1_SPE;
	__ISB();

	/*
	 * Disable some of the peripheral clocks so they don't glitch
	 * while we're changing frequencies.
	 */

	rccDisableSPI2 ();
	rccDisableDMA1 ();
	rccDisableDMA2 ();
	rccDisableSDMMC1 ();
	RCC->APB2ENR &= ~RCC_APB2ENR_LTDCEN;
	rccDisableDMA2D ();
	rccDisableAPB2 (RCC_APB2ENR_SAI2EN);

	/*
	 * First, temporarily set the system clock to the
	 * internal high speed oscillator (16MHz).
	 */

	RCC->CFGR &= ~RCC_CFGR_SW;
	__ISB();
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
		__ISB();

	/* If we're not targeting normal speed, turn off overdrive mode */

	if (speed != BADGE_CPU_SPEED_NORMAL) {
		/* Disable overdrive */
		PWR->CR1 &= ~(PWR_CR1_ODEN|PWR_CR1_ODSWEN);
		__ISB();
		while (PWR->CSR1 & PWR_CSR1_ODSWRDY)
			__ISB();
	}

	/* Now turn off the PLL */

	RCC->CR &= ~RCC_CR_PLLON;
	__ISB();
	while (RCC->CR & RCC_CR_PLLRDY)
		__ISB();

	/*
	 * Clear the ABP1 and APB2 pre-scaler values. We'll
	 * reset them below. We do this so that we can adjust the
	 * timer and peripheral clock frequencies so that they
	 * seem the same even after the CPU clock speed switch.
	 * This keeps the system clock timer from slowing down
	 * when we reduce CPU speed.
	 */

	RCC->CFGR &= ~(STM32_PPRE1_MASK | STM32_PPRE2_MASK);
	__ISB();

	/*
	 * Now set the system clock divisor. The three choices are:
	 *
	 * Divide by 2: 216MHz
	 * Divide by 4: 108MHz
	 * Divide by 8: 54MHz
	 *
	 * We also change the voltage scaling and overdive settings.
	 * For 216MHz, we want voltage scale 1 and overdrive enabled.
	 * For other speeds, we want scale 3 and overdrive disabled.
	 *
	 * Note that the slow speed will impact the SD card
	 * controller. It really will only work correctly at normal
	 * and medium speeds.
	 */

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;

	switch (speed) {
		case BADGE_CPU_SPEED_SLOW:
			RCC->PLLCFGR = STM32_PLLQ | STM32_PLLSRC |
			    STM32_PLLP_DIV8 | STM32_PLLN | STM32_PLLM;
			__ISB();
			RCC->CFGR |= STM32_PPRE1_DIV2 | STM32_PPRE2_DIV2;
			__ISB();
			/* Set voltage scale 3 */
			PWR->CR1 &= ~PWR_CR1_VOS_1;
			__ISB();
			break;
		case BADGE_CPU_SPEED_MEDIUM:
			RCC->PLLCFGR = STM32_PLLQ | STM32_PLLSRC |
			    STM32_PLLP_DIV4 | STM32_PLLN | STM32_PLLM;
			__ISB();
			RCC->CFGR |= STM32_PPRE1_DIV4 | STM32_PPRE2_DIV4;
			__ISB();
			/* Set voltage scale 3 */
			PWR->CR1 &= ~PWR_CR1_VOS_1;
			__ISB();
			break;
		case BADGE_CPU_SPEED_NORMAL:
		default:
			RCC->PLLCFGR = STM32_PLLQ | STM32_PLLSRC |
			    STM32_PLLP | STM32_PLLN | STM32_PLLM;
			__ISB();
			RCC->CFGR |= STM32_PPRE1 | STM32_PPRE2;
			__ISB();
			/* Restore voltage scale 1 */
			PWR->CR1 |= PWR_CR1_VOS_1;
			__ISB();
			break;
	}

	/*
	 * Now switch the PLL back on and wait for the
	 * voltage regulator to stabilize.
	 */

	RCC->CR |= RCC_CR_PLLON;
	__ISB();

	while ((PWR->CSR1 & PWR_CSR1_VOSRDY) == 0)
		__ISB();

	/* If we're targeting normal speed, turn on overdrive mode. */

	if (speed == BADGE_CPU_SPEED_NORMAL) {
		/* Re-enable overdrive */
		PWR->CR1 |= PWR_CR1_ODEN;
		__ISB();
		while ((PWR->CSR1 & PWR_CSR1_ODRDY) == 0)
			__ISB();
		PWR->CR1 |= PWR_CR1_ODSWEN;
		__ISB();
		while ((PWR->CSR1 & PWR_CSR1_ODSWRDY) == 0)
			__ISB();
	}

	/* Now wait for PLL to lock */

	while ((RCC->CR & RCC_CR_PLLRDY) == 0)
		__ISB();

	/* Now switch the system clock source back to the CPU */

	RCC->CFGR |= STM32_SW;
	__ISB();
  	while ((RCC->CFGR & RCC_CFGR_SWS) != (STM32_SW << 2))
		__ISB();

	rccEnableSPI2 (TRUE);
	rccEnableDMA1 (TRUE);
	rccEnableDMA2 (TRUE);
	rccEnableSDMMC1 (TRUE);
	RCC->APB2ENR |= RCC_APB2ENR_LTDCEN;
	rccEnableDMA2D (TRUE);
	rccEnableAPB2 (RCC_APB2ENR_SAI2EN, TRUE);

	badge_cpuspeed = speed;

	__enable_irq ();

	badge_cpu_dcache (TRUE);

	return;
}

static void
_putc (char c)
{
	USART1->TDR = (uint32_t)c;
	(void)USART1->TDR;
	while ((USART1->ISR & USART_ISR_TXE) == 0)
		;

	return;
}

static void
_puts (char * str)
{
	char * p;
	p = str;
	while (*p != '\0') {
		_putc (*p);
		p++;
	}
	_putc ('\r');
	_putc ('\n');
	return;
}

static void
dumpFrame (int type, uint32_t lr, EXC_FRAME *p)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (SCB->CFSR & (1 << i) && exc_msg[i] != NULL)
			_puts (exc_msg[i]);
	}

	if (SCB->HFSR & SCB_HFSR_VECTTBL_Msk)
		_puts ("Bus fault on vector table read during exception");

	if (type == HARD_FAULT && SCB->HFSR & SCB_HFSR_FORCED_Msk)
		_puts ("Forced fault due to configurable priority violation");

	if (SCB->CFSR & SCB_CFSR_MMARVALID_Msk) {
		sprintf (exc_msgbuf, "Memory fault address: 0x%08lX",
		    SCB->BFAR);
		_puts (exc_msgbuf);
	}

	if (SCB->CFSR & SCB_CFSR_BFARVALID_Msk) {
		sprintf (exc_msgbuf, "Bus fault address: 0x%08lX",
		    SCB->BFAR);
		_puts (exc_msgbuf);
	}

	sprintf (exc_msgbuf, "Fault while in %s mode", lr & 0x8 ?
	    "thread" : "handler");
	_puts (exc_msgbuf);

	sprintf (exc_msgbuf, "Floating point context %ssaved on stack",
	    lr & 0x10 ? "not " : "");
	_puts (exc_msgbuf);

	if (SCB->ICSR & SCB_ICSR_VECTPENDING_Msk) {
		sprintf (exc_msgbuf, "Interrupt is pending");
		_puts (exc_msgbuf);
	}

	if (SCB->ICSR & SCB_ICSR_VECTPENDING_Msk) {
		sprintf (exc_msgbuf, "Exception pending: %ld",
		    (SCB->ICSR & SCB_ICSR_VECTPENDING_Msk) >>
		    SCB_ICSR_VECTPENDING_Pos);
		_puts (exc_msgbuf);
	}

	if (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) {
		sprintf (exc_msgbuf, "Exception active: %ld",
		    (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) >>
		    SCB_ICSR_VECTACTIVE_Pos);
		_puts (exc_msgbuf);
	}

	sprintf (exc_msgbuf, "PC: 0x%08lX LR: 0x%08lX "
	    "SP: 0x%08lX SR: 0x%08lX",
	    p->exc_PC, p->exc_LR,
	    (uint32_t)p + (lr & 0x10 ? 32 : sizeof(EXC_FRAME)),
	    p->exc_xPSR);
	_puts (exc_msgbuf);
	sprintf (exc_msgbuf, "R0: 0x%08lX R1: 0x%08lX "
	    "R2: 0x%08lX R3: 0x%08lX R12: 0x%08lX",
	    p->exc_R[0], p->exc_R[1], p->exc_R[2], p->exc_R[3], p->exc_R12);
	_puts (exc_msgbuf);

	return;
}

void
trapHandle (int type, uint32_t exc_lr, EXC_FRAME * exc_sp)
{
	int i;

	/* Reset the serial port. */

	USART1->ISR = 0xFFFFFFFF;
	(void)USART1->ISR;

	for (i = 0; i < 1000; i++)
		__DSB();

	_puts ("");
	_puts ("");

	/*
	 * Disable the MPU before handling a fault.
	 * If the fault occurs during exception
	 * stacking, it means the CPU tried to push an
	 * exception frame onto a protected area of the
	 * stack. That protection will remain in effect
	 * if we try to decode the stack frame in the
	 * trap handler, and we'll trigger a hard fault.
	 * Decoding the stack frame in this case may not
	 * actually yield valid results, but it's wrong
	 * to trigger another fault too.
	 */

	mpuDisable ();

	switch (type) {
		case HARD_FAULT:
			_puts ("********** HARD FAULT **********");
			dumpFrame (type, exc_lr, exc_sp);
			break;
		case BUS_FAULT:
			_puts ("********** BUS FAULT **********");
			dumpFrame (type, exc_lr, exc_sp);
			break;
		case USAGE_FAULT:
			_puts ("********** USAGE FAULT **********");
			dumpFrame (type, exc_lr, exc_sp);
			break;
		case MEMMANAGE_FAULT:
			_puts ("********** MEMMANAGE FAULT **********");
			dumpFrame (type, exc_lr, exc_sp);
			break;
		default:
			_puts ("********** unknown fault **********");
			break;
	}

	/* Break into the debugger */

	__asm__ ("bkpt #0");

	while (1) {
	}

	/* NOTREACHED */
}
