/* $Id$ */
/**************************************************************************
 *   chars.c                                                              *
 *                                                                        *
 *   Copyright (C) 2005 Chris Allegretta                                  *
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
#include <ctype.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

#ifdef NANO_WIDE
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif
#endif

/* Return TRUE if the value of c is in byte range, and FALSE
 * otherwise. */
bool is_byte(int c)
{
    return ((unsigned int)c == (unsigned char)c);
}

/* This function is equivalent to isalnum(). */
bool is_alnum_char(int c)
{
    return isalnum(c);
}

/* This function is equivalent to isalnum() for multibyte characters. */
bool is_alnum_mbchar(const char *c)
{
    assert(c != NULL);

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	wchar_t wc;
	int c_mb_len = mbtowc(&wc, c, MB_CUR_MAX);

	if (c_mb_len <= 0) {
	    mbtowc(NULL, NULL, 0);
	    wc = (unsigned char)*c;
	}

	return is_alnum_wchar(wc);
    } else
#endif
	return is_alnum_char((unsigned char)*c);
}

#ifdef NANO_WIDE
/* This function is equivalent to isalnum() for wide characters. */
bool is_alnum_wchar(wchar_t wc)
{
    return iswalnum(wc);
}
#endif

/* This function is equivalent to isblank(). */
bool is_blank_char(int c)
{
    return
#ifdef HAVE_ISBLANK
	isblank(c)
#else
	isspace(c) && (c == '\t' || !is_cntrl_char(c))
#endif
	;
}

/* This function is equivalent to isblank() for multibyte characters. */
bool is_blank_mbchar(const char *c)
{
    assert(c != NULL);

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	wchar_t wc;
	int c_mb_len = mbtowc(&wc, c, MB_CUR_MAX);

	if (c_mb_len <= 0) {
	    mbtowc(NULL, NULL, 0);
	    wc = (unsigned char)*c;
	}

	return is_blank_wchar(wc);
    } else
#endif
	return is_blank_char((unsigned char)*c);
}

#ifdef NANO_WIDE
/* This function is equivalent to isblank() for wide characters. */
bool is_blank_wchar(wchar_t wc)
{
    return
#ifdef HAVE_ISWBLANK
	iswblank(wc)
#else
	iswspace(wc) && (wc == '\t' || !is_cntrl_wchar(wc))
#endif
	;
}
#endif

/* This function is equivalent to iscntrl(), except in that it also
 * handles control characters with their high bits set. */
bool is_cntrl_char(int c)
{
    return (-128 <= c && c < -96) || (0 <= c && c < 32) ||
	(127 <= c && c < 160);
}

/* This function is equivalent to iscntrl() for multibyte characters,
 * except in that it also handles multibyte control characters with
 * their high bits set. */
bool is_cntrl_mbchar(const char *c)
{
    assert(c != NULL);

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	wchar_t wc;
	int c_mb_len = mbtowc(&wc, c, MB_CUR_MAX);

	if (c_mb_len <= 0) {
	    mbtowc(NULL, NULL, 0);
	    wc = (unsigned char)*c;
	}

	return is_cntrl_wchar(wc);
    } else
#endif
	return is_cntrl_char((unsigned char)*c);
}

#ifdef NANO_WIDE
/* This function is equivalent to iscntrl() for wide characters, except
 * in that it also handles wide control characters with their high bits
 * set. */
bool is_cntrl_wchar(wchar_t wc)
{
    return (0 <= wc && wc < 32) || (127 <= wc && wc < 160);
}
#endif

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

/* c is a multibyte control character.  It displays as ^@, ^?, or ^[ch]
 * where ch is c + 64.  We return that multibyte character. */
char *control_mbrep(const char *c, char *crep, int *crep_len)
{
    assert(c != NULL);

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	wchar_t wc, wcrep;
	int c_mb_len = mbtowc(&wc, c, MB_CUR_MAX), crep_mb_len;

	if (c_mb_len <= 0) {
	    mbtowc(NULL, NULL, 0);
	    wc = (unsigned char)*c;
	}

	wcrep = control_wrep(wc);

	crep_mb_len = wctomb(crep, wcrep);

	if (crep_mb_len <= 0) {
	    wctomb(NULL, 0);
	    crep_mb_len = 0;
	}

	*crep_len = crep_mb_len;

	return crep;
    } else {
#endif
	*crep_len = 1;
	crep[0] = control_rep((unsigned char)*c);

	return crep;
#ifdef NANO_WIDE
    }
#endif
}

#ifdef NANO_WIDE
/* c is a wide control character.  It displays as ^@, ^?, or ^[ch] where
 * ch is c + 64.  We return that wide character. */
