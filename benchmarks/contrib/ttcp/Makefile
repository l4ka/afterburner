
all: ttcp


ttcp: ttcp.c Makefile ../../../hypervisor/include/ia32/perfmon.h
	$(CC) -O9 -o $@ $<

ttcp.man: ttcp.1
	nroff -man $^ > $@

clean:
	rm -f ttcp ttcp.man
