/*-
 * Copyright (c) 2019
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This module provides some OS-specific shims for the newlib libc
 * implementation so that we can use printf() and friends as well as
 * malloc() and free(). ChibiOS has its own heap management routines
 * (chHeapAlloc()/chHeapFree()) which must be disabled for us to use
 * the ones in newlib.
 *
 * Technically we can get away with the default _sbrk() shim in newlib,
 * but the one here uses __heap_base__ instead of "end" as the heap
 * start pointer which is slightly more correct.
 *
 * We need to add the "used" attribute tag to these functions to avoid a
 * potential problem with LTO. If newlib is built with link-time optimization
 * then there's no problem, but if it's not, the LTO optimizer can
 * mistakenly think that these functions are not needed (possibly because
 * they also appear in libnosys.a) and it will optimize them out. Using
 * the tag defeats this. (It's redundant but harmless when not using LTO.)
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "ff.h"

#include "badge.h"

static mutex_t malloc_mutex;
static mutex_t file_mutex;

#define MAX_FILES 5
static FIL file_handles[MAX_FILES];
static int file_used[MAX_FILES];

void
newlibStart (void)
{
	osalMutexObjectInit (&malloc_mutex);
	osalMutexObjectInit (&file_mutex);
}


__attribute__((used))
int
_read (int file, char * ptr, int len)
{
	int i;
	FIL * f;
	UINT br;

	/* descriptor 0 is always stdin */

	if (file == 0) {
		streamRead (console, (uint8_t *)&ptr[0], 1);
		return (1);
	}

	/* Can't read from stdout or stderr */

	if (file == 1 || file == 2 || file > MAX_FILES) {
		errno = EINVAL;
		return (-1);
	}

	i = file - 3;
	if (file_used[i] == 0) {
		errno = EINVAL;
		return (-1);
	}

	f = &file_handles [i];

	if (f_read (f, ptr, len, &br) != FR_OK) {
		errno = EIO;
		return (-1);
	}

	return (br);
}

__attribute__((used))
int
_write (int file, char * ptr, int len)
{
	int i;
	UINT br;
	FIL * f;

	if (file == 1 || file == 2) {
		for (i = 0; i < len; i++) {
			if (ptr[i] == '\n')
				streamPut (console, '\r');
			streamPut (console, ptr[i]);
		}
		return (len);
	}

	if (file == 0 || file > MAX_FILES) {
		errno = EINVAL;
		return (-1);
	}

	i = file - 3;

	if (file_used[i] == 0) {
		errno = EINVAL;
		return (-1);
	}

	f = &file_handles [i];

	if (f_write (f, ptr, len, &br) != FR_OK) {
		errno = EIO;
		return (-1);
	}

	return (br);
}

__attribute__((used))
int
_open (const char * file, int flags, int mode)
{
	FIL * f;
	FRESULT r;
	BYTE fmode = 0;
	int i;

	(void)mode;

	osalMutexLock (&file_mutex);

	for (i = 0; i < MAX_FILES; i++) {
		if (file_used[i] == 0)
			break;
	}

	if (i == MAX_FILES) {
		errno = ENOSPC;
		osalMutexUnlock (&file_mutex);
		return (-1);
	}

	f = &file_handles[i];

	fmode = FA_READ;

	if (flags & O_WRONLY)
		fmode = FA_WRITE;
	if (flags & O_RDWR)
		fmode = FA_READ|FA_WRITE;

	if (flags & O_CREAT)
		fmode |= FA_CREATE_ALWAYS;
	if (flags & O_APPEND)
		fmode |= FA_OPEN_APPEND;

	r = f_open (f, file, fmode);

	if (r != FR_OK) {
		if (r == FR_NO_FILE)
			errno = ENOENT;
		else if (r == FR_DENIED)
			errno = EPERM;
		else if (r == FR_EXIST)
			errno = EEXIST;
		else
			errno = EIO;
		osalMutexUnlock (&file_mutex);
		return (-1);
	}

	file_used[i] = 1;

	/*
	 * stdin, stdout and stderr are descriptors
	 * 0, 1 and 2, so start at 3
	 */

	i += 3;

	osalMutexUnlock (&file_mutex);

	return (i);
}

__attribute__((used))
int
_close (int file)
{
	int i;
	FIL * f;

	if (file < 3 || file > MAX_FILES) {
		errno = EINVAL;
		return (-1);
	}

	osalMutexLock (&file_mutex);

	i = file - 3;

	if (file_used[i] == 0) {
		errno = EINVAL;
		osalMutexUnlock (&file_mutex);
		return (-1);
	}

	f = &file_handles [i];
	file_used[i] = 0;

	f_close (f);

	memset (&file_handles[i], 0, sizeof(FIL));

	osalMutexUnlock (&file_mutex);

	return (0);
}

