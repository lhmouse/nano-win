mandir = @mandir@/fr
htmldir = @htmldir@/fr

BUILT_SOURCES =
dist_man_MANS = nano.1 rnano.1

if GROFF_HTML
BUILT_SOURCES += nano.1.html rnano.1.html
endif

if USE_NANORC
dist_man_MANS += nanorc.5
if GROFF_HTML
BUILT_SOURCES += nanorc.5.html
endif
endif

if GROFF_HTML
dist_html_DATA = nano.1.html nanorc.5.html rnano.1.html

nano.1.html: nano.1
	groff -t -mandoc -Thtml < $? > $@
nanorc.5.html: nanorc.5
	groff -t -mandoc -Thtml < $? > $@
rnano.1.html: rnano.1
	groff -t -mandoc -Thtml < $? > $@
endif

EXTRA_DIST = $(BUILT_SOURCES)
