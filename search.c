/**************************************************************************
 *   search.c                                                             *
 *                                                                        *
 *   Copyright (C) 2000 Chris Allegretta                                  *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "config.h"
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/* Set up the system variables for a search or replace.  Returns -1 on
   abort, 0 on success, and 1 on rerun calling program 
   Return -2 to run opposite program (searchg -> replace, replace -> search)

   replacing = 1 if we call from do_replace, 0 if called from do_search func.
*/
int search_init(int replacing)
{
    int i;
    char buf[135];

    if (last_search[0]) {
	sprintf(buf, " [%s]", last_search);
    } else {
	buf[0] = '\0';
    }

    i = statusq(replacing ? replace_list : whereis_list,
		replacing ? REPLACE_LIST_LEN : WHEREIS_LIST_LEN, "",
		ISSET(CASE_SENSITIVE) ? _("Case Sensitive Search%s") :
		_("Search%s"), buf);

    /* Cancel any search, or just return with no previous search */
    if ((i == -1) || (i < 0 && !last_search[0])) {
	statusbar(_("Search Cancelled"));
	reset_cursor();
	return -1;
    } else if (i == -2) {	/* Same string */
	strncpy(answer, last_search, 132);
    } else if (i == 0) {	/* They entered something new */
	strncpy(last_search, answer, 132);

	/* Blow away last_replace because they entered a new search
	   string....uh, right? =) */
	last_replace[0] = '\0';
    } else if (i == NANO_CASE_KEY) {	/* They want it case sensitive */
	if (ISSET(CASE_SENSITIVE))
	    UNSET(CASE_SENSITIVE);
	else
	    SET(CASE_SENSITIVE);

	return 1;
    } else if (i == NANO_OTHERSEARCH_KEY) {
	return -2;		/* Call the opposite search function */
    } else if (i == NANO_FROMSEARCHTOGOTO_KEY) {
	do_gotoline_void();
	return -3;
    } else {			/* First line key, etc. */
	do_early_abort();
	return -3;
    }

    return 0;
}

filestruct *findnextstr(int quiet, filestruct * begin, char *needle)
{
    filestruct *fileptr;
    char *searchstr, *found = NULL, *tmp;

    fileptr = current;

    searchstr = &current->data[current_x + 1];
    /* Look for searchstr until EOF */
    while (fileptr != NULL &&
	   (found = strstrwrapper(searchstr, needle)) == NULL) {
	fileptr = fileptr->next;

	if (fileptr == begin)
	    return NULL;

	if (fileptr != NULL)
	    searchstr = fileptr->data;
    }

    /* If we're not at EOF, we found an instance */
    if (fileptr != NULL) {
	current = fileptr;
	current_x = 0;
	for (tmp = fileptr->data; tmp != found; tmp++)
	    current_x++;

	edit_update(current);
	reset_cursor();
    } else {			/* We're at EOF, go back to the top, once */

	fileptr = fileage;

	while (fileptr != current && fileptr != begin &&
	       (found = strstrwrapper(fileptr->data, needle)) == NULL)
	    fileptr = fileptr->next;

	if (fileptr == begin) {
	    if (!quiet)
		statusbar(_("\"%s\" not found"), needle);

	    return NULL;
	}
	if (fileptr != current) {	/* We found something */
	    current = fileptr;
	    current_x = 0;
	    for (tmp = fileptr->data; tmp != found; tmp++)
		current_x++;

	    edit_update(current);
	    reset_cursor();

	    if (!quiet)
		statusbar(_("Search Wrapped"));
	} else {		/* Nada */

	    if (!quiet)
		statusbar(_("\"%s\" not found"), needle);
	    return NULL;
	}
    }

    return fileptr;
}

void search_abort(void)
{
    UNSET(KEEP_CUTBUFFER);
    display_main_list();
    wrefresh(bottomwin);
}

/* Search for a string */
int do_search(void)
{
    int i;
    filestruct *fileptr = current;

    wrap_reset();
    if ((i = search_init(0)) == -1) {
	current = fileptr;
	search_abort();
	return 0;
    } else if (i == -3) {
	search_abort();
	return 0;
    } else if (i == -2) {
	search_abort();
	do_replace();
	return 0;
    } else if (i == 1) {
	do_search();
	search_abort();
	return 1;
    }
    findnextstr(0, current, answer);
    search_abort();
    return 1;
}

void print_replaced(int num)
{
    if (num > 1)
	statusbar(_("Replaced %d occurences"), num);
    else if (num == 1)
	statusbar(_("Replaced 1 occurence"));
}

void replace_abort(void)
{
    UNSET(KEEP_CUTBUFFER);
    display_main_list();
    reset_cursor();
}

