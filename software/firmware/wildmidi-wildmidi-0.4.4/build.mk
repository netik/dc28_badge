WILDMIDISRC+=				\
	$(WILDMIDI)/src/wm_error.c	\
	$(WILDMIDI)/src/file_io.c	\
	$(WILDMIDI)/src/lock.c		\
	$(WILDMIDI)/src/wildmidi_lib.c	\
	$(WILDMIDI)/src/reverb.c	\
	$(WILDMIDI)/src/gus_pat.c	\
	$(WILDMIDI)/src/internal_midi.c	\
	$(WILDMIDI)/src/patches.c	\
	$(WILDMIDI)/src/f_xmidi.c	\
	$(WILDMIDI)/src/f_mus.c		\
	$(WILDMIDI)/src/f_hmp.c		\
	$(WILDMIDI)/src/f_midi.c	\
	$(WILDMIDI)/src/f_hmi.c		\
	$(WILDMIDI)/src/sample.c	\
	$(WILDMIDI)/src/mus2mid.c	\
	$(WILDMIDI)/src/xmi2mid.c

WILDMIDIINC+= $(WILDMIDI)/include $(WILDMIDI)/mingw

WILDMIDIDEFS+=
USE_COPT+= $(WILDMIDIDEFS)
