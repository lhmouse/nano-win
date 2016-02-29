/* $Id$ */
/**************************************************************************
 *   rcfile.c                                                             *
 *                                                                        *
 *   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,  *
 *   2010, 2011, 2013, 2014 Free Software Foundation, Inc.                *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 3, or (at your option)  *
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

#include "proto.h"

#include <glob.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#ifndef DISABLE_NANORC

static const rcoption rcopts[] = {
    {"boldtext", BOLD_TEXT},
#ifndef DISABLE_JUSTIFY
    {"brackets", 0},
#endif
    {"const", CONST_UPDATE},  /* deprecated form, remove in 2018 */
    {"constantshow", CONST_UPDATE},
#ifndef DISABLE_WRAPJUSTIFY
    {"fill", 0},
#endif
#ifndef DISABLE_HISTORIES
    {"historylog", HISTORYLOG},
#endif
    {"morespace", MORE_SPACE},
#ifndef DISABLE_MOUSE
    {"mouse", USE_MOUSE},
#endif
#ifndef DISABLE_MULTIBUFFER
    {"multibuffer", MULTIBUFFER},
#endif
    {"nohelp", NO_HELP},
    {"nonewlines", NO_NEWLINES},
#ifndef DISABLE_WRAPPING
    {"nowrap", NO_WRAP},
#endif
#ifndef DISABLE_OPERATINGDIR
    {"operatingdir", 0},
#endif
#ifndef DISABLE_HISTORIES
    {"poslog", POS_HISTORY},  /* deprecated form, remove in 2018 */
    {"positionlog", POS_HISTORY},
#endif
    {"preserve", PRESERVE},
#ifndef DISABLE_JUSTIFY
    {"punct", 0},
    {"quotestr", 0},
#endif
    {"rebinddelete", REBIND_DELETE},
    {"rebindkeypad", REBIND_KEYPAD},
#ifdef HAVE_REGEX_H
    {"regexp", USE_REGEXP},
#endif
#ifndef DISABLE_SPELLER
    {"speller", 0},
#endif
    {"suspend", SUSPEND},
    {"tabsize", 0},
    {"tempfile", TEMP_FILE},
    {"view", VIEW_MODE},
#ifndef NANO_TINY
    {"allow_insecure_backup", INSECURE_BACKUP},
    {"autoindent", AUTOINDENT},
    {"backup", BACKUP_FILE},
    {"backupdir", 0},
    {"backwards", BACKWARDS_SEARCH},
    {"casesensitive", CASE_SENSITIVE},
    {"cut", CUT_TO_END},
    {"justifytrim", JUSTIFY_TRIM},
    {"locking", LOCKING},
    {"matchbrackets", 0},
    {"noconvert", NO_CONVERT},
    {"quickblank", QUICK_BLANK},
    {"quiet", QUIET},
    {"smarthome", SMART_HOME},
    {"smooth", SMOOTH_SCROLL},
    {"softwrap", SOFTWRAP},
    {"tabstospaces", TABS_TO_SPACES},
    {"unix", MAKE_IT_UNIX},
    {"whitespace", 0},
    {"wordbounds", WORD_BOUNDS},
#endif
#ifndef DISABLE_COLOR
    {"titlecolor", 0},
    {"statuscolor", 0},
    {"keycolor", 0},
    {"functioncolor", 0},
#endif
    {NULL, 0}
};

static bool errors = FALSE;
	/* Whether we got any errors while parsing an rcfile. */
static size_t lineno = 0;
	/* If we did, the line number where the last error occurred. */
static char *nanorc = NULL;
	/* The path to the rcfile we're parsing. */
#ifndef DISABLE_COLOR
static bool opensyntax = FALSE;
	/* Whether we're allowed to add to the last syntax.  When a file ends,
	 * or when a new syntax command is seen, this bool becomes FALSE. */
static syntaxtype *endsyntax = NULL;
	/* The end of the list of syntaxes. */
static colortype *endcolor = NULL;
	/* The end of the color list for the current syntax. */
#endif

/* We have an error in some part of the rcfile.  Print the error message
 * on stderr, and then make the user hit Enter to continue starting
 * nano. */
void rcfile_error(const char *msg, ...)
{
    va_list ap;

    if (ISSET(QUIET))
	return;

    fprintf(stderr, "\n");
    if (lineno > 0) {
	errors = TRUE;
	fprintf(stderr, _("Error in %s on line %lu: "), nanorc, (unsigned long)lineno);
    }

    va_start(ap, msg);
    vfprintf(stderr, _(msg), ap);
    va_end(ap);

    fprintf(stderr, "\n");
}
#endif /* !DISABLE_NANORC */

#if !defined(DISABLE_NANORC) || !defined(DISABLE_HISTORIES)
/* Parse the next word from the string, null-terminate it, and return
 * a pointer to the first character after the null terminator.  The
 * returned pointer will point to '\0' if we hit the end of the line. */
