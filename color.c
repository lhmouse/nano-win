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

#ifdef ENABLE_COLOR

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

void color_on(WINDOW *win, int whatever)
{
    /* Temporary fallback, if the color value hasn't been set, 
	turn on hilighting */
    if (!colors[whatever - FIRST_COLORNUM].set) {
	wattron(win, A_REVERSE);
	return;
    }

    if (colors[whatever - FIRST_COLORNUM].bold)
        wattron(win, A_BOLD);

     /* If the foreground color is black, we've switched fg and bg (see
	the comment in colorinit_one() about this) so turn on reverse so
	it looks like it's supposed to */
    if (colors[whatever - FIRST_COLORNUM].fg == COLOR_BLACK)
	wattron(win, A_REVERSE);

    wattron(win, COLOR_PAIR(whatever));

}

void color_off(WINDOW *win, int whatever)
{
    if (!colors[whatever - FIRST_COLORNUM].set) {
	wattroff(win, A_REVERSE);
	return;
    }

    wattroff(win, COLOR_PAIR(whatever));

    if (colors[whatever - FIRST_COLORNUM].fg == COLOR_BLACK)
	wattroff(win, A_REVERSE);

    if (colors[whatever - FIRST_COLORNUM].bold)
        wattroff(win, A_BOLD);
}


void colorinit_one(int colortoset, short fg, short bg, int bold)
{
    colors[colortoset - FIRST_COLORNUM].fg = fg;
    colors[colortoset - FIRST_COLORNUM].bg = bg;
    colors[colortoset - FIRST_COLORNUM].bold = bold;
    colors[colortoset - FIRST_COLORNUM].set = 1;

     /* Okay, so if they want a black foreground, do a switch on the fg 
	and bg, because specifying black as the foreground color gives
	this ugly grey color. Then in color_on we will turn A_REVERSE
	which is probably what they want it to look like... */
    if (fg == COLOR_BLACK)
	init_pair(colortoset, bg, fg);
    else
	init_pair(colortoset, fg, bg);
}

int do_colorinit(void)
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
	    fprintf(stderr, "Running init_pair with fg = %d and bg = %d\n", tmpcolor->fg, tmpcolor->bg);
#endif

	    tmpcolor->pairnum = i;
	    i++;
	}
    }

/*
	if (use_default_colors() != ERR) {
	    init_pair(COLOR_BLACK, -1, -1);
	    init_pair(COLOR_GREEN, COLOR_GREEN, -1);
	    init_pair(COLOR_WHITE, COLOR_WHITE, -1);
	    init_pair(COLOR_RED, COLOR_RED, -1);
	    init_pair(COLOR_CYAN, COLOR_CYAN, -1);
	    init_pair(COLOR_MAGENTA, COLOR_MAGENTA, -1);
	    init_pair(COLOR_BLUE, COLOR_BLUE, -1);
	    init_pair(COLOR_YELLOW, COLOR_YELLOW, -1);

	} else {
	    init_pair(COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
	    init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
	    init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
	    init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
	    init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
	    init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
	    init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
	    init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
	}
*/

	/*  Okay I'll be nice and comment these out for the commit =)
	colorinit_one(COLOR_TITLEBAR, COLOR_GREEN, COLOR_BLUE, 1);
	colorinit_one(COLOR_BOTTOMBARS, COLOR_GREEN, COLOR_BLUE, 1);
	colorinit_one(COLOR_STATUSBAR, COLOR_BLACK, COLOR_CYAN, 0);
	colorinit_one(COLOR_TEXT, COLOR_WHITE, COLOR_BLACK, 0);
	colorinit_one(COLOR_MARKER, COLOR_BLACK, COLOR_CYAN, 0);
    }
*/
    return 0;
}

/* Update the color information based on the current filename */
void update_color(void)
{
    syntaxtype *tmpsyntax;

    colorstrings = NULL;
    for (tmpsyntax = syntaxes; tmpsyntax != NULL; tmpsyntax = tmpsyntax->next) {
	exttype *e;
	for (e = tmpsyntax->extensions; e != NULL; e = e->next) {
	    regcomp(&syntaxfile_regexp, e->val, 0);

	    /* Set colorstrings if we matched the extension regex */
            if (!regexec(&syntaxfile_regexp, filename, 1, synfilematches, 0)) 
		colorstrings = tmpsyntax->color; 
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

