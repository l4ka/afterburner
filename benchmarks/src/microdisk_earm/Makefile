-include ../../../Makeconf.local
-include Makeconf.local

##  Afterburn related configuration
diet = ../../..//extern/dietlibc-0.30/bin-i386/diet
interfaces_dir = ../../../l4ka-interfaces

##  IDL4 configuration
idl4 ?= idl4
idl4_config ?= idl4-config
idl4_flags = -fctypes -iV4 -Wall -fno-use-malloc --mapping=c++ -I$(interfaces_dir) -I$(cfg_pistachio_dir)/include

vpath %_idl.idl $(interfaces_dir)

CFLAGS  += -O2 $(shell $(idl4_config) --cflags) -static -I$(cfg_pistachio_dir)/include 
LDFLAGS += -L$(cfg_pistachio_dir)/lib -static 
LDLIBS  += -ll4 -lgcc -lm 

%_idl_client.h: %_idl.idl
	@echo "===> Creating $@ from $(<F)"
	@echo $(Q) $(idl4) $(idl4_flags) -h $@ -c $<
	$(Q) $(idl4) $(idl4_flags) -h $@ -c $<

%_idl_server.h: %_idl.idl
	@echo "===> Creating $@ from $(<F)"
	$(Q) $(idl4) $(idl4_flags) -h $@ -s $<

%_idl_template.cc: %_idl.idl
	@echo "===> Creating $@ from $(<F)"
	$(Q) $(idl4) $(idl4_flags) -o $@ -t $<

all: microdisk_earm ebudget


microdisk_earm: microdisk_earm.c 
	$(diet) $(CC) $(CFLAGS) -O9 -o $@ $<


ebudget: ebudget.c earm_idl_client.h resourcemon_idl_client.h
	$(diet) $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(Q) rm -f *_idl_client.h *_idl_server.h
	$(Q) rm -f *.o microdisk_earm ebudget

