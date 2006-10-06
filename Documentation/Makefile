
.PHONY: all
all: L4Ka-Afterburner.pdf

L4Ka-Afterburner.pdf: L4Ka-Afterburner.tex figures/*
	pdflatex $^
	pdflatex $^

clean:
	rm -f *.aux *.log *.blg *.bbl *.out *.toc L4Ka-Afterburner.pdf

