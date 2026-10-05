# Needs devkitARM (arm-none-eabi-gcc + gbafix). Paths are set below so PATH does not matter.
DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
export PATH := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

TARGET  := bore
CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
ARCH    := -mthumb -mthumb-interwork
CFLAGS  := $(ARCH) -O2 -Wall -fno-strict-aliasing -ffunction-sections
LDFLAGS := -specs=gba.specs $(ARCH)

all: $(TARGET).gba

$(TARGET).elf: source/main.c $(wildcard source/*.h) $(wildcard source/sfx/*.adp) $(wildcard source/music/*.adp)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	gbafix $@ -tBORE

clean:
	rm -f $(TARGET).elf $(TARGET).gba
.PHONY: all clean