wchar_t control_wrep(wchar_t wc)
{
    /* Treat newlines embedded in a line as encoded nulls. */
    if (wc == '\n')
	return '@';
    else if (wc == NANO_CONTROL_8)
	return '?';
    else
	return wc + 64;
}
#endif

/* This function is equivalent to wcwidth() for multibyte characters. */
int mbwidth(const char *c)
{
    assert(c != NULL);

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	wchar_t wc;
	int c_mb_len = mbtowc(&wc, c, MB_CUR_MAX), width;

	if (c_mb_len <= 0) {
	    mbtowc(NULL, NULL, 0);
	    wc = (unsigned char)*c;
	}

	width = wcwidth(wc);
	if (width == -1)
	    width++;

	return width;
    } else
#endif
	return 1;
}

/* Return the maximum width in bytes of a multibyte character. */
int mb_cur_max(void)
{
    return
#ifdef NANO_WIDE
	!ISSET(NO_UTF8) ? MB_CUR_MAX :
#endif
	1;
}

/* Convert the value in chr to a multibyte character with the same
 * wide character value as chr.  Return the multibyte character and its
 * length. */
char *make_mbchar(int chr, char *chr_mb, int *chr_mb_len)
{
    assert(chr_mb != NULL && chr_mb_len != NULL);

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	*chr_mb_len = wctomb(chr_mb, chr);

	if (*chr_mb_len <= 0) {
	    wctomb(NULL, 0);
	    *chr_mb_len = 0;
	}
    } else {
#endif
	*chr_mb_len = 1;
	chr_mb[0] = (char)chr;
#ifdef NANO_WIDE
    }
#endif

    return chr_mb;
}

/* Parse a multibyte character from buf.  Return the number of bytes
 * used.  If chr isn't NULL, store the multibyte character in it.  If
 * bad_chr isn't NULL, set it to TRUE if we have a bad multibyte
 * character.  If col isn't NULL, store the new display width in it.  If
 * *str is '\t', we expect col to have the current display width. */
int parse_mbchar(const char *buf, char *chr, bool *bad_chr, size_t
	*col)
{
    int buf_mb_len;

    assert(buf != NULL);

    if (bad_chr != NULL)
	*bad_chr = FALSE;

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	/* Get the number of bytes in the multibyte character. */
	buf_mb_len = mblen(buf, MB_CUR_MAX);

	/* If buf contains a null byte or an invalid multibyte
	 * character, set bad_chr to TRUE (if it contains the latter)
	 * and interpret buf's first byte. */
	if (buf_mb_len <= 0) {
	    mblen(NULL, 0);
	    if (buf_mb_len < 0 && bad_chr != NULL)
		*bad_chr = TRUE;
	    buf_mb_len = 1;
	}

	/* Save the multibyte character in chr. */
	if (chr != NULL) {
	    int i;
	    for (i = 0; i < buf_mb_len; i++)
		chr[i] = buf[i];
	}

	/* Save the column width of the wide character in col. */
	if (col != NULL) {
	    /* If we have a tab, get its width in columns using the
	     * current value of col. */
	    if (*buf == '\t')
		*col += tabsize - *col % tabsize;
	    /* If we have a control character, get its width using one
	     * column for the "^" that will be displayed in front of it,
	     * and the width in columns of its visible equivalent as
	     * returned by control_mbrep(). */
	    else if (is_cntrl_mbchar(buf)) {
		char *ctrl_buf_mb = charalloc(MB_CUR_MAX);
		int ctrl_buf_mb_len;

		(*col)++;

		ctrl_buf_mb = control_mbrep(buf, ctrl_buf_mb,
			&ctrl_buf_mb_len);

		*col += mbwidth(ctrl_buf_mb);

		free(ctrl_buf_mb);
	    /* If we have a normal character, get its width in columns
	     * normally. */
	    } else
		*col += mbwidth(buf);
	}
    } else {
#endif
	/* Get the number of bytes in the byte character. */
	buf_mb_len = 1;

	/* Save the byte character in chr. */
	if (chr != NULL)
	    *chr = *buf;

	if (col != NULL) {
	    /* If we have a tab, get its width in columns using the
	     * current value of col. */
	    if (*buf == '\t')
		*col += tabsize - *col % tabsize;
	    /* If we have a control character, it's two columns wide:
	     * one column for the "^" that will be displayed in front of
	     * it, and one column for its visible equivalent as returned
	     * by control_mbrep(). */
	    else if (is_cntrl_char((unsigned char)*buf))
		*col += 2;
	    /* If we have a normal character, it's one column wide. */
	    else
		(*col)++;
	}
#ifdef NANO_WIDE
    }
#endif

    return buf_mb_len;
}

/* Return the index in buf of the beginning of the multibyte character
 * before the one at pos. */
