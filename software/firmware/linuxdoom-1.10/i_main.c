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
//	Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_main.c,v 1.4 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>

#include "doomdef.h"

#include "m_argv.h"
#include "d_main.h"

#include "i_system.h"
#include "w_wad.h"
#include "doomstat.h"

jmp_buf exit_env;

int
doom_main
( int		argc,
  char**	argv ) 
{ 
    myargc = argc; 
    myargv = argv; 

    if (setjmp (exit_env) != 0)
        goto doom_exit;
 
    D_DoomMain (); 

doom_exit:

    if (doomcom != NULL) {
	free (doomcom);
	doomcom = NULL;
    }
    if (lumpinfo != NULL) {
	free (lumpinfo);
	lumpinfo = NULL;
    }
    if (lumpcache != NULL) {
	free (lumpcache);
	lumpcache = NULL;
    }

    return 0;
} 