char *parse_next_word(char *ptr)
{
    while (!isblank(*ptr) && *ptr != '\0')
	ptr++;

    if (*ptr == '\0')
	return ptr;

    /* Null-terminate and advance ptr. */
    *ptr++ = '\0';

    while (isblank(*ptr))
	ptr++;

    return ptr;
}
#endif /* !DISABLE_NANORC || !DISABLE_HISTORIES */

#ifndef DISABLE_NANORC
/* Parse an argument, with optional quotes, after a keyword that takes
 * one.  If the next word starts with a ", we say that it ends with the
 * last " of the line.  Otherwise, we interpret it as usual, so that the
 * arguments can contain "'s too. */
char *parse_argument(char *ptr)
{
    const char *ptr_save = ptr;
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
	rcfile_error(N_("Argument '%s' has an unterminated \""), ptr_save);
    } else {
	*last_quote = '\0';
	ptr = last_quote + 1;
    }
    if (ptr != NULL)
	while (isblank(*ptr))
	    ptr++;
    return ptr;
}

#ifndef DISABLE_COLOR
/* Parse the next regex string from the line at ptr, and return it. */
char *parse_next_regex(char *ptr)
{
    assert(ptr != NULL);

    /* Continue until the end of the line, or a " followed by a space, a
     * blank character, or \0. */
    while ((*ptr != '"' || (!isblank(*(ptr + 1)) &&
	*(ptr + 1) != '\0')) && *ptr != '\0')
	ptr++;

    assert(*ptr == '"' || *ptr == '\0');

    if (*ptr == '\0') {
	rcfile_error(
		N_("Regex strings must begin and end with a \" character"));
	return NULL;
    }

    /* Null-terminate and advance ptr. */
    *ptr++ = '\0';

    while (isblank(*ptr))
	ptr++;

    return ptr;
}

/* Compile the regular expression regex to see if it's valid.  Return
 * TRUE if it is, or FALSE otherwise. */
bool nregcomp(const char *regex, int eflags)
{
    regex_t preg;
    const char *r = fixbounds(regex);
    int rc = regcomp(&preg, r, REG_EXTENDED | eflags);

    if (rc != 0) {
	size_t len = regerror(rc, &preg, NULL, 0);
	char *str = charalloc(len);

	regerror(rc, &preg, str, len);
	rcfile_error(N_("Bad regex \"%s\": %s"), r, str);
	free(str);
    }

    regfree(&preg);
    return (rc == 0);
}

/* Parse the next syntax string from the line at ptr, and add it to the
 * global list of color syntaxes. */
void parse_syntax(char *ptr)
{
    const char *fileregptr = NULL, *nameptr = NULL;
    syntaxtype *tmpsyntax, *prev_syntax;
    regexlisttype *endext = NULL;
	/* The end of the extensions list for this syntax. */

    opensyntax = FALSE;

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

    nameptr = ++ptr;
    ptr = parse_next_regex(ptr);
    if (ptr == NULL)
	return;

    /* Redefining the "none" syntax is not allowed. */
    if (strcmp(nameptr, "none") == 0) {
	rcfile_error(N_("The \"none\" syntax is reserved"));
	return;
    }

    /* Search for a duplicate syntax name.  If we find one, free it, so
     * that we always use the last syntax with a given name. */
    prev_syntax = NULL;
    for (tmpsyntax = syntaxes; tmpsyntax != NULL;
	tmpsyntax = tmpsyntax->next) {
	if (strcmp(nameptr, tmpsyntax->name) == 0) {
	    syntaxtype *old_syntax = tmpsyntax;

	    if (endsyntax == tmpsyntax)
		endsyntax = prev_syntax;

	    tmpsyntax = tmpsyntax->next;
	    if (prev_syntax != NULL)
		prev_syntax->next = tmpsyntax;
	    else
		syntaxes = tmpsyntax;

	    free(old_syntax->name);
	    free(old_syntax);
	    break;
	}
	prev_syntax = tmpsyntax;
    }

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

    endsyntax->name = mallocstrcpy(NULL, nameptr);
    endsyntax->extensions = NULL;
    endsyntax->headers = NULL;
    endsyntax->magics = NULL;
    endsyntax->linter = NULL;
    endsyntax->formatter = NULL;
    endsyntax->color = NULL;
    endcolor = NULL;
    endsyntax->nmultis = 0;
    endsyntax->next = NULL;

    opensyntax = TRUE;

#ifdef DEBUG
    fprintf(stderr, "Starting a new syntax type: \"%s\"\n", nameptr);
#endif

    /* The default syntax should have no associated extensions. */
    if (strcmp(endsyntax->name, "default") == 0 && *ptr != '\0') {
	rcfile_error(
		N_("The \"default\" syntax must take no extensions"));
	return;
    }

    /* Now load the extension regexes into their part of the struct. */
    while (*ptr != '\0') {
	regexlisttype *newext;

	while (*ptr != '"' && *ptr != '\0')
	    ptr++;

	if (*ptr == '\0')
	    return;

	ptr++;

	fileregptr = ptr;
	ptr = parse_next_regex(ptr);
	if (ptr == NULL)
	    break;

	newext = (regexlisttype *)nmalloc(sizeof(regexlisttype));

	/* Save the extension regex if it's valid. */
	if (nregcomp(fileregptr, REG_NOSUB)) {
	    newext->full_regex = mallocstrcpy(NULL, fileregptr);
	    newext->rgx = NULL;

	    if (endext == NULL)
		endsyntax->extensions = newext;
	    else
		endext->next = newext;
	    endext = newext;
	    endext->next = NULL;
	} else
	    free(newext);
    }
}
#endif /* !DISABLE_COLOR */

