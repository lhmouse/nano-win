/* $Id$ */
/**************************************************************************
 *   cut.c                                                                *
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
#include <stdio.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

static int keep_cutbuffer = FALSE;
	/* Should we keep the contents of the cutbuffer? */
static int marked_cut;
	/* Is the cutbuffer from a mark?  0 means whole-line cut, 1
	 * means mark, and 2 means cut-from-cursor. */
#ifndef NANO_SMALL
static int concatenate_cut;
	/* Should we add this cut string to the end of the last one? */
#endif
static filestruct *cutbottom = NULL;
	/* Pointer to end of cutbuffer. */

void cutbuffer_reset(void)
{
    keep_cutbuffer = FALSE;
}

filestruct *get_cutbottom(void)
{
    return cutbottom;
}

void add_to_cutbuffer(filestruct *inptr, int allow_concat)
{
#ifdef DEBUG
    fprintf(stderr, "add_to_cutbuffer(): inptr->data = %s\n", inptr->data);
#endif

    if (cutbuffer == NULL)
	cutbuffer = inptr;
#ifndef NANO_SMALL
    else if (allow_concat && concatenate_cut) {
	/* Just tack the text in inptr onto the text in cutbottom,
	 * unless allow_concat is false. */
	cutbottom->data = charealloc(cutbottom->data,
		strlen(cutbottom->data) + strlen(inptr->data) + 1);
	strcat(cutbottom->data, inptr->data);
	return;
    }
#endif
    else {
	cutbottom->next = inptr;
	inptr->prev = cutbottom;
    }
    cutbottom = inptr;
    cutbottom->next = NULL;
}

#ifndef NANO_SMALL
/* Cut a marked segment instead of a whole line.
 *
 * The first cut character is top->data[top_x].  Unless top == bot, the
 * last cut line has length bot_x.  That is, if bot_x > 0 then we cut to
 * bot->data[bot_x - 1].
 *
 * We maintain totsize, totlines, filebot, the magicline, and line
 * numbers.  Also, we set current and current_x so the cursor will be on
 * the first character after what was cut.  We do not do any screen
 * updates.
 *
 * Note cutbuffer might not be NULL if cut to end is used. */
void cut_marked_segment(void)
{
    filestruct *top;
    filestruct *bot;
    filestruct *tmp;
    size_t top_x;
    size_t bot_x;
    size_t newsize;

    /* If the mark doesn't cover any text, get out. */
    if (current == mark_beginbuf && current_x == mark_beginx)
	return;
    assert(current != NULL && mark_beginbuf != NULL);

    /* Set up the top and bottom lines and coordinates of the marked
     * text. */
    mark_order((const filestruct **)&top, &top_x,
		(const filestruct **)&bot, &bot_x);

    /* Make the first cut line manually.  Move the cut part of the top
     * line into tmp, and set newsize to that partial line's length. */
    tmp = copy_node(top);
    newsize = (top == bot ? bot_x - top_x : strlen(top->data + top_x));
    charmove(tmp->data, tmp->data + top_x, newsize);
    null_at(&tmp->data, newsize);

    /* Add the contents of tmp to the cutbuffer.  Note that cutbuffer
     * might be non-NULL if we have cut to end enabled. */
    if (cutbuffer == NULL) {
	cutbuffer = tmp;
	cutbottom = tmp;
    } else {
	cutbottom->next = tmp;
	tmp->prev = cutbottom;
	cutbottom = tmp;
    }

    /* And make the top remainder line manually too.  Update current_x
     * and totlines to account for all the cut text, and update totsize
     * to account for the length of the cut part of the first line. */
    current_x = top_x;
    totsize -= newsize;
    totlines -= bot->lineno - top->lineno;

    /* Now set newsize to be the length of the top remainder line plus
     * the bottom remainder line, plus one for the null terminator. */
    newsize = top_x + strlen(bot->data + bot_x) + 1;

    if (top == bot) {
	/* In this case, we're only cutting one line or part of one
	 * line, so the remainder line is shorter.  This means that we
	 * must move text from the end forward first. */
	charmove(top->data + top_x, bot->data + bot_x, newsize - top_x);
	top->data = charealloc(top->data, newsize);

	cutbottom->next = NULL;
#ifdef DEBUG
	dump_buffer(cutbuffer);
#endif
	return;
    }

    /* Update totsize to account for the cut part of the last line. */
    totsize -= bot_x + 1;

    /* Here, the top remainder line might get longer (if the bottom
     * remainder line is added to the end of it), so we realloc() it
     * first. */
    top->data = charealloc(top->data, newsize);
    charmove(top->data + top_x, bot->data + bot_x, newsize - top_x);

    assert(cutbottom != NULL && cutbottom->next != NULL);
    /* We're cutting multiple lines, so in particular the next line is
     * cut too. */
    cutbottom->next->prev = cutbottom;

    /* Update totsize to account for all the complete lines that have
     * been cut.  After this, totsize is fully up to date. */
    for (tmp = top->next; tmp != bot; tmp = tmp->next)
	totsize -= strlen(tmp->data) + 1;

    /* Make the last cut line manually. */
    null_at(&bot->data, bot_x);

    /* Move the rest of the cut text (other than the cut part of the top
     * line) from the buffer to the end of the cutbuffer, and fix the
     * edit buffer to account for the cut text. */
    top->next = bot->next;
    cutbottom = bot;
    cutbottom->next = NULL;
    if (top->next != NULL)
	top->next->prev = top;
    renumber(top);
    current = top;

    /* If the bottom line of the cut was the magicline, set filebot
     * properly, and add a new magicline if the top remainder line
     * (which is now the new bottom line) is non-blank. */
    if (bot == filebot) {
	filebot = top;
	assert(bot_x == 0);
	if (top_x > 0)
	    new_magicline();
    }
#ifdef DEBUG
    dump_buffer(cutbuffer);
#endif
}
#endif

