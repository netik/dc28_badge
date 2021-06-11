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
//	Handles WAD file header, directory, lump I/O.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: w_wad.c,v 1.5 1997/02/03 16:47:57 b1 Exp $";


#ifdef NORMALUNIX
#define _GNU_SOURCE /* for strcasestr */
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef BADGEDOOM
#include <alloca.h>
#endif
#ifndef O_BINARY
#define O_BINARY		0
#endif
#endif

#include "doomtype.h"
#include "m_swap.h"
#include "i_system.h"
#include "z_zone.h"
#include "info.h"
#include "sounds.h"

#ifdef __GNUG__
#pragma implementation "w_wad.h"
#endif
#include "w_wad.h"






//
// GLOBALS
//

// Location of each lump on disk.
__attribute__((section(".ram7")))
lumpinfo_t*		lumpinfo;		
__attribute__((section(".ram7")))
int			numlumps;

__attribute__((section(".ram7")))
void**			lumpcache;


#define strcmpi	strcasecmp

#ifndef BADGEDOOM
void strupr (char* s)
{
    while (*s) { *s = toupper(*s); s++; }
}
#endif

int filelength (int handle) 
{ 
    struct stat	fileinfo;
    
    if (fstat (handle,&fileinfo) == -1)
	I_Error ("Error fstating");

    return fileinfo.st_size;
}


void
ExtractFileBase
( char*		path,
  char*		dest )
{
    char*	src;
    int		length;

    src = path + strlen(path) - 1;
    
    // back up until a \ or the start
    while (src != path
	   && *(src-1) != '\\'
	   && *(src-1) != '/')
    {
	src--;
    }
    
    // copy up to eight characters
    memset (dest,0,8);
    length = 0;
    
    while (*src && *src != '.')
    {
	if (++length == 9)
	    I_Error ("Filename base of %s >8 chars",path);

	*dest++ = toupper((int)*src++);
    }
}





//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//
// If filename starts with a tilde, the file is handled
//  specially to allow map reloads.
// But: the reload feature is a fragile hack...

__attribute__((section(".ram7")))
int			reloadlump;
__attribute__((section(".ram7")))
char*			reloadname;

/*
 * This is a helper function to search only a specified
 * region of the lumpinfo array for a lump with a given
 * name. This is used to find lumps in an IWAD that might
 * need to be overriden by lumps in a PWAD.
 */

__attribute__((section(".ram7")))
static int replacesfx;

static lumpinfo_t *
W_FindLump (char * name, int start, int cnt)
{
    int i;
    lumpinfo_t * lump_p;

    for (i = start; i < cnt; i++) {
        lump_p = &lumpinfo[i];
        if (strncmp (lump_p->name, name, 8) == 0)
            return (lump_p);
    }

    return (NULL);
}

#define MON_see		0
#define MON_active	1
#define MON_attack	2
#define MON_pain	3
#define MON_death	4

typedef struct _montype {
	char *		name;
	int		objtype;
	int		oldsound[5];
} montype_t;

static montype_t montype[] = {
    { "grunt", MT_POSSESSED, { 0, 0, 0, 0, 0 } },
    { "shotguy", MT_SHOTGUY, { 0, 0, 0, 0, 0 } },
    { "vile", MT_VILE, { 0, 0, 0, 0, 0 } },
    { "skeleton", MT_UNDEAD, { 0, 0, 0, 0, 0 } },
    { "fatso", MT_FATSO, { 0, 0, 0, 0, 0 } },
    { "chainguy", MT_CHAINGUY, { 0, 0, 0, 0, 0 } },
    { "imp", MT_TROOP, { 0, 0, 0, 0, 0 } },
    { "demon", MT_SERGEANT, { 0, 0, 0, 0, 0 } },
    { "spectre", MT_SHADOWS, { 0, 0, 0, 0, 0 } },
    { "caco", MT_HEAD, { 0, 0, 0, 0, 0 } },
    { "baron", MT_BRUISER, { 0, 0, 0, 0, 0 } },
    { "knight", MT_KNIGHT, { 0, 0, 0, 0, 0 } },
    { "skull", MT_SKULL, { 0, 0, 0, 0, 0 } },
    { "spider", MT_SPIDER, { 0, 0, 0, 0, 0 } },
    { "baby", MT_BABY, { 0, 0, 0, 0, 0 } },
    { "cyber", MT_CYBORG, { 0, 0, 0, 0, 0 } },
    { "pain", MT_PAIN, { 0, 0, 0, 0, 0 } },
    { "wolfss", MT_WOLFSS, { 0, 0, 0, 0, 0 } },
    { "keen", MT_KEEN, { 0, 0, 0, 0, 0 } },
    { "brain", MT_BOSSBRAIN, { 0, 0, 0, 0, 0 } },
    { NULL, 0, { 0, 0, 0, 0, 0 } }
};

