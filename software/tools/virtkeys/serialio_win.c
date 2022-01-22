#include <sys/types.h>
#include <sys/fcntl.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <tchar.h>

#include "serialio.h"

static HANDLE port_handle;

/*
 * Serial I/O routines.
 */

static int serial_setup (HANDLE);

/*
 * Read a character from the serial port. Note that this
 * routine will block until a valid character is read.
 */

int
serial_readchar (int fd, uint8_t * c)
{
	char		b;
	BOOL		r;
	DWORD		received;

	while ((r = ReadFile (port_handle, &b, 1, &received, NULL)) == FALSE)
		;

	if (r != -1) {
		*c = b;
#ifdef MSR_DEBUG
		printf ("[0x%x]\n", b);
#endif
	}

	return (received);
}

/*
 * Read a series of characters from the serial port. This
 * routine will block until the desired number of characters
 * is read.
 */

int
serial_read (int fd, void * buf, size_t len)
{
	size_t i;
	uint8_t b = 0, *p;

	p = buf;

	for (i = 0; i < len; i++) {
		serial_readchar (fd, &b);
		p[i] = b;
	}

	return (0);
}

int
serial_write (int fd, void * buf, size_t len)
{
	DWORD written;
	BOOL r;

	(void)fd;

	r = WriteFile (port_handle, buf, len, &written, NULL);

	if (r == FALSE)
		return (-1);

	return (written);
}

/*
 * Set serial line options. We need to set the baud rate and
 * turn off most of the internal processing in the tty layer in
 * order to avoid having some of the output from the card reader
 * interpreted as control characters and swallowed.
 */

static int
serial_setup (HANDLE fd)
{
	DCB dcb;
	BOOL r;

	SecureZeroMemory (&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);

	r = GetCommState (fd, &dcb);

	if (r == FALSE) {
		fprintf (stderr, "getting COM port state failed\n");
		return (-1);
	}

	dcb.BaudRate = CBR_115200;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	r = SetCommState (fd, &dcb);

	if (r == FALSE) {
		fprintf (stderr, "setting COM port state failed\n");
		return (-1);
	}

	return (0);
}

int
serial_open (char *path, int * fd)
{
	HANDLE		f;
	TCHAR *		port;

	port = TEXT(path);

	f = CreateFile (port, GENERIC_READ | GENERIC_WRITE, 0,
	    NULL, OPEN_EXISTING, 0, NULL);

	if (f == INVALID_HANDLE_VALUE) {
		fprintf (stderr, "opening [%s] failed\n", path);
		return (-1);
	}

	if (serial_setup (f) != 0) {
		CloseHandle (f);
		return (-1);
	}

	port_handle = f;

	*fd = 1;

	return (0);
}

int
serial_close (int fd)
{
	CloseHandle (port_handle);
	return (0);
}
