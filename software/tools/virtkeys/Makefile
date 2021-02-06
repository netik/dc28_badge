OBJS= serialio.o main.o
CFLAGS= -g -O2 -Wall -ansi -pedantic -I/usr/local/include/SDL2

xkey: $(OBJS)
	cc -o virtkeys $(OBJS) -L/usr/local/lib -lSDL2 -lSDL2_image

clean:
	/bin/rm -f *.o virtkeys
