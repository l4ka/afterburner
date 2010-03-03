######################################################################
##                
## Copyright (C) 2003-2010,  Karlsruhe University
##                
## File path:     kernel/Makefile
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
##  cfg.Mk should define:
##    cfg_pistachio_dir
##    cfg_marzipan_dir
##    cfg_wedge_build_dir

ifneq ($(CONFIG_AFTERBURN_DRIVERS),n)
include $(obj)/cfg.Mk
endif


obj-$(CONFIG_AFTERBURN_DRIVERS) += force_idl.o
obj-$(CONFIG_AFTERBURN_DRIVERS) += glue/
obj-$(CONFIG_AFTERBURN_DRIVERS_NET_SERVER) += lanaddress/ net/
obj-$(CONFIG_AFTERBURN_DRIVERS_NET_CLIENT) += net/
obj-$(CONFIG_AFTERBURN_DRIVERS_BLOCK_SERVER) += block/
obj-$(CONFIG_AFTERBURN_DRIVERS_BLOCK_CLIENT) += block/
obj-$(CONFIG_AFTERBURN_DRIVERS_PCI_SERVER) += pci/

ldflags_symbols := --just-symbols=$(cfg_wedge_build_dir)/afterburn-wedge.symbols

EXTRA_LDFLAGS += $(ldflags_symbols)
LDFLAGS_MODULE += $(ldflags_symbols)


IDL4 ?= idl4
IDL4_CONFIG ?= idl4-config
IDL4_FLAGS := -fctypes  -iV4 -Wall -fno-use-malloc -I$(cfg_pistachio_dir)/include

quiet_cmd_idl4_client = IDL4c   $<
quiet_cmd_idl4_server = IDL4s   $<
cmd_idl4_client = $(IDL4) $(IDL4_FLAGS) -h $@ -c $<
cmd_idl4_server = $(IDL4) $(IDL4_FLAGS) -h $@ -s $<

##  NOTE: the IDL files will be rebuilt if the IDL command line arguments
##  are changed.  Thus always add IDL4 output files to the extra-y list or 
##  they'll always be built.

$(obj)/%_idl_client.h: $(cfg_marzipan_dir)/%_idl.idl FORCE
	$(call if_changed,idl4_client)

$(obj)/%_idl_server.h: $(cfg_marzipan_dir)/%_idl.idl FORCE
	$(call if_changed,idl4_server)

idl4-interfaces = \
    resourcemon_idl_client.h lanaddress_idl_client.h lanaddress_idl_server.h \
    L4VMnet_idl_client.h L4VMnet_idl_server.h \
    L4VMblock_idl_client.h L4VMblock_idl_server.h \
    L4VMpci_idl_server.h

ifeq ($(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK),y)
idl4-interfaces += earm_idl_client.h earm_idl_server.h 
endif

extra-y := $(idl4-interfaces)

$(obj)/force_idl.o: $(addprefix $(obj)/,$(idl4-interfaces))
$(obj)/force_idl.o: $(cfg_wedge_build_dir)/afterburn-wedge.symbols
