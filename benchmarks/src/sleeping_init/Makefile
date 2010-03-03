######################################################################
##                
## Copyright (C) 2003-2010,  Karlsruhe University
##                
## File path:     sleeping_init/Makefile
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

CC  ?= /tools/bin/gcc2
CXX ?= $(CC)
TFTP_SCP ?= i30pc5.ira.uka.de.:/tftpboot/$(USER)/.
GENROMFS ?= $(HOME)/build/genromfs-0.5.1/genromfs
WORKDIR  ?= work

all: install

clean:
	rm -f sleeping_init 
	rm -f sleeping_init.img.gz
	rm -rf $(WORKDIR)

.PHONY: install Makefile Makeconf.local
install: sleeping_init.img.gz
	@echo "==> Copying to target tftp server ..."
	scp $^ $(TFTP_SCP)

sleeping_init.img.gz: sleeping_init
	@echo "==> Building ROM image ..."
	mkdir -p $(WORKDIR)
	cp $^ $(WORKDIR)/.
	strip $(addprefix $(WORKDIR)/,$^)
	$(GENROMFS) -d $(WORKDIR) -f sleeping_init.img
	gzip -f sleeping_init.img

sleeping_init: sleeping_init.c
	@echo "==> Building $@ ..."
	$(CC) -static -o $@ $^


