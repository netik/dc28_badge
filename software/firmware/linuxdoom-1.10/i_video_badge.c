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

#include <stdlib.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

static palette_color_t * palettebuf;
static int buttontmp;
static void * screenbuf;

#define        roundup(x, y)   ((((x)+((y)-1))/(y))*(y))

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
	free (screenbuf);
	screens[0] = NULL;
	palettebuf = NULL;
	screenbuf = NULL;

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
#ifdef notdef
    event_t event;

    // put event-grabbing stuff in here
    XNextEvent(X_display, &X_event);
    switch (X_event.type)
    {
      case KeyPress:
	event.type = ev_keydown;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "k");
	break;
      case KeyRelease:
	event.type = ev_keyup;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "ku");
	break;
      case ButtonPress:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0)
	    | (X_event.xbutton.button == Button1)
	    | (X_event.xbutton.button == Button2 ? 2 : 0)
	    | (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "b");
	break;
      case ButtonRelease:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0);
	// suggest parentheses around arithmetic in operand of |
	event.data1 =
	    event.data1
	    ^ (X_event.xbutton.button == Button1 ? 1 : 0)
	    ^ (X_event.xbutton.button == Button2 ? 2 : 0)
	    ^ (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "bu");
	break;
      case MotionNotify:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xmotion.state & Button1Mask)
	    | (X_event.xmotion.state & Button2Mask ? 2 : 0)
	    | (X_event.xmotion.state & Button3Mask ? 4 : 0);
	event.data2 = (X_event.xmotion.x - lastmousex) << 2;
	event.data3 = (lastmousey - X_event.xmotion.y) << 2;

	if (event.data2 || event.data3)
	{
	    lastmousex = X_event.xmotion.x;
	    lastmousey = X_event.xmotion.y;
	    if (X_event.xmotion.x != X_width/2 &&
		X_event.xmotion.y != X_height/2)
	    {
		D_PostEvent(&event);
		// fprintf(stderr, "m");
		mousemoved = false;
	    } else
	    {
		mousemoved = true;
	    }
	}
	break;
	
      case Expose:
      case ConfigureNotify:
	break;
	
      default:
	if (doShm && X_event.type == X_shmeventtype) shmFinished = true;
	break;
    }
#endif

}

//
// I_StartTic
//
void I_StartTic (void)
{
	event_t event;

	if (buttontmp == 5) {
		buttontmp = 0;
		event.type = ev_keydown;
		event.data1 = 'y';
		D_PostEvent(&event);
		event.type = ev_keyup;
		D_PostEvent(&event);
	}

	if (buttontmp == 0 && palReadLine (LINE_BUTTON_USER) == 1) {
		buttontmp = 1;
		event.type = ev_keydown;
		event.data1 = KEY_F10;
		D_PostEvent(&event);
		event.type = ev_keyup;
		D_PostEvent(&event);
	}

	if (buttontmp)
		buttontmp++;

#ifdef notdef
    if (!X_display)
	return;

    while (XPending(X_display))
	I_GetEvent();

    // Warp the pointer back to the middle of the window
    //  or it will wander off - that is, the game will
    //  loose input focus within X11.
    if (grabMouse)
    {
	if (!--doPointerWarp)
	{
	    XWarpPointer( X_display,
			  None,
			  X_mainWindow,
			  0, 0,
			  0, 0,
			  X_width/2, X_height/2);

	    doPointerWarp = POINTER_WARP_COUNTDOWN;
	}
    }

    mousemoved = false;
#endif
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
	/* Allocate pallete */

	palettebuf = malloc (sizeof(palette_color_t) * 256);

	idesDoubleBufferInit (IDES_DB_L8);

	buttontmp = 0;

	/* Allocate the screen buffer */

	screenbuf = malloc ((SCREENWIDTH * SCREENHEIGHT) + CACHE_LINE_SIZE);

	screens[0] = (unsigned char *)roundup((uintptr_t)screenbuf,
	    CACHE_LINE_SIZE);

	return;
}
