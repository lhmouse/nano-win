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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

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
#ifdef HAVE_REGEX_H
    {"regexp", USE_REGEXP},
#endif
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
static const char *nanorc;

/* We have an error in some part of the rcfile; put it on stderr and
   make the user hit return to continue starting up nano. */
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

/* Parse the next word from the string.  Returns NULL if we hit EOL. */
char *parse_next_word(char *ptr)
{
    while (!is_blank_char(*ptr) && *ptr != '\n' && *ptr != '\0')
	ptr++;

    if (*ptr == '\0')
	return NULL;

    /* Null terminate and advance ptr */
    *ptr++ = 0;

    while (is_blank_char(*ptr))
	ptr++;

    return ptr;
}

/* The keywords operatingdir, backupdir, fill, tabsize, speller,
 * punct, brackets, quotestr, and whitespace take an argument when set.
 * Among these, operatingdir, backupdir, speller, punct, brackets,
 * quotestr, and whitespace have to allow tabs and spaces in the
 * argument.  Thus, if the next word starts with a ", we say it ends
 * with the last " of the line.  Otherwise, the word is interpreted as
 * usual.  That is so the arguments can contain "s too. */
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

int colortoint(const char *colorname, int *bright)
{
    int mcolor = 0;

    if (colorname == NULL)
	return -1;

    if (strncasecmp(colorname, "bright", 6) == 0) {
	*bright = 1;
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
    else {
	rcfile_error(N_("Color %s not understood.\n"
		"Valid colors are \"green\", \"red\", \"blue\", \n"
		"\"white\", \"yellow\", \"cyan\", \"magenta\" and \n"
		"\"black\", with the optional prefix \"bright\" \n"
		"for foreground colors."), colorname);
	mcolor = -1;
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

    /* Null terminate and advance ptr. */
    *ptr++ = '\0';

    while (is_blank_char(*ptr))
	ptr++;

    return ptr;
}

/* Compile the regular expression regex to preg.  Returns FALSE on
 * success, or TRUE if the expression is invalid. */
int nregcomp(regex_t *preg, const char *regex, int eflags)
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
    syntaxtype *tmpsyntax = NULL;
    const char *fileregptr = NULL, *nameptr = NULL;
    exttype *endext = NULL;
	/* The end of the extensions list for this syntax. */

    while (*ptr == ' ')
	ptr++;

    if (*ptr == '\n' || *ptr == '\0')
	return;

    if (*ptr != '"') {
	rcfile_error(N_("Regex strings must begin and end with a \" character"));
	return;
    }
    ptr++;

    nameptr = ptr;
    ptr = parse_next_regex(ptr);

    if (ptr == NULL) {
	rcfile_error(N_("Missing syntax name"));
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
	fprintf(stderr, "Adding new syntax after 1st\n");
#endif
    }
    tmpsyntax->desc = mallocstrcpy(NULL, nameptr);
    tmpsyntax->color = NULL;
    tmpsyntax->extensions = NULL;
    tmpsyntax->next = NULL;
#ifdef DEBUG
    fprintf(stderr, "Starting a new syntax type\n");
    fprintf(stderr, "string val=%s\n", nameptr);
#endif

    /* Now load in the extensions to their part of the struct */
    while (*ptr != '\n' && *ptr != '\0') {
	exttype *newext;
	    /* The new extension structure. */

	while (*ptr != '"' && *ptr != '\n' && *ptr != '\0')
	    ptr++;

	if (*ptr == '\n' || *ptr == '\0')
	    return;
	ptr++;

	fileregptr = ptr;
	ptr = parse_next_regex(ptr);

	newext = (exttype *)nmalloc(sizeof(exttype));
	if (nregcomp(&newext->val, fileregptr, REG_NOSUB) != 0)
	    free(newext);
	else {
	    if (endext == NULL)
		tmpsyntax->extensions = newext;
	    else
		endext->next = newext;
	    endext = newext;
	    endext->next = NULL;
	}
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
	rcfile_error(N_("Missing color name"));
	return;
    }

    if (strstr(fgstr, ",")) {
	char *bgcolorname;
	strtok(fgstr, ",");
	bgcolorname = strtok(NULL, ",");
	if (strncasecmp(bgcolorname, "bright", 6) == 0) {
	    rcfile_error(N_("Background color %s cannot be bright"), bgcolorname);
	    return;
	}
	bg = colortoint(bgcolorname, &bright);
    } else
	bg = -1;

    fg = colortoint(fgstr, &bright);

    /* Don't try and parse screwed up fg colors */
    if (fg == -1)
	return;

    if (syntaxes == NULL) {
	rcfile_error(N_("Cannot add a color directive without a syntax line"));
	return;
    }

    for (tmpsyntax = syntaxes; tmpsyntax->next != NULL;
	 tmpsyntax = tmpsyntax->next)
	;

    /* Now the fun part, start adding regexps to individual strings
       in the colorstrings array, woo! */

    while (*ptr != '\0') {
	colortype *newcolor;
	    /* The new color structure. */
	int cancelled = 0;
	    /* The start expression was bad. */

	while (*ptr == ' ')
	    ptr++;

	if (*ptr == '\n' || *ptr == '\0')
	    break;

	if (strncasecmp(ptr, "start=", 6) == 0) {
	    ptr += 6;
	    expectend = 1;
	}

	if (*ptr != '"') {
	    rcfile_error(N_("Regex strings must begin and end with a \" character"));
	    ptr = parse_next_regex(ptr);
	    continue;
	}
	ptr++;

	newcolor = (colortype *)nmalloc(sizeof(colortype));
	fgstr = ptr;
	ptr = parse_next_regex(ptr);
	if (nregcomp(&newcolor->start, fgstr, 0) != 0) {
	    free(newcolor);
	    cancelled = 1;
	} else {
	    newcolor->fg = fg;
	    newcolor->bg = bg;
	    newcolor->bright = bright;
	    newcolor->next = NULL;
	    newcolor->end = NULL;

	    if (tmpsyntax->color == NULL) {
		tmpsyntax->color = newcolor;
#ifdef DEBUG
		fprintf(stderr, "Starting a new colorstring for fg %d bg %d\n", fg, bg);
#endif
	    } else {
		for (tmpcolor = tmpsyntax->color; tmpcolor->next != NULL;
			tmpcolor = tmpcolor->next)
		    ;
#ifdef DEBUG
		fprintf(stderr, "Adding new entry for fg %d bg %d\n", fg, bg);
#endif
		tmpcolor->next = newcolor;
	    }
	}

	if (expectend) {
	    if (ptr == NULL || strncasecmp(ptr, "end=", 4) != 0) {
		rcfile_error(N_("\"start=\" requires a corresponding \"end=\""));
		return;
	    }

	    ptr += 4;

	    if (*ptr != '"') {
		rcfile_error(N_("Regex strings must begin and end with a \" character"));
		continue;
	    }
	    ptr++;

	    fgstr = ptr;
	    ptr = parse_next_regex(ptr);

	    /* If the start regex was invalid, skip past the end regex to
	     * stay in sync. */
	    if (cancelled)
		continue;
	    newcolor->end = (regex_t *)nmalloc(sizeof(regex_t));
	    if (nregcomp(newcolor->end, fgstr, 0) != 0) {
		free(newcolor->end);
		newcolor->end = NULL;
	    }
	}
    }
}

#endif /* ENABLE_COLOR */

/* Parse the RC file, once it has been opened successfully */
void parse_rcfile(FILE *rcstream)
{
    char *buf, *ptr, *keyword, *option;
    int set = 0, i;

    buf = charalloc(1024);
    while (fgets(buf, 1023, rcstream) != 0) {
	lineno++;
	ptr = buf;
	while (is_blank_char(*ptr))
	    ptr++;

	if (*ptr == '\n' || *ptr == '\0')
	    continue;

	if (*ptr == '#') {
#ifdef DEBUG
	    fprintf(stderr, "%s: Read a comment\n", "parse_rcfile()");
#endif
	    continue;		/* Skip past commented lines */
	}

	/* Else skip to the next space */
	keyword = ptr;
	ptr = parse_next_word(ptr);
	if (ptr == NULL)
	    continue;

	/* Else try to parse the keyword */
	if (strcasecmp(keyword, "set") == 0)
	    set = 1;
	else if (strcasecmp(keyword, "unset") == 0)
	    set = -1;
#ifdef ENABLE_COLOR
	else if (strcasecmp(keyword, "syntax") == 0)
	    parse_syntax(ptr);
	else if (strcasecmp(keyword, "color") == 0)
	    parse_colors(ptr);
#endif				/* ENABLE_COLOR */
	else {
	    rcfile_error(N_("Command %s not understood"), keyword);
	    continue;
	}

	option = ptr;
	ptr = parse_next_word(ptr);
	/* We don't care if ptr == NULL, as it should if using proper syntax */

	if (set != 0) {
	    for (i = 0; rcopts[i].name != NULL; i++) {
		if (strcasecmp(option, rcopts[i].name) == 0) {
#ifdef DEBUG
		    fprintf(stderr, "%s: Parsing option %s\n", 
			    "parse_rcfile()", rcopts[i].name);
#endif
		    if (set == 1) {
			if (strcasecmp(rcopts[i].name, "tabsize") == 0
#ifndef DISABLE_OPERATINGDIR
				|| strcasecmp(rcopts[i].name, "operatingdir") == 0
#endif
#ifndef DISABLE_WRAPJUSTIFY
				|| strcasecmp(rcopts[i].name, "fill") == 0
#endif
#ifndef NANO_SMALL
				|| strcasecmp(rcopts[i].name, "whitespace") == 0
#endif
#ifndef DISABLE_JUSTIFY
				|| strcasecmp(rcopts[i].name, "punct") == 0
				|| strcasecmp(rcopts[i].name, "brackets") == 0
				|| strcasecmp(rcopts[i].name, "quotestr") == 0
#endif
#ifndef NANO_SMALL
			        || strcasecmp(rcopts[i].name, "backupdir") == 0
#endif
#ifndef DISABLE_SPELLER
				|| strcasecmp(rcopts[i].name, "speller") == 0
#endif
				) {
			    if (*ptr == '\n' || *ptr == '\0') {
				rcfile_error(N_("Option %s requires an argument"), rcopts[i].name);
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
			    if (strcasecmp(rcopts[i].name, "operatingdir") == 0)
				operating_dir = mallocstrcpy(NULL, option);
			    else
#endif
#ifndef DISABLE_WRAPJUSTIFY
			    if (strcasecmp(rcopts[i].name, "fill") == 0) {
				if (!parse_num(option, &wrap_at)) {
				    rcfile_error(N_("Requested fill size %s invalid"), option);
				    wrap_at = -CHARS_FROM_EOL;
				}
			    } else
#endif
#ifndef NANO_SMALL
			    if (strcasecmp(rcopts[i].name, "whitespace") == 0) {
				size_t ws_len;
				whitespace = mallocstrcpy(NULL, option);
				ws_len = strlen(whitespace);
				if (ws_len != 2 || (ws_len == 2 && (is_cntrl_char(whitespace[0]) || is_cntrl_char(whitespace[1])))) {
				    rcfile_error(N_("Two non-control characters required"));
				    free(whitespace);
				    whitespace = NULL;
				}
			    } else
#endif
#ifndef DISABLE_JUSTIFY
			    if (strcasecmp(rcopts[i].name, "punct") == 0) {
				punct = mallocstrcpy(NULL, option);
				if (strchr(punct, '\t') != NULL || strchr(punct, ' ') != NULL) {
				    rcfile_error(N_("Non-tab and non-space characters required"));
				    free(punct);
				    punct = NULL;
				}
			    } else if (strcasecmp(rcopts[i].name, "brackets") == 0) {
				brackets = mallocstrcpy(NULL, option);
				if (strchr(brackets, '\t') != NULL || strchr(brackets, ' ') != NULL) {
				    rcfile_error(N_("Non-tab and non-space characters required"));
				    free(brackets);
				    brackets = NULL;
				}
			    } else if (strcasecmp(rcopts[i].name, "quotestr") == 0)
				quotestr = mallocstrcpy(NULL, option);
			    else
#endif
#ifndef NANO_SMALL
			    if (strcasecmp(rcopts[i].name, "backupdir") == 0)
				backup_dir = mallocstrcpy(NULL, option);
			    else
#endif
#ifndef DISABLE_SPELLER
			    if (strcasecmp(rcopts[i].name, "speller") == 0)
				alt_speller = mallocstrcpy(NULL, option);
			    else
#endif
			    if (strcasecmp(rcopts[i].name, "tabsize") == 0) {
				if (!parse_num(option, &tabsize) || tabsize <= 0) {
				    rcfile_error(N_("Requested tab size %s invalid"), option);
				    tabsize = -1;
				}
			    }
			} else
			    SET(rcopts[i].flag);
#ifdef DEBUG
			fprintf(stderr, "set flag %ld!\n",
				rcopts[i].flag);
#endif
		    } else {
			UNSET(rcopts[i].flag);
#ifdef DEBUG
			fprintf(stderr, "unset flag %ld!\n",
				rcopts[i].flag);
#endif
		    }
		}
	    }
	}
    }
    free(buf);

    if (errors) {
	errors = FALSE;
	fprintf(stderr, _("\nPress Return to continue starting nano\n"));
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

    nanorc = SYSCONFDIR "/nanorc";
    /* Try to open system nanorc */
    rcstream = fopen(nanorc, "r");
    if (rcstream != NULL) {
	/* Parse it! */
	parse_rcfile(rcstream);
	fclose(rcstream);
    }
#endif

    lineno = 0;

    get_homedir();

    if (homedir == NULL) {
	rcfile_error(N_("I can't find my home directory!  Wah!"));
	SET(NO_RCFILE);
    } else {
	size_t homelen = strlen(homedir);
	char *nanorcf = charalloc(homelen + 9);

	nanorc = nanorcf;
	strcpy(nanorcf, homedir);
	strcpy(nanorcf + homelen, "/.nanorc");

#if defined(DISABLE_ROOTWRAP) && !defined(DISABLE_WRAPPING)
    /* If we've already read SYSCONFDIR/nanorc (if it's there), we're
       root, and --disable-wrapping-as-root is used, turn wrapping off */
	if (geteuid() == NANO_ROOT_UID)
	    SET(NO_WRAP);
#endif
	rcstream = fopen(nanorc, "r");
	if (rcstream == NULL) {
	    /* Don't complain about the file not existing */
	    if (errno != ENOENT) {
		rcfile_error(N_("Error reading %s: %s"), nanorc, strerror(errno));
		SET(NO_RCFILE);
	    }
	} else {
	    parse_rcfile(rcstream);
	    fclose(rcstream);
	}
	free(nanorcf);
    }

    lineno = 0;

#ifdef ENABLE_COLOR
    set_colorpairs();
#endif
}

#endif /* ENABLE_NANORC */
