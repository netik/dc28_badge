/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */

#ifndef _GDISP_LLD_CONFIG_H
#define _GDISP_LLD_CONFIG_H

#if GFX_USE_GDISP

/*===========================================================================*/
/* Driver hardware support.                                                  */
/*===========================================================================*/

#define GDISP_HARDWARE_DRAWPIXEL		GFXON
#define GDISP_HARDWARE_PIXELREAD		GFXON
#define GDISP_HARDWARE_CONTROL			GFXON
#define GDISP_HARDWARE_FILLS			GFXON
#define GDISP_HARDWARE_BITFILLS			GFXON

/*
 * Set this to your frame buffer pixel format.
 * We support either RGB888 or RGB565. Default is RGB565.
 */

#define GDISP_LLD_PIXELFORMAT           GDISP_PIXELFORMAT_RGB565

// Any other support comes from the board file
#include "board_STM32FB.h"

#ifndef GDISP_LLD_PIXELFORMAT
	#error "GDISP FrameBuffer: You must specify a GDISP_LLD_PIXELFORMAT in your board_framebuffer.h or your makefile"
#endif

#ifdef notdef
// This driver currently only supports unpacked formats with more than 8 bits per pixel
//	that is, we only support GRAY_SCALE with 8 bits per pixel or any unpacked TRUE_COLOR format.
// Note: At the time this file is included we have not calculated all our color
//			definitions so we need to do this by hand.
#if (GDISP_LLD_PIXELFORMAT & 0x4000) && (GDISP_LLD_PIXELFORMAT & 0xFF) != 8
	#error "GDISP FrameBuffer: This driver does not support the specified GDISP_LLD_PIXELFORMAT"
#endif
#endif

#endif	/* GFX_USE_GDISP */

#endif	/* _GDISP_LLD_CONFIG_H */
