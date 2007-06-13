CFLAGS = -O $(SYSFLAGS)

bsd:	
	@echo 'Options are "make bsd" and "make SysV" - the default is "bsd".'
	@echo 'If you get messages about missing functions, try "make SysV."'
	make Bonnie 

SysV:	
	make Bonnie 'SYSFLAGS=-DSysV'

Bonnie:	
