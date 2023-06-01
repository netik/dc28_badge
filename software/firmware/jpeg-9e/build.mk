LIBJPEGSRC+=				\
	$(LIBJPEG)/jcomapi.c		\
	$(LIBJPEG)/jaricom.c		\
	$(LIBJPEG)/jdapimin.c		\
	$(LIBJPEG)/jdapistd.c		\
	$(LIBJPEG)/jdarith.c		\
	$(LIBJPEG)/jdatadst.c		\
	$(LIBJPEG)/jdatasrc.c		\
	$(LIBJPEG)/jdcoefct.c		\
	$(LIBJPEG)/jdcolor.c		\
	$(LIBJPEG)/jddctmgr.c		\
	$(LIBJPEG)/jdhuff.c		\
	$(LIBJPEG)/jdinput.c		\
	$(LIBJPEG)/jdmainct.c		\
	$(LIBJPEG)/jdmarker.c		\
	$(LIBJPEG)/jdmaster.c		\
	$(LIBJPEG)/jdmerge.c		\
	$(LIBJPEG)/jdpostct.c		\
	$(LIBJPEG)/jdsample.c		\
	$(LIBJPEG)/jdtrans.c		\
	$(LIBJPEG)/jerror.c		\
	$(LIBJPEG)/jfdctflt.c		\
	$(LIBJPEG)/jfdctfst.c		\
	$(LIBJPEG)/jfdctint.c		\
	$(LIBJPEG)/jidctflt.c		\
	$(LIBJPEG)/jidctfst.c		\
	$(LIBJPEG)/jidctint.c		\
	$(LIBJPEG)/jquant1.c		\
	$(LIBJPEG)/jquant2.c		\
	$(LIBJPEG)/jutils.c		\
	$(LIBJPEG)/jmemmgr.c		\
	$(LIBJPEG)/jmemchibios.c

LIBJPEGINC+= $(LIBJPEG)

LIBJPEGDEFS+=
USE_COPT+= $(LIBJPEGDEFS)
