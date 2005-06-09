/* $Id$ */
/**************************************************************************
 *   rcfile.c                                                             *
 *                                                                        *
 *   Copyright (C) 1999-2005 Chris Allegretta                             *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 2, or (at your option)  *
 *   any later version.                                                   *
 *                                                                        *
 *   This program is distributed in the hope that it will be useful, but  *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    *
 *   General Public License for more details.                             *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program; if not, write to the Free Software          *
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA            *
 *   02110-1301, USA.                                                     *
 *                                                                        *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include "proto.h"

#ifdef ENABLE_NANORC

const static rcoption rcopts[] = {
#ifndef NANO_SMALL
    {"autoindent", AUTOINDENT},
    {"backup", BACKUP_FILE},
    {"backupdir", 0},
#endif
#ifndef DISABLE_JUSTIFY
    {"brackets", 0},
#endif
    {"const", CONSTUPDATE},
#ifndef NANO_SMALL
    {"cut", CUT_TO_END},
#endif
#ifndef DISABLE_WRAPJUSTIFY
    {"fill", 0},
#endif
#ifndef NANO_SMALL
    {"historylog", HISTORYLOG},
#endif
#ifndef DISABLE_MOUSE
    {"mouse", USE_MOUSE},
#endif
#ifdef ENABLE_MULTIBUFFER
    {"multibuffer", MULTIBUFFER},
#endif
    {"morespace", MORE_SPACE},
#ifndef NANO_SMALL
    {"noconvert", NO_CONVERT},
#endif
    {"nofollow", NOFOLLOW_SYMLINKS},
    {"nohelp", NO_HELP},
#ifndef DISABLE_WRAPPING
    {"nowrap", NO_WRAP},
#endif
#ifndef DISABLE_OPERATINGDIR
    {"operatingdir", 0},
#endif
    {"preserve", PRESERVE},
#ifndef DISABLE_JUSTIFY
    {"punct", 0},
    {"quotestr", 0},
#endif
    {"rebinddelete", REBIND_DELETE},
#ifndef NANO_SMALL
    {"smarthome", SMART_HOME},
    {"smooth", SMOOTHSCROLL},
#endif
#ifndef DISABLE_SPELLER
    {"speller", 0},
#endif
    {"suspend", SUSPEND},
    {"tabsize", 0},
    {"tempfile", TEMP_FILE},
    {"view", VIEW_MODE},
#ifndef NANO_SMALL
    {"whitespace", 0},
#endif
    {NULL, 0}
};

static bool errors = FALSE;
static int lineno = 0;
static char *nanorc = NULL;
#ifdef ENABLE_COLOR
static syntaxtype *endsyntax = NULL;
	/* The end of the list of syntaxes. */
static colortype *endcolor = NULL;
	/* The end of the color list for the current syntax. */
#endif

/* We have an error in some part of the rcfile.  Put it on stderr and
 * make the user hit Return to continue starting up nano. */
void rcfile_error(const char *msg, ...)
{
    va_list ap;

    fprintf(stderr, "\n");
    if (lineno > 0) {
	errors = TRUE;
	fprintf(stderr, _("Error in %s on line %d: "), nanorc, lineno);
    }

    va_start(ap, msg);
    vfprintf(stderr, _(msg), ap);
    va_end(ap);

    fprintf(stderr, "\n");
}

/* Parse the next word from the string.  Return points to '\0' if we hit
 * the end of the line. */
char *parse_next_word(char *ptr)
{
    while (!is_blank_char(*ptr) && *ptr != '\0')
	ptr++;

    if (*ptr == '\0')
	return ptr;

    /* Null-terminate and advance ptr. */
    *ptr++ = '\0';

    while (is_blank_char(*ptr))
	ptr++;

    return ptr;
}

/* The keywords operatingdir, backupdir, fill, tabsize, speller, punct,
 * brackets, quotestr, and whitespace take an argument when set.  Among
 * these, operatingdir, backupdir, speller, punct, brackets, quotestr,
 * and whitespace have to allow tabs and spaces in the argument.  Thus,
 * if the next word starts with a ", we say it ends with the last " of
 * the line.  Otherwise, the word is interpreted as usual.  That is so
 * the arguments can contain "s too. */
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
    } while (*ptr != '\0');

    if (last_quote == NULL) {
	if (*ptr == '\0')
	    ptr = NULL;
	else
	    *ptr++ = '\0';
	rcfile_error(N_("Argument %s has unterminated \""), ptr_bak);
    } else {
	*last_quote = '\0';
	ptr = last_quote + 1;
    }
    if (ptr != NULL)
	while (is_blank_char(*ptr))
	    ptr++;
    return ptr;
}

