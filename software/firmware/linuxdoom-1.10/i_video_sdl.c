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

#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>

#include <errno.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#define STRETCHEDHEIGHT 240

#define POINTER_WARP_COUNTDOWN	1

static int		X_width;
static int		X_height;
static int		X_depth;
static boolean		grabMouse;
static SDL_Window *	win;
static SDL_Surface *	surface;
static SDL_Surface *	screen;
static SDL_Surface *	screen_intermediate;
static SDL_Color	colors[256];

static uint8_t * frame;

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int	multiply=1;

int xlatekey(SDL_Keysym *key)
{

    int rc;

    switch(key->sym)
    {
      case SDLK_LEFT:   rc = KEY_LEFTARROW;     break;
      case SDLK_RIGHT:  rc = KEY_RIGHTARROW;    break;
      case SDLK_DOWN:   rc = KEY_DOWNARROW;     break;
      case SDLK_UP:     rc = KEY_UPARROW;       break;
      case SDLK_ESCAPE: rc = KEY_ESCAPE;        break;
      case SDLK_RETURN: rc = KEY_ENTER;         break;
      case SDLK_TAB:    rc = KEY_TAB;           break;
      case SDLK_F1:     rc = KEY_F1;            break;
      case SDLK_F2:     rc = KEY_F2;            break;
      case SDLK_F3:     rc = KEY_F3;            break;
      case SDLK_F4:     rc = KEY_F4;            break;
      case SDLK_F5:     rc = KEY_F5;            break;
      case SDLK_F6:     rc = KEY_F6;            break;
      case SDLK_F7:     rc = KEY_F7;            break;
      case SDLK_F8:     rc = KEY_F8;            break;
      case SDLK_F9:     rc = KEY_F9;            break;
      case SDLK_F10:    rc = KEY_F10;           break;
      case SDLK_F11:    rc = KEY_F11;           break;
      case SDLK_F12:    rc = KEY_F12;           break;
        
      case SDLK_BACKSPACE:
      case SDLK_DELETE: rc = KEY_BACKSPACE;     break;

      case SDLK_PAUSE:  rc = KEY_PAUSE;         break;

      case SDLK_EQUALS: rc = KEY_EQUALS;        break;

      case SDLK_KP_MINUS:
      case SDLK_MINUS:  rc = KEY_MINUS;         break;

      case SDLK_LSHIFT:
      case SDLK_RSHIFT:
        rc = KEY_RSHIFT;
        break;
        
      case SDLK_LCTRL:
      case SDLK_RCTRL:
        rc = KEY_RCTRL;
        break;
        
      case SDLK_LALT:
      case SDLK_LGUI:
      case SDLK_RALT:
      case SDLK_RGUI:
        rc = KEY_RALT;
        break;
        
      default:
        rc = key->sym;
        break;
    }

    return rc;

}


//
//  Translates the key currently in X_event
//

