/* $Id$ */
/**************************************************************************
 *   winio.c                                                              *
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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

static buffer *key_buffer = NULL;
				/* The default keystroke buffer,
				 * containing all the keystrokes we have
				 * at a given point. */
static size_t key_buffer_len = 0;
				/* The length of the default keystroke
				 * buffer. */
static int statusblank = 0;	/* The number of keystrokes left after
				 * we call statusbar(), before we
				 * actually blank the statusbar. */
static bool resetstatuspos = FALSE;
				/* Should we reset the cursor position
				 * at the statusbar prompt? */

/* Control character compatibility:
 *
 * - NANO_BACKSPACE_KEY is Ctrl-H, which is Backspace under ASCII, ANSI,
 *   VT100, and VT220.
 * - NANO_TAB_KEY is Ctrl-I, which is Tab under ASCII, ANSI, VT100,
 *   VT220, and VT320.
 * - NANO_ENTER_KEY is Ctrl-M, which is Enter under ASCII, ANSI, VT100,
 *   VT220, and VT320.
 * - NANO_XON_KEY is Ctrl-Q, which is XON under ASCII, ANSI, VT100,
 *   VT220, and VT320.
 * - NANO_XOFF_KEY is Ctrl-S, which is XOFF under ASCII, ANSI, VT100,
 *   VT220, and VT320.
 * - NANO_CONTROL_8 is Ctrl-8 (Ctrl-?), which is Delete under ASCII,
 *   ANSI, VT100, and VT220, and which is Backspace under VT320.
 *
 * Note: VT220 and VT320 also generate Esc [ 3 ~ for Delete.  By
 * default, xterm assumes it's running on a VT320 and generates Ctrl-8
 * (Ctrl-?) for Backspace and Esc [ 3 ~ for Delete.  This causes
 * problems for VT100-derived terminals such as the FreeBSD console,
 * which expect Ctrl-H for Backspace and Ctrl-8 (Ctrl-?) for Delete, and
 * on which the VT320 sequences are translated by the keypad to KEY_DC
 * and [nothing].  We work around this conflict via the REBIND_DELETE
 * flag: if it's not set, we assume VT320 compatibility, and if it is,
 * we assume VT100 compatibility.  Thanks to Lee Nelson and Wouter van
 * Hemel for helping work this conflict out.
 *
 * Escape sequence compatibility:
 *
 * We support escape sequences for ANSI, VT100, VT220, VT320, the Linux
 * console, the FreeBSD console, the Mach console (a.k.a. the Hurd
 * console), xterm, rxvt, and Eterm.  Among these, there are several
 * conflicts and omissions, outlined as follows:
 *
 * - Tab on ANSI == PageUp on FreeBSD console; the former is omitted.
 *   (Ctrl-I is also Tab on ANSI, which we already support.)
 * - PageDown on FreeBSD console == Center (5) on numeric keypad with
 *   NumLock off on Linux console; the latter is omitted.  (The editing
 *   keypad key is more important to have working than the numeric
 *   keypad key, because the latter has no value when NumLock is off.)
 * - F1 on FreeBSD console == the mouse key on xterm/rxvt/Eterm; the
 *   latter is omitted.  (Mouse input will only work properly if the
 *   extended keypad value KEY_MOUSE is generated on mouse events
 *   instead of the escape sequence.)
 * - F9 on FreeBSD console == PageDown on Mach console; the former is
 *   omitted.  (The editing keypad is more important to have working
 *   than the function keys, because the functions of the former are not
 *   arbitrary and the functions of the latter are.)
 * - F10 on FreeBSD console == PageUp on Mach console; the former is
 *   omitted.  (Same as above.)
 * - F13 on FreeBSD console == End on Mach console; the former is
 *   omitted.  (Same as above.)
 * - F15 on FreeBSD console == Shift-Up on rxvt/Eterm; the former is
 *   omitted.  (The arrow keys, with or without modifiers, are more
 *   important to have working than the function keys, because the
 *   functions of the former are not arbitrary and the functions of the
 *   latter are.)
 * - F16 on FreeBSD console == Shift-Down on rxvt/Eterm; the former is
 *   omitted.  (Same as above.)
 *
 * Note that Center (5) on the numeric keypad with NumLock off can also
 * be the Begin key. */

#ifndef NANO_SMALL
/* Reset all the input routines that rely on character sequences. */
void reset_kbinput(void)
{
#ifdef NANO_WIDE
    /* Reset the multibyte and wide character interpreter states. */
    if (!ISSET(NO_UTF8)) {
	mbtowc(NULL, NULL, 0);
	wctomb(NULL, 0);
    }
#endif
    parse_kbinput(NULL, NULL, NULL, TRUE);
    get_word_kbinput(0, TRUE);
}
#endif

/* Read in a sequence of keystrokes from win and save them in the
 * default keystroke buffer.  This should only be called when the
 * default keystroke buffer is empty. */
void get_buffer(WINDOW *win)
{
    int input;
    size_t i = 0;

#ifdef NANO_WIDE
    size_t wide_key_buffer_len = 0;
    buffer *wide_key_buffer = NULL;
#endif

    /* If the keystroke buffer isn't empty, get out. */
    if (key_buffer != NULL)
	return;

    /* Read in the first character using blocking input. */
    nodelay(win, FALSE);

#ifndef NANO_SMALL
    allow_pending_sigwinch(TRUE);
#endif
    input = wgetch(win);
#ifndef NANO_SMALL
    allow_pending_sigwinch(FALSE);
#endif

    /* Increment the length of the keystroke buffer, save the value of
     * the keystroke in key, and set key_code to TRUE if the keystroke
     * is an extended keypad value and hence shouldn't be treated as a
     * multibyte character. */
    key_buffer_len++;
    key_buffer = (buffer *)nmalloc(sizeof(buffer));
    key_buffer[0].key = input;
    key_buffer[0].key_code = !is_byte_char(input);

    /* Read in the remaining characters using non-blocking input. */
    nodelay(win, TRUE);

    while (TRUE) {
#ifndef NANO_SMALL
	allow_pending_sigwinch(TRUE);
#endif
	input = wgetch(win);
#ifndef NANO_SMALL
	allow_pending_sigwinch(FALSE);
#endif

	/* If there aren't any more characters, stop reading. */
	if (input == ERR)
	    break;

	/* Otherwise, increment the length of the keystroke buffer, save
	 * the value of the keystroke in key, and set key_code to TRUE
	 * if the keystroke is an extended keypad value and hence
	 * shouldn't be treated as a multibyte character. */
	key_buffer_len++;
	key_buffer = (buffer *)nrealloc(key_buffer, key_buffer_len *
		sizeof(buffer));
	key_buffer[key_buffer_len - 1].key = input;
	key_buffer[key_buffer_len - 1].key_code = !is_byte_char(input);
    }

    /* Switch back to non-blocking input. */
    nodelay(win, FALSE);

#ifdef NANO_WIDE
    if (!ISSET(NO_UTF8)) {
	/* Change all incomplete or invalid multibyte keystrokes to -1,
	 * and change all complete and valid multibyte keystrokes to
	 * their wide character value. */
	for (i = 0; i < key_buffer_len; i++) {
	    wchar_t wide_key;

	    if (!key_buffer[i].key_code) {
		key_buffer[i].key = mbtowc(&wide_key,
			(const char *)&key_buffer[i].key, 1);
		if (key_buffer[i].key != -1)
			key_buffer[i].key = wide_key;
	    }
	}

	/* Save all of the non-(-1) keystrokes in another buffer. */
	for (i = 0; i < key_buffer_len; i++) {
	    if (key_buffer[i].key != -1) {
		wide_key_buffer_len++;
		wide_key_buffer = (buffer *)nrealloc(wide_key_buffer,
			wide_key_buffer_len * sizeof(buffer));

		wide_key_buffer[wide_key_buffer_len - 1].key =
			key_buffer[i].key;
		wide_key_buffer[wide_key_buffer_len - 1].key_code =
			key_buffer[i].key_code;
	    }
	}

	/* Replace the default keystroke buffer with the non-(-1)
	 * keystroke buffer. */
	key_buffer_len = wide_key_buffer_len;
	free(key_buffer);
	key_buffer = wide_key_buffer;
    }
#endif
}

/* Return the length of the default keystroke buffer. */
size_t get_buffer_len(void)
{
    return key_buffer_len;
}

/* Return the key values stored in the keystroke buffer input,
 * discarding the key_code values in it. */
int *buffer_to_keys(buffer *input, size_t input_len)
{
    int *sequence = (int *)nmalloc(input_len * sizeof(int));
    size_t i;

    for (i = 0; i < input_len; i++)
	sequence[i] = input[i].key;

    return sequence;
}

/* Add the contents of the keystroke buffer input to the default
 * keystroke buffer. */
void unget_input(buffer *input, size_t input_len)
{
#ifndef NANO_SMALL
    allow_pending_sigwinch(TRUE);
    allow_pending_sigwinch(FALSE);
#endif

    /* If input is empty, get out. */
    if (input_len == 0)
	return;

    /* If adding input would put the default keystroke buffer beyond
     * maximum capacity, only add enough of input to put it at maximum
     * capacity. */
    if (key_buffer_len + input_len < key_buffer_len)
	input_len = (size_t)-1 - key_buffer_len;

    /* Add the length of input to the length of the default keystroke
     * buffer, and reallocate the default keystroke buffer so that it
     * has enough room for input. */
    key_buffer_len += input_len;
    key_buffer = (buffer *)nrealloc(key_buffer, key_buffer_len *
	sizeof(buffer));

    /* If the default keystroke buffer wasn't empty before, move its
     * beginning forward far enough so that we can add input to its
     * beginning. */
    if (key_buffer_len > input_len)
	memmove(key_buffer + input_len, key_buffer,
		(key_buffer_len - input_len) * sizeof(buffer));

    /* Copy input to the beginning of the default keystroke buffer. */
    memcpy(key_buffer, input, input_len * sizeof(buffer));
}

/* Put back the character stored in kbinput.  If func_key is TRUE and
 * the character is out of byte range, interpret it as an extended
 * keypad value.  If meta_key is TRUE, put back the Escape character
 * after putting back kbinput. */	
void unget_kbinput(int kbinput, bool meta_key, bool func_key)
{
    buffer input;

    input.key = kbinput;
    input.key_code = (func_key && !is_byte_char(kbinput));

    unget_input(&input, 1);

    if (meta_key) {
	input.key = NANO_CONTROL_3;
	input.key_code = FALSE;
	unget_input(&input, 1);
    }
}

/* Try to read input_len characters from the default keystroke buffer.
 * If the default keystroke buffer is empty and win isn't NULL, try to
 * read in more characters from win and add them to the default
 * keystroke buffer before doing anything else.  If the default
 * keystroke buffer is empty and win is NULL, return NULL. */
buffer *get_input(WINDOW *win, size_t input_len)
{
    buffer *input;

#ifndef NANO_SMALL
    allow_pending_sigwinch(TRUE);
    allow_pending_sigwinch(FALSE);
#endif

    if (key_buffer_len == 0) {
	if (win != NULL)
	    get_buffer(win);

	if (key_buffer_len == 0)
	    return NULL;
    }

    /* If input_len is greater than the length of the default keystroke
     * buffer, only read the number of characters in the default
     * keystroke buffer. */
    if (input_len > key_buffer_len)
	input_len = key_buffer_len;

    /* Subtract input_len from the length of the default keystroke
     * buffer, and allocate the keystroke buffer input so that it
     * has enough room for input_len keystrokes. */
    key_buffer_len -= input_len;
    input = (buffer *)nmalloc(input_len * sizeof(buffer));

    /* Copy input_len characters from the beginning of the default
     * keystroke buffer into input. */
    memcpy(input, key_buffer, input_len * sizeof(buffer));

    /* If the default keystroke buffer is empty, mark it as such. */
    if (key_buffer_len == 0) {
	free(key_buffer);
	key_buffer = NULL;
    /* If the default keystroke buffer isn't empty, move its
     * beginning forward far enough back so that the keystrokes in input
     * are no longer at its beginning. */
    } else {
	memmove(key_buffer, key_buffer + input_len, key_buffer_len *
		sizeof(buffer));
	key_buffer = (buffer *)nrealloc(key_buffer, key_buffer_len *
		sizeof(buffer));
    }

    return input;
}

/* Read in a single character.  If it's ignored, swallow it and go on.
 * Otherwise, try to translate it from ASCII, meta key sequences, escape
 * sequences, and/or extended keypad values.  Set meta_key to TRUE when
 * we get a meta key sequence, and set func_key to TRUE when we get an
 * extended keypad value.  Supported extended keypad values consist of
 * [arrow key], Ctrl-[arrow key], Shift-[arrow key], Enter, Backspace,
 * the editing keypad (Insert, Delete, Home, End, PageUp, and PageDown),
 * the function keypad (F1-F16), and the numeric keypad with NumLock
 * off.  Assume nodelay(win) is FALSE. */
int get_kbinput(WINDOW *win, bool *meta_key, bool *func_key)
{
    int kbinput;

    /* Read in a character and interpret it.  Continue doing this until
     * we get a recognized value or sequence. */
    while ((kbinput = parse_kbinput(win, meta_key, func_key
#ifndef NANO_SMALL
		, FALSE
#endif
		)) == ERR);

    return kbinput;
}

/* Translate ASCII characters, extended keypad values, and escape
 * sequences into their corresponding key values.  Set meta_key to TRUE
 * when we get a meta key sequence, and set func_key to TRUE when we get
 * a function key.  Assume nodelay(win) is FALSE. */
int parse_kbinput(WINDOW *win, bool *meta_key, bool *func_key
#ifndef NANO_SMALL
	, bool reset
#endif
	)

