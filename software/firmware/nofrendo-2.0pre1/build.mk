NESSRC+=				\
	$(NES)/src/config.o		\
	$(NES)/src/event.o		\
	$(NES)/src/gui.o		\
	$(NES)/src/gui_elem.o		\
	$(NES)/src/intro.o		\
	$(NES)/src/log.o		\
	$(NES)/src/pcx.o		\
	$(NES)/src/nes_bitmap.o		\
	$(NES)/src/nofrendo.o		\
	$(NES)/src/vid_drv.o		\
	$(NES)/src/cpu/dis6502.o	\
	$(NES)/src/cpu/nes6502.o	\
	$(NES)/src/libsnss/libsnss.o	\
	$(NES)/src/mappers/map000.o	\
	$(NES)/src/mappers/map001.o	\
	$(NES)/src/mappers/map002.o	\
	$(NES)/src/mappers/map003.o	\
	$(NES)/src/mappers/map004.o	\
	$(NES)/src/mappers/map005.o	\
	$(NES)/src/mappers/map007.o	\
	$(NES)/src/mappers/map008.o	\
	$(NES)/src/mappers/map009.o	\
	$(NES)/src/mappers/map011.o	\
	$(NES)/src/mappers/map015.o	\
	$(NES)/src/mappers/map016.o	\
	$(NES)/src/mappers/map018.o	\
	$(NES)/src/mappers/map019.o	\
	$(NES)/src/mappers/map024.o	\
	$(NES)/src/mappers/map032.o	\
	$(NES)/src/mappers/map033.o	\
	$(NES)/src/mappers/map034.o	\
	$(NES)/src/mappers/map040.o	\
	$(NES)/src/mappers/map041.o	\
	$(NES)/src/mappers/map042.o	\
	$(NES)/src/mappers/map046.o	\
	$(NES)/src/mappers/map050.o	\
	$(NES)/src/mappers/map064.o	\
	$(NES)/src/mappers/map065.o	\
	$(NES)/src/mappers/map066.o	\
	$(NES)/src/mappers/map070.o	\
	$(NES)/src/mappers/map073.o	\
	$(NES)/src/mappers/map075.o	\
	$(NES)/src/mappers/map078.o	\
	$(NES)/src/mappers/map079.o	\
	$(NES)/src/mappers/map085.o	\
	$(NES)/src/mappers/map087.o	\
	$(NES)/src/mappers/map093.o	\
	$(NES)/src/mappers/map094.o	\
	$(NES)/src/mappers/map099.o	\
	$(NES)/src/mappers/map160.o	\
	$(NES)/src/mappers/map229.o	\
	$(NES)/src/mappers/map231.o	\
	$(NES)/src/mappers/mapvrc.o	\
	$(NES)/src/nes/mmclist.o	\
	$(NES)/src/nes/nes.o		\
	$(NES)/src/nes/nes_mmc.o	\
	$(NES)/src/nes/nes_pal.o	\
	$(NES)/src/nes/nes_ppu.o	\
	$(NES)/src/nes/nes_rom.o	\
	$(NES)/src/nes/nesinput.o	\
	$(NES)/src/nes/nesstate.o	\
	$(NES)/src/sndhrdw/nes_apu.o	\
	$(NES)/src/sndhrdw/fds_snd.o	\
	$(NES)/src/sndhrdw/mmc5_snd.o	\
	$(NES)/src/sndhrdw/vrcvisnd.o	\
	$(NES)/src/unix/osd.o		\
	$(NES)/src/badge/badge_nes.c

NESINC+= $(NES)/src $(NES)/src/nes $(NES)/src/sndhrdw	\
	 $(NES)/src/libsnss $(NES)/src/cpu

NESDEFS+= -DNDEBUG -DHAVE_CONFIG_H
USE_COPT+= $(NESDEFS)
