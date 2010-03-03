######################################################################
##                
## Copyright (C) 2003-2010,  Karlsruhe University
##                
## File path:     vmdump/Makefile
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

PISTACHIO_USER	?= $(HOME)/pistachio/cvs/user
IDL4		?= idl4
IDL4_CONFIG	?= idl4-config

IDL4_FLAGS	= -fctypes -iV4 -Wall -fno-use-malloc -mc++
IDL4_CFLAGS	= $(shell $(IDL4_CONFIG) --cflags)
INCLUDE		= -I$(PISTACHIO_USER)/include
LDLIBS		= -L$(PISTACHIO_USER)/lib/l4 -ll4

vmdump: vmdump.c hypervisor_idl_client.h
	$(CC) -O2 -W -Wall -o $@ $< $(IDL4_CFLAGS) $(INCLUDE) $(LDLIBS)

%_idl_client.h: ../../../interfaces/%_idl.idl
	$(IDL4) $(IDL4_FLAGS) $(INCLUDE) -h $@ -c $<


clean:
	rm -f vmdump hypervisor_idl_client.h *~
