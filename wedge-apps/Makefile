
all: wedge-counters burn-prof

wedge-counters: wedge-counters.c
	diet gcc -O2 -Wall -o $@ $<

burn-prof: burn-prof.cc elfsimple.cc
	g++ -O2 -Wall -o $@ $^

burn-prof.cc elfsimple.cc: elfsimple.h ia32/afterburn_syscalls.h ia32/types.h
wedge-counters.c: ia32/afterburn_syscalls.h ia32/types.h

clean:
	rm -f wedge-counters
	rm -f burn-prof

