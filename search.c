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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/* Regular expression helper functions */

#ifdef HAVE_REGEX_H
void regexp_init(const char *regexp)
{
    regcomp(&search_regexp, regexp, ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE);
    SET(REGEXP_COMPILED);
}

void regexp_cleanup(void)
{
    UNSET(REGEXP_COMPILED);
    regfree(&search_regexp);
}
#endif

void search_init_globals(void)
{
    if (last_search == NULL) {
	last_search = nmalloc(1);
	last_search[0] = 0;
    }
    if (last_replace == NULL) {
	last_replace = nmalloc(1);
	last_replace[0] = 0;
    }
}

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
    static char *backupstring = NULL;

    search_init_globals();

    buf = nmalloc(strlen(last_search) + 5);
    buf[0] = 0;

     /* Okay, fun time.  backupstring is our holder for what is being 
	returned from the statusq call.  Using answer for this would be tricky.
	Here, if we're using PICO_MODE, we only want nano to put the
	old string back up as editable if it's not the same as last_search.

	Otherwise, if we don't already have a backupstring, set it to
	last_search.  */

    if (ISSET(PICO_MODE)) {
	if (backupstring == NULL || !strcmp(backupstring, last_search))
	    backupstring = mallocstrcpy(backupstring, "");
    }
    else if (backupstring == NULL)
	backupstring = mallocstrcpy(backupstring, last_search);

    /* If using Pico messages, we do things the old fashioned way... */
    if (ISSET(PICO_MODE)) {
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
    else
	strcpy(buf, "");

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

    /* This is now one simple call.  It just does a lot */
    i = statusq(0, replacing ? replace_list : whereis_list,
	replacing ? REPLACE_LIST_LEN : WHEREIS_LIST_LEN, backupstring,
	prompt, reprompt, buf);

    /* Cancel any search, or just return with no previous search */
    if ((i == -1) || (i < 0 && !last_search[0])) {
	statusbar(_("Search Cancelled"));
	reset_cursor();
	free(backupstring);
	backupstring = NULL;
	return -1;
    } else if (i == -2) {	/* Same string */
#ifdef HAVE_REGEX_H
	if (ISSET(USE_REGEXP)) {

	    /* If we're in pico mode, answer is "", use last_search! */
	    if (ISSET(PICO_MODE))
		regexp_init(last_search);
	    else
		regexp_init(answer);
	}
#else
	;
#endif
    } else if (i == 0) {	/* They entered something new */
#ifdef HAVE_REGEX_H
	if (ISSET(USE_REGEXP))
	    regexp_init(answer);
#endif
	free(backupstring);
	backupstring = NULL;
	last_replace[0] = '\0';
    } else if (i == NANO_CASE_KEY) {	/* They want it case sensitive */
	free(backupstring);
	backupstring = NULL;
	backupstring = mallocstrcpy(backupstring, answer);

	if (ISSET(CASE_SENSITIVE))
	    UNSET(CASE_SENSITIVE);
	else
	    SET(CASE_SENSITIVE);

	return 1;
    } else if (i == NANO_OTHERSEARCH_KEY) {
	backupstring = mallocstrcpy(backupstring, answer);
	return -2;		/* Call the opposite search function */
    } else if (i == NANO_FROMSEARCHTOGOTO_KEY) {
	free(backupstring);
	backupstring = NULL;
	do_gotoline_void();
	return -3;
    } else {			/* First line key, etc. */
	do_early_abort();
	free(backupstring);
	backupstring = NULL;
	return -3;
    }

    return 0;
}

void not_found_msg(char *str)
{
    if (strlen(str) <= COLS / 2)
	statusbar(_("\"%s\" not found"), str);
    else {
	char *foo = NULL;

	foo = mallocstrcpy(foo, str);
	foo[COLS / 2] = 0;
	statusbar(_("\"%s...\" not found"), foo);

	free(foo);
    }
}

filestruct *findnextstr(int quiet, filestruct * begin, int beginx,
			char *needle)
{
    filestruct *fileptr;
    char *searchstr, *found = NULL, *tmp;
    int past_editbot = 0, current_x_find;

    fileptr = current;

    current_x_find = current_x + 1;

    /* Are we searching the last line? (i.e. the line where search started) */
    if ((fileptr == begin) && (current_x_find < beginx))
	search_last_line = 1;

    /* Make sure we haven't passed the end of the string */
    if (strlen(fileptr->data) < current_x_find)
	current_x_find--;

    searchstr = &fileptr->data[current_x_find];

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
    current_x_find = 0;
    for (tmp = fileptr->data; tmp != found; tmp++)
	current_x_find++;

    /* Ensure we haven't wrap around again! */
    if ((search_last_line) && (current_x_find >= beginx)) {
	if (!quiet)
	    not_found_msg(needle);
	return NULL;
    }

    /* Set globals now that we are sure we found something */
    current = fileptr;
    current_x = current_x_find;

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
    if (!ISSET(PICO_MODE) && !strcmp(answer, "")) {
	statusbar(_("Search Cancelled"));
	search_abort();
	return 0;
    }

     /* If answer is now == "", then PICO_MODE is set.  So, copy
	last_search into answer... */

    if (!strcmp(answer, ""))
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

    search_last_line = 0;
    findnextstr(0, current, current_x, answer);
    search_abort();
    return 1;
}

