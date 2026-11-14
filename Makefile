TOOLCHAIN ?= /home/kryptik/handhelds/sf3000-work/buildroot/output/host
CROSS     := $(TOOLCHAIN)/bin/mips-mti-linux-gnu-
SYSROOT   ?= /home/kryptik/handhelds/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/mipsel-buildroot-linux-gnu/sysroot
CC        := $(CROSS)gcc
STRIP     := $(CROSS)strip

CFLAGS := -mips32r2 -march=mips32r2 -mtune=24kc -mfp32 -mhard-float -mlong-calls \
          -EL --sysroot=$(SYSROOT) -G0 -Os -Wall -Wextra -std=gnu99 \
          -I$(SYSROOT)/usr/include -I$(SYSROOT)/usr/include/freetype2 -Isrc
LDFLAGS := -mips32r2 -mhard-float -mfp32 -EL --sysroot=$(SYSROOT) \
           -L$(SYSROOT)/usr/lib -L$(SYSROOT)/lib

TARGET := froggythemes.mipsel
SHARED := src/hwdisp.c src/joykey.c

.PHONY: all clean
all: $(TARGET)

$(TARGET): src/froggythemes.c $(SHARED) src/hwdisp.h src/joykey.h src/tfenv.h
	$(CC) $(CFLAGS) src/froggythemes.c $(SHARED) $(LDFLAGS) \
		-Wl,--gc-sections -lfreetype -ljpeg -lpng16 -ldl -lm -o $@
	$(STRIP) $@

clean:
	rm -f $(TARGET)
