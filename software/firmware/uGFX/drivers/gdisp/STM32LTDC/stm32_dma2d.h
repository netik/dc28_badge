#ifndef _STM32_DMA2D_H
#define _STM32_DMA2D_H

#define OPFCCR_ARGB8888		0x00
#define OPFCCR_RGB888		0x01
#define OPFCCR_RGB565		0x02
#define OPFCCR_ARGB1555		0x03
#define OPFCCR_ARGB4444		0x04

#define FGPFCCR_CM_ARGB8888	0x00
#define FGPFCCR_CM_RGB888	0x01
#define FGPFCCR_CM_RGB565	0x02
#define FGPFCCR_CM_ARGB1555	0x03
#define FGPFCCR_CM_ARGB4444	0x04


#define DMA2D_CR_MODE_R2M	((gU32)0x00030000)	/* Register-to-memory mode */
#define DMA2D_CR_MODE_M2MBLEND	((gU32)0x00020000)	/* Register-to-memory with blending and pixel format conversion mode */
#define DMA2D_CR_MODE_M2MPFC	((gU32)0x00010000)	/* Register-to-memory with pixel format conversion mode */
#define DMA2D_CR_MODE_M2M	((gU32)0x00000000)	/* Register-to-memory mode */

#endif /* _STM32_DMA2D_H */
