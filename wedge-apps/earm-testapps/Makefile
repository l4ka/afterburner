TARGETS_S = aluadd pushpop aluadd2
TARGETS_C = memrw
TARGETS=$(TARGETS_S) $(TARGETS_C)

all: $(TARGETS)

$(TARGETS_S): %: %.s
	gcc -o $@ $<

$(TARGETS_C): %: %.c
	gcc -o $@ $<

clean:
	rm $(TARGETS)

install:
	cp $(TARGETS) /usr/local/bin

uninstall: $(addprefix /usr/local/bin/,$(TARGETS))
	rm $(addprefix /usr/local/bin/,$(TARGETS))