static char * soundtype[] = {
    "sight", "active", "attack", "pain", "death", NULL
};

/*
 * Check to see if we want to replace the sound lumps.
 * Some rules:
 *
 * - We don't support tying multiple sound lumps to the
 *   same effect and selecting one randomly like gzdoom
 *   does. So if there are multiple replacements (e.g.
 *   sight1, sight2, sight3), we always take the first
 *   one and then ignore the others. That is, if a sound
 *   has already been replaced once, don't do it again.
 *
 * - If there's a replacement rule for a sound effect
 *   but the replacement lumpname is the same as the default,
 *   then do nothing.
 *
 * - Don't re-assign the bgsit1 effect. These always apply
 *   only to the imp monster (not shared with other monsters)
 *   so there's no need to work around any conflict.
 *
 * Also, for each replacement we do, we save the old sound
 * effect so that we can un-do the changes to the mobjinfo
 * structures on exit.
 */

#define M_SFXREPLACE(sname, oname)				\
    if (strcmp (sound, sname) == 0) {				\
	sfxid = mobj->oname ## sound;				\
	if (/*sfxid == 0 ||*/ sfxid >= NUMSFX_OLD ||		\
	    strcasecmp (S_sfx[sfxid].name, lumpname) == 0 ||	\
	    strcmp (S_sfx[sfxid].name, "bgsit1") == 0)		\
	    return;						\
	m->oldsound[MON_ ## oname] = sfxid;			\
	mobj->oname ## sound = replacesfx;			\
    }

static void
W_DoSoundSub (char * sound, montype_t * m, char * p)
{
    mobjinfo_t * mobj;
    char * lumpname;
    int sfxid = -1;
    size_t i;

    /* Ran out of replacement slots */

    if (replacesfx == NUMSFX)
	return;

    /* Skip leading space */

    lumpname = strchr (p, ' ');

    if (lumpname == NULL)
	return;

    while (*lumpname == ' ')
	lumpname++;

    /* Some lump names have the DS prefix, some don't. */

    if (strncasecmp (lumpname, "ds", 2) == 0)
	lumpname += 2;

    for (i = 0; i < strlen(lumpname); i++) {
	if (isspace((int)lumpname[i]) || lumpname[i] == '\\')
	    break;
    }

    /* NUL terminate */

    lumpname[i] = '\0';

    mobj = &mobjinfo[m->objtype];

    M_SFXREPLACE ("sight", see);
    M_SFXREPLACE ("active", active);
    M_SFXREPLACE ("attack", attack);
    M_SFXREPLACE ("pain", pain);
    M_SFXREPLACE ("death", death);

    /* If no replacement rule matched, just return. */

    if (sfxid == -1)
	return;

    memcpy (&S_sfx[replacesfx], &S_sfx[sfxid], sizeof (sfxinfo_t));

#ifdef debug
    printf ("REPLACE monster %s sound %s sfxid %d %s with %s\n",
	m->name, sound, sfxid, S_sfx[sfxid].name, lumpname);
#endif

    S_sfx[replacesfx].name = lumpname;

    replacesfx++;

    return;
}

static void
W_CheckSound (char * line, montype_t * m)
{
    char * s;

    int i;

    i = 0;
    s = soundtype[i];

    while (s != NULL) {
	if (strcasestr (line, s) != NULL) {
	    W_DoSoundSub (s, m, line);
	    break;
	}
	i++;
	s = soundtype[i];
    }

    return;
}

static void
W_MonsterCheck (char * line)
{
    montype_t * m;
    char * p;

    m = &montype[0];

    /* A valid substitution line has a slash in it */

    p = strchr (line, '/');
    if (p == NULL)
	return;
    *p = '\0';
    p++;

    while (m->name != NULL) {
	if (strcasestr (line, m->name) != NULL) {
	    W_CheckSound (p, m);
	    break;
	}
	m++;
    }

    return;
}

static void
W_ParseSndinfo (char * snd, int len)
{
    char * p;
    char * t;
    char * eol;
    montype_t * m;

    replacesfx = NUMSFX_OLD;

    m = &montype[0];
    while (m->name != NULL) {
	m->oldsound[MON_see] = -1;
	m->oldsound[MON_active] = -1;
	m->oldsound[MON_attack] = -1;
	m->oldsound[MON_pain] = -1;
	m->oldsound[MON_death] = -1;
	m++;
    }

    p = snd;

    while (p < (snd + len)) {
	eol = strchr (p, '\n');
	*eol = '\0';
	t = p;
	/* Get rid of possible tabs */
	while (*t != '\0') {
	    if (*t == '\t')
		*t = ' ';
	    t++;
	}
	W_MonsterCheck (p);
	p = eol + 1;
    }

    p = snd;
    return;
}

void
W_RestoreSounds (void)
{
    montype_t * m;
    mobjinfo_t * mobj;

    if (replacesfx <= NUMSFX_OLD)
	return;

    m = &montype[0];

    while (m->name != NULL) {
	mobj = &mobjinfo[m->objtype];
	if (m->oldsound[MON_see] != -1)
	    mobj->seesound = m->oldsound[MON_see];
	if (m->oldsound[MON_active] != -1)
	    mobj->activesound = m->oldsound[MON_active];
	if (m->oldsound[MON_attack] != -1)
	    mobj->attacksound = m->oldsound[MON_attack];
	if (m->oldsound[MON_pain] != -1)
	    mobj->painsound = m->oldsound[MON_pain];
	if (m->oldsound[MON_death] != -1)
	    mobj->deathsound = m->oldsound[MON_death];
	m++;
    }

    return;
}

void W_AddFile (char *filename)
{
    wadinfo_t		header;
    lumpinfo_t*		lump_p;
    lumpinfo_t*		lump_o;
    boolean		do_replace;
    int			i;
    int			prevnumlumps;
    int			handle;
    int			length;
    int			startlump;
    filelump_t*		fileinfo;
    filelump_t		singleinfo;
    int			storehandle;
    
    // open the file and add to directory

    // handle reload indicator.
    if (filename[0] == '~')
    {
	filename++;
	reloadname = filename;
	reloadlump = numlumps;
    }
		
    if ( (handle = open (filename,O_RDONLY | O_BINARY)) == -1)
    {
	printf (" couldn't open %s\n",filename);
	return;
    }

    printf (" adding %s\n",filename);
    startlump = numlumps;
    prevnumlumps = numlumps;
	
    if (strcmpi (filename+strlen(filename)-3 , "wad" ) )
    {
	// single lump file
	fileinfo = &singleinfo;
	singleinfo.filepos = 0;
	singleinfo.size = LONG(filelength(handle));
	ExtractFileBase (filename, singleinfo.name);
	numlumps++;
    }
    else 
    {
	// WAD file
	read (handle, &header, sizeof(header));
	if (strncmp(header.identification,"IWAD",4))
	{
	    // Homebrew levels?
	    if (strncmp(header.identification,"PWAD",4))
	    {
		I_Error ("Wad file %s doesn't have IWAD "
			 "or PWAD id\n", filename);
	    }
	    
	    // ???modifiedgame = true;		
	}
	header.numlumps = LONG(header.numlumps);
	header.infotableofs = LONG(header.infotableofs);
	length = header.numlumps*sizeof(filelump_t);
	fileinfo = alloca (length);
	lseek (handle, header.infotableofs, SEEK_SET);
	read (handle, fileinfo, length);
	numlumps += header.numlumps;
    }

    
    // Fill in lumpinfo
    lumpinfo = realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));

    if (!lumpinfo)
	I_Error ("Couldn't realloc lumpinfo");

    lump_p = &lumpinfo[startlump];
	
    storehandle = reloadname ? -1 : handle;
    do_replace = false;
	
    for (i=startlump ; i<numlumps ; i++,lump_p++, fileinfo++)
    {
	lump_p->handle = storehandle;
	lump_p->position = LONG(fileinfo->filepos);
	lump_p->size = LONG(fileinfo->size);
	strncpy (lump_p->name, fileinfo->name, 8);

	/*
	 * The first WAD (startlump == 0) is always an IWAD. We
	 * don't want to look for substitutions unless this is
	 * a PWAD
	 */

	if (startlump == 0)
	    continue;

	/*
	 * The original vanilla Doom code doesn't handle having
	 * PWADs partially override sprite lumps. The assumption seems
	 * to be that all sprite lumps in a WAD file will appear in
	 * a region bounded by an S_START lump and an S_END lump,
	 * which are both empty lumps used as markers. Anything
	 * between those markers is a sprite, and Doom only makes
	 * one pass over the lump data looking for the markers.
	 *
	 * So the only way for a PWAD to override sprites is to
	 * have it override _all_ sprites, which means you need to
	 * provide replacements for every supported sprite in the
	 * game, and some of the replacements would just be copies
	 * of the original sprites which you want to leave alone.
	 *
	 * The Simpsons PWAD includes its own sprite replacements
	 * bounded by SS_START and SS_END markers. Since these
	 * names don't exactly match S_START and S_END (the strings
	 * that Doom searches for), they will normally be ignored.
	 * What we do here is check for such replacement markers,
	 * and if we find any sprite lumps between them with names
	 * which match any of the sprite lumps in the IWAD, we
	 * will override them with the data for the replacements.
	 */

        if (strstr (lump_p->name, "S_START") != NULL)
            do_replace = true;

	if (strstr (lump_p->name, "S_END") != NULL)
            do_replace = false;

	/*
	 * The Simpsons PWAD also includes some replacemnt
	 * animations, which use some new flats. Flats also have
	 * markers like sprites, called F_START and F_END. The
	 * code that handles flats calculates the number of flats
	 * based on the number of lumps between the two markers.
	 *
	 * The Simpsons PWAD contains its own flats between its
	 * own FF_START and FF_END markers. It only wants to
	 * override some of the original flats, and the lookup
	 * code will actually find the correct lumps that define
	 * them. However the Doom code allocates storage to hold
	 * pointers to the flats based on the lump index numbers
	 * between the F_START and F_END markers. Since the
	 * Simpsons IWAD has additional flats with higher index
	 * numbers that are outside this range, the memory
	 * allocated for the flats table ends up being too small,
	 * and this results in Doom corrupting memory of buffers
	 * that are adjacent to the table.
	 *
	 * As a hack, if we find a new *F_END marker in the PWAD,
	 * we turn it into the new F_END marker so that the
	 * allocation code ends up acquiring the right amount
	 * of memory.
	 */

	if (strstr (lump_p->name, "F_END") != NULL) {
            lump_o = W_FindLump ("F_END", 0, prevnumlumps);
            memset (lump_o->name, 0, sizeof(lump_p->name));
            strcpy (lump_p->name, "F_END");
	}

	/*
	 * If not within the PWAD's sprite region, don't try to
	 * do any replacements.
	 */

        if (do_replace == false)
            continue;

	/*
	 * For every lump in a PWAD between *S_START and *S_END
	 * markers, check for an original lump in the IWAD with
	 * the same name, and make it point to the replacement
	 * if found.
	 */

        lump_o = W_FindLump (lump_p->name, 0, prevnumlumps);

        if (lump_o != NULL) {
            lump_o->handle = storehandle;
            lump_o->position = LONG(fileinfo->filepos);
            lump_o->size = LONG(fileinfo->size);
            lump_p->handle = 0;
            lump_p->position = 0;
            lump_p->size = 0;
            memset (lump_p->name, 0, sizeof(lump_p->name));
        }
    }

    /*
     * The original Simpsons PWAD tries to override many of
     * Doom's sound effects, including those for some of the
     * monsters. For example the posessed soldier becomes
     * Moe the bartender, so the posessed soldier sound
     * effects are replaced with Moe quotes. However the
     * substitution is buggy. Moe/posessed-guy ends up saying
     * some of Grandpa/chaingun-guy's lines, and so on.
     *
     * Apparently gzdoom provides support for a custom lump
     * type called SNDINFO that allows you to specify the
     * specific sounds for a monster using a script. We try
     * to emulate support for SNDINFO lumps here, but there
     * are some limitations.
     *
     * 1) Vanilla Doom supports only 5 sound types: see, active,
     *    attack, pain and death. gzdoom supports a few additional
     *    sound types (e.g. melee). We can't really parse those
     *    entries.
     *
     * 2) Vanilla Doom tries to apply some randomizations for
     *    some sounds, so that monsters have some variety in their
     *    behavior. The gzdoom SNDINFO script allows you to specify
     *    several options for a given sound. For example, instead of
     *    just a 'death' sound, you could have 'death1,' 'death2' and
     *    'death3,' and gzdoom is smart enough to put these all in an
     *    array and randomly select one of them when the corresponding
     *    monster dies.
     *
     *    Unfortunately vanilla Doom isn't really set up to support
     *    that level of runtime configuration, so it's not possible to
     *    really handle that here.
     *
     * Note: Sound replacements have a detrimental effect on demos.
     * In the A_Look() function in p_enemy.c, the game tries to
     * randomize the selection of sounds for the posit1/2/3 and
     * bgsit1/2 effects. If a monster has posit1 or bgsit1 as its
     * seesound, the game tries to randomly choose an effect from
     * among posit1/2/3 and bgsit1/2. The whole reason we need
     * the SNDINFO workaround here is that the posit1/2/3 sounds
     * are shared among several monsters (posessed guy and chainguy,
     * for one) and this means that with the Simpsons PWAD, one
     * monster may end up saying another monster's lines. Notably,
     * Moe says Grandpa's lines. ("We have to kill the boooooooy!"
     * instead of "What're you lookin' at!?")
     *
     * Note that by chance, this doesn't happen with bgsit1/bgsit2
     * as those effects are only used with the imp monster. But the
     * possessed soldier guy, chaingun guy and shotgun guy all use
     * posit2. (I'm not sure who uses posit3.) Also, posessed guy
     * uses the same pistol shot sound as the player for attacksound.
     *
     * The simplest way to address this is to parse the SNDINFO lump
     * in the Simpsons PWAD to override the sound assignments, which
     * we can do by patching the sound IDs in the mobjinfo structures
     * for the corresponding monsters. This works well for all except
     * the seesound for posessed soldier guy, because of the way Doom
     * tries to randomize it.
     *
     * The problem is that Doom's M_Random() and P_Random() functions
     * are, in reality, not random. They select numbers from an array
     * using an index number that increases monotonically each time
     * they are called. The exact sequence of calls varies depending
     * on the player (what moves they make, and when they make them),
     * so the sequence of numbers will always be different each time
     * a human plays the game: it would be nearly impossible for a
     * human player to make *exactly* the same inputs with *exactly*
     * the same timing twice in a row.
     *
     * But demos are basically just game recordings, and recordings
     * *do* repeat the exact same steps exactly the same way every time
     * you play them back. (That's sort of the point.) This means each
     * time a demo plays, the same sequence of M_Random() and P_Random()
     * calls will occur, which results in the demo playback being
     * identical each time.
     *
     * But if we change the posessed guy's seesound to something
     * other than posit1, this will prevent P_Random() from being
     * called in A_Look() where it otherwise would be, so now the
     * sequence of random numbers will be different.
     *
     * This is not a problem for a human playing a live game, but it
     * causes the demos to play incorrectly (the player bangs into
     * walls, shoots the wrong things, and dies too soon before the
     * demo playback finishes). The same problem would happen if a
     * player made their recording with Doom without the Simpsons
     * PWAD loaded and then tried to play it back with it loaded (or
     * vice-versa).
     *
     * Whether or not this is considered a serious problem is a
     * matter of opinion.
     */

    i = W_CheckNumForName ("SNDINFO");
    if (i != -1) {
	char * p;
	p = Z_Malloc (W_LumpLength (i), PU_STATIC, NULL);
	W_ReadLump (i, p);
	W_ParseSndinfo (p, W_LumpLength (i));
	/* Don't free the lump: we point to the strings in it. */
	/*Z_Free (p);*/
    }
	
    if (reloadname)
	close (handle);
}