/* Check whether the given executable function is "universal" (meaning
 * any horizontal movement or deletion) and thus is present in almost
 * all menus. */
bool is_universal(void (*func))
{
    if (func == do_left || func == do_right ||
	func == do_home || func == do_end ||
#ifndef NANO_TINY
	func == do_prev_word_void || func == do_next_word_void ||
#endif
	func == do_verbatim_input || func == do_cut_text_void ||
	func == do_delete || func == do_backspace ||
	func == do_tab || func == do_enter)
	return TRUE;
    else
	return FALSE;
}

/* Bind or unbind a key combo, to or from a function. */
void parse_binding(char *ptr, bool dobind)
{
    char *keyptr = NULL, *keycopy = NULL, *funcptr = NULL, *menuptr = NULL;
    sc *s, *newsc = NULL;
    int menu;

    assert(ptr != NULL);

#ifdef DEBUG
    fprintf(stderr, "Starting the rebinding code...\n");
#endif

    if (*ptr == '\0') {
	rcfile_error(N_("Missing key name"));
	return;
    }

    keyptr = ptr;
    ptr = parse_next_word(ptr);
    keycopy = mallocstrcpy(NULL, keyptr);

    if (strlen(keycopy) < 2) {
	rcfile_error(N_("Key name is too short"));
	goto free_copy;
    }

    /* Uppercase only the first two or three characters of the key name. */
    keycopy[0] = toupper(keycopy[0]);
    keycopy[1] = toupper(keycopy[1]);
    if (keycopy[0] == 'M' && keycopy[1] == '-') {
	if (strlen(keycopy) > 2)
	    keycopy[2] = toupper(keycopy[2]);
	else {
	    rcfile_error(N_("Key name is too short"));
	    goto free_copy;
	}
    }

    /* Allow the codes for Insert and Delete to be rebound, but apart
     * from those two only Control, Meta and Function sequences. */
    if (!strcasecmp(keycopy, "Ins") || !strcasecmp(keycopy, "Del"))
	keycopy[1] = tolower(keycopy[1]);
    else if (keycopy[0] != '^' && keycopy[0] != 'M' && keycopy[0] != 'F') {
	rcfile_error(N_("Key name must begin with \"^\", \"M\", or \"F\""));
	goto free_copy;
    } else if (keycopy[0] == '^' && (keycopy[1] < 64 || keycopy[1] > 127)) {
	rcfile_error(N_("Key name %s is invalid"), keycopy);
	goto free_copy;
    }

    if (dobind) {
	funcptr = ptr;
	ptr = parse_next_word(ptr);

	if (funcptr[0] == '\0') {
	    rcfile_error(N_("Must specify a function to bind the key to"));
	    goto free_copy;
	}
    }

    menuptr = ptr;
    ptr = parse_next_word(ptr);

    if (menuptr[0] == '\0') {
	/* TRANSLATORS: Do not translate the word "all". */
	rcfile_error(N_("Must specify a menu (or \"all\") in which to bind/unbind the key"));
	goto free_copy;
    }

    if (dobind) {
	newsc = strtosc(funcptr);
	if (newsc == NULL) {
	    rcfile_error(N_("Cannot map name \"%s\" to a function"), funcptr);
	    goto free_copy;
	}
    }

    menu = strtomenu(menuptr);
    if (menu < 1) {
	rcfile_error(N_("Cannot map name \"%s\" to a menu"), menuptr);
	goto free_copy;
    }

#ifdef DEBUG
    if (dobind)
	fprintf(stderr, "newsc address is now %ld, assigned func = %ld, menu = %x\n",
	    (long)&newsc, (long)newsc->scfunc, menu);
    else
	fprintf(stderr, "unbinding \"%s\" from menu %x\n", keycopy, menu);
#endif

    if (dobind) {
	subnfunc *f;
	int mask = 0;

	/* Tally up the menus where the function exists. */
	for (f = allfuncs; f != NULL; f = f->next)
	    if (f->scfunc == newsc->scfunc)
		mask = mask | f->menus;

	/* Handle the special case of the toggles. */
	if (newsc->scfunc == do_toggle_void)
	    mask = MMAIN;

	/* Now limit the given menu to those where the function exists. */
	if (is_universal(newsc->scfunc))
	    menu = menu & MMOST;
	else
	    menu = menu & mask;

	if (!menu) {
	    rcfile_error(N_("Function '%s' does not exist in menu '%s'"), funcptr, menuptr);
	    free(newsc);
	    goto free_copy;
	}

	newsc->keystr = keycopy;
	newsc->menus = menu;
	newsc->type = strtokeytype(newsc->keystr);
	assign_keyinfo(newsc);

	/* Do not allow rebinding the equivalent of the Escape key. */
	if (newsc->type == META && newsc->seq == 91) {
	    rcfile_error(N_("Sorry, keystroke \"%s\" may not be rebound"), newsc->keystr);
	    free(newsc);
	    goto free_copy;
	}
#ifdef DEBUG
	fprintf(stderr, "s->keystr = \"%s\"\n", newsc->keystr);
	fprintf(stderr, "s->seq = \"%d\"\n", newsc->seq);
#endif
    }

    /* Now find and delete any existing same shortcut in the menu(s). */
    for (s = sclist; s != NULL; s = s->next) {
	if ((s->menus & menu) && !strcmp(s->keystr, keycopy)) {
#ifdef DEBUG
	    fprintf(stderr, "deleting entry from among menus %x\n", s->menus);
#endif
	    s->menus &= ~menu;
	}
    }

    if (dobind) {
	/* If this is a toggle, copy its sequence number. */
	if (newsc->scfunc == do_toggle_void) {
	    for (s = sclist; s != NULL; s = s->next)
		if (s->scfunc == do_toggle_void && s->toggle == newsc->toggle)
		    newsc->ordinal = s->ordinal;
	} else
	    newsc->ordinal = 0;
	/* Add the new shortcut at the start of the list. */
	newsc->next = sclist;
	sclist = newsc;
	return;
    }

  free_copy:
    free(keycopy);
}


