ARCH = ia32
CPP=cpp

IDL4 ?= idl4
IDL4_CONFIG ?= idl4-config

INCLUDES_l4ka += $(L4KA_PISTACHIO_USER)/include $(L4KA_INTERFACE_DIR)

######################################################################
#
# IDL files
#

IDL4_FLAGS_ia32  = --platform=ia32 --with-cpp=$(CPP)
IDL4_FLAGS_amd64 = --platform=ia32 --word-size=64 --with-cpp=$(CPP)
IDL4_FLAGS_ia64  = --platform=generic --word-size=64 --with-cpp=$(CPP)

#  NOTE: use idl4's --pre-call= and --post-call= parameters to ensure that
#  particular functions are invoked prior to and after each IPC.
CFLAGS += $(shell $(IDL4_CONFIG) --cflags)

IDL4_FLAGS = 	-fctypes -Wall -fno-use-malloc \
		--interface=v4 --mapping=c++ $(IDL4_FLAGS_$(ARCH)) \
		--with-cpp=$(CPP) \
		$(addprefix -I, $(INCLUDES_l4ka))

$(BUILDDIR)/include/%_idl_server.h: $(L4KA_INTERFACE_DIR)/%_idl.idl $(BUILDDIR)/include
	$(IDL4) $(IDL4_FLAGS) -h $@ -s $<

$(BUILDDIR)/include/%_idl_client.h: $(L4KA_INTERFACE_DIR)/%_idl.idl $(BUILDDIR)/include
	$(IDL4) $(IDL4_FLAGS) -h $@ -c $<

$(BUILDDIR)/include:
	mkdir -p $(BUILDDIR)/include

#
#  Determine our IDL source files from the $(IDL_SOURCES) variable created
#  by including all the Makeconf files from the source directories.
#

IDL_SERVER_OUTPUT = $(addprefix $(BUILDDIR)/include/, \
	$(patsubst %_idl.idl, %_idl_server.h, $(IDL_SOURCES)))

IDL_CLIENT_OUTPUT = $(addprefix $(BUILDDIR)/include/, \
	$(patsubst %_idl.idl, %_idl_client.h, $(IDL_SOURCES)))