//
// W_Reload
// Flushes any of the reloadable lumps in memory
//  and reloads the directory.
//
void W_Reload (void)
{
    wadinfo_t		header;
    int			lumpcount;
    lumpinfo_t*		lump_p;
    int			i;
    int			handle;
    int			length;
    filelump_t*		fileinfo;
	
    if (!reloadname)
	return;
		
    if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
	I_Error ("W_Reload: couldn't open %s",reloadname);

    read (handle, &header, sizeof(header));
    lumpcount = LONG(header.numlumps);
    header.infotableofs = LONG(header.infotableofs);
    length = lumpcount*sizeof(filelump_t);
    fileinfo = alloca (length);
    lseek (handle, header.infotableofs, SEEK_SET);
    read (handle, fileinfo, length);
    
    // Fill in lumpinfo
    lump_p = &lumpinfo[reloadlump];
	
    for (i=reloadlump ;
	 i<reloadlump+lumpcount ;
	 i++,lump_p++, fileinfo++)
    {
	if (lumpcache[i])
	    Z_Free (lumpcache[i]);

	lump_p->position = LONG(fileinfo->filepos);
	lump_p->size = LONG(fileinfo->size);
    }
	
    close (handle);
}



//
// W_InitMultipleFiles
// Pass a null terminated list of files to use.
// All files are optional, but at least one file
//  must be found.
// Files with a .wad extension are idlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//  does override all earlier ones.
//
void W_InitMultipleFiles (char** filenames)
{	
    int		size;
    
    // open all the files, load headers, and count lumps
    numlumps = 0;

    // will be realloced as lumps are added
    lumpinfo = NULL;

    for ( ; *filenames ; filenames++)
	W_AddFile (*filenames);

    if (!numlumps)
	I_Error ("W_InitFiles: no files found");
    
    // set up caching
    size = numlumps * sizeof(*lumpcache);
    lumpcache = malloc (size);
    
    if (!lumpcache)
	I_Error ("Couldn't allocate lumpcache");

    memset (lumpcache,0, size);
}




