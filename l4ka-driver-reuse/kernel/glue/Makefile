include $(srctree)/afterburn/drivers/Makesupport


obj-$(CONFIG_AFTERBURN_DRIVERS) += l4ka_glue.o
l4ka_glue-y := glue.o thread.o vmmemory.o vmserver.o

$(addprefix $(obj)/,$(obj-m)): $(wedge_symbols)
