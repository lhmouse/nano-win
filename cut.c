/* $Id$ */
/**************************************************************************
 *   cut.c                                                                *
 *                                                                        *
 *   Copyright (C) 1999 Chris Allegretta                                  *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 1, or (at your option)  *
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
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

static int marked_cut;		/* Is the cutbuffer from a mark */
static filestruct *cutbottom = NULL;	/* Pointer to end of cutbuffer */

void add_to_cutbuffer(filestruct * inptr)
{
#ifdef DEBUG
    fprintf(stderr, _("add_to_cutbuffer called with inptr->data = %s\n"),
	    inptr->data);
#endif

    totsize -= strlen(inptr->data);
    if (cutbuffer == NULL) {
	cutbuffer = inptr;
	inptr->prev = NULL;
    } else {
	cutbottom->next = inptr;
	inptr->prev = cutbottom;
    }

    inptr->next = NULL;
    cutbottom = inptr;
}

#ifndef NANO_SMALL
void cut_marked_segment(filestruct * top, int top_x, filestruct * bot,
			int bot_x)
{
    filestruct *tmp, *next, *botcopy;
    char *tmpstr;

    /* Set up the beginning of the cutbuffer */
    tmp = copy_node(top);
    tmpstr = charalloc(strlen(&top->data[top_x]) + 1);
    strcpy(tmpstr, &top->data[top_x]);
    free(tmp->data);
    tmp->data = tmpstr;

    /* Chop off the end of the first line */
    tmpstr = charalloc(top_x + 1);
    strncpy(tmpstr, top->data, top_x);
    free(top->data);
    top->data = tmpstr;

    do {
	next = tmp->next;
	add_to_cutbuffer(tmp);
	totlines--;
	totsize--;		/* newline (add_to_cutbuffer doesn't count newlines) */
	tmp = next;
    }
    while (next != bot && next != NULL);

    dump_buffer(cutbuffer);
    if (next == NULL)
	return;
    /* Now, paste bot[bot_x] into top[top_x] */
    tmpstr = charalloc(strlen(top->data) + strlen(&bot->data[bot_x]));
    strncpy(tmpstr, top->data, top_x);
    strcpy(&tmpstr[top_x], &bot->data[bot_x]);
    free(top->data);
    top->data = tmpstr;

    null_at(bot->data, bot_x);
    next = bot->next;

    /* We explicitly don't decrement totlines here because we don't snarf
     * up a newline when we're grabbing the last line of the mark.  For
     * the same reason, we don't do an extra totsize decrement. */


    /* I honestly do not know why this is needed.  After many hours of
       using gdb on an OpenBSD box, I can honestly say something is 
       screwed somewhere.  Not doing this causes update_line to annihilate
       the last line copied into the cutbuffer when the mark is set ?!?!? */
    botcopy = copy_node(bot);
    add_to_cutbuffer(botcopy);
    free(bot);

    top->next = next;
    if (next != NULL)
	next->prev = top;

    dump_buffer(cutbuffer);
    renumber(top);
    current = top;
    current_x = top_x;

    /* If we're hitting the end of the buffer, we should clean that up. */
    if (bot == filebot) {
	if (next != NULL) {
	    filebot = next;
	} else {
	    filebot = top;
	}
    }
    if (top->lineno < edittop->lineno)
	edit_update(top, CENTER);
}
#endif