{
    static int escapes = 0;
    static int word_digits = 0;
    buffer *kbinput;
    int retval = ERR;
    bool no_func_key = FALSE;

    if (reset) {
	escapes = 0;
	word_digits = 0;
	return ERR;
    }

    *meta_key = FALSE;
    *func_key = FALSE;

    /* Read in a character. */
    while ((kbinput = get_input(win, 1)) == NULL);

    /* If we got an extended keypad value or an ASCII character,
     * translate it. */
    if (kbinput->key_code || is_byte_char(kbinput->key)) {
	switch (kbinput->key) {
	    case ERR:
		break;
	    case NANO_CONTROL_3:
		/* Increment the escape counter. */
		escapes++;
		switch (escapes) {
		    case 1:
			/* One escape: wait for more input. */
		    case 2:
			/* Two escapes: wait for more input. */
			break;
		    default:
			/* More than two escapes: reset the escape
			 * counter and wait for more input. */
			escapes = 0;
		}
		break;
#if !defined(NANO_SMALL) && defined(KEY_RESIZE)
	    /* Since we don't change the default SIGWINCH handler when
	     * NANO_SMALL is defined, KEY_RESIZE is never generated.
	     * Also, Slang and SunOS 5.7-5.9 don't support
	     * KEY_RESIZE. */
	    case KEY_RESIZE:
		break;
#endif
#ifdef PDCURSES
	    case KEY_SHIFT_L:
	    case KEY_SHIFT_R:
	    case KEY_CONTROL_L:
	    case KEY_CONTROL_R:
	    case KEY_ALT_L:
	    case KEY_ALT_R:
		break;
#endif
	    default:
		switch (escapes) {
		    case 0:
			switch (kbinput->key) {
			    case NANO_CONTROL_8:
				retval = ISSET(REBIND_DELETE) ?
					NANO_DELETE_KEY :
					NANO_BACKSPACE_KEY;
				break;
			    case KEY_DOWN:
				retval = NANO_NEXTLINE_KEY;
				break;
			    case KEY_UP:
				retval = NANO_PREVLINE_KEY;
				break;
			    case KEY_LEFT:
				retval = NANO_BACK_KEY;
				break;
			    case KEY_RIGHT:
				retval = NANO_FORWARD_KEY;
				break;
#ifdef KEY_HOME
			    /* HP-UX 10 and 11 don't support
			     * KEY_HOME. */
			    case KEY_HOME:
				retval = NANO_HOME_KEY;
				break;
#endif
			    case KEY_BACKSPACE:
				retval = NANO_BACKSPACE_KEY;
				break;
			    case KEY_DC:
				retval = ISSET(REBIND_DELETE) ?
					NANO_BACKSPACE_KEY :
					NANO_DELETE_KEY;
				break;
			    case KEY_IC:
				retval = NANO_INSERTFILE_KEY;
				break;
			    case KEY_NPAGE:
				retval = NANO_NEXTPAGE_KEY;
				break;
			    case KEY_PPAGE:
				retval = NANO_PREVPAGE_KEY;
				break;
			    case KEY_ENTER:
				retval = NANO_ENTER_KEY;
				break;
			    case KEY_A1:	/* Home (7) on numeric
						 * keypad with NumLock
						 * off. */
				retval = NANO_HOME_KEY;
				break;
			    case KEY_A3:	/* PageUp (9) on numeric
						 * keypad with NumLock
						 * off. */
				retval = NANO_PREVPAGE_KEY;
				break;
			    case KEY_B2:	/* Center (5) on numeric
						 * keypad with NumLock
						 * off. */
				break;
			    case KEY_C1:	/* End (1) on numeric
						 * keypad with NumLock
						 * off. */
				retval = NANO_END_KEY;
				break;
			    case KEY_C3:	/* PageDown (4) on
						 * numeric keypad with
						 * NumLock off. */
				retval = NANO_NEXTPAGE_KEY;
				break;
#ifdef KEY_BEG
			    /* Slang doesn't support KEY_BEG. */
			    case KEY_BEG:	/* Center (5) on numeric
						 * keypad with NumLock
						 * off. */
				break;
#endif
#ifdef KEY_END
			    /* HP-UX 10 and 11 don't support KEY_END. */
			    case KEY_END:
				retval = NANO_END_KEY;
				break;
#endif
#ifdef KEY_SUSPEND
			    /* Slang doesn't support KEY_SUSPEND. */
			    case KEY_SUSPEND:
				retval = NANO_SUSPEND_KEY;
				break;
#endif
#ifdef KEY_SLEFT
			    /* Slang doesn't support KEY_SLEFT. */
			    case KEY_SLEFT:
				retval = NANO_BACK_KEY;
				break;
#endif
#ifdef KEY_SRIGHT
			    /* Slang doesn't support KEY_SRIGHT. */
			    case KEY_SRIGHT:
				retval = NANO_FORWARD_KEY;
				break;
#endif
			    default:
				retval = kbinput->key;
				break;
			}
			break;
		    case 1:
			/* One escape followed by a non-escape: escape
			 * sequence mode.  Reset the escape counter.  If
		 	 * there aren't any other keys waiting, we have
			 * a meta key sequence, so set meta_key to TRUE
			 * and save the lowercase version of the
			 * non-escape character as the result.  If there
			 * are other keys waiting, we have a true escape
			 * sequence, so interpret it. */
			escapes = 0;
			if (get_buffer_len() == 0) {
			    *meta_key = TRUE;
			    retval = tolower(kbinput->key);
			} else {
			    buffer *escape_kbinput;
			    int *sequence;
			    size_t seq_len;
			    bool ignore_seq;

			    /* Put back the non-escape character, get
			     * the complete escape sequence, translate
			     * its key values into the corresponding key
			     * value, and save that as the result. */
			    unget_input(kbinput, 1);
			    seq_len = get_buffer_len();
			    escape_kbinput = get_input(NULL, seq_len);
			    sequence = buffer_to_keys(escape_kbinput,
				seq_len);
			    retval = get_escape_seq_kbinput(sequence,
				seq_len, &ignore_seq);

			    /* If the escape sequence is unrecognized
			     * and not ignored, put back all of its
			     * characters except for the initial
			     * escape. */
			    if (retval == ERR && !ignore_seq)
				unget_input(escape_kbinput, seq_len);

			    free(escape_kbinput);
			}
			break;
		    case 2:
			/* Two escapes followed by one or more decimal
			 * digits: word sequence mode.  If the word
			 * sequence's range is limited to 6XXXX (the
			 * first digit is in the '0' to '6' range and
			 * it's the first digit, or it's in the '0' to
			 * '9' range and it's not the first digit),
			 * increment the word sequence counter,
			 * interpret the digit, and save the
			 * corresponding word value as the result.  If
			 * the word sequence's range is not limited to
			 * 6XXXX, fall through. */
			if (('0' <= kbinput->key && kbinput->key <= '6'
				&& word_digits == 0) ||
				('0' <= kbinput->key && kbinput->key <= '9'
				&& word_digits > 0)) {
			    word_digits++;
			    retval = get_word_kbinput(kbinput->key
#ifndef NANO_SMALL
				, FALSE
#endif
				);

			    if (retval != ERR) {
				/* If we've read in a complete word
				 * sequence, reset the word sequence
				 * counter and the escape counter, and
				 * mark it so that it isn't interpreted
				 * as an extended keypad value. */
				word_digits = 0;
				escapes = 0;
				no_func_key = TRUE;
			    }
			} else {
			    /* Reset the escape counter. */
			    escapes = 0;
			    if (word_digits == 0)
				/* Two escapes followed by a non-decimal
				 * digit or a decimal digit that would
				 * create a word sequence greater than
				 * 6XXXX, and we're not in the middle of
				 * a word sequence: control character
				 * sequence mode.  Interpret the control
				 * sequence and save the corresponding
				 * control character as the result. */
				retval = get_control_kbinput(kbinput->key);
			    else {
				/* If we're in the middle of a word
				 * sequence, reset the word sequence
				 * counter and save the character we got
				 * as the result. */
				word_digits = 0;
				retval = kbinput->key;
			    }
			}
			break;
		}
	}
    }

    /* If we have a result and it's an extended keypad value, set
     * func_key to TRUE. */
    if (retval != ERR)
	*func_key = !is_byte_char(retval);

#ifdef DEBUG
    fprintf(stderr, "parse_kbinput(): kbinput->key = %d, meta_key = %d, func_key = %d, escapes = %d, word_digits = %d, retval = %d\n", kbinput->key, (int)*meta_key, (int)*func_key, escapes, word_digits, retval);
#endif

    /* Return the result. */
    return retval;
}

/* Translate escape sequences, most of which correspond to extended
 * keypad values, into their corresponding key values.  These sequences
 * are generated when the keypad doesn't support the needed keys.  If
 * the escape sequence is recognized but we want to ignore it, return
 * ERR and set ignore_seq to TRUE; if it's unrecognized, return ERR and
 * set ignore_seq to FALSE.  Assume that Escape has already been read
 * in. */
