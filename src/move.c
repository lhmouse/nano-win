/* $Id$ */
/**************************************************************************
 *   move.c                                                               *
 *                                                                        *
 *   Copyright (C) 1999-2004 Chris Allegretta                             *
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
#include <ctype.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

int do_home(void)
{
#ifndef NANO_SMALL
    if (ISSET(SMART_HOME)) {
	int old_current_x = current_x;

	for (current_x = 0; isblank(current->data[current_x]) &&
		current->data[current_x] != '\0'; current_x++)
	    ;

	if (current_x == old_current_x || current->data[current_x] == '\0')
	    current_x = 0;

	placewewant = xplustabs();
    } else {
#endif
	current_x = 0;
	placewewant = 0;
#ifndef NANO_SMALL
    }
#endif
    check_statblank();
    update_line(current, current_x);
    return 1;
}

int do_end(void)
{
    current_x = strlen(current->data);
    placewewant = xplustabs();
    check_statblank();
    update_line(current, current_x);
    return 1;
}

int do_page_up(void)
{
    int i;

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If edittop is the first line of the file, move current up there
     * and put the cursor at the beginning of the line. */
    if (edittop == fileage) {
	current = fileage;
	placewewant = 0;
    } else {
	/* Move the top line of the edit window up a page. */
	for (i = 0; i < editwinrows - 2 && edittop->prev != NULL; i++)
	    edittop = edittop->prev;
#ifndef NANO_SMALL
	/* If we're in smooth scrolling mode and there was at least one
	 * page of text left, move the current line of the edit window
	 * up a page. */
	if (ISSET(SMOOTHSCROLL) && current->lineno > editwinrows - 2)
	    for (i = 0; i < editwinrows - 2; i++)
		current = current->prev;
	/* If we're not in smooth scrolling mode and there was at least
	 * one page of text left, put the cursor at the beginning of the
	 * top line of the edit window, as Pico does. */
	else {
#endif
	    current = edittop;
	    placewewant = 0;
#ifndef NANO_SMALL
	}
#endif
    }
    /* Get the equivalent x-coordinate of the new line. */
    current_x = actual_x(current->data, placewewant);

    edit_refresh();

    check_statblank();
    return 1;
}

int do_page_down(void)
{
    int i;

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If the last line of the file is onscreen, move current down
     * there and put the cursor at the beginning of the line. */
    if (edittop->lineno + editwinrows > filebot->lineno) {
	current = filebot;
	placewewant = 0;
    } else {
	/* Move the top line of the edit window down a page. */
	for (i = 0; i < editwinrows - 2; i++)
	    edittop = edittop->next;
#ifndef NANO_SMALL
	/* If we're in smooth scrolling mode and there was at least one
	 * page of text left, move the current line of the edit window
	 * down a page. */
	if (ISSET(SMOOTHSCROLL) && current->lineno + editwinrows - 2 <= filebot->lineno)
	    for (i = 0; i < editwinrows - 2; i++)
		current = current->next;
	/* If we're not in smooth scrolling mode and there was at least
	 * one page of text left, put the cursor at the beginning of the
	 * top line of the edit window, as Pico does. */
	else {
#endif
	    current = edittop;
	    placewewant = 0;
#ifndef NANO_SMALL
	}
#endif
    }
    /* Get the equivalent x-coordinate of the new line. */
    current_x = actual_x(current->data, placewewant);

    edit_refresh();

    check_statblank();
    return 1;
}

int do_up(void)
{
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    check_statblank();

    if (current->prev == NULL)
	return 0;

    assert(current_y == current->lineno - edittop->lineno);
    current = current->prev;
    current_x = actual_x(current->data, placewewant);
    if (current_y > 0) {
	update_line(current->next, 0);
	    /* It was necessary to change current first, so that the
	     * mark display will change! */
	update_line(current, current_x);
    } else
#ifndef NANO_SMALL
    if (ISSET(SMOOTHSCROLL))
	edit_update(current, TOP);
    else
#endif
	edit_update(current, CENTER);
    return 1;
}

/* Return value 1 means we moved down, 0 means we were already at the
 * bottom. */
int do_down(void)
{
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    check_statblank();

    if (current->next == NULL)
	return 0;

    assert(current_y == current->lineno - edittop->lineno);
    current = current->next;
    current_x = actual_x(current->data, placewewant);

    /* Note that current_y is zero-based.  This test checks for the
     * cursor's being not on the last row of the edit window. */
    if (current_y != editwinrows - 1) {
	update_line(current->prev, 0);
	update_line(current, current_x);
    } else
#ifndef NANO_SMALL
    if (ISSET(SMOOTHSCROLL))
	/* In this case current_y does not change.  The cursor remains
	 * at the bottom of the edit window. */
	edit_update(edittop->next, TOP);
    else
#endif
	edit_update(current, CENTER);
    return 1;
}

int do_left(void)
{
    if (current_x > 0)
	current_x--;
    else if (current != fileage) {
	do_up();
	current_x = strlen(current->data);
    }
    placewewant = xplustabs();
    check_statblank();
    update_line(current, current_x);
    return 1;
}

int do_right(void)
{
    assert(current_x <= strlen(current->data));

    if (current->data[current_x] != '\0')
	current_x++;
    else if (current->next != NULL) {
	do_down();
	current_x = 0;
    }
    placewewant = xplustabs();
    check_statblank();
    update_line(current, current_x);
    return 1;
}
