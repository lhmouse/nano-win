## $Id$
AUTOMAKE_OPTIONS = gnu no-dependencies

DEFS= -DSYSCONFDIR=\"$(sysconfdir)\"

bin_PROGRAMS = 	nano
nano_SOURCES =	color.c \
		cut.c \
		files.c \
		global.c \
		move.c \
		nano.c \
		nano.h \
		proto.h \
		rcfile.c \
		search.c \
		utils.c \
		winio.c 

man_MANS = nano.1
nano_LDADD = @GLIB_LIBS@ @INTLLIBS@

info_TEXINFOS = nano.texi
MAKEINFO = makeinfo --no-split

EXTRA_DIST =	ABOUT-NLS AUTHORS BUGS COPYING ChangeLog INSTALL NEWS \
		README THANKS TODO install-sh missing mkinstalldirs \
		nano.1 nano.1.html faq.html nanorc.sample nano.spec

SUBDIRS = po intl m4

localedir = $(datadir)/locale
INCLUDES = -Iintl -DLOCALEDIR=\"$(localedir)\" -I@includedir@
