# BIOS makefile

CC=gcc
MAKE=/usr/bin/make
MKTARGET=mkbios
OBJCOPY=objcopy
VGABIOS=./vgabios/VGABIOS-lgpl-latest.bin
ROMBIOS=./rombios/BIOS-bochs-legacy

all: mkbios vgabios rombios dummyelf
.PHONY : all mkbios vgabios rombios

clean:
	make -C rombios clean
	make -C vgabios clean
	rm -f *.elf

mkbios: mkbios.c
	${CC} -o ${MKTARGET} mkbios.c

vgabios: 
	${MAKE} -C ./vgabios/

rombios:
	${MAKE} -C ./rombios/

dummyelf:
	$(OBJCOPY) -I binary -O elf32-i386 $(ROMBIOS) rombios.elf --rename-section .data=.bios -B i386
	$(OBJCOPY) -I binary -O elf32-i386 $(VGABIOS) vgabios.elf --rename-section .data=.bios -B i386
