
include $(srctree)/afterburn/drivers/Makesupport

obj-$(CONFIG_AFTERBURN_DRIVERS_NET_SERVER) += l4ka_net_server.o
obj-$(CONFIG_AFTERBURN_DRIVERS_NET_CLIENT) += l4ka_net_client.o
l4ka_net_server-y := net.o server.o
l4ka_net_client-y := net.o client.o

$(addprefix $(obj)/,$(obj-m)): $(wedge_symbols)
