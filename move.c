/* $Id$ */
/**************************************************************************
 *   move.c                                                               *
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
#include <assert.h>
#include "proto.h"
#include "nano.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

int do_home(void)
{
    UNSET(KEEP_CUTBUFFER);
    current_x = 0;
    placewewant = 0;
    update_line(current, current_x);
    return 1;
}

int do_end(void)
{
    UNSET(KEEP_CUTBUFFER);
    current_x = strlen(current->data);
    placewewant = xplustabs();
    update_line(current, current_x);
    return 1;
}

void page_up(void)
{
    if (edittop != fileage) {
#ifndef NANO_SMALL
	if (ISSET(SMOOTHSCROLL))
	    edit_update(edittop->prev, TOP);
	else
#endif
	{
	    edit_update(edittop, CENTER);
	    /* Now that we've updated the edit window, edittop might be
	       at the top of the file; if so, just move the cursor up one
	       line and don't center it. */
	    if (edittop != fileage)
		center_cursor();
	    else
		reset_cursor();
	}
    } else
	current_y = 0;

    update_cursor();
}

int do_page_up(void)
{
    int i;

    wrap_reset();
    current_x = 0;
    placewewant = 0;

    if (current == fileage)
	return 0;

    current_y = 0;
    current = edittop;
    for (i = 0; i <= editwinrows - 3 && current->prev != NULL; i++)
	current = current->prev;

    edit_update(current, TOP);
    update_cursor();

    UNSET(KEEP_CUTBUFFER);
    check_statblank();
    return 1;
}

int do_page_down(void)
{
    wrap_reset();
    current_x = 0;
    placewewant = 0;

    if (current == filebot)
	return 0;

    /* AHEM, if we only have a screen or less of text, DON'T do an
       edit_update, just move the cursor to editbot! */
    if (edittop == fileage && editbot == filebot && totlines < editwinrows) {
	current = editbot;
	reset_cursor();
    } else if (editbot != filebot || edittop == fileage) {
	current_y = 0;
	current = editbot;

	if (current->prev != NULL)
	    current = current->prev;
	if (current->prev != NULL)
	    current = current->prev;
	edit_update(current, TOP);
    } else {
	while (current != filebot) {
	    current = current->next;
	    current_y++;
	}
	edit_update(edittop, TOP);
    }

    update_cursor();
    UNSET(KEEP_CUTBUFFER);
    check_statblank();
    return 1;
}

int do_up(void)
{
    wrap_reset();
    if (current->prev != NULL) {
	current_x = actual_x(current->prev, placewewant);
	current = current->prev;
	if (current_y > 0) {
	    update_line(current->next, 0);
		/* It is necessary to change current first, so the mark
		   display will change! */
	    current_y--;
	    update_line(current, current_x);
	} else
	    page_up();
	UNSET(KEEP_CUTBUFFER);
	check_statblank();
    }
    return 1;
}

/* Return value 1 means we moved down, 0 means we were already at the
 * bottom. */
int do_down(void) {
    wrap_reset();
    UNSET(KEEP_CUTBUFFER);
    check_statblank();

    if (current->next == NULL)
	return 0;

    current = current->next;
    current_x = actual_x(current, placewewant);

    /* Note current_y is zero-based.  This test checks for the cursor
     * being on the last row of the edit window. */
    if (current_y == editwinrows - 1) {
#ifndef NANO_SMALL
	if (ISSET(SMOOTHSCROLL)) {
	    /* In this case current_y does not change.  The cursor
	     * remains at the bottom of the edit window. */
	    edittop = edittop->next;
	    editbot = editbot->next;
	    edit_refresh();
	} else
#endif
	{
	    /* Set edittop so editbot->next (or else editbot) is
	     * centered, and set current_y = editwinrows / 2. */
	    edit_update(editbot->next != NULL ? editbot->next : editbot, CENTER);
	    center_cursor();
	}
    } else {
	update_line(current->prev, 0);
	update_line(current, current_x);
	current_y++;
    }
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
    update_line(current, current_x);
    UNSET(KEEP_CUTBUFFER);
    check_statblank();
    return 1;
}

int do_right(void)
{
    assert(current_x <= strlen(current->data));

    if (current->data[current_x] != '\0')
	current_x++;
    else if (current->next) {
	do_down();
	current_x = 0;
    }
    placewewant = xplustabs();
    update_line(current, current_x);
    UNSET(KEEP_CUTBUFFER);
    check_statblank();
    return 1;
}
