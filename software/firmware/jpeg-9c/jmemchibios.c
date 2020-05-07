/*
 * jmemansi.c
 *
 * Copyright (C) 1992-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file provides a simple generic implementation of the system-
 * dependent portion of the JPEG memory manager.  This implementation
 * assumes that you have the ANSI-standard library routine tmpfile().
 * Also, the problem of determining the amount of memory available
 * is shoved onto the user.
 */

#include "ch.h"
#include "hal.h"

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jmemsys.h"		/* import the system-dependent declarations */

#ifndef HAVE_STDLIB_H		/* <stdlib.h> should declare chHeapAlloc(),free() */
extern void * malloc JPP((size_t size));
extern void free JPP((void *ptr));
#endif

#ifndef SEEK_SET		/* pre-ANSI systems may not define this; */
#define SEEK_SET  0		/* if not, assume 0 is correct */
#endif

#include "ch.h"

#include <string.h>
#include <stdlib.h>

#ifdef notdef
/* Allocate these from the heap to save BSS space. */
static uint8_t b0[84];
static uint8_t b1[1784];
static uint8_t b2[16280];
static uint8_t b3[976];
static uint8_t b4[1296];
static uint8_t b5[5136];
static uint8_t b6[1296];
static uint8_t b7[1296];

static uint8_t * buffers[] = {
	b0, b1, b2, b3, b4, b5, b6, b7
};
#endif

static uint8_t bcnt = 0;
static uint8_t * buffers[8];

/*
 * Memory allocation and freeing are controlled by the regular library
 * routines malloc() and free().
 */
volatile int scnt = 0;
GLOBAL(void *)
jpeg_get_small (j_common_ptr cinfo, size_t sizeofobject)
{
  void * r;
  (void)cinfo;
  (void)sizeofobject;
  r = buffers[bcnt];
  bcnt++;
  return (r);
}

GLOBAL(void)
jpeg_free_small (j_common_ptr cinfo, void * object, size_t sizeofobject)
{
  (void)cinfo;
  (void)object;
  (void)sizeofobject;
  bcnt--;
}


/*
 * "Large" objects are treated the same as "small" ones.
 * NB: although we include FAR keywords in the routine declarations,
 * this file won't actually work in 80x86 small/medium model; at least,
 * you probably won't be able to process useful-size images in only 64KB.
 */

GLOBAL(void FAR *)
jpeg_get_large (j_common_ptr cinfo, size_t sizeofobject)
{
  void * r;
  (void)cinfo;
  (void)sizeofobject;
  r = buffers[bcnt];
  bcnt++;
  return (r);
}

GLOBAL(void)
jpeg_free_large (j_common_ptr cinfo, void FAR * object, size_t sizeofobject)
{
  (void)cinfo;
  (void)sizeofobject;
  (void)object;
  bcnt--;
}


/*
 * This routine computes the total memory space available for allocation.
 * It's impossible to do this in a portable way; our current solution is
 * to make the user tell us (with a default value set at compile time).
 * If you can actually get the available space, it's a good idea to subtract
 * a slop factor of 5% or so.
 */

#ifndef DEFAULT_MAX_MEM		/* so can override from makefile */
#define DEFAULT_MAX_MEM		1000000L /* default: one megabyte */
#endif

GLOBAL(long)
jpeg_mem_available (j_common_ptr cinfo, long min_bytes_needed,
		    long max_bytes_needed, long already_allocated)
{
  (void)cinfo;
  (void)min_bytes_needed;
  (void)max_bytes_needed;
  return cinfo->mem->max_memory_to_use - already_allocated;
}


/*
 * Backing store (temporary file) management.
 * Backing store objects are only used when the value returned by
 * jpeg_mem_available is less than the total space needed.  You can dispense
 * with these routines if you have plenty of virtual memory; see jmemnobs.c.
 */


METHODDEF(void)
read_backing_store (j_common_ptr cinfo, backing_store_ptr info,
		    void FAR * buffer_address,
		    long file_offset, long byte_count)
{
#ifdef notdef
  if (fseek(info->temp_file, file_offset, SEEK_SET))
    ERREXIT(cinfo, JERR_TFILE_SEEK);
  if (JFREAD(info->temp_file, buffer_address, byte_count)
      != (size_t) byte_count)
    ERREXIT(cinfo, JERR_TFILE_READ);
#endif
  (void)cinfo;
  (void)info;
  (void)buffer_address;
  (void)file_offset;
  (void)byte_count;
}


METHODDEF(void)
write_backing_store (j_common_ptr cinfo, backing_store_ptr info,
		     void FAR * buffer_address,
		     long file_offset, long byte_count)
{
#ifdef notdef
  if (fseek(info->temp_file, file_offset, SEEK_SET))
    ERREXIT(cinfo, JERR_TFILE_SEEK);
  if (JFWRITE(info->temp_file, buffer_address, byte_count)
      != (size_t) byte_count)
    ERREXIT(cinfo, JERR_TFILE_WRITE);
#endif
  (void)cinfo;
  (void)info;
  (void)buffer_address;
  (void)file_offset;
  (void)byte_count;
}


METHODDEF(void)
close_backing_store (j_common_ptr cinfo, backing_store_ptr info)
{
#ifdef notdef
  fclose(info->temp_file);
  /* Since this implementation uses tmpfile() to create the file,
   * no explicit file deletion is needed.
   */
#endif
  (void)cinfo;
  (void)info;
}


/*
 * Initial opening of a backing-store object.
 *
 * This version uses tmpfile(), which constructs a suitable file name
 * behind the scenes.  We don't have to use info->temp_name[] at all;
 * indeed, we can't even find out the actual name of the temp file.
 */

GLOBAL(void)
jpeg_open_backing_store (j_common_ptr cinfo, backing_store_ptr info,
			 long total_bytes_needed)
{
   (void)cinfo;
   (void)total_bytes_needed;
#ifdef notdef
  if ((info->temp_file = tmpfile()) == NULL)
    ERREXITS(cinfo, JERR_TFILE_CREATE, "");
#endif
  info->read_backing_store = read_backing_store;
  info->write_backing_store = write_backing_store;
  info->close_backing_store = close_backing_store;
}


/*
 * These routines take care of any system-dependent initialization and
 * cleanup required.
 */

GLOBAL(long)
jpeg_mem_init (j_common_ptr cinfo)
{
  (void)cinfo;
  bcnt = 0;
  buffers[0] = malloc (84);
  buffers[1] = malloc (1784);
  buffers[2] = malloc (16280);
  buffers[3] = malloc (976);
  buffers[4] = malloc (1296);
  buffers[5] = malloc (5136);
  buffers[6] = malloc (1296);
  buffers[7] = malloc (1296);

  return DEFAULT_MAX_MEM;	/* default for max_memory_to_use */
}

GLOBAL(void)
jpeg_mem_term (j_common_ptr cinfo)
{
  (void)cinfo;
  free (buffers[0]);
  free (buffers[1]);
  free (buffers[2]);
  free (buffers[3]);
  free (buffers[4]);
  free (buffers[5]);
  free (buffers[6]);
  free (buffers[7]);

  memset (buffers, 0, sizeof(buffers));
  return;
}