size_t move_mbleft(const char *buf, size_t pos)
{
    size_t pos_prev = pos;

    assert(str != NULL && pos <= strlen(buf));

    /* There is no library function to move backward one multibyte
     * character.  Here is the naive, O(pos) way to do it. */
    while (TRUE) {
	int buf_mb_len = parse_mbchar(buf + pos - pos_prev, NULL, NULL,
		NULL);

	if (pos_prev <= (size_t)buf_mb_len)
	    break;

	pos_prev -= buf_mb_len;
    }

    return pos - pos_prev;
}

/* Return the index in buf of the beginning of the multibyte character
 * after the one at pos. */
size_t move_mbright(const char *buf, size_t pos)
{
    return pos + parse_mbchar(buf + pos, NULL, NULL, NULL);
}

#ifndef HAVE_STRCASECMP
/* This function is equivalent to strcasecmp(). */
int nstrcasecmp(const char *s1, const char *s2)
{
    return
#ifdef HAVE_STRNCASECMP
	strncasecmp(s1, s2, (size_t)-1);
#else
	nstrncasecmp(s1, s2, (size_t)-1);
#endif
}
#endif

/* This function is equivalent to strcasecmp() for multibyte strings. */
int mbstrcasecmp(const char *s1, const char *s2)
{
    return mbstrncasecmp(s1, s2, (size_t)-1);
}

#ifndef HAVE_STRNCASECMP
/* This function is equivalent to strncasecmp(). */
int nstrncasecmp(const char *s1, const char *s2, size_t n)
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

/* This function is equivalent to strncasecmp() for multibyte
 * strings. */
int mbstrncasecmp(const char *s1, const char *s2, size_t n)
{
#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	char *s1_mb = charalloc(MB_CUR_MAX);
	char *s2_mb = charalloc(MB_CUR_MAX);
	wchar_t ws1, ws2;

	assert(s1 != NULL && s2 != NULL);

	while (n > 0 && *s1 != '\0' && *s2 != '\0') {
	    int s1_mb_len, s2_mb_len;

	    s1_mb_len = parse_mbchar(s1, s1_mb, NULL, NULL);

	    if (mbtowc(&ws1, s1_mb, s1_mb_len) <= 0) {
		mbtowc(NULL, NULL, 0);
		ws1 = (unsigned char)*s1_mb;
	    }

	    s2_mb_len = parse_mbchar(s2, s2_mb, NULL, NULL);

	    if (mbtowc(&ws2, s2_mb, s2_mb_len) <= 0) {
		mbtowc(NULL, NULL, 0);
		ws2 = (unsigned char)*s2_mb;
	    }

	    if (n == 0 || towlower(ws1) != towlower(ws2))
		break;

	    s1 += s1_mb_len;
	    s2 += s2_mb_len;
	    n--;
	}

	free(s1_mb);
	free(s2_mb);

	return (towlower(ws1) - towlower(ws2));
    } else
#endif
	return
#ifdef HAVE_STRNCASECMP
		strncasecmp(s1, s2, n);
#else
		nstrncasecmp(s1, s2, n);
#endif
}

#ifndef HAVE_STRCASESTR
/* This function is equivalent to strcasestr().  It was adapted from
 * mutt's mutt_stristr() function. */
const char *nstrcasestr(const char *haystack, const char *needle)
{
    assert(haystack != NULL && needle != NULL);

    for (; *haystack != '\0'; haystack++) {
	const char *r = haystack, *q = needle;

	for (; tolower(*r) == tolower(*q) && *q != '\0'; r++, q++)
	    ;

	if (*q == '\0')
	    return haystack;
    }

    return NULL;
}
#endif

/* This function is equivalent to strcasestr() for multibyte strings. */
const char *mbstrcasestr(const char *haystack, const char *needle)
{
#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	char *r_mb = charalloc(MB_CUR_MAX);
	char *q_mb = charalloc(MB_CUR_MAX);
	wchar_t wr, wq;
	bool found_needle = FALSE;

	assert(haystack != NULL && needle != NULL);

	while (*haystack != '\0') {
	    const char *r = haystack, *q = needle;
	    int r_mb_len, q_mb_len;

	    while (*q != '\0') {
		r_mb_len = parse_mbchar(r, r_mb, NULL, NULL);

		if (mbtowc(&wr, r_mb, r_mb_len) <= 0) {
		    mbtowc(NULL, NULL, 0);
		    wr = (unsigned char)*r;
		}

		q_mb_len = parse_mbchar(q, q_mb, NULL, NULL);

		if (mbtowc(&wq, q_mb, q_mb_len) <= 0) {
		    mbtowc(NULL, NULL, 0);
		    wq = (unsigned char)*q;
		}

		if (towlower(wr) != towlower(wq))
		    break;

		r += r_mb_len;
		q += q_mb_len;
	    }

	    if (*q == '\0') {
		found_needle = TRUE;
		break;
	    }

	    haystack += move_mbright(haystack, 0);
	}

	free(r_mb);
	free(q_mb);

	return found_needle ? haystack : NULL;
    } else