#ifndef DISABLE_COLOR
/* Read and parse additional syntax files. */
static void _parse_include(char *file)
{
    struct stat rcinfo;
    FILE *rcstream;

    /* Can't get the specified file's full path because it may screw up
     * our cwd depending on the parent directories' permissions (see
     * Savannah bug #25297). */

    /* Don't open directories, character files, or block files. */
    if (stat(file, &rcinfo) != -1) {
	if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) ||
		S_ISBLK(rcinfo.st_mode)) {
	    rcfile_error(S_ISDIR(rcinfo.st_mode) ?
		_("\"%s\" is a directory") :
		_("\"%s\" is a device file"), file);
	}
    }

    /* Open the new syntax file. */
    if ((rcstream = fopen(file, "rb")) == NULL) {
	rcfile_error(_("Error reading %s: %s"), file,
		strerror(errno));
	return;
    }

    /* Use the name and line number position of the new syntax file
     * while parsing it, so we can know where any errors in it are. */
    nanorc = file;
    lineno = 0;

#ifdef DEBUG
    fprintf(stderr, "Parsing file \"%s\"\n", file);
#endif

    parse_rcfile(rcstream, TRUE);
}

void parse_include(char *ptr)
{
    char *option, *nanorc_save = nanorc, *expanded;
    size_t lineno_save = lineno, i;
    glob_t files;

    option = ptr;
    if (*option == '"')
	option++;
    ptr = parse_argument(ptr);

    /* Expand tildes first, then the globs. */
    expanded = real_dir_from_tilde(option);

    if (glob(expanded, GLOB_ERR|GLOB_NOSORT, NULL, &files) == 0) {
	for (i = 0; i < files.gl_pathc; ++i)
	    _parse_include(files.gl_pathv[i]);
    } else {
	rcfile_error(_("Error expanding %s: %s"), option,
		strerror(errno));
    }

    globfree(&files);
    free(expanded);

    /* We're done with the new syntax file.  Restore the original
     * filename and line number position. */
    nanorc = nanorc_save;
    lineno = lineno_save;
}

/* Return the short value corresponding to the color named in colorname,
 * and set bright to TRUE if that color is bright. */
short color_to_short(const char *colorname, bool *bright)
{
    short mcolor = -1;

    assert(colorname != NULL && bright != NULL);

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
	rcfile_error(N_("Color \"%s\" not understood.\n"
		"Valid colors are \"green\", \"red\", \"blue\",\n"
		"\"white\", \"yellow\", \"cyan\", \"magenta\" and\n"
		"\"black\", with the optional prefix \"bright\"\n"
		"for foreground colors."), colorname);

    return mcolor;
}

/* Parse the color string in the line at ptr, and add it to the current
 * file's associated colors.  If icase is TRUE, treat the color string
 * as case insensitive. */
