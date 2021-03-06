##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#

# Compiler options here.
ifeq ($(USE_OPT),)
  USE_OPT = -Os -ggdb -fomit-frame-pointer -falign-functions=16
endif

# Build image to boot from RAM (yes) for from flash (no)
ifeq ($(BOOT_FROM_RAM),)
  BOOT_FROM_RAM = no
endif

# Turning off the CRT0_INIT_DATA macro is necessary because the trick
# that ChibiOS uses to copy initialized data from the flash into
# SRAM doesn't work when we run from SDRAM. It happens that U-Boot
# will correctly load the initialized data for us though because it
# can see it as a loadable segment when we boot using an ELF image.
ifeq ($(BOOT_FROM_RAM),yes)
  USE_OPT += -DBOOT_FROM_RAM=1 -DCRT0_INIT_DATA=FALSE
endif

# C specific options here (added to USE_OPT).
ifeq ($(USE_COPT),)
  USE_COPT = -DHAVE_MORECORE=1 -DHAVE_MMAP=0 -DUSE_LOCKS=2 -DUSE_DL_PREFIX
endif

# C++ specific options here (added to USE_OPT).
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif

# Enable this if you want the linker to remove unused code and data.
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Linker extra options here.
ifeq ($(USE_LDOPT),)
  USE_LDOPT = 
endif

# Enable this if you want link time optimizations (LTO).
ifeq ($(USE_LTO),)
  USE_LTO = yes
endif

# Enable this if you want to see the full log while compiling.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

# If enabled, this option makes the build process faster by not compiling
# modules not used in the current configuration.
ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

#
# Build global options
##############################################################################

##############################################################################
# Architecture or project specific options
#

# Stack size to be allocated to the Cortex-M process stack. This stack is
# the stack used by the main() thread.
ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x400
endif

# Stack size to the allocated to the Cortex-M main/exceptions stack. This
# stack is used for processing interrupts and exceptions.
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x400
endif

# Enables the use of FPU (no, softfp, hard).
ifeq ($(USE_FPU),)
  USE_FPU = hard
endif

# FPU-related options.
ifeq ($(USE_FPU_OPT),)
  USE_FPU_OPT = -mfloat-abi=$(USE_FPU) -mfpu=fpv5-sp-d16
endif

#
# Architecture or project specific options
##############################################################################

##############################################################################
# Project, target, sources and paths
#

# Define project name here
PROJECT = badge

# Target settings.
MCU  = cortex-m7

# Imported source files and paths.
CHIBIOS  := ../ChibiOS
CHIBIOS_CONTRIB := $(CHIBIOS)/community
CONFDIR  := ./cfg
BUILDDIR := ./build
DEPDIR   := ./.dep

# Licensing files.
include $(CHIBIOS)/os/license/license.mk
# Startup files.
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f7xx.mk
# HAL-OSAL files (optional).
include $(CHIBIOS_CONTRIB)/os/hal/hal.mk
include $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/STM32F7xx/platform.mk
include $(CHIBIOS)/os/hal/boards/ST_STM32F746G_DISCOVERY/board.mk
include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk
# RTOS files (optional).
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC/mk/port_v7m.mk
# Auto-build files in ./source recursively.
include $(CHIBIOS)/tools/mk/autobuild.mk
# Other files (optional).
include $(CHIBIOS)/os/hal/lib/streams/streams.mk
include $(CHIBIOS)/os/various/shell/shell.mk

# uGFX includes
GFXLIB = $(CHIBIOS)/../uGFX
include $(GFXLIB)/gfx.mk
include $(GFXLIB)/boards/base/Nonstandard-Ides-Of-Defcon/board.mk

# FatFS library
FATFS = $(CHIBIOS)/../FatFs
include $(FATFS)/build.mk

# WM8994 Codec driver
WM8994 = $(CHIBIOS)/../wm8994
include $(WM8994)/build.mk

# libjpeg library
LIBJPEG = $(CHIBIOS)/../jpeg-9d
include $(LIBJPEG)/build.mk

# lwIP TCP/IP stack
include $(CHIBIOS)/../lwip_bindings/lwip.mk

# NES emulator
NES = $(CHIBIOS)/../nofrendo-2.0pre1
include $(NES)/build.mk

