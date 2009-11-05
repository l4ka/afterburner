 include $(srctree)/afterburn/drivers/Makesupport

obj-$(CONFIG_AFTERBURN_DRIVERS_BLOCK_SERVER) += l4ka_block_server.o
obj-$(CONFIG_AFTERBURN_DRIVERS_BLOCK_CLIENT) += l4ka_block_client.o

l4ka_block_server-y := server.o
l4ka_block_client-y := client.o

l4ka_block_server-$(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK_SERVER) += earm_server.o
l4ka_block_client-$(CONFIG_AFTERBURN_DRIVERS_EARM_BLOCK_CLIENT) += earm_client.o

$(addprefix $(obj)/,$(obj-m)): $(wedge_symbols)
