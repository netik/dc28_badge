OBJS= serialio.o main.o undrattk.o
CFLAGS= -g -O2 -Wall -ansi -pedantic -I/usr/local/include/SDL2

# This is a bit of a hack. We want to take the .bmp splash image and
# turn it into an ELF object that we can link directly (which is a little
# nicer than using a script to turn it into a C header file with a giant
# byte array. But to do that we need to know the host system's native
# object format, which can vary. We try to figure it out automatically
# by looking at the format of the main.o object we just compiled
# using objdump.

OBJECT=main.o
OBJFORMAT= objdump -x $(OBJECT) | grep format | cut -d ' ' -f 8
MACHINE= objdump -x $(OBJECT) | grep architecture | sed s/:/,/ | cut -d ',' -f 2

virtkeys: $(OBJS)
	cc -o virtkeys $(OBJS) -L/usr/local/lib -lSDL2

undrattk.o: undrattk.bmp main.o
	objcopy -I binary -O `$(OBJFORMAT)` -B `$(MACHINE)` undrattk.bmp undrattk.o

clean:
	/bin/rm -f *.o virtkeys
