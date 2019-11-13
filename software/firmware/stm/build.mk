STMSRC+=					\
	$(STM)/stm32f7xx_ll_fmc.c		\
	$(STM)/stm32f7xx_hal_sdram.c		\
	$(STM)/stm32746g_discovery_sdram.c	\
	$(STM)/stm32f7xx_hal_ltdc_ex.c		\
	$(STM)/stm32f7xx_hal_ltdc.c

STMINC+= $(STM)

STMDEFS+=
USE_COPT+= $(STMDEFS)
