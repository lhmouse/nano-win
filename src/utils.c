/* $Id$ */
/**************************************************************************
 *   utils.c                                                              *
 *                                                                        *
 *   Copyright (C) 1999-2004 Chris Allegretta                             *
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
#include "nano.h"

#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#ifdef HAVE_REGEX_H
#ifdef BROKEN_REGEXEC
#undef regexec
int regexec_safe(const regex_t *preg, const char *string, size_t nmatch,
	regmatch_t pmatch[], int eflags)
{
    if (string != NULL && *string != '\0')
	return regexec(preg, string, nmatch, pmatch, eflags);
    return REG_NOMATCH;
}
#define regexec(preg, string, nmatch, pmatch, eflags) regexec_safe(preg, string, nmatch, pmatch, eflags)
#endif /* BROKEN_REGEXEC */

int regexp_bol_or_eol(const regex_t *preg, const char *string)
{
    return (regexec(preg, string, 0, NULL, 0) == 0 &&
	regexec(preg, string, 0, NULL, REG_NOTBOL | REG_NOTEOL) ==
	REG_NOMATCH);
}
#endif /* HAVE_REGEX_H */

#ifndef HAVE_ISBLANK
/* This function is equivalent to isblank(). */
int is_blank_char(int c)
{
    return (c == '\t' || c == ' ');
}
#endif

/* This function is equivalent to iscntrl(), except in that it also
 * handles control characters with their high bits set. */
int is_cntrl_char(int c)
{
    return (-128 <= c && c < -96) || (0 <= c && c < 32) ||
	(127 <= c && c < 160);
}

/* Return TRUE if the character c is in byte range, and FALSE
 * otherwise. */
