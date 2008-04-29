# Copyright (C) 2001  MandrakeSoft S.A.
#
#   MandrakeSoft S.A.
#   43, rue d'Aboukir
#   75002 Paris - France
#   http://www.linux-mandrake.com/
#   http://www.mandrakesoft.com/
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

# Makefile for the BIOS component of bochs


.SUFFIXES: .cc

#SRCDIR = /home/stoess/devel/afterburner-hvm/afterburn-wedge


SHELL = /bin/sh



CXX = g++
CXXFLAGS = -g -O2 -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES

LDFLAGS = 
LIBS =  -lm
RANLIB = ranlib

BCC = bcc
GCC = gcc
AS86 = as86

INCDIRS = -I$(SRCDIR)
LOCAL_CXXFLAGS =

BUILDDATE = `date '+%m/%d/%y'`
BIOS_BUILD_DATE = "-DBIOS_BUILD_DATE=\"$(BUILDDATE)\""

OBJCOPY=objcopy

#
# -------- end configurable options --------------------------
#


.cc.o:
	$(CXX) -c $(INCDIRS) $(CXXFLAGS) $(LOCAL_CXXFLAGS) $< -o $@



bios: biossums BIOS-bochs-latest BIOS-bochs-legacy rombios.elf

rombios.elf: BIOS-bochs-legacy
	cp $< rombios.bin
	$(OBJCOPY) -I binary -O elf32-i386 rombios.bin rombios.elf --rename-section .data=.bios -B i386

clean:
	rm -f  *.o *.a *.s _rombios*_.c rombios*.txt rombios*.sym
	rm -f  usage biossums rombios16.bin
	rm -f  rombios32.bin rombios32.out rombios.elf

dist-clean: clean
	rm -f  Makefile

bios-clean:
	rm -f  BIOS-bochs-*

BIOS-bochs-legacy: $(SRCDIR)/rombios.c $(SRCDIR)/apmbios.S biossums $(SRCDIR)/rombios.h
	$(GCC) $(BIOS_BUILD_DATE) -DLEGACY -E -P $< > _rombiosl_.c
	$(BCC) -o rombiosl.s -C-c -D__i86__ -0 -S _rombiosl_.c
	sed -e 's/^\.text//' -e 's/^\.data//' rombiosl.s > _rombiosl_.s
	$(AS86) _rombiosl_.s -b tmp.bin -u- -w- -g -0 -j -O -l rombiosl.txt
	-perl ${SRCDIR}/makesym.perl < rombiosl.txt > rombiosl.sym
	mv tmp.bin $@
	./biossums $@
	rm -f  _rombiosl_.s


rombios16.bin: $(SRCDIR)/rombios.c $(SRCDIR)/apmbios.S biossums $(SRCDIR)/rombios.h
	$(GCC) $(BIOS_BUILD_DATE) -E -P $< > _rombios_.c
	$(BCC) -o rombios.s -C-c -D__i86__ -0 -S _rombios_.c
	sed -e 's/^\.text//' -e 's/^\.data//' rombios.s > _rombios_.s
	$(AS86) _rombios_.s -b tmp.bin -u- -w- -g -0 -j -O -l rombios.txt
	-perl ${SRCDIR}/makesym.perl < rombios.txt > rombios.sym
	mv tmp.bin rombios16.bin
	./biossums rombios16.bin
	rm -f  _rombios_.s


rombios32.bin: rombios32.out $(SRCDIR)/rombios.h
	objcopy -O binary $< $@
	./biossums -pad $@

rombios32.out: rombios32start.o rombios32.o $(SRCDIR)/rombios32.ld
	ld -o $@ -T $(SRCDIR)/rombios32.ld rombios32start.o rombios32.o

rombios32.o: $(SRCDIR)/rombios32.c $(SRCDIR)/acpi-dsdt.hex
	$(GCC) -O2 -Wall -c -o $@ $<

ifeq ("1", "0")
acpi-dsdt.hex: acpi-dsdt.dsl
	iasl -tc -p $@ $<
endif

rombios32start.o: $(SRCDIR)/rombios32start.S
	$(GCC) -c -o $@ $<

BIOS-bochs-latest: rombios16.bin rombios32.bin
	cat rombios32.bin rombios16.bin > $@

biossums: $(SRCDIR)/biossums.c
	$(GCC) -o $@ $<