#ifdef ENABLE_COLOR
int color_to_int(const char *colorname, bool *bright)
{
    int mcolor = -1;

    if (colorname == NULL) {
	rcfile_error(N_("Missing color name"));
	return -1;
    }

    assert(bright != NULL);

    if (strncasecmp(colorname, "bright", 6) == 0) {
	*bright = TRUE;
	colorname += 6;
    }

    if (strcasecmp(colorname, "green") == 0)
	mcolor = COLOR_GREEN;
    else if (strcasecmp(colorname, "red") == 0)
	mcolor = COLOR_RED;
    else if (strcasecmp(colorname, "blue") == 0)
	mcolor = COLOR_BLUE;
    else if (strcasecmp(colorname, "white") == 0)
	mcolor = COLOR_WHITE;
    else if (strcasecmp(colorname, "yellow") == 0)
	mcolor = COLOR_YELLOW;
    else if (strcasecmp(colorname, "cyan") == 0)
	mcolor = COLOR_CYAN;
    else if (strcasecmp(colorname, "magenta") == 0)
	mcolor = COLOR_MAGENTA;
    else if (strcasecmp(colorname, "black") == 0)
	mcolor = COLOR_BLACK;
    else
	rcfile_error(N_("Color %s not understood.\n"
		"Valid colors are \"green\", \"red\", \"blue\",\n"
		"\"white\", \"yellow\", \"cyan\", \"magenta\" and\n"
		"\"black\", with the optional prefix \"bright\"\n"
		"for foreground colors."), colorname);

    return mcolor;
}

char *parse_next_regex(char *ptr)
{
    assert(ptr != NULL);

    /* Continue until the end of the line, or a " followed by a space, a
     * blank character, or \0. */
    while ((*ptr != '"' || (!is_blank_char(*(ptr + 1)) &&
	*(ptr + 1) != '\0')) && *ptr != '\0')
	ptr++;

    assert(*ptr == '"' || *ptr == '\0');

    if (*ptr == '\0') {
	rcfile_error(
		N_("Regex strings must begin and end with a \" character"));
	return NULL;
    }

    /* Null terminate and advance ptr. */
    *ptr++ = '\0';

    while (is_blank_char(*ptr))
	ptr++;

    return ptr;
}

/* Compile the regular expression regex to preg.  Return FALSE on
 * success, or TRUE if the expression is invalid. */
bool nregcomp(regex_t *preg, const char *regex, int eflags)
{
    int rc = regcomp(preg, regex, REG_EXTENDED | eflags);

    if (rc != 0) {
	size_t len = regerror(rc, preg, NULL, 0);
	char *str = charalloc(len);

	regerror(rc, preg, str, len);
	rcfile_error(N_("Bad regex \"%s\": %s"), regex, str);
	free(str);
    }

    return (rc != 0);
}

