/* $Id$ */
/**************************************************************************
 *   utils.c                                                              *
 *                                                                        *
 *   Copyright (C) 1999-2002 Chris Allegretta                             *
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "config.h"
#include "proto.h"
#include "nano.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

int is_cntrl_char(int c)
{
    if (iscntrl(c) || ((c & 127) != 127 && iscntrl(c & 127)))
	return 1;
    else
	return 0;
}

int num_of_digits(int n)
{
    int i = 1;

    if (n < 0)
	n = -n;

    while (n > 10) {
	n /= 10;
	i++;
    }

    return i;
}

/* Fix the memory allocation for a string. */
void align(char **strp)
{
    assert(strp != NULL);
    *strp = nrealloc(*strp, strlen(*strp) + 1);
}

/* Null a string at a certain index and align it. */
void null_at(char **data, size_t index)
{
    assert(data != NULL);
    *data = (char *)nrealloc(*data, sizeof(char) * (index + 1));
    (*data)[index] = '\0';
}

/* For non-null-terminated lines.  A line, by definition, shouldn't
 * normally have newlines in it, so encode its nulls as newlines. */
void unsunder(char *str, size_t true_len)
{
    assert(str != NULL);
    for(; true_len > 0; true_len--, str++)
	if (*str == '\0')
	    *str = '\n';
}

/* For non-null-terminated lines.  A line, by definition, shouldn't
 * normally have newlines in it, so decode its newlines into nulls. */
void sunder(char *str)
{
    assert(str != NULL);
    for(; *str != '\0'; str++)
	if (*str == '\n')
	    *str = '\0';
}

/* None of this is needed if we're using NANO_SMALL! */
#ifndef NANO_SMALL
const char *revstrstr(const char *haystack, const char *needle,
	const char *rev_start)
{
    for(; rev_start >= haystack ; rev_start--) {
	const char *r, *q;

	for (r = rev_start, q = needle ; *q == *r && *q != '\0'; r++, q++)
	    ;
	if (*q == '\0')
	    return rev_start;
    }
    return NULL;
}

const char *revstristr(const char *haystack, const char *needle,
	const char *rev_start)
{
    for (; rev_start >= haystack; rev_start--) {
	const char *r = rev_start, *q = needle;

	for (; (tolower(*q) == tolower(*r)) && (*q != '\0') ; r++, q++)
	    ;
	if (*q == '\0')
	    return rev_start;
    }
    return NULL;
}
#endif /* !NANO_SMALL */

/* This is now mutt's version (called mutt_stristr) because it doesn't
   use memory allocation to do a simple search (yuck). */
const char *stristr(const char *haystack, const char *needle)
{
    const char *p, *q;

    if (!haystack)
	return NULL;
    if (!needle)  
	return (haystack);
    
    while (*(p = haystack)) {
	for (q = needle; *p && *q && tolower(*p) == tolower(*q); p++, q++)
	    ;
	if (!*q)
	    return haystack;
	haystack++;
    }
    return NULL;
}

const char *strstrwrapper(const char *haystack, const char *needle,
	const char *rev_start, int line_pos)
{
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP)) {
	if (!ISSET(REVERSE_SEARCH)) {
	    if (!regexec(&search_regexp, haystack, 10, regmatches, (line_pos > 0) ? REG_NOTBOL : 0))
		return haystack + regmatches[0].rm_so;
	}
#ifndef NANO_SMALL
	else {
	    const char *i, *j;

	    /* do a quick search forward first */
	    if (!regexec(&search_regexp, haystack, 10, regmatches, 0)) {
		/* there's a match somewhere in the line - now search for it backwards, much slower */
		for (i = rev_start; i >= haystack; --i) {
		    if (!regexec(&search_regexp, i, 10, regmatches, (i > haystack) ? REG_NOTBOL : 0)) {
			j = i + regmatches[0].rm_so;
			if (j <= rev_start)
			    return j;
		    }
		}
	    }
	}
#endif
	return 0;
    }
#endif
#ifndef NANO_SMALL
    if (ISSET(CASE_SENSITIVE)) {
	if (ISSET(REVERSE_SEARCH))
	    return revstrstr(haystack, needle, rev_start);
        else
	    return strstr(haystack, needle);
    } else {
	if (ISSET(REVERSE_SEARCH))
	    return revstristr(haystack, needle, rev_start);
	else
#endif
	    return stristr(haystack, needle);
#ifndef NANO_SMALL
    }
