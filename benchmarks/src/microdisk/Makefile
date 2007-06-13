
-include ../../../Makeconf.local
-include Makeconf.local

microdisk: microdisk.c ../../../hypervisor/include/ia32/perfmon.h
	$(CC) -O9 -o $@ $<

clean:
	rm -f microdisk

