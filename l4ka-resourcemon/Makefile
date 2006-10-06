
##  This is the top-level Makefile for the resource monitor subproject.
##  It is designed to be invoked from a build directory, from the command 
##  line, using make's -f parameter.

resourcemon_dir := $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
resourcemon_dir := $(patsubst %/,%,$(resourcemon_dir))

ifneq ($(firstword $(subst /,/ ,$(resourcemon_dir))),/)
  resourcemon_dir := $(CURDIR)/$(resourcemon_dir)
endif

include $(resourcemon_dir)/Mk/Makefile.top

