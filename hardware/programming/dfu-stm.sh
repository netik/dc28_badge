#!/bin/sh
#
# The STM32F parts include an internal bootloader ROM which can be used
# to flash the device using various different I/O interfaces using the
# Device Firmware Update (DFU) protocol. The STM32F746 support DFU via
# USB, and we can use the dfu-util with that on a number of different
# OSes.
#
# The DFU firmware can be activated by resetting the CPU while holding
# the boot0 pin gounded. Alternatively, you can enter it manually by
# jumping the CPU to the DFU entry point. Per application note AN2606,
# the start address of the DFU firmware for the STM32F746 is 0x1ff00000.
# To force it to start with GDB/OpenOCD, you can use the following
# commands
#
# 1) Examine address 0x1ff00000 to see the initial stack pointer
#    and program counter values for entering the DFU ROM:
#
# (gdb) x 0x1ff00000
# 0x1ff00000:       0x20003af8
# (gdb) 
# 0x1ff00004:       0x1ff00c89
# (gdb) 
#
# 2) Force a CPU reset
#
# (gdb) monitor reset init
# xPSR: 0x01000000 pc: 0x00200320 msp: 0x20000400
# (gdb)
#
# 3) Load the SP and PC values into the $sp and $pc registers and run
#
# (gdb) set $sp=0x20003af8
# (gdb) set $pc=0x1ff00c89
# (gdb) c
# Continuing.
#
# Once the DFU firmware is running, plug in a USB cable to the USB-F port.
# The below command will load a binary image at the start of the internal
# flash bank. The flash is at altsetting 0.
#

dfu-util -l
dfu-util -a 0 -R -s 0x08000000 -D build/badge.bin
