## $Id$
AUTOMAKE_OPTIONS = gnu no-dependencies
M4DIR = $(srcdir)/m4
ACLOCAL_AMFLAGS = -I $(M4DIR)
ACINCLUDE_INPUTS = $(M4DIR)/gettext.m4 $(M4DIR)/iconv.m4 $(M4DIR)/largefile.m4 \
	$(M4DIR)/progtest.m4 $(M4DIR)/codeset.m4 $(M4DIR)/glibc21.m4 \
	$(M4DIR)/isc-posix.m4 $(M4DIR)/lcmessage.m4

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
		nano.1 nano.1.html faq.html nanorc.sample $(ACINCLUDE_INPUTS)

SUBDIRS = po intl

localedir = @LOCALE_DIR@
INCLUDES = -Iintl -DLOCALEDIR=\"$(localedir)\" -I@includedir@

$(srcdir)/acinclude.m4: $(ACINCLUDE_INPUTS)
	cat $(ACINCLUDE_INPUTS) > $@