int get_escape_seq_kbinput(const int *sequence, size_t seq_len, bool
	*ignore_seq)
{
    int retval = ERR;

    *ignore_seq = FALSE;

    if (seq_len > 1) {
	switch (sequence[0]) {
	    case 'O':
		switch (sequence[1]) {
		    case '2':
			if (seq_len >= 3) {
			    switch (sequence[2]) {
				case 'P': /* Esc O 2 P == F13 on
					   * xterm. */
				    retval = KEY_F(13);
				    break;
				case 'Q': /* Esc O 2 Q == F14 on
					   * xterm. */
				    retval = KEY_F(14);
				    break;
				case 'R': /* Esc O 2 R == F15 on
					   * xterm. */
				    retval = KEY_F(15);
				    break;
				case 'S': /* Esc O 2 S == F16 on
					   * xterm. */
				    retval = KEY_F(16);
				    break;
			    }
			}
			break;
		    case 'A': /* Esc O A == Up on VT100/VT320/xterm. */
		    case 'B': /* Esc O B == Down on
			       * VT100/VT320/xterm. */
		    case 'C': /* Esc O C == Right on
			       * VT100/VT320/xterm. */
		    case 'D': /* Esc O D == Left on
			       * VT100/VT320/xterm. */
			retval = get_escape_seq_abcd(sequence[1]);
			break;
		    case 'E': /* Esc O E == Center (5) on numeric keypad
			       * with NumLock off on xterm. */
			*ignore_seq = TRUE;
			break;
		    case 'F': /* Esc O F == End on xterm. */
			retval = NANO_END_KEY;
			break;
		    case 'H': /* Esc O H == Home on xterm. */
			retval = NANO_HOME_KEY;
			break;
		    case 'M': /* Esc O M == Enter on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * Eterm. */
			retval = NANO_ENTER_KEY;
			break;
		    case 'P': /* Esc O P == F1 on VT100/VT220/VT320/Mach
			       * console. */
			retval = KEY_F(1);
			break;
		    case 'Q': /* Esc O Q == F2 on VT100/VT220/VT320/Mach
			       * console. */
			retval = KEY_F(2);
			break;
		    case 'R': /* Esc O R == F3 on VT100/VT220/VT320/Mach
			       * console. */
			retval = KEY_F(3);
			break;
		    case 'S': /* Esc O S == F4 on VT100/VT220/VT320/Mach
			       * console. */
			retval = KEY_F(4);
			break;
		    case 'T': /* Esc O T == F5 on Mach console. */
			retval = KEY_F(5);
			break;
		    case 'U': /* Esc O U == F6 on Mach console. */
			retval = KEY_F(6);
			break;
		    case 'V': /* Esc O V == F7 on Mach console. */
			retval = KEY_F(7);
			break;
		    case 'W': /* Esc O W == F8 on Mach console. */
			retval = KEY_F(8);
			break;
		    case 'X': /* Esc O X == F9 on Mach console. */
			retval = KEY_F(9);
			break;
		    case 'Y': /* Esc O Y == F10 on Mach console. */
			retval = KEY_F(10);
			break;
		    case 'a': /* Esc O a == Ctrl-Up on rxvt. */
		    case 'b': /* Esc O b == Ctrl-Down on rxvt. */
		    case 'c': /* Esc O c == Ctrl-Right on rxvt. */
		    case 'd': /* Esc O d == Ctrl-Left on rxvt. */
			retval = get_escape_seq_abcd(sequence[1]);
			break;
		    case 'j': /* Esc O j == '*' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt. */
			retval = '*';
			break;
		    case 'k': /* Esc O k == '+' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt. */
			retval = '+';
			break;
		    case 'l': /* Esc O l == ',' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt. */
			retval = '+';
			break;
		    case 'm': /* Esc O m == '-' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt. */
			retval = '-';
			break;
		    case 'n': /* Esc O n == Delete (.) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * xterm/rxvt. */
			retval = NANO_DELETE_KEY;
			break;
		    case 'o': /* Esc O o == '/' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt. */
			retval = '/';
			break;
		    case 'p': /* Esc O p == Insert (0) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt. */
			retval = NANO_INSERTFILE_KEY;
			break;
		    case 'q': /* Esc O q == End (1) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt. */
			retval = NANO_END_KEY;
			break;
		    case 'r': /* Esc O r == Down (2) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt. */
			retval = NANO_NEXTLINE_KEY;
			break;
		    case 's': /* Esc O s == PageDown (3) on numeric
			       * keypad with NumLock off on VT100/VT220/
			       * VT320/rxvt. */
			retval = NANO_NEXTPAGE_KEY;
			break;
		    case 't': /* Esc O t == Left (4) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt. */
			retval = NANO_BACK_KEY;
			break;
		    case 'u': /* Esc O u == Center (5) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm. */
			*ignore_seq = TRUE;
			break;
		    case 'v': /* Esc O v == Right (6) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt. */
			retval = NANO_FORWARD_KEY;
			break;
		    case 'w': /* Esc O w == Home (7) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt. */
			retval = NANO_HOME_KEY;
			break;
		    case 'x': /* Esc O x == Up (8) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt. */
			retval = NANO_PREVLINE_KEY;
			break;
		    case 'y': /* Esc O y == PageUp (9) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt. */
			retval = NANO_PREVPAGE_KEY;
			break;
		}
		break;
	    case 'o':
		switch (sequence[1]) {
		    case 'a': /* Esc o a == Ctrl-Up on Eterm. */
		    case 'b': /* Esc o b == Ctrl-Down on Eterm. */
		    case 'c': /* Esc o c == Ctrl-Right on Eterm. */
		    case 'd': /* Esc o d == Ctrl-Left on Eterm. */
			retval = get_escape_seq_abcd(sequence[1]);
			break;
		}
		break;
	    case '[':
		switch (sequence[1]) {
		    case '1':
			if (seq_len >= 3) {
			    switch (sequence[2]) {
				case '1': /* Esc [ 1 1 ~ == F1 on rxvt/
					   * Eterm. */
				    retval = KEY_F(1);
				    break;
				case '2': /* Esc [ 1 2 ~ == F2 on rxvt/
					   * Eterm. */
				    retval = KEY_F(2);
				    break;
				case '3': /* Esc [ 1 3 ~ == F3 on rxvt/
					   * Eterm. */
				    retval = KEY_F(3);
				    break;
				case '4': /* Esc [ 1 4 ~ == F4 on rxvt/
					   * Eterm. */
				    retval = KEY_F(4);
				    break;
				case '5': /* Esc [ 1 5 ~ == F5 on xterm/
					   * rxvt/Eterm. */
				    retval = KEY_F(5);
				    break;
				case '7': /* Esc [ 1 7 ~ == F6 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    retval = KEY_F(6);
				    break;
				case '8': /* Esc [ 1 8 ~ == F7 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    retval = KEY_F(7);
				    break;
				case '9': /* Esc [ 1 9 ~ == F8 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    retval = KEY_F(8);
				    break;
				case ';':
    if (seq_len >= 4) {
	switch (sequence[3]) {
	    case '2':
		if (seq_len >= 5) {
		    switch (sequence[4]) {
			case 'A': /* Esc [ 1 ; 2 A == Shift-Up on
				   * xterm. */
			case 'B': /* Esc [ 1 ; 2 B == Shift-Down on
				   * xterm. */
			case 'C': /* Esc [ 1 ; 2 C == Shift-Right on
				   * xterm. */
			case 'D': /* Esc [ 1 ; 2 D == Shift-Left on
				   * xterm. */
			    retval = get_escape_seq_abcd(sequence[4]);
			    break;
		    }
		}
		break;
	    case '5':
		if (seq_len >= 5) {
		    switch (sequence[4]) {
			case 'A': /* Esc [ 1 ; 5 A == Ctrl-Up on
				   * xterm. */
			case 'B': /* Esc [ 1 ; 5 B == Ctrl-Down on
				   * xterm. */
			case 'C': /* Esc [ 1 ; 5 C == Ctrl-Right on
				   * xterm. */
			case 'D': /* Esc [ 1 ; 5 D == Ctrl-Left on
				   * xterm. */
			    retval = get_escape_seq_abcd(sequence[4]);
			    break;
		    }
		}
		break;
	}
    }
				    break;
				default: /* Esc [ 1 ~ == Home on
					  * VT320/Linux console. */
				    retval = NANO_HOME_KEY;
				    break;
			    }
			}
			break;
		    case '2':
			if (seq_len >= 3) {
			    switch (sequence[2]) {
				case '0': /* Esc [ 2 0 ~ == F9 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    retval = KEY_F(9);
				    break;
				case '1': /* Esc [ 2 1 ~ == F10 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    retval = KEY_F(10);
				    break;
				case '3': /* Esc [ 2 3 ~ == F11 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    retval = KEY_F(11);
				    break;
				case '4': /* Esc [ 2 4 ~ == F12 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    retval = KEY_F(12);
				    break;
				case '5': /* Esc [ 2 5 ~ == F13 on
					   * VT220/VT320/Linux console/
					   * rxvt/Eterm. */
				    retval = KEY_F(13);
				    break;
				case '6': /* Esc [ 2 6 ~ == F14 on
					   * VT220/VT320/Linux console/
					   * rxvt/Eterm. */
				    retval = KEY_F(14);
				    break;
				case '8': /* Esc [ 2 8 ~ == F15 on
					   * VT220/VT320/Linux console/
					   * rxvt/Eterm. */
				    retval = KEY_F(15);
				    break;
				case '9': /* Esc [ 2 9 ~ == F16 on
					   * VT220/VT320/Linux console/
					   * rxvt/Eterm. */
				    retval = KEY_F(16);
				    break;
				default: /* Esc [ 2 ~ == Insert on
					  * VT220/VT320/Linux console/
					  * xterm. */
				    retval = NANO_INSERTFILE_KEY;
				    break;
			    }
			}
			break;
		    case '3': /* Esc [ 3 ~ == Delete on VT220/VT320/
			       * Linux console/xterm. */
			retval = NANO_DELETE_KEY;
			break;
		    case '4': /* Esc [ 4 ~ == End on VT220/VT320/Linux
			       * console/xterm. */
			retval = NANO_END_KEY;
			break;
		    case '5': /* Esc [ 5 ~ == PageUp on VT220/VT320/
			       * Linux console/xterm; Esc [ 5 ^ ==
			       * PageUp on Eterm. */
			retval = NANO_PREVPAGE_KEY;
			break;
		    case '6': /* Esc [ 6 ~ == PageDown on VT220/VT320/
			       * Linux console/xterm; Esc [ 6 ^ ==
			       * PageDown on Eterm. */
			retval = NANO_NEXTPAGE_KEY;
			break;
		    case '7': /* Esc [ 7 ~ == Home on rxvt. */
			retval = NANO_HOME_KEY;
			break;
		    case '8': /* Esc [ 8 ~ == End on rxvt. */
			retval = NANO_END_KEY;
			break;
		    case '9': /* Esc [ 9 == Delete on Mach console. */
			retval = NANO_DELETE_KEY;
			break;
		    case '@': /* Esc [ @ == Insert on Mach console. */
			retval = NANO_INSERTFILE_KEY;
			break;
		    case 'A': /* Esc [ A == Up on ANSI/VT220/Linux
			       * console/FreeBSD console/Mach console/
			       * rxvt/Eterm. */
		    case 'B': /* Esc [ B == Down on ANSI/VT220/Linux
			       * console/FreeBSD console/Mach console/
			       * rxvt/Eterm. */
		    case 'C': /* Esc [ C == Right on ANSI/VT220/Linux
			       * console/FreeBSD console/Mach console/
			       * rxvt/Eterm. */
		    case 'D': /* Esc [ D == Left on ANSI/VT220/Linux
			       * console/FreeBSD console/Mach console/
			       * rxvt/Eterm. */
			retval = get_escape_seq_abcd(sequence[1]);
			break;
		    case 'E': /* Esc [ E == Center (5) on numeric keypad
			       * with NumLock off on FreeBSD console. */
			*ignore_seq = TRUE;
			break;
		    case 'F': /* Esc [ F == End on FreeBSD
			       * console/Eterm. */
			retval = NANO_END_KEY;
			break;
		    case 'G': /* Esc [ G == PageDown on FreeBSD
			       * console. */
			retval = NANO_NEXTPAGE_KEY;
			break;
		    case 'H': /* Esc [ H == Home on ANSI/VT220/FreeBSD
			       * console/Mach console/Eterm. */
			retval = NANO_HOME_KEY;
			break;
		    case 'I': /* Esc [ I == PageUp on FreeBSD
			       * console. */
			retval = NANO_PREVPAGE_KEY;
			break;
		    case 'L': /* Esc [ L == Insert on ANSI/FreeBSD
			       * console. */
			retval = NANO_INSERTFILE_KEY;
			break;
		    case 'M': /* Esc [ M == F1 on FreeBSD console. */
			retval = KEY_F(1);
			break;
		    case 'N': /* Esc [ N == F2 on FreeBSD console. */
			retval = KEY_F(2);
			break;
		    case 'O':
			if (seq_len >= 3) {
			    switch (sequence[2]) {
				case 'P': /* Esc [ O P == F1 on
					   * xterm. */
				    retval = KEY_F(1);
				    break;
				case 'Q': /* Esc [ O Q == F2 on
					   * xterm. */
				    retval = KEY_F(2);
				    break;
				case 'R': /* Esc [ O R == F3 on
					   * xterm. */
				    retval = KEY_F(3);
				    break;
				case 'S': /* Esc [ O S == F4 on
					   * xterm. */
				    retval = KEY_F(4);
				    break;
			    }
			} else {
			    /* Esc [ O == F3 on FreeBSD console. */
			    retval = KEY_F(3);
			}
			break;
		    case 'P': /* Esc [ P == F4 on FreeBSD console. */
			retval = KEY_F(4);
			break;
		    case 'Q': /* Esc [ Q == F5 on FreeBSD console. */
			retval = KEY_F(5);
			break;
		    case 'R': /* Esc [ R == F6 on FreeBSD console. */
			retval = KEY_F(6);
			break;
		    case 'S': /* Esc [ S == F7 on FreeBSD console. */
			retval = KEY_F(7);
			break;
		    case 'T': /* Esc [ T == F8 on FreeBSD console. */
			retval = KEY_F(8);
			break;
		    case 'U': /* Esc [ U == PageDown on Mach console. */
			retval = NANO_NEXTPAGE_KEY;
			break;
		    case 'V': /* Esc [ V == PageUp on Mach console. */
			retval = NANO_PREVPAGE_KEY;
			break;
		    case 'W': /* Esc [ W == F11 on FreeBSD console. */
			retval = KEY_F(11);
			break;
		    case 'X': /* Esc [ X == F12 on FreeBSD console. */
			retval = KEY_F(12);
			break;
		    case 'Y': /* Esc [ Y == End on Mach console. */
			retval = NANO_END_KEY;
			break;
		    case 'Z': /* Esc [ Z == F14 on FreeBSD console. */
			retval = KEY_F(14);
			break;
		    case 'a': /* Esc [ a == Shift-Up on rxvt/Eterm. */
		    case 'b': /* Esc [ b == Shift-Down on rxvt/Eterm. */
		    case 'c': /* Esc [ c == Shift-Right on rxvt/
			       * Eterm. */
		    case 'd': /* Esc [ d == Shift-Left on rxvt/Eterm. */
			retval = get_escape_seq_abcd(sequence[1]);
			break;
		    case '[':
			if (seq_len >= 3) {
			    switch (sequence[2]) {
				case 'A': /* Esc [ [ A == F1 on Linux
					   * console. */
				    retval = KEY_F(1);
				    break;
				case 'B': /* Esc [ [ B == F2 on Linux
					   * console. */
				    retval = KEY_F(2);
				    break;
				case 'C': /* Esc [ [ C == F3 on Linux
					   * console. */
				    retval = KEY_F(3);
				    break;
				case 'D': /* Esc [ [ D == F4 on Linux
					   * console. */
				    retval = KEY_F(4);
				    break;
				case 'E': /* Esc [ [ E == F5 on Linux
					   * console. */
				    retval = KEY_F(5);
				    break;
			    }
			}
			break;
		}
		break;
	}
    }

#ifdef DEBUG
    fprintf(stderr, "get_escape_seq_kbinput(): retval = %d, ignore_seq = %d\n", retval, (int)*ignore_seq);
#endif

    return retval;
}

/* Return the equivalent arrow key value for the case-insensitive
 * letters A (up), B (down), C (right), and D (left).  These are common
 * to many escape sequences. */
int get_escape_seq_abcd(int kbinput)
{
    switch (tolower(kbinput)) {
	case 'a':
	    return NANO_PREVLINE_KEY;
	case 'b':
	    return NANO_NEXTLINE_KEY;
	case 'c':
	    return NANO_FORWARD_KEY;
	case 'd':
	    return NANO_BACK_KEY;
	default:
	    return ERR;
    }
}

/* Translate a word sequence: turn a three-digit decimal number from
 * 000 to 255 into its corresponding word value. */
int get_word_kbinput(int kbinput
#ifndef NANO_SMALL
	, bool reset
#endif
	)
{
    static int word_digits = 0;
    static int word_kbinput = 0;
    int retval = ERR;

#ifndef NANO_SMALL
    if (reset) {
	word_digits = 0;
	word_kbinput = 0;
	return ERR;
    }
#endif

    /* Increment the word digit counter. */
    word_digits++;

    switch (word_digits) {
	case 1:
	    /* One digit: reset the word sequence holder and add the
	     * digit we got to the 10000's position of the word sequence
	     * holder. */
	    word_kbinput = 0;
	    if ('0' <= kbinput && kbinput <= '6')
		word_kbinput += (kbinput - '0') * 10000;
	    else
		/* If the character we got isn't a decimal digit, or if
		 * it is and it would put the word sequence out of word
		 * range, save it as the result. */
		retval = kbinput;
	    break;
	case 2:
	    /* Two digits: add the digit we got to the 1000's position
	     * of the word sequence holder. */
	    if (('0' <= kbinput && kbinput <= '5') ||
		(word_kbinput < 60000 && '6' <= kbinput &&
		kbinput <= '9'))
		word_kbinput += (kbinput - '0') * 1000;
	    else
		/* If the character we got isn't a decimal digit, or if
		 * it is and it would put the word sequence out of word
		 * range, save it as the result. */
		retval = kbinput;
	    break;
	case 3:
	    /* Three digits: add the digit we got to the 100's position
	     * of the word sequence holder. */
	    if (('0' <= kbinput && kbinput <= '5') ||
		(word_kbinput < 65000 && '6' <= kbinput &&
		kbinput <= '9'))
		word_kbinput += (kbinput - '0') * 100;
	    else
		/* If the character we got isn't a decimal digit, or if
		 * it is and it would put the word sequence out of word
		 * range, save it as the result. */
		retval = kbinput;
	    break;
	case 4:
	    /* Four digits: add the digit we got to the 10's position of
	     * the word sequence holder. */
	    if (('0' <= kbinput && kbinput <= '3') ||
		(word_kbinput < 65500 && '4' <= kbinput &&
		kbinput <= '9'))
		word_kbinput += (kbinput - '0') * 10;
	    else
		/* If the character we got isn't a decimal digit, or if
		 * it is and it would put the word sequence out of word
		 * range, save it as the result. */
		retval = kbinput;
	    break;
	case 5:
	    /* Five digits: add the digit we got to the 1's position of
	     * the word sequence holder, and save the corresponding word
	     * value as the result. */
	    if (('0' <= kbinput && kbinput <= '5') ||
		(word_kbinput < 65530 && '6' <= kbinput &&
		kbinput <= '9')) {
		word_kbinput += (kbinput - '0');
		retval = word_kbinput;
	    } else
		/* If the character we got isn't a decimal digit, or if
		 * it is and it would put the word sequence out of word
		 * range, save it as the result. */
		retval = kbinput;
	    break;
	default:
	    /* More than three digits: save the character we got as the
	     * result. */
	    retval = kbinput;
	    break;
    }

    /* If we have a result, reset the word digit counter and the word
     * sequence holder. */
    if (retval != ERR) {
	word_digits = 0;
	word_kbinput = 0;
    }

#ifdef DEBUG
    fprintf(stderr, "get_word_kbinput(): kbinput = %d, word_digits = %d, word_kbinput = %d, retval = %d\n", kbinput, word_digits, word_kbinput, retval);
#endif

    return retval;
}

/* Translate a control character sequence: turn an ASCII non-control
 * character into its corresponding control character. */