bool is_byte_char(int c)
{
    return (unsigned int)c == (unsigned char)c;
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

/* c is a control character.  It displays as ^@, ^?, or ^[ch] where ch
 * is c + 64.  We return that character. */
unsigned char control_rep(unsigned char c)
{
    /* Treat newlines embedded in a line as encoded nulls. */
    if (c == '\n')
	return '@';
    else if (c == NANO_CONTROL_8)
	return '?';
    else
	return c + 64;
}

/* Read a ssize_t from str, and store it in *val (if val is not NULL).
 * On error, we return FALSE and don't change *val.  Otherwise, we
 * return TRUE. */
bool parse_num(const char *str, ssize_t *val)
{
    char *first_error;
    ssize_t j;

    assert(str != NULL);

    j = (ssize_t)strtol(str, &first_error, 10);

    if (errno == ERANGE || *str == '\0' || *first_error != '\0')
	return FALSE;

    if (val != NULL)
	*val = j;

    return TRUE;
}

/* Parse a multi-byte character from str.  Return the number of bytes
 * used.  If chr isn't NULL, store the wide character in it.  If col
 * isn't NULL, store the new display width in it.  If *str is '\t', we
 * expect col to have the current display width.  If bad_char isn't
 * NULL, set it to TRUE if we have a null byte or a bad multibyte
 * character. */
int parse_char(const char *str, int *chr, size_t *col
#ifdef NANO_WIDE
	, bool *bad_char
#endif
	)
{
    int wide_str, wide_str_len;

    assert(str != NULL);

#ifdef NANO_WIDE
    if (bad_char != NULL)
	*bad_char = FALSE;

    if (!ISSET(NO_UTF8)) {
	wchar_t tmp;

	/* Get the wide character equivalent of the multibyte
	 * character. */
	wide_str_len = mbtowc(&tmp, str, MB_CUR_MAX);
	wide_str = (int)tmp;

	/* If str contains a null byte or an invalid multibyte
	 * character, interpret str's first byte as a single-byte
	 * sequence and set bad_char to TRUE. */
	if (wide_str_len <= 0) {
	    wide_str_len = 1;
	    wide_str = (unsigned char)*str;
	    if (bad_char != NULL)
		*bad_char = TRUE;
	}

	/* Save the wide character in chr. */
	if (chr != NULL)
	    *chr = wide_str;

	/* Save the column width of the wide character in col. */
	if (col != NULL) {
	    /* If we have a tab, get its width in columns using the
	     * current value of col. */
	    if (wide_str == '\t')
		*col += tabsize - *col % tabsize;
	    /* If we have a control character, get its width using one
	     * column for the "^" that will be displayed in front of it,
	     * and the width in columns of its visible equivalent as
	     * returned by control_rep(). */
	    else if (is_cntrl_char(wide_str)) {
		char *ctrl_wide_str = charalloc(MB_CUR_MAX);

		(*col)++;
		wide_str = control_rep((unsigned char)wide_str);

		if (wctomb(ctrl_wide_str, (wchar_t)wide_str) != -1)
		    *col += wcwidth(wide_str);

		free(ctrl_wide_str);
	    /* If we have a normal character, get its width in columns
	     * normally. */
	    } else
		*col += wcwidth(wide_str);
	}
    } else {
#endif
	/* Interpret str's first character as a single-byte sequence. */
	wide_str_len = 1;
	wide_str = (unsigned char)*str;

	/* Save the single-byte sequence in chr as though it's a wide
	 * character. */
	if (chr != NULL)
	    *chr = wide_str;

	if (col != NULL) {
	    /* If we have a tab, get its width in columns using the
	     * current value of col. */
	    if (wide_str == '\t')
		*col += tabsize - *col % tabsize;
	    /* If we have a control character, it's two columns wide:
	     * one column for the "^" that will be displayed in front of
	     * it, and one column for its visible equivalent as returned
	     * by control_rep(). */
	    else if (is_cntrl_char(wide_str))
		*col += 2;
	    /* If we have a normal character, it's one column wide. */
	    else
		(*col)++;
	}
#ifdef NANO_WIDE
    }
#endif

    return wide_str_len;
}

/* Return the index in str of the beginning of the character before the
 * one at pos. */
size_t move_left(const char *str, size_t pos)
{
    size_t pos_prev = pos;

    assert(str != NULL && pos <= strlen(str));

    /* There is no library function to move backward one multibyte
     * character.  Here is the naive, O(pos) way to do it. */
    while (TRUE) {
	int str_len = parse_char(str + pos - pos_prev, NULL, NULL
#ifdef NANO_WIDE
		, NULL
#endif
		);

	if (pos_prev <= str_len)
	    break;

	pos_prev -= str_len;
    }

    return pos - pos_prev;
}

/* Return the index in str of the beginning of the character after the
 * one at pos. */
size_t move_right(const char *str, size_t pos)
{
    return pos + parse_char(str + pos, NULL, NULL
#ifdef NANO_WIDE
	, NULL
#endif
	);
}

/* Fix the memory allocation for a string. */
void align(char **strp)
{
    assert(strp != NULL);
    if (*strp != NULL)
	*strp = charealloc(*strp, strlen(*strp) + 1);
}

/* Null a string at a certain index and align it. */
void null_at(char **data, size_t index)
{
    assert(data != NULL);
    *data = charealloc(*data, index + 1);
    (*data)[index] = '\0';
}

/* For non-null-terminated lines.  A line, by definition, shouldn't
 * normally have newlines in it, so encode its nulls as newlines. */
void unsunder(char *str, size_t true_len)
{
    assert(str != NULL);
    for (; true_len > 0; true_len--, str++) {
	if (*str == '\0')
	    *str = '\n';
    }
}

/* For non-null-terminated lines.  A line, by definition, shouldn't
 * normally have newlines in it, so decode its newlines into nulls. */
void sunder(char *str)
{
    assert(str != NULL);
    for (; *str != '\0'; str++) {
	if (*str == '\n')
	    *str = '\0';
    }
}

#ifndef HAVE_STRCASECMP
/* This function is equivalent to strcasecmp(). */
int nstricmp(const char *s1, const char *s2)
{
    assert(s1 != NULL && s2 != NULL);

    for (; *s1 != '\0' && *s2 != '\0'; s1++, s2++) {
	if (tolower(*s1) != tolower(*s2))
	    break;
    }

    return (tolower(*s1) - tolower(*s2));
}
#endif

#ifndef HAVE_STRNCASECMP
/* This function is equivalent to strncasecmp(). */
int nstrnicmp(const char *s1, const char *s2, size_t n)
{
    assert(s1 != NULL && s2 != NULL);

    for (; n > 0 && *s1 != '\0' && *s2 != '\0'; n--, s1++, s2++) {
	if (tolower(*s1) != tolower(*s2))
	    break;
    }

    if (n > 0)
	return (tolower(*s1) - tolower(*s2));
    else
	return 0;
}
#endif

#ifndef HAVE_STRCASESTR
/* This function is equivalent to strcasestr().  It was adapted from
 * mutt's mutt_stristr() function. */
const char *nstristr(const char *haystack, const char *needle)
{
    assert(haystack != NULL && needle != NULL);

    for (; *haystack != '\0'; haystack++) {
	const char *p = haystack;
	const char *q = needle;

	for (; tolower(*p) == tolower(*q) && *q != '\0'; p++, q++)
	    ;

	if (*q == '\0')
	    return haystack;
    }

    return NULL;
}
#endif

/* None of this is needed if we're using NANO_SMALL! */
#ifndef NANO_SMALL
const char *revstrstr(const char *haystack, const char *needle, const
	char *rev_start)
{
    for (; rev_start >= haystack; rev_start--) {
	const char *r, *q;

	for (r = rev_start, q = needle ; *q == *r && *q != '\0'; r++, q++)
	    ;
	if (*q == '\0')
	    return rev_start;
    }
    return NULL;
}

const char *revstristr(const char *haystack, const char *needle, const
	char *rev_start)
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

#ifndef HAVE_STRNLEN
/* This function is equivalent to strnlen(). */
size_t nstrnlen(const char *s, size_t maxlen)
{
    size_t n = 0;

    assert(s != NULL);

    for (; maxlen > 0 && *s != '\0'; maxlen--, n++, s++)
	;

    return n;
}
#endif

#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
#ifndef HAVE_GETLINE
/* This function is equivalent to getline().  It was adapted from
 * GNU mailutils' getline() function. */
ssize_t ngetline(char **lineptr, size_t *n, FILE *stream)
{
    return getdelim(lineptr, n, '\n', stream);
}
#endif

#ifndef HAVE_GETDELIM
/* This function is equivalent to getdelim().  It was adapted from
 * GNU mailutils' getdelim() function. */
ssize_t ngetdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
    size_t indx = 0;
    int c;

    /* Sanity checks. */
    if (lineptr == NULL || n == NULL || stream == NULL)
	return -1;

    /* Allocate the line the first time. */
    if (*lineptr == NULL) {
	*lineptr = charalloc(128);
	*n = 128;
    }

    while ((c = getc(stream)) != EOF) {
	/* Check if more memory is needed. */
	if (indx >= *n) {
	    *lineptr = charealloc(*lineptr, *n + 128);
	    *n += 128;
	}

	/* Push the result in the line. */
	(*lineptr)[indx++] = (char)c;

	/* Bail out. */
	if (c == delim)
	    break;
    }

    /* Make room for the null character. */
    if (indx >= *n) {
	*lineptr = charealloc(*lineptr, *n + 128);
	*n += 128;
    }

    /* Null terminate the buffer. */
    null_at(lineptr, indx++);
    *n = indx;

    /* The last line may not have the delimiter, we have to return what
     * we got and the error will be seen on the next iteration. */
    return (c == EOF && (indx - 1) == 0) ? -1 : indx - 1;
}
#endif
#endif /* !NANO_SMALL && ENABLE_NANORC */

