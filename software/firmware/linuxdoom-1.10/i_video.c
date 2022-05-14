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
#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <X11/extensions/XShm.h>
// Had to dig up XShm.c for this one.
// It is in the libXext, but not in the XFree86 headers.
#ifdef LINUX
int XShmGetEventBase( Display* dpy ); // problems with g++?
#endif

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#define POINTER_WARP_COUNTDOWN	1

Display*	X_display=0;
Window		X_mainWindow;
Colormap	X_cmap;
Visual*		X_visual;
GC		X_gc;
XEvent		X_event;
int		X_screen;
XVisualInfo	X_visualinfo;
XImage*		image;
static XColor	colors[256];
int		X_width;
int		X_height;
int		X_depth;

// MIT SHared Memory extension.
boolean		doShm;

XShmSegmentInfo	X_shminfo;
int		X_shmeventtype;

// Fake mouse handling.
// This cannot work properly w/o DGA.
// Needs an invisible mouse cursor at least.
boolean		grabMouse;
int		doPointerWarp = POINTER_WARP_COUNTDOWN;

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int	multiply=1;


//
//  Translates the key currently in X_event
//

int xlatekey(void)
{

    int rc;

    switch(rc = XKeycodeToKeysym(X_display, X_event.xkey.keycode, 0))
    {
      case XK_Left:	rc = KEY_LEFTARROW;	break;
      case XK_Right:	rc = KEY_RIGHTARROW;	break;
      case XK_Down:	rc = KEY_DOWNARROW;	break;
      case XK_Up:	rc = KEY_UPARROW;	break;
      case XK_Escape:	rc = KEY_ESCAPE;	break;
      case XK_Return:	rc = KEY_ENTER;		break;
      case XK_Tab:	rc = KEY_TAB;		break;
      case XK_F1:	rc = KEY_F1;		break;
      case XK_F2:	rc = KEY_F2;		break;
      case XK_F3:	rc = KEY_F3;		break;
      case XK_F4:	rc = KEY_F4;		break;
      case XK_F5:	rc = KEY_F5;		break;
      case XK_F6:	rc = KEY_F6;		break;
      case XK_F7:	rc = KEY_F7;		break;
      case XK_F8:	rc = KEY_F8;		break;
      case XK_F9:	rc = KEY_F9;		break;
      case XK_F10:	rc = KEY_F10;		break;
      case XK_F11:	rc = KEY_F11;		break;
      case XK_F12:	rc = KEY_F12;		break;
	
      case XK_BackSpace:
      case XK_Delete:	rc = KEY_BACKSPACE;	break;

      case XK_Pause:	rc = KEY_PAUSE;		break;

      case XK_KP_Equal:
      case XK_equal:	rc = KEY_EQUALS;	break;

      case XK_KP_Subtract:
      case XK_minus:	rc = KEY_MINUS;		break;

      case XK_Shift_L:
      case XK_Shift_R:
	rc = KEY_RSHIFT;
	break;
	
      case XK_Control_L:
      case XK_Control_R:
	rc = KEY_RCTRL;
	break;
	
      case XK_Alt_L:
      case XK_Meta_L:
      case XK_Alt_R:
      case XK_Meta_R:
	rc = KEY_RALT;
	break;
	
      default:
	if (rc >= XK_space && rc <= XK_asciitilde)
	    rc = rc - XK_space + ' ';
	if (rc >= 'A' && rc <= 'Z')
	    rc = rc - 'A' + 'a';
	break;
    }

    return rc;

}

