## $Id$
bin_PROGRAMS = 	nano
nano_SOURCES =	cut.c \
		files.c \
		global.c \
		move.c \
		nano.c \
		nano.h \
		proto.h \
		search.c \
		utils.c \
		winio.c 

man_MANS = nano.1
nano_LDADD = @INTLLIBS@

info_TEXINFOS = nano.texi
MAKEINFO = makeinfo --no-split

EXTRA_DIST =	AUTHORS BUGS COPYING ChangeLog INSTALL NEWS README \
		TODO install-sh missing mkinstalldirs nano.1 nano.1.html \
		faq.html

SUBDIRS = po intl

localedir = $(datadir)/locale
INCLUDES = -Iintl -DLOCALEDIR=\"$(localedir)\" -I@includedir@