__attribute__((used))
int
_lseek (int file, int ptr, int dir)
{
	int i;
	FIL * f;
	FSIZE_t offset = 0;

	/* Not sure how to handle SEEK_END */

	if (dir == SEEK_END) {
		errno = EINVAL;
		return (-1);
	}

	if (file < 3 || file > MAX_FILES) {
		errno = EINVAL;
		return (-1);
	}

	i = file - 3;

	if (file_used[i] == 0) {
		errno = EINVAL;
		return (-1);
	}

	f = &file_handles [i];

	if (dir == SEEK_SET)
		offset = ptr;

	if (dir == SEEK_CUR)
		offset = f_tell (f) + ptr;

	if (f_lseek (f, offset) != FR_OK) {
		errno = EIO;
		return (-1);
	}

	return (offset);
}

__attribute__((used))
int
_stat (const char * file, struct stat * st)
{
	FILINFO f;
	FRESULT r;

	r = f_stat (file, &f);

	if (r != FR_OK) {
		if (r == FR_NO_FILE)
			errno = ENOENT;
		else if (r == FR_DENIED)
			errno = EPERM;
		else
			errno = EIO;
		return (-1);

	}

	st->st_size = f.fsize;
	st->st_blksize = 512;
	st->st_blocks = f.fsize / 512;
	if (st->st_blocks == 0)
		st->st_blocks++;

	return (0);
}

__attribute__((used))
int
_fstat (int desc, struct stat * st)
{
	FIL * f;
	int i;

	st->st_blksize = 512;
	st->st_size = 0;
	st->st_blocks = 0;

	if (desc < 3)
		return (0);

	if (desc > MAX_FILES) {
		errno = EINVAL;
		return (-1);
	}

	i = desc - 3;
	if (file_used[i] == 0) {
		errno = EINVAL;
		return (-1);
	}

	f = &file_handles [i];

	st->st_size = f_size (f);
	st->st_blocks = f_size (f) / 512;
	if (st->st_blocks == 0)
		st->st_blocks++;

	return (0);
}

__attribute__((used))
int
_mkdir (const char * path, mode_t mode)
{
	(void)mode;

	if (f_mkdir (path) != FR_OK)
		return (-1);
	return (0);
}

__attribute__((used, alias ("_mkdir")))
int
mkdir (const char * path, mode_t mode);

__attribute__((used))
char *
getcwd (char * buf, size_t size)
{
	if (f_getcwd ((TCHAR *)buf, (UINT)size) != FR_OK)
		return (NULL);
	return (buf);
}

__attribute__((used))
int
chdir (const char * path)
{
	if (f_chdir (path) != FR_OK)
		return (-1);
	return (0);
}

__attribute__((used))
int
_isatty (int fd)
{
	if (fd >=0 && fd < 3)
		return (1);
	return (0);
}

__attribute__((used))
int
_gettimeofday (struct timeval * ptimeval, void * ptimezone)
{
	systime_t now;
	time_t secs;
	double usecs;

	(void)ptimezone;

	now = chVTGetSystemTimeX();

	secs = now;
	secs /= CH_CFG_ST_FREQUENCY;

	usecs = now;
	usecs -= (secs * CH_CFG_ST_FREQUENCY);
	usecs = (usecs / CH_CFG_ST_FREQUENCY) * 1000000;

	ptimeval->tv_sec = secs;
	ptimeval->tv_usec = usecs;

	return (0);
}

__attribute__((used))
int
usleep (useconds_t usecs)
{
	chThdSleepMicroseconds (usecs);
	return (0);
}

__attribute__((used))
void
__malloc_lock (struct _reent * ptr)
{
	(void)ptr;
	osalMutexLock (&malloc_mutex);
	return;
}

__attribute__((used))
void
__malloc_unlock (struct _reent * ptr)
{
	(void)ptr;
	osalMutexUnlock (&malloc_mutex);
	return;
}

__attribute__((used))
void *
_sbrk (int incr)
{
	static char * heap_end;
	char *        prev_heap_end;

	if (heap_end == NULL)
		heap_end = HEAP_BASE;
    
	if ((heap_end + incr) > HEAP_END) {
		errno = ENOMEM;
		return ((void *)-1);
	}

	prev_heap_end = heap_end;
	heap_end += incr;

	return (void *) prev_heap_end;
}