int get_control_kbinput(int kbinput)
{
    int retval;

     /* Ctrl-2 (Ctrl-Space, Ctrl-@, Ctrl-`) */
    if (kbinput == '2' || kbinput == ' ' || kbinput == '@' ||
	kbinput == '`')
	retval = NANO_CONTROL_SPACE;
    /* Ctrl-3 (Ctrl-[, Esc) to Ctrl-7 (Ctrl-_) */
    else if ('3' <= kbinput && kbinput <= '7')
	retval = kbinput - 24;
    /* Ctrl-8 (Ctrl-?) */
    else if (kbinput == '8' || kbinput == '?')
	retval = NANO_CONTROL_8;
    /* Ctrl-A to Ctrl-_ */
    else if ('A' <= kbinput && kbinput <= '_')
	retval = kbinput - 64;
    /* Ctrl-a to Ctrl-~ */
    else if ('a' <= kbinput && kbinput <= '~')
	retval = kbinput - 96;
    else
	retval = kbinput;

#ifdef DEBUG
    fprintf(stderr, "get_control_kbinput(): kbinput = %d, retval = %d\n", kbinput, retval);
#endif

    return retval;
}

/* Read in a string of characters verbatim, and return the length of the
 * string in kbinput_len.  Assume nodelay(win) is FALSE. */
int *get_verbatim_kbinput(WINDOW *win, size_t *kbinput_len)
{
    int *retval;

    /* Turn off flow control characters if necessary so that we can type
     * them in verbatim, and turn the keypad off so that we don't get
     * extended keypad values. */
    if (ISSET(PRESERVE))
	disable_flow_control();
    keypad(win, FALSE);

    /* Read in a stream of characters and interpret it if possible. */
    retval = parse_verbatim_kbinput(win, kbinput_len);

    /* Turn flow control characters back on if necessary and turn the
     * keypad back on now that we're done. */
    if (ISSET(PRESERVE))
	enable_flow_control();
    keypad(win, TRUE);

    return retval;
}

/* Read in a stream of all available characters.  Translate the first
 * few characters of the input into the corresponding word value if
 * possible.  After that, leave the input as-is. */ 
int *parse_verbatim_kbinput(WINDOW *win, size_t *kbinput_len)
{
    buffer *kbinput, *sequence;
    int word, *retval;

    /* Read in the first keystroke. */
    while ((kbinput = get_input(win, 1)) == NULL);

    /* Check whether the first keystroke is a decimal digit. */
    word = get_word_kbinput(kbinput->key
#ifndef NANO_SMALL
	, FALSE
#endif
	);

    /* If the first keystroke isn't a decimal digit, put back the first
     * keystroke. */
    if (word != ERR)
	unget_input(kbinput, 1);
    /* Otherwise, read in keystrokes until we have a complete word
     * sequence, and put back the corresponding word value. */
    else {
	buffer word_kbinput;

	while (word == ERR) {
	    while ((kbinput = get_input(win, 1)) == NULL);
	    word = get_word_kbinput(kbinput->key
#ifndef NANO_SMALL
		, FALSE
#endif
		);
	}

	word_kbinput.key = word;
	word_kbinput.key_code = FALSE;

	unget_input(&word_kbinput, 1);
    }

    /* Get the complete sequence, and save the key values in it as the
     * result. */
    *kbinput_len = get_buffer_len();
    sequence = get_input(NULL, *kbinput_len);
    retval = buffer_to_keys(sequence, *kbinput_len);
    free(sequence);

    return retval;
}

#ifndef DISABLE_MOUSE
/* Check for a mouse event, and if one's taken place, save the
 * coordinates where it took place in mouse_x and mouse_y.  After that,
 * assuming allow_shortcuts is FALSE, if the shortcut list on the
 * bottom two lines of the screen is visible and the mouse event took
 * place on it, figure out which shortcut was clicked and put back the
 * equivalent keystroke(s).  Return FALSE if no keystrokes were
 * put back, or TRUE if at least one was.  Assume that KEY_MOUSE has
 * already been read in. */
bool get_mouseinput(int *mouse_x, int *mouse_y, bool allow_shortcuts)
{
    MEVENT mevent;

    *mouse_x = -1;
    *mouse_y = -1;

    /* First, get the actual mouse event. */
    if (getmouse(&mevent) == ERR)
	return FALSE;

    /* Save the screen coordinates where the mouse event took place. */
    *mouse_x = mevent.x;
    *mouse_y = mevent.y;

    /* If we're allowing shortcuts, the current shortcut list is being
     * displayed on the last two lines of the screen, and the mouse
     * event took place inside it, we need to figure out which shortcut
     * was clicked and put back the equivalent keystroke(s) for it. */
    if (allow_shortcuts && !ISSET(NO_HELP) && wenclose(bottomwin,
	*mouse_y, *mouse_x)) {
	int i, j;
	size_t currslen;
	    /* The number of shortcuts in the current shortcut list. */
	const shortcut *s = currshortcut;
	    /* The actual shortcut we clicked on, starting at the first
	     * one in the current shortcut list. */

	/* Get the shortcut lists' length. */
	if (currshortcut == main_list)
	    currslen = MAIN_VISIBLE;
	else {
	    currslen = length_of_list(currshortcut);

	    /* We don't show any more shortcuts than the main list
	     * does. */
	    if (currslen > MAIN_VISIBLE)
		currslen = MAIN_VISIBLE;
	}

	/* Calculate the width of each shortcut in the list (it's the
	 * same for all of them). */
	if (currslen < 2)
	    i = COLS / 6;
	else
	    i = COLS / ((currslen / 2) + (currslen % 2));

	/* Calculate the y-coordinates relative to the beginning of
	 * bottomwin, i.e, the bottom three lines of the screen. */
	j = *mouse_y - (editwinrows + 3);

	/* If we're on the statusbar, beyond the end of the shortcut
	 * list, or beyond the end of a shortcut on the right side of
	 * the screen, don't do anything. */
	if (j < 0 || (*mouse_x / i) >= currslen)
	    return FALSE;
	j = (*mouse_x / i) * 2 + j;
	if (j >= currslen)
	    return FALSE;

	/* Go through the shortcut list to determine which shortcut was
	 * clicked. */
	for (; j > 0; j--)
	    s = s->next;

	/* And put back the equivalent key.  Assume that each shortcut
	 * has, at the very least, an equivalent control key, an
	 * equivalent primary meta key sequence, or both. */
	if (s->ctrlval != NANO_NO_KEY)
	    unget_kbinput(s->ctrlval, FALSE, FALSE);
	else if (s->metaval != NANO_NO_KEY)
	    unget_kbinput(s->metaval, TRUE, FALSE);

	return TRUE;
    }
    return FALSE;
}
#endif /* !DISABLE_MOUSE */

const shortcut *get_shortcut(const shortcut *s_list, int *kbinput, bool
	*meta_key, bool *func_key)
{
    const shortcut *s = s_list;
    size_t slen = length_of_list(s_list);

#ifdef DEBUG
    fprintf(stderr, "get_shortcut(): kbinput = %d, meta_key = %d, func_key = %d\n", *kbinput, (int)*meta_key, (int)*func_key);
#endif

    /* Check for shortcuts. */
    for (; slen > 0; slen--) {
	/* We've found a shortcut if:
	 *
	 * 1. The key exists.
	 * 2. The key is a control key in the shortcut list.
	 * 3. meta_key is TRUE and the key is the primary or
	 *    miscellaneous meta sequence in the shortcut list.
	 * 4. func_key is TRUE and the key is a function key in the
	 *    shortcut list. */

	if (*kbinput != NANO_NO_KEY && (*kbinput == s->ctrlval ||
		(*meta_key == TRUE && (*kbinput == s->metaval ||
		*kbinput == s->miscval)) || (*func_key == TRUE &&
		*kbinput == s->funcval))) {
	    break;
	}

	s = s->next;
    }

    /* Translate the shortcut to either its control key or its meta key
     * equivalent.  Assume that the shortcut has an equivalent control
     * key, an equivalent primary meta key sequence, or both. */
    if (slen > 0) {
	if (s->ctrlval != NANO_NO_KEY) {
	    *meta_key = FALSE;
	    *func_key = FALSE;
	    *kbinput = s->ctrlval;
	} else if (s->metaval != NANO_NO_KEY) {
	    *meta_key = TRUE;
	    *func_key = FALSE;
	    *kbinput = s->metaval;
	}
	return s;
    }

    return NULL;
}

#ifndef NANO_SMALL
const toggle *get_toggle(int kbinput, bool meta_key)
{
    const toggle *t = toggles;

#ifdef DEBUG
    fprintf(stderr, "get_toggle(): kbinput = %d, meta_key = %d\n", kbinput, (int)meta_key);
#endif

    /* Check for toggles. */
    for (; t != NULL; t = t->next) {
	/* We've found a toggle if meta_key is TRUE and the key is in
	 * the meta toggle list. */
	if (meta_key && kbinput == t->val)
	    break;
    }

    return t;
}
#endif /* !NANO_SMALL */

/* Return the placewewant associated with current_x.  That is, xplustabs
 * is the zero-based column position of the cursor.  Value is no smaller
 * than current_x. */
size_t xplustabs(void)
{
    return strnlenpt(current->data, current_x);
}

/* actual_x() gives the index in str of the character displayed at
 * column xplus.  That is, actual_x() is the largest value such that
 * strnlenpt(str, actual_x(str, xplus)) <= xplus. */
size_t actual_x(const char *str, size_t xplus)
{
    size_t i = 0;
	/* The position in str, returned. */
    size_t length = 0;
	/* The screen display width to str[i]. */

    assert(str != NULL);

    for (; length < xplus && *str != '\0'; i++, str++) {
	if (*str == '\t')
	    length += tabsize - (length % tabsize);
	else if (is_cntrl_char(*str))
	    length += 2;
	else
	    length++;
    }
    assert(length == strnlenpt(str - i, i));
    assert(i <= strlen(str - i));

    if (length > xplus)
	i--;

    return i;
}

/* A strlen() with tabs factored in, similar to xplustabs().  How many
 * columns wide are the first size characters of buf? */
size_t strnlenpt(const char *buf, size_t size)
{
    size_t length = 0;

    assert(buf != NULL);
    for (; *buf != '\0' && size != 0; size--, buf++) {
	if (*buf == '\t')
	    length += tabsize - (length % tabsize);
	else if (is_cntrl_char(*buf))
	    length += 2;
	else
	    length++;
    }
    return length;
}

/* How many columns wide is buf? */
size_t strlenpt(const char *buf)
{
    return strnlenpt(buf, (size_t)-1);
}

void blank_titlebar(void)
{
    mvwaddstr(topwin, 0, 0, hblank);
}

void blank_edit(void)
{
    int i;
    for (i = 0; i < editwinrows; i++)
	mvwaddstr(edit, i, 0, hblank);
}

void blank_statusbar(void)
{
    mvwaddstr(bottomwin, 0, 0, hblank);
}

void check_statusblank(void)
{
    if (statusblank > 1)
	statusblank--;
    else if (statusblank == 1 && !ISSET(CONSTUPDATE)) {
	statusblank = 0;
	blank_statusbar();
	wnoutrefresh(bottomwin);
	reset_cursor();
	wrefresh(edit);
    }
}

void blank_bottombars(void)
{
    if (!ISSET(NO_HELP)) {
	mvwaddstr(bottomwin, 1, 0, hblank);
	mvwaddstr(bottomwin, 2, 0, hblank);
    }
}

/* Convert buf into a string that can be displayed on screen.  The
 * caller wants to display buf starting with column start_col, and
 * extending for at most len columns.  start_col is zero-based.  len is
 * one-based, so len == 0 means you get "" returned.  The returned
 * string is dynamically allocated, and should be freed. */
char *display_string(const char *buf, size_t start_col, size_t len)
{
    size_t start_index;
	/* Index in buf of first character shown in return value. */
    size_t column;
	/* Screen column start_index corresponds to. */
    size_t end_index;
	/* Index in buf of last character shown in return value. */
    size_t alloc_len;
	/* The length of memory allocated for converted. */
    char *converted;
	/* The string we return. */
    size_t index;
	/* Current position in converted. */

    if (len == 0)
	return mallocstrcpy(NULL, "");

    start_index = actual_x(buf, start_col);
    column = strnlenpt(buf, start_index);
    assert(column <= start_col);
    end_index = actual_x(buf, start_col + len - 1);
    alloc_len = strnlenpt(buf, end_index + 1) - column;
    if (len > alloc_len + column - start_col)
	len = alloc_len + column - start_col;
    converted = charalloc(alloc_len + 1);
    buf += start_index;
    index = 0;

    for (; index < alloc_len; buf++) {
	if (*buf == '\t') {
	    converted[index++] =
#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
		ISSET(WHITESPACE_DISPLAY) ? whitespace[0] :
#endif
		' '; 
	    while ((column + index) % tabsize)
		converted[index++] = ' ';
	} else if (is_cntrl_char(*buf)) {
	    converted[index++] = '^';
	    if (*buf == '\n')
		/* Treat newlines embedded in a line as encoded nulls;
		 * the line in question should be run through unsunder()
		 * before reaching here. */
		converted[index++] = '@';
	    else if (*buf == NANO_CONTROL_8)
		converted[index++] = '?';
	    else
		converted[index++] = *buf + 64;
	} else if (*buf == ' ')
	    converted[index++] =
#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
		ISSET(WHITESPACE_DISPLAY) ? whitespace[1] :
#endif
		' ';
	else
	    converted[index++] = *buf;
    }
    assert(len <= alloc_len + column - start_col);
    charmove(converted, converted + start_col - column, len);
    null_at(&converted, len);

    return charealloc(converted, len + 1);
}

/* Repaint the statusbar when getting a character in nanogetstr().  buf
 * should be no longer than max(0, COLS - 4).
 *
 * Note that we must turn on A_REVERSE here, since do_help() turns it
 * off! */
void nanoget_repaint(const char *buf, const char *inputbuf, size_t x)
{
    size_t x_real = strnlenpt(inputbuf, x);
    int wid = COLS - strlen(buf) - 2;

    assert(0 <= x && x <= strlen(inputbuf));

    wattron(bottomwin, A_REVERSE);
    blank_statusbar();

    mvwaddstr(bottomwin, 0, 0, buf);
    waddch(bottomwin, ':');

    if (COLS > 1)
	waddch(bottomwin, x_real < wid ? ' ' : '$');
    if (COLS > 2) {
	size_t page_start = x_real - x_real % wid;
	char *expanded = display_string(inputbuf, page_start, wid);

	assert(wid > 0);
	assert(strlen(expanded) <= wid);
	waddstr(bottomwin, expanded);
	free(expanded);
	wmove(bottomwin, 0, COLS - wid + x_real - page_start);
    } else
	wmove(bottomwin, 0, COLS - 1);
    wattroff(bottomwin, A_REVERSE);
}

/* Get the input from the keyboard; this should only be called from
 * statusq(). */