void parse_syntax(char *ptr)
{
    const char *fileregptr = NULL, *nameptr = NULL;
    exttype *endext = NULL;
	/* The end of the extensions list for this syntax. */

    assert(ptr != NULL);

    if (*ptr == '\0') {
	rcfile_error(N_("Missing syntax name"));
	return;
    }

    if (*ptr != '"') {
	rcfile_error(
		N_("Regex strings must begin and end with a \" character"));
	return;
    }

    ptr++;

    nameptr = ptr;
    ptr = parse_next_regex(ptr);

    if (ptr == NULL)
	return;

    if (syntaxes == NULL) {
	syntaxes = (syntaxtype *)nmalloc(sizeof(syntaxtype));
	endsyntax = syntaxes;
    } else {
	endsyntax->next = (syntaxtype *)nmalloc(sizeof(syntaxtype));
	endsyntax = endsyntax->next;
#ifdef DEBUG
	fprintf(stderr, "Adding new syntax after first one\n");
#endif
    }
    endsyntax->desc = mallocstrcpy(NULL, nameptr);
    endsyntax->color = NULL;
    endcolor = NULL;
    endsyntax->extensions = NULL;
    endsyntax->next = NULL;
#ifdef DEBUG
    fprintf(stderr, "Starting a new syntax type: \"%s\"\n", nameptr);
#endif

    /* Now load the extensions into their part of the struct. */
    while (*ptr != '\0') {
	exttype *newext;
	    /* The new extension structure. */

	while (*ptr != '"' && *ptr != '\0')
	    ptr++;

	if (*ptr == '\0')
	    return;

	ptr++;

	fileregptr = ptr;
	ptr = parse_next_regex(ptr);
	if (ptr == NULL)
	    break;

	newext = (exttype *)nmalloc(sizeof(exttype));
	if (nregcomp(&newext->val, fileregptr, REG_NOSUB))
	    free(newext);
	else {
	    if (endext == NULL)
		endsyntax->extensions = newext;
	    else
		endext->next = newext;
	    endext = newext;
	    endext->next = NULL;
	}
    }
}

/* Parse the color stuff into the colorstrings array. */
void parse_colors(char *ptr)
{
    int fg, bg;
    bool bright = FALSE, no_fgcolor = FALSE;
    char *fgstr;

    assert(ptr != NULL);

    if (*ptr == '\0') {
	rcfile_error(N_("Missing color name"));
	return;
    }

    fgstr = ptr;
    ptr = parse_next_word(ptr);

    if (strchr(fgstr, ',') != NULL) {
	char *bgcolorname;

	strtok(fgstr, ",");
	bgcolorname = strtok(NULL, ",");
	if (bgcolorname == NULL) {
	    /* If we have a background color without a foreground color,
	     * parse it properly. */
	    bgcolorname = fgstr + 1;
	    no_fgcolor = TRUE;
	}
	if (strncasecmp(bgcolorname, "bright", 6) == 0) {
	    rcfile_error(
		N_("Background color %s cannot be bright"),
		bgcolorname);
	    return;
	}
	bg = color_to_int(bgcolorname, &bright);
    } else
	bg = -1;

    if (!no_fgcolor) {
	fg = color_to_int(fgstr, &bright);

	/* Don't try to parse screwed-up foreground colors. */
	if (fg == -1)
	    return;
    } else
	fg = -1;

    if (syntaxes == NULL) {
	rcfile_error(
		N_("Cannot add a color directive without a syntax line"));
	return;
    }

    if (*ptr == '\0') {
	rcfile_error(
		N_("Cannot add a color directive without a regex string"));
	return;
    }

    /* Now for the fun part.  Start adding regexes to individual strings
     * in the colorstrings array, woo! */
    while (ptr != NULL && *ptr != '\0') {
	colortype *newcolor;
	    /* The new color structure. */
	bool cancelled = FALSE;
	    /* The start expression was bad. */
	bool expectend = FALSE;
	    /* Do we expect an end= line? */

	if (strncasecmp(ptr, "start=", 6) == 0) {
	    ptr += 6;
	    expectend = TRUE;
	}

	if (*ptr != '"') {
	    rcfile_error(
		N_("Regex strings must begin and end with a \" character"));
	    ptr = parse_next_regex(ptr);
	    continue;
	}

	ptr++;

	newcolor = (colortype *)nmalloc(sizeof(colortype));
	fgstr = ptr;
	ptr = parse_next_regex(ptr);
	if (ptr == NULL)
	    break;

	if (nregcomp(&newcolor->start, fgstr, 0)) {
	    free(newcolor);
	    cancelled = TRUE;
	} else {
	    newcolor->fg = fg;
	    newcolor->bg = bg;
	    newcolor->bright = bright;
	    newcolor->next = NULL;
	    newcolor->end = NULL;

	    if (endcolor == NULL) {
		endsyntax->color = newcolor;
#ifdef DEBUG
		fprintf(stderr, "Starting a new colorstring for fg %d, bg %d\n", fg, bg);
#endif
	    } else {
#ifdef DEBUG
		fprintf(stderr, "Adding new entry for fg %d, bg %d\n", fg, bg);
#endif
		endcolor->next = newcolor;
	    }
	    endcolor = newcolor;
	}

	if (expectend) {
	    if (ptr == NULL || strncasecmp(ptr, "end=", 4) != 0) {
		rcfile_error(
			N_("\"start=\" requires a corresponding \"end=\""));
		return;
	    }
	    ptr += 4;
	    if (*ptr != '"') {
		rcfile_error(
			N_("Regex strings must begin and end with a \" character"));
		continue;
	    }

	    ptr++;

	    fgstr = ptr;
	    ptr = parse_next_regex(ptr);
	    if (ptr == NULL)
		break;

	    /* If the start regex was invalid, skip past the end regex to
	     * stay in sync. */
	    if (cancelled)
		continue;

	    newcolor->end = (regex_t *)nmalloc(sizeof(regex_t));
	    if (nregcomp(newcolor->end, fgstr, 0)) {
		free(newcolor->end);
		newcolor->end = NULL;
	    }
	}
    }
}
#endif /* ENABLE_COLOR */

