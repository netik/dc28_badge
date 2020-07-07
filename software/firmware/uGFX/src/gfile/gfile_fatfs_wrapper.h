/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */

/**
 * @file    src/gfile/gfile_fatfs_wrapper.h
 * @brief   GFILE FATFS wrapper.
 *
 */

#ifndef _FATFS_WRAPPER
#define _FATFS_WRAPPER

// Include the fatfs configuration from the local directory not the original source folder
#include "ffconf.h"

// Prevent preprocessor redefinition warnings
#ifdef notdef
#include "../../3rdparty/fatfs-0.13/source/integer.h"
#endif
#if defined(_T) && !defined(_INC_TCHAR)
	#define _INC_TCHAR
#endif

#ifdef notdef
// Include the fatfs API
#include "../../3rdparty/fatfs-0.13/source/ff.h"

// Include the fatfs diskio API
#include "../../3rdparty/fatfs-0.13/source/diskio.h"
#else
#include "ff.h"
#include "ffconf.h"
#endif

#endif //_FATFS_WRAPPER
