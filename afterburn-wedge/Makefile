######################################################################
#                
# Copyright (C) 2005, University of Karlsruhe
#                
# File path:     Makefile
# Description:   Initial Makefile for starting an Afterburn project.
#                
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#                
# $Id: Makefile,v 1.11 2006/09/21 13:34:02 joshua Exp $
#                
######################################################################

SHELL=bash

ifeq ($(strip $(SRCDIR)), )
# SRCDIR isn't set, we're probably in the source directory itself

ifeq ($(strip $(BUILDDIR)), )
# BUILDDIR isn't set. Print a small HOWTO create a build directory.
all:
	@echo "";
	@echo "Aieeee, this is the SOURCE directory!"
	@echo "-------------------------------------";
	@echo "";
	@echo "1. Run make BUILDDIR=<absolute path of nonexisting directory>"
	@echo "2. Chdir to the build directory."
	@echo "3. Verify the settings in Makeconf.*"
	@echo "4. Verify the settings in include/config.h"
	@echo "5. Run make again."
	@echo "";
	@exit 0
else
# BUILDDIR is set, but SRCDIR isnt - we're supposed to create a build directory
all:
	@echo ""
	@#
	@# Bail out if BUILDDIR is not an absolute path
	@#
	@if [ "/" != "`echo $(BUILDDIR) | cut -c1`" ]; then \
	   echo "Oops, $(BUILDDIR) is not an absolute path ... aborting." && \
	   echo ""; \
	   exit -1 ; \
	 fi
	@#
	@# Check, if we try to mess up an existing directory
	@#
	@if [ -d $(BUILDDIR) ]; then \
	   echo "Oops, $(BUILDDIR) already exists ... aborting." && \
	   echo ""; \
	   exit -1 ; \
	 fi
	@echo "Setting up build directory $(BUILDDIR) ..."
	@#
	@# Check if the parent of BUILDDIR exists.  If not, create it
	@#
	@if [ ! -d $(dir $(BUILDDIR:%/=%)) ]; then \
	   mkdir -p $(dir $(BUILDDIR:%/=%)); \
	 fi
	@#
	@# Copy the template build directory to the new location
	@#
	@cp -r Mk/template/ $(BUILDDIR)
	@#
	@# Remove any CVS control structures in the build directory
	@#
	@rm -rf `find $(BUILDDIR) -type d -name CVS`
	@#
	@# Fix up the location of the source directory in Makeconf.local
	@# using the Makeconf.local.template
	@#
	@sed -e 's@%SRCDIR%@$(PWD)@' $(BUILDDIR)/Makeconf.local.template \
		> $(BUILDDIR)/Makeconf.local
	@rm -f $(BUILDDIR)/Makeconf.local.template


	@echo done.
	@echo ""
endif
else

# load all the compiler settings
include $(SRCDIR)/Mk/Makerules

endif