/* If we are searching backwards, we will find the last match that
 * starts no later than start.  Otherwise we find the first match
 * starting no earlier than start.  If we are doing a regexp search, we
 * fill in the global variable regmatches with at most 9 subexpression
 * matches.  Also, all .rm_so elements are relative to the start of the
 * whole match, so regmatches[0].rm_so == 0. */
const char *strstrwrapper(const char *haystack, const char *needle,
	const char *start)
{
    /* start can be 1 character before the start or after the end of the
     * line.  In either case, we just say no match was found. */
    if ((start > haystack && *(start - 1) == '\0') || start < haystack)
	return NULL;
    assert(haystack != NULL && needle != NULL && start != NULL);
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP)) {
#ifndef NANO_SMALL
	if (ISSET(REVERSE_SEARCH)) {
	    if (regexec(&search_regexp, haystack, 1, regmatches, 0) == 0
		&& haystack + regmatches[0].rm_so <= start) {
		const char *retval = haystack + regmatches[0].rm_so;

		/* Search forward until there are no more matches. */
		while (regexec(&search_regexp, retval + 1, 1, regmatches,
			REG_NOTBOL) == 0 && retval + 1 +
			regmatches[0].rm_so <= start)
		    retval += 1 + regmatches[0].rm_so;
		/* Finally, put the subexpression matches in global
		 * variable regmatches.  The REG_NOTBOL flag doesn't
		 * matter now. */
		regexec(&search_regexp, retval, 10, regmatches, 0);
		return retval;
	    }
	} else
#endif /* !NANO_SMALL */
	if (regexec(&search_regexp, start, 10, regmatches,
		start > haystack ? REG_NOTBOL : 0) == 0) {
	    const char *retval = start + regmatches[0].rm_so;

	    regexec(&search_regexp, retval, 10, regmatches, 0);
	    return retval;
	}
	return NULL;
    }
