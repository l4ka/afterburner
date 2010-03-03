######################################################################
##                
## Copyright (C) 2003-2010,  Karlsruhe University
##                
## File path:     Makefile
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
CFLAGS += -Wall -O2
CFLAGS += -I. -I$(L4KA_PISTACHIO_USER)/include -I$(L4KA_INTERFACE_DIR)
CFLAGS += $(shell $(IDL4_CONFIG) --cflags)
CFLAGS += -L$(L4KA_PISTACHIO_USER)/lib/l4 
LIBS   += -ll4

INCLUDE  += -I $(L4KA_PISTACHIO_USER)/include -I$(L4KA_INTERFACE_DIR) -I $(BUILDDIR)

IDL4 ?= idl4
IDL4_CONFIG ?= idl4-config

#  NOTE: use idl4's --pre-call= and --post-call= parameters to ensure that
#  particular functions are invoked prior to and after each IPC.

IDL4_FLAGS = 	-fctypes -Wall -fno-use-malloc \
		--interface=v4 --mapping=c++ $(IDL4_FLAGS_$(ARCH)) \
		$(addprefix -I, $(INCLUDE))

$(BUILDDIR)/%_idl_server.h: $(L4KA_INTERFACE_DIR)/%_idl.idl 
	mkdir -p $(BUILDDIR)/include
	$(IDL4) $(IDL4_FLAGS) -h $@ -s $<

$(BUILDDIR)/%_idl_client.h: $(L4KA_INTERFACE_DIR)/%_idl.idl 
	mkdir -p $(BUILDDIR)/include
	$(IDL4) $(IDL4_FLAGS) -h $@ -c $<


#
#  Determine our IDL source files from the $(IDL_SOURCES) variable created
#  by including all the Makeconf files from the source directories.
#
IDL_SOURCES += resourcemon_idl.idl


IDL_SERVER_OUTPUT = $(addprefix $(BUILDDIR)/, \
	$(patsubst %_idl.idl, %_idl_server.h, $(IDL_SOURCES)))

IDL_CLIENT_OUTPUT = $(addprefix $(BUILDDIR)/, \
	$(patsubst %_idl.idl, %_idl_client.h, $(IDL_SOURCES)))


.PHONY: all mmc


#
# Main programs
#

APPS := wedge-counters burn-prof
ifeq "$(ENABLE_L4)" "y"
APPS += mmc rmon
endif

all: $(APPS) 


mmc: 
	$(Q) (cd mmc && make)

wedge-counters: wedge-counters.c
	diet gcc $(CFLAGS) -o $(BUILDDIR)/$@ $<

burn-prof: burn-prof.cc elfsimple.cc
	g++ $(CFLAGS) -o $(BUILDDIR)/$@ $^

rmon: rmon.cc

	g++ $(CFLAGS) -o $(BUILDDIR)/$@ $^ $(LIBS)

rmon.cc: $(IDL_SERVER_OUTPUT) $(IDL_CLIENT_OUTPUT)
burn-prof.cc elfsimple.cc: elfsimple.h ia32/afterburn_syscalls.h ia32/types.h
wedge-counters.c: ia32/afterburn_syscalls.h ia32/types.h

clean:
	rm -f $(BUILDDIR)/wedge-counters
	rm -f $(BUILDDIR)/burn-prof	
	$(Q) (cd mmc && make clean )