#endif
	return
#ifdef HAVE_STRCASESTR
		strcasestr(haystack, needle);
#else
		nstrcasestr(haystack, needle);
#endif
}

#ifndef NANO_SMALL
/* This function is equivalent to strstr(), except in that it scans the
 * string in reverse, starting at rev_start. */
const char *revstrstr(const char *haystack, const char *needle, const
	char *rev_start)
{
    assert(haystack != NULL && needle != NULL && rev_start != NULL);

    for (; rev_start >= haystack; rev_start--) {
	const char *r, *q;

	for (r = rev_start, q = needle; *r == *q && *q != '\0'; r++, q++)
	    ;

	if (*q == '\0')
	    return rev_start;
    }

    return NULL;
}

/* This function is equivalent to strcasestr(), except in that it scans
 * the string in reverse, starting at rev_start. */
const char *revstrcasestr(const char *haystack, const char *needle,
	const char *rev_start)
{
    assert(haystack != NULL && needle != NULL && rev_start != NULL);

    for (; rev_start >= haystack; rev_start--) {
	const char *r = rev_start, *q = needle;

	for (; tolower(*r) == tolower(*q) && *q != '\0'; r++, q++)
	    ;

	if (*q == '\0')
	    return rev_start;
    }

    return NULL;
}

/* This function is equivalent to strcasestr() for multibyte strings,
 * except in that it scans the string in reverse, starting at
 * rev_start. */
const char *mbrevstrcasestr(const char *haystack, const char *needle,
	const char *rev_start)
{
#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	char *r_mb = charalloc(MB_CUR_MAX);
	char *q_mb = charalloc(MB_CUR_MAX);
	wchar_t wr, wq;
	bool begin_line = FALSE, found_needle = FALSE;

	assert(haystack != NULL && needle != NULL && rev_start != NULL);

	while (!begin_line) {
	    const char *r = rev_start, *q = needle;
	    int r_mb_len, q_mb_len;

	    while (*q != '\0') {
		r_mb_len = parse_mbchar(r, r_mb, NULL, NULL);

		if (mbtowc(&wr, r_mb, r_mb_len) <= 0) {
		    mbtowc(NULL, NULL, 0);
		    wr = (unsigned char)*r;
		}

		q_mb_len = parse_mbchar(q, q_mb, NULL, NULL);

		if (mbtowc(&wq, q_mb, q_mb_len) <= 0) {
		    mbtowc(NULL, NULL, 0);
		    wq = (unsigned char)*q;
		}

		if (towlower(wr) != towlower(wq))
		    break;

		r += r_mb_len;
		q += q_mb_len;
	    }

	    if (*q == '\0') {
		found_needle = TRUE;
		break;
	    }

	    if (rev_start == haystack)
		begin_line = TRUE;
	    else
		rev_start = haystack + move_mbleft(haystack, rev_start -
			haystack);
	}

	free(r_mb);
	free(q_mb);

	return found_needle ? rev_start : NULL;
    } else
#endif
	return revstrcasestr(haystack, needle, rev_start);
}
#endif

/* This function is equivalent to strlen() for multibyte strings. */
size_t mbstrlen(const char *s)
{
    return mbstrnlen(s, (size_t)-1);
}

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

/* This function is equivalent to strnlen() for multibyte strings. */
size_t mbstrnlen(const char *s, size_t maxlen)
{
    assert(s != NULL);

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	size_t n = 0;
	char *s_mb = charalloc(MB_CUR_MAX);
	int s_mb_len;

	while (*s != '\0') {
	    s_mb_len = parse_mbchar(s, s_mb, NULL, NULL);

	    if (maxlen == 0)
		break;

	    maxlen--;
	    s += s_mb_len;
	    n++;
	}

	free(s_mb);

	return n;
    } else
#endif
	return
#ifdef HAVE_STRNLEN
		strnlen(s, maxlen);
#else
		nstrnlen(s, maxlen);
#endif
}

#ifndef DISABLE_TABCOMP
/* Find the one-based position of the last occurrence of character c in
 * the first n characters of s.  Return 0 if c is not found. */
size_t strrchrn(const char *s, int c, size_t n)
{
    assert(n <= strlen(s));

    for (s += n - 1; n >= 1; n--, s--) {
	if (c == *s)
	    return n;
    }

    return 0;
}
#endif
