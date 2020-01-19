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

#ifndef _STM32ADC_LLDH_
#define _STM32ADC_LLDH_

/*
 * The STM32F746 has some internal factory calibration values
 * for the temperature sensor and reference voltage stored in ROM.
 * The values are:
 *
 * TS_CAL1: raw Vsense value at 30 degrees C
 * TS_CAL2: raw Vsense value at 110 degrees C
 * REFIN_CAL: raw Vrefin at 30 degrees C
 *
 * Each of these is a 16-bit value.
 *
 * We use these for calculating the actual temperature value
 * from the ADC output at runtime.
 */

#define STM32_REFIN_CAL_ADDR	0x1FF0F44A
#define STM32_REFIN_CAL_VOLT	3300
#define STM32_TS_CAL1_ADDR	0x1FF0F44C
#define STM32_TS_CAL1_TEMP	30
#define STM32_TS_CAL2_ADDR	0x1FF0F44E
#define STM32_TS_CAL2_TEMP	110

extern float stm32TempGet (void);
extern float stm32VbatGet (void);
extern float stm32VrefGet (void);

#endif /* _STM32ADC_LLDH_ */
