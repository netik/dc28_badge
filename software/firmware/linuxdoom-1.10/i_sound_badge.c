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
//	System interface for sound.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_unix.c,v 1.5 1997/02/03 22:45:10 b1 Exp $";

#define ENABLE_DOOM_MUSIC

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <math.h>

#include <sys/time.h>
#include <sys/types.h>

#include "ch.h"
#include "hal.h"
#include "stm32sai_lld.h"

#include "z_zone.h"

#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"

#ifdef ENABLE_DOOM_MUSIC
#include "wildmidi_lib.h"
#endif

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.


// Needed for calling the actual sound output.
#define SAMPLECOUNT_MAX		650
#define SAMPLECOUNT_NORMAL	320
__attribute__((section(".ram7")))
static int SAMPLECOUNT;
#define NUM_CHANNELS		8
// It is 2 for 16bit, and 2 for two channels.
#define BUFMUL                  2
#define MIXBUFFERSIZE		(SAMPLECOUNT_MAX*BUFMUL)

/*
 * We come very close to 11025KHz with some careful manipulation of
 * the SAI PLL. The PLL is set to 192MHz, to which we apply a divisor
 * of 17 and then another divisor of 2. If we then apply the LRCLK
 * divisor of 256 and an additional divisor of 2 in the SAI, we
 * get:
 *
 * 192000000 / 17 / 2 / 256 / 2 == 11029.412KHz
 *
 * We pretend this is close enough to 11025 that nobody will notice
 * the difference.
 */

#define SAMPLERATE		11025	// Hz
#define SAMPLESIZE		2   	// 16bit

#ifdef ENABLE_DOOM_MUSIC
__attribute__((section(".ram7")))
static midi * songhandle;
__attribute__((section(".ram7")))
static bool song_paused = FALSE;
#endif

// The actual lengths of all sound effects.
__attribute__((section(".ram7")))
static int	lengths[NUMSFX];

// The global mixing buffer.
// Basically, samples from all active internal channels
//  are modifed and added, and stored in the buffer
//  that is submitted to the audio device.
__attribute__((section(".ram7")))
static signed short *   mixbuffer;

__attribute__((section(".ram7")))
static signed short	mixbuffer0[MIXBUFFERSIZE];

__attribute__((section(".ram7")))
static signed short	mixbuffer1[MIXBUFFERSIZE];

// The channel step amount...
__attribute__((section(".ram7")))
static unsigned int	channelstep[NUM_CHANNELS];
// ... and a 0.16 bit remainder of last step.
__attribute__((section(".ram7")))
static unsigned int	channelstepremainder[NUM_CHANNELS];


// The channel data pointers, start and end.
__attribute__((section(".ram7")))
static unsigned char*	channels[NUM_CHANNELS];
__attribute__((section(".ram7")))
static unsigned char*	channelsend[NUM_CHANNELS];


// Time/gametic that the channel started playing,
//  used to determine oldest, which automatically
//  has lowest priority.
// In case number of active sounds exceeds
//  available channels.
__attribute__((section(".ram7")))
static int		channelstart[NUM_CHANNELS];

// The sound in channel handles,
//  determined on registration,
//  might be used to unregister/stop/modify,
//  currently unused.
__attribute__((section(".ram7")))
static int 		channelhandles[NUM_CHANNELS];

// SFX id of the playing sound effect.
// Used to catch duplicates (like chainsaw).
__attribute__((section(".ram7")))
static int		channelids[NUM_CHANNELS];			

// Pitch to stepping lookup, unused.
__attribute__((section(".ram7")))
static int		steptable[256];

// Volume lookups.
__attribute__((section(".ram7")))
static int*		vol_lookup;

// Hardware left and right channel volume lookup.
__attribute__((section(".ram7")))
static int*		channelleftvol_lookup[NUM_CHANNELS];
__attribute__((section(".ram7")))
static int*		channelrightvol_lookup[NUM_CHANNELS];



