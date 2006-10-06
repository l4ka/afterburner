
##  This is the top-level Makefile for the entire Afterburner project.
##  It is designed to be invoked from a build directory, from the command 
##  line, using make's -f parameter.

afterburn_dir := $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
ifneq ($(firstword $(subst /,/ ,$(afterburn_dir))),/)
  afterburn_dir := $(CURDIR)/$(afterburn_dir)
endif

include $(afterburn_dir)/Mk/Makefile.top