/* Parse the rcfile, once it has been opened successfully. */
void parse_rcfile(FILE *rcstream)
{
    char *buf = NULL;
    ssize_t len;
    size_t n;

    while ((len = getline(&buf, &n, rcstream)) > 0) {
	char *ptr, *keyword, *option;
	int set = 0, i;

	/* Ignore the \n. */
	buf[len - 1] = '\0';

	lineno++;
	ptr = buf;
	while (is_blank_char(*ptr))
	    ptr++;

	/* If we have a blank line or a comment, skip to the next
	 * line. */
	if (*ptr == '\0' || *ptr == '#')
	    continue;

	/* Otherwise, skip to the next space. */
	keyword = ptr;
	ptr = parse_next_word(ptr);

	/* Try to parse the keyword. */
	if (strcasecmp(keyword, "set") == 0)
	    set = 1;
	else if (strcasecmp(keyword, "unset") == 0)
	    set = -1;
#ifdef ENABLE_COLOR
	else if (strcasecmp(keyword, "syntax") == 0)
	    parse_syntax(ptr);
	else if (strcasecmp(keyword, "color") == 0)
	    parse_colors(ptr);
#endif /* ENABLE_COLOR */
	else
	    rcfile_error(N_("Command %s not understood"), keyword);

	if (set == 0)
	    continue;

	if (*ptr == '\0') {
	    rcfile_error(N_("Missing flag"));
	    continue;
	}

	option = ptr;
	ptr = parse_next_word(ptr);

	for (i = 0; rcopts[i].name != NULL; i++) {
	    if (strcasecmp(option, rcopts[i].name) == 0) {
#ifdef DEBUG
		fprintf(stderr, "parse_rcfile(): name = \"%s\"\n", rcopts[i].name);
#endif
		if (set == 1) {
		    if (rcopts[i].flag != 0)
			/* This option has a flag, so it doesn't take an
			 * argument. */
			SET(rcopts[i].flag);
		    else {
			/* This option doesn't have a flag, so it takes
			 * an argument. */
			if (*ptr == '\0') {
			    rcfile_error(
				N_("Option %s requires an argument"),
				rcopts[i].name);
			    break;
			}
			option = ptr;
			if (*option == '"')
			    option++;
			ptr = parse_argument(ptr);

			/* Make sure option is a valid multibyte
			 * string. */
			option = make_mbstring(option);
#ifdef DEBUG
			fprintf(stderr, "option = \"%s\"\n", option);
#endif
#ifndef DISABLE_OPERATINGDIR
			if (strcasecmp(rcopts[i].name, "operatingdir") == 0)
			    operating_dir = option;
			else
#endif
#ifndef DISABLE_WRAPJUSTIFY
			if (strcasecmp(rcopts[i].name, "fill") == 0) {
			    if (!parse_num(option, &wrap_at)) {
				rcfile_error(
					N_("Requested fill size %s invalid"),
					option);
				wrap_at = -CHARS_FROM_EOL;
			    } else
				free(option);
			} else
#endif
#ifndef NANO_SMALL
			if (strcasecmp(rcopts[i].name, "whitespace") == 0) {
			    whitespace = option;
			    if (mbstrlen(whitespace) != 2 || strlenpt(whitespace) != 2) {
				rcfile_error(
					N_("Two single-column characters required"));
				free(whitespace);
				whitespace = NULL;
			    } else {
				whitespace_len[0] =
					parse_mbchar(whitespace, NULL,
					NULL, NULL);
				whitespace_len[1] =
					parse_mbchar(whitespace +
					whitespace_len[0], NULL,
					NULL, NULL);
			    }
			} else
#endif
#ifndef DISABLE_JUSTIFY
			if (strcasecmp(rcopts[i].name, "punct") == 0) {
			    punct = option;
			    if (strchr(punct, '\t') != NULL ||
				strchr(punct, ' ') != NULL) {
				rcfile_error(
					N_("Non-tab and non-space characters required"));
				free(punct);
				punct = NULL;
			    }
			} else if (strcasecmp(rcopts[i].name,
				"brackets") == 0) {
			    brackets = option;
			    if (strchr(brackets, '\t') != NULL ||
				strchr(brackets, ' ') != NULL) {
				rcfile_error(
					N_("Non-tab and non-space characters required"));
				free(brackets);
				brackets = NULL;
			    }
			} else if (strcasecmp(rcopts[i].name,
				"quotestr") == 0)
			    quotestr = option;
			else
#endif
#ifndef NANO_SMALL
			if (strcasecmp(rcopts[i].name,
				"backupdir") == 0)
			    backup_dir = option;
			else
#endif
#ifndef DISABLE_SPELLER
			if (strcasecmp(rcopts[i].name, "speller") == 0)
			    alt_speller = option;
			else
#endif
			if (strcasecmp(rcopts[i].name,
				"tabsize") == 0) {
			    if (!parse_num(option, &tabsize) ||
				tabsize <= 0) {
				rcfile_error(
					N_("Requested tab size %s invalid"),
					option);
				tabsize = -1;
			    } else
				free(option);
			} else
			    assert(FALSE);
		    }
#ifdef DEBUG
		    fprintf(stderr, "flag = %ld\n", rcopts[i].flag);
#endif
		} else if (rcopts[i].flag != 0)
		    UNSET(rcopts[i].flag);
		else
		    rcfile_error(N_("Cannot unset flag %s"),
			rcopts[i].name);
		break;
	    }
	}
	if (rcopts[i].name == NULL)
	    rcfile_error(N_("Unknown flag %s"), option);
    }

    free(buf);
    fclose(rcstream);
    lineno = 0;

    if (errors) {
	errors = FALSE;
	fprintf(stderr,
		_("\nPress Return to continue starting nano\n"));
	while (getchar() != '\n')
	    ;
    }

    return;
}