void I_ShutdownGraphics(void)
{
  if (X_display == NULL)
    return;

  if (doShm) {
    // Detach from X server
    if (!XShmDetach(X_display, &X_shminfo))
	    I_Error("XShmDetach() failed in I_ShutdownGraphics()");

    // Release shared memory.
    shmdt(X_shminfo.shmaddr);
    shmctl(X_shminfo.shmid, IPC_RMID, 0);
  }

  // Paranoia.
  if (image != NULL)
    image->data = NULL;
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

static int	lastmousex = 0;
static int	lastmousey = 0;
boolean		mousemoved = false;
boolean		shmFinished;

void I_GetEvent(void)
{

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

}

Cursor
createnullcursor
( Display*	display,
  Window	root )
{
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
				 &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
}

//
// I_StartTic
//
void I_StartTic (void)
{

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

}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

static void
I_LineFill (int srcy, int dsty, int mult)
{
	int linerep;
	int pixrep;
	int i;
	uint8_t * src;
	unsigned long pix;

        src = (uint8_t *)screens[0];
	src += (srcy * SCREENWIDTH);

	for (linerep = 0; linerep < mult; linerep++) {
		for (i = 0; i < SCREENWIDTH; i++) {
			if (X_visual->class == PseudoColor)
				pix = src[i];
			else if (X_visual->class == DirectColor)
				pix = src[i] << 16 | src[i] << 8 | src[i];
			else { /* TrueColor */
				pix = 0xFF000000;
				pix |= (colors[src[i]].red & 0xFF) << 16;
				pix |= (colors[src[i]].green & 0xFF) << 8;
				pix |= (colors[src[i]].blue & 0xFF);
			}
			for (pixrep = 0; pixrep < mult; pixrep++)
				XPutPixel (image, (i * mult) + pixrep,
				    dsty + linerep, pix);
		}
	}

	return;
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{

    static int	lasttic;
    int		tics;
    int		i;
    int		j;

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

    for (i = 0; i < SCREENHEIGHT; i++) {
	for (j = 0; j < multiply; j++)
		I_LineFill (i, (i * multiply) + j, multiply);
    }


    if (doShm)
    {

	if (!XShmPutImage(	X_display,
				X_mainWindow,
				X_gc,
				image,
				0, 0,
				0, 0,
				X_width, X_height,
				True ))
	    I_Error("XShmPutImage() failed\n");

	// wait for it to finish and processes all input events
	shmFinished = false;
	do
	{
	    I_GetEvent();
	} while (!shmFinished);

    }
    else
    {

	// draw the image
	XPutImage(	X_display,
			X_mainWindow,
			X_gc,
			image,
			0, 0,
			0, 0,
			X_width, X_height );

	// sync up with server
	XSync(X_display, False);

    }

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

void UploadNewPalette(Colormap cmap, byte *palette)
{

    register int	i;
    register int	c;
    static boolean	firstcall = true;

    // initialize the colormap
    if (firstcall)
    {
	firstcall = false;
	for (i=0 ; i<256 ; i++)
	{
	    if (X_visual->class == PseudoColor)
		colors[i].pixel = i;
	    else
		colors[i].pixel = i | i << 8 | i << 16;
	    colors[i].flags = DoRed|DoGreen|DoBlue;
	}
    }

    // set the X colormap entries
    for (i=0 ; i<256 ; i++)
    {
	c = gammatable[usegamma][*palette++];
	colors[i].red = (c<<8) + c;
	c = gammatable[usegamma][*palette++];
	colors[i].green = (c<<8) + c;
	c = gammatable[usegamma][*palette++];
	colors[i].blue = (c<<8) + c;
    }

    // store the colors to the current colormap
    if (X_visual->class != TrueColor)
	XStoreColors(X_display, cmap, colors, 256);
}

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
    UploadNewPalette(X_cmap, palette);
}


//
// This function is probably redundant,
//  if XShmDetach works properly.
// ddt never detached the XShm memory,
//  thus there might have been stale
//  handles accumulating.
//
void grabsharedmemory(int size)
{

  int			key = ('d'<<24) | ('o'<<16) | ('o'<<8) | 'm';
  struct shmid_ds	shminfo;
  int			minsize = 320*200;
  int			id;
  int			rc;
  // UNUSED int done=0;
  int			pollution=5;
  
  // try to use what was here before
  do
  {
    id = shmget((key_t) key, minsize, 0777); // just get the id
    if (id != -1)
    {
      rc=shmctl(id, IPC_STAT, &shminfo); // get stats on it
      if (!rc) 
      {
	if (shminfo.shm_nattch)
	{
	  fprintf(stderr, "User %d appears to be running "
		  "DOOM.  Is that wise?\n", shminfo.shm_cpid);
	  key++;
	}
	else
	{
	  if (getuid() == shminfo.shm_perm.cuid)
	  {
	    rc = shmctl(id, IPC_RMID, 0);
	    if (!rc)
	      fprintf(stderr,
		      "Was able to kill my old shared memory\n");
	    else
	      I_Error("Was NOT able to kill my old shared memory");
	    
	    id = shmget((key_t)key, size, IPC_CREAT|0777);
	    if (id==-1)
	      I_Error("Could not get shared memory");
	    
	    rc=shmctl(id, IPC_STAT, &shminfo);
	    
	    break;
	    
	  }
	  if (size >= shminfo.shm_segsz)
	  {
	    fprintf(stderr,
		    "will use %d's stale shared memory\n",
		    shminfo.shm_cpid);
	    break;
	  }
	  else
	  {
	    fprintf(stderr,
		    "warning: can't use stale "
		    "shared memory belonging to id %d, "
		    "key=0x%x\n",
		    shminfo.shm_cpid, key);
	    key++;
	  }
	}
      }
      else
      {
	I_Error("could not get stats on key=%d", key);
      }
    }
    else
    {
      id = shmget((key_t)key, size, IPC_CREAT|0777);
      if (id==-1)
      {
	extern int errno;
	fprintf(stderr, "errno=%d\n", errno);
	I_Error("Could not get any shared memory");
      }
      break;
    }
  } while (--pollution);
  
  if (!pollution)
  {
    I_Error("Sorry, system too polluted with stale "
	    "shared memory segments.\n");
    }	
  
  X_shminfo.shmid = id;
  
  // attach to the shared memory segment
  image->data = X_shminfo.shmaddr = shmat(id, 0, 0);
  
  fprintf(stderr, "shared memory id=%d, addr=%p\n", id,
	  image->data);
}

void I_InitGraphics(void)
{

    char*		displayname;
    char*		d;
    int			n;
    int			pnum;
    int			x=0;
    int			y=0;
    
    // warning: char format, different type arg
    char		xsign=' ';
    char		ysign=' ';
    
    int			oktodraw;
    unsigned long	attribmask;
    XSetWindowAttributes attribs;
    XGCValues		xgcvalues;
    int			valuemask;
    static int		firsttime=1;
    XTextProperty	winName;
    char *		windowTitle;

    if (!firsttime)
	return;
    firsttime = 0;

    signal(SIGINT, (void (*)(int)) I_Quit);

    if (M_CheckParm("-2"))
	multiply = 2;

    if (M_CheckParm("-3"))
	multiply = 3;

    if (M_CheckParm("-4"))
	multiply = 4;

    X_width = SCREENWIDTH * multiply;
    X_height = SCREENHEIGHT * multiply;

    // check for command-line display name
    if ( (pnum=M_CheckParm("-disp")) ) // suggest parentheses around assignment
	displayname = myargv[pnum+1];
    else
	displayname = 0;

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

    // open the display
    X_display = XOpenDisplay(displayname);
    if (!X_display)
    {
	if (displayname)
	    I_Error("Could not open display [%s]", displayname);
	else
	    I_Error("Could not open display (DISPLAY=[%s])", getenv("DISPLAY"));
    }

    // use the default visual 
    X_screen = DefaultScreen(X_display);

    /*
     * It used to be very common for people to have systems with
     * support for 8-bit pseudocolor visuals, which was convenient
     * since Doom renders graphics using 8-bit pseudocolor pixels
     * with a 24-bit color lookup table (palette), which could be
     * installed as a private color map.
     *
     * But these days, graphics cards are much fancier and you almost
     * never have 8-bit pseudocolor visual support anymore, which
     * means that while you can get Doom's original X11 display code
     * to compile on modern systems, you can't get it to run.
     *
     * Instead you have 24-bit depth with either TrueColor or
     * DirectColor visuals. Neither one is exactly idea. With
     * DirectColor an X11 application can still use a private color
     * map, but you have to populate 24 bits of pixel data instead
     * of just 8. This also results in terrible color map flashing.
     * With TrueColor you avoid the color map flashing, but you
     * also have to populate 24 bits of pixel data, _and_ you have
     * to perform the color map lookup in software.
     *
     * We prefer TrueColor here, because at least we avoid the
     * color map flashing so the user doesn't have a seizure.
     */

    X_depth = 8;

    if (!XMatchVisualInfo(X_display, X_screen, X_depth,
	PseudoColor, &X_visualinfo)) {
	printf ("256-color PseudoColor visual not found, trying TrueColor\n");
	X_depth = 24;
	if (!XMatchVisualInfo (X_display, X_screen,
	    X_depth, TrueColor, &X_visualinfo))
	    I_Error ("no compatible visual found\n");
    }

    X_visual = X_visualinfo.visual;

    // check for the MITSHM extension
    doShm = XShmQueryExtension(X_display);

    // even if it's available, make sure it's a local connection
    if (doShm)
    {
	if (!displayname) displayname = (char *) getenv("DISPLAY");
	if (displayname)
	{
	    d = displayname;
	    while (*d && (*d != ':')) d++;
	    if (*d) *d = 0;
	    if (!(!strcasecmp(displayname, "unix") || !*displayname)) doShm = false;
	}
    }

    if (doShm)
	fprintf(stderr, "Using MITSHM extension\n");

    // create the colormap
    if (X_visual->class != TrueColor)
	X_cmap = XCreateColormap(X_display, RootWindow(X_display,
						   X_screen), X_visual, AllocAll);
    else
	X_cmap = XCreateColormap(X_display, XDefaultRootWindow(X_display),
						   X_visual, AllocNone);

    // setup attributes for main window
    attribmask = CWEventMask | CWColormap | CWBorderPixel;
    attribs.event_mask =
	KeyPressMask
	| KeyReleaseMask
	// | PointerMotionMask | ButtonPressMask | ButtonReleaseMask
	| ExposureMask;

    attribs.colormap = X_cmap;
    attribs.border_pixel = 0;

    // create the main window
    X_mainWindow = XCreateWindow(	X_display,
					RootWindow(X_display, X_screen),
					x, y,
					X_width, X_height,
					0, // borderwidth
					X_depth, // depth
					InputOutput,
					X_visual,
					attribmask,
					&attribs );

    windowTitle = "X11 Doom 1.10";
    XStringListToTextProperty (&windowTitle, 1, &winName);
    XSetWMName (X_display, X_mainWindow, &winName);

    XDefineCursor(X_display, X_mainWindow,
		  createnullcursor( X_display, X_mainWindow ) );

    // create the GC
    valuemask = GCGraphicsExposures;
    xgcvalues.graphics_exposures = False;
    X_gc = XCreateGC(	X_display,
  			X_mainWindow,
  			valuemask,
  			&xgcvalues );

    // map the window
    XMapWindow(X_display, X_mainWindow);

    // wait until it is OK to draw
    oktodraw = 0;
    while (!oktodraw)
    {
	XNextEvent(X_display, &X_event);
	if (X_event.type == Expose
	    && !X_event.xexpose.count)
	{
	    oktodraw = 1;
	}
    }

    // grabs the pointer so it is restricted to this window
    if (grabMouse)
	XGrabPointer(X_display, X_mainWindow, True,
		     ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
		     GrabModeAsync, GrabModeAsync,
		     X_mainWindow, None, CurrentTime);

    if (doShm)
    {

	X_shmeventtype = XShmGetEventBase(X_display) + ShmCompletion;

	// create the image
	image = XShmCreateImage(	X_display,
					X_visual,
					X_depth,
					ZPixmap,
					0,
					&X_shminfo,
					X_width,
					X_height);

	grabsharedmemory(image->bytes_per_line * image->height *
	    sizeof(uint32_t));


	// UNUSED
	// create the shared memory segment
	// X_shminfo.shmid = shmget (IPC_PRIVATE,
	// image->bytes_per_line * image->height, IPC_CREAT | 0777);
	// if (X_shminfo.shmid < 0)
	// {
	// perror("");
	// I_Error("shmget() failed in InitGraphics()");
	// }
	// fprintf(stderr, "shared memory id=%d\n", X_shminfo.shmid);
	// attach to the shared memory segment
	// image->data = X_shminfo.shmaddr = shmat(X_shminfo.shmid, 0, 0);
	

	if (!image->data)
	{
	    perror("");
	    I_Error("shmat() failed in InitGraphics()");
	}

	// get the X server to attach to it
	if (!XShmAttach(X_display, &X_shminfo))
	    I_Error("XShmAttach() failed in InitGraphics()");

    }
    else
    {
	image = XCreateImage(	X_display,
    				X_visual,
    				X_depth,
    				ZPixmap,
    				0,
    				(char*)malloc((X_width * X_height) *
				    sizeof(unsigned long)),
    				X_width, X_height,
    				32, 0);

    }

    screens[0] = (unsigned char *) malloc (SCREENWIDTH * SCREENHEIGHT);
    return;
}
