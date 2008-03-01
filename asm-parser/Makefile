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

CXXFLAGS  += -Wall -O2 $(antlr_cflags) -I. -I$(O) -DARCH_$(ARCH)
LIBS    += $(antlr_libs)

.PHONY: afterburner
afterburner: $(O)/afterburner
$(O)/afterburner: $(O)/afterburner.o \
                  $(O)/AsmLexer.o $(O)/AsmParser.o $(O)/AsmTreeParser.o
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

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
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

.PHONY: vmi2html
vmi2html: $(O)/vmi2html
$(O)/vmi2html: $(O)/vmi2html.o $(O)/VmiCallsLexer.o $(O)/VmiCallsParser.o $(O)/VmiCallsHtmlEmitter.o
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(O)/afterburner.o: afterburner.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

$(O)/%.o: $(O)/%.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

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

