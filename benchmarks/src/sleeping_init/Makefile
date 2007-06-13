
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


