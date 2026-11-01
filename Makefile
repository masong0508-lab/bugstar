TC ?= arm-none-eabi-
CFLAGS := -mcpu=arm7tdmi -mthumb -mthumb-interwork -O2 -Wall -ffreestanding -fno-strict-aliasing
all: bugstar.gba
assets.h: gen.py texgen.py kartgen.py bignums.py facts.py facts_more.py monsters.py
	python3 gen.py
bugstar.elf: main.c game.inc save.inc ui.inc sound.inc rumble.inc dungeon.inc terrace.inc space.inc planet.inc town.inc ship.inc hangar.inc forge.inc ore.inc kart.inc kart2.inc kart3.inc sizes.inc deep.inc extras.inc coil.inc coil2.inc coil3.inc coilnum.inc crt0.s gba.ld assets.h logo.c logo.h
	$(TC)gcc $(CFLAGS) -nostdlib -nostartfiles -T gba.ld crt0.s main.c logo.c -lgcc -o $@
bugstar.gba: bugstar.elf
	@python3 -c "import subprocess,sys; o=subprocess.run(['$(TC)nm','$<'],capture_output=True,text=True).stdout; b=[int(x.split()[0],16) for x in o.splitlines() if x.endswith(' __bss_end')][0]; f=0x03007E80-b; print('IWRAM: %d bytes left for the stack (want >1500)'%f); print('WARNING: stack may hit the data' if f<1500 else 'ok')"
	$(TC)objcopy -O binary $< $@
	gbafix $@ -tBUGSTAR -cV49E -mCL
clean:
	rm -f bugstar.elf bugstar.gba
