DEFS= -DSYSCONFDIR=\"$(sysconfdir)\"
localedir = $(datadir)/locale
INCLUDES = -Iintl -DLOCALEDIR=\"$(localedir)\"

ACLOCAL_AMFLAGS = -I m4

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

nano_LDADD = @GLIB_LIBS@ @LIBINTL@

install-exec-hook:
	cd $(DESTDIR)$(bindir) && $(LN_S) nano rnano