//
// W_InitFile
// Just initialize from a single file.
//
void W_InitFile (char* filename)
{
    char*	names[2];

    names[0] = filename;
    names[1] = NULL;
    W_InitMultipleFiles (names);
}



//
// W_NumLumps
//
int W_NumLumps (void)
{
    return numlumps;
}



//
// W_CheckNumForName
// Returns -1 if name not found.
//

int W_CheckNumForName (char* name)
{
    union {
	char	s[9];
	int	x[2];
	
    } name8;
    
    int		v1;
    int		v2;
    lumpinfo_t*	lump_p;

    // make the name into two integers for easy compares
    strncpy (name8.s,name,8);

    // in case the name was a fill 8 chars
    name8.s[8] = 0;

    // case insensitive
    strupr (name8.s);		

    v1 = name8.x[0];
    v2 = name8.x[1];


    // scan backwards so patch lump files take precedence
    lump_p = lumpinfo + numlumps;

    while (lump_p-- != lumpinfo)
    {
	if ( *(int *)lump_p->name == v1
	     && *(int *)&lump_p->name[4] == v2)
	{
	    return lump_p - lumpinfo;
	}
    }

    // TFB. Not found.
    return -1;
}




//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName (char* name)
{
    int	i;

    i = W_CheckNumForName (name);
    
    if (i == -1)
      I_Error ("W_GetNumForName: %s not found!", name);
      
    return i;
}