int nanogetstr(bool allow_tabs, const char *buf, const char *def,
#ifndef NANO_SMALL
		historyheadtype *history_list,
#endif
		const shortcut *s
#ifndef DISABLE_TABCOMP
		, bool *list
#endif
		)
{
    int kbinput;
    bool meta_key, func_key;
    static size_t x = (size_t)-1;
	/* the cursor position in 'answer' */
    size_t xend;
	/* length of 'answer', the status bar text */
    bool tabbed = FALSE;
	/* used by input_tab() */
    const shortcut *t;

#ifndef NANO_SMALL
   /* for history */
    char *history = NULL;
    char *currentbuf = NULL;
    char *complete = NULL;
    int last_kbinput = 0;

    /* This variable is used in the search history code.  use_cb == 0 
       means that we're using the existing history and ignoring
       currentbuf.  use_cb == 1 means that the entry in answer should be
       moved to currentbuf or restored from currentbuf to answer. 
       use_cb == 2 means that the entry in currentbuf should be moved to
       answer or restored from answer to currentbuf. */
    int use_cb = 0;
#endif
    xend = strlen(def);

    /* Only put x at the end of the string if it's uninitialized, if it
       would be past the end of the string as it is, or if
       resetstatuspos is TRUE.  Otherwise, leave it alone.  This is so
       the cursor position stays at the same place if a prompt-changing
       toggle is pressed. */
    if (x == (size_t)-1 || x > xend || resetstatuspos)
	x = xend;

    answer = charealloc(answer, xend + 1);
    if (xend > 0)
	strcpy(answer, def);
    else
	answer[0] = '\0';

#if !defined(DISABLE_HELP) || !defined(DISABLE_MOUSE)
    currshortcut = s;
#endif

    /* Get the input! */

    nanoget_repaint(buf, answer, x);

    /* Make sure any editor screen updates are displayed before getting
       input */
    wnoutrefresh(edit);
    wrefresh(bottomwin);

    /* If we're using restricted mode, we aren't allowed to change the
     * name of a file once it has one because that would allow writing
     * to files not specified on the command line.  In this case,
     * disable all keys that would change the text if the filename isn't
     * blank and we're at the "Write File" prompt. */
    while ((kbinput = get_kbinput(bottomwin, &meta_key, &func_key)) !=
	NANO_CANCEL_KEY && kbinput != NANO_ENTER_KEY) {
	for (t = s; t != NULL; t = t->next) {
#ifdef DEBUG
	    fprintf(stderr, "Aha! \'%c\' (%d)\n", kbinput, kbinput);
#endif

	    /* Temporary hack to interpret NANO_HELP_FKEY correctly. */
	    if (kbinput == t->funcval)
		kbinput = t->ctrlval;

	    if (kbinput == t->ctrlval && is_cntrl_char(kbinput)) {

#ifndef DISABLE_HELP
		/* Have to do this here, it would be too late to do it
		   in statusq() */
		if (kbinput == NANO_HELP_KEY) {
		    do_help();
		    break;
		}
#endif
#ifndef NANO_SMALL
		/* Have to handle these here too, for the time being */
		if (kbinput == NANO_PREVLINE_KEY || kbinput == NANO_NEXTLINE_KEY)
		    break;
#endif

		return t->ctrlval;
	    }
	}
	assert(x <= xend && xend == strlen(answer));

	if (kbinput != '\t')
	    tabbed = FALSE;

	switch (kbinput) {
#ifndef DISABLE_MOUSE
	case KEY_MOUSE:
	    {
		int mouse_x, mouse_y;
		get_mouseinput(&mouse_x, &mouse_y, TRUE);
	    }
	    break;
#endif
	case NANO_REFRESH_KEY:
	    total_refresh();
	    break;
	case NANO_HOME_KEY:
#ifndef NANO_SMALL
	    if (ISSET(SMART_HOME)) {
		size_t old_x = x;

		for (x = 0; isblank(answer[x]) && x < xend; x++)
		    ;

		if (x == old_x || x == xend)
		    x = 0;
	    } else
#endif
		x = 0;
	    break;
	case NANO_END_KEY:
	    x = xend;
	    break;
	case NANO_FORWARD_KEY:
	    if (x < xend)
		x++;
	    break;
	case NANO_DELETE_KEY:
	    /* If we're using restricted mode, the filename isn't blank,
	     * and we're at the "Write File" prompt, disable Delete. */
	    if (!ISSET(RESTRICTED) || filename[0] == '\0' || s != writefile_list) {
		if (x < xend) {
		    charmove(answer + x, answer + x + 1, xend - x);
		    xend--;
		}
	    }
	    break;
	case NANO_CUT_KEY:
	    /* If we're using restricted mode, the filename isn't blank,
	     * and we're at the "Write File" prompt, disable Cut. */
	    if (!ISSET(RESTRICTED) || filename[0] == '\0' || s != writefile_list) {
		null_at(&answer, 0);
		xend = 0;
		x = 0;
	    }
	    break;
	case NANO_BACKSPACE_KEY:
	    /* If we're using restricted mode, the filename isn't blank,
	     * and we're at the "Write File" prompt, disable
	     * Backspace. */
	    if (!ISSET(RESTRICTED) || filename[0] == '\0' || s != writefile_list) {
		if (x > 0) {
		    charmove(answer + x - 1, answer + x, xend - x + 1);
		    x--;
		    xend--;
		}
	    }
	    break;
	case NANO_TAB_KEY:
#ifndef NANO_SMALL
	    /* tab history completion */
	    if (history_list != NULL) {
		if (!complete || last_kbinput != NANO_TAB_KEY) {
		    history_list->current = (historytype *)history_list;
		    history_list->len = strlen(answer);
		}

		if (history_list->len > 0) {
		    complete = get_history_completion(history_list, answer);
		    xend = strlen(complete);
		    x = xend;
		    answer = mallocstrcpy(answer, complete);
		}
	    }
#ifndef DISABLE_TABCOMP
	    else
#endif
#endif
#ifndef DISABLE_TABCOMP
	    if (allow_tabs) {
		int shift = 0;

		answer = input_tab(answer, x, &tabbed, &shift, list);
		xend = strlen(answer);
		x += shift;
		if (x > xend)
		    x = xend;
	    }
#endif
	    break;
	case NANO_BACK_KEY:
	    if (x > 0)
		x--;
	    break;
	case NANO_PREVLINE_KEY:
#ifndef NANO_SMALL
	    if (history_list != NULL) {

		/* if currentbuf is NULL, or if use_cb is 1, currentbuf
		   isn't NULL, and currentbuf is different from answer,
		   it means that we're scrolling up at the top of the
		   search history, and we need to save the current
		   answer in currentbuf; do this and reset use_cb to
		   0 */
		if (currentbuf == NULL || (use_cb == 1 &&
			strcmp(currentbuf, answer) != 0)) {
		    currentbuf = mallocstrcpy(currentbuf, answer);
		    use_cb = 0;
		}

		/* if currentbuf isn't NULL, use_cb is 2, and currentbuf 
		   is different from answer, it means that we're
		   scrolling up at the bottom of the search history, and
		   we need to make the string in currentbuf the current
		   answer; do this, blow away currentbuf since we don't
		   need it anymore, and reset use_cb to 0 */
		if (currentbuf != NULL && use_cb == 2 &&
			strcmp(currentbuf, answer) != 0) {
		    answer = mallocstrcpy(answer, currentbuf);
		    free(currentbuf);
		    currentbuf = NULL;
		    xend = strlen(answer);
		    use_cb = 0;

		/* else get older search from the history list and save
		   it in answer; if there is no older search, blank out 
		   answer */
		} else if ((history = get_history_older(history_list)) != NULL) {
		    answer = mallocstrcpy(answer, history);
		    xend = strlen(history);
		} else {
		    answer = mallocstrcpy(answer, "");
		    xend = 0;
		}
		x = xend;
	    }
#endif
	    break;
	case NANO_NEXTLINE_KEY:
#ifndef NANO_SMALL
	    if (history_list != NULL) {

		/* get newer search from the history list and save it 
		   in answer */
		if ((history = get_history_newer(history_list)) != NULL) {
		    answer = mallocstrcpy(answer, history);
		    xend = strlen(history);

		/* if there is no newer search, we're here */
		
		/* if currentbuf isn't NULL and use_cb isn't 2, it means 
		   that we're scrolling down at the bottom of the search
		   history and we need to make the string in currentbuf
		   the current answer; do this, blow away currentbuf
		   since we don't need it anymore, and set use_cb to
		   1 */
		} else if (currentbuf != NULL && use_cb != 2) {
		    answer = mallocstrcpy(answer, currentbuf);
		    free(currentbuf);
		    currentbuf = NULL;
		    xend = strlen(answer);
		    use_cb = 1;

		/* otherwise, if currentbuf is NULL and use_cb isn't 2, 
		   it means that we're scrolling down at the bottom of
		   the search history and the current answer (if it's
		   not blank) needs to be saved in currentbuf; do this,
		   blank out answer (if necessary), and set use_cb to
		   2 */
		} else if (use_cb != 2) {
		    if (answer[0] != '\0') {
			currentbuf = mallocstrcpy(currentbuf, answer);
			answer = mallocstrcpy(answer, "");
		    }
		    xend = 0;
		    use_cb = 2;
		}
		x = xend;
	    }
#endif
	    break;
	    default:

		for (t = s; t != NULL; t = t->next) {
#ifdef DEBUG
		    fprintf(stderr, "Aha! \'%c\' (%d)\n", kbinput,
			    kbinput);
#endif
		    if (meta_key && (kbinput == t->metaval || kbinput == t->miscval))
			/* We hit a meta key.  Do like above.  We don't
			 * just put back the letter and let it get
			 * caught above cause that screws the
			 * keypad... */
			return kbinput;
		}

	    /* If we're using restricted mode, the filename isn't blank,
	     * and we're at the "Write File" prompt, act as though the
	     * unhandled character we got is a control character and
	     * throw it away. */
	    if (is_cntrl_char(kbinput) || (ISSET(RESTRICTED) && filename[0] != '\0' && s == writefile_list))
		break;
	    answer = charealloc(answer, xend + 2);
	    charmove(answer + x + 1, answer + x, xend - x + 1);
	    xend++;
	    answer[x] = kbinput;
	    x++;

#ifdef DEBUG
	    fprintf(stderr, "input \'%c\' (%d)\n", kbinput, kbinput);
#endif
	} /* switch (kbinput) */
#ifndef NANO_SMALL
	last_kbinput = kbinput;
#endif
	nanoget_repaint(buf, answer, x);
	wrefresh(bottomwin);
    } /* while (kbinput ...) */

    /* We finished putting in an answer; reset x */
    x = (size_t)-1;

    return kbinput;
}

/* Ask a question on the statusbar.  Answer will be stored in answer
 * global.  Returns -1 on aborted enter, -2 on a blank string, and 0
 * otherwise, the valid shortcut key caught.  def is any editable text
 * we want to put up by default.
 *
 * New arg tabs tells whether or not to allow tab completion. */
int statusq(bool allow_tabs, const shortcut *s, const char *def,
#ifndef NANO_SMALL
		historyheadtype *which_history,
#endif
		const char *msg, ...)
{
    va_list ap;
    char *foo = charalloc(COLS - 3);
    int ret;
#ifndef DISABLE_TABCOMP
    bool list = FALSE;
#endif

    bottombars(s);

    va_start(ap, msg);
    vsnprintf(foo, COLS - 4, msg, ap);
    va_end(ap);
    foo[COLS - 4] = '\0';

    ret = nanogetstr(allow_tabs, foo, def,
#ifndef NANO_SMALL
		which_history,
#endif
		s
#ifndef DISABLE_TABCOMP
		, &list
#endif
		);
    free(foo);
    resetstatuspos = FALSE;

    switch (ret) {
	case NANO_FIRSTLINE_KEY:
	case NANO_FIRSTLINE_FKEY:
	    do_first_line();
	    resetstatuspos = TRUE;
	    break;
	case NANO_LASTLINE_KEY:
	case NANO_LASTLINE_FKEY:
	    do_last_line();
	    resetstatuspos = TRUE;
	    break;
#ifndef DISABLE_JUSTIFY
	case NANO_PARABEGIN_KEY:
	case NANO_PARABEGIN_ALTKEY1:
	case NANO_PARABEGIN_ALTKEY2:
	    do_para_begin();
	    resetstatuspos = TRUE;
	    break;
	case NANO_PARAEND_KEY:
	case NANO_PARAEND_ALTKEY1:
	case NANO_PARAEND_ALTKEY2:
	    do_para_end();
	    resetstatuspos = TRUE;
	    break;
	case NANO_FULLJUSTIFY_KEY:
	case NANO_FULLJUSTIFY_ALTKEY:
	    if (!ISSET(VIEW_MODE))
		do_full_justify();
	    resetstatuspos = TRUE;
	    break;
#endif
	case NANO_CANCEL_KEY:
	    ret = -1;
	    resetstatuspos = TRUE;
	    break;
	case NANO_ENTER_KEY:
	    ret = (answer[0] == '\0') ? -2 : 0;
	    resetstatuspos = TRUE;
	    break;
    }
    blank_statusbar();

#ifdef DEBUG
    fprintf(stderr, "I got \"%s\"\n", answer);
#endif

#ifndef DISABLE_TABCOMP
	/* if we've done tab completion, there might be a list of
	   filename matches on the edit window at this point; make sure
	   they're cleared off. */
	if (list)
	    edit_refresh();
#endif

    return ret;
}

void statusq_abort(void)
{
    resetstatuspos = TRUE;
}

