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

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

#ifdef NANO_EXTRA
#include <time.h>
#endif

static int statblank = 0;	/* Number of keystrokes left after
				   we call statusbar(), before we
				   actually blank the statusbar */

/* Read in a single input character.  If it's ignored, swallow it and go
 * on.  Otherwise, try to translate it from ASCII, extended keypad
 * values, and/or escape sequences.  Supported extended keypad values
 * consist of [arrow key], Ctrl-[arrow key], Shift-[arrow key], Enter,
 * Backspace, Insert, Delete, Home, End, PageUp, PageDown, and F1-F14.
 * Assume nodelay(win) is FALSE. */
int get_kbinput(WINDOW *win, int *meta_key)
{
    int kbinput, retval;

#ifndef NANO_SMALL
    allow_pending_sigwinch(TRUE);
#endif

    kbinput = get_ignored_kbinput(win);
    retval = get_accepted_kbinput(win, kbinput, meta_key);

#ifndef NANO_SMALL
    allow_pending_sigwinch(FALSE);
#endif

    return retval;
}

/* Read in a string of input characters (e.g. an escape sequence)
 * verbatim, and return the length of the string in kbinput_len.  Assume
 * nodelay(win) is FALSE. */
int *get_verbatim_kbinput(WINDOW *win, int *kbinput_len, int
	allow_ascii)
{
    int kbinput, *verbatim_kbinput;

#ifndef NANO_SMALL
    allow_pending_sigwinch(TRUE);
#endif

    /* Switch to raw mode so that we can type ^C, ^Q, ^S, ^Z, and ^\
     * (and ^Y on the Hurd) without getting interrupts, and Turn the
     * keypad off so that we don't get extended keypad values all of
     * which are outside the ASCII range. */
#ifdef _POSIX_VDISABLE
    raw();
#endif
    keypad(win, FALSE);

    kbinput = wgetch(win);
    verbatim_kbinput = (int *)nmalloc(sizeof(int));
    verbatim_kbinput[0] = kbinput;
    *kbinput_len = 1;

    if (allow_ascii && kbinput >= '0' && kbinput <= '2')
	/* Entering a three-digit decimal ASCII code from 000-255 in
	 * verbatim mode will produce the corresponding ASCII
	 * character. */
	verbatim_kbinput[0] = get_ascii_kbinput(win, kbinput);
    else {
	nodelay(win, TRUE);
	while ((kbinput = wgetch(win)) != ERR) {
	    (*kbinput_len)++;
	    verbatim_kbinput = realloc(verbatim_kbinput, *kbinput_len * sizeof(int));
	    verbatim_kbinput[*kbinput_len - 1] = kbinput;
	}
	nodelay(win, FALSE);
    }

    /* Switch back to cbreak mode and turn the keypad back on now that
     * we're done. */
#ifdef _POSIX_VDISABLE
    cbreak();
#endif
    keypad(win, TRUE);

#ifdef DEBUG
    fprintf(stderr, "get_verbatim_kbinput(): verbatim_kbinput = %s\n", verbatim_kbinput);
#endif

#ifndef NANO_SMALL
    allow_pending_sigwinch(FALSE);
#endif

    return verbatim_kbinput;
}

/* Swallow input characters that should be quietly ignored, and return
 * the first input character that shouldn't be. */
int get_ignored_kbinput(WINDOW *win)
{
    int kbinput;

    while (TRUE) {
	kbinput = wgetch(win);
	switch (kbinput) {
	    case ERR:
	    case KEY_RESIZE:
#ifdef PDCURSES
	    case KEY_SHIFT_L:
	    case KEY_SHIFT_R:
	    case KEY_CONTROL_L:
	    case KEY_CONTROL_R:
	    case KEY_ALT_L:
	    case KEY_ALT_R:
#endif
#ifdef DEBUG
		fprintf(stderr, "get_ignored_kbinput(): kbinput = %d\n", kbinput);
#endif
		break;
	    default:
		return kbinput;
	}
    }
}

/* Translate acceptable ASCII, extended keypad values, and/or escape
 * sequences.  Set meta_key to 1 if we get a Meta sequence.  Assume
 * nodelay(win) is FALSE. */
int get_accepted_kbinput(WINDOW *win, int kbinput, int *meta_key)
{
    *meta_key = FALSE;

    switch (kbinput) {
	case NANO_CONTROL_3: /* Escape */
	    kbinput = wgetch(win);
	    switch (kbinput) {
		case NANO_CONTROL_3: /* Escape */
		    kbinput = wgetch(win);
		    /* Esc Esc [three-digit decimal ASCII code from
		     * 000-255] == [corresponding ASCII character];
		     * Esc Esc 2 obviously can't be Ctrl-2 here */
		    if (kbinput >= '0' && kbinput <= '2')
			kbinput = get_ascii_kbinput(win, kbinput);
		    /* Esc Esc [character] == Ctrl-[character];
		     * Ctrl-Space (Ctrl-2) == Ctrl-@ == Ctrl-` */
		    else if (kbinput == ' ' || kbinput == '@' || kbinput == '`')
			kbinput = NANO_CONTROL_SPACE;
		    /* Ctrl-3 (Ctrl-[, Esc) to Ctrl-7 (Ctrl-_) */
		    else if (kbinput >= '3' && kbinput <= '7')
			kbinput -= 24;
		    /* Ctrl-8 (Ctrl-?) */
		    else if (kbinput == '8' || kbinput == '?')
			kbinput = NANO_CONTROL_8;
		    /* Ctrl-A to Ctrl-_ */
		    else if (kbinput >= 'A' && kbinput <= '_')
			kbinput -= 64;
		    /* Ctrl-A to Ctrl-~ */
		    else if (kbinput >= 'a' && kbinput <= '~')
			kbinput -= 96;
		    break;
		case 'O':
		case 'o':
		/* Terminal breakage, part 1: We shouldn't get an escape
		 * sequence here for terminals that support Delete, but
		 * we do sometimes on FreeBSD.  Thank you, Wouter van
		 * Hemel. */
		case '[':
		{
		    int old_kbinput = kbinput, *escape_seq, escape_seq_len;
		    nodelay(win, TRUE);
		    kbinput = wgetch(win);
		    switch (kbinput) {
			case ERR:
			    kbinput = tolower(old_kbinput);
			    *meta_key = TRUE;
			    break;
			default:
			    ungetch(kbinput);
			    ungetch(old_kbinput);
			    escape_seq = get_verbatim_kbinput(win, &escape_seq_len, 0);
			    kbinput = get_escape_seq_kbinput(win, escape_seq, escape_seq_len);
			    free(escape_seq);
		    }
		    nodelay(win, FALSE);
		    break;
		}
		default:
		    /* Esc [character] == Meta-[character] */
		    kbinput = tolower(kbinput);
		    *meta_key = TRUE;
	    }
	    break;
	case NANO_CONTROL_8:
	    /* Terminal breakage, part 2: We shouldn't get Ctrl-8
	     * (Ctrl-?) for Backspace or Delete, but we do sometimes. */
	    kbinput = ISSET(REBIND_DELETE) ? NANO_DELETE_KEY : NANO_BACKSPACE_KEY;
	    break;
	case KEY_DOWN:
	    kbinput = NANO_NEXTLINE_KEY;
	    break;
	case KEY_UP:
	    kbinput = NANO_PREVLINE_KEY;
	    break;
	case KEY_LEFT:
	    kbinput = NANO_BACK_KEY;
	    break;
	case KEY_RIGHT:
	    kbinput = NANO_FORWARD_KEY;
	    break;
	case KEY_HOME:
	    kbinput = NANO_HOME_KEY;
	    break;
	case KEY_BACKSPACE:
	    kbinput = NANO_BACKSPACE_KEY;
	    break;
	case KEY_DC:
	    /* Terminal breakage, part 3: We should only get KEY_DC when
	     * hitting Delete, but we get it when hitting Backspace
	     * sometimes on FreeBSD.  Thank you, Lee Nelson. */
	    kbinput = ISSET(REBIND_DELETE) ? NANO_BACKSPACE_KEY : NANO_DELETE_KEY;
	    break;
	case KEY_IC:
	    kbinput = NANO_INSERTFILE_KEY;
	    break;
	case KEY_NPAGE:
	    kbinput = NANO_NEXTPAGE_KEY;
	    break;
	case KEY_PPAGE:
	    kbinput = NANO_PREVPAGE_KEY;
	    break;
	case KEY_ENTER:
	    kbinput = NANO_ENTER_KEY;
	    break;
	case KEY_END:
	    kbinput = NANO_END_KEY;
	    break;
	case KEY_SUSPEND:
	    kbinput = NANO_SUSPEND_KEY;
	    break;
	case KEY_SLEFT:
	    kbinput = NANO_BACK_KEY;
	    break;
	case KEY_SRIGHT:
	    kbinput = NANO_FORWARD_KEY;
	    break;
    }
#ifdef DEBUG
    fprintf(stderr, "get_accepted_kbinput(): kbinput = %d, meta_key = %d\n", kbinput, *meta_key);
#endif
    return kbinput;
}

