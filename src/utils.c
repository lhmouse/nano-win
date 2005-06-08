/* $Id$ */
/**************************************************************************
 *   utils.c                                                              *
 *                                                                        *
 *   Copyright (C) 1999-2005 Chris Allegretta                             *
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
#include <pwd.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "proto.h"

#ifdef HAVE_REGEX_H
#ifdef BROKEN_REGEXEC
/* Work around a potential segfault in glibc 2.2.3's regexec(). */
int safe_regexec(const regex_t *preg, const char *string, size_t nmatch,
	regmatch_t pmatch[], int eflags)
{
    if (string != NULL && *string != '\0')
	return regexec(preg, string, nmatch, pmatch, eflags);

    return REG_NOMATCH;
}
#endif

int regexp_bol_or_eol(const regex_t *preg, const char *string)
{
    return (regexec(preg, string, 0, NULL, 0) == 0 &&
	regexec(preg, string, 0, NULL, REG_NOTBOL | REG_NOTEOL) ==
	REG_NOMATCH);
}
#endif /* HAVE_REGEX_H */

int digits(size_t n)
{
    int i = 1;

    while (n > 10) {
	n /= 10;
	i++;
    }

    return i;
}

/* Return the user's home directory.  We use $HOME, and if that fails,
 * we fall back on getpwuid(). */
void get_homedir(void)
{
    if (homedir == NULL) {
	const char *homenv = getenv("HOME");

	if (homenv == NULL) {
	    const struct passwd *userage = getpwuid(geteuid());

	    if (userage != NULL)
		homenv = userage->pw_dir;
	}
	homedir = mallocstrcpy(NULL, homenv);
    }
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

/* Read an int and a ssize_t, separated by a comma, from str, and store
 * them in *line and *column (if they're not both NULL).  On error, we
 * return FALSE.  Otherwise, we return TRUE. */
bool parse_line_column(const char *str, int *line, ssize_t *column)
{
    bool retval = TRUE;
    const char *comma;

    assert(str != NULL);

    comma = strchr(str, ',');

    if (comma != NULL && column != NULL) {
	if (!parse_num(str + (comma - str + 1), column))
	    retval = FALSE;
    }

    if (line != NULL) {
	if (comma != NULL) {
	    char *str_line = mallocstrncpy(NULL, str, comma - str + 1);
	    str_line[comma - str] = '\0';

	    if (str_line[0] != '\0' && !parse_num(str_line, line))
		retval = FALSE;

	    free(str_line);
	} else if (!parse_num(str, line))
	    retval = FALSE;
    }

    return retval;
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
	*lineptr = charalloc(MAX_BUF_SIZE);
	*n = MAX_BUF_SIZE;
    }

    while ((c = getc(stream)) != EOF) {
	/* Check if more memory is needed. */
	if (indx >= *n) {
	    *lineptr = charealloc(*lineptr, *n + MAX_BUF_SIZE);
	    *n += MAX_BUF_SIZE;
	}

	/* Push the result in the line. */
	(*lineptr)[indx++] = (char)c;

	/* Bail out. */
	if (c == delim)
	    break;
    }

    /* Make room for the null character. */
    if (indx >= *n) {
	*lineptr = charealloc(*lineptr, *n + MAX_BUF_SIZE);
	*n += MAX_BUF_SIZE;
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
		(start > haystack) ? REG_NOTBOL : 0) == 0) {
	    const char *retval = start + regmatches[0].rm_so;

	    regexec(&search_regexp, retval, 10, regmatches, 0);
	    return retval;
	}
	return NULL;
    }
#endif /* HAVE_REGEX_H */
#if !defined(NANO_SMALL) || !defined(DISABLE_SPELLER)
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
	return mbrevstrcasestr(haystack, needle, start);
#endif
    return mbstrcasestr(start, needle);
}

/* This is a wrapper for the perror() function.  The wrapper takes care
 * of curses, calls perror() (which writes to stderr), and then
 * refreshes the screen.  Note that nperror() causes the window to
 * flicker once. */
void nperror(const char *s)
{
    /* Leave curses mode and go to the terminal. */
    if (endwin() != ERR) {
	perror(s);		/* Print the error. */
	total_refresh();	/* Return to curses and refresh. */
    }
}

/* Thanks, BG, many people have been asking for this... */
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
    charcpy(dest, src, n);

    return dest;
}

/* Copy one malloc()ed string to another pointer.  Should be used as:
 * "dest = mallocstrcpy(dest, src);". */
char *mallocstrcpy(char *dest, const char *src)
{
    return mallocstrncpy(dest, src, (src == NULL) ? 1 :
	strlen(src) + 1);
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
	*lines, size_t *size)
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
	    *size += mbstrlen(f->data);

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
	    *size += mbstrlen(f->data);

	    /* Count the newline if we have one. */
	    if (f->next != NULL)
		(*size)++;
	}
    }
}
