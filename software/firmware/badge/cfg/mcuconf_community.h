

/*
 * FSMC driver system settings.
 */
#define STM32_FSMC_USE_FSMC1                TRUE
#define STM32_FSMC_FSMC1_IRQ_PRIORITY       10
#define STM32_FSMC_DMA_CHN                  0x03010201

/*
 * FSMC NAND driver system settings.
 */
#define STM32_NAND_USE_NAND1                FALSE
#define STM32_NAND_USE_NAND2                FALSE
#define STM32_NAND_USE_EXT_INT              FALSE
#define STM32_NAND_DMA_STREAM               STM32_DMA_STREAM_ID(2, 7)
#define STM32_NAND_DMA_PRIORITY             0
#define STM32_NAND_DMA_ERROR_HOOK(nandp)    osalSysHalt("DMA failure")

/*
 * FSMC SRAM driver system settings.
 */
#define STM32_SRAM_USE_SRAM1                TRUE
#define STM32_SRAM_USE_SRAM2                FALSE
#define STM32_SRAM_USE_SRAM3                FALSE
#define STM32_SRAM_USE_SRAM4                FALSE

/*
 * FSMC SDRAM driver system settings.
 */
#define STM32_SDRAM_USE_SDRAM1              TRUE
#define STM32_SDRAM_USE_SDRAM2              FALSE

/*
 * LTDC driver system settings.
 */
#define STM32_LTDC_USE_LTDC                 TRUE
#define STM32_LTDC_EV_IRQ_PRIORITY          11
#define STM32_LTDC_ER_IRQ_PRIORITY          11

/*
 * DMA2D driver system settings.
 */
#define STM32_DMA2D_USE_DMA2D               TRUE
#define STM32_DMA2D_IRQ_PRIORITY            11

/*
 * USBH driver system settings.
 */
#define STM32_OTG_FS_CHANNELS_NUMBER          12
#define STM32_OTG_HS_CHANNELS_NUMBER          16

#define STM32_USBH_USE_OTG1                 FALSE
#define STM32_OTG1_USE_ULPI                 FALSE
#define STM32_OTG1_RXFIFO_SIZE              1024
#define STM32_OTG1_PTXFIFO_SIZE             128
#define STM32_OTG1_NPTXFIFO_SIZE            128

#define STM32_USBH_USE_OTG2                 TRUE
#define STM32_OTG2_USE_HS                   TRUE
#define STM32_OTG2_USE_ULPI                 TRUE
#define STM32_OTG2_RXFIFO_SIZE              2048
#define STM32_OTG2_PTXFIFO_SIZE             1024
#define STM32_OTG2_NPTXFIFO_SIZE            1024

#define STM32_USBH_MIN_QSPACE               4
#define STM32_USBH_CHANNELS_NP              4
