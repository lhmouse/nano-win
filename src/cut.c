/* $Id$ */
/**************************************************************************
 *   cut.c                                                                *
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
#include <stdio.h>
#include <assert.h>
#include "proto.h"

static bool keep_cutbuffer = FALSE;
	/* Should we keep the contents of the cutbuffer? */
static filestruct *cutbottom = NULL;
	/* Pointer to end of cutbuffer. */

void cutbuffer_reset(void)
{
    keep_cutbuffer = FALSE;
}

/* If we're not on the magicline, move all the text of the current line,
 * plus the newline at the end, to the cutbuffer, and set the current
 * place we want to where the line used to start. */
void cut_line(void)
{
    if (openfile->current->next != NULL) {
	move_to_filestruct(&cutbuffer, &cutbottom, openfile->current, 0,
		openfile->current->next, 0);
	openfile->placewewant = xplustabs();
    }
}

#ifndef NANO_SMALL
/* Move all currently marked text to the cutbuffer, and set the current
 * place we want to where the text used to start. */
void cut_marked(void)
{
    filestruct *top, *bot;
    size_t top_x, bot_x;

    mark_order((const filestruct **)&top, &top_x,
	(const filestruct **)&bot, &bot_x, NULL);

    move_to_filestruct(&cutbuffer, &cutbottom, top, top_x, bot, bot_x);
    openfile->placewewant = xplustabs();
}

/* If we're not at the end of the current line, move all the text from
 * the current cursor position to the end of the current line,
 * not counting the newline at the end, to the cutbuffer.  If we are,
 * and we're not on the magicline, move the newline at the end to the
 * cutbuffer, and set the current place we want to where the newline
 * used to be. */
void cut_to_eol(void)
{
    size_t data_len = strlen(openfile->current->data);

    assert(openfile->current_x <= data_len);

    if (openfile->current_x < data_len)
	/* If we're not at the end of the line, move all the text from
	 * the current position up to it, not counting the newline at
	 * the end, to the cutbuffer. */
	move_to_filestruct(&cutbuffer, &cutbottom, openfile->current,
		openfile->current_x, openfile->current, data_len);
    else if (openfile->current->next != NULL) {
	/* If we're at the end of the line, and it isn't the magicline,
	 * move all the text from the current position up to the
	 * beginning of the next line, i.e, the newline at the end, to
	 * the cutbuffer. */
	move_to_filestruct(&cutbuffer, &cutbottom, openfile->current,
		openfile->current_x, openfile->current->next, 0);
	openfile->placewewant = xplustabs();
    }
}
#endif /* !NANO_SMALL */

/* Move text from the current filestruct into the cutbuffer. */
void do_cut_text(void)
{
    assert(openfile->current != NULL && openfile->current->data != NULL);

    check_statusblank();

    /* If keep_cutbuffer is FALSE and the cutbuffer isn't empty, blow
     * away the text in the cutbuffer. */
    if (!keep_cutbuffer && cutbuffer != NULL) {
	free_filestruct(cutbuffer);
	cutbuffer = NULL;
#ifdef DEBUG
	fprintf(stderr, "Blew away cutbuffer =)\n");
#endif
    }

    /* Set keep_cutbuffer to TRUE, so that the text we're going to move
     * into the cutbuffer will be added to the text already in the
     * cutbuffer instead of replacing it. */
    keep_cutbuffer = TRUE;

#ifndef NANO_SMALL
    if (openfile->mark_set) {
	/* If the mark is on, move the marked text to the cutbuffer and
	 * turn the mark off. */
	cut_marked();
	openfile->mark_set = FALSE;
    } else if (ISSET(CUT_TO_END))
	/* Otherwise, if the CUT_TO_END flag is set, move all text up to
	 * the end of the line into the cutbuffer. */
	cut_to_eol();
    else
#endif
	/* Otherwise, move the entire line into the cutbuffer. */
	cut_line();

    edit_refresh();
    set_modified();

#ifdef DEBUG
    dump_filestruct(cutbuffer);
#endif
}

#ifndef NANO_SMALL
/* Cut from the current cursor position to the end of the file. */
void do_cut_till_end(void)
{
    assert(openfile->current != NULL && openfile->current->data != NULL);

    check_statusblank();

    move_to_filestruct(&cutbuffer, &cutbottom, openfile->current,
	openfile->current_x, openfile->filebot, 0);

    edit_refresh();
    set_modified();

#ifdef DEBUG
    dump_filestruct(cutbuffer);
#endif
}
#endif /* !NANO_SMALL */

/* Copy text from the cutbuffer into the current filestruct. */
void do_uncut_text(void)
{
    assert(openfile->current != NULL && openfile->current->data != NULL);

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    check_statusblank();

    /* If the cutbuffer is empty, get out. */
    if (cutbuffer == NULL)
	return;

    /* Add a copy of the text in the cutbuffer to the current filestruct
     * at the current cursor position. */
    copy_from_filestruct(cutbuffer, cutbottom);

    /* Set the current place we want to where the text from the
     * cutbuffer ends. */
    openfile->placewewant = xplustabs();

    edit_refresh();
    set_modified();

#ifdef DEBUG
    dump_filestruct_reverse();
#endif
}
