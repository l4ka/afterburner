# BIOS makefile

CC=gcc
MAKE=/usr/bin/make
MKTARGET=mkbios

all: mkbios vgabios rombios ass
.PHONY : all mkbios vgabios rombios

clean:
	make -C rombios clean
	make -C vgabios clean
	rm -f mkbios

mkbios: mkbios.c
	${CC} -o ${MKTARGET} mkbios.c

vgabios: 
	${MAKE} -C ./vgabios/

rombios:
	${MAKE} -C ./rombios/

ass:
	./mkbios vgabios/VGABIOS-lgpl-latest.bin rombios/BIOS-bochs-legacy ~/boot/bios
