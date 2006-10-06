
include $(srctree)/afterburn/drivers/Makesupport

obj-$(CONFIG_AFTERBURN_DRIVERS_PCI_SERVER) += l4ka_pci_server.o
l4ka_pci_server-y := server.o

$(addprefix $(obj)/,$(obj-m)): $(wedge_symbols)
