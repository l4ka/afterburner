
-include ../../../Makeconf.local
-include Makeconf.local

all: microdisk ebudget

microdisk: microdisk_earm.c 
	$(CC)  -O9 -o $@ $<

ebudget: ebudget.c
	$(CC) -L$(PISTACHIO_USER)/lib -o $@  $< -ll4

clean:
	rm -f microdisk ebudget

