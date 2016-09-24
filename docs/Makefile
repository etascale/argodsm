STYLE_FILE = style.css
LAYOUT_FILE = layout.html5
INCLUDE_FILES = argo_example.cpp pthreads_example.cpp
PANDOC_SCRIPTS = include

PAGES = about code-style documentation download index tutorial advanced
PAGES_HTML = $(PAGES:=.html)

.PHONY: all clean scripts

all: $(PAGES_HTML)

scripts: $(PANDOC_SCRIPTS)

clean:
	rm -f $(PAGES_HTML)

clean_all: clean
	rm -f $(PANDOC_SCRIPTS) include.hi include.o

$(PAGES_HTML): %.html: %.md $(INCLUDE_FILES) $(LAYOUT_FILE) $(PANDOC_SCRIPTS)
	pandoc -t json $< | ./include | pandoc -f json --template layout.html5 -t html5 -s -T "ArgoDSM" -c $(STYLE_FILE) -o $@

include: include.hs
	ghc -o $@ --make $<