/* Translate a three-digit decimal ASCII code from 000-255 into the
 * corresponding ASCII character. */
int get_ascii_kbinput(WINDOW *win, int kbinput)
{
    int retval;

    switch (kbinput) {
	case '0':
	case '1':
	case '2':
	    retval = (kbinput - '0') * 100;
	    break;
	default:
	    return kbinput;
    }

    kbinput = wgetch(win);
    switch (kbinput) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	    retval += (kbinput - '0') * 10;
	    break;
	case '6':
	case '7':
	case '8':
	case '9':
	    if (retval < 200) {
		retval += (kbinput - '0') * 10;
		break;
	    }
	default:
	    return kbinput;
    }

    kbinput = wgetch(win);
    switch (kbinput) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	    retval += kbinput - '0';
	    break;
	case '6':
	case '7':
	case '8':
	case '9':
	    if (retval < 250) {
		retval += kbinput - '0';
		break;
	    }
	default:
	    return kbinput;
    }

#ifdef DEBUG
    fprintf(stderr, "get_ascii_kbinput(): kbinput = %d\n", kbinput);
#endif
    return retval;
}

/* Translate escape sequences, most of which correspond to extended
 * keypad values.  These sequences are generated when the terminal
 * doesn't support the needed keys.  Assume that Escape has already been
 * read in, and that nodelay(win) is TRUE.
 *
 * The supported terminals are the Linux console, the FreeBSD console,
 * the Hurd console (a.k.a. the Mach console), xterm, rxvt, and Eterm.
 * There are several escape sequence conflicts and omissions, outlined
 * as follows:
 *
 * - F1 on FreeBSD console == kmous on xterm/rxvt/Eterm; the latter is
 *   omitted.  (Mouse input will only work properly if the extended
 *   keypad value KEY_MOUSE is generated on mouse events instead of the
 *   kmous escape sequence.)
 * - F9 on FreeBSD console == PageDown on Hurd console; the former is
 *   omitted.  (The editing keypad, consisting of Insert, Delete, Home,
 *   End, PageUp, and PageDown, is more important to have working than
 *   the function keys, because the functions of the former are not
 *   arbitrary and the functions of the latter are.)
 * - F10 on FreeBSD console == PageUp on Hurd console; the former is
 *   omitted.  (Same as above.)
 * - F13 on FreeBSD console == End on Hurd console; the former is
 *   omitted.  (Same as above.)
 * - The Hurd console has no escape sequences for F11, F12, F13, or
 *   F14. */
