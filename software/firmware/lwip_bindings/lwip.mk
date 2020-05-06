# List of the required lwIP files.
LWIPDIR = $(CHIBIOS)/../lwip-2.1.2/src

# The various blocks of files are outlined in Filelists.mk.
include $(LWIPDIR)/Filelists.mk

LWBINDSRC = \
        $(CHIBIOS)/../lwip_bindings/arch/sys_arch.c \
	$(CHIBIOS)/os/various/evtimer.c


# Add blocks of files from Filelists.mk as required for enabled options
LWSRC = $(COREFILES) $(CORE4FILES) $(APIFILES) $(LWBINDSRC) $(NETIFFILES) $(HTTPDFILES)

LWINC = \
        $(CHIBIOS)/../lwip_bindings \
        $(LWIPDIR)/include

# Shared variables
ALLCSRC += $(LWSRC)
ALLINC  += $(LWINC) \
           $(CHIBIOS)/os/various 
