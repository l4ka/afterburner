######################################################################
##                
## Copyright (C) 2003-2010,  Karlsruhe University
##                
## File path:     perfmon/Makefile
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

######################################################################
##  Configuration
######################################################################

-include Makeconf.local

O ?= .
PERFMON_INCLUDE ?= ../../../hypervisor/include/ia32
EXTRA_CFLAGS ?= -mno-tls-direct-seg-refs

######################################################################
##  Top level build
######################################################################

targets = $(O)/perfmon $(O)/perfsweep

all: $(targets)

######################################################################
##  Build rules
######################################################################

CFLAGS += $(EXTRA_CFLAGS) -Wall -O2
CFLAGS += -I$(PERFMON_INCLUDE)

LDFLAGS += $(EXTRA_CFLAGS)

$(O)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(O)/perfmon.o: perfmon.c $(PERFMON_INCLUDE)/perfmon.h
$(O)/perfsweep.o: perfsweep.c $(PERFMON_INCLUDE)/perfmon.h

$(O)/perfmon: $(O)/perfmon.o
$(O)/perfsweep: $(O)/perfsweep.o

clean:
	rm -f $(targets) $(O)/*.o

