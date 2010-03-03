######################################################################
##                
## Copyright (C) 2003-2010,  Karlsruhe University
##                
## File path:     microdisk_earm/Makefile
## Description:   
##                
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
## 
## THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
## ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
## DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
## OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
## HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
## LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
## OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
## SUCH DAMAGE.
##                
## $Id$
##                
######################################################################
-include ../../../Makeconf.local
-include Makeconf.local

##  Afterburn related configuration
diet = ../../..//extern/dietlibc-0.32/bin-i386/diet
interfaces_dir = ../../../l4ka-interfaces

##  IDL4 configuration
idl4 ?= idl4
idl4_config ?= idl4-config
idl4_flags = -fctypes -iV4 -Wall -fno-use-malloc --mapping=c++ -I$(interfaces_dir) -I$(cfg_pistachio_dir)/include

vpath %_idl.idl $(interfaces_dir)

CFLAGS  += -O2 $(shell $(idl4_config) --cflags) -static -I$(cfg_pistachio_dir)/include 
LDFLAGS += -L$(cfg_pistachio_dir)/lib -static 
LDLIBS  += -ll4 -lgcc -lm 

%_idl_client.h: %_idl.idl
	@echo "===> Creating $@ from $(<F)"
	@echo $(Q) $(idl4) $(idl4_flags) -h $@ -c $<
	$(Q) $(idl4) $(idl4_flags) -h $@ -c $<

%_idl_server.h: %_idl.idl
	@echo "===> Creating $@ from $(<F)"
	$(Q) $(idl4) $(idl4_flags) -h $@ -s $<

%_idl_template.cc: %_idl.idl
	@echo "===> Creating $@ from $(<F)"
	$(Q) $(idl4) $(idl4_flags) -o $@ -t $<

all: microdisk_earm ebudget


microdisk_earm: microdisk_earm.c 
	$(diet) $(CC) $(CFLAGS) -O9 -o $@ $<


ebudget: ebudget.c earm_idl_client.h resourcemon_idl_client.h
	$(diet) $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(Q) rm -f *_idl_client.h *_idl_server.h
	$(Q) rm -f *.o microdisk_earm ebudget

