
INCDIRS = -I$(SRCDIR) -I.

GCC = gcc $(INCDIRS)
BCC = bcc
AS86 = as86
OBJCOPY=objcopy

RELEASE = `pwd | sed "s-.*/--"`
RELDATE = `date '+%d %b %Y'`
RELVERS = `pwd | sed "s-.*/--" | sed "s/vgabios//" | sed "s/-//"`

VGABIOS_DATE = "-DVGABIOS_DATE=\"$(RELDATE)\""

.PHONY: all
all: bios cirrus-bios

.PHONY: bios
bios: biossums vgabios.bin vgabios.debug.bin vgabios.elf

.PHONY: cirrus-bios
cirrus-bios: vgabios-cirrus.bin vgabios-cirrus.debug.bin

.PHONY: clean
clean:
	rm -f biossums vbetables-gen vbetables.h *.o *.s *.ld86 \
          temp.awk.* vgabios*.orig _vgabios_* _vgabios-debug_* core vgabios*.bin vgabios*.txt $(RELEASE).bin *.bak
	rm -f VGABIOS-lgpl-latest*.bin vgabios.elf


.PHONY: release
release: 
	VGABIOS_VERS=\"-DVGABIOS_VERS=\\\"$(RELVERS)\\\"\" make bios cirrus-bios
	/bin/rm -f  *.o *.s *.ld86 \
          temp.awk.* vgabios.*.orig _vgabios_.*.c core *.bak .#*
	cp VGABIOS-lgpl-latest.bin ../$(RELEASE).bin
	cp VGABIOS-lgpl-latest.debug.bin ../$(RELEASE).debug.bin
	cp VGABIOS-lgpl-latest.cirrus.bin ../$(RELEASE).cirrus.bin
	cp VGABIOS-lgpl-latest.cirrus.debug.bin ../$(RELEASE).cirrus.debug.bin
	tar czvf ../$(RELEASE).tgz --exclude CVS -C .. $(RELEASE)/

vgabios.elf: VGABIOS-lgpl-latest.bin
	cp $< vgabios.bin
	$(OBJCOPY) -I binary -O elf32-i386 vgabios.bin vgabios.elf --rename-section .data=.bios -B i386

vgabios.bin: biossums $(SRCDIR)/vgabios.c $(SRCDIR)/vgabios.h $(SRCDIR)/vgafonts.h $(SRCDIR)/vgatables.h $(SRCDIR)/vbe.h $(SRCDIR)/vbe.c vbetables.h
	$(GCC) -E -P $(SRCDIR)/vgabios.c $(VGABIOS_VERS) -DVBE $(VGABIOS_DATE) > _vgabios_.c
	$(BCC) -o vgabios.s -C-c -D__i86__ -S -0 _vgabios_.c
	sed -e 's/^\.text//' -e 's/^\.data//' vgabios.s > _vgabios_.s
	$(AS86) _vgabios_.s -b vgabios.bin -u -w- -g -0 -j -O -l vgabios.txt
	rm -f _vgabios_.s _vgabios_.c vgabios.s
	cp vgabios.bin VGABIOS-lgpl-latest.bin
	./biossums VGABIOS-lgpl-latest.bin
	ls -l VGABIOS-lgpl-latest.bin

vgabios.debug.bin: $(SRCDIR)/biossums $(SRCDIR)/vgabios.c $(SRCDIR)/vgabios.h $(SRCDIR)/vgafonts.h $(SRCDIR)/vgatables.h $(SRCDIR)/vbe.h $(SRCDIR)/vbe.c vbetables.h 
	$(GCC) -E -P $(SRCDIR)/vgabios.c $(VGABIOS_VERS) -DVBE -DDEBUG $(VGABIOS_DATE) > _vgabios-debug_.c
	$(BCC) -o vgabios-debug.s -C-c -D__i86__ -S -0 _vgabios-debug_.c
	sed -e 's/^\.text//' -e 's/^\.data//' vgabios-debug.s > _vgabios-debug_.s
	$(AS86) _vgabios-debug_.s -b vgabios.debug.bin -u -w- -g -0 -j -O -l vgabios.debug.txt
	rm -f _vgabios-debug_.s _vgabios-debug_.c vgabios-debug.s
	cp vgabios.debug.bin VGABIOS-lgpl-latest.debug.bin
	./biossums VGABIOS-lgpl-latest.debug.bin
	ls -l VGABIOS-lgpl-latest.debug.bin

vgabios-cirrus.bin: biossums $(SRCDIR)/vgabios.c $(SRCDIR)/vgabios.h $(SRCDIR)/vgafonts.h $(SRCDIR)/vgatables.h $(SRCDIR)/clext.c
	$(GCC) -E -P $(SRCDIR)/vgabios.c $(VGABIOS_VERS) -DCIRRUS $(VGABIOS_DATE) > _vgabios-cirrus_.c
	$(BCC) -o vgabios-cirrus.s -C-c -D__i86__ -S -0 _vgabios-cirrus_.c
	sed -e 's/^\.text//' -e 's/^\.data//' vgabios-cirrus.s > _vgabios-cirrus_.s
	$(AS86) _vgabios-cirrus_.s -b vgabios-cirrus.bin -u -w- -g -0 -j -O -l vgabios-cirrus.txt
	rm -f _vgabios-cirrus_.s _vgabios-cirrus_.c vgabios-cirrus.s
	cp vgabios-cirrus.bin VGABIOS-lgpl-latest.cirrus.bin
	./biossums VGABIOS-lgpl-latest.cirrus.bin
	ls -l VGABIOS-lgpl-latest.cirrus.bin

vgabios-cirrus.debug.bin: biossums $(SRCDIR)/vgabios.c $(SRCDIR)/vgabios.h $(SRCDIR)/vgafonts.h $(SRCDIR)/vgatables.h $(SRCDIR)/clext.c
	$(GCC) -E -P $(SRCDIR)/vgabios.c $(VGABIOS_VERS) -DCIRRUS -DCIRRUS_DEBUG $(VGABIOS_DATE) > _vgabios-cirrus-debug_.c
	$(BCC) -o vgabios-cirrus-debug.s -C-c -D__i86__ -S -0 _vgabios-cirrus-debug_.c
	sed -e 's/^\.text//' -e 's/^\.data//' vgabios-cirrus-debug.s > _vgabios-cirrus-debug_.s
	$(AS86) _vgabios-cirrus-debug_.s -b vgabios-cirrus.debug.bin -u -w- -g -0 -j -O -l vgabios-cirrus.debug.txt
	rm -f _vgabios-cirrus-debug_.s _vgabios-cirrus-debug_.c vgabios-cirrus-debug.s
	cp vgabios-cirrus.debug.bin VGABIOS-lgpl-latest.cirrus.debug.bin
	./biossums VGABIOS-lgpl-latest.cirrus.debug.bin
	ls -l VGABIOS-lgpl-latest.cirrus.debug.bin

biossums: $(SRCDIR)/biossums.c
	$(GCC) -o $@ $<

vbetables-gen: $(SRCDIR)/vbetables-gen.c
	$(GCC) -o vbetables-gen $(SRCDIR)/vbetables-gen.c

vbetables.h: vbetables-gen
	./vbetables-gen > $@

