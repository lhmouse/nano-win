/* $Id$ */
/**************************************************************************
 *   move.c                                                               *
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
#include <ctype.h>
#include <assert.h>
#include "proto.h"

void do_first_line(void)
{
    size_t pww_save = placewewant;
    current = fileage;
    placewewant = 0;
    current_x = 0;
    if (edittop != fileage || need_vertical_update(pww_save))
	edit_update(TOP);
}

void do_last_line(void)
{
    size_t pww_save = placewewant;
    current = filebot;
    placewewant = 0;
    current_x = 0;
    if (edittop->lineno + (editwinrows / 2) != filebot->lineno ||
	need_vertical_update(pww_save))
	edit_update(CENTER);
}

void do_home(void)
{
    size_t pww_save = placewewant;
#ifndef NANO_SMALL
    if (ISSET(SMART_HOME)) {
	size_t current_x_save = current_x;

	current_x = indent_length(current->data);

	if (current_x == current_x_save ||
		current->data[current_x] == '\0')
	    current_x = 0;

	placewewant = xplustabs();
    } else {
#endif
	current_x = 0;
	placewewant = 0;
#ifndef NANO_SMALL
    }
#endif
    check_statusblank();
    if (need_horizontal_update(pww_save))
	update_line(current, current_x);
}

void do_end(void)
{
    size_t pww_save = placewewant;
    current_x = strlen(current->data);
    placewewant = xplustabs();
    check_statusblank();
    if (need_horizontal_update(pww_save))
	update_line(current, current_x);
}

void do_page_up(void)
{
    size_t pww_save = placewewant;
    const filestruct *current_save = current;
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If the first line of the file is onscreen, move current up there
     * and put the cursor at the beginning of the line. */
    if (edittop == fileage) {
	current = fileage;
	placewewant = 0;
    } else {
	edit_scroll(UP, editwinrows - 2);

#ifndef NANO_SMALL
	/* If we're in smooth scrolling mode and there's at least one
	 * page of text left, move the current line of the edit window
	 * up a page. */
	if (ISSET(SMOOTH_SCROLL) && current->lineno > editwinrows - 2) {
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
	    placewewant = 0;
#ifndef NANO_SMALL
	}
#endif
    }

    /* Get the equivalent x-coordinate of the new line. */
    current_x = actual_x(current->data, placewewant);

    /* Update all the lines that need to be updated. */
    edit_redraw(current_save, pww_save);

    check_statusblank();
}

void do_page_down(void)
{
    size_t pww_save = placewewant;
    const filestruct *current_save = current;
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If the last line of the file is onscreen, move current down
     * there and put the cursor at the beginning of the line. */
    if (edittop->lineno + editwinrows > filebot->lineno) {
	current = filebot;
	placewewant = 0;
    } else {
	edit_scroll(DOWN, editwinrows - 2);

#ifndef NANO_SMALL
	/* If we're in smooth scrolling mode and there's at least one
	 * page of text left, move the current line of the edit window
	 * down a page. */
	if (ISSET(SMOOTH_SCROLL) && current->lineno + editwinrows - 2 <=
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
	    placewewant = 0;
#ifndef NANO_SMALL
	}
#endif
    }

    /* Get the equivalent x-coordinate of the new line. */
    current_x = actual_x(current->data, placewewant);

    /* Update all the lines that need to be updated. */
    edit_redraw(current_save, pww_save);

    check_statusblank();
}

void do_up(void)
{
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    check_statusblank();

    if (current->prev == NULL)
	return;

    assert(current_y == current->lineno - edittop->lineno);

    current = current->prev;
    current_x = actual_x(current->data, placewewant);

    /* If we're on the first row of the edit window, scroll up one line
     * if we're in smooth scrolling mode, or up half a page if we're
     * not. */
    if (current_y == 0)
	edit_scroll(UP,
#ifndef NANO_SMALL
		ISSET(SMOOTH_SCROLL) ? 1 :
#endif
		editwinrows / 2);

    /* Update the lines left alone by edit_scroll(): the line we were on
     * before and the line we're on now.  The former needs to be redrawn
     * if we're not on the first page, and the latter needs to be
     * drawn. */
    if (need_vertical_update(0))
	update_line(current->next, 0);
    update_line(current, current_x);
}

void do_down(void)
{
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    check_statusblank();

    if (current->next == NULL)
	return;

    assert(current_y == current->lineno - edittop->lineno);

    current = current->next;
    current_x = actual_x(current->data, placewewant);

    /* If we're on the last row of the edit window, scroll down one line
     * if we're in smooth scrolling mode, or down half a page if we're
     * not. */
    if (current_y == editwinrows - 1)
	edit_scroll(DOWN,
#ifndef NANO_SMALL
		ISSET(SMOOTH_SCROLL) ? 1 :
#endif
		editwinrows / 2);

    /* Update the lines left alone by edit_scroll(): the line we were on
     * before and the line we're on now.  The former needs to be redrawn
     * if we're not on the first page, and the latter needs to be
     * drawn. */
    if (need_vertical_update(0))
	update_line(current->prev, 0);
    update_line(current, current_x);
}

void do_left(bool allow_update)
{
    size_t pww_save = placewewant;
    if (current_x > 0)
	current_x = move_mbleft(current->data, current_x);
    else if (current != fileage) {
	do_up();
	current_x = strlen(current->data);
    }
    placewewant = xplustabs();
    check_statusblank();
    if (allow_update && need_horizontal_update(pww_save))
	update_line(current, current_x);
}

void do_left_void(void)
{
    do_left(TRUE);
}

void do_right(bool allow_update)
{
    size_t pww_save = placewewant;
    assert(current_x <= strlen(current->data));

    if (current->data[current_x] != '\0')
	current_x = move_mbright(current->data, current_x);
    else if (current->next != NULL) {
	do_down();
	current_x = 0;
    }
    placewewant = xplustabs();
    check_statusblank();
    if (allow_update && need_horizontal_update(pww_save))
	update_line(current, current_x);
}

void do_right_void(void)
{
    do_right(TRUE);
}