void print_replaced(int num)
{
    if (num > 1)
	statusbar(_("Replaced %d occurrences"), num);
    else if (num == 1)
	statusbar(_("Replaced 1 occurrence"));
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

/* step through each replace word and prompt user before replacing word */
int do_replace_loop(char *prevanswer, filestruct *begin, int *beginx,
			int wholewords, int *i)
{
    int replaceall = 0, numreplaced = 0;
    filestruct *fileptr;
    char *copy;

    switch (*i) {
    case -1:				/* Aborted enter */
	if (strcmp(last_replace, ""))
	    answer = mallocstrcpy(answer, last_replace);
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    case 0:		/* They actually entered something */
	break;
    default:
        if (*i != -2) {	/* First page, last page, for example 
				   could get here */
	    do_early_abort();
	    replace_abort();
	    return 0;
        }
    }

    if (ISSET(PICO_MODE) && !strcmp(answer, ""))
	answer = mallocstrcpy(answer, last_replace);

    last_replace = mallocstrcpy(last_replace, answer);
    while (1) {

	/* Sweet optimization by Rocco here */
	fileptr = findnextstr(replaceall, begin, *beginx, prevanswer);

	/* No more matches.  Done! */
	if (!fileptr)
	    break;

	/* Make sure only wholewords are found */
	if (wholewords)
	{
	    /* start of line or previous character not a letter */
	    if ((current_x == 0) || (!isalpha((int) fileptr->data[current_x-1])))
	    {
		/* end of line or next character not a letter */
		if (((current_x + strlen(prevanswer)) == strlen(fileptr->data))
			|| (!isalpha((int) fileptr->data[current_x + strlen(prevanswer)])))
		    ;
		else
		    continue;
	    }
	    else
		continue;
	}

	/* If we're here, we've found the search string */
	if (!replaceall) {

	    curs_set(0);
	    do_replace_highlight(TRUE, prevanswer);

	    *i = do_yesno(1, 1, _("Replace this instance?"));

	    do_replace_highlight(FALSE, prevanswer);
	    curs_set(1);
	}

	if (*i > 0 || replaceall) {	/* Yes, replace it!!!! */
	    if (*i == 2)
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
		*beginx += strlen(last_replace) - strlen(last_search);

		/* For strings that cross the search start/end boundary */
		/* Don't go outside of allocated memory */
		if (*beginx < 1)
		    *beginx = 1;
	    }

	    edit_refresh();
	    set_modified();
	    numreplaced++;
	} else if (*i == -1)	/* Abort, else do nothing and continue loop */
	    break;
    }

    return numreplaced;
}

/* Replace a string */
int do_replace(void)
{
    int i, numreplaced, beginx;
    filestruct *begin;
    char *prevanswer = NULL, *buf = NULL;

    if (ISSET(VIEW_MODE)) {
	print_view_warning();
	replace_abort();
	return 0;
    }

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
    if (!ISSET(PICO_MODE) && !strcmp(answer, "")) {
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    }

     /* If answer is now == "", then PICO_MODE is set.  So, copy
	last_search into answer (and prevanswer)... */
    if (!strcmp(answer, "")) {
	answer = mallocstrcpy(answer, last_search);
	prevanswer = mallocstrcpy(prevanswer, last_search);
    } else {
	last_search = mallocstrcpy(last_search, answer);
	prevanswer = mallocstrcpy(prevanswer, answer);
    }

    if (ISSET(PICO_MODE)) {
	buf = nmalloc(strlen(last_replace) + 5);
	if (strcmp(last_replace, "")) {
	    if (strlen(last_replace) > (COLS / 3)) {
		strncpy(buf, last_replace, COLS / 3);
		sprintf(&buf[COLS / 3 - 1], "...");
	    } else
		sprintf(buf, "%s", last_replace);

	    i = statusq(0, replace_list_2, REPLACE_LIST_2_LEN, "",
			_("Replace with [%s]"), buf);
	}
	else
	    i = statusq(0, replace_list_2, REPLACE_LIST_2_LEN, "",
			_("Replace with"));
    }
    else
	i = statusq(0, replace_list_2, REPLACE_LIST_2_LEN, last_replace, 
			_("Replace with"));

    /* save where we are */
    begin = current;
    beginx = current_x + 1;

    search_last_line = 0;

    numreplaced = do_replace_loop(prevanswer, begin, &beginx, FALSE, &i);

    /* restore where we were */
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

int do_gotoline(long line)
{
    long i = 1;

    if (line <= 0) {		/* Ask for it */

	long j = 0;

	j = statusq(0, goto_list, GOTO_LIST_LEN, "", _("Enter line number"));
	if (j != 0) {
	    statusbar(_("Aborted"));
	    goto_abort();
	    return 0;
	}

	line = atoi(answer);

	/* Bounds check */
	if (line <= 0) {
	    statusbar(_("Come on, be reasonable"));
	    goto_abort();
	    return 0;
	}
    }

    for (current = fileage; ((current->next != NULL) && (i < line)); i++)
	current = current->next;

    current_x = 0;
    edit_update(current, CENTER);

    goto_abort();
    return 1;
}

int do_gotoline_void(void)
{
    return do_gotoline(0);
}