void I_ShutdownGraphics(void)
{

    if (frame == NULL)
	return;

    printf ("I_ShutdownGraphics: Destroying SDL context\n");

    SDL_FreeSurface (screen);
    if (X_depth != 8)
	SDL_FreeSurface (screen_intermediate);
    SDL_DestroyWindow (win);
    SDL_Quit ();

    free (frame);

    return;
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

void I_GetEvent (SDL_Event * evt)
{
    event_t event;
    Uint32 buttonstate;

    switch (evt->type) {
	case SDL_KEYDOWN:
	    event.type = ev_keydown;
	    event.data1 = xlatekey (&evt->key.keysym);
	    D_PostEvent (&event);
	    break;

	case SDL_KEYUP:
	    event.type = ev_keyup;
	    event.data1 = xlatekey (&evt->key.keysym);
	    D_PostEvent (&event);
	    break;

	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
	    buttonstate = SDL_GetMouseState (NULL, NULL);
	    event.type = ev_mouse;
	    event.data1 = (buttonstate & SDL_BUTTON(1) ? 1 : 0) |
			  (buttonstate & SDL_BUTTON(2) ? 2 : 0) |
			  (buttonstate & SDL_BUTTON(3) ? 4 : 0);
	    event.data2 = event.data3 = 0;
	    D_PostEvent (&event);
	    break;

	case SDL_MOUSEMOTION:
        /* Ignore mouse warp events */
        if ((evt->motion.x != surface->w/2)||(evt->motion.y != surface->h/2))
        {
            /* Warp the mouse back to the center */
            if (grabMouse) {
                SDL_WarpMouseInWindow(win, surface->w/2, surface->h/2);
            }
            event.type = ev_mouse;
            event.data1 = 0
                | (evt->motion.state & SDL_BUTTON(1) ? 1 : 0)
                | (evt->motion.state & SDL_BUTTON(2) ? 2 : 0)
                | (evt->motion.state & SDL_BUTTON(3) ? 4 : 0);
            event.data2 = evt->motion.xrel << 2;
            event.data3 = -evt->motion.yrel << 2;
            D_PostEvent (&event);
        }
        break;

	case SDL_QUIT:
	    I_Quit ();
	    break;
	default:
	    break;
    }

    return;
}

//
// I_StartTic
//
void I_StartTic (void)
{
    SDL_Event evt;

    while (SDL_PollEvent (&evt))
	I_GetEvent (&evt);

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

    // UNUSED static unsigned char *bigscreen=0;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    
    }

    if (SDL_BlitSurface (screen, NULL, screen_intermediate, NULL) < 0) {
	I_Error ("Failed to blit pixels to screen surface\n");
    }

    if (X_depth != 8 && SDL_BlitScaled (screen_intermediate, NULL, surface, NULL) < 0) {
	I_Error ("Failed to blit pixels to screen surface\n");
    }

    if (SDL_UpdateWindowSurface (win) < 0)
	I_Error ("SDL window update failed\n");

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
// Palette stuff.
//

void UploadNewPalette(SDL_Color * cmap, byte *palette)
{

    register int	i;
    register int	c;

    // set the X colormap entries

    for (i=0 ; i<256 ; i++)
    {
	c = gammatable[usegamma][*palette++];
	cmap[i].r = (c<<8) + c;
	c = gammatable[usegamma][*palette++];
	cmap[i].g = (c<<8) + c;
	c = gammatable[usegamma][*palette++];
	cmap[i].b = (c<<8) + c;
	cmap[i].a = 0xff;
    }

    if (SDL_SetPaletteColors (screen->format->palette, cmap, 0, 256) < 0)
	I_Error ("Failed to set SDL palette\n");

    return;
}

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
    UploadNewPalette (colors, palette);
}


void I_InitGraphics(void)
{

    int			n;
    int			pnum;
    int			x=0;
    int			y=0;
    
    // warning: char format, different type arg
    char		xsign=' ';
    char		ysign=' ';
    
    static int		firsttime=1;

    if (!firsttime)
	return;
    firsttime = 0;

    if (M_CheckParm("-2"))
	multiply = 2;

    if (M_CheckParm("-3"))
	multiply = 3;

    if (M_CheckParm("-4"))
	multiply = 4;

    X_width = SCREENWIDTH * multiply;
    X_height = STRETCHEDHEIGHT * multiply;

    // check if the user wants to grab the mouse (quite unnice)
    grabMouse = !!M_CheckParm("-grabmouse");

    // check for command-line geometry
    if ( (pnum=M_CheckParm("-geom")) ) // suggest parentheses around assignment
    {
	// warning: char format, different type arg 3,5
	n = sscanf(myargv[pnum+1], "%c%d%c%d", &xsign, &x, &ysign, &y);
	
	if (n==2)
	    x = y = 0;
	else if (n==6)
	{
	    if (xsign == '-')
		x = -x;
	    if (ysign == '-')
		y = -y;
	}
	else
	    I_Error("bad -geom parameter");
    }

    if (SDL_Init (SDL_INIT_VIDEO) < 0)
	I_Error ("Initializing SDL video failed\n");

    /* Create SDL window */

    win = SDL_CreateWindow ("SDL Doom 1.10", SDL_WINDOWPOS_UNDEFINED,
	SDL_WINDOWPOS_UNDEFINED, X_width, X_height, SDL_WINDOW_SHOWN);

    if (win == NULL)
	I_Error ("Creating SDL window failed\n");

    printf ("I_InitGraphics: Initialized SDL context\n");

    /* Get the surface handle */

    surface = SDL_GetWindowSurface (win);

    X_depth = surface->format->BitsPerPixel;

    /*
     * SDL does not allow scaled blitting directly from an 8-bit surface
     * to a 32-bit one, so we need to perform an intermediate surface
     * blit from the 8-bit initial screen buffer to a 32-bit one first,
     * then do scaled blit ot the window surface.
     */

    screen = SDL_CreateRGBSurface (SDL_SWSURFACE, SCREENWIDTH,
	SCREENHEIGHT, 8, 0, 0, 0, 0);

    if (X_depth != 8)
	screen_intermediate = SDL_CreateRGBSurface (SDL_SWSURFACE,
	    SCREENWIDTH, SCREENHEIGHT, X_depth, 0, 0, 0, 0);
    else
	screen_intermediate = surface;

    frame = malloc (X_width * X_height * sizeof(uint32_t));

    screens[0] = (unsigned char *) screen->pixels;

    return;
}