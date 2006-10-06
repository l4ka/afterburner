include $(srctree)/afterburn/drivers/Makesupport

obj-$(CONFIG_AFTERBURN_DRIVERS_NET_SERVER) += l4ka_lanaddress.o
l4ka_lanaddress-y := lanaddress.o

$(addprefix $(obj)/,$(obj-m)): $(wedge_symbols)
