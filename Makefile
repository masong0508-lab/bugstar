TC ?= arm-none-eabi-
CFLAGS := -mcpu=arm7tdmi -mthumb -mthumb-interwork -O2 -Wall -ffreestanding -fno-strict-aliasing
all: bugstar.gba
assets.h: gen.py texgen.py kartgen.py bignums.py facts.py monsters.py
	python3 gen.py
bugstar.elf: main.c game.inc save.inc ui.inc sound.inc dungeon.inc terrace.inc space.inc planet.inc town.inc ship.inc hangar.inc forge.inc kart.inc kart2.inc kart3.inc sizes.inc deep.inc extras.inc crt0.s gba.ld assets.h logo.c logo.h
	$(TC)gcc $(CFLAGS) -nostdlib -nostartfiles -T gba.ld crt0.s main.c logo.c -lgcc -o $@
bugstar.gba: bugstar.elf
	$(TC)objcopy -O binary $< $@
	gbafix $@ -tBUGSTAR -cBUGS -mCL
clean:
	rm -f bugstar.elf bugstar.gba
