/* $Id$ */
/**************************************************************************
 *   search.c                                                             *
 *                                                                        *
 *   Copyright (C) 2000-2004 Chris Allegretta                             *
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
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"


#ifdef HAVE_REGEX_H
static int regexp_compiled = FALSE;

/* Regular expression helper functions. */

/* Compile the given regular expression.  Return value 0 means the
 * expression was invalid, and we wrote an error message on the status
 * bar.  Return value 1 means success. */
int regexp_init(const char *regexp)
{
    int rc = regcomp(&search_regexp, regexp, REG_EXTENDED |
	(ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE));

    assert(!regexp_compiled);
    if (rc != 0) {
	size_t len = regerror(rc, &search_regexp, NULL, 0);
	char *str = charalloc(len);

	regerror(rc, &search_regexp, str, len);
	statusbar(_("Bad regex \"%s\": %s"), regexp, str);
	free(str);
	return 0;
    }

    regexp_compiled = TRUE;
    return 1;
}

void regexp_cleanup(void)
{
    if (regexp_compiled) {
	regexp_compiled = FALSE;
	regfree(&search_regexp);
    }
}
#endif

void not_found_msg(const char *str)
{
    char *disp;
    int numchars;
 
    assert(str != NULL);

    disp = display_string(str, 0, (COLS / 2) + 1);
    numchars = strnlen(disp, COLS / 2);

    statusbar(_("\"%.*s%s\" not found"), numchars, disp,
	disp[numchars] == '\0' ? "" : "...");

    free(disp);
}