int do_cut_text(void)
{
    filestruct *fileptr;

    assert(current != NULL && current->data != NULL);

    check_statblank();

    if (!keep_cutbuffer) {
	free_filestruct(cutbuffer);
	cutbuffer = NULL;
	marked_cut = 0;
#ifndef NANO_SMALL
	concatenate_cut = FALSE;
#endif
#ifdef DEBUG
	fprintf(stderr, "Blew away cutbuffer =)\n");
#endif
    }

    /* You can't cut the magicline except with the mark.  But trying
     * does clear the cutbuffer if keep_cutbuffer is FALSE. */
    if (current == filebot
#ifndef NANO_SMALL
			&& !ISSET(MARK_ISSET)
#endif
						)
	return 0;

    keep_cutbuffer = TRUE;

#ifndef NANO_SMALL
    if (ISSET(CUT_TO_END) && !ISSET(MARK_ISSET)) {
	assert(current_x >= 0 && current_x <= strlen(current->data));

	if (current->data[current_x] == '\0') {
	    /* If the line is empty and we didn't just cut a non-blank
	     * line, create a dummy blank line and add it to the
	     * cutbuffer. */
	    if (marked_cut != 1 && current->next != filebot) {
		filestruct *junk = make_new_node(current);

		junk->data = charalloc(1);
		junk->data[0] = '\0';
		add_to_cutbuffer(junk, TRUE);
#ifdef DEBUG
		dump_buffer(cutbuffer);
#endif
	    }

	    do_delete();
	    marked_cut = 2;
	    return 1;
	} else {
	    SET(MARK_ISSET);

	    mark_beginx = strlen(current->data);
	    mark_beginbuf = current;
	}
    }

    if (ISSET(MARK_ISSET)) {
	cut_marked_segment();

	placewewant = xplustabs();
	UNSET(MARK_ISSET);

	/* If we just did a marked cut of part of a line, we should add
	 * the first line of any cut done immediately afterward to the
	 * end of this cut, as Pico does. */
	if (current == mark_beginbuf && current_x < strlen(current->data))
	    concatenate_cut = TRUE;
	marked_cut = 1;
	edit_refresh();
	set_modified();

	return 1;
    }
#endif /* !NANO_SMALL */

    totlines--;
    totsize -= strlen(current->data) + 1;
    fileptr = current;
    current = current->next;
    current->prev = fileptr->prev;
    add_to_cutbuffer(fileptr, TRUE);
#ifdef DEBUG
    dump_buffer(cutbuffer);
#endif

    if (fileptr == fileage)
	fileage = current;
    else
	current->prev->next = current;

    if (fileptr == edittop)
	edittop = current;

    renumber(current);
    current_x = 0;
    edit_refresh();
    set_modified();
    marked_cut = 0;
#ifndef NANO_SMALL
    concatenate_cut = FALSE;
#endif
    return 1;
}