void parse_colors(char *ptr, bool icase)
{
    short fg, bg;
    bool bright = FALSE;
    char *fgstr;

    assert(ptr != NULL);

    if (!opensyntax) {
	rcfile_error(
		N_("Cannot add a color command without a syntax command"));
	return;
    }

    if (*ptr == '\0') {
	rcfile_error(N_("Missing color name"));
	return;
    }

    fgstr = ptr;
    ptr = parse_next_word(ptr);
    if (!parse_color_names(fgstr, &fg, &bg, &bright))
	return;

    if (*ptr == '\0') {
	rcfile_error(N_("Missing regex string"));
	return;
    }

    /* Now for the fun part.  Start adding regexes to individual strings
     * in the colorstrings array, woo! */
    while (ptr != NULL && *ptr != '\0') {
	colortype *newcolor;
	    /* The container for a color plus its regexes. */
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

	fgstr = ptr;
	ptr = parse_next_regex(ptr);
	if (ptr == NULL)
	    break;

	newcolor = (colortype *)nmalloc(sizeof(colortype));

	/* Save the starting regex string if it's valid, and set up the
	 * color information. */
	if (nregcomp(fgstr, icase ? REG_ICASE : 0)) {
	    newcolor->fg = fg;
	    newcolor->bg = bg;
	    newcolor->bright = bright;
	    newcolor->icase = icase;

	    newcolor->start_regex = mallocstrcpy(NULL, fgstr);
	    newcolor->start = NULL;

	    newcolor->end_regex = NULL;
	    newcolor->end = NULL;

	    newcolor->next = NULL;

	    if (endcolor == NULL) {
		endsyntax->color = newcolor;
#ifdef DEBUG
		fprintf(stderr, "Starting a new colorstring for fg %hd, bg %hd\n", fg, bg);
#endif
	    } else {
#ifdef DEBUG
		fprintf(stderr, "Adding new entry for fg %hd, bg %hd\n", fg, bg);
#endif
		/* Need to recompute endcolor now so we can extend
		 * colors to syntaxes. */
		for (endcolor = endsyntax->color; endcolor->next != NULL; endcolor = endcolor->next)
		    ;
		endcolor->next = newcolor;
	    }

	    endcolor = newcolor;
	} else {
	    free(newcolor);
	    cancelled = TRUE;
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

	    /* If the start regex was invalid, skip past the end regex
	     * to stay in sync. */
	    if (cancelled)
		continue;

	    /* Save the ending regex string if it's valid. */
	    newcolor->end_regex = (nregcomp(fgstr, icase ? REG_ICASE :
		0)) ? mallocstrcpy(NULL, fgstr) : NULL;

	    /* Lame way to skip another static counter. */
	    newcolor->id = endsyntax->nmultis;
	    endsyntax->nmultis++;
	}
    }
}

/* Parse the color name, or pair of color names, in combostr. */
bool parse_color_names(char *combostr, short *fg, short *bg, bool *bright)
{
    bool no_fgcolor = FALSE;

    if (combostr == NULL)
	return FALSE;

    if (strchr(combostr, ',') != NULL) {
	char *bgcolorname;
	strtok(combostr, ",");
	bgcolorname = strtok(NULL, ",");
	if (bgcolorname == NULL) {
	    /* If we have a background color without a foreground color,
	     * parse it properly. */
	    bgcolorname = combostr + 1;
	    no_fgcolor = TRUE;
	}
	if (strncasecmp(bgcolorname, "bright", 6) == 0) {
	    rcfile_error(N_("Background color \"%s\" cannot be bright"), bgcolorname);
	    return FALSE;
	}
	*bg = color_to_short(bgcolorname, bright);
    } else
	*bg = -1;

    if (!no_fgcolor) {
	*fg = color_to_short(combostr, bright);

	/* Don't try to parse screwed-up foreground colors. */
	if (*fg == -1)
	    return FALSE;
    } else
	*fg = -1;

    return TRUE;
}


/* Read regex strings enclosed in double quotes from the line pointed at
 * by ptr, and store them quoteless in the passed storage place. */
void grab_and_store(char *ptr, const char *kind, regexlisttype **storage)
{
    regexlisttype *lastthing;

    if (!opensyntax) {
	rcfile_error(
		N_("A '%s' command requires a preceding 'syntax' command"), kind);
	return;
    }

    if (*ptr == '\0') {
	rcfile_error(N_("Missing regex string after '%s' command"), kind);
	return;
    }

    lastthing = *storage;

    /* If there was an earlier command, go to the last of those regexes. */
    while (lastthing != NULL && lastthing->next != NULL)
	lastthing = lastthing->next;

    /* Now gather any valid regexes and add them to the linked list. */
    while (*ptr != '\0') {
	const char *regexstring;
	regexlisttype *newthing;

	if (*ptr != '"') {
	    rcfile_error(
		N_("Regex strings must begin and end with a \" character"));
	    return;
	}

	regexstring = ++ptr;
	ptr = parse_next_regex(ptr);
	if (ptr == NULL)
	    return;

	/* If the regex string is malformed, skip it. */
	if (nregcomp(regexstring, REG_NOSUB) != 0)
	    continue;

	/* Copy the regex into a struct, and hook this in at the end. */
	newthing = (regexlisttype *)nmalloc(sizeof(regexlisttype));
	newthing->full_regex = mallocstrcpy(NULL, regexstring);
	newthing->rgx = NULL;
	newthing->next = NULL;

	if (lastthing == NULL)
	    *storage = newthing;
	else
	    lastthing->next = newthing;

	lastthing = newthing;
    }
}

