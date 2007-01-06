if USE_NLS
SUBDIRS = fr
else
SUBDIRS =
endif

if USE_NANORC
man_MANS = nano.1 nanorc.5 rnano.1
else
man_MANS = nano.1 rnano.1
endif

nano_mans = nano.1 nanorc.5 rnano.1

BUILT_SOURCES = nano.1.html nanorc.5.html rnano.1.html

nano.1.html: nano.1
	cat $< | groff -t -mandoc -Thtml > $@
nanorc.5.html: nanorc.5
	cat $< | groff -t -mandoc -Thtml > $@
rnano.1.html: rnano.1
	cat $< | groff -t -mandoc -Thtml > $@

EXTRA_DIST = $(nano_mans) $(BUILT_SOURCES)