int get_escape_seq_kbinput(WINDOW *win, int *escape_seq, int
	escape_seq_len)
{
    int kbinput = ERR;

    if (escape_seq_len > 1) {
	switch (escape_seq[0]) {
	    case 'O':
		switch (escape_seq[1]) {
		    case '2':
			if (escape_seq_len >= 3) {
			    switch (escape_seq[2]) {
				case 'P': /* Esc O 2 P == F13 on
					   * xterm. */
				    kbinput = KEY_F(13);
				    break;
				case 'Q': /* Esc O 2 Q == F14 on
					   * xterm. */
				    kbinput = KEY_F(14);
				    break;
			    }
			}
			break;
		    case 'A': /* Esc O A == Up on xterm. */
		    case 'B': /* Esc O B == Down on xterm. */
		    case 'C': /* Esc O C == Right on xterm. */
		    case 'D': /* Esc O D == Left on xterm. */
			kbinput = get_escape_seq_abcd(escape_seq[1]);
			break;
		    case 'F': /* Esc O F == End on xterm. */
			kbinput = NANO_END_KEY;
			break;
		    case 'H': /* Esc O H == Home on xterm. */
			kbinput = NANO_HOME_KEY;
			break;
		    case 'P': /* Esc O P == F1 on Hurd console. */
			kbinput = KEY_F(1);
			break;
		    case 'Q': /* Esc O Q == F2 on Hurd console. */
			kbinput = KEY_F(2);
			break;
		    case 'R': /* Esc O R == F3 on Hurd console. */
			kbinput = KEY_F(3);
			break;
		    case 'S': /* Esc O S == F4 on Hurd console. */
			kbinput = KEY_F(4);
			break;
		    case 'T': /* Esc O T == F5 on Hurd console. */
			kbinput = KEY_F(5);
			break;
		    case 'U': /* Esc O U == F6 on Hurd console. */
			kbinput = KEY_F(6);
			break;
		    case 'V': /* Esc O V == F7 on Hurd console. */
			kbinput = KEY_F(7);
			break;
		    case 'W': /* Esc O W == F8 on Hurd console. */
			kbinput = KEY_F(8);
			break;
		    case 'X': /* Esc O X == F9 on Hurd console. */
			kbinput = KEY_F(9);
			break;
		    case 'Y': /* Esc O Y == F10 on Hurd console. */
			kbinput = KEY_F(10);
			break;
		    case 'a': /* Esc O a == Ctrl-Up on rxvt. */
		    case 'b': /* Esc O b == Ctrl-Down on rxvt. */
		    case 'c': /* Esc O c == Ctrl-Right on rxvt. */
		    case 'd': /* Esc O d == Ctrl-Left on rxvt. */
			kbinput = get_escape_seq_abcd(escape_seq[1]);
			break;
		}
		break;
	    case 'o':
		switch (escape_seq[1]) {
		    case 'a': /* Esc o a == Ctrl-Up on Eterm. */
		    case 'b': /* Esc o b == Ctrl-Down on Eterm. */
		    case 'c': /* Esc o c == Ctrl-Right on Eterm. */
		    case 'd': /* Esc o d == Ctrl-Left on Eterm. */
			kbinput = get_escape_seq_abcd(escape_seq[1]);
			break;
		}
		break;
	    case '[':
		switch (escape_seq[1]) {
		    case '1':
			if (escape_seq_len >= 3) {
			    switch (escape_seq[2]) {
				case '1': /* Esc [ 1 1 ~ == F1 on
					   * rxvt/Eterm. */
				    kbinput = KEY_F(1);
				    break;
				case '2': /* Esc [ 1 2 ~ == F2 on
					   * rxvt/Eterm. */
				    kbinput = KEY_F(2);
				    break;
				case '3': /* Esc [ 1 3 ~ == F3 on
					   * rxvt/Eterm. */
				    kbinput = KEY_F(3);
				    break;
				case '4': /* Esc [ 1 4 ~ == F4 on
					   * rxvt/Eterm. */
				    kbinput = KEY_F(4);
				    break;
				case '5': /* Esc [ 1 5 ~ == F5 on
					   * xterm/rxvt/Eterm. */
				    kbinput = KEY_F(5);
				    break;
				case '7': /* Esc [ 1 7 ~ == F6 on Linux
					   * console/xterm/rxvt/Eterm. */
				    kbinput = KEY_F(6);
				    break;
				case '8': /* Esc [ 1 8 ~ == F7 on Linux
					   * console/xterm/rxvt/Eterm. */
				    kbinput = KEY_F(7);
				    break;
				case '9': /* Esc [ 1 9 ~ == F8 on Linux
					   * console/xterm/rxvt/Eterm. */
				    kbinput = KEY_F(8);
				    break;
				case ';':
    if (escape_seq_len >= 4) {
	switch (escape_seq[3]) {
	    case '2':
		if (escape_seq_len >= 5) {
		    switch (escape_seq[4]) {
			case 'A': /* Esc [ 1 ; 2 A == Shift-Up on
				   * xterm. */
			case 'B': /* Esc [ 1 ; 2 B == Shift-Down on
				   * xterm. */
			case 'C': /* Esc [ 1 ; 2 C == Shift-Right on
				   * xterm. */
			case 'D': /* Esc [ 1 ; 2 D == Shift-Left on
				   * xterm. */
			    kbinput = get_escape_seq_abcd(escape_seq[4]);
			    break;
		    }
		}
		break;
	    case '5':
		if (escape_seq_len >= 5) {
		    switch (escape_seq[4]) {
			case 'A': /* Esc [ 1 ; 5 A == Ctrl-Up on
				   * xterm. */
			case 'B': /* Esc [ 1 ; 5 B == Ctrl-Down on
				   * xterm. */
			case 'C': /* Esc [ 1 ; 5 C == Ctrl-Right on
				   * xterm. */
			case 'D': /* Esc [ 1 ; 5 D == Ctrl-Left on
				   * xterm. */
			    kbinput = get_escape_seq_abcd(escape_seq[4]);
			    break;
		    }
		}
		break;
	}
    }
				    break;
				default: /* Esc [ 1 ~ == Home on Linux
					  * console. */
				    kbinput = NANO_HOME_KEY;
				    break;
			    }
			}
			break;
		    case '2':
			if (escape_seq_len >= 3) {
			    switch (escape_seq[2]) {
				case '0': /* Esc [ 2 0 ~ == F9 on Linux
					   * console/xterm/rxvt/Eterm. */
				    kbinput = KEY_F(9);
				    break;
				case '1': /* Esc [ 2 1 ~ == F10 on Linux
					   * console/xterm/rxvt/Eterm. */
				    kbinput = KEY_F(10);
				    break;
				case '3': /* Esc [ 2 3 ~ == F11 on Linux
					   * console/xterm/rxvt/Eterm. */
				    kbinput = KEY_F(11);
				    break;
				case '4': /* Esc [ 2 4 ~ == F12 on Linux
					   * console/xterm/rxvt/Eterm. */
				    kbinput = KEY_F(12);
				    break;
				case '5': /* Esc [ 2 5 ~ == F13 on Linux
					   * console/rxvt/Eterm. */
				    kbinput = KEY_F(13);
				    break;
				case '6': /* Esc [ 2 6 ~ == F14 on Linux
					   * console/rxvt/Eterm. */
				    kbinput = KEY_F(14);
				    break;
				default: /* Esc [ 2 ~ == Insert on Linux
					  * console/xterm. */
				    kbinput = NANO_INSERTFILE_KEY;
				    break;
			    }
			}
			break;
		    case '3': /* Esc [ 3 ~ == Delete on Linux
			       * console/xterm. */
			kbinput = NANO_DELETE_KEY;
			break;
		    case '4': /* Esc [ 4 ~ == End on Linux
			       * console/xterm. */
			kbinput = NANO_END_KEY;
			break;
		    case '5': /* Esc [ 5 ~ == PageUp on Linux
			       * console/xterm; Esc [ 5 ^ == PageUp on
			       * Eterm. */
			kbinput = NANO_PREVPAGE_KEY;
			break;
		    case '6': /* Esc [ 6 ~ == PageDown on Linux
			       * console/xterm; Esc [ 6 ^ == PageDown on
			       * Eterm. */
			kbinput = NANO_NEXTPAGE_KEY;
			break;
		    case '7': /* Esc [ 7 ~ == Home on rxvt. */
			kbinput = NANO_HOME_KEY;
			break;
		    case '8': /* Esc [ 8 ~ == End on rxvt. */
			kbinput = NANO_END_KEY;
			break;
		    case '9': /* Esc [ 9 == Delete on Hurd console. */
			kbinput = NANO_DELETE_KEY;
			break;
		    case '@': /* Esc [ @ == Insert on Hurd console. */
			kbinput = NANO_INSERTFILE_KEY;
			break;
		    case 'A': /* Esc [ A == Up on Linux console/FreeBSD
			       * console/Hurd console/rxvt/Eterm. */
		    case 'B': /* Esc [ B == Down on Linux
			       * console/FreeBSD console/Hurd
			       * console/rxvt/Eterm. */
		    case 'C': /* Esc [ C == Right on Linux
			       * console/FreeBSD console/Hurd
			       * console/rxvt/Eterm. */
		    case 'D': /* Esc [ D == Left on Linux
			       * console/FreeBSD console/Hurd
			       * console/rxvt/Eterm. */
			kbinput = get_escape_seq_abcd(escape_seq[1]);
			break;
		    case 'F': /* Esc [ F == End on FreeBSD
			       * console/Eterm. */
			kbinput = NANO_END_KEY;
			break;
		    case 'G': /* Esc [ G == PageDown on FreeBSD
				   * console. */
			kbinput = NANO_NEXTPAGE_KEY;
			break;
		    case 'H': /* Esc [ H == Home on FreeBSD
			       * console/Hurd console/Eterm. */
			kbinput = NANO_HOME_KEY;
			break;
		    case 'I': /* Esc [ I == PageUp on FreeBSD
			       * console. */
			kbinput = NANO_PREVPAGE_KEY;
			break;
		    case 'L': /* Esc [ L == Insert on FreeBSD
			       * console. */
			kbinput = NANO_INSERTFILE_KEY;
			break;
		    case 'M': /* Esc [ M == F1 on FreeBSD console. */
			kbinput = KEY_F(1);
			break;
		    case 'N': /* Esc [ N == F2 on FreeBSD console. */
			kbinput = KEY_F(2);
			break;
		    case 'O':
			if (escape_seq_len >= 3) {
			    switch (escape_seq[2]) {
				case 'P': /* Esc [ O P == F1 on
					   * xterm. */
				    kbinput = KEY_F(1);
				    break;
				case 'Q': /* Esc [ O Q == F2 on
					   * xterm. */
				    kbinput = KEY_F(2);
				    break;
				case 'R': /* Esc [ O R == F3 on
					   * xterm. */
				    kbinput = KEY_F(3);
				    break;
				case 'S': /* Esc [ O S == F4 on
					   * xterm. */
				    kbinput = KEY_F(4);
				    break;
				default: /* Esc [ O == F3 on
					  * FreeBSD console. */
				    kbinput = KEY_F(3);
				    break;
			    }
			}
			break;
		    case 'P': /* Esc [ P == F4 on FreeBSD console. */
			kbinput = KEY_F(4);
			break;
		    case 'Q': /* Esc [ Q == F5 on FreeBSD console. */
			kbinput = KEY_F(5);
			break;
		    case 'R': /* Esc [ R == F6 on FreeBSD console. */
			kbinput = KEY_F(6);
			break;
		    case 'S': /* Esc [ S == F7 on FreeBSD console. */
			kbinput = KEY_F(7);
			break;
		    case 'T': /* Esc [ T == F8 on FreeBSD console. */
			kbinput = KEY_F(8);
			break;
		    case 'U': /* Esc [ U == PageDown on Hurd console. */
			kbinput = NANO_NEXTPAGE_KEY;
			break;
		    case 'V': /* Esc [ V == PageUp on Hurd console. */
			kbinput = NANO_PREVPAGE_KEY;
			break;
		    case 'W': /* Esc [ W == F11 on FreeBSD console. */
			kbinput = KEY_F(11);
			break;
		    case 'X': /* Esc [ X == F12 on FreeBSD console. */
			kbinput = KEY_F(12);
			break;
		    case 'Y': /* Esc [ Y == End on Hurd console. */
			kbinput = NANO_END_KEY;
			break;
		    case 'Z': /* Esc [ Z == F14 on FreeBSD console. */
			kbinput = KEY_F(14);
			break;
		    case 'a': /* Esc [ a == Shift-Up on rxvt/Eterm. */
		    case 'b': /* Esc [ b == Shift-Down on rxvt/Eterm. */
		    case 'c': /* Esc [ c == Shift-Right on
			       * rxvt/Eterm. */
		    case 'd': /* Esc [ d == Shift-Left on rxvt/Eterm. */
			kbinput = get_escape_seq_abcd(escape_seq[1]);
			break;
		    case '[':
			if (escape_seq_len >= 3) {
			    switch (escape_seq[2]) {
				case 'A': /* Esc [ [ A == F1 on Linux
					   * console. */
				    kbinput = KEY_F(1);
				    break;
				case 'B': /* Esc [ [ B == F2 on Linux
					   * console. */
				    kbinput = KEY_F(2);
				    break;
				case 'C': /* Esc [ [ C == F3 on Linux
					   * console. */
				    kbinput = KEY_F(3);
				    break;
				case 'D': /* Esc [ [ D == F4 on Linux
					   * console. */
				    kbinput = KEY_F(4);
				    break;
				case 'E': /* Esc [ [ E == F5 on Linux
					   * console. */
				    kbinput = KEY_F(5);
				    break;
			    }
			}
			break;
		}
		break;
	}
    }

    if (kbinput == ERR) {
	/* This escape sequence is unrecognized; send it back. */
	for (; escape_seq_len > 1; escape_seq_len--)
	    ungetch(escape_seq[escape_seq_len - 1]);
	kbinput = escape_seq[0];
    }

    return kbinput;
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

#ifndef DISABLE_MOUSE
/* Check for a mouse event, and if one's taken place, save the
 * coordinates where it took place in mouse_x and mouse_y.  After that,
 * if allow_shortcuts is zero, return 0.  Otherwise, if the mouse event
 * took place on the shortcut list on the bottom two lines of the screen
 * (assuming that the shortcut list is visible), figure out which
 * shortcut was clicked and ungetch() the equivalent keystroke(s).
 * Return 0 if no keystrokes were ungetch()ed, or 1 if at least one was.
 * Assume that KEY_MOUSE has already been read in. */
int get_mouseinput(int *mouse_x, int *mouse_y, int allow_shortcuts)
{
    MEVENT mevent;

    *mouse_x = -1;
    *mouse_y = -1;

    /* First, get the actual mouse event. */
    if (getmouse(&mevent) == ERR)
	return 0;

    /* Save the screen coordinates where the mouse event took place. */
    *mouse_x = mevent.x;
    *mouse_y = mevent.y;

    /* If we're not allowing shortcuts' we're done now. */
    if (!allow_shortcuts)
	return 0;

    /* Otherwise, if the current shortcut list is being displayed on the
     * last two lines of the screen and the mouse event took place
     * inside it, we need to figure out which shortcut was clicked and
     * ungetch() the equivalent keystroke(s) for it. */
    if (!ISSET(NO_HELP) && wenclose(bottomwin, *mouse_y, *mouse_x)) {
	int i, j;
	int currslen;
	    /* The number of shortcuts in the current shortcut list. */
	const shortcut *s = currshortcut;
	    /* The actual shortcut we clicked on, starting at the first
	     * one in the current shortcut list. */

	/* Get the shortcut lists' length. */
	if (currshortcut == main_list)
	    currslen = MAIN_VISIBLE;
	else
	    currslen = length_of_list(currshortcut);

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
	    return 0;
	j = (*mouse_x / i) * 2 + j;
	if (j >= currslen)
	    return 0;

	/* Go through the shortcut list to determine which shortcut was
	 * clicked. */
	for (; j > 0; j--)
	    s = s->next;

	/* And ungetch() the equivalent control key.  If it's a Meta key
	 * sequence, we need to ungetch() Escape too.  Assume that the
	 * shortcut has an equivalent control key, meta key sequence, or
	 * both. */
	if (s->ctrlval != NANO_NO_KEY)
	    ungetch(s->ctrlval);
	else {
	    ungetch(s->metaval);
	    ungetch(NANO_CONTROL_3);
	}

	return 1;
    }
    return 0;
}
#endif

int do_first_line(void)
{
    current = fileage;
    placewewant = 0;
    current_x = 0;
    edit_update(current, TOP);
    return 1;
}

int do_last_line(void)
{
    current = filebot;
    placewewant = 0;
    current_x = 0;
    edit_update(current, CENTER);
    return 1;
}

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
	/* the position in str, returned */
    size_t length = 0;
	/* the screen display width to str[i] */

    assert(str != NULL);

    for (; length < xplus && *str != '\0'; i++, str++) {
	if (*str == '\t')
	    length += tabsize - (length % tabsize);
	else if (is_cntrl_char((int)*str))
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

/* A strlen with tabs factored in, similar to xplustabs().  How many
 * columns wide are the first size characters of buf? */
size_t strnlenpt(const char *buf, size_t size)
{
    size_t length = 0;

    assert(buf != NULL);
    for (; *buf != '\0' && size != 0; size--, buf++) {
	if (*buf == '\t')
	    length += tabsize - (length % tabsize);
	else if (is_cntrl_char((int)*buf))
	    length += 2;
	else
	    length++;
    }
    return length;
}

/* How many columns wide is buf? */
size_t strlenpt(const char *buf)
{
    return strnlenpt(buf, -1);
}

void blank_bottombars(void)
{
    if (!ISSET(NO_HELP)) {
	mvwaddstr(bottomwin, 1, 0, hblank);
	mvwaddstr(bottomwin, 2, 0, hblank);
    }
}

void blank_bottomwin(void)
{
    if (ISSET(NO_HELP))
	return;

    mvwaddstr(bottomwin, 1, 0, hblank);
    mvwaddstr(bottomwin, 2, 0, hblank);
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

void blank_statusbar_refresh(void)
{
    blank_statusbar();
    wrefresh(bottomwin);
}

void check_statblank(void)
{
    if (statblank > 1)
	statblank--;
    else if (statblank == 1 && !ISSET(CONSTUPDATE)) {
	statblank--;
	blank_statusbar_refresh();
    }
}

/* Convert buf into a string that can be displayed on screen.  The
 * caller wants to display buf starting with column start_col, and
 * extending for at most len columns.  start_col is zero-based.  len is
 * one-based, so len == 0 means you get "" returned.  The returned
 * string is dynamically allocated, and should be freed. */
char *display_string(const char *buf, size_t start_col, int len)
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
	if (*buf == '\t')
	    do {
		converted[index++] = ' ';
	    } while ((column + index) % tabsize);
	else if (is_cntrl_char(*buf)) {
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
	} else
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
int nanogetstr(int allowtabs, const char *buf, const char *def,
#ifndef NANO_SMALL
		historyheadtype *history_list,
#endif
		const shortcut *s
#ifndef DISABLE_TABCOMP
		, int *list
#endif
		)
{
    int kbinput;
    int meta_key;
    static int x = -1;
	/* the cursor position in 'answer' */
    int xend;
	/* length of 'answer', the status bar text */
    int tabbed = 0;
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

    /* Only put x at the end of the string if it's uninitialized or if
       it would be past the end of the string as it is.  Otherwise,
       leave it alone.  This is so the cursor position stays at the same
       place if a prompt-changing toggle is pressed. */
    if (x == -1 || x > xend || resetstatuspos)
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
    wrefresh(edit);

    while ((kbinput = get_kbinput(bottomwin, &meta_key)) != NANO_ENTER_KEY) {
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
	assert(0 <= x && x <= xend && xend == strlen(answer));

	if (kbinput != '\t')
	    tabbed = 0;

	switch (kbinput) {
#ifndef DISABLE_MOUSE
	case KEY_MOUSE:
	    do_mouse();
	    break;
#endif
	case NANO_HOME_KEY:
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
	    if (x < xend) {
		charmove(answer + x, answer + x + 1, xend - x);
		xend--;
	    }
	    break;
	case NANO_CUT_KEY:
	case NANO_UNCUT_KEY:
	    null_at(&answer, 0);
	    xend = 0;
	    x = 0;
	    break;
	case NANO_BACKSPACE_KEY:
	    if (x > 0) {
		charmove(answer + x - 1, answer + x, xend - x + 1);
		x--;
		xend--;
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
	    if (allowtabs) {
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
		if (currentbuf == NULL || (use_cb == 1 && strcmp(currentbuf, answer))) {
		    currentbuf = mallocstrcpy(currentbuf, answer);
		    use_cb = 0;
		}

		/* if currentbuf isn't NULL, use_cb is 2, and currentbuf 
		   is different from answer, it means that we're
		   scrolling up at the bottom of the search history, and
		   we need to make the string in currentbuf the current
		   answer; do this, blow away currentbuf since we don't
		   need it anymore, and reset use_cb to 0 */
		if (currentbuf != NULL && use_cb == 2 && strcmp(currentbuf, answer)) {
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
		    if (meta_key == TRUE && (kbinput == t->metaval || kbinput == t->miscval))
			/* We hit a Meta key.  Do like above.  We don't
			 * just ungetch() the letter and let it get
			 * caught above cause that screws the
			 * keypad... */
			return kbinput;
		}

	    if (is_cntrl_char(kbinput))
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
    x = -1;

    /* Just check for a blank answer here */
    if (answer[0] == '\0')
	return -2;
    else
	return 0;
}

/* If modified is not already set, set it and update titlebar. */
void set_modified(void)
{
    if (!ISSET(MODIFIED)) {
	SET(MODIFIED);
	titlebar(NULL);
	wrefresh(topwin);
    }
}

void titlebar(const char *path)
{
    int namelen, space;
    const char *what = path;

    if (path == NULL)
	what = filename;

    wattron(topwin, A_REVERSE);

    mvwaddstr(topwin, 0, 0, hblank);
    mvwaddnstr(topwin, 0, 2, VERMSG, COLS - 3);

    space = COLS - sizeof(VERMSG) - 23;

    namelen = strlen(what);

    if (space > 0) {
        if (what[0] == '\0')
      	    mvwaddnstr(topwin, 0, COLS / 2 - 6, _("New Buffer"),
			COLS / 2 + COLS % 2 - 6);
        else if (namelen > space) {
	    if (path == NULL)
		waddstr(topwin, _("  File: ..."));
	    else
		waddstr(topwin, _("   DIR: ..."));
	    waddstr(topwin, &what[namelen - space]);
	} else {
	    if (path == NULL)
		mvwaddstr(topwin, 0, COLS / 2 - (namelen / 2 + 1),
				_("File: "));
	    else
		mvwaddstr(topwin, 0, COLS / 2 - (namelen / 2 + 1),
				_(" DIR: "));
	    waddstr(topwin, what);
	}
    } /* If we don't have space, we shouldn't bother */
    if (ISSET(MODIFIED))
	mvwaddnstr(topwin, 0, COLS - 11, _(" Modified "), 11);
    else if (ISSET(VIEW_MODE))
	mvwaddnstr(topwin, 0, COLS - 11, _(" View "), 11);

    wattroff(topwin, A_REVERSE);

    wrefresh(topwin);
    reset_cursor();
}

void bottombars(const shortcut *s)
{
    int i, j, numcols;
    char keystr[9];
    int slen;

    if (ISSET(NO_HELP))
	return;

    if (s == main_list) {
	slen = MAIN_VISIBLE;
	assert(MAIN_VISIBLE <= length_of_list(s));
    } else
	slen = length_of_list(s);

    /* There will be this many columns of shortcuts */
    numcols = (slen + (slen % 2)) / 2;

    blank_bottomwin();

    for (i = 0; i < numcols; i++) {
	for (j = 0; j <= 1; j++) {

	    wmove(bottomwin, 1 + j, i * (COLS / numcols));

	    /* Yucky sentinel values we can't handle a better way */
	    if (s->ctrlval != NANO_NO_KEY) {
#ifndef NANO_SMALL
		if (s->ctrlval == NANO_HISTORY_KEY)
		    strncpy(keystr, _("Up"), 8);
		else
#endif
		if (s->ctrlval == NANO_CONTROL_SPACE)
		    strcpy(keystr, "^ ");
		else if (s->ctrlval == NANO_CONTROL_8)
		    strcpy(keystr, "^?");
		else
		    sprintf(keystr, "^%c", s->ctrlval + 64);
	    } else if (s->metaval != NANO_NO_KEY)
		sprintf(keystr, "M-%c", toupper(s->metaval));

	    onekey(keystr, s->desc, COLS / numcols);

	    s = s->next;
	    if (s == NULL)
		goto break_completely_out;
	}
    }

  break_completely_out:
    wrefresh(bottomwin);
}

/* Write a shortcut key to the help area at the bottom of the window. 
 * keystroke is e.g. "^G" and desc is e.g. "Get Help".
 * We are careful to write exactly len characters, even if len is
 * very small and keystroke and desc are long. */
void onekey(const char *keystroke, const char *desc, int len)
{
    wattron(bottomwin, A_REVERSE);
    waddnstr(bottomwin, keystroke, len);
    wattroff(bottomwin, A_REVERSE);
    len -= strlen(keystroke);
    if (len > 0) {
	waddch(bottomwin, ' ');
	len--;
	waddnstr(bottomwin, desc, len);
	len -= strlen(desc);
	for (; len > 0; len--)
	    waddch(bottomwin, ' ');
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
 * cursor at (current_y, current_x). */
void reset_cursor(void)
{
    /* Yuck.  This condition can be true after open_file() when opening
     * the first file. */
    if (edittop == NULL)
	return;

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
 * characters.  start is the column number of the first character
 * of this page.  That is, the first character of converted corresponds to
 * character number actual_x(fileptr->data, start) of the line. */
void edit_add(const filestruct *fileptr, const char *converted,
		int yval, size_t start)
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
			    x_start = strnlenpt(fileptr->data, startmatch.rm_so)
				- start;
			paintlen = strnlenpt(fileptr->data, startmatch.rm_eo)
				- start - x_start;
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
		int searched_later_lines = 0;
		    /* Used in step 2.  Have we looked for an end on
		     * lines after fileptr? */

		while (start_line != NULL &&
			regexec(&tmpcolor->start, start_line->data, 1,
			&startmatch, 0) == REG_NOMATCH) {
		    /* If there is an end on this line, there is no need
		     * to look for starts on earlier lines. */
		    if (regexec(tmpcolor->end, start_line->data, 0, NULL, 0)
			== 0)
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
			    start_line->data + start_col, 1, &startmatch,
			    REG_NOTBOL) == REG_NOMATCH)
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
			regexec(tmpcolor->end, end_line->data, 1, &endmatch, 0))
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

  step_two:	/* Second step, we look for starts on this line. */
		start_col = 0;
		while (start_col < endpos) {
		    if (regexec(&tmpcolor->start, fileptr->data + start_col, 1,
			&startmatch, start_col == 0 ? 0 : REG_NOTBOL)
			== REG_NOMATCH || start_col + startmatch.rm_so >=
			endpos)
			/* No more starts on this line. */
			break;
		    /* Translate the match to be relative to the
		     * beginning of the line. */
		    startmatch.rm_so += start_col;
		    startmatch.rm_eo += start_col;

		    if (startmatch.rm_so <= startpos)
			x_start = 0;
		    else
			x_start = strnlenpt(fileptr->data, startmatch.rm_so)
					- start;
		    if (regexec(tmpcolor->end, fileptr->data + startmatch.rm_eo,
			1, &endmatch, startmatch.rm_eo == 0 ? 0 :
			REG_NOTBOL) == 0) {
			/* Translate the end match to be relative to the
			 * beginning of the line. */
			endmatch.rm_so += startmatch.rm_eo;
			endmatch.rm_eo += startmatch.rm_eo;
			/* There is an end on this line.  But does it
			 * appear on this page, and is the match more than
			 * zero characters long? */
			if (endmatch.rm_eo > startpos &&
				endmatch.rm_eo > startmatch.rm_so) {
			    paintlen = strnlenpt(fileptr->data, endmatch.rm_eo)
					- start - x_start;
			    if (x_start + paintlen > COLS)
				paintlen = COLS - x_start;

			    assert(0 <= x_start && 0 < paintlen &&
				    x_start + paintlen <= COLS);
			    mvwaddnstr(edit, yval, x_start,
				converted + x_start, paintlen);
			}
		    } else if (!searched_later_lines) {
			searched_later_lines = 1;
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
					converted + x_start,
					COLS - x_start);
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

	mark_order(&top, &top_x, &bot, &bot_x);

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
		paintlen = strnlenpt(fileptr->data, bot_x) - (x_start +
			start);

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
 * If fileptr != current, then index is considered 0.
 * The line will be displayed starting with fileptr->data[index].
 * Likely args are current_x or 0. */
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

/* This function updates current, based on where current_y is;
 * reset_cursor() does the opposite. */
void update_cursor(void)
{
    int i = 0;

#ifdef DEBUG
    fprintf(stderr, "Moved to (%d, %d) in edit buffer\n", current_y,
	    current_x);
#endif

    current = edittop;
    while (i < current_y && current->next != NULL) {
	current = current->next;
	i++;
    }

#ifdef DEBUG
    fprintf(stderr, "current->data = \"%s\"\n", current->data);
#endif
}

void center_cursor(void)
{
    current_y = editwinrows / 2;
    wmove(edit, current_y, current_x);
}

/* Refresh the screen without changing the position of lines. */
void edit_refresh(void)
{
    /* Neither of these conditions should occur, but they do.  edittop
     * is NULL when you open an existing file on the command line, and
     * ENABLE_COLOR is defined.  Yuck. */
    if (current == NULL)
	return;
    if (edittop == NULL)
	edittop = current;

    if (current->lineno < edittop->lineno ||
	    current->lineno >= edittop->lineno + editwinrows)
	/* Note that edit_update() changes edittop so that
	 * current->lineno = edittop->lineno + editwinrows / 2.  Thus
	 * when it then calls edit_refresh(), there is no danger of
	 * getting an infinite loop. */
	edit_update(current, CENTER);
    else {
	int nlines = 0;
	const filestruct *foo = edittop;

#ifdef DEBUG
	fprintf(stderr, "edit_refresh(): edittop->lineno = %ld\n", edittop->lineno);
#endif

	/* Don't let the cursor jump around the screen while
	 * updating. */
	leaveok(edit, TRUE);

	while (nlines < editwinrows) {
	    update_line(foo, current_x);
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
	/* What the hell are we expecting to update the screen if this
	 * isn't here?  Luck? */
	wrefresh(edit);

	/* Let the cursor jump around the screen again. */
	leaveok(edit, FALSE);
    }
}

/* Same as above, but touch the window first, so everything is
 * redrawn. */
void edit_refresh_clearok(void)
{
    clearok(edit, TRUE);
    edit_refresh();
    clearok(edit, FALSE);
}

/* Nice generic routine to update the edit buffer, given a pointer to the
 * file struct =) */
void edit_update(filestruct *fileptr, topmidnone location)
{
    if (fileptr == NULL)
	return;

    if (location != TOP) {
	int goal = location == NONE ? current_y : editwinrows / 2;

	for (; goal > 0 && fileptr->prev != NULL; goal--)
	    fileptr = fileptr->prev;
    }
    edittop = fileptr;
    edit_refresh();
}

/* Ask a question on the statusbar.  Answer will be stored in answer
 * global.  Returns -1 on aborted enter, -2 on a blank string, and 0
 * otherwise, the valid shortcut key caught.  Def is any editable text we
 * want to put up by default.
 *
 * New arg tabs tells whether or not to allow tab completion. */
int statusq(int allowtabs, const shortcut *s, const char *def,
#ifndef NANO_SMALL
		historyheadtype *which_history,
#endif
		const char *msg, ...)
{
    va_list ap;
    char *foo = charalloc(COLS - 3);
    int ret;
#ifndef DISABLE_TABCOMP
    int list = 0;
#endif

    bottombars(s);

    va_start(ap, msg);
    vsnprintf(foo, COLS - 4, msg, ap);
    va_end(ap);
    foo[COLS - 4] = '\0';

    ret = nanogetstr(allowtabs, foo, def,
#ifndef NANO_SMALL
		which_history,
#endif
		s
#ifndef DISABLE_TABCOMP
		, &list
#endif
		);
    free(foo);
    resetstatuspos = 0;

    switch (ret) {
    case NANO_FIRSTLINE_KEY:
    case NANO_FIRSTLINE_FKEY:
	do_first_line();
	resetstatuspos = 1;
	break;
    case NANO_LASTLINE_KEY:
    case NANO_LASTLINE_FKEY:
	do_last_line();
	resetstatuspos = 1;
	break;
#ifndef DISABLE_JUSTIFY
    case NANO_PARABEGIN_KEY:
	do_para_begin();
	resetstatuspos = 1;
	break;
    case NANO_PARAEND_KEY:
	do_para_end();
	resetstatuspos = 1;
	break;
#endif
    case NANO_CANCEL_KEY:
	ret = -1;
	resetstatuspos = 1;
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

/* Ask a simple yes/no question on the statusbar.  Returns 1 for Y, 0
 * for N, 2 for All (if all is nonzero when passed in) and -1 for abort
 * (^C). */
int do_yesno(int all, const char *msg)
{
    int ok = -2, width = 16;
    const char *yesstr;		/* String of yes characters accepted */
    const char *nostr;		/* Same for no */
    const char *allstr;		/* And all, surprise! */

    /* Yes, no and all are strings of any length.  Each string consists
     * of all characters accepted as a valid character for that value.
     * The first value will be the one displayed in the shortcuts. */
    yesstr = _("Yy");
    nostr = _("Nn");
    allstr = _("Aa");

    /* Remove gettext call for keybindings until we clear the thing
     * up. */
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
	int meta_key;
#ifndef DISABLE_MOUSE
	int mouse_x, mouse_y;
#endif

	kbinput = get_kbinput(edit, &meta_key);

	if (kbinput == NANO_CANCEL_KEY)
	    ok = -1;
#ifndef DISABLE_MOUSE
	/* Look ma!  We get to duplicate lots of code from
	 * do_mouse()!! */
	else if (kbinput == KEY_MOUSE) {
	    kbinput = get_mouseinput(&mouse_x, &mouse_y, 0);

	    if (mouse_x != -1 && mouse_y != -1 && !ISSET(NO_HELP) &&
		wenclose(bottomwin, mouse_y, mouse_x) && mouse_x <
		(width * 2) && mouse_y >= editwinrows + 3) {

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
	 * str. */
	else if (strchr(yesstr, kbinput) != NULL)
	    ok = 1;
	else if (strchr(nostr, kbinput) != NULL)
	    ok = 0;
	else if (all && strchr(allstr, kbinput) != NULL)
	    ok = 2;
    } while (ok == -2);

    return ok;
}

int total_refresh(void)
{
    clearok(edit, TRUE);
    clearok(topwin, TRUE);
    clearok(bottomwin, TRUE);
    wnoutrefresh(edit);
    wnoutrefresh(topwin);
    wnoutrefresh(bottomwin);
    doupdate();
    clearok(edit, FALSE);
    clearok(topwin, FALSE);
    clearok(bottomwin, FALSE);
    edit_refresh();
    titlebar(NULL);
    return 1;
}

void display_main_list(void)
{
    bottombars(main_list);
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
	int start_x = 0;
	size_t foo_len;
	bar = charalloc(COLS - 3);
	vsnprintf(bar, COLS - 3, msg, ap);
	va_end(ap);
	foo = display_string(bar, 0, COLS - 4);
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
	wrefresh(edit);
	    /* Leave the cursor at its position in the edit window, not
	     * in the statusbar. */
    }

    SET(DISABLE_CURPOS);
    statblank = 26;
}

/* If constant is false, the user typed ^C so we unconditionally display
 * the cursor position.  Otherwise, we display it only if the character
 * position changed, and DISABLE_CURPOS is not set.
 *
 * If constant and DISABLE_CURPOS is set, we unset it and update old_i and
 * old_totsize.  That way, we leave the current statusbar alone, but next
 * time we will display. */
int do_cursorpos(int constant)
{
    const filestruct *fileptr;
    unsigned long i = 0;
    static unsigned long old_i = 0;
    static long old_totsize = -1;

    assert(current != NULL && fileage != NULL && totlines != 0);

    if (old_totsize == -1)
	old_totsize = totsize;

    for (fileptr = fileage; fileptr != current; fileptr = fileptr->next) {
	assert(fileptr != NULL);
	i += strlen(fileptr->data) + 1;
    }
    i += current_x;

    if (constant && ISSET(DISABLE_CURPOS)) {
	UNSET(DISABLE_CURPOS);
	old_i = i;
	old_totsize = totsize;
	return 0;
    }

    /* If constant is false, display the position on the statusbar
     * unconditionally; otherwise, only display the position when the
     * character values have changed. */
    if (!constant || old_i != i || old_totsize != totsize) {
	unsigned long xpt = xplustabs() + 1;
	unsigned long cur_len = strlenpt(current->data) + 1;
	int linepct = 100 * current->lineno / totlines;
	int colpct = 100 * xpt / cur_len;
	int bytepct = totsize == 0 ? 0 : 100 * i / totsize;

	statusbar(
	    _("line %ld/%ld (%d%%), col %lu/%lu (%d%%), char %lu/%ld (%d%%)"),
		    current->lineno, totlines, linepct,
		    xpt, cur_len, colpct,
		    i, totsize, bytepct);
	UNSET(DISABLE_CURPOS);
    }

    old_i = i;
    old_totsize = totsize;

    reset_cursor();
    return 0;
}

int do_cursorpos_void(void)
{
    return do_cursorpos(0);
}

/* Calculate the next line of help_text, starting at ptr. */
int line_len(const char *ptr)
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

/* Our shortcut-list-compliant help function, which is better than
 * nothing, and dynamic! */
int do_help(void)
{
#ifndef DISABLE_HELP
    int i, page = 0, kbinput = ERR, meta_key, no_more = 0;
    int no_help_flag = 0;
    const shortcut *oldshortcut;

    blank_edit();
    curs_set(0);
    wattroff(bottomwin, A_REVERSE);
    blank_statusbar();

    /* Set help_text as the string to display. */
    help_init();
    assert(help_text != NULL);

    oldshortcut = currshortcut;

    currshortcut = help_list;

    if (ISSET(NO_HELP)) {

	/* Well, if we're going to do this, we should at least do it the
	 * right way. */
	no_help_flag = 1;
	UNSET(NO_HELP);
	window_init();
	bottombars(help_list);

    } else
	bottombars(help_list);

    do {
	const char *ptr = help_text;

	switch (kbinput) {
#ifndef DISABLE_MOUSE
	case KEY_MOUSE:
	    do_mouse();
	    break;
#endif
	case NANO_NEXTPAGE_KEY:
	case NANO_NEXTPAGE_FKEY:
	    if (!no_more) {
		blank_edit();
		page++;
	    }
	    break;
	case NANO_PREVPAGE_KEY:
	case NANO_PREVPAGE_FKEY:
	    if (page > 0) {
		no_more = 0;
		blank_edit();
		page--;
	    }
	    break;
	}

	/* Calculate where in the text we should be, based on the
	 * page. */
	for (i = 1; i < page * (editwinrows - 1); i++) {
	    ptr += line_len(ptr);
	    if (*ptr == '\n')
		ptr++;
	}

	for (i = 0; i < editwinrows && *ptr != '\0'; i++) {
	    int j = line_len(ptr);

	    mvwaddnstr(edit, i, 0, ptr, j);
	    ptr += j;
	    if (*ptr == '\n')
		ptr++;
	}

	if (*ptr == '\0') {
	    no_more = 1;
	    continue;
	}
    } while ((kbinput = get_kbinput(edit, &meta_key)) != NANO_EXIT_KEY && kbinput != NANO_EXIT_FKEY);

    currshortcut = oldshortcut;

    if (no_help_flag) {
	blank_bottombars();
	wrefresh(bottomwin);
	SET(NO_HELP);
	window_init();
    } else
	bottombars(currshortcut);

    curs_set(1);
    edit_refresh();

    /* The help_init() at the beginning allocated help_text, which has
     * now been written to the screen. */
    free(help_text);
    help_text = NULL;

#elif defined(DISABLE_HELP)
    nano_disabled_msg();
#endif

    return 1;
}

/* Highlight the current word being replaced or spell checked.  We
 * expect word to have tabs and control characters expanded. */
void do_replace_highlight(int highlight_flag, const char *word)
{
    int y = xplustabs();
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
    int i, j = 0, k, place = 0, start_x;
    struct timespec scrolldelay;

    const char *what;
    const char *xlcredits[XLCREDIT_LEN];

    const char *credits[CREDIT_LEN] = { 
	"0",				/* "The nano text editor" */
	"1",				/* "version" */
	VERSION,
	"",
	"2",				/* "Brought to you by:" */
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
	"3",				/* "Special thanks to:" */
	"Plattsburgh State University",
	"Benet Laboratories",
	"Amy Allegretta",
	"Linda Young",
	"Jeremy Robichaud",
	"Richard Kolb II",
	"4",				/* "The Free Software Foundation" */
	"Linus Torvalds",
	"5",				/* "For ncurses:" */
	"Thomas Dickey",
	"Pavel Curtis",
	"Zeyd Ben-Halim",
	"Eric S. Raymond",
	"6",				/* "and anyone else we forgot..." */
	"7",				/* "Thank you for using nano!\n" */
	"", "", "", "",
	"(c) 1999-2004 Chris Allegretta",
	"", "", "", "",
	"http://www.nano-editor.org/"
    };

    xlcredits[0] = _("The nano text editor");
    xlcredits[1] = _("version ");
    xlcredits[2] = _("Brought to you by:");
    xlcredits[3] = _("Special thanks to:");
    xlcredits[4] = _("The Free Software Foundation");
    xlcredits[5] = _("For ncurses:");
    xlcredits[6] = _("and anyone else we forgot...");
    xlcredits[7] = _("Thank you for using nano!\n");

    scrolldelay.tv_sec = 0;
    scrolldelay.tv_nsec = 700000000;

    curs_set(0);
    nodelay(edit, TRUE);
    blank_bottombars();
    mvwaddstr(topwin, 0, 0, hblank);
    blank_edit();
    wrefresh(edit);
    wrefresh(bottomwin);
    wrefresh(topwin);

    while (wgetch(edit) == ERR) {
	for (k = 0; k <= 1; k++) {
	    blank_edit();
	    for (i = editwinrows / 2 - 1; i >= (editwinrows / 2 - 1 - j);
		 i--) {
		mvwaddstr(edit, i * 2 - k, 0, hblank);

		if (place - (editwinrows / 2 - 1 - i) < CREDIT_LEN) {
		    what = credits[place - (editwinrows / 2 - 1 - i)];

		    /* God I've missed hacking.  If what is exactly
			1 char long, it's a sentinel for a translated
			string, so use that instead.  This means no
			thanking people with 1 character long names ;-) */
		    if (strlen(what) == 1)
			what = xlcredits[atoi(what)];
		} else
		    what = "";

		start_x = COLS / 2 - strlen(what) / 2 - 1;
		mvwaddstr(edit, i * 2 - k, start_x, what);
	    }
	    nanosleep(&scrolldelay, NULL);
	    wrefresh(edit);
	}
	if (j < editwinrows / 2 - 1)
	    j++;

	place++;

	if (place >= CREDIT_LEN + editwinrows / 2)
	    break;
    }

    nodelay(edit, FALSE);
    curs_set(1);
    display_main_list();
    total_refresh();
}
#endif
