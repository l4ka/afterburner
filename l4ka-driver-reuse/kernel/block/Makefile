######################################################################
##                
## Copyright (C) 2003-2010,  Karlsruhe University
##                
## File path:     kernel/block/Makefile
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
include $(srctree)/afterburn/drivers/Makesupport

obj-$(CONFIG_AFTERBURN_DRIVERS_BLOCK_SERVER) += l4ka_block_server.o
obj-$(CONFIG_AFTERBURN_DRIVERS_BLOCK_CLIENT) += l4ka_block_client.o
#obj-$(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK_SERVER) += \
#	ebc_hcache.o ebc_lhmap.o ebc_lru.o ebc_prefetcher.o ebc_policy1.o ebc_trace.o

l4ka_block_server-y := server.o
l4ka_block_client-y := client.o

l4ka_block_server-$(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK_SERVER) += earm_server.o 
l4ka_block_client-$(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK_CLIENT) += earm_client.o


$(addprefix $(obj)/,$(obj-m)): $(wedge_symbols)