#endif /* HAVE_REGEX_H */
#if !defined(DISABLE_SPELLER) || !defined(NANO_SMALL)
    if (ISSET(CASE_SENSITIVE)) {
#ifndef NANO_SMALL
	if (ISSET(REVERSE_SEARCH))
	    return revstrstr(haystack, needle, start);
	else
#endif
	    return strstr(start, needle);
    }
#endif /* !DISABLE_SPELLER || !NANO_SMALL */
#ifndef NANO_SMALL
    else if (ISSET(REVERSE_SEARCH))
	return revstristr(haystack, needle, start);
#endif
    return strcasestr(start, needle);
}

/* This is a wrapper for the perror() function.  The wrapper takes care
 * of ncurses, calls perror (which writes to stderr), then refreshes the
 * screen.  Note that nperror() causes the window to flicker once. */
void nperror(const char *s)
{
    /* leave ncurses mode, go to the terminal */
    if (endwin() != ERR) {
	perror(s);		/* print the error */
	total_refresh();	/* return to ncurses and repaint */
    }
}

/* Thanks BG, many ppl have been asking for this... */
void *nmalloc(size_t howmuch)
{
    void *r = malloc(howmuch);

    if (r == NULL && howmuch != 0)
	die(_("nano is out of memory!"));

    return r;
}

void *nrealloc(void *ptr, size_t howmuch)
{
    void *r = realloc(ptr, howmuch);

    if (r == NULL && howmuch != 0)
	die(_("nano is out of memory!"));

    return r;
}

/* Copy the first n characters of one malloc()ed string to another
 * pointer.  Should be used as: "dest = mallocstrncpy(dest, src,
 * n);". */
char *mallocstrncpy(char *dest, const char *src, size_t n)
{
    if (src == NULL)
	src = "";

    if (src != dest)
	free(dest);

    dest = charalloc(n);
    strncpy(dest, src, n);

    return dest;
}