void search_abort(void)
{
    display_main_list();
#ifndef NANO_SMALL
    if (ISSET(MARK_ISSET))
	edit_refresh();
#endif
#ifdef HAVE_REGEX_H
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

/* Set up the system variables for a search or replace.  Return -1 if
 * the search should be cancelled (due to Cancel, Go to Line, or a
 * failed regcomp()).  Return 0 on success, and 1 on rerun calling
 * program.  Return -2 to run opposite program (search -> replace,
 * replace -> search).
 *
 * replacing = 1 if we call from do_replace(), 0 if called from
 * do_search(). */
int search_init(int replacing)
{
    int i = 0;
    char *buf;
    static char *backupstring = NULL;

    /* We display the search prompt below.  If the user types a partial
     * search string and then Replace or a toggle, we will return to
     * do_search() or do_replace() and be called again.  In that case,
     * we should put the same search string back up.  backupstring holds
     * this string. */

    search_init_globals();

    /* If we don't already have a backupstring, set it. */
    if (backupstring == NULL)
	backupstring = mallocstrcpy(NULL, "");

#ifndef NANO_SMALL
    search_history.current = (historytype *)&search_history.next;
#endif

    if (last_search[0] != '\0') {
	char *disp = display_string(last_search, 0, COLS / 3);

	buf = charalloc(COLS / 3 + 7);
	/* We use COLS / 3 here because we need to see more on the
	 * line. */
	sprintf(buf, " [%s%s]", disp,
		strlenpt(last_search) > COLS / 3 ? "..." : "");
	free(disp);
    } else {
	buf = charalloc(1);
	buf[0] = '\0';
    }

    /* This is now one simple call.  It just does a lot. */
    i = statusq(FALSE, replacing ? replace_list : whereis_list,
	backupstring,
#ifndef NANO_SMALL
	&search_history,
#endif
	"%s%s%s%s%s%s", _("Search"),

#ifndef NANO_SMALL
	/* This string is just a modifier for the search prompt; no
	 * grammar is implied. */
	ISSET(CASE_SENSITIVE) ? _(" [Case Sensitive]") :
#endif
		"",

#ifdef HAVE_REGEX_H
	/* This string is just a modifier for the search prompt; no
	 * grammar is implied. */
	ISSET(USE_REGEXP) ? _(" [Regexp]") :
#endif
		"",

#ifndef NANO_SMALL
	/* This string is just a modifier for the search prompt; no
	 * grammar is implied. */
	ISSET(REVERSE_SEARCH) ? _(" [Backwards]") :
#endif
		"",

	replacing ? _(" (to replace)") : "",
	buf);

    /* Release buf now that we don't need it anymore. */
    free(buf);

    free(backupstring);
    backupstring = NULL;

    /* Cancel any search, or just return with no previous search. */
    if (i == -1 || (i < 0 && last_search[0] == '\0') ||
	    (!replacing && i == 0 && answer[0] == '\0')) {
	statusbar(_("Search Cancelled"));
#ifndef NANO_SMALL
	search_history.current = search_history.next;
#endif
	return -1;
    } else {
	switch (i) {
	case -2:	/* It's the same string. */
#ifdef HAVE_REGEX_H
	    /* Since answer is "", use last_search! */
	    if (ISSET(USE_REGEXP) && regexp_init(last_search) == 0)
		return -1;
#endif
	    break;
	case 0:		/* They entered something new. */
	    last_replace[0] = '\0';
#ifdef HAVE_REGEX_H
	    if (ISSET(USE_REGEXP) && regexp_init(answer) == 0)
		return -1;
#endif
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
	    return -2;	/* Call the opposite search function. */
	case NANO_FROMSEARCHTOGOTO_KEY:
#ifndef NANO_SMALL
	    search_history.current = search_history.next;
#endif
	    i = (int)strtol(answer, &buf, 10);	/* Just testing answer here. */
	    if (!(errno == ERANGE || *answer == '\0' || *buf != '\0'))
		do_gotoline(-1, FALSE);
	    else
		do_gotoline_void();
	    /* Fall through. */
	default:
	    return -1;
	}
    }
    return 0;
}

int is_whole_word(int curr_pos, const char *datastr, const char
	*searchword)
{
    size_t sln = curr_pos + strlen(searchword);

    /* Start of line or previous character is not a letter and end of
     * line or next character is not a letter. */
    return (curr_pos < 1 || !isalpha((int)datastr[curr_pos - 1])) &&
	(sln == strlen(datastr) || !isalpha((int)datastr[sln]));
}

/* Look for needle, starting at current, column current_x.  If
 * no_sameline is nonzero, skip over begin when looking for needle.
 * begin is the line where we first started searching, at column beginx.
 * If can_display_wrap is nonzero, we put messages on the statusbar, and
 * wrap around the file boundaries.  The return value specifies whether
 * we found anything. */
int findnextstr(int can_display_wrap, int wholeword, const filestruct
	*begin, size_t beginx, const char *needle, int no_sameline)
{
    filestruct *fileptr = current;
    const char *rev_start = NULL, *found = NULL;
    size_t current_x_find = 0;
	/* Where needle was found. */

    /* rev_start might end up 1 character before the start or after the
     * end of the line.  This won't be a problem because strstrwrapper()
     * will return immediately and say that no match was found, and
     * rev_start will be properly set when the search continues on the
     * previous or next line. */
#ifndef NANO_SMALL
    if (ISSET(REVERSE_SEARCH))
	rev_start = fileptr->data + (current_x - 1);
    else
#endif
	rev_start = fileptr->data + (current_x + 1);

    /* Look for needle in searchstr. */
    while (TRUE) {
	found = strstrwrapper(fileptr->data, needle, rev_start);

	if (found != NULL && (!wholeword || is_whole_word(found -
		fileptr->data, fileptr->data, needle))) {
	    if (!no_sameline || fileptr != current)
		break;
	}

	/* Finished processing file, get out. */
	if (search_last_line) {
	    if (can_display_wrap)
		not_found_msg(needle);
	    return 0;
	}
	fileptr =
#ifndef NANO_SMALL
		ISSET(REVERSE_SEARCH) ? fileptr->prev :
#endif
		fileptr->next;

	/* Start or end of buffer reached; wrap around. */
	if (fileptr == NULL) {
	    if (!can_display_wrap)
		return 0;
	    fileptr =
#ifndef NANO_SMALL
		ISSET(REVERSE_SEARCH) ? filebot :
#endif
		fileage;
	    if (can_display_wrap)
		statusbar(_("Search Wrapped"));
	}

	/* Original start line reached. */
	if (fileptr == begin)
	    search_last_line = TRUE;
	rev_start = fileptr->data;
#ifndef NANO_SMALL
	if (ISSET(REVERSE_SEARCH))
	    rev_start += strlen(fileptr->data);
#endif
    }

    /* We found an instance. */
    current_x_find = found - fileptr->data;

    /* Ensure we haven't wrapped around again! */
    if (search_last_line &&
#ifndef NANO_SMALL
	((!ISSET(REVERSE_SEARCH) && current_x_find > beginx) ||
	(ISSET(REVERSE_SEARCH) && current_x_find < beginx))
#else
	current_x_find > beginx
#endif
	) {

	if (can_display_wrap)
	    not_found_msg(needle);
	return 0;
    }

    /* Set globals now that we are sure we found something. */
    current = fileptr;
    current_x = current_x_find;

    return 1;
}

/* Search for a string. */
void do_search(void)
{
    int old_pww = placewewant, i, fileptr_x = current_x, didfind;
    filestruct *fileptr = current;

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    i = search_init(0);
    if (i == -1)	/* Cancel, Go to Line, blank search string, or
			 * regcomp() failed. */
	search_abort();
    else if (i == -2)	/* Replace. */
	do_replace();
#ifndef NANO_SMALL
    else if (i == 1)	/* Case Sensitive, Backwards, or Regexp search
			 * toggle. */
	do_search();
#endif

    if (i != 0)
	return;

    /* If answer is now "", copy last_search into answer. */
    if (answer[0] == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

#ifndef NANO_SMALL
    /* If answer is not "", add this search string to the search history
     * list. */
    if (answer[0] != '\0')
	update_history(&search_history, answer);
#endif

    search_last_line = FALSE;
    didfind = findnextstr(TRUE, FALSE, current, current_x, answer, FALSE);

    /* Check to see if there's only one occurrence of the string and
     * we're on it now. */
    if (fileptr == current && fileptr_x == current_x && didfind) {
#ifdef HAVE_REGEX_H
	/* Do the search again, skipping over the current line, if we're
	 * doing a bol and/or eol regex search ("^", "$", or "^$"), so
	 * that we find one only once per line.  We should only end up
	 * back at the same position if the string isn't found again, in
	 * which case it's the only occurrence. */
	if (ISSET(USE_REGEXP) && regexp_bol_or_eol(&search_regexp, last_search)) {
	    didfind = findnextstr(TRUE, FALSE, current, current_x, answer, TRUE);
	    if (fileptr == current && fileptr_x == current_x && !didfind)
		statusbar(_("This is the only occurrence"));
	} else {
#endif
	    statusbar(_("This is the only occurrence"));
#ifdef HAVE_REGEX_H
	}
#endif
    }

    placewewant = xplustabs();
    edit_redraw(fileptr, old_pww);
    search_abort();
}

#ifndef NANO_SMALL
/* Search for the next string without prompting. */
void do_research(void)
{
    int old_pww = placewewant, fileptr_x = current_x, didfind;
    filestruct *fileptr = current;

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif
    search_init_globals();

    if (last_search[0] != '\0') {

#ifdef HAVE_REGEX_H
	/* Since answer is "", use last_search! */
	if (ISSET(USE_REGEXP) && regexp_init(last_search) == 0)
	    return;
#endif

	search_last_line = FALSE;
	didfind = findnextstr(TRUE, FALSE, current, current_x, last_search, FALSE);

	/* Check to see if there's only one occurrence of the string and
	 * we're on it now. */
	if (fileptr == current && fileptr_x == current_x && didfind) {
#ifdef HAVE_REGEX_H
	    /* Do the search again, skipping over the current line, if
	     * we're doing a bol and/or eol regex search ("^", "$", or
	     * "^$"), so that we find one only once per line.  We should
	     * only end up back at the same position if the string isn't
	     * found again, in which case it's the only occurrence. */
	    if (ISSET(USE_REGEXP) && regexp_bol_or_eol(&search_regexp, last_search)) {
		didfind = findnextstr(TRUE, FALSE, current, current_x, answer, TRUE);
		if (fileptr == current && fileptr_x == current_x && !didfind)
		    statusbar(_("This is the only occurrence"));
	    } else {
#endif
		statusbar(_("This is the only occurrence"));
#ifdef HAVE_REGEX_H
	    }
#endif
	}
    } else
        statusbar(_("No current search pattern"));

    placewewant = xplustabs();
    edit_redraw(fileptr, old_pww);
    search_abort();
}
#endif

void replace_abort(void)
{
    /* Identical to search_abort(), so we'll call it here.  If it does
     * something different later, we can change it back.  For now, it's
     * just a waste to duplicate code. */
    search_abort();
    placewewant = xplustabs();
}

#ifdef HAVE_REGEX_H
int replace_regexp(char *string, int create_flag)
{
    /* Split personality here - if create_flag is zero, just calculate
     * the size of the replacement line (necessary because of
     * subexpressions \1 to \9 in the replaced text). */

    const char *c = last_replace;
    int search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;
    int new_size = strlen(current->data) + 1 - search_match_count;

    /* Iterate through the replacement text to handle subexpression
     * replacement using \1, \2, \3, etc. */
    while (*c != '\0') {
	int num = (int)(*(c + 1) - '0');

	if (*c != '\\' || num < 1 || num > 9 || num > search_regexp.re_nsub) {
	    if (create_flag)
		*string++ = *c;
	    c++;
	    new_size++;
	} else {
	    int i = regmatches[num].rm_eo - regmatches[num].rm_so;

	    /* Skip over the replacement expression. */
	    c += 2;

	    /* But add the length of the subexpression to new_size. */
	    new_size += i;

	    /* And if create_flag is nonzero, append the result of the
	     * subexpression match to the new line. */
	    if (create_flag) {
		strncpy(string, current->data + current_x +
			regmatches[num].rm_so, i);
		string += i;
	    }
	}
    }

    if (create_flag)
	*string = '\0';

    return new_size;
}
#endif

char *replace_line(const char *needle)
{
    char *copy;
    int new_line_size;
    int search_match_count;

    /* Calculate the size of the new line. */
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP)) {
	search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;
	new_line_size = replace_regexp(NULL, 0);
    } else {
#endif
	search_match_count = strlen(needle);
	new_line_size = strlen(current->data) - search_match_count +
	    strlen(answer) + 1;
#ifdef HAVE_REGEX_H
    }
#endif

    /* Create the buffer. */
    copy = charalloc(new_line_size);

    /* The head of the original line. */
    strncpy(copy, current->data, current_x);

    /* The replacement text. */
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP))
	replace_regexp(copy + current_x, TRUE);
    else
