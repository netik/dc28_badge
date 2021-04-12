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
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include "ch.h"
#include "hal.h"

#include "gfx.h"
#include "ides_gfx.h"
#include "hal_stm32_dma2d.h"
#include "hal_stm32_ltdc.h"

#include "capture.h"

#include <stdlib.h>
#include <malloc.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

__attribute__((section(".ram7")))
static palette_color_t * palettebuf;
__attribute__((section(".ram7")))
static int buttontmp;

//
//  Translates the key 
//

static int xlatekey(uint32_t sym)
{
 	int rc;

	switch (sym) {
		case CAP_LEFT:   rc = KEY_LEFTARROW;     break;
		case CAP_RIGHT:  rc = KEY_RIGHTARROW;    break;
		case CAP_DOWN:   rc = KEY_DOWNARROW;     break;
		case CAP_UP:     rc = KEY_UPARROW;       break;
		case CAP_ESCAPE: rc = KEY_ESCAPE;        break;
		case CAP_RETURN: rc = KEY_ENTER;         break;
		case CAP_TAB:    rc = KEY_TAB;           break;
		case CAP_F1:     rc = KEY_F1;            break;
		case CAP_F2:     rc = KEY_F2;            break;
		case CAP_F3:     rc = KEY_F3;            break;
		case CAP_F4:     rc = KEY_F4;            break;
		case CAP_F5:     rc = KEY_F5;            break;
		case CAP_F6:     rc = KEY_F6;            break;
		case CAP_F7:     rc = KEY_F7;            break;
		case CAP_F8:     rc = KEY_F8;            break;
		case CAP_F9:     rc = KEY_F9;            break;
		case CAP_F10:    rc = KEY_F10;           break;
		case CAP_F11:    rc = KEY_F11;           break;
		case CAP_F12:    rc = KEY_F12;           break;
        
		case CAP_BACKSPACE:
		case CAP_DELETE: rc = KEY_BACKSPACE;     break;

		case CAP_PAUSE:  rc = KEY_PAUSE;         break;

		case CAP_EQUALS: rc = KEY_EQUALS;        break;

		case CAP_KP_MINUS:
		case CAP_MINUS:  rc = KEY_MINUS;         break;

		case CAP_LSHIFT:
		case CAP_RSHIFT:
			rc = KEY_RSHIFT;
			break;
        
		case CAP_LCTRL:
		case CAP_RCTRL:
			rc = KEY_RCTRL;
			break;
        
		case CAP_LALT:
		case CAP_LMETA:
		case CAP_RALT:
		case CAP_RMETA:
			rc = KEY_RALT;
			break;
        
		default:
			if (sym & 0x40000000)
				rc = 0;
			else
				rc = sym;
			break;
	}

	return (rc);
}

//
// I_ShutdownGraphics
//
void I_ShutdownGraphics (void)
{
	/*
	 * Grrr. Sometimes this function may be called even though
	 * we haven't set up the graphics yet.
	 */

	if (palettebuf == NULL)
		return;

	idesDoubleBufferStop ();

	free (palettebuf);
	free (screens[0]);
	screens[0] = NULL;
	palettebuf = NULL;

	return;
}


//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

void I_GetEvent(void)
{
	uint32_t	keysym;
	event_t		event;

	if (capture_queue_get (&keysym)) {
		if (CAPTURE_DIR(keysym) == CAPTURE_KEY_UP)
			event.type = ev_keyup;
		if (CAPTURE_DIR(keysym) == CAPTURE_KEY_DOWN)
			event.type = ev_keydown;
		keysym = CAPTURE_CODE(keysym);
		event.data1 = xlatekey (keysym);
		D_PostEvent (&event);
	}

	return; 
}

//
// I_StartTic
//
void I_StartTic (void)
{
	event_t event;
	uint32_t sts;

	I_GetEvent ();

	/*chThdSleep (50);*/
	sts = palReadLine (LINE_BUTTON_USER);

	if (buttontmp == 2 && sts == 1) {
		buttontmp = 0;
		event.type = ev_keydown;
		event.data1 = 'y';
		D_PostEvent(&event);
		event.type = ev_keyup;
		D_PostEvent(&event);
	}

	if (buttontmp == 0 && sts == 1) {
		buttontmp = 1;
		event.type = ev_keydown;
		event.data1 = KEY_F10;
		D_PostEvent(&event);
		event.type = ev_keyup;
		D_PostEvent(&event);
	}

	if (buttontmp == 1 && sts == 0)
		buttontmp = 2;

	return;
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
__attribute__((section(".ram7")))
	static int	lasttic;
	int		tics;
	int		i;

	if (palettebuf == NULL)
		return;

	/* draws little dots on the bottom of the screen */

	if (devparm) {
		i = I_GetTime();
		tics = i - lasttic;
		lasttic = i;
		if (tics > 20) tics = 20;

		for (i=0 ; i<tics*2 ; i+=2)
	    		screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
		for ( ; i<20*2 ; i+=2)
	    		screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    	}

	idesDoubleBufferBlit (0, 20, SCREENWIDTH, SCREENHEIGHT, screens[0]);

	return;
}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
	int i;
	int c;

	if (palettebuf == NULL) {
		/* Go home Doom, you're drunk. */
		return;
	}

	for (i = 0; i < 256; i++) {
		palettebuf[i].a = 0;
		c = gammatable[usegamma][*palette++];
		palettebuf[i].r = c;
		c = gammatable[usegamma][*palette++];
		palettebuf[i].g = c;
		c = gammatable[usegamma][*palette++];
		palettebuf[i].b = c;
	}

	idesDoubleBufferPaletteLoad (palettebuf);

	return;
}


void I_InitGraphics(void)
{
	uint32_t sym;

	/* Allocate pallete */

	palettebuf = malloc (sizeof(palette_color_t) * 256);

	idesDoubleBufferInit (IDES_DB_L8);

	buttontmp = 0;

	/* Allocate the screen buffer */

	screens[0] = memalign (CACHE_LINE_SIZE, (SCREENWIDTH * SCREENHEIGHT));

	memset (screens[0], 0, (SCREENWIDTH * SCREENHEIGHT));

	/* Drain the input queue */

	while (capture_queue_get (&sym)) {
	}

	return;
}
