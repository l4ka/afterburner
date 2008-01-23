# BIOS makefile

CC=gcc
MAKE=/usr/bin/make
MKTARGET=mkbios
OBJCOPY=objcopy
VGABIOS=./vgabios/VGABIOS-lgpl-latest.bin
ROMBIOS=./rombios/BIOS-bochs-legacy

all: vgabios rombios dummyelf
.PHONY : all vgabios rombios

clean:
	make -C rombios clean
	make -C vgabios clean
	rm -f *.elf *.bin 

vgabios: 
	${MAKE} -C ./vgabios/

rombios:
	${MAKE} -C ./rombios/

dummyelf:
	cp $(ROMBIOS) ./rombios.bin
	cp $(VGABIOS) ./vgabios.bin
	$(OBJCOPY) -I binary -O elf32-i386 rombios.bin rombios.elf --rename-section .data=.bios -B i386
	$(OBJCOPY) -I binary -O elf32-i386 vgabios.bin vgabios.elf --rename-section .data=.bios -B i386