# WildMIDI library
WILDMIDI = $(CHIBIOS)/../wildmidi-wildmidi-0.4.4
include $(WILDMIDI)/build.mk

# Doom
DOOM = $(CHIBIOS)/../linuxdoom-1.10
include $(DOOM)/build.mk

# Define linker script file here
ifeq ($(BOOT_FROM_RAM),yes)
LDSCRIPT= STM32F746xG.ld.SDRAM
else
LDSCRIPT= STM32F746xG.ld
endif

# C sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CSRC = $(ALLCSRC) \
       $(FATFSSRC) \
       $(GFXSRC) \
       $(STMSRC) \
       $(TJPGDSRC) \
       $(WM8994SRC) \
       $(LIBJPEGSRC) \
       $(NESSRC) \
       $(DOOMSRC) \
       $(WILDMIDISRC) \
       main.c \
       usbcfg.c \
       cmd-adc.c \
       cmd-audio.c \
       cmd-capture.c \
       cmd-cpu.c \
       cmd-mem.c \
       cmd-radio.c \
       cmd-reset.c \
       cmd-video.c \
       cmd-scroll.c \
       cmd-sleep.c \
       cmd-ping.c \
       cmd-unix.c \
       cmd-usb.c \
       crc32.c \
       newlib_syscall.c \
       nullprot_lld.c \
       usbhid_lld.c \
       badge_console.c \
       badge_finder.c \
       badge_vectors.c \
       async_io_lld.c \
       video_lld.c \
       stm32adc_lld.c \
       stm32flash_lld.c \
       stm32sai_lld.c \
       sddetect_lld.c \
       sx1262_lld.c \
       touch_lld.c \
       scroll_lld.c \
       malloc.c \
       orchard-app.c \
       orchard-ui.c \
       fix_fft.c \
       ides_gfx.c \
       app-badge.c \
       app-dialer.c \
       app-doomguy.c \
       app-doom.c \
       app-email.c \
       app-info.c \
       app-intro.c \
       app-launcher.c \
       app-ledsign.c \
       app-music.c \
       app-name.c \
       app-nes.c \
       app-netdoom.c \
       app-notify.c \
       app-setup.c \
       app-shout.c \
       app-update.c \
       app-video.c \
       userconfig.c \
       ui-keyboard.c \
       ui-list.c

# C++ sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CPPSRC = $(ALLCPPSRC)

# List ASM source files here.
ASMSRC = $(ALLASMSRC)

# List ASM with preprocessor source files here.
ASMXSRC = $(ALLXASMSRC) badge_fault.S

# Inclusion directories.
INCDIR = $(CONFDIR) $(ALLINC) $(CHIBIOS_CONTRIB)/os/various $(FATFSINC)
INCDIR += $(GFXINC) $(STMINC) $(TJPGDINC) $(WM8994INC) $(LIBJPEGINC)
INCDIR += $(NESINC) $(DOOMINC) $(WILDMIDIINC)

# Define C warning options here.
CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes

# Define C++ warning options here.
CPPWARN = -Wall -Wextra -Wundef

#
# Project, target, sources and paths
##############################################################################

##############################################################################
# Start of user section
#

# List all user C define here, like -D_DEBUG=1
UDEFS = -DSHELL_PROMPT_STR="\"badge> \""

# Define ASM defines here
UADEFS =

# List all user directories here
UINCDIR =

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS = -lc_nano -lnosys -lm
# This enables the use of the versions of printf() and scanf()
# in newlib that support floating point
ULIBS += -u _printf_float -u _scanf_float

#
# End of user section
##############################################################################

##############################################################################
# Common rules
#

RULESPATH = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk
include $(RULESPATH)/arm-none-eabi.mk
include $(RULESPATH)/rules.mk

#
# Common rules
##############################################################################

##############################################################################
# Custom rules
#

.PHONY: updatebuildtime
PRE_MAKE_ALL_RULE_HOOK: buildtime.h

buildtime.h: updatebuildtime
	echo "#define BUILDTIME \"Built: `/bin/date`\"" > $@
	echo "#define BUILDVER `/bin/date +%s`" >> $@
	echo "#define BUILDMAGIC 0xCAFEBABE" >> $@

#
# Custom rules
##############################################################################