void titlebar(const char *path)
{
    size_t space;
	/* The space we have available for display. */
    size_t verlen = strlen(VERMSG) + 1;
	/* The length of the version message. */
    const char *prefix;
	/* "File:", "Dir:", or "New Buffer".  Goes before filename. */
    size_t prefixlen;
	/* strlen(prefix) + 1. */
    const char *state;
	/* "Modified", "View", or spaces the length of "Modified".
	 * Tells the state of this buffer. */
    size_t statelen = 0;
	/* strlen(state) + 1. */
    char *exppath = NULL;
	/* The file name, expanded for display. */
    size_t explen = 0;
	/* strlen(exppath) + 1. */
    int newbuffer = FALSE;
	/* Do we say "New Buffer"? */
    int dots = FALSE;
	/* Do we put an ellipsis before the path? */

    assert(path != NULL || filename != NULL);
    assert(COLS >= 0);

    wattron(topwin, A_REVERSE);

    blank_titlebar();

    if (COLS <= 5 || COLS - 5 < verlen)
	space = 0;
    else {
	space = COLS - 5 - verlen;
	/* Reserve 2/3 of the screen plus one column for after the
	 * version message. */
	if (space < COLS - (COLS / 3) + 1)
	    space = COLS - (COLS / 3) + 1;
    }

    if (COLS > 4) {
	/* The version message should only take up 1/3 of the screen
	 * minus one column. */
	mvwaddnstr(topwin, 0, 2, VERMSG, (COLS / 3) - 3);
	waddstr(topwin, "  ");
    }

    if (ISSET(MODIFIED))
	state = _("Modified");
    else if (path == NULL && ISSET(VIEW_MODE))
	state = _("View");
    else {
	if (space > 0)
	    statelen = strnlen(_("Modified"), space - 1) + 1;
	state = &hblank[COLS - statelen];
    }
    statelen = strnlen(state, COLS);
    /* We need a space before state. */
    if ((ISSET(MODIFIED) || ISSET(VIEW_MODE)) && statelen < COLS)
	statelen++;

    assert(space >= 0);
    if (space == 0 || statelen >= space)
	goto the_end;

#ifndef DISABLE_BROWSER
    if (path != NULL)
	prefix = _("DIR:");
    else
#endif
    if (filename[0] == '\0') {
	prefix = _("New Buffer");
	newbuffer = TRUE;
    } else
	prefix = _("File:");
    assert(statelen < space);
    prefixlen = strnlen(prefix, space - statelen);
    /* If newbuffer is FALSE, we need a space after prefix. */
    if (!newbuffer && prefixlen + statelen < space)
	prefixlen++;

    if (path == NULL)
	path = filename;
    space -= prefixlen + statelen;
	/* space is now the room we have for the file name. */
    if (!newbuffer) {
	size_t lenpt = strlenpt(path), start_col;

	if (lenpt > space)
	    start_col = actual_x(path, lenpt - space);
	else
	    start_col = 0;
	exppath = display_string(path, start_col, space);
	dots = (lenpt > space);
	explen = strlen(exppath);
    }

    if (!dots) {
	/* There is room for the whole filename, so we center it. */
	waddnstr(topwin, hblank, (space - explen) / 3);
	waddnstr(topwin, prefix, prefixlen);
	if (!newbuffer) {
	    assert(strlen(prefix) + 1 == prefixlen);
	    waddch(topwin, ' ');
	    waddstr(topwin, exppath);
	}
    } else {
	/* We will say something like "File: ...ename". */
	waddnstr(topwin, prefix, prefixlen);
	if (space <= 0 || newbuffer)
	    goto the_end;
	waddch(topwin, ' ');
	waddnstr(topwin, "...", space);
	if (space <= 3)
	    goto the_end;
	space -= 3;
	assert(explen = space + 3);
	waddnstr(topwin, exppath + 3, space);
    }

  the_end:

    free(exppath);

    if (COLS <= 1 || statelen >= COLS - 1)
	mvwaddnstr(topwin, 0, 0, state, COLS);
    else {
	assert(COLS - statelen - 2 >= 0);
	mvwaddch(topwin, 0, COLS - statelen - 2, ' ');
	mvwaddnstr(topwin, 0, COLS - statelen - 1, state, statelen);
    }

    wattroff(topwin, A_REVERSE);

    wnoutrefresh(topwin);
    reset_cursor();
    wrefresh(edit);
}

/* If modified is not already set, set it and update titlebar. */
void set_modified(void)
{
    if (!ISSET(MODIFIED)) {
	SET(MODIFIED);
	titlebar(NULL);
    }
}

void statusbar(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);

    /* Curses mode is turned off.  If we use wmove() now, it will muck
     * up the terminal settings.  So we just use vfprintf(). */
    if (curses_ended) {
	vfprintf(stderr, msg, ap);
	va_end(ap);
	return;
    }

    /* Blank out the line. */
    blank_statusbar();

    if (COLS >= 4) {
	char *bar;
	char *foo;
	size_t start_x = 0, foo_len;
#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
	bool old_whitespace = ISSET(WHITESPACE_DISPLAY);
	UNSET(WHITESPACE_DISPLAY);
#endif
	bar = charalloc(COLS - 3);
	vsnprintf(bar, COLS - 3, msg, ap);
	va_end(ap);
	foo = display_string(bar, 0, COLS - 4);
#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
	if (old_whitespace)
	    SET(WHITESPACE_DISPLAY);
#endif
	free(bar);
	foo_len = strlen(foo);
	start_x = (COLS - foo_len - 4) / 2;

	wmove(bottomwin, 0, start_x);
	wattron(bottomwin, A_REVERSE);

	waddstr(bottomwin, "[ ");
	waddstr(bottomwin, foo);
	free(foo);
	waddstr(bottomwin, " ]");
	wattroff(bottomwin, A_REVERSE);
	wnoutrefresh(bottomwin);
	reset_cursor();
	wrefresh(edit);
	    /* Leave the cursor at its position in the edit window, not
	     * in the statusbar. */
    }

    SET(DISABLE_CURPOS);
    statusblank = 26;
}

void bottombars(const shortcut *s)
{
    size_t i, colwidth, slen;

    if (ISSET(NO_HELP))
	return;

    if (s == main_list) {
	slen = MAIN_VISIBLE;
	assert(slen <= length_of_list(s));
    } else {
	slen = length_of_list(s);

	/* Don't show any more shortcuts than the main list does. */
	if (slen > MAIN_VISIBLE)
	    slen = MAIN_VISIBLE;
    }

    /* There will be this many characters per column.  We need at least
     * 3 to display anything properly.*/
    colwidth = COLS / ((slen / 2) + (slen % 2));

    blank_bottombars();

    for (i = 0; i < slen; i++, s = s->next) {
	const char *keystr;

	/* Yucky sentinel values we can't handle a better way. */
#ifndef NANO_SMALL
	if (s->ctrlval == NANO_HISTORY_KEY)
	    keystr = _("Up");
	else {
#endif
	    char foo[4];

	    if (s->ctrlval == NANO_CONTROL_SPACE)
		strcpy(foo, "^ ");
	    else if (s->ctrlval == NANO_CONTROL_8)
		strcpy(foo, "^?");
	    /* Normal values.  Assume that the shortcut has an
	     * equivalent control key, meta key sequence, or both. */
	    else if (s->ctrlval != NANO_NO_KEY)
		sprintf(foo, "^%c", s->ctrlval + 64);
	    else if (s->metaval != NANO_NO_KEY)
		sprintf(foo, "M-%c", toupper(s->metaval));

	    keystr = foo;
#ifndef NANO_SMALL
	}
#endif

	wmove(bottomwin, 1 + i % 2, (i / 2) * colwidth);
	onekey(keystr, s->desc, colwidth);
    }

    wnoutrefresh(bottomwin);
    reset_cursor();
    wrefresh(edit);
}

/* Write a shortcut key to the help area at the bottom of the window.
 * keystroke is e.g. "^G" and desc is e.g. "Get Help".  We are careful
 * to write at most len characters, even if len is very small and
 * keystroke and desc are long.  Note that waddnstr(,,(size_t)-1) adds
 * the whole string!  We do not bother padding the entry with blanks. */
void onekey(const char *keystroke, const char *desc, size_t len)
{
    assert(keystroke != NULL && desc != NULL && len >= 0);
    wattron(bottomwin, A_REVERSE);
    waddnstr(bottomwin, keystroke, len);
    wattroff(bottomwin, A_REVERSE);
    len -= strlen(keystroke) + 1;
    if (len > 0) {
	waddch(bottomwin, ' ');
	waddnstr(bottomwin, desc, len);
    }
}

/* And so start the display update routines. */

#ifndef NDEBUG
int check_linenumbers(const filestruct *fileptr)
{
    int check_line = 0;
    const filestruct *filetmp;

    for (filetmp = edittop; filetmp != fileptr; filetmp = filetmp->next)
	check_line++;
    return check_line;
}
#endif

/* nano scrolls horizontally within a line in chunks.  This function
 * returns the column number of the first character displayed in the
 * window when the cursor is at the given column.  Note that
 * 0 <= column - get_page_start(column) < COLS. */
size_t get_page_start(size_t column)
{
    assert(COLS > 0);
    if (column == 0 || column < COLS - 1)
	return 0;
    else if (COLS > 9)
	return column - 7 - (column - 7) % (COLS - 8);
    else if (COLS > 2)
	return column - (COLS - 2);
    else
	return column - (COLS - 1);
		/* The parentheses are necessary to avoid overflow. */
}

/* Resets current_y, based on the position of current, and puts the
 * cursor in the edit window at (current_y, current_x). */
void reset_cursor(void)
{
    /* If we haven't opened any files yet, put the cursor in the top
     * left corner of the edit window and get out. */
    if (edittop == NULL || current == NULL) {
	wmove(edit, 0, 0);
	return;
    }

    current_y = current->lineno - edittop->lineno;
    if (current_y < editwinrows) {
	size_t x = xplustabs();
	wmove(edit, current_y, x - get_page_start(x));
     }
}

/* edit_add() takes care of the job of actually painting a line into the
 * edit window.  fileptr is the line to be painted, at row yval of the
 * window.  converted is the actual string to be written to the window,
 * with tabs and control characters replaced by strings of regular
 * characters.  start is the column number of the first character of
 * this page.  That is, the first character of converted corresponds to
 * character number actual_x(fileptr->data, start) of the line. */
