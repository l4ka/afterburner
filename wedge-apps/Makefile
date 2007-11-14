CFLAGS += -Wall -O2
CFLAGS += -I. -I$(L4KA_PISTACHIO_USER)/include -I$(L4KA_INTERFACE_DIR)
CFLAGS += $(shell $(IDL4_CONFIG) --cflags)
CFLAGS += -L$(L4KA_PISTACHIO_USER)/lib/l4 
LIBS   += -ll4

INCLUDE  += -I $(L4KA_PISTACHIO_USER)/include -I$(L4KA_INTERFACE_DIR) -I $(BUILDDIR)

IDL4 ?= idl4
IDL4_CONFIG ?= idl4-config

#  NOTE: use idl4's --pre-call= and --post-call= parameters to ensure that
#  particular functions are invoked prior to and after each IPC.

IDL4_FLAGS = 	-fctypes -Wall -fno-use-malloc \
		--interface=v4 --mapping=c++ $(IDL4_FLAGS_$(ARCH)) \
		$(addprefix -I, $(INCLUDE))

$(BUILDDIR)/%_idl_server.h: $(L4KA_INTERFACE_DIR)/%_idl.idl 
	mkdir -p $(BUILDDIR)/include
	$(IDL4) $(IDL4_FLAGS) -h $@ -s $<

$(BUILDDIR)/%_idl_client.h: $(L4KA_INTERFACE_DIR)/%_idl.idl 
	mkdir -p $(BUILDDIR)/include
	$(IDL4) $(IDL4_FLAGS) -h $@ -c $<


#
#  Determine our IDL source files from the $(IDL_SOURCES) variable created
#  by including all the Makeconf files from the source directories.
#
IDL_SOURCES += resourcemon_idl.idl


IDL_SERVER_OUTPUT = $(addprefix $(BUILDDIR)/, \
	$(patsubst %_idl.idl, %_idl_server.h, $(IDL_SOURCES)))

IDL_CLIENT_OUTPUT = $(addprefix $(BUILDDIR)/, \
	$(patsubst %_idl.idl, %_idl_client.h, $(IDL_SOURCES)))


.PHONY: all mmc


#
# Main programs
#

APPS: wedge-counters burn-prof rmon mmc

all: $(APPS) 


mmc: 
	$(Q) (cd mmc && make)

wedge-counters: wedge-counters.c
	diet gcc $(CFLAGS) -o $(BUILDDIR)/$@ $<

burn-prof: burn-prof.cc elfsimple.cc
	g++ $(CFLAGS) -o $(BUILDDIR)/$@ $^

rmon: rmon.cc

	g++ $(CFLAGS) -o $(BUILDDIR)/$@ $^ $(LIBS)

rmon.cc: $(IDL_SERVER_OUTPUT) $(IDL_CLIENT_OUTPUT)
burn-prof.cc elfsimple.cc: elfsimple.h ia32/afterburn_syscalls.h ia32/types.h
wedge-counters.c: ia32/afterburn_syscalls.h ia32/types.h

clean:
	rm -f $(BUILDDIR)/wedge-counters
	rm -f $(BUILDDIR)/burn-prof	
	$(Q) (cd mmc && make clean )

