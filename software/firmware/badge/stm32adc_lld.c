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

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "stm32adc_lld.h"

static const ADCConversionGroup adcgrpcfg0 = {
	FALSE,						/* not circular */
	1,						/* channels */
	NULL,						/* callback */
	NULL,						/* err callback */
	0,						/* CR1   */
	ADC_CR2_SWSTART,				/* CR2   */
	ADC_SMPR1_SMP_SENSOR(ADC_SAMPLE_480),		/* SMPR1 */
	0,						/* SMPR2 */
	0,						/* HTR */
	0,						/* LTR */
	0,						/* SQR1  */
	0,						/* SQR2  */
	ADC_SQR3_SQ1_N(ADC_CHANNEL_SENSOR)		/* SQR3  */
};

static const ADCConversionGroup adcgrpcfg1 = {
	FALSE,						/* not circular */
	1,						/* channels */
	NULL,						/* callback */
	NULL,						/* err callback */
	0,						/* CR1   */
	ADC_CR2_SWSTART,				/* CR2   */
	ADC_SMPR1_SMP_VBAT(ADC_SAMPLE_480),		/* SMPR1 */
	0,						/* SMPR2 */
	0,						/* HTR */
	0,						/* LTR */
	0,						/* SQR1  */
	0,						/* SQR2  */
	ADC_SQR3_SQ1_N(ADC_CHANNEL_VBAT)		/* SQR3  */
};

static const ADCConversionGroup adcgrpcfg2 = {
	FALSE,						/* not circular */
	1,						/* channels */
	NULL,						/* callback */
	NULL,						/* err callback */
	0,						/* CR1   */
	ADC_CR2_SWSTART,				/* CR2   */
	ADC_SMPR1_SMP_VREF(ADC_SAMPLE_480),		/* SMPR1 */
	0,						/* SMPR2 */
	0,						/* HTR */
	0,						/* LTR */
	0,						/* SQR1  */
	0,						/* SQR2  */
	ADC_SQR3_SQ1_N(ADC_CHANNEL_VREFINT)		/* SQR3  */
};

float
stm32TempGet (void)
{
	adcsample_t samples[100];
	float avg;
	float slope;
	float temp;
	uint16_t cal1, cal2;
	int i;

	/* Take 100 temperature sensor samples */

	adcAcquireBus (&ADCD1);
	adcSTM32EnableTSVREFE ();
	adcConvert (&ADCD1, &adcgrpcfg0, samples, 100);
	adcSTM32DisableTSVREFE ();
	adcReleaseBus (&ADCD1);

	/* Average them together */

	avg = 0;
	for (i = 0; i < 100; i++)
		avg += samples[i];
	avg /= 100;

	/* Get calibration values */

	cal1 = *(uint16_t *)STM32_TS_CAL1_ADDR;
	cal2 = *(uint16_t *)STM32_TS_CAL2_ADDR;

	slope = (float)(cal2 - cal1) /
	    (STM32_TS_CAL2_TEMP - STM32_TS_CAL1_TEMP);

	/* Calculate result */

	temp = ((avg - (float)cal1) / slope) + STM32_TS_CAL1_TEMP;

	return (temp);
}

float
stm32VbatGet (void)
{
	adcsample_t samples[100];
	float avg;
	float vbat;
	int i;

	/* Take 100 Vbat samples */

	adcAcquireBus (&ADCD1);
	adcSTM32EnableVBATE ();
	adcConvert (&ADCD1, &adcgrpcfg1, samples, 100);
	adcSTM32DisableVBATE ();
	adcReleaseBus (&ADCD1);

	/* Average them together */

	avg = 0;
	for (i = 0; i < 100; i++)
		avg += samples[i];
	avg /= 100;

	/* Calculate voltage */

	vbat = avg * 4 * 3300 / 0xFFF;

	vbat /= 1000;

	return (vbat);
}

float
stm32VrefGet (void)
{
	adcsample_t samples[100];
	float avg;
	float vref;
	uint16_t cal1;
	int i;

	/* Take 100 Vrefint sensor samples */

	adcAcquireBus (&ADCD1);
	adcSTM32EnableTSVREFE ();
	adcConvert (&ADCD1, &adcgrpcfg2, samples, 100);
	adcSTM32DisableTSVREFE ();
	adcReleaseBus (&ADCD1);

	/* Average them together */

	avg = 0;
	for (i = 0; i < 100; i++)
		avg += samples[i];
	avg /= 100;

	/* Get calibration values */

	cal1 = *(uint16_t *)STM32_REFIN_CAL_ADDR;

	/* Convert to voltage */

	vref = avg * STM32_REFIN_CAL_VOLT / cal1;
	vref /= 1000;

	return (vref);
}

