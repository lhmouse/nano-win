/* $Id$ */
/**************************************************************************
 *   color.c                                                              *
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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "proto.h"
#include "nano.h"

#ifdef ENABLE_COLOR

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

void do_colorinit(void)
{
    int i;
    colortype *tmpcolor = NULL, *beforenow = NULL;
    int defok = 0;

    if (has_colors()) {
	start_color();
	/* Add in colors, if available */

#ifdef HAVE_USE_DEFAULT_COLORS
 	if (use_default_colors() != ERR)
	    defok = 1;
#endif

	i = 1;
	for (tmpcolor = colorstrings; tmpcolor != NULL; 
		tmpcolor = tmpcolor->next) {

	    for (beforenow = colorstrings; beforenow != NULL
		 && beforenow != tmpcolor && 
		 (beforenow->fg != tmpcolor->fg || beforenow->bg != tmpcolor->bg
		 || beforenow->bright != tmpcolor->bright);
		beforenow = beforenow->next)
		;

	    if (beforenow != NULL && beforenow != tmpcolor) {
		tmpcolor->pairnum = beforenow->pairnum;
		continue;
	    }
	    
	    if (defok && tmpcolor->bg == -1)
		init_pair(i, tmpcolor->fg, -1);
            else if (tmpcolor->bg == -1)
		init_pair(i, tmpcolor->fg, COLOR_BLACK);
	    else /* They picked a fg and bg color */
		init_pair(i, tmpcolor->fg, tmpcolor->bg);

#ifdef DEBUG
	    fprintf(stderr, _("Running init_pair with fg = %d and bg = %d\n"), tmpcolor->fg, tmpcolor->bg);
#endif

	    tmpcolor->pairnum = i;
	    i++;
	}
    }

    return;
}

/* Update the color information based on the current filename */
void update_color(void)
{
    syntaxtype *tmpsyntax;

    colorstrings = NULL;
    for (tmpsyntax = syntaxes; tmpsyntax != NULL; tmpsyntax = tmpsyntax->next) {
	exttype *e;
	for (e = tmpsyntax->extensions; e != NULL; e = e->next) {
	    regcomp(&syntaxfile_regexp, e->val, REG_EXTENDED);

	    /* Set colorstrings if we matched the extension regex */
            if (!regexec(&syntaxfile_regexp, filename, 1, synfilematches, 0))
		colorstrings = tmpsyntax->color;

	    regfree(&syntaxfile_regexp);
	}
    }

    /* if we haven't found a match, use the override string */
    if (colorstrings == NULL && syntaxstr != NULL) {
	for (tmpsyntax = syntaxes; tmpsyntax != NULL; 
	     tmpsyntax = tmpsyntax->next) {
	    if (!strcasecmp(tmpsyntax->desc, syntaxstr))
		colorstrings = tmpsyntax->color;
	}
    }
    do_colorinit();
    edit_refresh();
}

#endif /* ENABLE_COLOR */
