/* $Id$ */
/**************************************************************************
 *   rcfile.c                                                             *
 *                                                                        *
 *   Copyright (C) 1999-2002 Chris Allegretta                             *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 2, or (at your option)  *
 *   any later version.                                                   *
 *                                                                        *
 *   This program is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *   GNU General Public License for more details.                         *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program; if not, write to the Free Software          *
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            *
 *                                                                        *
 **************************************************************************/

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

#ifdef ENABLE_NANORC

const static rcoption rcopts[] = {
#ifndef NANO_SMALL
    {"autoindent", AUTOINDENT},
    {"backup", BACKUP_FILE},
#endif
    {"const", CONSTUPDATE},
#ifndef NANO_SMALL
    {"cut", CUT_TO_END},
#endif
#ifndef DISABLE_WRAPJUSTIFY
    {"fill", 0},
#endif
    {"keypad", ALT_KEYPAD},
#ifndef DISABLE_MOUSE
    {"mouse", USE_MOUSE},
#endif
#ifdef ENABLE_MULTIBUFFER
    {"multibuffer", MULTIBUFFER},
#endif
#ifndef NANO_SMALL
    {"noconvert", NO_CONVERT},
#endif
    {"nofollow", FOLLOW_SYMLINKS},
    {"nohelp", NO_HELP},
#ifndef DISABLE_WRAPPING
    {"nowrap", NO_WRAP},
#endif
#ifndef DISABLE_OPERATINGDIR
    {"operatingdir", 0},
#endif
    {"pico", PICO_MODE},
#ifndef NANO_SMALL
    {"quotestr", 0},
#endif
#ifdef HAVE_REGEX_H
    {"regexp", USE_REGEXP},
#endif
#ifndef NANO_SMALL
    {"smooth", SMOOTHSCROLL},
#endif
#ifndef DISABLE_SPELLER
    {"speller", 0},
#endif
    {"suspend", SUSPEND},
    {"tabsize", 0},
    {"tempfile", TEMP_OPT},
    {"view", VIEW_MODE},
    {NULL, 0}
};

static int errors = 0;
static int lineno = 0;
static char *nanorc;

/* We have an error in some part of the rcfile; put it on stderr and
   make the user hit return to continue starting up nano. */
void rcfile_error(const char *msg, ...)
{
    va_list ap;

    fprintf(stderr, "\n");
    if (lineno > 0)
	fprintf(stderr, _("Error in %s on line %d: "), nanorc, lineno);

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, _("\nPress return to continue starting nano\n"));

    while (getchar() != '\n');
}

