/* $Id$ */
/**************************************************************************
 *   utils.c                                                              *
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
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


/* I can't believe I have to write this function */
char *strcasestr(char *haystack, char *needle)
{
    char *localneedle, *localhaystack, *found, *tmp, *tmp2;

    /* Make a copy of the search string and searcgh space */
    localneedle = nmalloc(strlen(needle) + 2);
    localhaystack = nmalloc(strlen(haystack) + 2);

    strcpy(localneedle, needle);
    strcpy(localhaystack, haystack);

    /* Make them lowercase */
    lowercase(localneedle);
    lowercase(localhaystack);

    /* Look for the lowercased substring in the lowercased search space -
       return NULL if we didn't find anything */
    if ((found = strstr(localhaystack, localneedle)) == NULL) {
	free(localneedle);
	free(localhaystack);
	return NULL;
    }
    /* Else return the pointer to the same place in the real search space */
    tmp2 = haystack;
    for (tmp = localhaystack; tmp != found; tmp++)
	tmp2++;

    free(localneedle);
    free(localhaystack);
    return tmp2;
}

char *strstrwrapper(char *haystack, char *needle)
{
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP)) {
	int result = regexec(&search_regexp, haystack, 10, regmatches, 0);
	if (!result)
	    return haystack + regmatches[0].rm_so;
	return 0;
    }
#endif
    if (ISSET(CASE_SENSITIVE))
	return strstr(haystack, needle);
    else
	return strcasestr(haystack, needle);
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

/* We're going to need this too */
void *ncalloc(size_t howmuch, size_t size)
{
    void *r;

    /* Panic save? */

    if (!(r = calloc(howmuch, size)))
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

/* Copy one malloced string to another pointer.

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

    dest = nmalloc(strlen(src) + 1);
    strcpy(dest, src);

    return dest;
}


/* Append a new magic-line to filebot */
void new_magicline(void)
{
    filebot->next = nmalloc(sizeof(filestruct));
    filebot->next->data = nmalloc(1);
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