//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength (int lump)
{
    if (lump >= numlumps)
	I_Error ("W_LumpLength: %i >= numlumps",lump);

    return lumpinfo[lump].size;
}



//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//
void
W_ReadLump
( int		lump,
  void*		dest )
{
    int		c;
    lumpinfo_t*	l;
    int		handle;
	
    if (lump >= numlumps)
	I_Error ("W_ReadLump: %i >= numlumps",lump);

    l = lumpinfo+lump;
	
    // ??? I_BeginRead ();
	
    if (l->handle == -1)
    {
	// reloadable file, so use open / read / close
	if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
	    I_Error ("W_ReadLump: couldn't open %s",reloadname);
    }
    else
	handle = l->handle;
		
    lseek (handle, l->position, SEEK_SET);
    c = read (handle, dest, l->size);

    if (c < l->size)
	I_Error ("W_ReadLump: only read %i of %i on lump %i",
		 c,l->size,lump);	

    if (l->handle == -1)
	close (handle);
		
    // ??? I_EndRead ();
}




//
// W_CacheLumpNum
//
void*
W_CacheLumpNum
( int		lump,
  int		tag )
{
    if (lump >= numlumps)
	I_Error ("W_CacheLumpNum: %i >= numlumps",lump);
		
    if (!lumpcache[lump])
    {
	// read the lump in
	
	//printf ("cache miss on lump %i\n",lump);
	(void) Z_Malloc (W_LumpLength (lump), tag, &lumpcache[lump]);
	W_ReadLump (lump, lumpcache[lump]);
    }
    else
    {
	//printf ("cache hit on lump %i\n",lump);
	Z_ChangeTag (lumpcache[lump],tag);
    }
	
    return lumpcache[lump];
}