/* Parse the header-line regexes that may influence the choice of syntax. */
void parse_header_exp(char *ptr)
{
    grab_and_store(ptr, "header", &endsyntax->headers);
}

#ifdef HAVE_LIBMAGIC
/* Parse the magic regexes that may influence the choice of syntax. */
void parse_magic_exp(char *ptr)
{
    grab_and_store(ptr, "magic", &endsyntax->magics);
}
#endif /* HAVE_LIBMAGIC */

/* Parse the linter requested for this syntax. */
void parse_linter(char *ptr)
{
    assert(ptr != NULL);

    if (!opensyntax) {
	rcfile_error(
		N_("Cannot add a linter without a syntax command"));
	return;
    }

    if (*ptr == '\0') {
	rcfile_error(N_("Missing linter command"));
	return;
    }

    free(endsyntax->linter);

    /* Let them unset the linter by using "". */
    if (!strcmp(ptr, "\"\""))
	endsyntax->linter = NULL;
    else
	endsyntax->linter = mallocstrcpy(NULL, ptr);
}

#ifndef DISABLE_SPELLER
/* Parse the formatter requested for this syntax. */
void parse_formatter(char *ptr)
{
    assert(ptr != NULL);

    if (!opensyntax) {
	rcfile_error(
		N_("Cannot add formatter without a syntax command"));
	return;
    }

    if (*ptr == '\0') {
	rcfile_error(N_("Missing formatter command"));
	return;
    }

    free(endsyntax->formatter);

    /* Let them unset the formatter by using "". */
    if (!strcmp(ptr, "\"\""))
	endsyntax->formatter = NULL;
    else
	endsyntax->formatter = mallocstrcpy(NULL, ptr);
}
#endif /* !DISABLE_SPELLER */
#endif /* !DISABLE_COLOR */

/* Check whether the user has unmapped every shortcut for a
 * sequence we consider 'vital', like the exit function. */
static void check_vitals_mapped(void)
{
    subnfunc *f;
    int v;
#define VITALS 5
    void (*vitals[VITALS])(void) = { do_exit, do_exit, do_cancel, do_cancel, do_cancel };
    int inmenus[VITALS] = { MMAIN, MHELP, MWHEREIS, MREPLACE, MGOTOLINE };

    for  (v = 0; v < VITALS; v++) {
	for (f = allfuncs; f != NULL; f = f->next) {
	    if (f->scfunc == vitals[v] && f->menus & inmenus[v]) {
		const sc *s = first_sc_for(inmenus[v], f->scfunc);
		if (!s) {
		    fprintf(stderr, _("Fatal error: no keys mapped for function "
				     "\"%s\".  Exiting.\n"), f->desc);
		    fprintf(stderr, _("If needed, use nano with the -I option "
				     "to adjust your nanorc settings.\n"));
		     exit(1);
		}
		break;
	    }
	}
    }
}

/* Parse the rcfile, once it has been opened successfully at rcstream,
 * and close it afterwards.  If syntax_only is TRUE, only allow the file
 * to contain color syntax commands: syntax, color, and icolor. */
