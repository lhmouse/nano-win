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
#include <ctype.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

#if defined(HAVE_WCHAR_H) && defined(NANO_WIDE)
#include <wchar.h>
#endif

#if defined(HAVE_WCTYPE_H) && defined(NANO_WIDE)
#include <wctype.h>
#endif

/* This function is equivalent to isblank(). */
bool is_blank_char(unsigned char c)
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
bool is_cntrl_char(unsigned char c)
{
    return (c < 32) || (127 <= c && c < 160);
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
#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8))
	return MB_CUR_MAX;
    else
#endif
	return 1;
}

/* Convert the value in chr to a multibyte character with the same
 * wide character value as chr.  Return the multibyte character and its
 * length. */
char *make_mbchar(unsigned int chr, char *chr_mb, int *chr_mb_len)
{
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
 * bad_chr isn't NULL, set it to TRUE if we have a null byte or a bad
 * multibyte character.  If col isn't NULL, store the new display width
 * in it.  If *str is '\t', we expect col to have the current display
 * width. */
int parse_mbchar(const char *buf, char *chr
#ifdef NANO_WIDE
	, bool *bad_chr
#endif
	, size_t *col)
{
    int buf_mb_len;

    assert(buf != NULL);

#ifdef NANO_WIDE
    if (bad_chr != NULL)
	*bad_chr = FALSE;

    if (!ISSET(NO_UTF8)) {
	/* Get the number of bytes in the multibyte character. */
	buf_mb_len = mblen(buf, MB_CUR_MAX);

	/* If buf contains a null byte or an invalid multibyte
	 * character, interpret buf's first byte and set bad_chr to
	 * TRUE. */
	if (buf_mb_len <= 0) {
	    mblen(NULL, 0);
	    buf_mb_len = 1;
	    if (bad_chr != NULL)
		*bad_chr = TRUE;
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
	     * returned by control_rep(). */
	    else if (is_cntrl_mbchar(buf)) {
		char *ctrl_buf_mb = charalloc(mb_cur_max());
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
	     * by control_rep(). */
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