void edit_add(const filestruct *fileptr, const char *converted, int
	yval, size_t start)
{
#if defined(ENABLE_COLOR) || !defined(NANO_SMALL)
    size_t startpos = actual_x(fileptr->data, start);
	/* The position in fileptr->data of the leftmost character
	 * that displays at least partially on the window. */
    size_t endpos = actual_x(fileptr->data, start + COLS - 1) + 1;
	/* The position in fileptr->data of the first character that is
	 * completely off the window to the right.
	 *
	 * Note that endpos might be beyond the null terminator of the
	 * string. */
#endif

    assert(fileptr != NULL && converted != NULL);
    assert(strlen(converted) <= COLS);

    /* Just paint the string in any case (we'll add color or reverse on
     * just the text that needs it). */
    mvwaddstr(edit, yval, 0, converted);

#ifdef ENABLE_COLOR
    if (colorstrings != NULL && ISSET(COLOR_SYNTAX)) {
	const colortype *tmpcolor = colorstrings;

	for (; tmpcolor != NULL; tmpcolor = tmpcolor->next) {
	    int x_start;
		/* Starting column for mvwaddnstr.  Zero-based. */
	    int paintlen;
		/* Number of chars to paint on this line.  There are COLS
		 * characters on a whole line. */
	    regmatch_t startmatch;	/* match position for start_regexp */
	    regmatch_t endmatch;	/* match position for end_regexp */

	    if (tmpcolor->bright)
		wattron(edit, A_BOLD);
	    wattron(edit, COLOR_PAIR(tmpcolor->pairnum));
	    /* Two notes about regexec().  Return value 0 means there is
	     * a match.  Also, rm_eo is the first non-matching character
	     * after the match. */

	    /* First case, tmpcolor is a single-line expression. */
	    if (tmpcolor->end == NULL) {
		size_t k = 0;

		/* We increment k by rm_eo, to move past the end of the
		 * last match.  Even though two matches may overlap, we
		 * want to ignore them, so that we can highlight
		 * C-strings correctly. */
		while (k < endpos) {
		    /* Note the fifth parameter to regexec().  It says
		     * not to match the beginning-of-line character
		     * unless k is 0.  If regexec() returns REG_NOMATCH,
		     * there are no more matches in the line. */
		    if (regexec(&tmpcolor->start, &fileptr->data[k], 1,
			&startmatch, k == 0 ? 0 : REG_NOTBOL) == REG_NOMATCH)
			break;
		    /* Translate the match to the beginning of the line. */
		    startmatch.rm_so += k;
		    startmatch.rm_eo += k;
		    if (startmatch.rm_so == startmatch.rm_eo) {
			startmatch.rm_eo++;
			statusbar(_("Refusing 0 length regex match"));
		    } else if (startmatch.rm_so < endpos &&
				startmatch.rm_eo > startpos) {
			if (startmatch.rm_so <= startpos)
			    x_start = 0;
			else
			    x_start = strnlenpt(fileptr->data,
				startmatch.rm_so) - start;
			paintlen = strnlenpt(fileptr->data,
				startmatch.rm_eo) - start - x_start;
			if (paintlen > COLS - x_start)
			    paintlen = COLS - x_start;

			assert(0 <= x_start && 0 < paintlen &&
				x_start + paintlen <= COLS);
			mvwaddnstr(edit, yval, x_start,
				converted + x_start, paintlen);
		    }
		    k = startmatch.rm_eo;
		}
	    } else {
		/* This is a multi-line regexp.  There are two steps. 
		 * First, we have to see if the beginning of the line is
		 * colored by a start on an earlier line, and an end on
		 * this line or later.
		 *
		 * We find the first line before fileptr matching the
		 * start.  If every match on that line is followed by an
		 * end, then go to step two.  Otherwise, find the next line
		 * after start_line matching the end.  If that line is not
		 * before fileptr, then paint the beginning of this line. */

		const filestruct *start_line = fileptr->prev;
		    /* the first line before fileptr matching start */
		regoff_t start_col;
		    /* where it starts in that line */
		const filestruct *end_line;

		while (start_line != NULL &&
			regexec(&tmpcolor->start, start_line->data, 1,
			&startmatch, 0) == REG_NOMATCH) {
		    /* If there is an end on this line, there is no need
		     * to look for starts on earlier lines. */
		    if (regexec(tmpcolor->end, start_line->data, 0,
			NULL, 0) == 0)
			goto step_two;
		    start_line = start_line->prev;
		}
		/* No start found, so skip to the next step. */
		if (start_line == NULL)
		    goto step_two;
		/* Now start_line is the first line before fileptr
		 * containing a start match.  Is there a start on this
		 * line not followed by an end on this line? */

		start_col = 0;
		while (TRUE) {
		    start_col += startmatch.rm_so;
		    startmatch.rm_eo -= startmatch.rm_so;
 		    if (regexec(tmpcolor->end,
			start_line->data + start_col + startmatch.rm_eo,
			0, NULL, start_col + startmatch.rm_eo == 0 ? 0 :
			REG_NOTBOL) == REG_NOMATCH)
			/* No end found after this start. */
			break;
		    start_col++;
		    if (regexec(&tmpcolor->start,
			start_line->data + start_col, 1,
			&startmatch, REG_NOTBOL) == REG_NOMATCH)
			/* No later start on this line. */
			goto step_two;
		}
		/* Indeed, there is a start not followed on this line by
		 * an end. */

		/* We have already checked that there is no end before
		 * fileptr and after the start.  Is there an end after
		 * the start at all?  We don't paint unterminated
		 * starts. */
		end_line = fileptr;
		while (end_line != NULL &&
			regexec(tmpcolor->end, end_line->data, 1,
			&endmatch, 0) == REG_NOMATCH)
		    end_line = end_line->next;

		/* No end found, or it is too early. */
		if (end_line == NULL ||
			(end_line == fileptr && endmatch.rm_eo <= startpos))
		    goto step_two;

		/* Now paint the start of fileptr. */
		paintlen = end_line != fileptr ? COLS :
			strnlenpt(fileptr->data, endmatch.rm_eo) - start;
		if (paintlen > COLS)
		    paintlen = COLS;

		assert(0 < paintlen && paintlen <= COLS);
		mvwaddnstr(edit, yval, 0, converted, paintlen);

		/* We have already painted the whole line. */
		if (paintlen == COLS)
		    goto skip_step_two;

  step_two:
		/* Second step, we look for starts on this line. */
		start_col = 0;
		while (start_col < endpos) {
		    if (regexec(&tmpcolor->start,
			fileptr->data + start_col, 1, &startmatch,
			start_col == 0 ? 0 : REG_NOTBOL) == REG_NOMATCH ||
			start_col + startmatch.rm_so >= endpos)
			/* No more starts on this line. */
			break;
		    /* Translate the match to be relative to the
		     * beginning of the line. */
		    startmatch.rm_so += start_col;
		    startmatch.rm_eo += start_col;

		    if (startmatch.rm_so <= startpos)
			x_start = 0;
		    else
			x_start = strnlenpt(fileptr->data,
				startmatch.rm_so) - start;
		    if (regexec(tmpcolor->end,
			fileptr->data + startmatch.rm_eo, 1, &endmatch,
			startmatch.rm_eo == 0 ? 0 : REG_NOTBOL) == 0) {
			/* Translate the end match to be relative to the
			 * beginning of the line. */
			endmatch.rm_so += startmatch.rm_eo;
			endmatch.rm_eo += startmatch.rm_eo;
			/* There is an end on this line.  But does it
			 * appear on this page, and is the match more
			 * than zero characters long? */
			if (endmatch.rm_eo > startpos &&
				endmatch.rm_eo > startmatch.rm_so) {
			    paintlen = strnlenpt(fileptr->data,
				endmatch.rm_eo) - start - x_start;
			    if (x_start + paintlen > COLS)
				paintlen = COLS - x_start;

			    assert(0 <= x_start && 0 < paintlen &&
				x_start + paintlen <= COLS);
			    mvwaddnstr(edit, yval, x_start,
				converted + x_start, paintlen);
			}
		    } else {
			/* There is no end on this line.  But we haven't
			 * yet looked for one on later lines. */
			end_line = fileptr->next;
			while (end_line != NULL &&
				regexec(tmpcolor->end, end_line->data, 0,
				NULL, 0) == REG_NOMATCH)
			    end_line = end_line->next;
			if (end_line != NULL) {
			    assert(0 <= x_start && x_start < COLS);
			    mvwaddnstr(edit, yval, x_start,
				converted + x_start, COLS - x_start);
			    /* We painted to the end of the line, so
			     * don't bother checking any more starts. */
			    break;
			}
		    }
		    start_col = startmatch.rm_so + 1;
		} /* while start_col < endpos */
	    } /* if (tmp_color->end != NULL) */

  skip_step_two:
	    wattroff(edit, A_BOLD);
	    wattroff(edit, COLOR_PAIR(tmpcolor->pairnum));
	} /* for tmpcolor in colorstrings */
    }
#endif				/* ENABLE_COLOR */

#ifndef NANO_SMALL
    if (ISSET(MARK_ISSET)
	    && (fileptr->lineno <= mark_beginbuf->lineno
		|| fileptr->lineno <= current->lineno)
	    && (fileptr->lineno >= mark_beginbuf->lineno
		|| fileptr->lineno >= current->lineno)) {
	/* fileptr is at least partially selected. */

	const filestruct *top;
	    /* Either current or mark_beginbuf, whichever is first. */
	size_t top_x;
	    /* current_x or mark_beginx, corresponding to top. */
	const filestruct *bot;
	size_t bot_x;
	int x_start;
	    /* Starting column for mvwaddnstr.  Zero-based. */
	int paintlen;
	    /* Number of chars to paint on this line.  There are COLS
	     * characters on a whole line. */

	mark_order(&top, &top_x, &bot, &bot_x, NULL);

	if (top->lineno < fileptr->lineno || top_x < startpos)
	    top_x = startpos;
	if (bot->lineno > fileptr->lineno || bot_x > endpos)
	    bot_x = endpos;

	/* The selected bit of fileptr is on this page. */
	if (top_x < endpos && bot_x > startpos) {
	    assert(startpos <= top_x);

	    /* x_start is the expanded location of the beginning of the
	     * mark minus the beginning of the page. */
	    x_start = strnlenpt(fileptr->data, top_x) - start;

	    if (bot_x >= endpos)
		/* If the end of the mark is off the page, paintlen is
		 * -1, meaning that everything on the line gets
		 * painted. */
		paintlen = -1;
	    else
		/* Otherwise, paintlen is the expanded location of the
		 * end of the mark minus the expanded location of the
		 * beginning of the mark. */
		paintlen = strnlenpt(fileptr->data, bot_x)
			- (x_start + start);

	    /* If x_start is before the beginning of the page, shift
	     * paintlen x_start characters to compensate, and put
	     * x_start at the beginning of the page. */
	    if (x_start < 0) {
		paintlen += x_start;
		x_start = 0;
	    }

	    assert(x_start >= 0 && x_start <= strlen(converted));

	    wattron(edit, A_REVERSE);
	    mvwaddnstr(edit, yval, x_start, converted + x_start, paintlen);
	    wattroff(edit, A_REVERSE);
	}
    }
#endif /* !NANO_SMALL */
}

/* Just update one line in the edit buffer.  Basically a wrapper for
 * edit_add().
 *
 * If fileptr != current, then index is considered 0.  The line will be
 * displayed starting with fileptr->data[index].  Likely args are
 * current_x or 0. */
void update_line(const filestruct *fileptr, size_t index)
{
    int line;
	/* line in the edit window for CURSES calls */
    char *converted;
	/* fileptr->data converted to have tabs and control characters
	 * expanded. */
    size_t page_start;

    assert(fileptr != NULL);

    line = fileptr->lineno - edittop->lineno;

    /* We assume the line numbers are valid.  Is that really true? */
    assert(line < 0 || line == check_linenumbers(fileptr));

    if (line < 0 || line >= editwinrows)
	return;

    /* First, blank out the line (at a minimum) */
    mvwaddstr(edit, line, 0, hblank);

    /* Next, convert variables that index the line to their equivalent
     * positions in the expanded line. */
    index = (fileptr == current) ? strnlenpt(fileptr->data, index) : 0;
    page_start = get_page_start(index);

    /* Expand the line, replacing Tab by spaces, and control characters
     * by their display form. */
    converted = display_string(fileptr->data, page_start, COLS);

    /* Now, paint the line */
    edit_add(fileptr, converted, line, page_start);
    free(converted);

    if (page_start > 0)
	mvwaddch(edit, line, 0, '$');
    if (strlenpt(fileptr->data) > page_start + COLS)
	mvwaddch(edit, line, COLS - 1, '$');
}

/* Return a nonzero value if we need an update after moving
 * horizontally.  We need one if the mark is on or if old_pww and
 * placewewant are on different pages. */
int need_horizontal_update(size_t old_pww)
{
    return
#ifndef NANO_SMALL
	ISSET(MARK_ISSET) ||
#endif
	get_page_start(old_pww) != get_page_start(placewewant);
}

/* Return a nonzero value if we need an update after moving vertically.
 * We need one if the mark is on or if old_pww and placewewant
 * are on different pages. */
int need_vertical_update(size_t old_pww)
{
    return
#ifndef NANO_SMALL
	ISSET(MARK_ISSET) ||
#endif
	get_page_start(old_pww) != get_page_start(placewewant);
}

/* Scroll the edit window in the given direction and the given number
 * of lines, and draw new lines on the blank lines left after the
 * scrolling.  direction is the direction to scroll, either UP or DOWN,
 * and nlines is the number of lines to scroll.  Don't redraw the old
 * topmost or bottommost line (where we assume current is) before
 * scrolling or draw the new topmost or bottommost line after scrolling
 * (where we assume current will be), since we don't know where we are
 * on the page or whether we'll stay there. */
void edit_scroll(updown direction, int nlines)
{
    filestruct *foo;
    int i, scroll_rows = 0;

    /* Scrolling less than one line or more than editwinrows lines is
     * redundant, so don't allow it. */
    if (nlines < 1 || nlines > editwinrows)
	return;

    /* Move the top line of the edit window up or down (depending on the
     * value of direction) nlines lines.  If there are fewer lines of
     * text than that left, move it to the top or bottom line of the
     * file (depending on the value of direction).  Keep track of
     * how many lines we moved in scroll_rows. */
    for (i = nlines; i > 0; i--) {
	if (direction == UP) {
	    if (edittop->prev == NULL)
		break;
	    edittop = edittop->prev;
	    scroll_rows--;
	} else {
	    if (edittop->next == NULL)
		break;
	    edittop = edittop->next;
	    scroll_rows++;
	}
    }

    /* Scroll the text on the screen up or down scroll_rows lines,
     * depending on the value of direction. */
    scrollok(edit, TRUE);
    wscrl(edit, scroll_rows);
    scrollok(edit, FALSE);

    foo = edittop;
    if (direction != UP) {
	int slines = editwinrows - nlines;
	for (; slines > 0 && foo != NULL; slines--)
	    foo = foo->next;
    }

    /* And draw new lines on the blank top or bottom lines of the edit
     * window, depending on the value of direction.  Don't draw the new
     * topmost or new bottommost line. */
    while (scroll_rows != 0 && foo != NULL) {
	if (foo->next != NULL)
	    update_line(foo, 0);
	if (direction == UP)
	    scroll_rows++;
	else
	    scroll_rows--;
	foo = foo->next;
    }
}

/* Update any lines between old_current and current that need to be
 * updated. */
void edit_redraw(const filestruct *old_current, size_t old_pww)
{
    int do_refresh = need_vertical_update(0) ||
	need_vertical_update(old_pww);
    const filestruct *foo;

    /* If either old_current or current is offscreen, refresh the screen
     * and get out. */
    if (old_current->lineno < edittop->lineno || old_current->lineno >=
	edittop->lineno + editwinrows || current->lineno <
	edittop->lineno || current->lineno >= edittop->lineno +
	editwinrows) {
	edit_refresh();
	return;
    }

    /* Update old_current and current if we're not on the first page
     * and/or we're not on the same page as before.  If the mark is on,
     * update all the lines between old_current and current too. */
    foo = old_current;
    while (foo != current) {
	if (do_refresh)
	    update_line(foo, 0);
#ifndef NANO_SMALL
	if (!ISSET(MARK_ISSET))
#endif
	    break;
	if (foo->lineno > current->lineno)
	    foo = foo->prev;
	else
	    foo = foo->next;
    }
    if (do_refresh)
	update_line(current, current_x);
}

/* Refresh the screen without changing the position of lines. */
void edit_refresh(void)
{
    if (current->lineno < edittop->lineno ||
	    current->lineno >= edittop->lineno + editwinrows)
	/* Note that edit_update() changes edittop so that it's in range
	 * of current.  Thus, when it then calls edit_refresh(), there
	 * is no danger of getting an infinite loop. */
	edit_update(
#ifndef NANO_SMALL
		ISSET(SMOOTHSCROLL) ? NONE :
#endif
		CENTER);
    else {
	int nlines = 0;
	const filestruct *foo = edittop;

#ifdef DEBUG
	fprintf(stderr, "edit_refresh(): edittop->lineno = %d\n", edittop->lineno);
#endif

	while (nlines < editwinrows) {
	    update_line(foo, foo == current ? current_x : 0);
	    nlines++;
	    if (foo->next == NULL)
		break;
	    foo = foo->next;
	}
	while (nlines < editwinrows) {
	    mvwaddstr(edit, nlines, 0, hblank);
	    nlines++;
	}
	reset_cursor();
	wrefresh(edit);
    }
}

/* A nice generic routine to update the edit buffer.  We keep current in
 * the same place and move edittop to put it in range of current. */
void edit_update(topmidnone location)
{
    filestruct *foo = current;

    if (location != TOP) {
	/* If location is CENTER, we move edittop up (editwinrows / 2)
	 * lines.  This puts current at the center of the screen.  If
	 * location is NONE, we move edittop up current_y lines if
	 * current_y is in range of the screen, 0 lines if current_y is
	 * less than 0, or (editwinrows - 1) lines if current_y is
	 * greater than (editwinrows - 1).  This puts current at the
	 * same place on the screen as before, or at the top or bottom
	 * of the screen if edittop is beyond either. */
	int goal;

	if (location == CENTER)
	    goal = editwinrows / 2;
	else {
	    goal = current_y;

	    /* Limit goal to (editwinrows - 1) lines maximum. */
	    if (goal > editwinrows - 1)
		goal = editwinrows - 1;
	}

	for (; goal > 0 && foo->prev != NULL; goal--)
	    foo = foo->prev;
    }

    edittop = foo;
    edit_refresh();
}

/* Ask a simple yes/no question, specified in msg, on the statusbar.
 * Return 1 for Y, 0 for N, 2 for All (if all is TRUE when passed in)
 * and -1 for abort (^C). */