//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
static void*
getsfx
( char*         sfxname,
  int*          len )
{
    unsigned char*      sfx;
    unsigned char*      paddedsfx;
    int                 i;
    int                 size;
    int                 paddedsize;
    char                name[20];
    int                 sfxlump;

    
    // Get the sound data from the WAD, allocate lump
    //  in zone memory.
    sprintf(name, "ds%s", sfxname);

    // Now, there is a severe problem with the
    //  sound handling, in it is not (yet/anymore)
    //  gamemode aware. That means, sounds from
    //  DOOM II will be requested even with DOOM
    //  shareware.
    // The sound list is wired into sounds.c,
    //  which sets the external variable.
    // I do not do runtime patches to that
    //  variable. Instead, we will use a
    //  default sound for replacement.
    if ( W_CheckNumForName(name) == -1 )
      sfxlump = W_GetNumForName("dspistol");
    else
      sfxlump = W_GetNumForName(name);
    
    size = W_LumpLength( sfxlump );

    // Debug.
    // fprintf( stderr, "." );
    //fprintf( stderr, " -loading  %s (lump %d, %d bytes)\n",
    //	     sfxname, sfxlump, size );
    //fflush( stderr );
    
    sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );

    // Pads the sound effect out to the mixing buffer size.
    // The original realloc would interfere with zone memory.
    paddedsize = ((size-8 + (SAMPLECOUNT-1)) / SAMPLECOUNT) * SAMPLECOUNT;

    // Allocate from zone memory.
    paddedsfx = (unsigned char*)Z_Malloc( paddedsize+8, PU_STATIC, 0 );
    // ddt: (unsigned char *) realloc(sfx, paddedsize+8);
    // This should interfere with zone memory handling,
    //  which does not kick in in the soundserver.

    // Now copy and pad.
    memcpy(  paddedsfx, sfx, size );
    for (i=size ; i<paddedsize+8 ; i++)
        paddedsfx[i] = 128;

    // Remove the cached lump.
    Z_Free( sfx );
    
    // Preserve padded length.
    *len = paddedsize;

    // Return allocated padded data.
    return (void *) (paddedsfx + 8);
}





//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
static int
addsfx
( int		sfxid,
  int		volume,
  int		step,
  int		seperation )
{
    static unsigned short	handlenums = 0;
 
    int		i;
    int		rc = -1;
    
    int		oldest = gametic;
    int		oldestnum = 0;
    int		slot;

    int		rightvol;
    int		leftvol;

    // Chainsaw troubles.
    // Play these sound effects only one at a time.
    if ( sfxid == sfx_sawup
	 || sfxid == sfx_sawidl
	 || sfxid == sfx_sawful
	 || sfxid == sfx_sawhit
	 || sfxid == sfx_stnmov
	 || sfxid == sfx_pistol	 )
    {
	// Loop all channels, check.
	for (i=0 ; i<NUM_CHANNELS ; i++)
	{
	    // Active, and using the same SFX?
	    if ( (channels[i])
		 && (channelids[i] == sfxid) )
	    {
		// Reset.
		channels[i] = 0;
		// We are sure that iff,
		//  there will only be one.
		break;
	    }
	}
    }

    // Loop all channels to find oldest SFX.
    for (i=0; (i<NUM_CHANNELS) && (channels[i]); i++)
    {
	if (channelstart[i] < oldest)
	{
	    oldestnum = i;
	    oldest = channelstart[i];
	}
    }

    // Tales from the cryptic.
    // If we found a channel, fine.
    // If not, we simply overwrite the first one, 0.
    // Probably only happens at startup.
    if (i == NUM_CHANNELS)
	slot = oldestnum;
    else
	slot = i;

    // Okay, in the less recent channel,
    //  we will handle the new SFX.
    // Set pointer to raw data.
    channels[slot] = (unsigned char *) S_sfx[sfxid].data;
    // Set pointer to end of raw data.
    channelsend[slot] = channels[slot] + lengths[sfxid];

    // Reset current handle number, limited to 0..100.
    if (!handlenums)
	handlenums = 100;

    // Assign current handle number.
    // Preserved so sounds could be stopped (unused).
    channelhandles[slot] = rc = handlenums++;

    // Set stepping???
    // Kinda getting the impression this is never used.
    channelstep[slot] = step;
    // ???
    channelstepremainder[slot] = 0;
    // Should be gametic, I presume.
    channelstart[slot] = gametic;

    // Separation, that is, orientation/stereo.
    //  range is: 1 - 256
    seperation += 1;

    // Per left/right channel.
    //  x^2 seperation,
    //  adjust volume properly.
    leftvol =
	volume - ((volume*seperation*seperation) >> 16); ///(256*256);
    seperation = seperation - 257;
    rightvol =
	volume - ((volume*seperation*seperation) >> 16);	

    leftvol *= 10;
    if (leftvol > 127)
        leftvol = 127;

    rightvol *= 10;
    if (rightvol > 127)
        rightvol = 127;

    // Sanity check, clamp volume.
    if (rightvol < 0 || rightvol > 127)
	I_Error("rightvol out of bounds");
    
    if (leftvol < 0 || leftvol > 127)
	I_Error("leftvol out of bounds");
    
    // Get the proper lookup table piece
    //  for this volume level???
    channelleftvol_lookup[slot] = &vol_lookup[leftvol*256];
    channelrightvol_lookup[slot] = &vol_lookup[rightvol*256];

    // Preserve sound SFX id,
    //  e.g. for avoiding duplicates of chainsaw.
    channelids[slot] = sfxid;

    // You tell me.
    return rc;
}





