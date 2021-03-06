DOOMSRC+=				\
		$(DOOM)/doomdef.o	\
		$(DOOM)/doomstat.o	\
		$(DOOM)/dstrings.o	\
		$(DOOM)/i_main_badge.o	\
		$(DOOM)/i_sound_badge.o	\
		$(DOOM)/i_video_badge.o	\
		$(DOOM)/i_system_badge.o\
		$(DOOM)/i_net_badge.o	\
		$(DOOM)/tables.o	\
		$(DOOM)/f_finale.o	\
		$(DOOM)/f_wipe.o 	\
		$(DOOM)/d_main.o	\
		$(DOOM)/d_net.o		\
		$(DOOM)/d_items.o	\
		$(DOOM)/g_game.o	\
		$(DOOM)/m_menu.o	\
		$(DOOM)/m_misc.o	\
		$(DOOM)/m_argv.o  	\
		$(DOOM)/m_bbox.o	\
		$(DOOM)/m_fixed.o	\
		$(DOOM)/m_swap.o	\
		$(DOOM)/m_cheat.o	\
		$(DOOM)/m_random.o	\
		$(DOOM)/am_map.o	\
		$(DOOM)/p_ceilng.o	\
		$(DOOM)/p_doors.o	\
		$(DOOM)/p_enemy.o	\
		$(DOOM)/p_floor.o	\
		$(DOOM)/p_inter.o	\
		$(DOOM)/p_lights.o	\
		$(DOOM)/p_map.o		\
		$(DOOM)/p_maputl.o	\
		$(DOOM)/p_plats.o	\
		$(DOOM)/p_pspr.o	\
		$(DOOM)/p_setup.o	\
		$(DOOM)/p_sight.o	\
		$(DOOM)/p_spec.o	\
		$(DOOM)/p_switch.o	\
		$(DOOM)/p_mobj.o	\
		$(DOOM)/p_telept.o	\
		$(DOOM)/p_tick.o	\
		$(DOOM)/p_saveg.o	\
		$(DOOM)/p_user.o	\
		$(DOOM)/r_bsp.o		\
		$(DOOM)/r_data.o	\
		$(DOOM)/r_draw.o	\
		$(DOOM)/r_main.o	\
		$(DOOM)/r_plane.o	\
		$(DOOM)/r_segs.o	\
		$(DOOM)/r_sky.o		\
		$(DOOM)/r_things.o	\
		$(DOOM)/w_wad.o		\
		$(DOOM)/wi_stuff.o	\
		$(DOOM)/v_video.o	\
		$(DOOM)/st_lib.o	\
		$(DOOM)/st_stuff.o	\
		$(DOOM)/hu_stuff.o	\
		$(DOOM)/hu_lib.o	\
		$(DOOM)/s_sound.o	\
		$(DOOM)/z_zone.o	\
		$(DOOM)/info.o		\
		$(DOOM)/sounds.o

DOOMINC+= $(DOOM)

DOOMDEFS+= -DNORMALUNIX -DBADGEDOOM
USE_COPT+= $(DOOMDEFS) -Drcsid="volatile rcsid" -fsigned-char
ifeq ($(USE_LTO),yes)
  USE_OPT += -Wno-lto-type-mismatch
endif