#endif
	strcpy(copy + current_x, answer);

    /* The tail of the original line. */
    assert(current_x + search_match_count <= strlen(current->data));
    strcat(copy, current->data + current_x + search_match_count);

    return copy;
}

/* Step through each replace word and prompt user before replacing.
 * Parameters real_current and real_current_x are needed by the internal
 * speller, to allow the cursor position to be updated when a word
 * before the cursor is replaced by a shorter word.
 *
 * needle is the string to seek.  We replace it with answer.  Return -1
 * if needle isn't found, else the number of replacements performed. */
int do_replace_loop(const char *needle, const filestruct *real_current,
	size_t *real_current_x, int wholewords)
{
    int old_pww = placewewant, replaceall = 0, numreplaced = -1;
    size_t current_x_save = current_x;
    const filestruct *current_save = current;
#ifdef HAVE_REGEX_H
    /* The starting-line match and bol/eol regex flags. */
    int begin_line = FALSE, bol_or_eol = FALSE;
#endif
#ifndef NANO_SMALL
    int mark_set = ISSET(MARK_ISSET);

    UNSET(MARK_ISSET);
    edit_refresh();
#endif

    while (findnextstr(TRUE, wholewords, current_save, current_x_save,
	needle,
#ifdef HAVE_REGEX_H
	/* We should find a bol and/or eol regex only once per line.  If
	 * the bol_or_eol flag is set, it means that the last search
	 * found one on the beginning line, so we should skip over the
	 * beginning line when doing this search. */
	bol_or_eol
#else
	FALSE
#endif
	) != 0) {

	int i = 0;
	size_t match_len;

#ifdef HAVE_REGEX_H
	/* If the bol_or_eol flag is set, we've found a match on the
	 * beginning line already, and we're still on the beginning line
	 * after the search, it means that we've wrapped around, so
	 * we're done. */
	if (bol_or_eol && begin_line && current == real_current)
	    break;
	/* Otherwise, set the begin_line flag if we've found a match on
	 * the beginning line, reset the bol_or_eol flag, and
	 * continue. */
	else {
	    if (current == real_current)
		begin_line = TRUE;
	    bol_or_eol = FALSE;
	}
#endif

	if (!replaceall)
	    edit_redraw(current_save, old_pww);

#ifdef HAVE_REGEX_H
	if (ISSET(USE_REGEXP))
	    match_len = regmatches[0].rm_eo - regmatches[0].rm_so;
	else
#endif
	    match_len = strlen(needle);

	/* Record for the return value that we found the search string. */
	if (numreplaced == -1)
	    numreplaced = 0;

	if (!replaceall) {
	    char *exp_word;
	    size_t xpt = xplustabs();

	    exp_word = display_string(current->data, xpt,
		strnlenpt(current->data, match_len + current_x) - xpt);

	    curs_set(0);
	    do_replace_highlight(TRUE, exp_word);

	    i = do_yesno(TRUE, _("Replace this instance?"));

	    do_replace_highlight(FALSE, exp_word);
	    free(exp_word);
	    curs_set(1);

	    if (i == -1)	/* We canceled the replace. */
		break;
	}

#ifdef HAVE_REGEX_H
	/* Set the bol_or_eol flag if we're doing a bol and/or eol regex
	 * replace ("^", "$", or "^$"). */
	if (ISSET(USE_REGEXP) && regexp_bol_or_eol(&search_regexp,
		needle))
	    bol_or_eol = TRUE;
#endif

	if (i > 0 || replaceall) {	/* Yes, replace it!!!! */
	    char *copy;
	    int length_change;

	    if (i == 2)
		replaceall = 1;

	    copy = replace_line(needle);

	    length_change = strlen(copy) - strlen(current->data);

#ifndef NANO_SMALL
	    if (current == mark_beginbuf && mark_beginx > current_x) {
		if (mark_beginx < current_x + match_len)
		    mark_beginx = current_x;
		else
		    mark_beginx += length_change;
	    }
#endif

	    assert(0 <= match_len + length_change);
	    if (current == real_current && current_x <= *real_current_x) {
		if (*real_current_x < current_x + match_len)
		    *real_current_x = current_x + match_len;
		*real_current_x += length_change;
	    }

	    /* Set the cursor at the last character of the replacement
	     * text, so searching will resume after the replacement
	     * text.  Note that current_x might be set to -1 here. */
#ifndef NANO_SMALL
	    if (!ISSET(REVERSE_SEARCH))
#endif
		current_x += match_len + length_change - 1;

	    /* Cleanup. */
	    totsize += length_change;
	    free(current->data);
	    current->data = copy;

	    if (!replaceall) {
#ifdef ENABLE_COLOR
		if (ISSET(COLOR_SYNTAX))
		    edit_refresh();
		else
#endif
		    update_line(current, current_x);
	    }

	    set_modified();
	    numreplaced++;
	}
    }

    /* If text has been added to the magicline, make a new magicline. */
    if (filebot->data[0] != '\0')
	new_magicline();

#ifndef NANO_SMALL
    if (mark_set)
	SET(MARK_ISSET);
#endif

    return numreplaced;
}

