bin_PROGRAMS = 	nano
nano_SOURCES =	cut.c \
		global.c \
		nano.c \
		nano.h \
		proto.h \
		utils.c \
		winio.c 

man_MANS = nano.1
nano_LDADD = @INTLLIBS@

EXTRA_DIST =	AUTHORS BUGS COPYING ChangeLog INSTALL NEWS README \
		TODO install-sh missing mkinstalldirs nano.1 nano.1.html

SUBDIRS = po intl

localedir = $(datadir)/locale
INCLUDES = -Iintl -DLOCALEDIR=\"$(localedir)\" -I@includedir@
