/* $Id$ */
/**************************************************************************
 *   search.c                                                             *
 *                                                                        *
 *   Copyright (C) 2000-2005 Chris Allegretta                             *
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
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "proto.h"

static bool search_last_line = FALSE;
	/* Have we gone past the last line while searching? */
#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
static bool history_changed = FALSE;
	/* Have any of the history lists changed? */
#endif
#ifdef HAVE_REGEX_H
static bool regexp_compiled = FALSE;
	/* Have we compiled any regular expressions? */

/* Regular expression helper functions. */

/* Compile the given regular expression.  Return value 0 means the
 * expression was invalid, and we wrote an error message on the status
 * bar.  Return value 1 means success. */
int regexp_init(const char *regexp)
{
    int rc = regcomp(&search_regexp, regexp, REG_EXTENDED
#ifndef NANO_SMALL
	| (ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE)
#endif
	);

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

    disp = display_string(str, 0, (COLS / 2) + 1, FALSE);
    numchars = actual_x(disp, mbstrnlen(disp, COLS / 2));

    statusbar(_("\"%.*s%s\" not found"), numchars, disp,
	(disp[numchars] == '\0') ? "" : "...");

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
    if (last_search == NULL)
	last_search = mallocstrcpy(NULL, "");
    if (last_replace == NULL)
	last_replace = mallocstrcpy(NULL, "");
}

/* Set up the system variables for a search or replace.  If use_answer
 * is TRUE, only set backupstring to answer.  Return -2 to run opposite
 * program (search -> replace, replace -> search), return -1 if the
 * search should be canceled (due to Cancel, Go to Line, or a failed
 * regcomp()), return 0 on success, and return 1 on rerun calling
 * program.
 *
 * replacing is TRUE if we call from do_replace(), and FALSE if called
 * from do_search(). */
int search_init(bool replacing, bool use_answer)
{
    int i = 0;
    char *buf;
    static char *backupstring = NULL;
	/* The search string we'll be using. */

    /* If backupstring doesn't exist, initialize it to "". */
    if (backupstring == NULL)
	backupstring = mallocstrcpy(NULL, "");

    /* If use_answer is TRUE, set backupstring to answer and get out. */
    if (use_answer) {
	backupstring = mallocstrcpy(backupstring, answer);
	return 0;
    }

    /* We display the search prompt below.  If the user types a partial
     * search string and then Replace or a toggle, we will return to
     * do_search() or do_replace() and be called again.  In that case,
     * we should put the same search string back up. */

    search_init_globals();

    if (last_search[0] != '\0') {
	char *disp = display_string(last_search, 0, COLS / 3, FALSE);

	buf = charalloc(strlen(disp) + 7);
	/* We use COLS / 3 here because we need to see more on the
	 * line. */
	sprintf(buf, " [%s%s]", disp,
		strlenpt(last_search) > COLS / 3 ? "..." : "");
	free(disp);
    } else
	buf = mallocstrcpy(NULL, "");

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

	replacing ?
#ifndef NANO_SMALL
		(ISSET(MARK_ISSET) ? _(" (to replace) in selection") :
#endif
		_(" (to replace)")
#ifndef NANO_SMALL
		)
#endif
		: "",

	buf);

    /* Release buf now that we don't need it anymore. */
    free(buf);

    free(backupstring);
    backupstring = NULL;

    /* Cancel any search, or just return with no previous search. */
    if (i == -1 || (i < 0 && last_search[0] == '\0') ||
	    (!replacing && i == 0 && answer[0] == '\0')) {
	statusbar(_("Cancelled"));
	return -1;
    } else {
	switch (i) {
	    case -2:		/* It's the same string. */
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
#endif
#ifdef HAVE_REGEX_H
	    case NANO_REGEXP_KEY:
		TOGGLE(USE_REGEXP);
		backupstring = mallocstrcpy(backupstring, answer);
		return 1;
#endif
	    case NANO_TOOTHERSEARCH_KEY:
		backupstring = mallocstrcpy(backupstring, answer);
		return -2;	/* Call the opposite search function. */
	    case NANO_TOGOTOLINE_KEY:
		do_gotolinecolumn(current->lineno, placewewant + 1,
			TRUE, TRUE, FALSE);
				/* Put answer up on the statusbar and
				 * fall through. */
	    default:
		return -1;
	}
    }

    return 0;
}