//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels(void)
{
  // Init internal lookups (raw data, mixing buffer, channels).
  // This function sets up internal lookups used during
  //  the mixing process. 
  int		i;
  int		j;
    
  int*	steptablemid = steptable + 128;
  
  // Okay, reset internal mixing channels to zero.
  /*for (i=0; i<NUM_CHANNELS; i++)
  {
    channels[i] = 0;
  }*/

  // This table provides step widths for pitch parameters.
  // I fail to see that this is currently used.
  for (i=-128 ; i<128 ; i++)
    steptablemid[i] = (int)(pow(2.0, (i/64.0))*65536.0);
  
  
  // Generates volume lookup tables
  //  which also turn the unsigned samples
  //  into signed samples.
  vol_lookup = (int *)malloc (sizeof(int) * 128 * 256);
  for (i=0 ; i<128 ; i++)
    for (j=0 ; j<256 ; j++)
      vol_lookup[i*256+j] = (i*(j-128)*256)/127;
}	

 
void I_SetSfxVolume(int volume)
{
  // Identical to DOS.
  // Basically, this should propagate
  //  the menu/config file setting
  //  to the state variable used in
  //  the mixing.
  snd_SfxVolume = volume;
}

// MUSIC API - dummy. Some code from DOS version.
void I_SetMusicVolume(int volume)
{
#ifdef ENABLE_DOOM_MUSIC
  int v;
#endif

  // Internal state variable.
  snd_MusicVolume = volume;
  // Now set volume on output device.
  // Whatever( snd_MusciVolume );

#ifdef ENABLE_DOOM_MUSIC
  v = volume * 10;
  if (v > 127)
    v = 127;

  if (songhandle != NULL)
    WildMidi_MasterVolume (v);
#endif
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{

  // UNUSED
  (void)priority;

  // Debug.
  //fprintf( stderr, "starting sound %d", id );
    
  // Returns a handle (not used).
  id = addsfx( id, vol, steptable[pitch], sep );

  // fprintf( stderr, "/handle is %d\n", id );
    
  return id;
}



void I_StopSound (int handle)
{
  // You need the handle returned by StartSound.
  // Would be looping all channels,
  //  tracking down the handle,
  //  an setting the channel to zero.
  
  // UNUSED.
  (void)handle;
}


int I_SoundIsPlaying(int handle)
{
    // Ouch.
    return gametic < handle;
}




//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the global
//  mixbuffer, clamping it to the allowed range,
//  and sets up everything for transferring the
//  contents of the mixbuffer to the (two)
//  hardware channels (left and right, that is).
//
// This function currently supports only 16bit.
//
void I_UpdateSound( void )
{

  
  // Mix current sound data.
  // Data, from raw sound, for right and left.
  register unsigned int	sample;
  register int		dl;
  register int		dr;
  
  // Pointers in global mixbuffer, left, right, end.
  signed short*		leftout;
  signed short*		rightout;
  signed short*		leftend;
  // Step in mixbuffer, left and right, thus two.
  int				step;

  // Mixing channel index.
  int				chan;
    
    /* if there's a song playing, get the samples for it */

#ifdef ENABLE_DOOM_MUSIC
    if (songhandle != NULL && song_paused == FALSE)
      WildMidi_GetOutput (songhandle, (int8_t *)mixbuffer,
        SAMPLECOUNT * BUFMUL * 2);
    else
      memset (mixbuffer, 0, SAMPLECOUNT * BUFMUL * 2);
#endif

    // Left and right channel
    //  are in global mixbuffer, alternating.
    leftout = mixbuffer;
    rightout = mixbuffer+1;
    step = 2;

    // Determine end, for left channel only
    //  (right channel is implicit).
    leftend = mixbuffer + SAMPLECOUNT*step;

    // Mix sounds into the mixing buffer.
    // Loop over step*SAMPLECOUNT,
    //  that is 512 values for two channels.
    while (leftout != leftend)
    {
	// Reset left/right value. 
	dl = 0;
	dr = 0;

	// Love thy L2 chache - made this a loop.
	// Now more channels could be set at compile time
	//  as well. Thus loop those  channels.
	for ( chan = 0; chan < NUM_CHANNELS; chan++ )
	{
	    // Check channel, if active.
	    if (channels[ chan ])
	    {
		// Get the raw data from the channel. 
		sample = *channels[ chan ];
		// Add left and right part
		//  for this channel (sound)
		//  to the current data.
		// Adjust volume accordingly.
		dl += channelleftvol_lookup[ chan ][sample];
		dr += channelrightvol_lookup[ chan ][sample];
		// Increment index ???
		channelstepremainder[ chan ] += channelstep[ chan ];
		// MSB is next sample???
		channels[ chan ] += channelstepremainder[ chan ] >> 16;
		// Limit to LSB???
		channelstepremainder[ chan ] &= 65536-1;

		// Check whether we are done.
		if (channels[ chan ] >= channelsend[ chan ])
		    channels[ chan ] = 0;
	    }
	}
	
#ifdef ENABLE_DOOM_MUSIC
        /* Mix in music samples */

        if (songhandle) {
          dr += *rightout << 2;
          dl += *leftout << 2;
        }
#endif

	// Clamp to range. Left hardware channel.
	// Has been char instead of short.
	// if (dl > 127) *leftout = 127;
	// else if (dl < -128) *leftout = -128;
	// else *leftout = dl;

	if (dl > 0x7fff)
	    *leftout = 0x7fff;
	else if (dl < -0x8000)
	    *leftout = -0x8000;
	else
	    *leftout = dl;

	// Same for right hardware channel.
	if (dr > 0x7fff)
	    *rightout = 0x7fff;
	else if (dr < -0x8000)
	    *rightout = -0x8000;
	else
	    *rightout = dr;

	// Increment current pointers in mixbuffer.
	leftout += step;
	rightout += step;
    }
    return;
}


// 
// This would be used to write out the mixbuffer
//  during each game loop update.
// Updates sound buffer and audio device at runtime. 
// It is called during Timer interrupt with SNDINTR.
// Mixing now done synchronous, and
//  only output be done asynchronous?
//

void
I_SubmitSound(void)
{
  // Write it to DSP device.

  /* Check that previous transfer finished before we start a new one. */

  i2sSamplesWait ();

  /* Start new DMA transfer. */

  i2sSamplesPlay (mixbuffer, SAMPLECOUNT * 2);

  if (mixbuffer == mixbuffer0)
    mixbuffer = mixbuffer1;
  else
    mixbuffer = mixbuffer0;

  return;
}



void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
  // I fail too see that this is used.
  // Would be using the handle to identify
  //  on which channel the sound might be active,
  //  and resetting the channel parameters.

  // UNUSED.
  (void)handle;
  (void)vol;
  (void)sep;
  (void)pitch;
}




