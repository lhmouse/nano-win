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

int do_first_line(void)
{
    int old_pww = placewewant;
    current = fileage;
    placewewant = 0;
    current_x = 0;
    if (edittop != fileage || need_vertical_update(old_pww))
	edit_update(current, TOP);
    return 1;
}

int do_last_line(void)
{
    int old_pww = placewewant;
    current = filebot;
    placewewant = 0;
    current_x = 0;
    if (edittop->lineno + (editwinrows / 2) != filebot->lineno ||
	need_vertical_update(old_pww))
	edit_update(current, CENTER);
    return 1;
}

int do_home(void)
{
    int old_pww = placewewant;
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
    if (need_horizontal_update(old_pww))
	update_line(current, current_x);
    return 1;
}

int do_end(void)
{
    int old_pww = placewewant;
    current_x = strlen(current->data);
    placewewant = xplustabs();
    check_statblank();
    if (need_horizontal_update(old_pww))
	update_line(current, current_x);
    return 1;
}

int do_page_up(void)
{
    int new_pww = placewewant;
    const filestruct *old_current = current;
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If the first line of the file is onscreen, move current up there
     * and put the cursor at the beginning of the line. */
    if (edittop == fileage) {
	current = fileage;
	new_pww = 0;
    } else {
	edit_scroll(UP, editwinrows - 2);

#ifndef NANO_SMALL
	/* If we're in smooth scrolling mode and there's at least one
	 * page of text left, move the current line of the edit window
	 * up a page. */
	if (ISSET(SMOOTHSCROLL) && current->lineno > editwinrows - 2) {
	    int i;
	    for (i = 0; i < editwinrows - 2; i++)
		current = current->prev;
	}
	/* If we're not in smooth scrolling mode or there isn't at least
	 * one page of text left, put the cursor at the beginning of the
	 * top line of the edit window, as Pico does. */
	else {
#endif
	    current = edittop;
	    new_pww = 0;
#ifndef NANO_SMALL
	}
#endif
    }

    /* Get the equivalent x-coordinate of the new line. */
    current_x = actual_x(current->data, new_pww);

    /* Update all the lines that need to be updated, and then set
     * placewewant, so that the update will work properly. */
    edit_redraw(old_current);
    placewewant = new_pww;

    check_statblank();
    return 1;
}

int do_page_down(void)
{
    int new_pww = placewewant;
    const filestruct *old_current = current;
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If the last line of the file is onscreen, move current down
     * there and put the cursor at the beginning of the line. */
    if (edittop->lineno + editwinrows > filebot->lineno) {
	current = filebot;
	new_pww = 0;
    } else {
	edit_scroll(DOWN, editwinrows - 2);

#ifndef NANO_SMALL
	/* If we're in smooth scrolling mode and there's at least one
	 * page of text left, move the current line of the edit window
	 * down a page. */
	if (ISSET(SMOOTHSCROLL) && current->lineno + editwinrows - 2 <=
		filebot->lineno) {
	    int i;
	    for (i = 0; i < editwinrows - 2; i++)
		current = current->next;
	}
	/* If we're not in smooth scrolling mode or there isn't at least
	 * one page of text left, put the cursor at the beginning of the
	 * top line of the edit window, as Pico does. */
	else {
#endif
	    current = edittop;
	    new_pww = 0;
#ifndef NANO_SMALL
	}
#endif
    }

    /* Get the equivalent x-coordinate of the new line. */
    current_x = actual_x(current->data, new_pww);

    /* Update all the lines that need to be updated, and then set
     * placewewant, so that the update will work properly. */
    edit_redraw(old_current);
    placewewant = new_pww;

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

    /* If we're on the first row of the edit window, scroll up one line
     * if we're in smooth scrolling mode, or up half a page if we're
     * not. */
    if (current_y == 0)
	edit_scroll(UP,
#ifndef NANO_SMALL
		ISSET(SMOOTHSCROLL) ? 1 :
#endif
		editwinrows / 2);

    /* Update the lines left alone by edit_scroll(): the line we were on
     * before and the line we're on now.  The former needs to be redrawn
     * if we're not on the first page, and the latter needs to be
     * drawn. */
    if (need_vertical_update(0))
	update_line(current->next, 0);
    update_line(current, current_x);

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

    /* If we're on the last row of the edit window, scroll down one line
     * if we're in smooth scrolling mode, or down half a page if we're
     * not. */
    if (current_y == editwinrows - 1)
	edit_scroll(DOWN,
#ifndef NANO_SMALL
		ISSET(SMOOTHSCROLL) ? 1 :
#endif
		editwinrows / 2);

    /* Update the lines left alone by edit_scroll(): the line we were on
     * before and the line we're on now.  The former needs to be redrawn
     * if we're not on the first page, and the latter needs to be
     * drawn. */
    if (need_vertical_update(0))
	update_line(current->prev, 0);
    update_line(current, current_x);

    return 1;
}

int do_left(int allow_update)
{
    int old_pww = placewewant;
    if (current_x > 0)
	current_x--;
    else if (current != fileage) {
	do_up();
	current_x = strlen(current->data);
    }
    placewewant = xplustabs();
    check_statblank();
    if (allow_update && need_horizontal_update(old_pww))
	update_line(current, current_x);
    return 1;
}

int do_left_void(void)
{
    return do_left(TRUE);
}

int do_right(int allow_update)
{
    int old_pww = placewewant;
    assert(current_x <= strlen(current->data));

    if (current->data[current_x] != '\0')
	current_x++;
    else if (current->next != NULL) {
	do_down();
	current_x = 0;
    }
    placewewant = xplustabs();
    check_statblank();
    if (allow_update && need_horizontal_update(old_pww))
	update_line(current, current_x);
    return 1;
}

int do_right_void(void)
{
    return do_right(TRUE);
}