bool is_whole_word(size_t pos, const char *buf, const char *word)
{
    char *p = charalloc(mb_cur_max()), *r = charalloc(mb_cur_max());
    size_t word_end = pos + strlen(word);
    bool retval;

    assert(buf != NULL && pos <= strlen(buf) && word != NULL);

    parse_mbchar(buf + move_mbleft(buf, pos), p, NULL, NULL);
    parse_mbchar(buf + word_end, r, NULL, NULL);

    /* If we're at the beginning of the line or the character before the
     * word isn't a non-punctuation "word" character, and if we're at
     * the end of the line or the character after the word isn't a
     * non-punctuation "word" character, we have a whole word. */
    retval = (pos == 0 || !is_word_mbchar(p, FALSE)) &&
	(word_end == strlen(buf) || !is_word_mbchar(r, FALSE));

    free(p);
    free(r);

    return retval;
}

/* Look for needle, starting at (current, current_x).  If no_sameline is
 * TRUE, skip over begin when looking for needle.  begin is the line
 * where we first started searching, at column beginx.  If
 * can_display_wrap is TRUE, we put messages on the statusbar, wrap
 * around the file boundaries.  The return value specifies whether we
 * found anything.  If we did, set needle_len to the length of the
 * string we found if it isn't NULL. */
