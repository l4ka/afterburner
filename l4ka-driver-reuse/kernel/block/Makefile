include $(srctree)/afterburn/drivers/Makesupport

obj-$(CONFIG_AFTERBURN_DRIVERS_BLOCK_SERVER) += l4ka_block_server.o
obj-$(CONFIG_AFTERBURN_DRIVERS_BLOCK_CLIENT) += l4ka_block_client.o
l4ka_block_server-y := server26.o
l4ka_block_client-y := client26.o

$(addprefix $(obj)/,$(obj-m)): $(wedge_symbols)