/* Replace a string. */
void do_replace(void)
{
    int i, numreplaced;
    filestruct *edittop_save, *begin;
    size_t beginx;

    if (ISSET(VIEW_MODE)) {
	print_view_warning();
	replace_abort();
	return;
    }

    i = search_init(1);
    if (i == -1) {		/* Cancel, Go to Line, blank search
				 * string, or regcomp() failed. */
	replace_abort();
	return;
    } else if (i == -2) {	/* No Replace. */
	do_search();
	return;
    } else if (i == 1)		/* Case Sensitive, Backwards, or Regexp
				 * search toggle. */
	do_replace();

    if (i != 0)
	return;

    /* If answer is not "", add answer to the search history list and
     * copy answer into last_search. */
    if (answer[0] != '\0') {
#ifndef NANO_SMALL
	update_history(&search_history, answer);
#endif
	last_search = mallocstrcpy(last_search, answer);
    }

#ifndef NANO_SMALL
    replace_history.current = (historytype *)&replace_history.next;
    last_replace = mallocstrcpy(last_replace, "");
#endif

    i = statusq(FALSE, replace_list_2, last_replace,
#ifndef NANO_SMALL
	&replace_history,
#endif
	_("Replace with"));

#ifndef NANO_SMALL
    /* Add this replace string to the replace history list.  i == 0
     * means that the string is not "". */
    if (i == 0)
	update_history(&replace_history, answer);
#endif

    if (i != 0 && i != -2) {
	if (i == -1) {		/* Cancel. */
	    if (last_replace[0] != '\0')
		answer = mallocstrcpy(answer, last_replace);
	    statusbar(_("Replace Cancelled"));
	}
	replace_abort();
	return;
    }

    last_replace = mallocstrcpy(last_replace, answer);

    /* Save where we are. */
    begin = current;
    beginx = current_x;
    edittop_save = edittop;
    search_last_line = FALSE;

    numreplaced = do_replace_loop(last_search, begin, &beginx, FALSE);

    /* Restore where we were. */
    current = begin;
    current_x = beginx;
    renumber_all();
    edit_update(edittop_save, TOP);

    if (numreplaced >= 0)
	statusbar(P_("Replaced %d occurrence", "Replaced %d occurrences",
		numreplaced), numreplaced);

    replace_abort();
}

