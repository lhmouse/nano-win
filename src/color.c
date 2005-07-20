/* $Id$ */
/**************************************************************************
 *   color.c                                                              *
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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include "proto.h"

#ifdef ENABLE_COLOR

/* For each syntax list entry, we go through the list of colors and
 * assign color pairs. */
void set_colorpairs(void)
{
    const syntaxtype *this_syntax = syntaxes;

    for (; this_syntax != NULL; this_syntax = this_syntax->next) {
	colortype *this_color = this_syntax->color;
	int color_pair = 1;

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
		this_color->pairnum = color_pair;
		color_pair++;
	    }
	}
    }
}

/* Initialize the color information. */
void color_init(void)
{
    assert(openfile != NULL);

    if (has_colors() && can_change_color()) {
	const colortype *tmpcolor;
#ifdef HAVE_USE_DEFAULT_COLORS
	bool defok;
#endif

	start_color();

#ifdef HAVE_USE_DEFAULT_COLORS
	/* Add in colors, if available. */
	defok = (use_default_colors() != ERR);
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
	    fprintf(stderr, "init_pair(): fg = %hd, bg = %hd\n",
		tmpcolor->fg, tmpcolor->bg);
#endif
	}
    }
}

/* Update the color information based on the current filename. */
void color_update(void)
{
    const syntaxtype *tmpsyntax;
    colortype *tmpcolor;

    assert(openfile != NULL);

    openfile->colorstrings = NULL;
    for (tmpsyntax = syntaxes; tmpsyntax != NULL;
	tmpsyntax = tmpsyntax->next) {
	const exttype *e;

	for (e = tmpsyntax->extensions; e != NULL; e = e->next) {
	    /* Set colorstrings if we matched the extension regex. */
	    if (regexec(&e->ext, openfile->filename, 0, NULL, 0) == 0)
		openfile->colorstrings = tmpsyntax->color;

	    if (openfile->colorstrings != NULL)
		break;
	}
    }

    /* If we haven't found a match, use the override string. */
    if (openfile->colorstrings == NULL && syntaxstr != NULL) {
	for (tmpsyntax = syntaxes; tmpsyntax != NULL;
		tmpsyntax = tmpsyntax->next) {
	    if (mbstrcasecmp(tmpsyntax->desc, syntaxstr) == 0)
		openfile->colorstrings = tmpsyntax->color;

	    if (openfile->colorstrings != NULL)
		break;
	}
    }

    /* tmpcolor->start_regex and tmpcolor->end_regex have already been
     * checked for validity elsewhere.  Compile their associated regexes
     * if we haven't already. */
    for (tmpcolor = openfile->colorstrings; tmpcolor != NULL;
	tmpcolor = tmpcolor->next) {
	if (tmpcolor->start_regex != NULL) {
	    tmpcolor->start = (regex_t *)nmalloc(sizeof(regex_t));
	    regcomp(tmpcolor->start, tmpcolor->start_regex,
		REG_EXTENDED | (tmpcolor->icase ? REG_ICASE : 0));
	}

	if (tmpcolor->end_regex != NULL) {
	    tmpcolor->end = (regex_t *)nmalloc(sizeof(regex_t));
	    regcomp(tmpcolor->end, tmpcolor->end_regex,
		REG_EXTENDED | (tmpcolor->icase ? REG_ICASE : 0));
	}
    }
}

#endif /* ENABLE_COLOR */
