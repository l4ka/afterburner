
##  This is the top-level Makefile for the native driver subproject.
##  It is designed to be invoked from a build directory, from the command 
##  line, using make's -f parameter.

drivers_dir := $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
ifneq ($(firstword $(subst /,/ ,$(drivers_dir))),/)
  drivers_dir := $(CURDIR)/$(drivers_dir)
endif

include $(drivers_dir)/Mk/Makefile.top