void do_gotoline(int line, int save_pos)
{
    if (line <= 0) {		/* Ask for it */
	char *ans = mallocstrcpy(NULL, answer);
	int st = statusq(FALSE, goto_list, line != 0 ? ans : "",
#ifndef NANO_SMALL
		NULL,
#endif
		_("Enter line number"));

	free(ans);

	/* Cancel, or Enter with blank string. */
	if (st == -1 || st == -2)
	    statusbar(_("Aborted"));
	if (st != 0) {
	    display_main_list();
	    return;
	}

	line = atoi(answer);

	/* Bounds check. */
	if (line <= 0) {
	    statusbar(_("Come on, be reasonable"));
	    display_main_list();
	    return;
	}
    }

    for (current = fileage; current->next != NULL && line > 1; line--)
	current = current->next;

    current_x = 0;

    /* If save_pos is TRUE, don't change the cursor position when
     * updating the edit window. */
    edit_update(current, save_pos ? NONE : CENTER);

    placewewant = 0;
    display_main_list();
}

void do_gotoline_void(void)
{
    do_gotoline(0, FALSE);
}

#if defined(ENABLE_MULTIBUFFER) || !defined(DISABLE_SPELLER)
void do_gotopos(int line, int pos_x, int pos_y, int pos_placewewant)
{
    /* since do_gotoline() resets the x-coordinate but not the
       y-coordinate, set the coordinates up this way */
    current_y = pos_y;
    do_gotoline(line, TRUE);

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
void do_find_bracket(void)
{
    char ch_under_cursor, wanted_ch;
    const char *pos, *brackets = "([{<>}])";
    char regexp_pat[] = "[  ]";
    int old_pww = placewewant, current_x_save, flagsave, count = 1;
    filestruct *current_save;

    ch_under_cursor = current->data[current_x];

    pos = strchr(brackets, ch_under_cursor);
    if (ch_under_cursor == '\0' || pos == NULL) {
	statusbar(_("Not a bracket"));
	return;
    }

    assert(strlen(brackets) % 2 == 0);
    wanted_ch = brackets[(strlen(brackets) - 1) - (pos - brackets)];

    current_x_save = current_x;
    current_save = current;
    flagsave = flags;
    SET(USE_REGEXP);

    /* Apparent near redundancy with regexp_pat[] here is needed.
     * "[][]" works, "[[]]" doesn't. */

    if (pos < brackets + (strlen(brackets) / 2)) {	/* On a left bracket. */
	regexp_pat[1] = wanted_ch;
	regexp_pat[2] = ch_under_cursor;
	UNSET(REVERSE_SEARCH);
    } else {			/* On a right bracket. */
	regexp_pat[1] = ch_under_cursor;
	regexp_pat[2] = wanted_ch;
	SET(REVERSE_SEARCH);
    }

    regexp_init(regexp_pat);
    /* We constructed regexp_pat to be a valid expression. */
    assert(regexp_compiled);

    search_last_line = FALSE;
    while (TRUE) {
	if (findnextstr(FALSE, FALSE, current, current_x, regexp_pat, FALSE) != 0) {
	    /* Found identical bracket. */
	    if (current->data[current_x] == ch_under_cursor)
		count++;
	    /* Found complementary bracket. */
	    else if (--count == 0) {
		placewewant = xplustabs();
		edit_redraw(current_save, old_pww);
		break;
	    }
	} else {
	    /* Didn't find either a left or right bracket. */
	    statusbar(_("No matching bracket"));
	    current_x = current_x_save;
	    current = current_save;
	    update_line(current, current_x);
	    break;
	}
    }

    regexp_cleanup();
    flags = flagsave;
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
    for (; h->next != NULL; h = h->next)
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

    a = (historytype *)nmalloc(sizeof(historytype));
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

    if ((p = find_node(h->next, s)) != NULL) {
	if (p == h->next)		/* catch delete and re-insert of
					   same string in 1st node */
	    goto up_hs;
	remove_node(p);			/* delete identical older string */
	h->count--;
    }
    if (h->count == MAX_SEARCH_HISTORY) {	/* list 'full', delete oldest */
	remove_node(h->tail);
	h->count--;
    }
    insert_node((historytype *)h, s);
    h->count++;
    SET(HISTORY_CHANGED);
  up_hs:
    h->current = h->next;
}

/* return a pointer to either the next older history or NULL if no more */
char *get_history_older(historyheadtype *h)
{
    if (h->current->next != NULL) {	/* any older entries? */
	h->current = h->current->next;	/* yes */
	return h->current->data;	/* return it */
    }
    return NULL;			/* end of list */
}

char *get_history_newer(historyheadtype *h)
{
    if (h->current->prev != NULL) {
	h->current = h->current->prev;
	if (h->current->prev != NULL)
	    return h->current->data;
    }
    return NULL;
}

/* get a completion */
char *get_history_completion(historyheadtype *h, char *s)
{
    historytype *p;

    for (p = h->current->next; p->next != NULL; p = p->next) {
	if (strncmp(s, p->data, h->len) == 0 && strlen(p->data) != h->len) {
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

    for (p = h->next; (n = p->next); p = n)
	remove_node(p);
}

/* end of history support functions */
#endif /* !NANO_SMALL */
