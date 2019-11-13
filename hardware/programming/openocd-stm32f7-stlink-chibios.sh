#!/bin/sh

# This script is used to debug/program an STM32F746G Discovery board using
# the on-board ST Link debugger.
#
# Note that you need to set your PATH environment variable to include the
# location where your openocd binary resides before running this script.

openocd -f board/stm32f7discovery.cfg			\
	-c "stm32f7x.cpu configure -rtos ChibiOS"	\
	-c "gdb_flash_program enable"			\
	-c "gdb_breakpoint_override hard"	"$@"