void parse_rcfile(FILE *rcstream
#ifndef DISABLE_COLOR
	, bool syntax_only
#endif
	)
{
    char *buf = NULL;
    ssize_t len;
    size_t n = 0;
#ifndef DISABLE_COLOR
    syntaxtype *end_syn_save = NULL;
#endif

    while ((len = getline(&buf, &n, rcstream)) > 0) {
	char *ptr, *keyword, *option;
	int set = 0;
	size_t i;

	/* Ignore the newline. */
	if (buf[len - 1] == '\n')
	    buf[len - 1] = '\0';

	lineno++;
	ptr = buf;
	while (isblank(*ptr))
	    ptr++;

	/* If we have a blank line or a comment, skip to the next
	 * line. */
	if (*ptr == '\0' || *ptr == '#')
	    continue;

	/* Otherwise, skip to the next space. */
	keyword = ptr;
	ptr = parse_next_word(ptr);

#ifndef DISABLE_COLOR
	/* Handle extending first... */
	if (strcasecmp(keyword, "extendsyntax") == 0) {
	    char *syntaxname = ptr;
	    syntaxtype *ts = NULL;

	    ptr = parse_next_word(ptr);
	    for (ts = syntaxes; ts != NULL; ts = ts->next)
		if (!strcmp(ts->name, syntaxname))
		    break;

	    if (ts == NULL) {
		rcfile_error(N_("Could not find syntax \"%s\" to extend"), syntaxname);
		continue;
	    } else {
		opensyntax = TRUE;
		end_syn_save = endsyntax;
		endsyntax = ts;
		keyword = ptr;
		ptr = parse_next_word(ptr);
	    }
	}
#endif

	/* Try to parse the keyword. */
	if (strcasecmp(keyword, "set") == 0) {
#ifndef DISABLE_COLOR
	    if (syntax_only)
		rcfile_error(
			N_("Command \"%s\" not allowed in included file"),
			keyword);
	    else
#endif
		set = 1;
	} else if (strcasecmp(keyword, "unset") == 0) {
#ifndef DISABLE_COLOR
	    if (syntax_only)
		rcfile_error(
			N_("Command \"%s\" not allowed in included file"),
			keyword);
	    else
#endif
		set = -1;
	}
#ifndef DISABLE_COLOR
	else if (strcasecmp(keyword, "include") == 0) {
	    if (syntax_only)
		rcfile_error(
			N_("Command \"%s\" not allowed in included file"),
			keyword);
	    else
		parse_include(ptr);
	} else if (strcasecmp(keyword, "syntax") == 0) {
	    if (endsyntax != NULL && endcolor == NULL)
		rcfile_error(N_("Syntax \"%s\" has no color commands"),
			endsyntax->name);
	    parse_syntax(ptr);
	}
	else if (strcasecmp(keyword, "magic") == 0)
#ifdef HAVE_LIBMAGIC
	    parse_magic_exp(ptr);
#else
	    ;
#endif
	else if (strcasecmp(keyword, "header") == 0)
	    parse_header_exp(ptr);
	else if (strcasecmp(keyword, "color") == 0)
	    parse_colors(ptr, FALSE);
	else if (strcasecmp(keyword, "icolor") == 0)
	    parse_colors(ptr, TRUE);
	else if (strcasecmp(keyword, "linter") == 0)
	    parse_linter(ptr);
	else if (strcasecmp(keyword, "formatter") == 0)
#ifndef DISABLE_SPELLER
	    parse_formatter(ptr);
#else
	    ;
#endif
#endif /* !DISABLE_COLOR */
	else if (strcasecmp(keyword, "bind") == 0)
	    parse_binding(ptr, TRUE);
	else if (strcasecmp(keyword, "unbind") == 0)
	    parse_binding(ptr, FALSE);
	else
	    rcfile_error(N_("Command \"%s\" not understood"), keyword);

#ifndef DISABLE_COLOR
	/* If we temporarily reset endsyntax to allow extending,
	 * restore the value here. */
	if (end_syn_save != NULL) {
	    endsyntax = end_syn_save;
	    end_syn_save = NULL;
	}
#endif

	if (set == 0)
	    continue;

	if (*ptr == '\0') {
	    rcfile_error(N_("Missing option"));
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
			 * an argument. */
			if (*ptr == '\0') {
			    rcfile_error(
				N_("Option \"%s\" requires an argument"),
				rcopts[i].name);
			    break;
			}
			option = ptr;
			if (*option == '"')
			    option++;
			ptr = parse_argument(ptr);

			option = mallocstrcpy(NULL, option);
#ifdef DEBUG
			fprintf(stderr, "option = \"%s\"\n", option);
#endif

			/* Make sure option is a valid multibyte
			 * string. */
			if (!is_valid_mbstring(option)) {
			    rcfile_error(
				N_("Option is not a valid multibyte string"));
			    break;
			}

#ifndef DISABLE_COLOR
			if (strcasecmp(rcopts[i].name, "titlecolor") == 0)
			    specified_color_combo[TITLE_BAR] = option;
			else if (strcasecmp(rcopts[i].name, "statuscolor") == 0)
			    specified_color_combo[STATUS_BAR] = option;
			else if (strcasecmp(rcopts[i].name, "keycolor") == 0)
			    specified_color_combo[KEY_COMBO] = option;
			else if (strcasecmp(rcopts[i].name, "functioncolor") == 0)
			    specified_color_combo[FUNCTION_TAG] = option;
			else
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
					N_("Requested fill size \"%s\" is invalid"),
					option);
				wrap_at = -CHARS_FROM_EOL;
			    } else
				free(option);
			} else
