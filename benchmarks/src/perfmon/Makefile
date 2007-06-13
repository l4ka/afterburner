
######################################################################
##  Configuration
######################################################################

-include Makeconf.local

O ?= .
PERFMON_INCLUDE ?= ../../../hypervisor/include/ia32
EXTRA_CFLAGS ?= -mno-tls-direct-seg-refs

######################################################################
##  Top level build
######################################################################

targets = $(O)/perfmon $(O)/perfsweep

all: $(targets)

######################################################################
##  Build rules
######################################################################

CFLAGS += $(EXTRA_CFLAGS) -Wall -O2
CFLAGS += -I$(PERFMON_INCLUDE)

LDFLAGS += $(EXTRA_CFLAGS)

$(O)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(O)/perfmon.o: perfmon.c $(PERFMON_INCLUDE)/perfmon.h
$(O)/perfsweep.o: perfsweep.c $(PERFMON_INCLUDE)/perfmon.h

$(O)/perfmon: $(O)/perfmon.o
$(O)/perfsweep: $(O)/perfsweep.o

clean:
	rm -f $(targets) $(O)/*.o

