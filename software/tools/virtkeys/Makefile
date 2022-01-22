OBJS= main.o undrattk.o

detected_OS := $(shell sh -c 'uname 2>/dev/null || echo Unknown')

ifeq ($(detected_OS), Linux)

# Linux doesn't define CRTSCTS when using -ansi.

CFLAGS= -g -O2 -Wall
OBJS+= serialio.o
LIBS= -L/usr/local/lib -lSDL2

else ifeq ($(OS), Windows_NT)

CFLAGS= -g -O2 -Wall -ansi -pedantic
OBJS+= serialio_win.o
LIBS += -static -lmingw32 -lSDL2main -lSDL2 -lsetupapi -lgdi32
LIBS += -lwinmm -limm32 -lole32 -loleaut32 -lversion
OBJOPTS = --change-leading-char

else

# Works with FreeBSD and MacOS

CFLAGS= -g -O2 -Wall -ansi -pedantic -I/usr/local/include
OBJS+= serialio.o
LIBS= -L/usr/local/lib -lSDL2

endif

# This is a bit of a hack. We want to take the .bmp splash image and
# turn it into an ELF object that we can link directly (which is a little
# nicer than using a script to turn it into a C header file with a giant
# byte array. But to do that we need to know the host system's native
# object format, which can vary. We try to figure it out automatically
# by looking at the format of the main.o object we just compiled
# using objdump.

OBJECT= main.o
OBJFORMAT= objdump -x $(OBJECT) | grep format | cut -d ' ' -f 8
MACHINE= objdump -x $(OBJECT) | grep architecture | sed s/:/,/ | cut -d ',' -f 2

virtkeys: $(OBJS)
	$(CC) -o virtkeys $(OBJS) $(LIBS)

undrattk.o: undrattk.bmp main.o
	objcopy $(OBJOPTS) -I binary -O `$(OBJFORMAT)` -B `$(MACHINE)` undrattk.bmp undrattk.o

clean:
	/bin/rm -f *.o virtkeys
