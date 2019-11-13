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

#include <stdio.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "badge.h"

static void
cmd_cpu (BaseSequentialStream *chp, int argc, char *argv[])
{
	uint32_t freq;
	uint32_t pll_pll_divisor;
	uint32_t pll_multiplier;
	uint32_t pll_sysclk_divisor;

	(void)argv;
	(void)chp;

	if (argc > 0) {
		printf ("Usage: cpu\n");
		return;
	}

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

orchard_command("cpu", cmd_cpu);