/* The main rc file function, tries to open the rc file */
void do_rcfile(void)
{
    FILE *rcstream;

#ifdef SYSCONFDIR
    assert(sizeof(SYSCONFDIR) == strlen(SYSCONFDIR) + 1);

    nanorc = mallocstrcpy(nanorc, SYSCONFDIR "/nanorc");
    /* Try to open the system-wide nanorc. */
    rcstream = fopen(nanorc, "r");
    if (rcstream != NULL)
	parse_rcfile(rcstream);
#endif

#if defined(DISABLE_ROOTWRAP) && !defined(DISABLE_WRAPPING)
    /* We've already read SYSCONFDIR/nanorc, if it's there.  If we're
     * root and --disable-wrapping-as-root is used, turn wrapping
     * off now. */
    if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif

    get_homedir();

    if (homedir == NULL)
	rcfile_error(N_("I can't find my home directory!  Wah!"));
    else {
	nanorc = charealloc(nanorc, strlen(homedir) + 9);
	sprintf(nanorc, "%s/.nanorc", homedir);
	rcstream = fopen(nanorc, "r");

	if (rcstream == NULL) {
	    /* Don't complain about the file's not existing. */
	    if (errno != ENOENT)
		rcfile_error(N_("Error reading %s: %s"), nanorc,
			strerror(errno));
	} else
	    parse_rcfile(rcstream);
    }

    free(nanorc);
    nanorc = NULL;

#ifdef ENABLE_COLOR
    set_colorpairs();
#endif
}

#endif /* ENABLE_NANORC */
