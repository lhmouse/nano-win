SUBDIRS = fr

man_MANS = nano.1 nanorc.5

BUILT_SOURCES = nano.1.html nanorc.5.html

nano.1.html: nano.1
	cat $< | groff -t -mandoc -Thtml > $@
nanorc.5.html: nanorc.5
	cat $< | groff -t -mandoc -Thtml > $@
