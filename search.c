/* $Id$ */
/**************************************************************************
 *   search.c                                                             *
 *                                                                        *
 *   Copyright (C) 2000-2002 Chris Allegretta                             *
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
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#ifdef ENABLE_NANORC
#include <pwd.h>
#endif
#endif

static int past_editbuff;
	/* findnextstr() is now searching lines not displayed */

/* Regular expression helper functions */

#ifdef HAVE_REGEX_H
void regexp_init(const char *regexp)
{
    regcomp(&search_regexp, regexp, (ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE) | REG_EXTENDED);
    SET(REGEXP_COMPILED);
}

void regexp_cleanup(void)
{
    UNSET(REGEXP_COMPILED);
    regfree(&search_regexp);
}
#endif

void not_found_msg(const char *str)
{
    if (strlen(str) <= COLS / 2)
	statusbar(_("\"%s\" not found"), str);
    else {
	char *foo = mallocstrcpy(NULL, str);

	foo[COLS / 2] = '\0';
	statusbar(_("\"%s...\" not found"), foo);
	free(foo);
    }
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

void search_init_globals(void)
{
    if (last_search == NULL) {
	last_search = charalloc(1);
	last_search[0] = '\0';
    }
    if (last_replace == NULL) {
	last_replace = charalloc(1);
	last_replace[0] = '\0';
    }
}

/* Set up the system variables for a search or replace.  Returns -1 on
   abort, 0 on success, and 1 on rerun calling program
   Return -2 to run opposite program (search -> replace, replace ->
   search).

   replacing = 1 if we call from do_replace, 0 if called from do_search
   func. */
int search_init(int replacing)
{
    int i = 0;
    char *buf;
    static char *backupstring = NULL;

    search_init_globals();

    /* Clear the backupstring if we've changed from Pico mode to regular
       mode */
    if (ISSET(CLEAR_BACKUPSTRING)) {
	free(backupstring);
	backupstring = NULL;
	UNSET(CLEAR_BACKUPSTRING);
    }

     /* Okay, fun time.  backupstring is our holder for what is being
	returned from the statusq call.  Using answer for this would be tricky.
	Here, if we're using PICO_MODE, we only want nano to put the
	old string back up as editable if it's not the same as last_search.

	Otherwise, if we don't already have a backupstring, set it to
	last_search. */
#if 0
/* might need again ;)*/
    if (ISSET(PICO_MODE)) {
	if (backupstring == NULL || !strcmp(backupstring, last_search)) {
	    /* backupstring = mallocstrcpy(backupstring, ""); */
	    backupstring = charalloc(1);
	    backupstring[0] = '\0';
	}
    } else
#endif
    if (backupstring == NULL)
#ifndef NANO_SMALL
	backupstring = mallocstrcpy(backupstring, search_history.current->data);
#else
	backupstring = mallocstrcpy(backupstring, last_search);
#endif

/* NEW TEST */
    if (ISSET(PICO_MODE)) {
	backupstring = mallocstrcpy(backupstring, "");
	search_history.current = (historytype *)&search_history.next;
    }
/* */
    /* If using Pico messages, we do things the old fashioned way... */
    if (ISSET(PICO_MODE) && last_search[0] != '\0') {
	buf = charalloc(COLS / 3 + 7);
	/* We use COLS / 3 here because we need to see more on the line */
	sprintf(buf, " [%.*s%s]", COLS / 3, last_search,
		strlen(last_search) > COLS / 3 ? "..." : "");
    } else {
	buf = charalloc(1);
	buf[0] = '\0';
    }

    /* This is now one simple call.  It just does a lot */
    i = statusq(0, replacing ? replace_list : whereis_list, backupstring,
#ifndef NANO_SMALL
	&search_history,
#endif
	"%s%s%s%s%s%s",
	_("Search"),

	/* This string is just a modifier for the search prompt,
	   no grammar is implied */
	ISSET(CASE_SENSITIVE) ? _(" [Case Sensitive]") : "",

	/* This string is just a modifier for the search prompt,
	   no grammar is implied */
	ISSET(USE_REGEXP) ? _(" [Regexp]") : "",

	/* This string is just a modifier for the search prompt,
	   no grammar is implied */
	ISSET(REVERSE_SEARCH) ? _(" [Backwards]") : "",

	replacing ? _(" (to replace)") : "",
	buf);

    /* Release buf now that we don't need it anymore */
    free(buf);

    /* Cancel any search, or just return with no previous search */
    if (i == -1 || (i < 0 && last_search[0] == '\0')) {
	statusbar(_("Search Cancelled"));
	reset_cursor();
	free(backupstring);
	backupstring = NULL;
#ifndef NANO_SMALL
	search_history.current = search_history.next;
#endif
	return -1;
    } else {
	switch (i) {
	case -2:	/* Same string */
#ifdef HAVE_REGEX_H
	    if (ISSET(USE_REGEXP))
		/* If we're in Pico mode, and answer is "", use
		   last_search! */
		regexp_init(ISSET(PICO_MODE) ? last_search : answer);
#endif
	    break;
	case 0:		/* They entered something new */
#ifdef HAVE_REGEX_H
	    if (ISSET(USE_REGEXP))
		regexp_init(answer);
#endif
	    free(backupstring);
	    backupstring = NULL;
	    last_replace[0] = '\0';
	    break;
#ifndef NANO_SMALL
	case TOGGLE_CASE_KEY:
	    TOGGLE(CASE_SENSITIVE);
	    backupstring = mallocstrcpy(backupstring, answer);
	    return 1;
	case TOGGLE_BACKWARDS_KEY:
	    TOGGLE(REVERSE_SEARCH);
	    backupstring = mallocstrcpy(backupstring, answer);
	    return 1;
#ifdef HAVE_REGEX_H
	case TOGGLE_REGEXP_KEY:
	    TOGGLE(USE_REGEXP);
	    backupstring = mallocstrcpy(backupstring, answer);
	    return 1;
#endif
#endif /* !NANO_SMALL */
	case NANO_OTHERSEARCH_KEY:
	    backupstring = mallocstrcpy(backupstring, answer);
	    return -2;		/* Call the opposite search function */
	case NANO_FROMSEARCHTOGOTO_KEY:
	    free(backupstring);
	    backupstring = NULL;
#ifndef NANO_SMALL
	    search_history.current = search_history.next;
#endif
	    i = (int)strtol(answer, &buf, 10);		/* just testing answer here */
	    if (!(errno == ERANGE || *answer == '\0' || *buf != '\0'))
		do_gotoline(-1, 0);
	    else
		do_gotoline_void();
	    return -3;
	default:
	    do_early_abort();
	    free(backupstring);
	    backupstring = NULL;
	    return -3;
	}
    }
    return 0;
}

int is_whole_word(int curr_pos, const char *datastr, const char *searchword)
{
    size_t sln = curr_pos + strlen(searchword);

    /* start of line or previous character not a letter and end of line
     * or next character not a letter */
    return (curr_pos < 1 || !isalpha((int) datastr[curr_pos - 1])) &&
	(sln == strlen(datastr) || !isalpha((int) datastr[sln]));
}

filestruct *findnextstr(int quiet, int bracket_mode,
			const filestruct *begin, int beginx,
			const char *needle)
{
    filestruct *fileptr = current;
    const char *searchstr, *rev_start = NULL, *found = NULL;
    int current_x_find = 0;

    past_editbuff = 0;

    if (!ISSET(REVERSE_SEARCH)) {		/* forward search */
	/* Argh, current_x is set to -1 by nano.c:do_int_spell_fix(), and
	 * strlen returns size_t, which is unsigned. */
	assert(current_x < 0 || current_x <= strlen(fileptr->data));
	current_x_find = current_x;
	if (current_x_find < 0 || fileptr->data[current_x_find] != '\0')
	    current_x_find++;

	searchstr = &fileptr->data[current_x_find];

	/* Look for needle in searchstr */
	while ((found = strstrwrapper(searchstr, needle, rev_start, current_x_find)) == NULL) {

	    /* finished processing file, get out */
	    if (search_last_line) {
		if (!quiet)
		    not_found_msg(needle);
		update_line(fileptr, current_x);
	        return NULL;
	    }

	    update_line(fileptr, 0);

	    /* reset current_x_find between lines */
	    current_x_find = 0;

	    fileptr = fileptr->next;

	    if (fileptr == editbot)
		past_editbuff = 1;

	    /* EOF reached ?, wrap around once */
	    if (fileptr == NULL) {
		/* don't wrap if looking for bracket match */
		if (bracket_mode)
		    return NULL;
		fileptr = fileage;
		past_editbuff = 1;
		if (!quiet) {
		    statusbar(_("Search Wrapped"));
		    SET(DISABLE_CURPOS);
		}
	    }

	    /* Original start line reached */
	    if (fileptr == begin)
		search_last_line = 1;

	    searchstr = fileptr->data;
	}

	/* We found an instance */
	current_x_find = found - fileptr->data;
	/* Ensure we haven't wrapped around again! */
	if ((search_last_line) && (current_x_find > beginx)) {
	    if (!quiet)
		not_found_msg(needle);
	    return NULL;
	}
    }
#ifndef NANO_SMALL
    else {	/* reverse search */
	current_x_find = current_x - 1;
	/* Make sure we haven't passed the begining of the string */
	rev_start = &fileptr->data[current_x_find];
	searchstr = fileptr->data;

	/* Look for needle in searchstr */
	while ((found = strstrwrapper(searchstr, needle, rev_start, current_x_find)) == NULL) {
	    /* finished processing file, get out */
	    if (search_last_line) {
		if (!quiet)
		    not_found_msg(needle);
		return NULL;
	    }

	    update_line(fileptr, 0);

	    /* reset current_x_find between lines */
	    current_x_find = 0;

	    fileptr = fileptr->prev;

	    if (fileptr == edittop->prev)
		past_editbuff = 1;

	    /* SOF reached?, wrap around once */
/* ? */	    if (fileptr == NULL) {
		if (bracket_mode)
		   return NULL;
		fileptr = filebot;
		past_editbuff = 1;
		if (!quiet) {
		    statusbar(_("Search Wrapped"));
		    SET(DISABLE_CURPOS);
		}
	    }
	    /* Original start line reached */
	    if (fileptr == begin)
		search_last_line = 1;

	    searchstr = fileptr->data;
	    rev_start = fileptr->data + strlen(fileptr->data);
	}

	/* We found an instance */
	current_x_find = found - fileptr->data;
	/* Ensure we haven't wrapped around again! */
	if ((search_last_line) && (current_x_find < beginx)) {
	    if (!quiet)
		not_found_msg(needle);
	    return NULL;
	}
    }
#endif /* !NANO_SMALL */

    /* Set globals now that we are sure we found something */
    current = fileptr;
    current_x = current_x_find;

    if (!bracket_mode) {
	if (past_editbuff)
	   edit_update(fileptr, CENTER);
	else
	   update_line(current, current_x);

	placewewant = xplustabs();
	reset_cursor();
    }
    return fileptr;
}

/* Search for a string. */
int do_search(void)
{
    int i;
    filestruct *fileptr = current, *didfind;
    int fileptr_x = current_x;

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
    if (!ISSET(PICO_MODE) && answer[0] == '\0') {
	statusbar(_("Search Cancelled"));
#ifndef NANO_SMALL
	search_history.current = search_history.next;
#endif
	search_abort();
	return 0;
    }

     /* If answer is now == "", then PICO_MODE is set.  So, copy
	last_search into answer... */

    if (answer[0] == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

#ifndef NANO_SMALL
    /* add this search string to the search history list */
    update_history(&search_history, answer);
#endif	/* !NANO_SMALL */

    search_last_line = 0;
    didfind = findnextstr(FALSE, FALSE, current, current_x, answer);

    if ((fileptr == current) && (fileptr_x == current_x) && didfind != NULL)
	statusbar(_("This is the only occurrence"));

    search_abort();

    return 1;
}

void replace_abort(void)
{
    /* Identical to search_abort, so we'll call it here.  If it
       does something different later, we can change it back.  For now
       it's just a waste to duplicate code */
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

    /* Iterate through the replacement text to handle subexpression
     * replacement using \1, \2, \3, etc. */

    c = last_replace;
    while (*c != '\0') {
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
		       exist. */
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
	 * text, return NULL, indicating an error */
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
    copy = charalloc(new_line_size);

    /* Head of Original Line */
    strncpy(copy, current->data, current_x);
    copy[current_x] = '\0';

    /* Replacement Text */
    if (!ISSET(USE_REGEXP))
	strcat(copy, last_replace);
#ifdef HAVE_REGEX_H
    else
	replace_regexp(copy + current_x, 1);
#endif

    /* The tail of the original line */

    /* This may expose other bugs, because it no longer goes through
     * each character in the string and tests for string goodness.  But
     * because we can assume the invariant that current->data is less
     * than current_x + strlen(last_search) long, this should be safe. 
     * Or it will expose bugs ;-) */
    tmp = current->data + current_x + search_match_count;
    strcat(copy, tmp);

    return copy;
}

void print_replaced(int num)
{
    if (num > 1)
	statusbar(_("Replaced %d occurrences"), num);
    else if (num == 1)
	statusbar(_("Replaced 1 occurrence"));
}

/* step through each replace word and prompt user before replacing word */
int do_replace_loop(const char *prevanswer, const filestruct *begin,
			int *beginx, int wholewords, int *i)
{
    int replaceall = 0, numreplaced = 0;

    filestruct *fileptr = NULL;
    char *copy;

    switch (*i) {
    case -1:		/* Aborted enter */
	if (last_replace[0] != '\0')
	    answer = mallocstrcpy(answer, last_replace);
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    case 0:		/* They actually entered something */
	break;
    default:
        if (*i != -2) {	/* First page, last page, for example, could
			   get here */
	    do_early_abort();
	    replace_abort();
	    return 0;
        }
    }

    if (ISSET(PICO_MODE) && answer[0] == '\0')
	answer = mallocstrcpy(answer, last_replace);

    last_replace = mallocstrcpy(last_replace, answer);
    while (1) {
	/* Sweet optimization by Rocco here */
	fileptr = findnextstr(fileptr || replaceall || search_last_line,
				FALSE, begin, *beginx, prevanswer);

	/* No more matches.  Done! */
	if (fileptr == NULL)
	    break;

	/* Make sure only whole words are found */
	if (wholewords && !is_whole_word(current_x, fileptr->data, prevanswer))
	    continue;

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
	    if (copy == NULL) {
		statusbar(_("Replace failed: unknown subexpression!"));
		replace_abort();
		return 0;
	    }

	    /* Cleanup */
	    totsize -= strlen(current->data);
	    free(current->data);
	    current->data = copy;
	    totsize += strlen(current->data);

	    if (!ISSET(REVERSE_SEARCH)) {
		/* Stop bug where we replace a substring of the
		   replacement text */
		current_x += strlen(last_replace) - 1;

		/* Adjust the original cursor position - COULD BE IMPROVED */
		if (search_last_line) {
		    *beginx += strlen(last_replace) - strlen(last_search);

		    /* For strings that cross the search start/end boundary */

		    /* Don't go outside of allocated memory */
		    if (*beginx < 1)
			*beginx = 1;
		}
	    } else {
		if (current_x > 1)
		    current_x--;

		if (search_last_line) {
		    *beginx += strlen(last_replace) - strlen(last_search);

		    if (*beginx > strlen(current->data))
			*beginx = strlen(current->data);
		}
	    }

	    edit_refresh();
	    set_modified();
	    numreplaced++;
	} else if (*i == -1)	/* Abort, else do nothing and continue
				   loop */
	    break;
    }

    return numreplaced;
}

/* Replace a string */
int do_replace(void)
{
    int i, numreplaced, beginx;
    filestruct *begin;
    char *prevanswer = NULL;

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

#ifndef NANO_SMALL
    update_history(&search_history, answer);
#endif	/* !NANO_SMALL */

    /* Again, there was a previous string, but they deleted it and hit enter */
    if (!ISSET(PICO_MODE) && answer[0] == '\0') {
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    }

     /* If answer is now == "", then PICO_MODE is set.  So, copy
	last_search into answer (and prevanswer)... */
    if (answer[0] == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

    prevanswer = mallocstrcpy(prevanswer, last_search);

    if (ISSET(PICO_MODE) && last_replace[0] != '\0') {
	if (strlen(last_replace) > COLS / 3) {
	    char *buf = charalloc(COLS / 3 + 3);

	    strncpy(buf, last_replace, COLS / 3 - 1);
	    strcpy(buf + COLS / 3 - 1, "...");
	    i = statusq(0, replace_list_2, "",
#ifndef NANO_SMALL
		&replace_history,
#endif
		_("Replace with [%s]"), buf);
	    free(buf);
	} else
	    i = statusq(0, replace_list_2, "",
#ifndef NANO_SMALL
		 &replace_history,
#endif
		_("Replace with [%s]") ,last_replace);
    } else {
#ifndef NANO_SMALL
	replace_history.current = (historytype *)&replace_history.next;
	last_replace = mallocstrcpy(last_replace, "");
#endif
	i = statusq(0, replace_list_2, last_replace,
#ifndef NANO_SMALL
		&replace_history,
#endif
		_("Replace with"));
   }
#ifndef NANO_SMALL
    if (i == 0)
	update_history(&replace_history, answer);
#endif	/* !NANO_SMALL */

    begin = current;
    beginx = current_x;
    search_last_line = 0;

    numreplaced = do_replace_loop(prevanswer, begin, &beginx, FALSE, &i);

    /* restore where we were */
    current = begin;
    current_x = beginx;
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

int do_gotoline(int line, int save_pos)
{
    if (line <= 0) {		/* Ask for it */
	if (statusq(0, goto_list, (line ? answer : ""), 0, _("Enter line number"))) {
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

    for (current = fileage; current->next != NULL && line > 1; line--)
	current = current->next;

    current_x = 0;

    /* if save_pos is nonzero, don't change the cursor position when
       updating the edit window */
    if (save_pos)
    	edit_update(current, NONE);
    else
	edit_update(current, CENTER);
    placewewant = 0;
    goto_abort();
    blank_statusbar_refresh();
    return 1;
}

int do_gotoline_void(void)
{
    return do_gotoline(0, 0);
}

#if defined(ENABLE_MULTIBUFFER) || !defined(DISABLE_SPELLER)
void do_gotopos(int line, int pos_x, int pos_y, int pos_placewewant)
{
    /* since do_gotoline() resets the x-coordinate but not the
       y-coordinate, set the coordinates up this way */
    current_y = pos_y;
    do_gotoline(line, 1);

    /* make sure that the x-coordinate is sane here */
    if (pos_x > strlen(current->data))
	pos_x = strlen(current->data);

    /* set the rest of the coordinates up */
    current_x = pos_x;
    placewewant = pos_placewewant;
    update_line(current, pos_x);
}
#endif

#if !defined(NANO_SMALL) && defined(HAVE_REGEX_H)
int do_find_bracket(void)
{
    char ch_under_cursor, wanted_ch;
    const char *pos, *brackets = "([{<>}])";
    char regexp_pat[] = "[  ]";
    int offset, have_past_editbuff = 0, flagsave, current_x_save, count = 1;
    filestruct *current_save;

    ch_under_cursor = current->data[current_x];

/*    if ((!(pos = strchr(brackets, ch_under_cursor))) || (!((offset = pos - brackets) < 8))) { */

    if (((pos = strchr(brackets, ch_under_cursor)) == NULL) || (((offset = pos - brackets) < 8) == 0)) {
	statusbar(_("Not a bracket"));
	return 1;
    }

    blank_statusbar_refresh();

    wanted_ch = *(brackets + ((strlen(brackets) - (offset + 1))));

    current_x_save = current_x;
    current_save = current;
    flagsave = flags;
    SET(USE_REGEXP);

/* apparent near redundancy with regexp_pat[] here is needed, [][] works, [[]] doesn't */

    if (offset < (strlen(brackets) / 2)) {			/* on a left bracket */
	regexp_pat[1] = wanted_ch;
	regexp_pat[2] = ch_under_cursor;
	UNSET(REVERSE_SEARCH);
    } else {							/* on a right bracket */
	regexp_pat[1] = ch_under_cursor;
	regexp_pat[2] = wanted_ch;
	SET(REVERSE_SEARCH);
    }

    regexp_init(regexp_pat);

    while (1) {
	search_last_line = 0;
	if (findnextstr(1, 1, current, current_x, regexp_pat) != NULL) {
	    have_past_editbuff |= past_editbuff;
	    if (current->data[current_x] == ch_under_cursor)	/* found identical bracket */
		count++;
	    else {						/* found complementary bracket */
		if (!(--count)) {
		    if (have_past_editbuff)
			edit_update(current, CENTER);
		    else
			update_line(current, current_x);
		    placewewant = xplustabs();
		    reset_cursor();
		    break;
		}
	    }
	} else {						/* didn't find either left or right bracket */
	    statusbar(_("No matching bracket"));
	    current_x = current_x_save;
	    current = current_save;
	    break;
	}
    }

    if (ISSET(REGEXP_COMPILED))
	regexp_cleanup();
    flags = flagsave;
    return 0;
}
#endif

#ifndef NANO_SMALL
/*
 * search and replace history list support functions
 */

/* initialize search and replace history lists */
void history_init(void)
{
    search_history.next = (historytype *)&search_history.prev;
    search_history.prev = NULL;
    search_history.tail = (historytype *)&search_history.next;
    search_history.current = search_history.next;
    search_history.count = 0;
    search_history.len = 0;

    replace_history.next = (historytype *)&replace_history.prev;
    replace_history.prev = NULL;
    replace_history.tail = (historytype *)&replace_history.next;
    replace_history.current = replace_history.next;
    replace_history.count = 0;
    replace_history.len = 0;
}

/* find first node containing string *s in history list *h */
historytype *find_node(historytype *h, char *s)
{
    for ( ; h->next ; h = h->next)
	if (strcmp(s, h->data) == 0)
	    return h;
    return NULL;
}

/* remove node *r */
void remove_node(historytype *r)
{
    r->prev->next = r->next;
    r->next->prev = r->prev;
    free(r->data);
    free(r);
}

/* add a node after node *h */
void insert_node(historytype *h, const char *s)
{
    historytype *a;

    a = nmalloc(sizeof(historytype));
    a->next = h->next;
    a->prev = h->next->prev;
    h->next->prev = a;
    h->next = a;
    a->data = mallocstrcpy(NULL, s);
}

/* update history list */
void update_history(historyheadtype *h, char *s)
{
    historytype *p;

    if ((p = find_node(h->next, s))) {
	if (p == h->next)		/* catch delete and re-insert of same string in 1st node */
	    goto up_hs;
	remove_node(p);				/* delete identical older string */
	h->count--;
    }
    if (h->count == MAX_SEARCH_HISTORY) {	/* list 'full', delete oldest */
	remove_node(h->tail);
	h->count--;
    }
    insert_node((historytype *)h, s);
    h->count++;
up_hs:
    h->current = h->next;
}

/* return a pointer to either the next older history or NULL if no more */
char *get_history_older(historyheadtype *h)
{
    if (h->current->next) {		/* any older entries ? */
	h->current = h->current->next;	/* yes */
	return h->current->data;	/* return it */
    }
    return NULL;			/* end of list */
}

char *get_history_newer(historyheadtype *h)
{
    if (h->current->prev) {
	h->current = h->current->prev;
	if (h->current->prev)
	    return h->current->data;
    }
    return NULL;
}

/* get a completion */
char *get_history_completion(historyheadtype *h, char *s)
{
    historytype *p;

    for (p = h->current->next ; p->next ; p = p->next) {
	if ((strncmp(s, p->data, h->len) == 0) && (strlen(p->data) != h->len)) {
	    h->current = p;
	    return p->data;
	}
    }
    h->current = (historytype*)h;
    null_at(&s, h->len);
    return s;
}

/* free a history list */
void free_history(historyheadtype *h)
{
    historytype *p, *n;

    for (p = h->next ; (n = p->next) ; p = n)
	remove_node(p);
}

/* end of history support functions */
#endif /* !NANO_SMALL */