int do_uncut_text(void)
{
    filestruct *tmp = current;
    filestruct *newbuf = NULL;
    filestruct *newend = NULL;

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    check_statblank();
    if (cutbuffer == NULL || current == NULL)
	return 0;		/* AIEEEEEEEEEEEE */

    /* If we're uncutting a previously non-marked block, uncut to end if
     * we're not at the beginning of the line.  If we are at the
     * beginning of the line, set placewewant to 0.  Pico does both of
     * these. */
    if (marked_cut == 0) {
	if (current_x != 0)
	    marked_cut = 2;
	else
	    placewewant = 0;
    }

    /* If we're going to uncut on the magicline, always make a new
     * magicline in advance, as Pico does. */
    if (current->next == NULL)
	new_magicline();

    if (marked_cut == 0 || cutbuffer->next != NULL) {
	newbuf = copy_filestruct(cutbuffer);
	for (newend = newbuf; newend->next != NULL && newend != NULL;
		newend = newend->next)
	    totlines++;
    }

    /* Hook newbuf in at current. */
    if (marked_cut != 0) {
	filestruct *hold = current;

	/* If there's only one line in the cutbuffer... */
	if (cutbuffer->next == NULL) {
	    size_t buf_len = strlen(cutbuffer->data);
	    size_t cur_len = strlen(current->data);

	    current->data = charealloc(current->data, cur_len + buf_len + 1);
	    charmove(current->data + current_x + buf_len,
			current->data + current_x, cur_len - current_x + 1);
	    strncpy(current->data + current_x, cutbuffer->data, buf_len);
		/* Use strncpy() to not copy the null terminator. */

	    current_x += buf_len;
	    totsize += buf_len;

	    placewewant = xplustabs();
	} else {		/* Yuck -- no kidding! */
	    char *tmpstr, *tmpstr2;

	    tmp = current->next;

	    /* New beginning. */
	    tmpstr = charalloc(current_x + strlen(newbuf->data) + 1);
	    strncpy(tmpstr, current->data, current_x);
	    strcpy(&tmpstr[current_x], newbuf->data);
	    totsize += strlen(newbuf->data) + strlen(newend->data) + 1;

	    /* New end. */
	    tmpstr2 = charalloc(strlen(newend->data) +
			      strlen(&current->data[current_x]) + 1);
	    strcpy(tmpstr2, newend->data);
	    strcat(tmpstr2, &current->data[current_x]);

	    free(current->data);
	    current->data = tmpstr;
	    current->next = newbuf->next;
	    newbuf->next->prev = current;
	    delete_node(newbuf);

	    current_x = strlen(newend->data);
	    placewewant = xplustabs();
	    free(newend->data);
	    newend->data = tmpstr2;

	    newend->next = tmp;

	    /* If tmp isn't NULL, we're in the middle: update the
	     * prev pointer.  If it IS NULL, we're at the end; update
	     * the filebot pointer. */
	    if (tmp != NULL)
		tmp->prev = newend;
	    else {
		filebot = newend;
		new_magicline();
	    }

	    /* Now why don't we update the totsize also? */
	    for (tmp = current->next; tmp != newend; tmp = tmp->next)
		totsize += strlen(tmp->data) + 1;

	    current = newend;
	}

	/* If marked cut == 2, that means that we're doing a cut to end
	 * and we don't want anything else on the line, so we have to
	 * screw up all the work we just did and separate the line.
	 * There must be a better way to do this, but not at 1 AM on a
	 * work night. */
	if (marked_cut == 2) {
	    tmp = make_new_node(current);
	    tmp->data = mallocstrcpy(NULL, current->data + current_x);
	    splice_node(current, tmp, current->next);
	    null_at(&current->data, current_x);
	    current = current->next;
	    current_x = 0;
	    placewewant = 0;

	    /* Extra line added; update stuff. */
	    totlines++;
	    totsize++;
	}
	/* Renumber from BEFORE where we pasted ;) */
	renumber(hold);

#ifdef DEBUG
	dump_buffer(fileage);
	dump_buffer(cutbuffer);
#endif
	set_modified();
	edit_refresh();
	return 0;
    }

    if (current != fileage) {
	tmp = current->prev;
	tmp->next = newbuf;
	newbuf->prev = tmp;
    } else
	fileage = newbuf;
    totlines++;		/* Unmarked uncuts don't split lines. */

    /* This is so uncutting at the top of the buffer will work => */
    if (current_y == 0)
	edittop = newbuf;

    /* Connect the end of the buffer to the filestruct. */
    newend->next = current;
    current->prev = newend;

    /* Recalculate size *sigh* */
    for (tmp = newbuf; tmp != current; tmp = tmp->next)
	totsize += strlen(tmp->data) + 1;

    renumber(newbuf);
    edit_refresh();

#ifdef DEBUG
    dump_buffer_reverse();
#endif

    set_modified();
    return 1;
}
