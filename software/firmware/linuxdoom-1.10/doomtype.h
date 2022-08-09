// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Simple basic typedefs, isolated here to make it easier
//	 separating modules.
//    
//-----------------------------------------------------------------------------


#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
// Fixed to use builtin bool type with C++.
#ifdef __cplusplus
typedef bool boolean;
#else
#ifdef notdef
typedef enum {false, true} boolean;
#endif

#ifdef sun

#include <sys/types.h>

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned long uint32_t;
typedef signed long int32_t;
typedef int socklen_t;
typedef unsigned int uintptr_t;

#define true 1
#define false 0

/*
 * Very old versions of GCC for SunOS don't support the
 * __attribute__ keyword, so make sure to make it disappear.
 */

#define __attribute__(x)

#else

#include <stdbool.h>
#include <stdint.h>

#endif

/*
 * Why do we define boolean to be an int instead of a bool? Well,
 * apparently Doom has a funny idea that boolean variables can
 * sometimes have three states: 0 (false), 1 (true) or -1 (list
 * termination). A boolean variable is really only supposed to
 * have two states, so this is a little nuts. But what do I know,
 * I'm only an engineer.
 *
 * We also include stdbool.h to get the definitions of true and false.
 */

typedef int boolean;

#endif
typedef unsigned char byte;
#endif


// Predefined with some OS.
#ifdef LINUX
#include <values.h>
#else
#define MAXCHAR		((char)0x7f)
#define MAXSHORT	((short)0x7fff)

// Max pos 32-bit int.
#define MAXINT		((int)0x7fffffff)	
#define MAXLONG		((long)0x7fffffff)
#define MINCHAR		((char)0x80)
#define MINSHORT	((short)0x8000)

// Max negative 32-bit integer.
#define MININT		((int)0x80000000)	
#define MINLONG		((long)0x80000000)
#endif




#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
