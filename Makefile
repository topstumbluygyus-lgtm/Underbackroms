TARGET = SunsetSoul
OBJS   = main.o

CFLAGS   = -O2 -G0 -Wall -std=gnu99
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)

LIBS = -lpspdebug -lpspdisplay -lpspge -lpspctrl -lm

BUILD_PRX = 1
PSP_FW_VERSION = 660

EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = Sunset Soul

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
