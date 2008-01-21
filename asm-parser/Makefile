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

CXXFLAGS  += -Wall -O2 $(antlr_cflags) -I. -DARCH_$(ARCH)
LIBS    += $(antlr_libs)

.PHONY: afterburner
afterburner: $(O)/afterburner
$(O)/afterburner: $(O)/afterburner.o \
                  $(O)/AsmLexer.o $(O)/AsmParser.o $(O)/AsmTreeParser.o
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(O)/afterburner.o: AsmLexer.hpp AsmParser.hpp AsmTreeParser.hpp afterburner.cpp
$(O)/AsmParser.o: AsmParser.hpp AsmParser.cpp
$(O)/AsmLexer.o: AsmLexer.hpp AsmLexer.cpp
$(O)/AsmTreeParser.o: AsmTreeParser.hpp AsmTreeParser.cpp

$(O)/autovmi.o: VmiCallsParser.hpp VmiCallsLexer.hpp autovmi.cpp
$(O)/VmiCallsParser.o: VmiCallsParser.hpp VmiCallsParser.cpp
$(O)/VmiCallsLexer.o: VmiCallsLexer.hpp VmiCallsLexer.cpp

.PHONY: autovmi
autovmi: $(O)/autovmi
$(O)/autovmi: $(O)/autovmi.o $(O)/VmiCallsLexer.o $(O)/VmiCallsParser.o
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

.PHONY: vmi2html
vmi2html: $(O)/vmi2html
$(O)/vmi2html: $(O)/vmi2html.o $(O)/VmiCallsLexer.o $(O)/VmiCallsParser.o $(O)/VmiCallsHtmlEmitter.o
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(O)/%.o: %.cpp
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
compile-grammar: AsmParser.cpp VmiCallsParser.cpp

AsmTreeParser.cpp AsmTreeParser.hpp: Asm.g.i
AsmParser.cpp AsmParser.hpp: Asm.g.i
AsmLexer.hpp AsmLexer.cpp: Asm.g.i

VmiCallsParser.cpp VmiCallsParser.hpp: VmiCalls.g
VmiCallsLexer.cpp VmiCallsLexer.hpp: VmiCalls.g
VmiCallsHtmlEmitter.cpp VmiCallsHtmlEmitter.hpp: VmiCalls.g

%Lexer.hpp %Lexer.cpp \
%Parser.cpp %Parser.hpp \
%TreeParser.cpp %TreeParser.hpp \
%ParserTokenTypes.hpp %ParserTokenTypes.txt: %.g.i
	antlr $<

%.g.i: %.g
	cpp $(CXXFLAGS) $^ '-DPREPROCESS_FIRST_MASK(x)=x' | egrep -v '^# ' > $@

endif

clean-antlr:
	-rm -f *.class
	-rm -f *TokenTypes.txt *TokenTypes.hpp
	-rm -f *Parser.hpp *Parser.cpp *Lexer.hpp *Lexer.cpp
	-rm -f *Emitter.hpp *Emitter.cpp
	-rm -f *.g.i

