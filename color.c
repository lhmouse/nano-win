/* $Id$ */
/**************************************************************************
 *   color.c                                                              *
 *                                                                        *
 *   Copyright (C) 1999 Chris Allegretta                                  *
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
    if (has_colors()) {
	start_color();
	/* Add in colors, if available */
#ifdef HAVE_USE_DEFAULT_COLORS
	/* Use if at all possible for transparent terminals =-) */
	use_default_colors();
#endif
	/* Some defaults values to play with */

	/*  Okay I'll be nice and comment these out for the commit =)
	colorinit_one(COLOR_TITLEBAR, COLOR_GREEN, COLOR_BLUE, 1);
	colorinit_one(COLOR_BOTTOMBARS, COLOR_GREEN, COLOR_BLUE, 1);
	colorinit_one(COLOR_STATUSBAR, COLOR_BLACK, COLOR_CYAN, 0);
	colorinit_one(COLOR_TEXT, COLOR_WHITE, COLOR_BLACK, 0);
	colorinit_one(COLOR_MARKER, COLOR_BLACK, COLOR_CYAN, 0);
	*/
    }

    return 0;
}

#endif /* ENABLE_COLOR */

