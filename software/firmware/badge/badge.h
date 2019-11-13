#ifndef _BADGE_H_
#define _BADGE_H_
#include "chprintf.h"
#include "hal_fsmc.h"

#include <stdio.h>

/* Orchard command linker set handling */

#define orchard_command_start()						\
	static char start[4] __attribute__((unused,			\
	    aligned(4), section(".chibi_list_cmd_1")))

#define orchard_commands()      (const ShellCommand *)&start[4]

#define orchard_command(_name, _func)					\
	const ShellCommand _orchard_cmd_list_##_func			\
	__attribute__((used, aligned(4),				\
	     section(".chibi_list_cmd_2_" _name))) = { _name, _func }

#define orchard_command_end()						\
	const ShellCommand _orchard_cmd_list_##_func			\
	__attribute__((used, aligned(4),				\
	    section(".chibi_list_cmd_3_end"))) = { NULL, NULL }

#ifdef NEWLIB_HEAP_INTERNAL
extern char   __heap_base__; /* Set by linker */
extern char   __heap_end__; /* Set by linker */

#define HEAP_BASE &__heap_base__
#define HEAP_END &__heap_end__
#endif

#ifdef NEWLIB_HEAP_SDRAM
/*
 * We reserve the top of RAM for the graphics frame buffer.
 * The size is 480 * 272 * 2 bytes per pixel.
 */
#define FB_SIZE 0x3FC00
#define HEAP_BASE ((char *)FSMC_Bank5_MAP_BASE + FB_SIZE)
#define HEAP_END ((char *)(FSMC_Bank5_MAP_BASE + 0x7FFFFF))
#endif

extern void badge_sleep_enable (void);
extern void badge_sleep_disable (void);

extern BaseSequentialStream * console;
extern thread_reference_t shell_ref_usb;

extern void newlibStart (void);

#endif /* _BADGE_H_ */
