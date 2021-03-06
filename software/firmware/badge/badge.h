#ifndef _BADGE_H_
#define _BADGE_H_
#include "chprintf.h"
#include "hal_fsmc.h"

#include <stdio.h>

#define NEWLIB_HEAP_SDRAM

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
 * We reserve the top of RAM for the graphics frame buffers.
 * The size is 320 * 240 * 2 bytes per pixel, and we use two
 * layers, so the total is 307200 bytes. We round this up to
 * 327680 bytes to turn it into two blocks of power of two
 * size, to make it easier to mark the frame buffer memory
 * as uncached with the MPU.
 */
#define FB_SIZE 0x50000
#define FB_BASE0 0xC07B0000
#define FB_BASE1 0xC07D6000
#define FB_BASE FB_BASE0
#define EXTRA_BSS 0x54000
#ifdef BOOT_FROM_RAM
#define KERNEL_MEM	0x100000
#else
#define KERNEL_MEM	0x0
#endif
#define HEAP_BASE ((char *)(FSMC_Bank5_MAP_BASE + EXTRA_BSS + KERNEL_MEM))
#define HEAP_END (HEAP_BASE + (0x7AFFFF - EXTRA_BSS - KERNEL_MEM))
#endif

/*
 * Structure defining the STM32 96-bit manufacturer
 * info in ROM. This can be used as a unique chip ID.
 */

typedef struct stm32_id {
	uint16_t	stm32_wafer_x;
	uint16_t	stm32_wafer_y;
	uint8_t		stm32_wafernum;
	uint8_t		stm32_lotnum[7];
} STM32_ID;

#define BADGE_ADDR_LEN	6

/*
 * Make up a fake vendor OUI (00:1D:E5) ("IDES"). This isn't really ethernet
 * so it doesn't matter if this overlaps with a real ethernet OUI.
 */

#define BADGE_OUI_0	0x00
#define BADGE_OUI_1	0x1D
#define BADGE_OUI_2	0xE5

extern uint8_t badge_addr[BADGE_ADDR_LEN];

#define BADGE_CPU_SPEED_SLOW	54
#define BADGE_CPU_SPEED_MEDIUM	108
#define BADGE_CPU_SPEED_NORMAL	216

extern void badge_sleep_enable (void);
extern void badge_sleep_disable (void);
extern uint8_t badge_lpidle_get (void);
extern void badge_lpidle_enable (void);
extern void badge_lpidle_disable (void);
extern void badge_lpidle_suspend (void);
extern void badge_lpidle_resume (void);
extern void badge_deepsleep_init (void);
extern void badge_deepsleep_enable (void);
extern void badge_deepsleep_timed (uint16_t);
extern void badge_cpu_show (void);
extern uint32_t badge_cpu_speed_get (void);
extern void badge_cpu_dcache (bool);
extern void badge_cpu_icache (bool);
extern void badge_cpu_speed_set (int);
extern void badge_delay (uint16_t);

extern BaseSequentialStream * conin;
extern BaseSequentialStream * conout;
extern mutex_t conmutex;
extern thread_reference_t shell_ref_usb;

/* Variables for networked Doom support */

extern char * net_doom_peer;
extern char * net_doom_node;
extern bool net_doom_deathmatch;
extern uint8_t net_doom_skill;
extern uint8_t net_doom_episode;
extern uint8_t net_doom_map;
extern uint32_t net_doom_freq;

extern void newlibStart (void);

#endif /* _BADGE_H_ */