bool findnextstr(bool can_display_wrap, bool wholeword, bool
	no_sameline, const filestruct *begin, size_t beginx, const char
	*needle, size_t *needle_len)
{
    filestruct *fileptr = current;
    const char *rev_start = NULL, *found = NULL;
    size_t found_len;
	/* The length of the match we found. */
    size_t current_x_find = 0;
	/* The location of the match we found. */
    int current_y_find = current_y;

    /* rev_start might end up 1 character before the start or after the
     * end of the line.  This won't be a problem because strstrwrapper()
     * will return immediately and say that no match was found, and
     * rev_start will be properly set when the search continues on the
     * previous or next line. */
    rev_start =
#ifndef NANO_SMALL
	ISSET(REVERSE_SEARCH) ? fileptr->data + (current_x - 1) :
#endif
	fileptr->data + (current_x + 1);

    /* Look for needle in searchstr. */
    while (TRUE) {
	found = strstrwrapper(fileptr->data, needle, rev_start);

	/* We've found a potential match. */
	if (found != NULL) {
	    bool found_whole = FALSE;
		/* Is this potential match a whole word? */

	    /* Set found_len to the length of the potential match. */
	    found_len =
#ifdef HAVE_REGEX_H
		ISSET(USE_REGEXP) ?
		regmatches[0].rm_eo - regmatches[0].rm_so :
#endif
		strlen(needle);

	    /* If we're searching for whole words, see if this potential
	     * match is a whole word. */
	    if (wholeword) {
		char *word = mallocstrncpy(NULL, found, found_len + 1);
		word[found_len] = '\0';

		found_whole = is_whole_word(found - fileptr->data,
			fileptr->data, word);
		free(word);
	    }

	    /* If we're searching for whole words and this potential
	     * match isn't a whole word, or if we're not allowed to find
	     * a match on the same line we started on and this potential
	     * match is on that line, continue searching. */
	    if ((!wholeword || found_whole) && (!no_sameline ||
		fileptr != current))
		break;
	}

	/* We've finished processing the file, so get out. */
	if (search_last_line) {
	    if (can_display_wrap)
		not_found_msg(needle);
	    return FALSE;
	}

#ifndef NANO_SMALL
	if (ISSET(REVERSE_SEARCH)) {
	    fileptr = fileptr->prev;
	    current_y_find--;
	} else {
#endif
	    fileptr = fileptr->next;
	    current_y_find++;
#ifndef NANO_SMALL
	}
#endif

	/* Start or end of buffer reached, so wrap around. */
	if (fileptr == NULL) {
	    if (!can_display_wrap)
		return FALSE;

#ifndef NANO_SMALL
	    if (ISSET(REVERSE_SEARCH)) {
		fileptr = filebot;
		current_y_find = editwinrows - 1;
	    } else {
#endif
		fileptr = fileage;
		current_y_find = 0;
#ifndef NANO_SMALL
	    }
#endif

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
	return FALSE;
    }

    /* Set globals now that we are sure we found something. */
    current = fileptr;
    current_x = current_x_find;
    current_y = current_y_find;
    placewewant = xplustabs();

    /* needle_len holds the length of needle. */
    if (needle_len != NULL)
	*needle_len = found_len;

    return TRUE;
}

void findnextstr_wrap_reset(void)
{
    search_last_line = FALSE;
}

/* Search for a string. */
void do_search(void)
{
    size_t old_pww = placewewant, fileptr_x = current_x;
    int i;
    bool didfind;
    filestruct *fileptr = current;

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    i = search_init(FALSE, FALSE);
    if (i == -1)	/* Cancel, Go to Line, blank search string, or
			 * regcomp() failed. */
	search_abort();
    else if (i == -2)	/* Replace. */
	do_replace();
#if !defined(NANO_SMALL) || defined(HAVE_REGEX_H)
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

    findnextstr_wrap_reset();
    didfind = findnextstr(TRUE, FALSE, FALSE, current, current_x,
	answer, NULL);

    /* Check to see if there's only one occurrence of the string and
     * we're on it now. */
    if (fileptr == current && fileptr_x == current_x && didfind) {
#ifdef HAVE_REGEX_H
	/* Do the search again, skipping over the current line, if we're
	 * doing a bol and/or eol regex search ("^", "$", or "^$"), so
	 * that we find one only once per line.  We should only end up
	 * back at the same position if the string isn't found again, in
	 * which case it's the only occurrence. */
	if (ISSET(USE_REGEXP) && regexp_bol_or_eol(&search_regexp,
		last_search)) {
	    didfind = findnextstr(TRUE, FALSE, TRUE, current, current_x,
		answer, NULL);
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
    size_t old_pww = placewewant, fileptr_x = current_x;
    bool didfind;
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

	findnextstr_wrap_reset();
	didfind = findnextstr(TRUE, FALSE, FALSE, current, current_x,
		last_search, NULL);

	/* Check to see if there's only one occurrence of the string and
	 * we're on it now. */
	if (fileptr == current && fileptr_x == current_x && didfind) {
#ifdef HAVE_REGEX_H
	    /* Do the search again, skipping over the current line, if
	     * we're doing a bol and/or eol regex search ("^", "$", or
	     * "^$"), so that we find one only once per line.  We should
	     * only end up back at the same position if the string isn't
	     * found again, in which case it's the only occurrence. */
	    if (ISSET(USE_REGEXP) && regexp_bol_or_eol(&search_regexp,
		last_search)) {
		didfind = findnextstr(TRUE, FALSE, TRUE, current,
			current_x, answer, NULL);
		if (fileptr == current && fileptr_x == current_x &&
			!didfind)
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
int replace_regexp(char *string, bool create)
{
    /* We have a split personality here.  If create is FALSE, just
     * calculate the size of the replacement line (necessary because of
     * subexpressions \1 to \9 in the replaced text). */

    const char *c = last_replace;
    int search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;
    int new_size = strlen(current->data) + 1 - search_match_count;

    /* Iterate through the replacement text to handle subexpression
     * replacement using \1, \2, \3, etc. */
    while (*c != '\0') {
	int num = (int)(*(c + 1) - '0');

	if (*c != '\\' || num < 1 || num > 9 ||
		num > search_regexp.re_nsub) {
	    if (create)
		*string++ = *c;
	    c++;
	    new_size++;
	} else {
	    int i = regmatches[num].rm_eo - regmatches[num].rm_so;

	    /* Skip over the replacement expression. */
	    c += 2;

	    /* But add the length of the subexpression to new_size. */
	    new_size += i;

	    /* And if create is TRUE, append the result of the
	     * subexpression match to the new line. */
	    if (create) {
		charcpy(string, current->data + current_x +
			regmatches[num].rm_so, i);
		string += i;
	    }
	}
    }

    if (create)
	*string = '\0';

    return new_size;
}
#endif

char *replace_line(const char *needle)
{
    char *copy;
    size_t new_line_size, search_match_count;

    /* Calculate the size of the new line. */
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP)) {
	search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;
	new_line_size = replace_regexp(NULL, FALSE);
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
    charcpy(copy, current->data, current_x);

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
 * Parameters real_current and real_current_x are needed in order to
 * allow the cursor position to be updated when a word before the cursor
 * is replaced by a shorter word.
 *
 * needle is the string to seek.  We replace it with answer.  Return -1
 * if needle isn't found, else the number of replacements performed.  If
 * canceled isn't NULL, set it to TRUE if we canceled. */
ssize_t do_replace_loop(const char *needle, const filestruct
	*real_current, size_t *real_current_x, bool wholewords, bool
	*canceled)
{
    ssize_t numreplaced = -1;
    size_t match_len;
    bool replaceall = FALSE;
#ifdef HAVE_REGEX_H
    /* The starting-line match and bol/eol regex flags. */
    bool begin_line = FALSE, bol_or_eol = FALSE;
#endif
#ifndef NANO_SMALL
    bool old_mark_set = ISSET(MARK_ISSET);
    filestruct *edittop_save = edittop, *top, *bot;
    size_t top_x, bot_x;
    bool right_side_up = FALSE;
	/* TRUE if (mark_beginbuf, mark_beginx) is the top of the mark,
	 * FALSE if (current, current_x) is. */

    if (old_mark_set) {
	/* If the mark is on, partition the filestruct so that it
	 * contains only the marked text, set edittop to the top of the
	 * partition, turn the mark off, and refresh the screen. */
	mark_order((const filestruct **)&top, &top_x,
	    (const filestruct **)&bot, &bot_x, &right_side_up);
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	edittop = fileage;
	UNSET(MARK_ISSET);
	edit_refresh();
    }
#endif

    if (canceled != NULL)
	*canceled = FALSE;

    findnextstr_wrap_reset();
    while (findnextstr(TRUE, wholewords,
#ifdef HAVE_REGEX_H
	/* We should find a bol and/or eol regex only once per line.  If
	 * the bol_or_eol flag is set, it means that the last search
	 * found one on the beginning line, so we should skip over the
	 * beginning line when doing this search. */
	bol_or_eol
#else
	FALSE
#endif
	, real_current, *real_current_x, needle, &match_len)) {
	int i = 0;

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
	    edit_refresh();

	/* Indicate that we found the search string. */
	if (numreplaced == -1)
	    numreplaced = 0;

	if (!replaceall) {
	    size_t xpt = xplustabs();
	    char *exp_word = display_string(current->data, xpt,
		strnlenpt(current->data, current_x + match_len) - xpt,
		FALSE);

	    curs_set(0);

	    do_replace_highlight(TRUE, exp_word);

	    i = do_yesno(TRUE, _("Replace this instance?"));

	    do_replace_highlight(FALSE, exp_word);

	    free(exp_word);

	    curs_set(1);

	    if (i == -1) {	/* We canceled the replace. */
		if (canceled != NULL)
		    *canceled = TRUE;
		break;
	    }
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
	    size_t length_change;

	    if (i == 2)
		replaceall = TRUE;

	    copy = replace_line(needle);

	    length_change = strlen(copy) - strlen(current->data);

#ifndef NANO_SMALL
	    /* If the mark was on and (mark_beginbuf, mark_begin_x) was
	     * the top of it, don't change mark_beginx. */
	    if (!old_mark_set || !right_side_up) {
		/* Keep mark_beginx in sync with the text changes. */
		if (current == mark_beginbuf &&
			mark_beginx > current_x) {
		    if (mark_beginx < current_x + match_len)
			mark_beginx = current_x;
		    else
			mark_beginx += length_change;
		}
	    }

	    /* If the mark was on and (current, current_x) was the top
	     * of it, don't change real_current_x. */
	    if (!old_mark_set || right_side_up) {
#endif
		/* Keep real_current_x in sync with the text changes. */
		if (current == real_current &&
			current_x <= *real_current_x) {
		    if (*real_current_x < current_x + match_len)
			*real_current_x = current_x + match_len;
		    *real_current_x += length_change;
		}
#ifndef NANO_SMALL
	    }
#endif

	    /* Set the cursor at the last character of the replacement
	     * text, so searching will resume after the replacement
	     * text.  Note that current_x might be set to (size_t)-1
	     * here. */
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
		if (!ISSET(NO_COLOR_SYNTAX))
		    edit_refresh();
		else
#endif
		    update_line(current, current_x);
	    }

	    set_modified();
	    numreplaced++;
	}
    }

#ifndef NANO_SMALL
    if (old_mark_set) {
	/* If the mark was on, unpartition the filestruct so that it
	 * contains all the text again, set edittop back to what it was
	 * before, turn the mark back on, and refresh the screen. */
	unpartition_filestruct(&filepart);
	edittop = edittop_save;
	SET(MARK_ISSET);
	edit_refresh();
    }
#endif

    /* If text has been added to the magicline, make a new magicline. */
    if (filebot->data[0] != '\0')
	new_magicline();

    return numreplaced;
}

/* Replace a string. */
void do_replace(void)
{
    int i;
    filestruct *edittop_save, *begin;
    size_t beginx, pww_save;
    ssize_t numreplaced;

    if (ISSET(VIEW_MODE)) {
	print_view_warning();
	replace_abort();
	return;
    }

    i = search_init(TRUE, FALSE);
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
	    statusbar(_("Cancelled"));
	}
	replace_abort();
	return;
    }

    last_replace = mallocstrcpy(last_replace, answer);

    /* Save where we are. */
    edittop_save = edittop;
    begin = current;
    beginx = current_x;
    pww_save = placewewant;

    numreplaced = do_replace_loop(last_search, begin, &beginx, FALSE,
	NULL);

    /* Restore where we were. */
    edittop = edittop_save;
    current = begin;
    current_x = beginx;
    placewewant = pww_save;

    renumber_all();
    edit_refresh();

    if (numreplaced >= 0)
	statusbar(P_("Replaced %lu occurrence",
		"Replaced %lu occurrences", (unsigned long)numreplaced),
		(unsigned long)numreplaced);

    replace_abort();
}

/* Go to the specified line and column, or ask for them if interactive
 * is TRUE.  Save the x-coordinate and y-coordinate if save_pos is TRUE.
 * Note that both the line and column numbers should be one-based. */
void do_gotolinecolumn(int line, ssize_t column, bool use_answer, bool
	interactive, bool save_pos)
{
    if (interactive) {		/* Ask for it. */
	char *ans = mallocstrcpy(NULL, answer);
	int i = statusq(FALSE, gotoline_list, use_answer ? ans : "",
#ifndef NANO_SMALL
		NULL,
#endif
		_("Enter line number, column number"));

	free(ans);

	/* Cancel, or Enter with blank string. */
	if (i < 0) {
	    statusbar(_("Cancelled"));
	    display_main_list();
	    return;
	}

	if (i == NANO_TOOTHERWHEREIS_KEY) {
	    /* Keep answer up on the statusbar. */
	    search_init(TRUE, TRUE);

	    do_search();
	    return;
	}

	/* Do a bounds check.  Display a warning on an out-of-bounds
	 * line or column number only if we hit Enter at the statusbar
	 * prompt. */
	if (!parse_line_column(answer, &line, &column) || line < 1 ||
		column < 1) {
	    if (i == 0)
		statusbar(_("Come on, be reasonable"));
	    display_main_list();
	    return;
	}
    } else {
	if (line < 1)
	    line = current->lineno;

	if (column < 1)
	    column = placewewant + 1;
    }

    for (current = fileage; current->next != NULL && line > 1; line--)
	current = current->next;

    current_x = actual_x(current->data, column - 1);
    placewewant = column - 1;

    /* If save_pos is TRUE, don't change the cursor position when
     * updating the edit window. */
    edit_update(save_pos ? NONE : CENTER);

    display_main_list();
}

void do_gotolinecolumn_void(void)
{
    do_gotolinecolumn(current->lineno, placewewant + 1, FALSE, TRUE,
	FALSE);
}

#if defined(ENABLE_MULTIBUFFER) || !defined(DISABLE_SPELLER)
void do_gotopos(int line, size_t pos_x, int pos_y, size_t pos_pww)
{
    /* Since do_gotolinecolumn() resets the x-coordinate but not the
     * y-coordinate, set the coordinates up this way. */
    current_y = pos_y;
    do_gotolinecolumn(line, pos_x + 1, FALSE, FALSE, TRUE);

    /* Set the rest of the coordinates up. */
    placewewant = pos_pww;
    update_line(current, pos_x);
}
#endif

#if !defined(NANO_SMALL) && defined(HAVE_REGEX_H)
void do_find_bracket(void)
{
    char ch_under_cursor, wanted_ch;
    const char *pos, *brackets = "([{<>}])";
    char regexp_pat[] = "[  ]";
    size_t current_x_save, pww_save;
    int count = 1;
    unsigned long flags_save;
    filestruct *current_save;

    ch_under_cursor = current->data[current_x];

    pos = strchr(brackets, ch_under_cursor);
    if (ch_under_cursor == '\0' || pos == NULL) {
	statusbar(_("Not a bracket"));
	return;
    }

    assert(strlen(brackets) % 2 == 0);

    wanted_ch = brackets[(strlen(brackets) - 1) - (pos - brackets)];

    current_save = current;
    current_x_save = current_x;
    pww_save = placewewant;
    flags_save = flags;
    SET(USE_REGEXP);

    /* Apparent near redundancy with regexp_pat[] here is needed.
     * "[][]" works, "[[]]" doesn't. */
    if (pos < brackets + (strlen(brackets) / 2)) {
	/* On a left bracket. */
	regexp_pat[1] = wanted_ch;
	regexp_pat[2] = ch_under_cursor;
	UNSET(REVERSE_SEARCH);
    } else {
	/* On a right bracket. */
	regexp_pat[1] = ch_under_cursor;
	regexp_pat[2] = wanted_ch;
	SET(REVERSE_SEARCH);
    }

    regexp_init(regexp_pat);

    /* We constructed regexp_pat to be a valid expression. */
    assert(regexp_compiled);

    findnextstr_wrap_reset();
    while (TRUE) {
	if (findnextstr(FALSE, FALSE, FALSE, current, current_x,
		regexp_pat, NULL)) {
	    /* Found identical bracket. */
	    if (current->data[current_x] == ch_under_cursor)
		count++;
	    /* Found complementary bracket. */
	    else if (--count == 0) {
		placewewant = xplustabs();
		edit_redraw(current_save, pww_save);
		break;
	    }
	} else {
	    /* Didn't find either a left or right bracket. */
	    statusbar(_("No matching bracket"));
	    current = current_save;
	    current_x = current_x_save;
	    update_line(current, current_x);
	    break;
	}
    }

    regexp_cleanup();
    flags = flags_save;
}
#endif

#ifndef NANO_SMALL
#ifdef ENABLE_NANORC
/* Indicate whether any of the history lists have changed. */
bool history_has_changed(void)
{
    return history_changed;
}
#endif

/* Initialize the search and replace history lists. */
void history_init(void)
{
    search_history = make_new_node(NULL);
    search_history->data = mallocstrcpy(NULL, "");
    searchage = search_history;
    searchbot = search_history;

    replace_history = make_new_node(NULL);
    replace_history->data = mallocstrcpy(NULL, "");
    replaceage = replace_history;
    replacebot = replace_history;
}

/* Return the first node containing the first len characters of the
 * string s in the history list, starting at h_start and ending at
 * h_end, or NULL if there isn't one. */
filestruct *find_history(filestruct *h_start, filestruct *h_end, const
	char *s, size_t len)
{
    filestruct *p;

    for (p = h_start; p != h_end->next && p != NULL; p = p->next) {
	if (strncmp(s, p->data, len) == 0)
	    return p;
    }

    return NULL;
}

/* Update a history list.  h should be the current position in the
 * list. */
void update_history(filestruct **h, const char *s)
{
    filestruct **hage = NULL, **hbot = NULL, *p;

    assert(h != NULL && s != NULL);

    if (*h == search_history) {
	hage = &searchage;
	hbot = &searchbot;
    } else if (*h == replace_history) {
	hage = &replaceage;
	hbot = &replacebot;
    }

    assert(hage != NULL && hbot != NULL);

    /* If this string is already in the history, delete it. */
    p = find_history(*hage, *hbot, s, (size_t)-1);

    if (p != NULL) {
	filestruct *foo, *bar;

	/* If the string is at the beginning, move the beginning down to
	 * the next string. */
	if (p == *hage)
	    *hage = (*hage)->next;

	/* Delete the string. */
	foo = p;
	bar = p->next;
	unlink_node(foo);
	delete_node(foo);
	renumber(bar);
    }

    /* If the history is full, delete the beginning entry to make room
     * for the new entry at the end. */
    if ((*hbot)->lineno == MAX_SEARCH_HISTORY + 1) {
	filestruct *foo = *hage;

	*hage = (*hage)->next;
	unlink_node(foo);
	delete_node(foo);
	renumber(*hage);
    }

    /* Add the new entry to the end. */
    (*hbot)->data = mallocstrcpy(NULL, s);
    splice_node(*hbot, make_new_node(*hbot), (*hbot)->next);
    *hbot = (*hbot)->next;
    (*hbot)->data = mallocstrcpy(NULL, "");

#ifdef ENABLE_NANORC
    /* Indicate that the history's been changed. */
    history_changed = TRUE;
#endif

    /* Set the current position in the list to the bottom. */
    *h = *hbot;
}

/* Move h to the string in the history list just before it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_older(filestruct **h)
{
    assert(h != NULL);

    if ((*h)->prev == NULL)
	return NULL;

    *h = (*h)->prev;

    return (*h)->data;
}

/* Move h to the string in the history list just after it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_newer(filestruct **h)
{
    assert(h != NULL);

    if ((*h)->next == NULL)
	return NULL;

    *h = (*h)->next;

    return (*h)->data;
}

#ifndef DISABLE_TABCOMP
/* Move h to the next string that's a tab completion of the string s,
 * looking at only the first len characters of s, and return that
 * string.  If there isn't one, or if len is 0, don't move h and return
 * s. */
char *get_history_completion(filestruct **h, char *s, size_t len)
{
    assert(s != NULL);

    if (len > 0) {
	filestruct *hage = NULL, *hbot = NULL, *p;

	assert(h != NULL);

	if (*h == search_history) {
	    hage = searchage;
	    hbot = searchbot;
	} else if (*h == replace_history) {
	    hage = replaceage;
	    hbot = replacebot;
	}

	assert(hage != NULL && hbot != NULL);

	/* Search the history list from the current position to the
	 * bottom for a match of len characters.  Skip over an exact
	 * match. */
	p = find_history((*h)->next, hbot, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
	    p = find_history(p->next, hbot, s, len);

	if (p != NULL) {
	    *h = p;
	    return (*h)->data;
	}

	/* Search the history list from the top to the current position
	 * for a match of len characters.  Skip over an exact match. */
	p = find_history(hage, *h, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
	    p = find_history(p->next, *h, s, len);

	if (p != NULL) {
	    *h = p;
	    return (*h)->data;
	}
    }

    /* If we're here, we didn't find a match, we didn't find an inexact
     * match, or len is 0.  Return s. */
    return s;
}
#endif
#endif /* !NANO_SMALL && ENABLE_NANORC */
