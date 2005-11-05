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

#include <string.h>
#include <ctype.h>
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
    openfile->current_x = strlen(openfile->filebot->data);
    openfile->placewewant = xplustabs();
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

    for (i = editwinrows - 2; i > 0 && openfile->current !=
	openfile->fileage; i--)
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

    for (i = editwinrows - 2; i > 0 && openfile->current !=
	openfile->filebot; i--)
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

    if (openfile->current != openfile->fileage) {
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

    while (openfile->current != openfile->filebot &&
	!inpar(openfile->current))
	openfile->current = openfile->current->next;

    while (openfile->current != openfile->filebot &&
	inpar(openfile->current->next) &&
	!begpar(openfile->current->next)) {
	openfile->current = openfile->current->next;
	openfile->current_y++;
    }

    if (openfile->current != openfile->filebot)
	openfile->current = openfile->current->next;

    if (allow_update)
	edit_redraw(current_save, pww_save);
}

void do_para_end_void(void)
{
    do_para_end(TRUE);
}
#endif /* !DISABLE_JUSTIFY */

#ifndef NANO_SMALL
/* Move to the next word in the current filestruct.  If allow_punct is
 * TRUE, treat punctuation as part of a word.  If allow_update is TRUE,
 * update the screen afterward.  Return TRUE if we started on a word,
 * and FALSE otherwise. */
bool do_next_word(bool allow_punct, bool allow_update)
{
    size_t pww_save = openfile->placewewant;
    const filestruct *current_save = openfile->current;
    char *char_mb;
    int char_mb_len;
    bool end_line = FALSE, started_on_word = FALSE;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    check_statusblank();

    char_mb = charalloc(mb_cur_max());

    /* Move forward until we find the character after the last letter of
     * the current word. */
    while (!end_line) {
	char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL);

	/* If we've found it, stop moving forward through the current
	 * line. */
	if (!is_word_mbchar(char_mb, allow_punct))
	    break;

	/* If we haven't found it, then we've started on a word, so set
	 * started_on_word to TRUE. */
	started_on_word = TRUE;

	if (openfile->current->data[openfile->current_x] == '\0')
	    end_line = TRUE;
	else
	    openfile->current_x += char_mb_len;
    }

    /* Move forward until we find the first letter of the next word. */
    if (openfile->current->data[openfile->current_x] == '\0')
	end_line = TRUE;
    else
	openfile->current_x += char_mb_len;

    for (; openfile->current != NULL;
	openfile->current = openfile->current->next) {
	while (!end_line) {
	    char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL);

	    /* If we've found it, stop moving forward through the
	     * current line. */
	    if (is_word_mbchar(char_mb, allow_punct))
		break;

	    if (openfile->current->data[openfile->current_x] == '\0')
		end_line = TRUE;
	    else
		openfile->current_x += char_mb_len;
	}

	/* If we've found it, stop moving forward to the beginnings of
	 * subsequent lines. */
	if (!end_line)
	    break;

	if (openfile->current != openfile->filebot) {
	    end_line = FALSE;
	    openfile->current_x = 0;
	}
    }

    free(char_mb);

    /* If we haven't found it, leave the cursor at the end of the
     * file. */
    if (openfile->current == NULL)
	openfile->current = openfile->filebot;

    openfile->placewewant = xplustabs();

    /* If allow_update is TRUE, update the screen. */
    if (allow_update)
	edit_redraw(current_save, pww_save);

    /* Return whether we started on a word. */
    return started_on_word;
}

void do_next_word_void(void)
{
    do_next_word(ISSET(WORD_BOUNDS), TRUE);
}

/* Move to the previous word in the current filestruct.  If allow_punct
 * is TRUE, treat punctuation as part of a word.  If allow_update is
 * TRUE, update the screen afterward.  Return TRUE if we started on a
 * word, and FALSE otherwise. */