int do_cut_text(void)
{
    filestruct *tmp, *fileptr = current;
#ifndef NANO_SMALL
    char *tmpstr;
    int newsize, cuttingtoend = 0;
#endif

    check_statblank();
    if (fileptr == NULL || fileptr->data == NULL)
	return 0;

    tmp = fileptr->next;

    if (!ISSET(KEEP_CUTBUFFER)) {
	free_filestruct(cutbuffer);
	cutbuffer = NULL;

#ifdef DEBUG
	fprintf(stderr, _("Blew away cutbuffer =)\n"));
#endif
    }

    /* Must let cutbuffer get blown away first before we do this... */
    if (fileptr == filebot && !ISSET(MARK_ISSET))
	return 0;

#ifndef NANO_SMALL
    if (ISSET(CUT_TO_END) && !ISSET(MARK_ISSET)) {
	if (current_x == strlen(current->data)) {
	    do_delete();
	    SET(KEEP_CUTBUFFER);
	    marked_cut = 2;
	    return 1;
	} else {
	    SET(MARK_ISSET);
	    SET(KEEP_CUTBUFFER);

	    mark_beginx = strlen(current->data);
	    mark_beginbuf = current;
	    cuttingtoend = 1;
	}
    }
    if (ISSET(MARK_ISSET)) {
	if (current->lineno == mark_beginbuf->lineno) {
	    tmp = copy_node(current);
	    newsize = abs(mark_beginx - current_x) + 1;

	    tmpstr = charalloc(newsize + 1);
	    if (current_x < mark_beginx) {
		strncpy(tmpstr, &current->data[current_x], newsize);
		memmove(&current->data[current_x],
			&current->data[mark_beginx],
			strlen(&current->data[mark_beginx]) + 1);
	    } else {
		strncpy(tmpstr, &current->data[mark_beginx], newsize);
		memmove(&current->data[mark_beginx],
			&current->data[current_x],
			strlen(&current->data[current_x]) + 1);
		current_x = mark_beginx;
		update_cursor();
	    }
	    tmpstr[newsize - 1] = 0;
	    tmp->data = tmpstr;
	    add_to_cutbuffer(tmp);
	    dump_buffer(cutbuffer);
	    align(&current->data);
	} else if (current->lineno < mark_beginbuf->lineno)
	    cut_marked_segment(current, current_x, mark_beginbuf,
			       mark_beginx);
	else
	    cut_marked_segment(mark_beginbuf, mark_beginx, current,
			       current_x);

	placewewant = xplustabs();
	UNSET(MARK_ISSET);

	marked_cut = 1;
	set_modified();
	if (cuttingtoend)
	    edit_refresh();
	else
	    edit_update(current, CENTER);

	return 1;
#else
    if (0) {
#endif
    } else if (fileptr == fileage) {
	/* we're cutting the first line */
	if (fileptr->next != NULL) {
	    fileptr = fileptr->next;
	    tmp = fileptr;
	    fileage = fileptr;
	    add_to_cutbuffer(fileptr->prev);
	    totsize--;		/* get the newline */
	    totlines--;
	    fileptr->prev = NULL;
	    current = fileptr;
	    edit_update(fileage, CENTER);
	} else {
	    add_to_cutbuffer(fileptr);
	    fileage = make_new_node(NULL);
	    fileage->data = charalloc(1);
	    fileage->data[0] = '\0';
	    current = fileage;
	}
    } else {
	if (fileptr->prev != NULL)
	    fileptr->prev->next = fileptr->next;

	if (fileptr->next != NULL) {
	    (fileptr->next)->prev = fileptr->prev;
	    current = fileptr->next;
	    totlines--;
	    totsize--;		/* get the newline */
	}
	/* No longer an else here, because we never get here anymore...
	   No need to cut the magic line, as it's empty */
	add_to_cutbuffer(fileptr);
    }

    if (fileptr == edittop)
	edittop = current;

    edit_refresh();

    dump_buffer(cutbuffer);
    reset_cursor();

    set_modified();
    marked_cut = 0;
    current_x = 0;
    placewewant = 0;
    update_cursor();
    renumber(tmp);
    SET(KEEP_CUTBUFFER);
    return 1;
}

int do_uncut_text(void)
{
    filestruct *tmp = current, *fileptr = current, *newbuf, *newend;
#ifndef NANO_SMALL
    char *tmpstr, *tmpstr2;
    filestruct *hold = current;
#endif
    int i;

    wrap_reset();
    check_statblank();
    if (cutbuffer == NULL || fileptr == NULL)
	return 0;		/* AIEEEEEEEEEEEE */

    newbuf = copy_filestruct(cutbuffer);
    for (newend = newbuf; newend->next != NULL && newend != NULL;
	 newend = newend->next) {
	totlines++;
    }

    /* Hook newbuf into fileptr */
#ifndef NANO_SMALL
    if (marked_cut) {
	/* If there's only one line in the cutbuffer */
	if (cutbuffer->next == NULL) {
	    tmpstr =
		charalloc(strlen(current->data) + strlen(cutbuffer->data) +
			1);
	    strncpy(tmpstr, current->data, current_x);
	    strcpy(&tmpstr[current_x], cutbuffer->data);
	    strcat(tmpstr, &current->data[current_x]);
	    free(current->data);
	    current->data = tmpstr;
	    current_x += strlen(cutbuffer->data);
	    totsize += strlen(cutbuffer->data);

	    placewewant = xplustabs();
	    update_cursor();
	} else {		/* yuck -- no kidding! */
	    tmp = current->next;
	    /* New beginning */
	    tmpstr = charalloc(current_x + strlen(newbuf->data) + 1);
	    strncpy(tmpstr, current->data, current_x);
	    strcpy(&tmpstr[current_x], newbuf->data);
	    totsize += strlen(newbuf->data) + strlen(newend->data) + 1;

	    /* New end */
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

	    /* If tmp isn't null, we're in the middle: update the
	     * prev pointer.  If it IS null, we're at the end; update
	     * the filebot pointer */

	    if (tmp != NULL)
		tmp->prev = newend;
	    else {
		/* Fix the editbot pointer too */
		if (editbot == filebot)
		    editbot = newend;
		filebot = newend;
		new_magicline();
	    }

	    /* Now why don't we update the totsize also */
	    for (tmp = current->next; tmp != newend; tmp = tmp->next)
		totsize += strlen(tmp->data) + 1;

	    i = editbot->lineno;

	    current = newend;
	    if (i <= newend->lineno)
		edit_update(current, CENTER);
	}

	/* If marked cut == 2, that means that we're doing a cut to end
	   and we don't want anything else on the line, so we have to
	   screw up all the work we just did and separate the line.  There
	   must be a better way to do this, but not at 1AM on a work night. */

	if (marked_cut == 2 && current_x != strlen(current->data)) {
	    tmp = make_new_node(current);
	    tmp->data = charalloc(strlen(&current->data[current_x]) + 1);
	    strcpy(tmp->data, &current->data[current_x]);
	    splice_node(current, tmp, current->next);
	    null_at(current->data, current_x);
	    current = current->next;
	    current_x = 0;
	    placewewant = 0;
	}
	/* Renumber from BEFORE where we pasted ;) */
	renumber(hold);

	dump_buffer(fileage);
	dump_buffer(cutbuffer);
	set_modified();
	edit_refresh();
	return 0;
#else
    if (0) {
#endif
    } else if (fileptr != fileage) {
	tmp = fileptr->prev;
	tmp->next = newbuf;
	newbuf->prev = tmp;
	totlines++;		/* Unmarked uncuts don't split lines */
    } else {
	fileage = newbuf;
	totlines++;		/* Unmarked uncuts don't split lines */
    }

    /* This is so uncutting at the top of the buffer will work => */
    if (current_y == 0)
	edittop = newbuf;

    /* Connect the end of the buffer to the filestruct */
    newend->next = fileptr;
    fileptr->prev = newend;

    /* recalculate size *sigh* */
    for (tmp = newbuf; tmp != fileptr; tmp = tmp->next)
	totsize += strlen(tmp->data) + 1;

    i = editbot->lineno;
    renumber(newbuf);
    if (i < newend->lineno)
	edit_update(fileptr, CENTER);

    dump_buffer_reverse(fileptr);

    set_modified();
    UNSET(KEEP_CUTBUFFER);
    edit_refresh();
    return 1;
}
