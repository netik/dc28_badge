#!/bin/sh

# This script is used to generate an ARM development toolchain for a
# FreeBSD system using the stock GNU tools and OpenOCD git repo.
# Everything gets put under /usr/local/cortex-m7. These all build out
# of the box using the native Clang compiler.
#
# OpenOCD needs to have hidapi installed.
# For GCC you need mpfr and gmp installed.
#
# Locations for software:
# binutils 2.33.1
# gcc 9.2.0
# gdb 8.3.1
# newlib 3.1.0
# openocd from official git repo
#
# https://ftp.gnu.org/gnu/binutils/binutils-2.33.1.tar.xz
# https://ftp.gnu.org/gnu/gcc/gcc-9.2.0/gcc-9.2.0.tar.xz
# https://ftp.gnu.org/gnu/gdb/gdb-8.3.1.tar.xz
# ftp://sourceware.org/pub/newlib/newlib-3.1.0.tar.gz
# https://github.com/ntfreak/openocd.git
#
# Note: clone the openocd repo, then run the bootstrap script!


set -x

INSTALLPATH=/usr/local/cortex-m7

export PATH=$PATH:$INSTALLPATH/bin

# Use clang, not GCC as host compiler

export CC=cc
export CXX=c++

/bin/rm -fr $INSTALLPATH/*

# First build binutils

cd binutils-2.33.1
/bin/rm -fr build
mkdir build
cd build
../configure --prefix=$INSTALLPATH --target=arm-none-eabi --with-cpu=cortex-m7 --with-mode=thumb --with-gnu-as --with-gnu-ld --enable-languages=c,c++ --with-newlib --disable-nls --disable-gdb --enable-interwork --enable--plugins --with-float-abi=hard
gmake -j4
gmake install

cd ../..

# Now build bootstrap GCC compiler

export CFLAGS="-I/usr/local/include -fbracket-depth=1024"
export CXXFLAGS="-I/usr/local/include -fbracket-depth=1024"
export LDFLAGS="-L/usr/lib -L/usr/local/lib -rpath /usr/lib:/usr/local/lib"

cd gcc-9.2.0
/bin/rm -fr build
mkdir build
cd build
../configure --prefix=$INSTALLPATH		\
	--target=arm-none-eabi			\
	--with-cpu=cortex-m7			\
	--disable-multilib			\
	--with-mode=thumb			\
	--with-gnu-as				\
	--with-gnu-ld				\
	--enable-languages=c			\
	--with-newlib				\
	--disable-nls				\
	--with-float-abi=hard			\
	--disable-libssp			\
	--disable-threads			\
	--disable-libffi			\
	--disable-decimal-float			\
	--disable-libgomp			\
	--disable-libmudflap			\
	--disable-libquadmath			\
	--disable-libstdcxx-pch			\
	--disable-tls				\
	--without-headers

gmake -j4
gmake install

unset CFLAGS
unset CXXFLAGS
unset LDFLAGS

cd ../..

# Now build newlib (libc)

export CFLAGS_FOR_TARGET="-Os -g -ffunction-sections -fdata-sections -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-sp-d16"

cd newlib-3.1.0
/bin/rm -fr build
mkdir build
cd build
../configure --prefix=$INSTALLPATH		\
	--target=arm-none-eabi			\
	--disable-multilib			\
	--disable-nls				\
	--with-float-abi=hard			\
        --enable-fpu				\
        --with-cpu=cortex-m7			\
        --with-fpu=-mfpu=fpv5-sp-d16		\
	--disable-newlib-supplied-syscalls	\
	--enable-newlib-reent-small		\
	--disable-newlib-fvwrite-in-streamio	\
	--disable-newlib-fseek-optimization	\
	--disable-newlib-wide-orient		\
	--enable-newlib-nano-malloc		\
	--disable-newlib-unbuf-stream-opt	\
	--enable-lite-exit			\
	--enable-newlib-global-atexit		\
	--enable-newlib-nano-formatted-io
gmake -j4
gmake install

unset CFLAGS

cd ../..

# Now build full GCC and G++

export CFLAGS="-I/usr/local/include -fbracket-depth=1024"
export CXXFLAGS="-I/usr/local/include -fbracket-depth=1024"
export LDFLAGS="-L/usr/lib -L/usr/local/lib -rpath /usr/lib:/usr/local/lib"

cd gcc-9.2.0
/bin/rm -fr build
mkdir build
cd build
../configure --prefix=$INSTALLPATH		\
	--target=arm-none-eabi			\
	--with-cpu=cortex-m7			\
	--disable-multilib			\
	--with-mode=thumb			\
	--with-gnu-as				\
	--with-gnu-ld				\
	--enable-languages=c,c++		\
	--enable-plugins			\
	--with-newlib				\
	--with-headers=yes			\
	--disable-nls				\
	--with-float-abi=hard			\
	--disable-libffi			\
	--disable-libgomp			\
	--disable-libmudflap			\
	--disable-libquadmath			\
	--disable-libssp			\
    	--disable-libstdcxx-pch			\
	--disable-threads			\
	--disable-tls


gmake -j4
gmake install

unset CFLAGS
unset CXXFLAGS
unset LDFLAGS

cd ../..

# Now build GDB

export LDFLAGS="-L/usr/lib -L/usr/local/lib -rpath /usr/lib:/usr/local/lib"

cd gdb-8.3.1
/bin/rm -fr build
mkdir build
cd build
../configure --prefix=$INSTALLPATH --with-python=no --target=arm-none-eabi --disable-nls --with-expat
gmake -j4
gmake install

unset LDFLAGS

cd ../..

# Lastly, build OpenOCD

export LIBUSB0_CFLAGS=-I/usr/include
export LIBUSB0_LIBS=-L/usr/lib
export libusb_CFLAGS=-I/usr/include
export libusb_LDFLAGS=-L/usr/lib
export libusb_LIBS=-lusb

cd openocd
/bin/rm -fr build
mkdir build
cd build
../configure --prefix=$INSTALLPATH --enable-parport --enable-remote-bitbang
gmake -j4
gmake install

unset LIBUSB0_CFLAGS
unset LIBUSB0_LIBS
unset libusb_CFLAGS
unset libusb_LDFLAGS
unset libusb_LIBS

cd ../..
