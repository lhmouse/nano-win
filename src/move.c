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
    size_t pww_save = openfile->placewewant;

    openfile->current = openfile->fileage;
    openfile->current_x = 0;
    openfile->placewewant = 0;

    if (openfile->edittop != openfile->fileage ||
	need_vertical_update(pww_save))
	edit_update(TOP);
}

void do_last_line(void)
{
    size_t pww_save = openfile->placewewant;

    openfile->current = openfile->filebot;
    openfile->current_x = 0;
    openfile->placewewant = 0;

    if (openfile->edittop->lineno + (editwinrows / 2) !=
	openfile->filebot->lineno || need_vertical_update(pww_save))
	edit_update(CENTER);
}

void do_home(void)
{
    size_t pww_save = openfile->placewewant;

#ifndef NANO_SMALL
    if (ISSET(SMART_HOME)) {
	size_t current_x_save = openfile->current_x;

	openfile->current_x = indent_length(openfile->current->data);

	if (openfile->current_x == current_x_save ||
		openfile->current->data[openfile->current_x] == '\0')
	    openfile->current_x = 0;

	openfile->placewewant = xplustabs();
    } else {
#endif
	openfile->current_x = 0;
	openfile->placewewant = 0;
#ifndef NANO_SMALL
    }
#endif

    check_statusblank();

    if (need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}

void do_end(void)
{
    size_t pww_save = openfile->placewewant;

    openfile->current_x = strlen(openfile->current->data);
    openfile->placewewant = xplustabs();

    check_statusblank();

    if (need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}

void do_page_up(void)
{
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If the first line of the file is onscreen, move current up there
     * and put the cursor at the beginning of the line. */
    if (openfile->edittop == openfile->fileage) {
	openfile->current = openfile->fileage;
	openfile->current_x = 0;
	openfile->placewewant = 0;
    } else {
#ifndef NANO_SMALL
	/* If we're in smooth scrolling mode and there's at least one
	 * page of text left, move the current line of the edit window
	 * up a page, and then get the equivalent x-coordinate of the
	 * current line. */
	if (ISSET(SMOOTH_SCROLL) && openfile->current->lineno >
		editwinrows - 2) {
	    int i = 0;
	    for (; i < editwinrows - 2; i++)
		openfile->current = openfile->current->prev;

	    openfile->current_x = actual_x(openfile->current->data,
		openfile->placewewant);
	}
	/* If we're not in smooth scrolling mode or there isn't at least
	 * one page of text left, put the cursor at the beginning of the
	 * top line of the edit window, as Pico does. */
	else {
#endif
	    openfile->current = openfile->edittop;
	    openfile->current_x = 0;
	    openfile->placewewant = 0;
#ifndef NANO_SMALL
	}
#endif

	/* Scroll the edit window down a page. */
	edit_scroll(UP, editwinrows - 2);
    }

    check_statusblank();
}

void do_page_down(void)
{
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If the last line of the file is onscreen, move current down
     * there and put the cursor at the beginning of the line. */
    if (openfile->edittop->lineno + editwinrows >
	openfile->filebot->lineno) {
	openfile->current = openfile->filebot;
	openfile->current_x = 0;
	openfile->placewewant = 0;
    } else {
#ifndef NANO_SMALL
	/* If we're in smooth scrolling mode and there's at least one
	 * page of text left, move the current line of the edit window
	 * down a page, and then get the equivalent x-coordinate of the
	 * current line. */
	if (ISSET(SMOOTH_SCROLL) && openfile->current->lineno +
		editwinrows - 2 <= openfile->filebot->lineno) {
	    int i = 0;

	    for (; i < editwinrows - 2; i++)
		openfile->current = openfile->current->next;

	    openfile->current_x = actual_x(openfile->current->data,
		openfile->placewewant);
	}
	/* If we're not in smooth scrolling mode or there isn't at least
	 * one page of text left, put the cursor at the beginning of the
	 * top line of the edit window, as Pico does. */
	else {
#endif
	    openfile->current = openfile->edittop;
	    openfile->current_x = 0;
	    openfile->placewewant = 0;
#ifndef NANO_SMALL
	}
#endif

	/* Scroll the edit window down a page. */
	edit_scroll(DOWN, editwinrows - 2);
    }

    check_statusblank();
}

void do_up(void)
{
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    check_statusblank();

    /* If we're at the top of the file, get out. */
    if (openfile->current->prev == NULL)
	return;

    assert(openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

    /* Move the current line of the edit window up, and then get the
     * equivalent x-coordinate of the current line. */
    openfile->current = openfile->current->prev;
    openfile->current_x = actual_x(openfile->current->data,
	openfile->placewewant);

    /* If we're on the first row of the edit window, scroll the edit
     * window up one line if we're in smooth scrolling mode, or up half
     * a page if we're not. */
    if (openfile->current_y == 0)
	edit_scroll(UP,
#ifndef NANO_SMALL
		ISSET(SMOOTH_SCROLL) ? 1 :
#endif
		editwinrows / 2);
    /* Otherwise, update the line we were on before and the line we're
     * on now.  The former needs to be redrawn if we're not on the first
     * page, and the latter needs to be redrawn unconditionally. */
    else {
	if (need_vertical_update(0))
	    update_line(openfile->current->next, 0);
	update_line(openfile->current, openfile->current_x);
    }
}

void do_down(void)
{
#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    check_statusblank();

    /* If we're at the bottom of the file, get out. */
    if (openfile->current->next == NULL)
	return;

    assert(openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

    /* Move the current line of the edit window down, and then get the
     * equivalent x-coordinate of the current line. */
    openfile->current = openfile->current->next;
    openfile->current_x = actual_x(openfile->current->data,
	openfile->placewewant);

    /* If we're on the last row of the edit window, scroll the edit
     * window down one line if we're in smooth scrolling mode, or down
     * half a page if we're not. */
    if (openfile->current_y == editwinrows - 1)
	edit_scroll(DOWN,
#ifndef NANO_SMALL
		ISSET(SMOOTH_SCROLL) ? 1 :
#endif
		editwinrows / 2);
    /* Otherwise, update the line we were on before and the line we're
     * on now.  The former needs to be redrawn if we're not on the first
     * page, and the latter needs to be redrawn unconditionally. */
    else {
	if (need_vertical_update(0))
	    update_line(openfile->current->prev, 0);
	update_line(openfile->current, openfile->current_x);
    }
}

void do_left(bool allow_update)
{
    size_t pww_save = openfile->placewewant;

    if (openfile->current_x > 0)
	openfile->current_x = move_mbleft(openfile->current->data,
		openfile->current_x);
    else if (openfile->current != openfile->fileage) {
	do_up();
	openfile->current_x = strlen(openfile->current->data);
    }

    openfile->placewewant = xplustabs();

    check_statusblank();

    if (allow_update && need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}

void do_left_void(void)
{
    do_left(TRUE);
}

void do_right(bool allow_update)
{
    size_t pww_save = openfile->placewewant;

    assert(openfile->current_x <= strlen(openfile->current->data));

    if (openfile->current->data[openfile->current_x] != '\0')
	openfile->current_x = move_mbright(openfile->current->data,
		openfile->current_x);
    else if (openfile->current->next != NULL) {
	do_down();
	openfile->current_x = 0;
    }

    openfile->placewewant = xplustabs();

    check_statusblank();

    if (allow_update && need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}

void do_right_void(void)
{
    do_right(TRUE);
}