void I_ShutdownSound(void)
{
  // Wait till all pending sounds are finished.
  int done = 0;
  int i;
  
#ifdef ENABLE_DOOM_MUSIC
  songhandle = NULL;

  WildMidi_Shutdown ();
#endif

  // FIXME (below).
  for (i = 0; i < NUMMUSIC; i++)
    S_music[i].handle = 0;

  fprintf( stderr, "I_ShutdownSound: NOT finishing pending sounds\n");
  fflush( stderr );
  
  while ( !done )
  {
    for( i=0 ; i<8 && !channels[i] ; i++);
    
    // FIXME. No proper channel output.
    //if (i==8)
    done=1;
  }
  
  // Cleaning up -releasing the DSP device.
  saiSpeed (&SAID2, I2S_SPEED_NORMAL);

  free (vol_lookup);
  vol_lookup = NULL;

  // Done.
  return;
}






void
I_InitSound(void)
{ 
  int i;
  
  // Secure and configure sound device first.
  fprintf( stderr, "I_InitSound: ");

  saiSpeed (&SAID2, I2S_SPEED_DOOM);
  SAMPLECOUNT = SAMPLECOUNT_NORMAL;

  fprintf(stderr, " configured audio device\n" );

    
  // Initialize external data (all sounds) at start, keep static.
  fprintf( stderr, "I_InitSound: ");
  
  for (i=1 ; i<NUMSFX ; i++)
  { 
    // Alias? Example is the chaingun sound linked to pistol.
    if (!S_sfx[i].link)
    {
      // Load data from WAD file.
      S_sfx[i].data = getsfx( S_sfx[i].name, &lengths[i] );
    }	
    else
    {
      // Previously loaded already?
      S_sfx[i].data = S_sfx[i].link->data;
      lengths[i] = lengths[(S_sfx[i].link - S_sfx)/sizeof(sfxinfo_t)];
    }
  }

  fprintf( stderr, " pre-cached all sound data\n");
  
  // Now initialize mixbuffer with zero.
  for ( i = 0; i< MIXBUFFERSIZE; i++ ) {
    mixbuffer0[i] = 0;
    mixbuffer1[i] = 0;
  }
  mixbuffer = mixbuffer0;
  
  // Finished initialization.
  fprintf(stderr, "I_InitSound: sound module ready\n");

  return;    
}