/* Copy one malloc()ed string to another pointer.  Should be used as:
 * "dest = mallocstrcpy(dest, src);". */
char *mallocstrcpy(char *dest, const char *src)
{
    return mallocstrncpy(dest, src, src == NULL ? 1 : strlen(src) + 1);
}

/* Free the malloc()ed string at dest and return the malloc()ed string
 * at src.  Should be used as: "answer = mallocstrassn(answer,
 * real_dir_from_tilde(answer));". */
char *mallocstrassn(char *dest, char *src)
{
    free(dest);
    return src;
}

/* Append a new magicline to filebot. */
void new_magicline(void)
{
    filebot->next = (filestruct *)nmalloc(sizeof(filestruct));
    filebot->next->data = mallocstrcpy(NULL, "");
    filebot->next->prev = filebot;
    filebot->next->next = NULL;
    filebot->next->lineno = filebot->lineno + 1;
    filebot = filebot->next;
    totlines++;
    totsize++;
}

#ifndef NANO_SMALL
/* Remove the magicline from filebot, if there is one and it isn't the
 * only line in the file. */
void remove_magicline(void)
{
    if (filebot->data[0] == '\0' && filebot->prev != NULL) {
	filebot = filebot->prev;
	free_filestruct(filebot->next);
	filebot->next = NULL;
	totlines--;
	totsize--;
    }
}

/* Set top_x and bot_x to the top and bottom x-coordinates of the mark,
 * respectively, based on the locations of top and bot.  If
 * right_side_up isn't NULL, set it to TRUE If the mark begins with
 * (mark_beginbuf, mark_beginx) and ends with (current, current_x), or
 * FALSE otherwise. */
void mark_order(const filestruct **top, size_t *top_x, const filestruct
	**bot, size_t *bot_x, bool *right_side_up)
{
    assert(top != NULL && top_x != NULL && bot != NULL && bot_x != NULL);

    if ((current->lineno == mark_beginbuf->lineno && current_x >
	mark_beginx) || current->lineno > mark_beginbuf->lineno) {
	*top = mark_beginbuf;
	*top_x = mark_beginx;
	*bot = current;
	*bot_x = current_x;
	if (right_side_up != NULL)
	    *right_side_up = TRUE;
    } else {
	*bot = mark_beginbuf;
	*bot_x = mark_beginx;
	*top = current;
	*top_x = current_x;
	if (right_side_up != NULL)
	    *right_side_up = FALSE;
    }
}
#endif

/* Calculate the number of lines and the number of characters between
 * begin and end, and return them in lines and size, respectively. */
void get_totals(const filestruct *begin, const filestruct *end, int
	*lines, long *size)
{
    const filestruct *f;

    if (lines != NULL)
	*lines = 0;
    if (size != NULL)
	*size = 0;

    /* Go through the lines from begin to end->prev, if we can. */
    for (f = begin; f != NULL && f != end; f = f->next) {
	/* Count this line. */
	if (lines != NULL)
	    (*lines)++;

	/* Count the number of characters on this line. */
	if (size != NULL) {
	    *size += strlen(f->data);

	    /* Count the newline if we have one. */
	    if (f->next != NULL)
		(*size)++;
	}
    }

    /* Go through the line at end, if we can. */
    if (f != NULL) {
	/* Count this line. */
	if (lines != NULL)
	    (*lines)++;

	/* Count the number of characters on this line. */
	if (size != NULL) {
	    *size += strlen(f->data);

	    /* Count the newline if we have one. */
	    if (f->next != NULL)
		(*size)++;
	}
    }
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

    while (*text != '\0' || *pattern != '\0') {
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
	    if (!found && len != 0) {
		return FALSE;
	    }
	    if (found) {
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
		if (*text != '\0')
		    text++;
		break;
	    }

	    if (*text != '\0') {
		pattern = retrypat;
		text = ++retrytext;
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