/* Search for a string */
int do_replace(void)
{
    int i, replaceall = 0, numreplaced = 0, beginx;
    filestruct *fileptr, *begin;
    char *tmp, *copy, prevanswer[132] = "";

    if ((i = search_init(1)) == -1) {
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    } else if (i == 1) {
	do_replace();
	return 1;
    } else if (i == -2) {
	replace_abort();
	do_search();
	return 0;
    } else if (i == -3) {
	replace_abort();
	return 0;
    }
    strncpy(prevanswer, answer, 132);

    if (strcmp(last_replace, "")) {	/* There's a previous replace str */
	i = statusq(replace_list, REPLACE_LIST_LEN, "",
		    _("Replace with [%s]"), last_replace);

	if (i == -1) {		/* Aborted enter */
	    strncpy(answer, last_replace, 132);
	    statusbar(_("Replace Cancelled"));
	    replace_abort();
	    return 0;
	} else if (i == 0)	/* They actually entered something */
	    strncpy(last_replace, answer, 132);
	else if (i == NANO_CASE_KEY) {	/* They asked for case sensitivity */
	    if (ISSET(CASE_SENSITIVE))
		UNSET(CASE_SENSITIVE);
	    else
		SET(CASE_SENSITIVE);

	    do_replace();
	    return 0;
	} else {		/* First page, last page, for example could get here */

	    do_early_abort();
	    replace_abort();
	    return 0;
	}
    } else {			/* last_search is empty */

	i = statusq(replace_list, REPLACE_LIST_LEN, "", _("Replace with"));
	if (i == -1) {
	    statusbar(_("Replace Cancelled"));
	    reset_cursor();
	    replace_abort();
	    return 0;
	} else if (i == 0)	/* They entered something new */
	    strncpy(last_replace, answer, 132);
	else if (i == NANO_CASE_KEY) {	/* They want it case sensitive */
	    if (ISSET(CASE_SENSITIVE))
		UNSET(CASE_SENSITIVE);
	    else
		SET(CASE_SENSITIVE);

	    do_replace();
	    return 1;
	} else {		/* First line key, etc. */

	    do_early_abort();
	    replace_abort();
	    return 0;
	}
    }

    /* save where we are */
    begin = current;
    beginx = current_x;

    while (1) {

	if (replaceall)
	    fileptr = findnextstr(1, begin, prevanswer);
	else
	    fileptr = findnextstr(0, begin, prevanswer);

	/* No more matches.  Done! */
	if (!fileptr)
	    break;

	/* If we're here, we've found the search string */
	if (!replaceall)
	    i = do_yesno(1, 1, _("Replace this instance?"));

	if (i > 0 || replaceall) {	/* Yes, replace it!!!! */
	    if (i == 2)
		replaceall = 1;

	    /* Create buffer */
	    copy = nmalloc(strlen(current->data) - strlen(last_search) +
			   strlen(last_replace) + 1);

	    /* Head of Original Line */
	    strncpy(copy, current->data, current_x);
	    copy[current_x] = 0;

	    /* Replacement Text */
	    strcat(copy, last_replace);

	    /* The tail of the original line */
	    /*  This may expose other bugs, because it no longer
	       goes through each character on the string
	       and tests for string goodness.  But because
	       we can assume the invariant that current->data
	       is less than current_x + strlen(last_search) long,   
	       this should be safe.  Or it will expose bugs ;-) */
	    tmp = current->data + current_x + strlen(last_search);
	    strcat(copy, tmp);

	    /* Cleanup */
	    free(current->data);
	    current->data = copy;

	    /* Stop bug where we replace a substring of the replacement text */
	    current_x += strlen(last_replace);

	    edit_refresh();
	    set_modified();
	    numreplaced++;
	} else if (i == -1)	/* Abort, else do nothing and continue loop */
	    break;
    }

    current = begin;
    current_x = beginx;
    renumber_all();
    edit_update(current);
    print_replaced(numreplaced);
    replace_abort();
    return 1;
}

void goto_abort(void)
{
    UNSET(KEEP_CUTBUFFER);
    display_main_list();
}

int do_gotoline(long defline)
{
    long line, i = 1, j = 0;
    filestruct *fileptr;

    if (defline > 0)		/* We already know what line we want to go to */
	line = defline;
    else {			/* Ask for it */

	j = statusq(goto_list, GOTO_LIST_LEN, "", _("Enter line number"));
	if (j == -1) {
	    statusbar(_("Aborted"));
	    goto_abort();
	    return 0;
	} else if (j != 0) {
	    do_early_abort();
	    goto_abort();
	    return 0;
	}
	if (!strcmp(answer, "$")) {
	    current = filebot;
	    current_x = 0;
	    edit_update(current);
	    goto_abort();
	    return 1;
	}
	line = atoi(answer);
    }

    /* Bounds check */
    if (line <= 0) {
	statusbar(_("Come on, be reasonable"));
	goto_abort();
	return 0;
    }
    if (line > totlines) {
	statusbar(_("Only %d lines available, skipping to last line"),
		  filebot->lineno);
	current = filebot;
	current_x = 0;
	edit_update(current);
    } else {
	for (fileptr = fileage; fileptr != NULL && i < line; i++)
	    fileptr = fileptr->next;

	current = fileptr;
	current_x = 0;
	edit_update(current);
    }

    goto_abort();
    return 1;
}

int do_gotoline_void(void)
{
    return do_gotoline(0);
}