#endif
}

/* This is a wrapper for the perror function.  The wrapper takes care of 
 * ncurses, calls perror (which writes to STDERR), then refreshes the 
 * screen.  Note that nperror causes the window to flicker once. */
void nperror(const char *s) {
	/* leave ncurses mode, go to the terminal */
    if (endwin() != ERR) {
	perror(s);		/* print the error */
	total_refresh();	/* return to ncurses and repaint */
    }
}

/* Thanks BG, many ppl have been asking for this... */
void *nmalloc(size_t howmuch)
{
    void *r;

    /* Panic save? */

    if (!(r = malloc(howmuch)))
	die(_("nano: malloc: out of memory!"));

    return r;
}

/* We're going to need this too - Hopefully this will minimize
   the transition cost of moving to the appropriate function. */
char *charalloc(size_t howmuch)
{
    char *r;

    /* Panic save? */

    if (!(r = (char *)calloc(howmuch, sizeof (char))))
	die(_("nano: calloc: out of memory!"));

    return r;
}

void *nrealloc(void *ptr, size_t howmuch)
{
    void *r;

    if (!(r = realloc(ptr, howmuch)))
	die(_("nano: realloc: out of memory!"));

    return r;
}

/* Copy one malloc()ed string to another pointer.  Should be used as:
 * dest = mallocstrcpy(dest, src); */
char *mallocstrcpy(char *dest, const char *src)
{
    if (src == dest)
	return dest;

    if (dest)
	free(dest);

    if (!src)
	return NULL;

    dest = charalloc(strlen(src) + 1);
    strcpy(dest, src);

    return dest;
}

/* Append a new magic-line to filebot. */
void new_magicline(void)
{
    filebot->next = nmalloc(sizeof(filestruct));
    filebot->next->data = charalloc(1);
    filebot->next->data[0] = '\0';
    filebot->next->prev = filebot;
    filebot->next->next = NULL;
    filebot->next->lineno = filebot->lineno + 1;
    filebot = filebot->next;
    totlines++;
    totsize++;
}

#ifndef DISABLE_TABCOMP
/*
 * Routine to see if a text string is matched by a wildcard pattern.
 * Returns TRUE if the text is matched, or FALSE if it is not matched
 * or if the pattern is invalid.
 *  *		matches zero or more characters
 *  ?		matches a single character
 *  [abc]	matches 'a', 'b' or 'c'
 *  \c		quotes character c
 * Adapted from code written by Ingo Wilken, and
 * then taken from sash, Copyright (c) 1999 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 * Permission to distribute this code under the GPL has been granted.
 */
int check_wildcard_match(const char *text, const char *pattern)
{
    const char *retrypat;
    const char *retrytext;
    int ch;
    int found;
    int len;

    retrypat = NULL;
    retrytext = NULL;

    while (*text || *pattern) {
	ch = *pattern++;

	switch (ch) {
	case '*':
	    retrypat = pattern;
	    retrytext = text;
	    break;

	case '[':
	    found = FALSE;

	    while ((ch = *pattern++) != ']') {
		if (ch == '\\')
		    ch = *pattern++;

		if (ch == '\0')
		    return FALSE;

		if (*text == ch)
		    found = TRUE;
	    }
	    len = strlen(text);
	    if (found == FALSE && len != 0) {
		return FALSE;
	    }
	    if (found == TRUE) {
		if (strlen(pattern) == 0 && len == 1) {
		    return TRUE;
		}
		if (len != 0) {
		    text++;
		    continue;
		}
	    }

	    /* fall into next case */

	case '?':
	    if (*text++ == '\0')
		return FALSE;

	    break;

	case '\\':
	    ch = *pattern++;

	    if (ch == '\0')
		return FALSE;

	    /* fall into next case */

	default:
	    if (*text == ch) {
		if (*text)
		    text++;
		break;
	    }

	    if (*text) {
		pattern = retrypat;
		text = ++retrytext;
		break;
	    }

	    return FALSE;
	}

	if (!pattern)
	    return FALSE;
    }

    return TRUE;
}
#endif
