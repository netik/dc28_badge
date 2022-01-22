WHAT IS THIS?

The virtkeys utility can be used to allow your computer's keyboard
to control your badge. This allows you to use your keyboard in place of
the badge's on-board controls, and to play Doom. (Yes, this means you can
use the Doom cheat codes.)

WHY DO I NEED A SPECIAL UTILITY TO DO THAT?

It's relatively easy to provide serial communications over USB, but it's
not as easy to use a serial port to simulate button press and button release
events. Doom in particular expects to use keyboard press/release events
for its motion controls (press the up arrow key to start moving forward
and release it to stop). It also uses modifier keys such as the CTRL
key for firing your weapon and SHIFT to run instead of walk. These keys
don't generate characters when you press them so typing them into a serial
terminal session doesn't do anything.

It's therefore necessary to have a utility to capture keystrokes from
the keyboard and encode them so they can be sent over the serial channel.
Unfortunately the way to directly capture keystrokes can vary depending
on your computer and operating system.

To deal with this, the virtkeys utility is written using the SDL library
which is available for most OSes (at least, for those we want to suport).

HOW DO I USE IT?

In order for this to work, you need to connect your badge to your host
host system via the badge's USB client port. The badge should show up as
a USB modem device. You need to determine the device name. On FreeBSD
it should be something like /dev/cuaU0. Once you know the device name,
do the following:

1) Run the virtkeys utility with the serial device name as an argument,
e.g.:

% virtkeys /dev/cuaU0

2) The virtkeys window should open a small window on your screen. Position
your mouse cursor over this window.

3) Use your keyboard. As long as the mouse cursor remains over the virtkeys
window, all your keystrokes will be captured and sent to the badge over
the serial port.

WHAT SYSTEMS ARE SUPPORTED?

The virtkeys utility has been compiled and tested on the following
platforms:

FreeBSD/amd64 12.3-RELEASE (x86-64)
Ubuntu 20.04 (x86-64)
Windows 10 (x86-64)
MacOS 11 (x86-64)

HOW DO I COMPILE IT MYSELF?

For FreeBSD
-----------

- Install the GNU make utility:

# pkg install gmake

- Install the SDL2 library:

# pkg install sdl2

- Run gmake in the virtkeys source directory

Note that installing SDL2 may also install additional packages as
dependencies.

For Ubuntu Linux
----------------

- Make sure you install the SDL2 development library:

# apt-get install libsdl2-dev

- Make sure you install the libusb library:

# apt-get install libusb-dev

- Run make in the virtkeys source directory

For Windows (to build a 64-bit binary)
--------------------------------------

- Install MSYS2 from https://www.msys2.org/

- Open an MSYS2 shell window

- Install the mingw64 C compiler, SDL library, libusb library and make:

$ pacman -S mingw64/mingw-x64-x86_64-gcc
$ pacman -S mingw64/mingw-x64-x86_64-libusb
$ pacman -S mingw64/mingw-x64-x86_64-SDL2
$ pacman -S msys/make

- Now open a MINGW64 development window:

$ /mingw64.exe

- In the MINGW64 window, navigate to the virtkeys directory and run make

For Windows (to build a 32-bit binary)
--------------------------------------

- Install MSYS2 from https://www.msys2.org/

- Open an MSYS2 shell window

- Install the mingw32 C compiler, SDL library, libusb library and make:

$ pacman -S mingw32/mingw-i686_64-gcc
$ pacman -S mingw32/mingw-i686_64-libusb
$ pacman -S mingw32/mingw-i686_64-SDL2
$ pacman -S msys/make

- Now open a MINGW32 development window:

$ /mingw32.exe

- In the MINGW32 window, navigate to the virtkeys directory and run make

Note that the 32-bit binary should run on both 32-bit and 64-bit Windows
installations.

Launching a separate MINGW64 or MINGW32 shell window is required in order
to compile the virtkeys utility into a fully native Windows executable.
If you run the build from the MSYS shell window instead, you may end up
using the MSYS GCC compiler instead, which will link your executable with
the msys-2.0.dll shared library. This means that you will only be able to
run it on computers that have MSYS 2.0 installed.