//
// MUSIC API.
// Still no music done.
// Remains. Dummies.
//
void I_InitMusic(void)		{ }
void I_ShutdownMusic(void)	{ }

__attribute__((section(".ram7")))
static int	looping=0;
__attribute__((section(".ram7")))
static int	musicdies=-1;

void I_PlaySong(int handle, int looping)
{
  int i;
#ifdef ENABLE_DOOM_MUSIC
  musicinfo_t * p;
#endif

  // UNUSED.
  (void)handle;
  (void)looping;
  musicdies = gametic + TICRATE*30;

  if (ticdup == 2)
    SAMPLECOUNT = SAMPLECOUNT_MAX;
  else
    SAMPLECOUNT = SAMPLECOUNT_NORMAL;

#ifdef ENABLE_DOOM_MUSIC
  for (i = 0; i < NUMMUSIC; i++) {
    if (S_music[i].handle == handle)
      break;
  }

  if (i == NUMMUSIC)
    return;

  p = &S_music[i];

  WildMidi_Init ("/midi/wildmidi.cfg", SAMPLERATE, WM_MO_LOOP);

  songhandle = WildMidi_OpenBuffer (p->data, W_LumpLength (p->lumpnum));

  i = snd_MusicVolume * 10;
  if (i > 127)
    i = 127;
  WildMidi_MasterVolume (i);
#endif

  return;
}

void I_PauseSong (int handle)
{
  // UNUSED.
  (void)handle;

#ifdef ENABLE_DOOM_MUSIC
  song_paused = TRUE;
#endif

  return;
}

void I_ResumeSong (int handle)
{
  // UNUSED.
  (void)handle;

#ifdef ENABLE_DOOM_MUSIC
  song_paused = FALSE;
#endif

  return;
}

void I_StopSong(int handle)
{
  // UNUSED.
  (void)handle;
  
  looping = 0;
  musicdies = 0;

#ifdef ENABLE_DOOM_MUSIC
  songhandle = NULL;
  WildMidi_Shutdown ();
#endif

  return;
}

void I_UnRegisterSong(int handle)
{
#ifdef ENABLE_DOOM_MUSIC
  int i;
#endif
  // UNUSED.
  (void)handle;

#ifdef ENABLE_DOOM_MUSIC
  for (i = 0; i < NUMMUSIC; i++) {
    if (S_music[i].handle == handle)
        break;
  }

  if (i == NUMMUSIC)
    return;

  S_music[i].handle = 0;
#endif

  return;
}

int I_RegisterSong(void* data)
{
  // UNUSED.
  (void)data;
  
  return 1;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
  // UNUSED.
  (void)handle;
  return looping || musicdies > gametic;
}
