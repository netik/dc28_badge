#define HAL_SDRAM_MODULE_ENABLED
#define HAL_LTCD_MODULE_ENABLED
#define USE_HAL_SDRAM_REGISTER_CALLBACKS 0
#define USE_RTOS 0
#include "ch.h"
#define HAL_Delay(x) chThdSleep (x)
#define assert_param(x) (void)(x)
#define __HAL_RCC_FMC_CLK_ENABLE() \
    do { \
        __IO uint32_t tmpreg; \
        SET_BIT(RCC->AHB3ENR, RCC_AHB3ENR_FMCEN);\
        /* Delay after an RCC peripheral clock enabling */ \
        tmpreg = READ_BIT(RCC->AHB3ENR, RCC_AHB3ENR_FMCEN);\
        UNUSED(tmpreg); \
    } while(0)
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_sdram.h"
