/* $Id$ */
/**************************************************************************
 *   color.c                                                              *
 *                                                                        *
 *   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,  *
 *   2010, 2011, 2013, 2014, 2015 Free Software Foundation, Inc.          *
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

#ifndef DISABLE_COLOR

/* For each syntax list entry, go through the list of colors and assign
 * the color pairs. */
void set_colorpairs(void)
{
    const syntaxtype *this_syntax = syntaxes;
    bool defok = FALSE;
    short fg, bg;
    size_t i;

    start_color();

#ifdef HAVE_USE_DEFAULT_COLORS
    /* Use the default colors, if available. */
    defok = (use_default_colors() != ERR);
#endif

    for (i = 0; i < NUMBER_OF_ELEMENTS; i++) {
	bool bright = FALSE;

	if (parse_color_names(specified_color_combo[i], &fg, &bg, &bright)) {
	    if (fg == -1 && !defok)
		fg = COLOR_WHITE;
	    if (bg == -1 && !defok)
		bg = COLOR_BLACK;
	    init_pair(i + 1, fg, bg);
	    interface_color_pair[i].bright = bright;
	    interface_color_pair[i].pairnum = COLOR_PAIR(i + 1);
	}
	else {
	    interface_color_pair[i].bright = FALSE;
	    if (i != FUNCTION_TAG)
		interface_color_pair[i].pairnum = hilite_attribute;
	    else
		interface_color_pair[i].pairnum = A_NORMAL;
	}

	free(specified_color_combo[i]);
	specified_color_combo[i] = NULL;
    }

    for (; this_syntax != NULL; this_syntax = this_syntax->next) {
	colortype *this_color = this_syntax->color;
	int clr_pair = NUMBER_OF_ELEMENTS + 1;

	for (; this_color != NULL; this_color = this_color->next) {
	    const colortype *beforenow = this_syntax->color;

	    for (; beforenow != this_color &&
		(beforenow->fg != this_color->fg ||
		beforenow->bg != this_color->bg ||
		beforenow->bright != this_color->bright);
		beforenow = beforenow->next)
		;

	    if (beforenow != this_color)
		this_color->pairnum = beforenow->pairnum;
	    else {
		this_color->pairnum = clr_pair;
		clr_pair++;
	    }
	}
    }
}

/* Initialize the color information. */
void color_init(void)
{
    assert(openfile != NULL);

    if (has_colors()) {
	const colortype *tmpcolor;
#ifdef HAVE_USE_DEFAULT_COLORS
	/* Use the default colors, if available. */
	bool defok = (use_default_colors() != ERR);
#endif

	for (tmpcolor = openfile->colorstrings; tmpcolor != NULL;
		tmpcolor = tmpcolor->next) {
	    short foreground = tmpcolor->fg, background = tmpcolor->bg;
	    if (foreground == -1) {
#ifdef HAVE_USE_DEFAULT_COLORS
		if (!defok)
#endif
		    foreground = COLOR_WHITE;
	    }

	    if (background == -1) {
#ifdef HAVE_USE_DEFAULT_COLORS
		if (!defok)
#endif
		    background = COLOR_BLACK;
	    }

	    init_pair(tmpcolor->pairnum, foreground, background);

#ifdef DEBUG
	    fprintf(stderr, "init_pair(): fg = %hd, bg = %hd\n", tmpcolor->fg, tmpcolor->bg);
#endif
	}
    }
}

/* Clean up a regex we previously compiled. */
void nfreeregex(regex_t **r)
{
    assert(r != NULL);

    regfree(*r);
    free(*r);
    *r = NULL;
}

