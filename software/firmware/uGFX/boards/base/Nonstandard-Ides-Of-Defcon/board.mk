include $(GFXLIB)/drivers/gdisp/STM32FB/driver.mk
include $(GFXLIB)/drivers/ginput/touch/FT5336/driver.mk

GFXINC  += $(GFXLIB)/boards/base/Nonstandard-Ides-Of-Defcon
GFXSRC  +=
GFXDEFS += -DGFX_USE_CHIBIOS=TRUE