bool do_prev_word(bool allow_punct, bool allow_update)
{
    size_t pww_save = openfile->placewewant;
    const filestruct *current_save = openfile->current;
    char *char_mb;
    int char_mb_len;
    bool begin_line = FALSE, started_on_word = FALSE;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    check_statusblank();

    char_mb = charalloc(mb_cur_max());

    /* Move backward until we find the character before the first letter
     * of the current word. */
    while (!begin_line) {
	char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL);

	/* If we've found it, stop moving backward through the current
	 * line. */
	if (!is_word_mbchar(char_mb, allow_punct))
	    break;

	/* If we haven't found it, then we've started on a word, so set
	 * started_on_word to TRUE. */
	started_on_word = TRUE;

	if (openfile->current_x == 0)
	    begin_line = TRUE;
	else
	    openfile->current_x = move_mbleft(openfile->current->data,
		openfile->current_x);
    }

    /* Move backward until we find the last letter of the previous
     * word. */
    if (openfile->current_x == 0)
	begin_line = TRUE;
    else
	openfile->current_x = move_mbleft(openfile->current->data,
		openfile->current_x);

    for (; openfile->current != NULL;
	openfile->current = openfile->current->prev) {
	while (!begin_line) {
	    char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL);

	    /* If we've found it, stop moving backward through the
	     * current line. */
	    if (is_word_mbchar(char_mb, allow_punct))
		break;

	    if (openfile->current_x == 0)
		begin_line = TRUE;
	    else
		openfile->current_x =
			move_mbleft(openfile->current->data,
			openfile->current_x);
	}

	/* If we've found it, stop moving backward to the ends of
	 * previous lines. */
	if (!begin_line)
	    break;

	if (openfile->current != openfile->fileage) {
	    begin_line = FALSE;
	    openfile->current_x = strlen(openfile->current->prev->data);
	}
    }

    /* If we haven't found it, leave the cursor at the beginning of the
     * file. */
    if (openfile->current == NULL)
	openfile->current = openfile->fileage;
    /* If we've found it, move backward until we find the character
     * before the first letter of the previous word. */
    else if (!begin_line) {
	if (openfile->current_x == 0)
	    begin_line = TRUE;
	else
	    openfile->current_x = move_mbleft(openfile->current->data,
		openfile->current_x);

	while (!begin_line) {
	    char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL);

	    /* If we've found it, stop moving backward through the
	     * current line. */
	    if (!is_word_mbchar(char_mb, allow_punct))
		break;

	    if (openfile->current_x == 0)
		begin_line = TRUE;
	    else
		openfile->current_x =
			move_mbleft(openfile->current->data,
			openfile->current_x);
	}

	/* If we've found it, move forward to the first letter of the
	 * previous word. */
	if (!begin_line)
	    openfile->current_x += char_mb_len;
    }

    free(char_mb);

    openfile->placewewant = xplustabs();

    /* If allow_update is TRUE, update the screen. */
    if (allow_update)
	edit_redraw(current_save, pww_save);

    /* Return whether we started on a word. */
    return started_on_word;
}

void do_prev_word_void(void)
{
    do_prev_word(ISSET(WORD_BOUNDS), TRUE);
}
#endif /* !NANO_SMALL */

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
    if (openfile->current == openfile->fileage)
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

#ifndef NANO_SMALL
void do_scroll_up(void)
{
    check_statusblank();

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If the top of the file is onscreen, get out. */
    if (openfile->edittop == openfile->fileage)
	return;

    assert(openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

    /* Move the current line of the edit window up. */
    openfile->current = openfile->current->prev;
    openfile->current_x = actual_x(openfile->current->data,
	openfile->placewewant);

    /* Scroll the edit window up one line. */
    edit_scroll(UP, 1);
}
#endif /* !NANO_SMALL */

void do_down(void)
{
    check_statusblank();

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If we're at the bottom of the file, get out. */
    if (openfile->current == openfile->filebot)
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

#ifndef NANO_SMALL
void do_scroll_down(void)
{
    check_statusblank();

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    /* If we're at the bottom of the file, get out. */
    if (openfile->current == openfile->filebot)
	return;

    assert(openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

    /* Move the current line of the edit window down. */
    openfile->current = openfile->current->next;
    openfile->current_x = actual_x(openfile->current->data,
	openfile->placewewant);

    /* Scroll the edit window down one line. */
    edit_scroll(DOWN, 1);
}
#endif /* !NANO_SMALL */

void do_left(void)
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

    if (need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}

void do_right(void)
{
    size_t pww_save = openfile->placewewant;

    check_statusblank();

    assert(openfile->current_x <= strlen(openfile->current->data));

    if (openfile->current->data[openfile->current_x] != '\0')
	openfile->current_x = move_mbright(openfile->current->data,
		openfile->current_x);
    else if (openfile->current != openfile->filebot) {
	do_down();
	openfile->current_x = 0;
    }

    openfile->placewewant = xplustabs();

    if (need_horizontal_update(pww_save))
	update_line(openfile->current, openfile->current_x);
}