/* Update the color information based on the current filename. */
void color_update(void)
{
    syntaxtype *tmpsyntax;
    syntaxtype *defsyntax = NULL;
    colortype *tmpcolor, *defcolor = NULL;
    regexlisttype *e;

    assert(openfile != NULL);

    openfile->syntax = NULL;
    openfile->colorstrings = NULL;

    /* If the rcfiles were not read, or contained no syntaxes, get out. */
    if (syntaxes == NULL)
	return;

    /* If we specified a syntax override string, use it. */
    if (syntaxstr != NULL) {
	/* If the syntax override is "none", it's the same as not having
	 * a syntax at all, so get out. */
	if (strcmp(syntaxstr, "none") == 0)
	    return;

	for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {
	    if (strcmp(tmpsyntax->desc, syntaxstr) == 0) {
		openfile->syntax = tmpsyntax;
		openfile->colorstrings = tmpsyntax->color;
	    }

	    if (openfile->colorstrings != NULL)
		break;
	}

	if (openfile->colorstrings == NULL)
	    statusbar(_("Unknown syntax name: %s"), syntaxstr);
    }

    /* If we didn't specify a syntax override string, or if we did and
     * there was no syntax by that name, get the syntax based on the
     * file extension, then try the headerline, and then try magic. */
    if (openfile->colorstrings == NULL) {
	char *currentdir = getcwd(NULL, PATH_MAX + 1);
	char *joinednames = charalloc(PATH_MAX + 1);
	char *fullname = NULL;

	if (currentdir != NULL) {
	    /* Concatenate the current working directory with the
	     * specified filename, and canonicalize the result. */
	    sprintf(joinednames, "%s/%s", currentdir, openfile->filename);
	    fullname = realpath(joinednames, NULL);
	    free(currentdir);
	}

	if (fullname == NULL)
	    fullname = mallocstrcpy(fullname, openfile->filename);

	for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {

	    /* If this is the default syntax, it has no associated
	     * extensions, which we've checked for elsewhere.  Skip over
	     * it here, but keep track of its color regexes. */
	    if (strcmp(tmpsyntax->desc, "default") == 0) {
		defsyntax = tmpsyntax;
		defcolor = tmpsyntax->color;
		continue;
	    }

	    for (e = tmpsyntax->extensions; e != NULL; e = e->next) {
		bool not_compiled = (e->ext == NULL);

		/* e->ext_regex has already been checked for validity
		 * elsewhere.  Compile its specified regex if we haven't
		 * already. */
		if (not_compiled) {
		    e->ext = (regex_t *)nmalloc(sizeof(regex_t));
		    regcomp(e->ext, fixbounds(e->ext_regex), REG_EXTENDED);
		}

		/* Set colorstrings if we match the extension regex. */
		if (regexec(e->ext, fullname, 0, NULL, 0) == 0) {
		    openfile->syntax = tmpsyntax;
		    openfile->colorstrings = tmpsyntax->color;
		    break;
		}

		/* Decompile e->ext_regex's specified regex if we aren't
		 * going to use it. */
		if (not_compiled)
		    nfreeregex(&e->ext);
	    }
	}

	free(joinednames);
	free(fullname);

	/* Check the headerline if the extension didn't match anything. */
	if (openfile->colorstrings == NULL) {
#ifdef DEBUG
	    fprintf(stderr, "No result from file extension, trying headerline...\n");
#endif
	    for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {

		for (e = tmpsyntax->headers; e != NULL; e = e->next) {
		    bool not_compiled = (e->ext == NULL);

		    if (not_compiled) {
			e->ext = (regex_t *)nmalloc(sizeof(regex_t));
			regcomp(e->ext, fixbounds(e->ext_regex), REG_EXTENDED);
		    }
#ifdef DEBUG
		    fprintf(stderr, "Comparing header regex \"%s\" to fileage \"%s\"...\n",
				    e->ext_regex, openfile->fileage->data);
#endif
		    /* Set colorstrings if we match the header-line regex. */
		    if (regexec(e->ext, openfile->fileage->data, 0, NULL, 0) == 0) {
			openfile->syntax = tmpsyntax;
			openfile->colorstrings = tmpsyntax->color;
			break;
		    }

		    if (not_compiled)
			nfreeregex(&e->ext);
		}
	    }
	}

#ifdef HAVE_LIBMAGIC
	/* Check magic if we don't have an answer yet. */
	if (openfile->colorstrings == NULL) {
	    struct stat fileinfo;
	    magic_t cookie = NULL;
	    const char *magicstring = NULL;
#ifdef DEBUG
	    fprintf(stderr, "No result from headerline either, trying libmagic...\n");
#endif
	    if (stat(openfile->filename, &fileinfo) == 0) {
		/* Open the magic database and get a diagnosis of the file. */
		cookie = magic_open(MAGIC_SYMLINK |
#ifdef DEBUG
				    MAGIC_DEBUG | MAGIC_CHECK |
#endif
				    MAGIC_ERROR);
		if (cookie == NULL || magic_load(cookie, NULL) < 0)
		    statusbar(_("magic_load() failed: %s"), strerror(errno));
		else {
		    magicstring = magic_file(cookie, openfile->filename);
		    if (magicstring == NULL) {
			statusbar(_("magic_file(%s) failed: %s"),
					openfile->filename, magic_error(cookie));
		    }
#ifdef DEBUG
		    fprintf(stderr, "Returned magic string is: %s\n", magicstring);
#endif
		}
	    }

	    /* Now try and find a syntax that matches the magicstring. */
	    for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {

		for (e = tmpsyntax->magics; e != NULL; e = e->next) {
		    bool not_compiled = (e->ext == NULL);

		    if (not_compiled) {
			e->ext = (regex_t *)nmalloc(sizeof(regex_t));
			regcomp(e->ext, fixbounds(e->ext_regex), REG_EXTENDED);
		    }
#ifdef DEBUG
		    fprintf(stderr, "Matching regex \"%s\" against \"%s\"\n", e->ext_regex, magicstring);
#endif
		    /* Set colorstrings if we match the magic-string regex. */
		    if (magicstring && regexec(e->ext, magicstring, 0, NULL, 0) == 0) {
			openfile->syntax = tmpsyntax;
			openfile->colorstrings = tmpsyntax->color;
			break;
		    }

		    if (not_compiled)
			nfreeregex(&e->ext);
		}
		if (openfile->syntax != NULL)
		    break;
	    }
	    if (stat(openfile->filename, &fileinfo) == 0)
		magic_close(cookie);
	}
#endif /* HAVE_LIBMAGIC */
    }

    /* If we didn't find any syntax yet, and we do have a default one,
     * use it. */
    if (openfile->colorstrings == NULL && defcolor != NULL) {
	openfile->syntax = defsyntax;
	openfile->colorstrings = defcolor;
    }

    for (tmpcolor = openfile->colorstrings; tmpcolor != NULL;
	tmpcolor = tmpcolor->next) {
	/* tmpcolor->start_regex and tmpcolor->end_regex have already
	 * been checked for validity elsewhere.  Compile their specified
	 * regexes if we haven't already. */
	if (tmpcolor->start == NULL) {
	    tmpcolor->start = (regex_t *)nmalloc(sizeof(regex_t));
	    regcomp(tmpcolor->start, fixbounds(tmpcolor->start_regex),
		REG_EXTENDED | (tmpcolor->icase ? REG_ICASE : 0));
	}

	if (tmpcolor->end_regex != NULL && tmpcolor->end == NULL) {
	    tmpcolor->end = (regex_t *)nmalloc(sizeof(regex_t));
	    regcomp(tmpcolor->end, fixbounds(tmpcolor->end_regex),
		REG_EXTENDED | (tmpcolor->icase ? REG_ICASE : 0));
	}
    }
}

