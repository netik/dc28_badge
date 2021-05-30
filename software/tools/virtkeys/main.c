#include <SDL.h>
#include <SDL_video.h>
#include <SDL_image.h>

#include <stdio.h>

#include "serialio.h"

extern char _binary_undrattk_bmp_start[];
extern uint32_t * _binary_undrattk_bmp_size;

static int fd;

static void
keyShow (SDL_KeyboardEvent * key)
{
	char msgbuf[16];

	/* Ignore repeat events */

	if (key->repeat)
		return;

	if (key->type == SDL_KEYDOWN)
		sprintf (msgbuf, "X%XX", key->keysym.sym);

	if (key->type == SDL_KEYUP)
		sprintf (msgbuf, "Y%XY", key->keysym.sym);

	serial_write (fd, msgbuf, strlen(msgbuf));

	return;
}

int
main (int argc, char * argv[])
{
	SDL_Event event;
	SDL_Window * win;
	SDL_Renderer * renderer;
	SDL_RWops * ops;
	SDL_Texture * imgtexture;
	SDL_Surface * imgsurface;
	SDL_Rect rect;
	int h, w;

	if (argc == 1) {
		fprintf (stderr, "specify device path\n");
		exit (-1);
	}

	if (serial_open (argv[1], &fd) == -1) {
		perror ("open failed");
		exit (-1);
	}

	if (SDL_Init (SDL_INIT_VIDEO) < 0) {
		fprintf (stderr, "Initializing SDL failed\n");
		serial_close (fd);
		exit (-1);
	}

	/* Create window and renderer */

	win = SDL_CreateWindow ("Team Ides Virtual Keyboard Input",
	    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	    320, 240, SDL_WINDOW_SHOWN);

	renderer = SDL_CreateRenderer (win, -1, SDL_RENDERER_ACCELERATED);

	ops = SDL_RWFromMem ((void *)_binary_undrattk_bmp_start,
	    (int)(uintptr_t)&_binary_undrattk_bmp_size);

	imgsurface = SDL_LoadBMP_RW (ops, 0);
	imgtexture = SDL_CreateTextureFromSurface (renderer, imgsurface);

	SDL_QueryTexture (imgtexture, NULL, NULL, &w, &h);

	rect.x = 0;
	rect.y = 0;
	rect.h = h;
	rect.w = w;

	/* Send start code */

	serial_write (fd, "\rcapture\r", 10);

	/* Process SDL events */

	while (1) {
		if (SDL_WaitEvent (&event)) {

			/* Check for key press/release events */

			if (event.type == SDL_KEYDOWN ||
			    event.type == SDL_KEYUP)
				keyShow (&event.key);

			/* Exit */

			if (event.type == SDL_QUIT) {
				printf ("Exiting.\n");
				break;
			}

			/* Draw the image */

			if (event.type == SDL_WINDOWEVENT ||
			    event.type == SDL_WINDOWEVENT_EXPOSED) {
				SDL_RenderClear (renderer);
				SDL_RenderCopy (renderer, imgtexture,
				    NULL, &rect);
				SDL_RenderPresent (renderer);
			}
		}
	}

	/* Send exit code */

	serial_write (fd, "Z", 1);

	serial_close (fd);

	SDL_DestroyTexture (imgtexture);
	SDL_FreeSurface (imgsurface);
	SDL_DestroyRenderer (renderer);
	SDL_DestroyWindow (win);

	SDL_Quit ();

	exit (0);
}
