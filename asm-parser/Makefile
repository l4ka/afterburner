######################################################################
##                
## Copyright (C) 2003-2010,  Karlsruhe University
##                
## File path:     Makefile
## Description:   
##                
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
## 
## THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
## ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
## DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
## OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
## HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
## LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
## OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
## SUCH DAMAGE.
##                
## $Id$
##                
######################################################################
##  This Makefile builds the afterburner assembler parser.
##
##  Build parameters of interest to the user:
##    O = The output directory for object files and binaries.
##    rebuild-antlr : Define this parameter to expose the make rules that
##        process antlr grammars.
##

O ?= .
ARCH ?= ia32

antlr_cflags ?= $(shell antlr-config --cflags)
antlr_libs   ?= $(shell antlr-config --libs)

.PHONY: all
all: $(O)/afterburner

ifeq "$(origin CXX)" "default"
CXX=g++
endif

CXXFLAGS  += -Wall -O2 $(antlr_cflags) -I. -I$(O) -DARCH_$(ARCH)
LIBS    += $(antlr_libs)

.PHONY: afterburner
afterburner: $(O)/afterburner
$(O)/afterburner: $(O)/afterburner.o \
                  $(O)/AsmLexer.o $(O)/AsmParser.o $(O)/AsmTreeParser.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(O)/afterburner.o: $(O)/AsmLexer.hpp $(O)/AsmParser.hpp $(O)/AsmTreeParser.hpp afterburner.cpp
$(O)/AsmParser.o: $(O)/AsmParser.hpp $(O)/AsmParser.cpp
$(O)/AsmLexer.o: $(O)/AsmLexer.hpp $(O)/AsmLexer.cpp
$(O)/AsmTreeParser.o: $(O)/AsmTreeParser.hpp $(O)/AsmTreeParser.cpp

$(O)/autovmi.o: $(O)/VmiCallsParser.hpp $(O)/VmiCallsLexer.hpp autovmi.cpp
$(O)/VmiCallsParser.o: $(O)/VmiCallsParser.hpp $(O)/VmiCallsParser.cpp
$(O)/VmiCallsLexer.o: $(O)/VmiCallsLexer.hpp $(O)/VmiCallsLexer.cpp

.PHONY: autovmi
autovmi: $(O)/autovmi
$(O)/autovmi: $(O)/autovmi.o $(O)/VmiCallsLexer.o $(O)/VmiCallsParser.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

.PHONY: vmi2html
vmi2html: $(O)/vmi2html
$(O)/vmi2html: $(O)/vmi2html.o $(O)/VmiCallsLexer.o $(O)/VmiCallsParser.o $(O)/VmiCallsHtmlEmitter.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(O)/afterburner.o: afterburner.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(O)/%.o: $(O)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	-rm -f $(O)/*.o $(O)/afterburner

######################################################################
##
##  Process the antlr grammars, but only if rebuild-antlr=1
##
######################################################################

ifdef rebuild-antlr

.PHONY: compile-grammar
compile-grammar: $(O)/AsmParser.cpp VmiCallsParser.cpp

$(O)/AsmTreeParser.cpp $(O)/AsmTreeParser.hpp: $(O)/Asm.g.i
$(O)/AsmParser.cpp $(O)/AsmParser.hpp: $(O)/Asm.g.i
$(O)/AsmLexer.hpp $(O)/AsmLexer.cpp: $(O)/Asm.g.i

VmiCallsParser.cpp VmiCallsParser.hpp: VmiCalls.g
VmiCallsLexer.cpp VmiCallsLexer.hpp: VmiCalls.g
VmiCallsHtmlEmitter.cpp VmiCallsHtmlEmitter.hpp: VmiCalls.g

$(O)/%Lexer.hpp $(O)%Lexer.cpp \
$(O)/%Parser.cpp $(O)/%Parser.hpp \
$(O)/%TreeParser.cpp $(O)%TreeParser.hpp $(O)/%ParserTokenTypes.hpp $(O)/%ParserTokenTypes.txt: $(O)/%.g.i
	antlr -o $(O) 	$< 

$(O)/%.g.i: %.g
	cpp $(CXXFLAGS) $^ '-DPREPROCESS_FIRST_MASK(x)=x' | egrep -v '^# ' > $@

endif

clean-antlr:
	-rm -f *.class
	-rm -f *TokenTypes.txt *TokenTypes.hpp
	-rm -f *Parser.hpp *Parser.cpp *Lexer.hpp *Lexer.cpp
	-rm -f *Emitter.hpp *Emitter.cpp
	-rm -f *.g.i