/* Just print the error (one of many, perhaps) but don't abort, yet. */
void rcfile_msg(const char *msg, ...)
{
    va_list ap;

    if (!errors) {
	errors = 1;
	fprintf(stderr, "\n");
    }
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/* Parse the next word from the string.  Returns NULL if we hit EOL. */
char *parse_next_word(char *ptr)
{
    while (*ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\0')
	ptr++;

    if (*ptr == '\0')
	return NULL;

    /* Null terminate and advance ptr */
    *ptr++ = 0;

    while (*ptr == ' ' || *ptr == '\t')
	ptr++;

    return ptr;
}

/* The keywords operatingdir, fill, tabsize, speller, and quotestr take
 * an argument when set.  Among these, operatingdir, speller, and
 * quotestr have to allow tabs and spaces in the argument.  Thus, if the
 * next word starts with a ", we say it ends with the last " of the line.
 * Otherwise, the word is interpreted as usual.  That is so the arguments
 * can contain "s too. */
char *parse_argument(char *ptr)
{
    const char *ptr_bak = ptr;
    char *last_quote = NULL;

    assert(ptr != NULL);

    if (*ptr != '"')
	return parse_next_word(ptr);

    do {
	ptr++;
	if (*ptr == '"')
	    last_quote = ptr;
    } while (*ptr != '\n' && *ptr != '\0');

    if (last_quote == NULL) {
	if (*ptr == '\0')
	    ptr = NULL;
	else
	    *ptr++ = '\0';
	rcfile_error(_("argument %s has unterminated \""), ptr_bak);
    } else {
	*last_quote = '\0';
	ptr = last_quote + 1;
    }
    if (ptr != NULL)
	while (*ptr == ' ' || *ptr == '\t')
	    ptr++;
    return ptr;
}

#ifdef ENABLE_COLOR

int colortoint(const char *colorname, int *bright)
{
    int mcolor = 0;

    if (colorname == NULL)
	return -1;

    if (!strncasecmp(colorname, "bright", 6)) {
	*bright = 1;
	colorname += 6;
    }

    if (!strcasecmp(colorname, "green"))
	mcolor = COLOR_GREEN;
    else if (!strcasecmp(colorname, "red"))
	mcolor = COLOR_RED;
    else if (!strcasecmp(colorname, "blue"))
	mcolor = COLOR_BLUE;
    else if (!strcasecmp(colorname, "white"))
	mcolor = COLOR_WHITE;
    else if (!strcasecmp(colorname, "yellow"))
	mcolor = COLOR_YELLOW;
    else if (!strcasecmp(colorname, "cyan"))
	mcolor = COLOR_CYAN;
    else if (!strcasecmp(colorname, "magenta"))
	mcolor = COLOR_MAGENTA;
    else if (!strcasecmp(colorname, "black"))
	mcolor = COLOR_BLACK;
    else {
	rcfile_error(_("color %s not understood.\n"
		       "Valid colors are \"green\", \"red\", \"blue\", \n"
		       "\"white\", \"yellow\", \"cyan\", \"magenta\" and \n"
		       "\"black\", with the optional prefix \"bright\".\n"));
	exit(1);
    }

    return mcolor;
}

char *parse_next_regex(char *ptr)
{
    while ((*ptr != '"' || (*(ptr + 1) != ' ' && *(ptr + 1) != '\n'))
	   && *ptr != '\n' && *ptr != '\0')
	ptr++;

    if (*ptr == '\0')
	return NULL;

    /* Null terminate and advance ptr */
    *ptr++ = '\0';

    while (*ptr == ' ' || *ptr == '\t')
	ptr++;

    return ptr;
}

void parse_syntax(char *ptr)
{
    syntaxtype *tmpsyntax = NULL;
    const char *fileregptr = NULL, *nameptr = NULL;
    exttype *exttmp = NULL;

    while (*ptr == ' ')
	ptr++;

    if (*ptr == '\n' || *ptr == '\0')
	return;

    if (*ptr != '"') {
	rcfile_error(_("regex strings must begin and end with a \" character\n"));
	return;
    }
    ptr++;

    nameptr = ptr;
    ptr = parse_next_regex(ptr);

    if (ptr == NULL) {
	rcfile_error(_("Missing syntax name"));
	return;
    }

    if (syntaxes == NULL) {
	syntaxes = (syntaxtype *)nmalloc(sizeof(syntaxtype));
	tmpsyntax = syntaxes;
	SET(COLOR_SYNTAX);
    } else {
	for (tmpsyntax = syntaxes; tmpsyntax->next != NULL;
		tmpsyntax = tmpsyntax->next)
	    ;
	tmpsyntax->next = (syntaxtype *)nmalloc(sizeof(syntaxtype));
	tmpsyntax = tmpsyntax->next;
#ifdef DEBUG
	fprintf(stderr, _("Adding new syntax after 1st\n"));
#endif
    }
    tmpsyntax->desc = mallocstrcpy(NULL, nameptr);
    tmpsyntax->color = NULL;
    tmpsyntax->extensions = NULL;
    tmpsyntax->next = NULL;
#ifdef DEBUG
    fprintf(stderr, _("Starting a new syntax type\n"));
    fprintf(stderr, "string val=%s\n", nameptr);
#endif

    /* Now load in the extensions to their part of the struct */
    while (*ptr != '\n' && *ptr != '\0') {
	while (*ptr != '"' && *ptr != '\n' && *ptr != '\0')
	    ptr++;

	if (*ptr == '\n' || *ptr == '\0')
	    return;
	ptr++;

	fileregptr = ptr;
	ptr = parse_next_regex(ptr);

	if (tmpsyntax->extensions == NULL) {
	    tmpsyntax->extensions = (exttype *)nmalloc(sizeof(exttype));
	    exttmp = tmpsyntax->extensions;
	} else {
	    for (exttmp = tmpsyntax->extensions; exttmp->next != NULL;
		 exttmp = exttmp->next);
	    exttmp->next = (exttype *)nmalloc(sizeof(exttype));
	    exttmp = exttmp->next;
	}
	exttmp->val = mallocstrcpy(NULL, fileregptr);
	exttmp->next = NULL;
    }
}

/* Parse the color stuff into the colorstrings array */
void parse_colors(char *ptr)
{
    int fg, bg, bright = 0;
    int expectend = 0;		/* Do we expect an end= line? */
    char *fgstr;
    colortype *tmpcolor = NULL;
    syntaxtype *tmpsyntax = NULL;

    fgstr = ptr;
    ptr = parse_next_word(ptr);

    if (ptr == NULL) {
	rcfile_error(_("Missing color name"));
	return;
    }

    if (strstr(fgstr, ",")) {
	strtok(fgstr, ",");
	bg = colortoint(strtok(NULL, ","), &bright);
    } else
	bg = -1;

    fg = colortoint(fgstr, &bright);

    if (syntaxes == NULL) {
	rcfile_error(_("Cannot add a color directive without a syntax line"));
	return;
    }

    for (tmpsyntax = syntaxes; tmpsyntax->next != NULL;
	 tmpsyntax = tmpsyntax->next)
	;

    /* Now the fun part, start adding regexps to individual strings
       in the colorstrings array, woo! */

    while (*ptr != '\0') {
	while (*ptr == ' ')
	    ptr++;

	if (*ptr == '\n' || *ptr == '\0')
	    break;

	if (!strncasecmp(ptr, "start=", 6)) {
	    ptr += 6;
	    expectend = 1;
	}

	if (*ptr != '"') {
	    rcfile_error(_("regex strings must begin and end with a \" character\n"));
	    ptr = parse_next_regex(ptr);
	    continue;
	}
	ptr++;

	if (tmpsyntax->color == NULL) {
	    tmpsyntax->color = nmalloc(sizeof(colortype));
	    tmpcolor = tmpsyntax->color;
#ifdef DEBUG
	    fprintf(stderr, _("Starting a new colorstring for fg %d bg %d\n"),
		    fg, bg);
#endif
	} else {
	    for (tmpcolor = tmpsyntax->color;
		 tmpcolor->next != NULL; tmpcolor = tmpcolor->next);
#ifdef DEBUG
	    fprintf(stderr, _("Adding new entry for fg %d bg %d\n"), fg, bg);
#endif
	    tmpcolor->next = nmalloc(sizeof(colortype));
	    tmpcolor = tmpcolor->next;
	}
	tmpcolor->fg = fg;
	tmpcolor->bg = bg;
	tmpcolor->bright = bright;
	tmpcolor->next = NULL;

	tmpcolor->start = ptr;
	ptr = parse_next_regex(ptr);
	tmpcolor->start = mallocstrcpy(NULL, tmpcolor->start);
#ifdef DEBUG
	fprintf(stderr, _("string val=%s\n"), tmpcolor->start);
#endif

	if (!expectend)
	    tmpcolor->end = NULL;
	else {
	    if (ptr == NULL || strncasecmp(ptr, "end=", 4)) {
		rcfile_error(_
			     ("\n\t\"start=\" requires a corresponding \"end=\""));
		return;
	    }

	    ptr += 4;

	    if (*ptr != '"') {
		rcfile_error(_
			     ("regex strings must begin and end with a \" character\n"));
		continue;
	    }
	    ptr++;

	    tmpcolor->end = ptr;
	    ptr = parse_next_regex(ptr);
	    tmpcolor->end = mallocstrcpy(NULL, tmpcolor->end);
#ifdef DEBUG
	    fprintf(stderr, _("For end part, beginning = \"%s\"\n"),
		    tmpcolor->end);
#endif
	}
    }
}

#endif /* ENABLE_COLOR */

/* Parse the RC file, once it has been opened successfully */
void parse_rcfile(FILE *rcstream)
{
    char *buf, *ptr, *keyword, *option;
    int set = 0, i, j;

    buf = charalloc(1024);
    while (fgets(buf, 1023, rcstream) != 0) {
	lineno++;
	ptr = buf;
	while (*ptr == ' ' || *ptr == '\t')
	    ptr++;

	if (*ptr == '\n' || *ptr == '\0')
	    continue;

	if (*ptr == '#') {
#ifdef DEBUG
	    fprintf(stderr, _("parse_rcfile: Read a comment\n"));
#endif
	    continue;		/* Skip past commented lines */
	}

	/* Else skip to the next space */
	keyword = ptr;
	ptr = parse_next_word(ptr);
	if (!ptr)
	    continue;

	/* Else try to parse the keyword */
	if (!strcasecmp(keyword, "set"))
	    set = 1;
	else if (!strcasecmp(keyword, "unset"))
	    set = -1;
#ifdef ENABLE_COLOR
	else if (!strcasecmp(keyword, "syntax"))
	    parse_syntax(ptr);
	else if (!strcasecmp(keyword, "color"))
	    parse_colors(ptr);
#endif				/* ENABLE_COLOR */
	else {
	    rcfile_msg(_("command %s not understood"), keyword);
	    continue;
	}

	option = ptr;
	ptr = parse_next_word(ptr);
	/* We don't care if ptr == NULL, as it should if using proper syntax */

	if (set != 0) {
	    for (i = 0; rcopts[i].name != NULL; i++) {
		if (!strcasecmp(option, rcopts[i].name)) {
#ifdef DEBUG
		    fprintf(stderr, _("parse_rcfile: Parsing option %s\n"),
			    rcopts[i].name);
#endif
		    if (set == 1 || rcopts[i].flag == FOLLOW_SYMLINKS) {
			if (!strcasecmp(rcopts[i].name, "tabsize")
#ifndef DISABLE_OPERATINGDIR
				|| !strcasecmp(rcopts[i].name, "operatingdir")
#endif
#ifndef DISABLE_WRAPJUSTIFY
				|| !strcasecmp(rcopts[i].name, "fill")
#endif
#ifndef DISABLE_JUSTIFY
				|| !strcasecmp(rcopts[i].name, "quotestr")
#endif
#ifndef DISABLE_SPELLER
				|| !strcasecmp(rcopts[i].name, "speller")
#endif
				) {
			    if (*ptr == '\n' || *ptr == '\0') {
				rcfile_error(_
					     ("option %s requires an argument"),
					     rcopts[i].name);
				continue;
			    }
			    option = ptr;
			    if (*option == '"')
				option++;
			    ptr = parse_argument(ptr);
#ifdef DEBUG
			    fprintf(stderr, "option = %s\n", option);
#endif
#ifndef DISABLE_OPERATINGDIR
			    if (!strcasecmp(rcopts[i].name, "operatingdir"))
				operating_dir = mallocstrcpy(NULL, option);
			    else
#endif
#ifndef DISABLE_WRAPJUSTIFY
			    if (!strcasecmp(rcopts[i].name, "fill")) {
				char *first_error;

				/* Using strtol instead of atoi lets us
				 * accept 0 while checking other
				 * errors. */
				j = (int)strtol(option, &first_error, 10);
				if (errno == ERANGE || *option == '\0' || *first_error != '\0')
				    rcfile_error(_("requested fill size %d invalid"),
						 j);
				else
				    wrap_at = j;
			    } else
#endif
#ifndef DISABLE_JUSTIFY
			    if (!strcasecmp(rcopts[i].name, "quotestr"))
				quotestr = mallocstrcpy(NULL, option);
			    else
#endif
#ifndef DISABLE_SPELLER
			    if (!strcasecmp(rcopts[i].name, "speller"))
				alt_speller = mallocstrcpy(NULL, option);
			    else
#endif
			    {
				char *first_error;

				/* Using strtol instead of atoi lets us
				 * accept 0 while checking other
				 * errors. */
				j = (int)strtol(option, &first_error, 10);
				if (errno == ERANGE || *option == '\0' || *first_error != '\0')
				    rcfile_error(_("requested tab size %d invalid"),
						 j);
				else
				    tabsize = j;
			    }
			} else
			    SET(rcopts[i].flag);
#ifdef DEBUG
			fprintf(stderr, _("set flag %d!\n"),
				rcopts[i].flag);
#endif
		    } else {
			UNSET(rcopts[i].flag);
#ifdef DEBUG
			fprintf(stderr, _("unset flag %d!\n"),
				rcopts[i].flag);
#endif
		    }
		}
	    }
	}
    }
    free(buf);
    if (errors)
	rcfile_error(_("Errors found in .nanorc file"));

    return;
}

/* The main rc file function, tries to open the rc file */
void do_rcfile(void)
{
    FILE *rcstream;
    const struct passwd *userage;
    uid_t euid = geteuid();

#ifdef SYSCONFDIR
    assert(sizeof(SYSCONFDIR) == strlen(SYSCONFDIR) + 1);
    nanorc = charalloc(sizeof(SYSCONFDIR) + 7);
    sprintf(nanorc, "%s/nanorc", SYSCONFDIR);
    /* Try to open system nanorc */
    if ((rcstream = fopen(nanorc, "r")) != NULL) {
	/* Parse it! */
	parse_rcfile(rcstream);
	fclose(rcstream);
    }
#endif

    /* Determine home directory using getpwent(), don't rely on $HOME */
    do {
	userage = getpwent();
    } while (userage != NULL && userage->pw_uid != euid);
    endpwent();

    lineno = 0;

    if (userage == NULL)
	rcfile_error(_("I can't find my home directory!  Wah!"));
    else {
	nanorc = nrealloc(nanorc, strlen(userage->pw_dir) + 9);
	sprintf(nanorc, "%s/.nanorc", userage->pw_dir);

#if defined(DISABLE_ROOTWRAP) && !defined(DISABLE_WRAPPING)
    /* If we've already read $SYSCONFDIR/nanorc (if it's there), we're
       root, and --disable-wrapping-as-root is used, turn wrapping off */
	if (euid == 0)
	    SET(NO_WRAP);
#endif
	if ((rcstream = fopen(nanorc, "r")) == NULL) {
	    /* Don't complain about the file not existing */
	    if (errno != ENOENT)
		rcfile_error(_("Unable to open ~/.nanorc file, %s"),
			strerror(errno));
	} else {
	    parse_rcfile(rcstream);
	    fclose(rcstream);
	}
    }

    free(nanorc);
#ifdef ENABLE_COLOR
    set_colorpairs();
#endif
}

#endif /* ENABLE_NANORC */
