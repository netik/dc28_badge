#ifndef MCUCONF_COMMUNITY_H
#define MCUCONF_COMMUNITY_H

/*
 * LTDC driver system settings.
 */
#define STM32_LTDC_USE_LTDC                 TRUE
#define LTDC_USE_MUTUAL_EXCLUSION           FALSE
#define LTDC_USE_WAIT                       FALSE
#define STM32_LTDC_EV_IRQ_PRIORITY          11
#define STM32_LTDC_ER_IRQ_PRIORITY          11

static inline void
chSchDoYieldS (void)
{
	__WFI();
}

#endif