#endif
#ifndef NANO_TINY
			if (strcasecmp(rcopts[i].name,
				"matchbrackets") == 0) {
			    matchbrackets = option;
			    if (has_blank_mbchars(matchbrackets)) {
				rcfile_error(
					N_("Non-blank characters required"));
				free(matchbrackets);
				matchbrackets = NULL;
			    }
			} else if (strcasecmp(rcopts[i].name,
				"whitespace") == 0) {
			    whitespace = option;
			    if (mbstrlen(whitespace) != 2 ||
				strlenpt(whitespace) != 2) {
				rcfile_error(
					N_("Two single-column characters required"));
				free(whitespace);
				whitespace = NULL;
			    } else {
				whitespace_len[0] =
					parse_mbchar(whitespace, NULL,
					NULL);
				whitespace_len[1] =
					parse_mbchar(whitespace +
					whitespace_len[0], NULL, NULL);
			    }
			} else
#endif
#ifndef DISABLE_JUSTIFY
			if (strcasecmp(rcopts[i].name, "punct") == 0) {
			    punct = option;
			    if (has_blank_mbchars(punct)) {
				rcfile_error(
					N_("Non-blank characters required"));
				free(punct);
				punct = NULL;
			    }
			} else if (strcasecmp(rcopts[i].name,
				"brackets") == 0) {
			    brackets = option;
			    if (has_blank_mbchars(brackets)) {
				rcfile_error(
					N_("Non-blank characters required"));
				free(brackets);
				brackets = NULL;
			    }
			} else if (strcasecmp(rcopts[i].name,
				"quotestr") == 0)
			    quotestr = option;
			else
#endif
#ifndef NANO_TINY
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
					N_("Requested tab size \"%s\" is invalid"),
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
		    rcfile_error(N_("Cannot unset option \"%s\""),
			rcopts[i].name);
		break;
	    }
	}
	if (rcopts[i].name == NULL)
	    rcfile_error(N_("Unknown option \"%s\""), option);
    }

#ifndef DISABLE_COLOR
    if (endsyntax != NULL && endcolor == NULL)
	rcfile_error(N_("Syntax \"%s\" has no color commands"),
		endsyntax->name);
#endif

    opensyntax = FALSE;

    free(buf);
    fclose(rcstream);
    lineno = 0;

    check_vitals_mapped();
    return;
}

/* The main rcfile function.  It tries to open the system-wide rcfile,
 * followed by the current user's rcfile. */
void do_rcfile(void)
{
    struct stat rcinfo;
    FILE *rcstream;

    nanorc = mallocstrcpy(nanorc, SYSCONFDIR "/nanorc");

    /* Don't open directories, character files, or block files. */
    if (stat(nanorc, &rcinfo) != -1) {
	if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) ||
		S_ISBLK(rcinfo.st_mode))
	    rcfile_error(S_ISDIR(rcinfo.st_mode) ?
		_("\"%s\" is a directory") :
		_("\"%s\" is a device file"), nanorc);
    }

#ifdef DEBUG
    fprintf(stderr, "Parsing file \"%s\"\n", nanorc);
#endif

    /* Try to open the system-wide nanorc. */
    rcstream = fopen(nanorc, "rb");
    if (rcstream != NULL)
	parse_rcfile(rcstream
#ifndef DISABLE_COLOR
		, FALSE
#endif
		);

#ifdef DISABLE_ROOTWRAPPING
    /* We've already read SYSCONFDIR/nanorc, if it's there.  If we're
     * root, and --disable-wrapping-as-root is used, turn wrapping off
     * now. */
    if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif

    get_homedir();

    if (homedir == NULL)
	rcfile_error(N_("I can't find my home directory!  Wah!"));
    else {
#ifndef RCFILE_NAME
#define RCFILE_NAME ".nanorc"
#endif
	nanorc = charealloc(nanorc, strlen(homedir) + strlen(RCFILE_NAME) + 2);
	sprintf(nanorc, "%s/%s", homedir, RCFILE_NAME);

	/* Don't open directories, character files, or block files. */
	if (stat(nanorc, &rcinfo) != -1) {
	    if (S_ISDIR(rcinfo.st_mode) || S_ISCHR(rcinfo.st_mode) ||
		S_ISBLK(rcinfo.st_mode))
		rcfile_error(S_ISDIR(rcinfo.st_mode) ?
			_("\"%s\" is a directory") :
			_("\"%s\" is a device file"), nanorc);
	}

	/* Try to open the current user's nanorc. */
	rcstream = fopen(nanorc, "rb");
	if (rcstream == NULL) {
	    /* Don't complain about the file's not existing. */
	    if (errno != ENOENT)
		rcfile_error(N_("Error reading %s: %s"), nanorc,
			strerror(errno));
	} else
	    parse_rcfile(rcstream
#ifndef DISABLE_COLOR
		, FALSE
#endif
		);
    }

    free(nanorc);
    nanorc = NULL;

    if (errors && !ISSET(QUIET)) {
	errors = FALSE;
	fprintf(stderr,
		_("\nPress Enter to continue starting nano.\n"));
	while (getchar() != '\n')
	    ;
    }
}

#endif /* !DISABLE_NANORC */