/* Reset the multicolor info cache for records for any lines which need
 * to be recalculated. */
void reset_multis_after(filestruct *fileptr, int mindex)
{
    filestruct *oof;
    for (oof = fileptr->next; oof != NULL; oof = oof->next) {
	alloc_multidata_if_needed(oof);
	if (oof->multidata[mindex] != CNONE)
	    oof->multidata[mindex] = -1;
	else
	    break;
    }
    for (; oof != NULL; oof = oof->next) {
	alloc_multidata_if_needed(oof);
	if (oof->multidata[mindex] == CNONE)
	    oof->multidata[mindex] = -1;
	else
	    break;
    }
    edit_refresh_needed = TRUE;
}

void reset_multis_before(filestruct *fileptr, int mindex)
{
    filestruct *oof;
    for (oof = fileptr->prev; oof != NULL; oof = oof->prev) {
	alloc_multidata_if_needed(oof);
	if (oof->multidata[mindex] != CNONE)
	    oof->multidata[mindex] = -1;
	else
	    break;
    }
    for (; oof != NULL; oof = oof->prev) {
	alloc_multidata_if_needed(oof);
	if (oof->multidata[mindex] == CNONE)
	    oof->multidata[mindex] = -1;
	else
	    break;
    }
    edit_refresh_needed = TRUE;
}

/* Reset one multiline regex info. */
void reset_multis_for_id(filestruct *fileptr, int num)
{
    reset_multis_before(fileptr, num);
    reset_multis_after(fileptr, num);
    fileptr->multidata[num] = -1;
}

/* Reset multi-line strings around the filestruct fileptr, trying to be
 * smart about stopping.  Bool force means: reset everything regardless,
 * useful when we don't know how much screen state has changed. */
void reset_multis(filestruct *fileptr, bool force)
{
    int nobegin, noend;
    regmatch_t startmatch, endmatch;
    const colortype *tmpcolor = openfile->colorstrings;

    if (!openfile->syntax)
	return;

    for (; tmpcolor != NULL; tmpcolor = tmpcolor->next) {
	/* If it's not a multi-line regex, amscray. */
	if (tmpcolor->end == NULL)
	    continue;

	alloc_multidata_if_needed(fileptr);

	if (force == FALSE) {
	    /* Check whether the multidata still matches the current situation. */
	    nobegin = regexec(tmpcolor->start, fileptr->data, 1, &startmatch, 0);
	    noend = regexec(tmpcolor->end, fileptr->data, 1, &endmatch, 0);
	    if ((fileptr->multidata[tmpcolor->id] == CWHOLELINE ||
			fileptr->multidata[tmpcolor->id] == CNONE) &&
			nobegin && noend)
		continue;
	    else if (fileptr->multidata[tmpcolor->id] == CSTARTENDHERE &&
			!nobegin && !noend && startmatch.rm_so < endmatch.rm_so)
		continue;
	    else if (fileptr->multidata[tmpcolor->id] == CBEGINBEFORE &&
			nobegin && !noend)
		continue;
	    else if (fileptr->multidata[tmpcolor->id] == CENDAFTER &&
			!nobegin && noend)
		continue;
	}

	/* If we got here, things have changed. */
	reset_multis_for_id(fileptr, tmpcolor->id);
    }
}

#endif /* !DISABLE_COLOR */