//
// W_CacheLumpName
//
void*
W_CacheLumpName
( char*		name,
  int		tag )
{
    return W_CacheLumpNum (W_GetNumForName(name), tag);
}


//
// W_Profile
//
__attribute__((section(".ram7")))
int		info[2500][10];
__attribute__((section(".ram7")))
int		profilecount;

void W_Profile (void)
{
    int		i;
    memblock_t*	block;
    void*	ptr;
    char	ch;
    FILE*	f;
    int		j;
    char	name[9];
	
	
    for (i=0 ; i<numlumps ; i++)
    {	
	ptr = lumpcache[i];
	if (!ptr)
	{
	    ch = ' ';
	    continue;
	}
	else
	{
	    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	    if (block->tag < PU_PURGELEVEL)
		ch = 'S';
	    else
		ch = 'P';
	}
	info[i][profilecount] = ch;
    }
    profilecount++;
	
    f = fopen ("waddump.txt","w");
    name[8] = 0;

    for (i=0 ; i<numlumps ; i++)
    {
	memcpy (name,lumpinfo[i].name,8);

	for (j=0 ; j<8 ; j++)
	    if (!name[j])
		break;

	for ( ; j<8 ; j++)
	    name[j] = ' ';

	fprintf (f,"%s ",name);

	for (j=0 ; j<profilecount ; j++)
	    fprintf (f,"    %c",info[i][j]);

	fprintf (f,"\n");
    }
    fclose (f);
}


