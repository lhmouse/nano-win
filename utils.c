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

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "nano.h"
#include "proto.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/* Lower case a string - must be null terminated */
void lowercase(char *src)
{
    long i = 0;

    while (src[i] != 0) {
	src[i] = (char) tolower(src[i]);
	i++;
    }
}

/* None of this is needed if we're using NANO_SMALL! */
#ifndef NANO_SMALL
char *revstrstr(char *haystack, char *needle, char *rev_start)
{
    char *p, *q, *r;

    for(p = rev_start ; p >= haystack ; --p) {
	for (r = p, q = needle ; (*q == *r) && (*q != '\0') ; r++, q++)
	    ;
	if (*q == '\0')
	    return p;
    }
    return 0;
}

char *revstrcasestr(char *haystack, char *needle, char *rev_start)
{
    char *p, *q, *r;

    for(p = rev_start ; p >= haystack ; --p) {
	for (r = p, q = needle ; (tolower(*q) == tolower(*r)) && (*q != '\0') ; r++, q++)
	    ;
	if (*q == '\0')
	    return p;
    }
    return 0;
}

/* This is now mutt's version (called mutt_stristr) because it doesn't
   use memory allocation to do a simple search (yuck). */
char *strcasestr(char *haystack, char *needle)
{
    const char *p, *q;

    if (!haystack)
	return NULL;
    if (!needle)  
	return (haystack);
    
    while (*(p = haystack)) {
	for (q = needle; *p && *q && tolower (*p) == tolower (*q); p++, q++)
	    ;
	if (!*q)
	    return (haystack);
	haystack++;
    }
    return NULL;
}
#endif /* NANO_SMALL */

char *strstrwrapper(char *haystack, char *needle, char *rev_start)
{

#ifdef HAVE_REGEX_H
    int  result;

    if (ISSET(USE_REGEXP)) {
	if (!ISSET(REVERSE_SEARCH)) {
	    result = regexec(&search_regexp, haystack, 10, regmatches, 0);
	    if (!result)
		return haystack + regmatches[0].rm_so;
#ifndef NANO_SMALL
	} else {
	    char *i, *j;

	    /* do quick check first */
	    if (!(regexec(&search_regexp, haystack, 10, regmatches, 0))) {
		/* there is a match */
		for(i = rev_start ; i >= haystack ; --i)
		    if (!(result = regexec(&search_regexp, i, 10, regmatches, 0))) {
			j = i + regmatches[0].rm_so;
			if (j <= rev_start)
			    return j;
		    }

	    }
#endif
	}
	return 0;
    }
#endif
#ifndef NANO_SMALL
    if (ISSET(CASE_SENSITIVE)) {
	if (ISSET(REVERSE_SEARCH))
	    return revstrstr(haystack, needle, rev_start);
        else
	    return strstr(haystack,needle);

    } else {
	if (ISSET(REVERSE_SEARCH))
	    return revstrcasestr(haystack, needle, rev_start);
	else
#endif
	    return strcasestr(haystack, needle);
#ifndef NANO_SMALL
    }
#endif
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
   the transition cost of moving to the apropriate function. */
char *charalloc(size_t howmuch)
{
    void *r;

    /* Panic save? */

    if (!(r = calloc(howmuch, sizeof (char))))
	die(_("nano: calloc: out of memory!"));

    return (char *) r;
}

void *nrealloc(void *ptr, size_t howmuch)
{
    void *r;

    if (!(r = realloc(ptr, howmuch)))
	die(_("nano: realloc: out of memory!"));

    return r;
}

/* Copy one malloc()ed string to another pointer.

   Should be used as dest = mallocstrcpy(dest, src);
*/
void *mallocstrcpy(char *dest, char *src)
{


    if (src == dest)
	return src;

    if (dest != NULL)
	free(dest);

    if (src == NULL) {
	dest = NULL;
	return(dest);
    }

    dest = charalloc(strlen(src) + 1);
    strcpy(dest, src);

    return dest;
}


/* Append a new magic-line to filebot */
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
    const char *retryPat;
    const char *retryText;
    int ch;
    int found;
    int len;

    retryPat = NULL;
    retryText = NULL;

    while (*text || *pattern) {
	ch = *pattern++;

	switch (ch) {
	case '*':
	    retryPat = pattern;
	    retryText = text;
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
		pattern = retryPat;
		text = ++retryText;
		break;
	    }

	    return FALSE;
	}

	if (pattern == NULL)
	    return FALSE;
    }

    return TRUE;
}
#endif
