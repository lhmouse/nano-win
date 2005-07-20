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
    const filestruct *current_save = openfile->current;
    size_t pww_save = openfile->placewewant;

    check_statusblank();

    openfile->current = openfile->fileage;
    openfile->current_x = 0;
    openfile->placewewant = 0;

    edit_redraw(current_save, pww_save);
}

void do_last_line(void)
{
    const filestruct *current_save = openfile->current;
    size_t pww_save = openfile->placewewant;

    check_statusblank();

    openfile->current = openfile->filebot;
    openfile->current_x = 0;
    openfile->placewewant = 0;
    openfile->current_y = editwinrows - 1;

    edit_redraw(current_save, pww_save);
}

void do_page_up(void)
{
    int i;

    check_statusblank();

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If there's less than a page of text left on the screen, put the
     * cursor at the beginning of the first line of the file, and then
     * update the edit window. */
    if (openfile->current->lineno <= editwinrows - 2) {
	do_first_line();
	return;
    }

    /* If we're not in smooth scrolling mode, put the cursor at the
     * beginning of the top line of the edit window, as Pico does. */
#ifndef NANO_SMALL
    if (!ISSET(SMOOTH_SCROLL)) {
#endif
	openfile->current = openfile->edittop;
	openfile->placewewant = 0;
#ifndef NANO_SMALL
    }
#endif

    for (i = editwinrows - 2; i > 0 && openfile->current->prev != NULL;
	i--)
	openfile->current = openfile->current->prev;

    openfile->current_x = actual_x(openfile->current->data,
	openfile->placewewant);

    /* Scroll the edit window up a page. */
    edit_scroll(UP, editwinrows - 2);
}

void do_page_down(void)
{
    int i;

    check_statusblank();

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If there's less than a page of text left on the screen, put the
     * cursor at the beginning of the last line of the file, and then
     * update the edit window. */
    if (openfile->current->lineno + editwinrows - 2 >=
	openfile->filebot->lineno) {
	do_last_line();
	return;
    }

    /* If we're not in smooth scrolling mode, put the cursor at the
     * beginning of the top line of the edit window, as Pico does. */
#ifndef NANO_SMALL
    if (!ISSET(SMOOTH_SCROLL)) {
#endif
	openfile->current = openfile->edittop;
	openfile->placewewant = 0;
#ifndef NANO_SMALL
    }
#endif

    for (i = editwinrows - 2; i > 0 && openfile->current->next != NULL;
	i--)
	openfile->current = openfile->current->next;

    openfile->current_x = actual_x(openfile->current->data,
	openfile->placewewant);

    /* Scroll the edit window down a page. */
    edit_scroll(DOWN, editwinrows - 2);
}

#ifndef DISABLE_JUSTIFY
/* Move up to the last beginning-of-paragraph line before the current
 * line. */
void do_para_begin(bool allow_update)
{
    const filestruct *current_save = openfile->current;
    const size_t pww_save = openfile->placewewant;

    check_statusblank();

    openfile->current_x = 0;
    openfile->placewewant = 0;

    if (openfile->current->prev != NULL) {
	do {
	    openfile->current = openfile->current->prev;
	    openfile->current_y--;
	} while (!begpar(openfile->current));
    }

    if (allow_update)
	edit_redraw(current_save, pww_save);
}

void do_para_begin_void(void)
{
    do_para_begin(TRUE);
}

/* Move down to the end of a paragraph, then one line farther.  A line
 * is the last line of a paragraph if it is in a paragraph, and the next
 * line either is a beginning-of-paragraph line or isn't in a
 * paragraph. */
void do_para_end(bool allow_update)
{
    const filestruct *const current_save = openfile->current;
    const size_t pww_save = openfile->placewewant;

    check_statusblank();

    openfile->current_x = 0;
    openfile->placewewant = 0;

    while (openfile->current->next != NULL && !inpar(openfile->current))
	openfile->current = openfile->current->next;

    while (openfile->current->next != NULL &&
	inpar(openfile->current->next) &&
	!begpar(openfile->current->next)) {
	openfile->current = openfile->current->next;
	openfile->current_y++;
    }

    if (openfile->current->next != NULL)
	openfile->current = openfile->current->next;

    if (allow_update)
	edit_redraw(current_save, pww_save);
}

void do_para_end_void(void)
{
    do_para_end(TRUE);
}
#endif /* !DISABLE_JUSTIFY */

void do_home(void)
{
    size_t pww_save = openfile->placewewant;

    check_statusblank();

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

    if (need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}

void do_end(void)
{
    size_t pww_save = openfile->placewewant;

    check_statusblank();

    openfile->current_x = strlen(openfile->current->data);
    openfile->placewewant = xplustabs();

    if (need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}

void do_up(void)
{
    check_statusblank();

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If we're at the top of the file, get out. */
    if (openfile->current->prev == NULL)
	return;

    assert(openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

    /* Move the current line of the edit window up. */
    openfile->current = openfile->current->prev;
    openfile->current_x = actual_x(openfile->current->data,
	openfile->placewewant);

    /* If we're on the first line of the edit window, scroll the edit
     * window up one line if we're in smooth scrolling mode, or up half
     * a page if we're not. */
    if (openfile->current_y == 0)
	edit_scroll(UP,
#ifndef NANO_SMALL
		ISSET(SMOOTH_SCROLL) ? 1 :
#endif
		editwinrows / 2);
    /* Update the line we were on before and the line we're on now.  The
     * former needs to be redrawn if we're not on the first page, and
     * the latter needs to be drawn unconditionally. */
    else {
	if (need_vertical_update(0))
	    update_line(openfile->current->next, 0);
	update_line(openfile->current, openfile->current_x);
    }
}

void do_down(void)
{
    check_statusblank();

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If we're at the bottom of the file, get out. */
    if (openfile->current->next == NULL)
	return;

    assert(openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

    /* Move the current line of the edit window down. */
    openfile->current = openfile->current->next;
    openfile->current_x = actual_x(openfile->current->data,
	openfile->placewewant);

    /* If we're on the last line of the edit window, scroll the edit
     * window down one line if we're in smooth scrolling mode, or down
     * half a page if we're not. */
    if (openfile->current_y == editwinrows - 1)
	edit_scroll(DOWN,
#ifndef NANO_SMALL
		ISSET(SMOOTH_SCROLL) ? 1 :
#endif
		editwinrows / 2);
    /* Update the line we were on before and the line we're on now.  The
     * former needs to be redrawn if we're not on the first page, and
     * the latter needs to be drawn unconditionally. */
    else {
	if (need_vertical_update(0))
	    update_line(openfile->current->prev, 0);
	update_line(openfile->current, openfile->current_x);
    }
}

void do_left(bool allow_update)
{
    size_t pww_save = openfile->placewewant;

    check_statusblank();

    if (openfile->current_x > 0)
	openfile->current_x = move_mbleft(openfile->current->data,
		openfile->current_x);
    else if (openfile->current != openfile->fileage) {
	do_up();
	openfile->current_x = strlen(openfile->current->data);
    }

    openfile->placewewant = xplustabs();

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

    check_statusblank();

    assert(openfile->current_x <= strlen(openfile->current->data));

    if (openfile->current->data[openfile->current_x] != '\0')
	openfile->current_x = move_mbright(openfile->current->data,
		openfile->current_x);
    else if (openfile->current->next != NULL) {
	do_down();
	openfile->current_x = 0;
    }

    openfile->placewewant = xplustabs();

    if (allow_update && need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}

void do_right_void(void)
{
    do_right(TRUE);
}
