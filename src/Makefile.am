localedir = $(datadir)/locale
INCLUDES = -Iintl -DLOCALEDIR=\"$(localedir)\" -DSYSCONFDIR=\"$(sysconfdir)\"

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = 	nano
nano_SOURCES =	chars.c \
		color.c \
		cut.c \
		files.c \
		global.c \
		move.c \
		nano.c \
		nano.h \
		prompt.c \
		proto.h \
		rcfile.c \
		search.c \
		text.c \
		utils.c \
		winio.c 

nano_LDADD = @GLIB_LIBS@ @LIBINTL@

install-exec-hook:
	cd $(DESTDIR)$(bindir) && rm -f rnano && $(LN_S) nano rnano
