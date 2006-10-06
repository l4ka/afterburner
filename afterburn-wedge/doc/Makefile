#
#  Makefile
#
#  $Id: Makefile,v 1.4 2005/05/20 15:19:05 joshua Exp $
#
#  To create a LaTeX diff against the CVS HEAD revision, use the target
#  userman-diff.  To use a revision other than HEAD, 
#  speciffy DIFF=x on the make command line to choose the revision x.
#
#  Note: $< expands to the first dependency, $^ expands to all dependencies,
#    $@ expands to the output

TARGETS = userman
OPTIONAL = userman-diff devguide devguide-diff
OPTIONAL_SUBDIRS = userman-html devguide-html
ALL = $(TARGETS) $(OPTIONAL)

PAPER_SIZE = a4

PS = $(addsuffix .ps, $(TARGETS))
PDF = $(addsuffix .pdf, $(TARGETS))

all: pdf
ps: $(PS)
pdf: $(PDF)

##############################################################################

DIFF ?= HEAD

%-diff.dvi: %-diff.tex

%-diff.tex: %.tex FORCE
	@echo "====> Retrieving revision $(DIFF) of $<"
	cvs update -p -r $(DIFF) $<  > $(@:-diff.tex=-$(DIFF)-diff.tex)
	@echo "====> Creating diff of revision $(DIFF) of $<"
	./tools/latexdiff $(@:-diff.tex=-$(DIFF)-diff.tex) $< > $@

.PHONY: FORCE
FORCE:

##############################################################################

LATEX = latex -interaction=batchmode
BIBTEX = bibtex


##  Build a PDF directly from DVI.  To enable, put this rule before other PDF
##  rules.
#%.pdf: %.dvi
#	@echo "====> Generating PDF"
#	dvipdfm -p $(PAPER_SIZE) -o $@ $<

##  Build a PDF from PS.  To enable, put this rule before other PDF rules.
%.pdf: %.ps
	@echo "====> Converting Postscript to PDF"
	ps2pdf $< $@

##  Build a Postscript file from the DVI.
%.ps: %.dvi
	@echo "====> Generating Postscript"
	dvips -t $(PAPER_SIZE) -Ppdf -G0 $< -o $@


BASETEX = $(basename $<)
RERUN = '(There were undefined references|Rerun to get (cross-references|the bars) right)'
RERUNBIB = 'No file.*\.bbl|Citation.*undefined'
UNDEFINED = '((Reference|Citation).*undefined)|(Label.*multiply defined)'
%.dvi: %.tex
	@echo "====> LaTeX first pass"
	@$(LATEX) $<
	@if egrep -q $(RERUNBIB) $(BASETEX).log ; then echo "====> BibTex" && $(BIBTEX) $* && echo "====> LaTeX BibTeX pass" && $(LATEX) $< ; fi
	@if [ $(BASETEX).toc -nt $< ] ; then echo "====> LaTeX rerun" && $(LATEX) $<; fi
	@if egrep -q $(RERUN) $(BASETEX).log ; then echo "====> LaTeX rerun" && $(LATEX) $<; fi
	@if egrep -q $(RERUN) $(BASETEX).log ; then echo "====> LaTeX rerun" && $(LATEX) $<; fi
	@echo "====> Undefined references and citations:"
	@egrep -i $(UNDEFINED) $*.log || echo "None."
	@echo "====> Dimensions:"
	@grep "dimension:" $*.log || echo "None."


clean:
	rm -rf $(addsuffix .pdf, $(ALL)) $(addsuffix .ps, $(ALL))
	rm -rf $(addsuffix .dvi, $(ALL)) $(addsuffix .toc, $(ALL))
	rm -rf $(addsuffix .log, $(ALL)) $(addsuffix .aux, $(ALL))
	rm -rf $(addsuffix .bbl, $(ALL)) $(addsuffix .blg, $(ALL))
	rm -rf *-diff.tex
	rm -rf $(OPTIONAL_SUBDIRS)


##  View the file in the Mac OS X Preview application, and close the prior
##  version if necessary.
preview-%.pdf: %.pdf
	@osascript -e 'run script file "tools:preview-tiger.scpt" with parameters {"$<"}'

##############################################################################
#
#  tex4ht
#
#  To build the html output for a given LaTeX file, such as userman.tex,
#  substitute -html for .tex.  For example: make userman-html
#
##############################################################################

TEX4HT = $(HOME)/tex4ht.dir
TEX4HT_BIN = $(TEX4HT)/bin/unix/
TEX4HT_STYLES = $(TEX4HT)/texmf/tex/generic/tex4ht

%-html: %.tex FORCE
	mkdir -p $@
	cd $@ && env PATH=$(PATH):$(TEX4HT_BIN) TEXINPUTS=:..:$(TEX4HT_STYLES) htlatex $<