int do_yesno(bool all, const char *msg)
{
    int ok = -2, width = 16;
    const char *yesstr;		/* String of yes characters accepted. */
    const char *nostr;		/* Same for no. */
    const char *allstr;		/* And all, surprise! */

    /* yesstr, nostr, and allstr are strings of any length.  Each string
     * consists of all characters accepted as a valid character for that
     * value.  The first value will be the one displayed in the
     * shortcuts.  Translators: if possible, specify both the shortcuts
     * for your language and English.  For example, in French: "OoYy"
     * for "Oui". */
    yesstr = _("Yy");
    nostr = _("Nn");
    allstr = _("Aa");

    if (!ISSET(NO_HELP)) {
	char shortstr[3];		/* Temp string for Y, N, A. */

	if (COLS < 32)
	    width = COLS / 2;

	/* Write the bottom of the screen. */
	blank_bottombars();

	sprintf(shortstr, " %c", yesstr[0]);
	wmove(bottomwin, 1, 0);
	onekey(shortstr, _("Yes"), width);

	if (all) {
	    wmove(bottomwin, 1, width);
	    shortstr[1] = allstr[0];
	    onekey(shortstr, _("All"), width);
	}

	wmove(bottomwin, 2, 0);
	shortstr[1] = nostr[0];
	onekey(shortstr, _("No"), width);

	wmove(bottomwin, 2, 16);
	onekey("^C", _("Cancel"), width);
    }

    wattron(bottomwin, A_REVERSE);

    blank_statusbar();
    mvwaddnstr(bottomwin, 0, 0, msg, COLS - 1);

    wattroff(bottomwin, A_REVERSE);

    wrefresh(bottomwin);

    do {
	int kbinput;
	bool meta_key, func_key;
#ifndef DISABLE_MOUSE
	int mouse_x, mouse_y;
#endif

	kbinput = get_kbinput(edit, &meta_key, &func_key);

	if (kbinput == NANO_CANCEL_KEY)
	    ok = -1;
#ifndef DISABLE_MOUSE
	/* Look, ma!  We get to duplicate lots of code from
	 * do_mouse()!! */
	else if (kbinput == KEY_MOUSE) {
	    get_mouseinput(&mouse_x, &mouse_y, FALSE);

	    if (mouse_x != -1 && mouse_y != -1 && !ISSET(NO_HELP) &&
		wenclose(bottomwin, mouse_y, mouse_x) &&
		mouse_x < (width * 2) && mouse_y >= editwinrows + 3) {
		int x = mouse_x / width;
		    /* Did we click in the first column of shortcuts, or
		     * the second? */
		int y = mouse_y - editwinrows - 3;
		    /* Did we click in the first row of shortcuts? */

		assert(0 <= x && x <= 1 && 0 <= y && y <= 1);

		/* x = 0 means they clicked Yes or No.
		 * y = 0 means Yes or All. */
		ok = -2 * x * y + x - y + 1;

		if (ok == 2 && !all)
		    ok = -2;
	    }
	}
#endif
	/* Look for the kbinput in the yes, no and (optionally) all
	 * strings. */
	else if (strchr(yesstr, kbinput) != NULL)
	    ok = 1;
	else if (strchr(nostr, kbinput) != NULL)
	    ok = 0;
	else if (all && strchr(allstr, kbinput) != NULL)
	    ok = 2;
    } while (ok == -2);

    return ok;
}

void total_refresh(void)
{
    clearok(topwin, TRUE);
    clearok(edit, TRUE);
    clearok(bottomwin, TRUE);
    wnoutrefresh(topwin);
    wnoutrefresh(edit);
    wnoutrefresh(bottomwin);
    doupdate();
    clearok(topwin, FALSE);
    clearok(edit, FALSE);
    clearok(bottomwin, FALSE);
    titlebar(NULL);
    edit_refresh();
    /* FIXME: bottomwin needs to be refreshed too. */
}

void display_main_list(void)
{
    bottombars(main_list);
}

/* If constant is FALSE, the user typed Ctrl-C, so we unconditionally
 * display the cursor position.  Otherwise, we display it only if the
 * character position changed and DISABLE_CURPOS is not set.
 *
 * If constant is TRUE and DISABLE_CURPOS is set, we unset it and update
 * old_i and old_totsize.  That way, we leave the current statusbar
 * alone, but next time we will display. */
void do_cursorpos(bool constant)
{
    const filestruct *fileptr;
    size_t i = 0;
    static size_t old_i = 0;
    static long old_totsize = -1;

    assert(current != NULL && fileage != NULL && totlines != 0);

    if (old_totsize == -1)
	old_totsize = totsize;

    for (fileptr = fileage; fileptr != current; fileptr = fileptr->next) {
	assert(fileptr != NULL);
	i += strlen(fileptr->data) + 1;
    }
    i += current_x;

    /* Check whether totsize is correct.  Else there is a bug
     * somewhere. */
    assert(current != filebot || i == totsize);

    if (constant && ISSET(DISABLE_CURPOS)) {
	UNSET(DISABLE_CURPOS);
	old_i = i;
	old_totsize = totsize;
	return;
    }

    /* If constant is FALSE, display the position on the statusbar
     * unconditionally; otherwise, only display the position when the
     * character values have changed. */
    if (!constant || old_i != i || old_totsize != totsize) {
	size_t xpt = xplustabs() + 1;
	size_t cur_len = strlenpt(current->data) + 1;
	int linepct = 100 * current->lineno / totlines;
	int colpct = 100 * xpt / cur_len;
	int bytepct = totsize == 0 ? 0 : 100 * i / totsize;

	statusbar(
	    _("line %ld/%ld (%d%%), col %lu/%lu (%d%%), char %lu/%ld (%d%%)"),
		    current->lineno, totlines, linepct,
		    (unsigned long)xpt, (unsigned long)cur_len, colpct,
		    (unsigned long)i, totsize, bytepct);
	UNSET(DISABLE_CURPOS);
    }

    old_i = i;
    old_totsize = totsize;
}

void do_cursorpos_void(void)
{
    do_cursorpos(FALSE);
}

#ifndef DISABLE_HELP
/* Calculate the next line of help_text, starting at ptr. */
int help_line_len(const char *ptr)
{
    int j = 0;

    while (*ptr != '\n' && *ptr != '\0' && j < COLS - 5) {
	ptr++;
	j++;
    }
    if (j == COLS - 5) {
	/* Don't wrap at the first of two spaces following a period. */
	if (*ptr == ' ' && *(ptr + 1) == ' ')
	    j++;
	/* Don't print half a word if we've run out of space. */
	while (*ptr != ' ' && j > 0) {
	    ptr--;
	    j--;
	}
	/* Word longer than COLS - 5 chars just gets broken. */
	if (j == 0)
	    j = COLS - 5;
    }
    assert(j >= 0 && j <= COLS - 4 && (j > 0 || *ptr == '\n'));
    return j;
}

/* Our dynamic, shortcut-list-compliant help function. */
void do_help(void)
{
    int line = 0;
	/* The line number in help_text of the first displayed help line.
	 * This variable is zero-based. */
    bool no_more = FALSE;
	/* no_more means the end of the help text is shown, so don't go
	 * down any more. */
    int kbinput = ERR;
    bool meta_key, func_key;

    bool old_no_help = ISSET(NO_HELP);
#ifndef DISABLE_MOUSE
    const shortcut *oldshortcut = currshortcut;
	/* We will set currshortcut to allow clicking on the help
	 * screen's shortcut list. */
#endif

    curs_set(0);
    blank_edit();
    wattroff(bottomwin, A_REVERSE);
    blank_statusbar();

    /* Set help_text as the string to display. */
    help_init();
    assert(help_text != NULL);

#ifndef DISABLE_MOUSE
    /* Set currshortcut to allow clicking on the help screen's shortcut
     * list, AFTER help_init(). */
    currshortcut = help_list;
#endif

    if (ISSET(NO_HELP)) {
	/* Make sure that the help screen's shortcut list will actually
	 * be displayed. */
	UNSET(NO_HELP);
	window_init();
    }
    bottombars(help_list);

    do {
	int i;
	int old_line = line;
	    /* We redisplay the help only if it moved. */
	const char *ptr = help_text;

	switch (kbinput) {
#ifndef DISABLE_MOUSE
	    case KEY_MOUSE:
		{
		    int mouse_x, mouse_y;
		    get_mouseinput(&mouse_x, &mouse_y, TRUE);
		}
		break;
#endif
	    case NANO_NEXTPAGE_KEY:
	    case NANO_NEXTPAGE_FKEY:
		if (!no_more)
		    line += editwinrows - 2;
		break;
	    case NANO_PREVPAGE_KEY:
	    case NANO_PREVPAGE_FKEY:
		if (line > 0) {
		    line -= editwinrows - 2;
		    if (line < 0)
			line = 0;
		}
		break;
	    case NANO_PREVLINE_KEY:
		if (line > 0)
		    line--;
		break;
	    case NANO_NEXTLINE_KEY:
		if (!no_more)
		    line++;
		break;
	}

	if (line == old_line && kbinput != ERR)
	    goto skip_redisplay;

	blank_edit();

	assert(COLS > 5);

	/* Calculate where in the text we should be, based on the
	 * page. */
	for (i = 0; i < line; i++) {
	    ptr += help_line_len(ptr);
	    if (*ptr == '\n')
		ptr++;
	}

	for (i = 0; i < editwinrows && *ptr != '\0'; i++) {
	    int j = help_line_len(ptr);

	    mvwaddnstr(edit, i, 0, ptr, j);
	    ptr += j;
	    if (*ptr == '\n')
		ptr++;
	}
	no_more = (*ptr == '\0');

  skip_redisplay:
	kbinput = get_kbinput(edit, &meta_key, &func_key);
    } while (kbinput != NANO_CANCEL_KEY && kbinput != NANO_EXIT_KEY &&
	kbinput != NANO_EXIT_FKEY);

#ifndef DISABLE_MOUSE
    currshortcut = oldshortcut;
#endif

    if (old_no_help) {
	blank_bottombars();
	wrefresh(bottomwin);
	SET(NO_HELP);
	window_init();
    } else
	bottombars(currshortcut);

    curs_set(1);
    edit_refresh();

    /* The help_init() at the beginning allocated help_text.  Since 
     * help_text has now been written to the screen, we don't need it
     * anymore. */
    free(help_text);
    help_text = NULL;
}
#endif /* !DISABLE_HELP */

/* Highlight the current word being replaced or spell checked.  We
 * expect word to have tabs and control characters expanded. */
void do_replace_highlight(int highlight_flag, const char *word)
{
    size_t y = xplustabs();
    size_t word_len = strlen(word);

    y = get_page_start(y) + COLS - y;
	/* Now y is the number of characters we can display on this
	 * line. */

    reset_cursor();

    if (highlight_flag)
	wattron(edit, A_REVERSE);

#ifdef HAVE_REGEX_H
    /* This is so we can show zero-length regexes. */
    if (word_len == 0)
	waddstr(edit, " ");
    else
#endif
	waddnstr(edit, word, y - 1);

    if (word_len > y)
	waddch(edit, '$');
    else if (word_len == y)
	waddch(edit, word[word_len - 1]);

    if (highlight_flag)
	wattroff(edit, A_REVERSE);
}

#ifdef DEBUG
/* Dump the passed-in file structure to stderr. */
void dump_buffer(const filestruct *inptr)
{
    if (inptr == fileage)
	fprintf(stderr, "Dumping file buffer to stderr...\n");
    else if (inptr == cutbuffer)
	fprintf(stderr, "Dumping cutbuffer to stderr...\n");
    else
	fprintf(stderr, "Dumping a buffer to stderr...\n");

    while (inptr != NULL) {
	fprintf(stderr, "(%d) %s\n", inptr->lineno, inptr->data);
	inptr = inptr->next;
    }
}

/* Dump the file structure to stderr in reverse. */
void dump_buffer_reverse(void)
{
    const filestruct *fileptr = filebot;

    while (fileptr != NULL) {
	fprintf(stderr, "(%d) %s\n", fileptr->lineno, fileptr->data);
	fileptr = fileptr->prev;
    }
}
#endif /* DEBUG */

#ifdef NANO_EXTRA
#define CREDIT_LEN 53
#define XLCREDIT_LEN 8

/* Easter egg: Display credits.  Assume nodelay(edit) is FALSE. */
void do_credits(void)
{
    int crpos = 0, xlpos = 0;
    const char *credits[CREDIT_LEN] = {
	NULL,				/* "The nano text editor" */
	NULL,				/* "version" */
	VERSION,
	"",
	NULL,				/* "Brought to you by:" */
	"Chris Allegretta",
	"Jordi Mallach",
	"Adam Rogoyski",
	"Rob Siemborski",
	"Rocco Corsi",
	"David Lawrence Ramsey",
	"David Benbennick",
	"Ken Tyler",
	"Sven Guckes",
	"Florian König",
	"Pauli Virtanen",
	"Daniele Medri",
	"Clement Laforet",
	"Tedi Heriyanto",
	"Bill Soudan",
	"Christian Weisgerber",
	"Erik Andersen",
	"Big Gaute",
	"Joshua Jensen",
	"Ryan Krebs",
	"Albert Chin",
	"",
	NULL,				/* "Special thanks to:" */
	"Plattsburgh State University",
	"Benet Laboratories",
	"Amy Allegretta",
	"Linda Young",
	"Jeremy Robichaud",
	"Richard Kolb II",
	NULL,				/* "The Free Software Foundation" */
	"Linus Torvalds",
	NULL,				/* "For ncurses:" */
	"Thomas Dickey",
	"Pavel Curtis",
	"Zeyd Ben-Halim",
	"Eric S. Raymond",
	NULL,				/* "and anyone else we forgot..." */
	NULL,				/* "Thank you for using nano!" */
	"",
	"",
	"",
	"",
	"(c) 1999-2004 Chris Allegretta",
	"",
	"",
	"",
	"",
	"http://www.nano-editor.org/"
    };

    const char *xlcredits[XLCREDIT_LEN] = {
	N_("The nano text editor"),
	N_("version"),
	N_("Brought to you by:"),
	N_("Special thanks to:"),
	N_("The Free Software Foundation"),
	N_("For ncurses:"),
	N_("and anyone else we forgot..."),
	N_("Thank you for using nano!")
    };

    curs_set(0);
    nodelay(edit, TRUE);
    scrollok(edit, TRUE);
    blank_titlebar();
    blank_edit();
    blank_statusbar();
    blank_bottombars();
    wrefresh(topwin);
    wrefresh(edit);
    wrefresh(bottomwin);

    for (crpos = 0; crpos < CREDIT_LEN + editwinrows / 2; crpos++) {
	if (wgetch(edit) != ERR)
	    break;
	if (crpos < CREDIT_LEN) {
	    const char *what = credits[crpos];
	    size_t start_x;

	    if (what == NULL) {
		assert(0 <= xlpos && xlpos < XLCREDIT_LEN);
		what = _(xlcredits[xlpos]);
		xlpos++;
	    }
	    start_x = COLS / 2 - strlen(what) / 2 - 1;
	    mvwaddstr(edit, editwinrows - 1 - editwinrows % 2, start_x, what);
	}
	napms(700);
	scroll(edit);
	wrefresh(edit);
	if (wgetch(edit) != ERR)
	    break;
	napms(700);
	scroll(edit);
	wrefresh(edit);
    }

    scrollok(edit, FALSE);
    nodelay(edit, FALSE);
    curs_set(1);
    display_main_list();
    total_refresh();
}
#endif
