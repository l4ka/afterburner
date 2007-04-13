
CONFIG_AFTERBURN_WEDGE_NAME := $(subst ",,$(CONFIG_AFTERBURN_WEDGE_NAME))

##  cfg.Mk should define:
##    cfg_pistachio_dir
##    cfg_marzipan_dir
##    cfg_wedge_build_prefix
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

ldflags_symbols := --just-symbols=$(cfg_wedge_build_prefix)$(CONFIG_AFTERBURN_WEDGE_NAME)/afterburn-wedge.symbols

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
    resourcemon_idl_client.h l4linux_idl_client.h \
    lanaddress_idl_client.h lanaddress_idl_server.h \
    L4VMnet_idl_client.h L4VMnet_idl_server.h \
    L4VMblock_idl_client.h L4VMblock_idl_server.h \
    L4VMpci_idl_server.h

extra-y := $(idl4-interfaces)

$(obj)/force_idl.o: $(addprefix $(obj)/,$(idl4-interfaces))
$(obj)/force_idl.o: $(cfg_wedge_build_prefix)$(CONFIG_AFTERBURN_WEDGE_NAME)/afterburn-wedge.symbols
