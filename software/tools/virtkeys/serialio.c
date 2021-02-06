#include <sys/types.h>
#include <sys/fcntl.h>

#ifdef __linux__
#define __BSD_SOURCE
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <termios.h>

#include "serialio.h"

/*
 * Serial I/O routines.
 */

static int serial_setup (int);

/*
 * Read a character from the serial port. Note that this
 * routine will block until a valid character is read.
 */

int
serial_readchar (int fd, uint8_t * c)
{
	char		b;
	int		r;

	while ((r = read (fd, &b, 1)) == -1)
		;

	if (r != -1) {
		*c = b;
#ifdef MSR_DEBUG
		printf ("[0x%x]\n", b);
#endif
	}

	return (r);
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
	uint8_t b, *p;

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
	return (write (fd, buf, len));
}

/*
 * Set serial line options. We need to set the baud rate and
 * turn off most of the internal processing in the tty layer in
 * order to avoid having some of the output from the card reader
 * interpreted as control characters and swallowed.
 */

static int
serial_setup (int fd)
{
        struct termios	options;


	if (tcgetattr (fd, &options) == -1)
		return (-1);

	/* Set baud rate */

	cfsetispeed (&options, B115200);
	cfsetospeed (&options, B115200);

	/* Control modes */

	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag |= CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_cflag |= CRTSCTS;

	/*
         * Local modes
         * We have to clear the ISIG flag to defeat signal
	 * processing in order to see the file separator
	 * character (0x1C) which the device will send as
	 * part of its end of record markers.
	 */

	options.c_lflag &= ~ICANON;
	options.c_lflag &= ~ECHO;
	options.c_lflag &= ~ECHONL;
	options.c_lflag &= ~ISIG;
	options.c_lflag &= ~IEXTEN;

	/* Input modes */

	options.c_iflag &= ~ICRNL;
	options.c_iflag &= ~IXON;
	options.c_iflag &= ~IXOFF;
	options.c_iflag &= ~IXANY;
	options.c_iflag |= IGNBRK;
	options.c_iflag |= IGNPAR;

	/* Output modes */

	options.c_oflag &= ~OPOST;

	if (tcsetattr (fd, TCSANOW, &options) == -1)
		return (-1);

	return (0);
}

#ifndef O_FSYNC
#define O_FSYNC 0
#endif

int
serial_open (char *path, int * fd)
{
	int		f;

	f = open (path, O_RDWR | O_FSYNC);

	if (f == -1)
		return (-1);

	if (serial_setup (f) != 0) {
		close (f);
		return (-1);
	}

	*fd = f;

	return (0);
}

int
serial_close (int fd)
{
	close (fd);
	return (0);
}
