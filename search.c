/* $Id$ */
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
#include <unistd.h>
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

static char *last_search = NULL;	/* Last string we searched for */
static char *last_replace = NULL;	/* Last replacement string */
static int search_last_line;


/* Regular expression helper functions */

#ifdef HAVE_REGEX_H
void regexp_init(const char *regexp)
{
    regcomp(&search_regexp, regexp, ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE);
    SET(REGEXP_COMPILED);
}

void regexp_cleanup()
{
    UNSET(REGEXP_COMPILED);
    regfree(&search_regexp);
}
#endif

/* Set up the system variables for a search or replace.  Returns -1 on
   abort, 0 on success, and 1 on rerun calling program 
   Return -2 to run opposite program (searchg -> replace, replace -> search)

   replacing = 1 if we call from do_replace, 0 if called from do_search func.
*/
int search_init(int replacing)
{
    int i = 0;
    char *buf;
    char *prompt, *reprompt = "";

   if (last_search == NULL) {
	last_search = nmalloc(1);
	last_search[0] = 0;
   }
   if (last_replace == NULL) {
	last_replace = nmalloc(1);
	last_replace[0] = 0;
   }

   buf = nmalloc(strlen(last_search) + 5);
   buf[0] = 0;

   /* If using Pico messages, we do things the old fashioned way... */
   if (ISSET(PICO_MSGS)) {
	if (last_search[0]) {

	    /* We use COLS / 3 here because we need to see more on the line */
	    if (strlen(last_search) > COLS / 3) {
		snprintf(buf, COLS / 3 + 3, " [%s", last_search);
		sprintf(&buf[COLS / 3 + 2], "...]");
	    } else
		sprintf(buf, " [%s]", last_search);
	} else {
	    buf[0] = '\0';
	}
    }

    if (ISSET(USE_REGEXP) && ISSET(CASE_SENSITIVE))
	prompt = _("Case Sensitive Regexp Search%s%s");
    else if (ISSET(USE_REGEXP))
	prompt = _("Regexp Search%s%s");
    else if (ISSET(CASE_SENSITIVE))
	prompt = _("Case Sensitive Search%s%s");
    else
	prompt = _("Search%s%s");

    if (replacing)
	reprompt = _(" (to replace)");

    if (ISSET(PICO_MSGS))
	i = statusq(replacing ? replace_list : whereis_list,
		replacing ? REPLACE_LIST_LEN : WHEREIS_LIST_LEN, "",
		prompt, reprompt, buf);
    else
	i = statusq(replacing ? replace_list : whereis_list,
		replacing ? REPLACE_LIST_LEN : WHEREIS_LIST_LEN, last_search,
		prompt, reprompt, "");

    /* Cancel any search, or just return with no previous search */
    if ((i == -1) || (i < 0 && !last_search[0])) {
	statusbar(_("Search Cancelled"));
	reset_cursor();
	return -1;
    } else if (i == -2) {	/* Same string */
	answer = mallocstrcpy(answer, last_search);
#ifdef HAVE_REGEX_H
	if (ISSET(USE_REGEXP))
	    regexp_init(answer);
#endif
    } else if (i == 0) {	/* They entered something new */
	last_search = mallocstrcpy(last_search, answer);
#ifdef HAVE_REGEX_H
	if (ISSET(USE_REGEXP))
	    regexp_init(answer);
#endif
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

void not_found_msg(char *str)
{
    char foo[COLS];

    if (strlen(str) < COLS / 2)
	statusbar(_("\"%s\" not found"), str);
    else {
	strncpy(foo, str, COLS / 2);
	foo[COLS / 2] = 0;
	statusbar(_("\"%s...\" not found"), foo);
    }
}

filestruct *findnextstr(int quiet, filestruct * begin, int beginx,
			char *needle)
{
    filestruct *fileptr;
    char *searchstr, *found = NULL, *tmp;
    int past_editbot = 0;

    fileptr = current;

    current_x++;

    /* Are we searching the last line? (i.e. the line where search started) */
    if ((fileptr == begin) && (current_x < beginx))
	search_last_line = 1;

    /* Make sure we haven't passed the end of the string */
    if (strlen(fileptr->data) < current_x)
	current_x--;

    searchstr = &fileptr->data[current_x];

    /* Look for needle in searchstr */
    while ((found = strstrwrapper(searchstr, needle)) == NULL) {

	/* finished processing file, get out */
	if (search_last_line) {
	    if (!quiet)
		not_found_msg(needle);
	    return NULL;
	}

	fileptr = fileptr->next;

	if (!past_editbot && (fileptr == editbot))
	    past_editbot = 1;

	/* EOF reached, wrap around once */
	if (fileptr == NULL) {
	    fileptr = fileage;

	    past_editbot = 1;

	    if (!quiet)
		statusbar(_("Search Wrapped"));
	}

	/* Original start line reached */
	if (fileptr == begin)
	    search_last_line = 1;

	searchstr = fileptr->data;
    }

    /* We found an instance */
    current = fileptr;
    current_x = 0;
    for (tmp = fileptr->data; tmp != found; tmp++)
	current_x++;

    /* Ensure we haven't wrap around again! */
    if ((search_last_line) && (current_x >= beginx)) {
	if (!quiet)
	    not_found_msg(needle);
	return NULL;
    }

    if (past_editbot)
	edit_update(fileptr, CENTER);
    else
	update_line(current, current_x);

    placewewant = xplustabs();
    reset_cursor();

    return fileptr;
}

void search_abort(void)
{
    UNSET(KEEP_CUTBUFFER);
    display_main_list();
    wrefresh(bottomwin);
    if (ISSET(MARK_ISSET))
	edit_refresh_clearok();

#ifdef HAVE_REGEX_H
    if (ISSET(REGEXP_COMPILED))
	regexp_cleanup();
#endif
}

/* Search for a string */
int do_search(void)
{
    int i;
    filestruct *fileptr = current;

    wrap_reset();
    i = search_init(0);
    switch (i) {
    case -1:
	current = fileptr;
	search_abort();
	return 0;
    case -3:
	search_abort();
	return 0;
    case -2:
	do_replace();
	return 0;
    case 1:
	do_search();
	search_abort();
	return 1;
    }

    /* The sneaky user deleted the previous search string */
    if (!strcmp(answer, "")) {
	statusbar(_("Search Cancelled"));
	search_abort();
	return 0;
    }

    search_last_line = 0;
    findnextstr(0, current, current_x, answer);
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
    /* Identicle to search_abort, so we'll call it here.  If it
       does something different later, we can change it back.  For now
       it's just a waste to duplicat code */
    search_abort();
    placewewant = xplustabs();
}

#ifdef HAVE_REGEX_H
int replace_regexp(char *string, int create_flag)
{
    /* split personality here - if create_flag is null, just calculate
     * the size of the replacement line (necessary because of
     * subexpressions like \1 \2 \3 in the replaced text) */

    char *c;
    int new_size = strlen(current->data) + 1;
    int search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;

    new_size -= search_match_count;

    /* Iterate through the replacement text to handle
     * subexpression replacement using \1, \2, \3, etc */

    c = last_replace;
    while (*c) {
	if (*c != '\\') {
	    if (create_flag)
		*string++ = *c;
	    c++;
	    new_size++;
	} else {
	    int num = (int) *(c + 1) - (int) '0';
	    if (num >= 1 && num <= 9) {

		int i = regmatches[num].rm_so;

		if (num > search_regexp.re_nsub) {
		    /* Ugh, they specified a subexpression that doesn't
		       exist.  */
		    return -1;
		}

		/* Skip over the replacement expression */
		c += 2;

		/* But add the length of the subexpression to new_size */
		new_size += regmatches[num].rm_eo - regmatches[num].rm_so;

		/* And if create_flag is set, append the result of the
		 * subexpression match to the new line */
		while (create_flag && i < regmatches[num].rm_eo)
		    *string++ = *(current->data + i++);

	    } else {
		if (create_flag)
		    *string++ = *c;
		c++;
		new_size++;
	    }
	}
    }

    if (create_flag)
	*string = 0;

    return new_size;
}
#endif

char *replace_line(void)
{
    char *copy, *tmp;
    int new_line_size;
    int search_match_count;

    /* Calculate size of new line */
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP)) {
	search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;
	new_line_size = replace_regexp(NULL, 0);
	/* If they specified an invalid subexpression in the replace
	 * text, return NULL indicating an error */
	if (new_line_size < 0)
	    return NULL;
    } else {
#else
    {
#endif
	search_match_count = strlen(last_search);
	new_line_size = strlen(current->data) - strlen(last_search) +
	    strlen(last_replace) + 1;
    }

    /* Create buffer */
    copy = nmalloc(new_line_size);

    /* Head of Original Line */
    strncpy(copy, current->data, current_x);
    copy[current_x] = 0;

    /* Replacement Text */
    if (!ISSET(USE_REGEXP))
	strcat(copy, last_replace);
#ifdef HAVE_REGEX_H
    else
	(void) replace_regexp(copy + current_x, 1);
#endif

    /* The tail of the original line */
    /* This may expose other bugs, because it no longer
       goes through each character on the string
       and tests for string goodness.  But because
       we can assume the invariant that current->data
       is less than current_x + strlen(last_search) long,
       this should be safe.  Or it will expose bugs ;-) */
    tmp = current->data + current_x + search_match_count;
    strcat(copy, tmp);

    return copy;
}

/* Replace a string */
int do_replace(void)
{
    int i, replaceall = 0, numreplaced = 0, beginx;
    filestruct *fileptr, *begin;
    char *copy, *prevanswer = NULL, *buf = NULL;

    i = search_init(1);
    switch (i) {
    case -1:
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    case 1:
	do_replace();
	return 1;
    case -2:
	do_search();
	return 0;
    case -3:
	replace_abort();
	return 0;
    }

    /* Again, there was a previous string but they deleted it and hit enter */
    if (!strcmp(answer, "")) {
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    }

    prevanswer = mallocstrcpy(prevanswer, answer);

    if (ISSET(PICO_MSGS)) {
	buf = nmalloc(strlen(last_replace) + 5);
	if (strcmp(last_replace, "")) {
	    if (strlen(last_replace) > (COLS / 3)) {
		strncpy(buf, last_replace, COLS / 3);
		sprintf(&buf[COLS / 3 - 1], "...");
	    } else
		sprintf(buf, "%s", last_replace);

	    i = statusq(replace_list_2, REPLACE_LIST_2_LEN, "",
			_("Replace with [%s]"), buf);
	}
	else
	    i = statusq(replace_list_2, REPLACE_LIST_2_LEN, "",
			_("Replace with"));
    }
    else
	i = statusq(replace_list_2, REPLACE_LIST_2_LEN, last_replace, 
			_("Replace with"));

    switch (i) {
    case -1:				/* Aborted enter */
	if (strcmp(last_replace, ""))
	    answer = mallocstrcpy(answer, last_replace);
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    case 0:		/* They actually entered something */
	last_replace = mallocstrcpy(last_replace, answer);
	break;
    default:
        if (i != -2) {	/* First page, last page, for example 
				   could get here */
	    do_early_abort();
	    replace_abort();
	    return 0;
        }
    }

    /* save where we are */
    begin = current;
    beginx = current_x + 1;
    search_last_line = 0;

    while (1) {

	/* Sweet optimization by Rocco here */
	fileptr = findnextstr(replaceall, begin, beginx, prevanswer);

	/* No more matches.  Done! */
	if (!fileptr)
	    break;

	/* If we're here, we've found the search string */
	if (!replaceall)
	    i = do_yesno(1, 1, _("Replace this instance?"));

	if (i > 0 || replaceall) {	/* Yes, replace it!!!! */
	    if (i == 2)
		replaceall = 1;

	    copy = replace_line();
	    if (!copy) {
		statusbar(_("Replace failed: unknown subexpression!"));
		replace_abort();
		return 0;
	    }

	    /* Cleanup */
	    free(current->data);
	    current->data = copy;

	    /* Stop bug where we replace a substring of the replacement text */
	    current_x += strlen(last_replace) - 1;

	    /* Adjust the original cursor position - COULD BE IMPROVED */
	    if (search_last_line) {
		beginx += strlen(last_replace) - strlen(last_search);

		/* For strings that cross the search start/end boundary */
		/* Don't go outside of allocated memory */
		if (beginx < 1)
		    beginx = 1;
	    }

	    edit_refresh();
	    set_modified();
	    numreplaced++;
	} else if (i == -1)	/* Abort, else do nothing and continue loop */
	    break;
    }

    current = begin;
    current_x = beginx - 1;
    renumber_all();
    edit_update(current, CENTER);
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
	    edit_update(current, CENTER);
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
	edit_update(current, CENTER);
    } else {
	for (fileptr = fileage; fileptr != NULL && i < line; i++)
	    fileptr = fileptr->next;

	current = fileptr;
	current_x = 0;
	edit_update(current, CENTER);
    }

    goto_abort();
    return 1;
}

int do_gotoline_void(void)
{
    return do_gotoline(0);
}
