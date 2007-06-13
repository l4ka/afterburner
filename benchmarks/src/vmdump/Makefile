
-include ../../../Makeconf.local
-include Makeconf.local

PISTACHIO_USER	?= $(HOME)/pistachio/cvs/user
IDL4		?= idl4
IDL4_CONFIG	?= idl4-config

IDL4_FLAGS	= -fctypes -iV4 -Wall -fno-use-malloc -mc++
IDL4_CFLAGS	= $(shell $(IDL4_CONFIG) --cflags)
INCLUDE		= -I$(PISTACHIO_USER)/include
LDLIBS		= -L$(PISTACHIO_USER)/lib/l4 -ll4

vmdump: vmdump.c hypervisor_idl_client.h
	$(CC) -O2 -W -Wall -o $@ $< $(IDL4_CFLAGS) $(INCLUDE) $(LDLIBS)

%_idl_client.h: ../../../interfaces/%_idl.idl
	$(IDL4) $(IDL4_FLAGS) $(INCLUDE) -h $@ -c $<


clean:
	rm -f vmdump hypervisor_idl_client.h *~
