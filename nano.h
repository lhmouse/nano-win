/* $Id$ */
/**************************************************************************
 *   nano.h                                                               *
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

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef NANO_H
#define NANO_H 1

/* Macros for the flags int... */
#define SET(bit) flags |= bit
#define UNSET(bit) flags &= ~bit
#define ISSET(bit) (flags & bit)
#define TOGGLE(bit) flags ^= bit


#ifdef USE_SLANG	/* Slang support enabled */
#include <slcurses.h>
#define KEY_DC 0x113
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else /* Uh oh */
#include <curses.h> 
#endif /* CURSES_H */

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include "config.h"

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)
#include <glib.h>
# ifndef HAVE_SNPRINTF
#  define snprintf	g_snprintf
# endif
# ifndef HAVE_VSNPRINTF
#  define vsnprintf	g_vsnprintf
# endif
#endif

#define VERMSG "GNU nano " VERSION

#if defined(DISABLE_WRAPPING) && defined(DISABLE_JUSTIFY)
#define DISABLE_WRAPJUSTIFY 1
#endif

/* Structure types */
typedef struct filestruct {
    char *data;
    struct filestruct *next;	/* Next node */
    struct filestruct *prev;	/* Previous node */
    int lineno;			/* The line number */
} filestruct;

#ifdef ENABLE_MULTIBUFFER
typedef struct openfilestruct {
    char *filename;
    struct openfilestruct *next;	/* Next node */
    struct openfilestruct *prev;	/* Previous node */
    struct filestruct *fileage;	/* Current file */
    struct filestruct *filebot;	/* Current file's last line */
#ifndef NANO_SMALL
    struct filestruct *file_mark_beginbuf;
				/* Current file's beginning marked line */
    int file_mark_beginx;	/* Current file's beginning marked line's
				   x-coordinate position */
#endif
    int file_current_x;		/* Current file's x-coordinate position */
    int file_current_y;		/* Current file's y-coordinate position */
    int file_flags;		/* Current file's flags: modification
				   status (and marking status, if
				   available) */
    int file_placewewant;	/* Current file's place we want */
    int file_totlines;		/* Current file's total number of lines */
    long file_totsize;		/* Current file's total size */
    int file_lineno;		/* Current file's line number */
} openfilestruct;
#endif

typedef struct shortcut {
   int val;		/* Actual sequence that generates the keystroke */
   int altval;		/* Alt key # for this function, or 0 for none */
   int misc1;		/* Other int functions we want bound */
   int misc2;
   int viewok;		/* is this function legal in view mode? */
   int (*func) (void);	/* Function to call when we catch this key */
   const char *desc;	/* Description, e.g. "Page Up" */
#ifndef DISABLE_HELP
   const char *help;	/* Help file entry text */
#endif
   struct shortcut *next;
} shortcut;

typedef struct toggle {
   int val;		/* Sequence to toggle the key.  Should only need 1 */
   const char *desc;	/* Description for when toggle is, uh, toggled,
			   e.g. "Pico Messages"; we'll append Enabled or
			   Disabled */
   int flag;		/* What flag actually gets toggled */
   struct toggle *next;
} toggle;

#ifdef ENABLE_NANORC
typedef struct rcoption {
   char *name;
   int flag;
} rcoption;
 
#endif /* ENABLE_NANORC */

#ifdef ENABLE_COLOR

#define COLORSTRNUM 16

typedef struct colortype {
    int fg;			/* fg color */
    int bg;			/* bg color */
    int bright;			/* Is this color A_BOLD? */
    int pairnum;		/* Color pair number used for this fg/bg */
    char *start;		/* Start (or all) of the regex string */
    char *end;			/* End of the regex string */
    struct colortype *next;
} colortype;

typedef struct exttype {
    char *val;
    struct exttype *next;
} exttype;

typedef struct syntaxtype {
    char *desc;			/* Name of this syntax type */
    struct exttype *extensions;	/* List of extensions that this applies to */
    colortype *color;		/* color struct for this syntax */
    struct syntaxtype *next;
} syntaxtype;

#endif /* ENABLE_COLOR */


/* Bitwise flags so we can save space (or more correctly, not waste it) */

#define MODIFIED		(1<<0)
#define KEEP_CUTBUFFER		(1<<1)
#define CASE_SENSITIVE		(1<<2)
#define MARK_ISSET		(1<<3)
#define CONSTUPDATE		(1<<4)
#define NO_HELP			(1<<5)
#define PICO_MODE		(1<<6)
#define FOLLOW_SYMLINKS		(1<<7)
#define SUSPEND			(1<<8)
#define NO_WRAP			(1<<9)
#define AUTOINDENT		(1<<10)
#define SAMELINEWRAP		(1<<11)
#define VIEW_MODE		(1<<12)
#define USE_MOUSE		(1<<13)
#define USE_REGEXP              (1<<14)
#define REGEXP_COMPILED         (1<<15)
#define TEMP_OPT         	(1<<16)
#define CUT_TO_END         	(1<<17)
#define REVERSE_SEARCH		(1<<18)
#define MULTIBUFFER		(1<<19)
#define CLEAR_BACKUPSTRING	(1<<20)
#define DOS_FILE		(1<<21)
#define MAC_FILE		(1<<22)
#define SMOOTHSCROLL		(1<<23)
#define DISABLE_CURPOS		(1<<24)	/* Damn, we still need it */
#define ALT_KEYPAD		(1<<25)
#define NO_CONVERT		(1<<26)

/* Control key sequences, changing these would be very very bad */

#ifndef NANO_SMALL
#  define NANO_CONTROL_SPACE 0
#endif
#define NANO_CONTROL_A 1
#define NANO_CONTROL_B 2
#define NANO_CONTROL_C 3
#define NANO_CONTROL_D 4
#define NANO_CONTROL_E 5
#define NANO_CONTROL_F 6
#define NANO_CONTROL_G 7
#define NANO_CONTROL_H 8
#define NANO_CONTROL_I 9
#define NANO_CONTROL_J 10
#define NANO_CONTROL_K 11
#define NANO_CONTROL_L 12
#define NANO_CONTROL_M 13
#define NANO_CONTROL_N 14
#define NANO_CONTROL_O 15
#define NANO_CONTROL_P 16
#define NANO_CONTROL_Q 17
#define NANO_CONTROL_R 18
#define NANO_CONTROL_S 19
#define NANO_CONTROL_T 20
#define NANO_CONTROL_U 21
#define NANO_CONTROL_V 22
#define NANO_CONTROL_W 23
#define NANO_CONTROL_X 24
#define NANO_CONTROL_Y 25
#define NANO_CONTROL_Z 26

#define NANO_CONTROL_4 28
#define NANO_CONTROL_5 29
#define NANO_CONTROL_6 30
#define NANO_CONTROL_7 31

#define NANO_ALT_A 'a'
#define NANO_ALT_B 'b'
#define NANO_ALT_C 'c'
#define NANO_ALT_D 'd'
#define NANO_ALT_E 'e'
#define NANO_ALT_F 'f'
#define NANO_ALT_G 'g'
#define NANO_ALT_H 'h'
#define NANO_ALT_I 'i'
#define NANO_ALT_J 'j'
#define NANO_ALT_K 'k'
#define NANO_ALT_L 'l'
#define NANO_ALT_M 'm'
#define NANO_ALT_N 'n'
#define NANO_ALT_O 'o'
#define NANO_ALT_P 'p'
#define NANO_ALT_Q 'q'
#define NANO_ALT_R 'r'
#define NANO_ALT_S 's'
#define NANO_ALT_T 't'
#define NANO_ALT_U 'u'
#define NANO_ALT_V 'v'
#define NANO_ALT_W 'w'
#define NANO_ALT_X 'x'
#define NANO_ALT_Y 'y'
#define NANO_ALT_Z 'z'
#define NANO_ALT_PERIOD '.'
#define NANO_ALT_COMMA ','
#define NANO_ALT_LCARAT '<'
#define NANO_ALT_RCARAT '>'
#define NANO_ALT_BRACKET ']'
#ifndef NANO_SMALL
#  define NANO_ALT_SPACE ' '
#endif

/* Some semi-changeable keybindings; don't play with unless you're sure you
know what you're doing */

#define NANO_INSERTFILE_KEY	NANO_CONTROL_R
#define NANO_INSERTFILE_FKEY	KEY_F(5)
#define NANO_EXIT_KEY 		NANO_CONTROL_X
#define NANO_EXIT_FKEY 		KEY_F(2)
#define NANO_WRITEOUT_KEY	NANO_CONTROL_O
#define NANO_WRITEOUT_FKEY	KEY_F(3)
#define NANO_GOTO_KEY		NANO_CONTROL_7
#define NANO_GOTO_FKEY		KEY_F(13)
#define NANO_ALT_GOTO_KEY	NANO_ALT_G
#define NANO_HELP_KEY		NANO_CONTROL_G
#define NANO_HELP_FKEY		KEY_F(1)
#define NANO_WHEREIS_KEY	NANO_CONTROL_W
#define NANO_WHEREIS_FKEY	KEY_F(6)
#define NANO_REPLACE_KEY	NANO_CONTROL_4
#define NANO_REPLACE_FKEY	KEY_F(14)
#define NANO_ALT_REPLACE_KEY	NANO_ALT_R
#define NANO_OTHERSEARCH_KEY	NANO_CONTROL_R
#define NANO_PREVPAGE_KEY	NANO_CONTROL_Y
#define NANO_PREVPAGE_FKEY	KEY_F(7)
#define NANO_NEXTPAGE_KEY	NANO_CONTROL_V
#define NANO_NEXTPAGE_FKEY	KEY_F(8)
#define NANO_CUT_KEY		NANO_CONTROL_K
#define NANO_CUT_FKEY		KEY_F(9)
#define NANO_UNCUT_KEY		NANO_CONTROL_U
#define NANO_UNCUT_FKEY		KEY_F(10)
#define NANO_CURSORPOS_KEY	NANO_CONTROL_C
#define NANO_CURSORPOS_FKEY	KEY_F(11)
#define NANO_SPELL_KEY		NANO_CONTROL_T
#define NANO_SPELL_FKEY		KEY_F(12)
#define NANO_FIRSTLINE_KEY	NANO_PREVPAGE_KEY
#define NANO_LASTLINE_KEY	NANO_NEXTPAGE_KEY
#define NANO_CANCEL_KEY		NANO_CONTROL_C
#define NANO_REFRESH_KEY	NANO_CONTROL_L
#define NANO_JUSTIFY_KEY	NANO_CONTROL_J
#define NANO_JUSTIFY_FKEY	KEY_F(4)
#define NANO_UNJUSTIFY_KEY	NANO_CONTROL_U
#define NANO_UP_KEY		NANO_CONTROL_P
#define NANO_DOWN_KEY		NANO_CONTROL_N
#define NANO_FORWARD_KEY	NANO_CONTROL_F
#define NANO_BACK_KEY		NANO_CONTROL_B
#define NANO_MARK_KEY		NANO_CONTROL_6
#define NANO_ALT_MARK_KEY	NANO_ALT_A
#define NANO_HOME_KEY		NANO_CONTROL_A
#define NANO_END_KEY		NANO_CONTROL_E
#define NANO_DELETE_KEY		NANO_CONTROL_D
#define NANO_BACKSPACE_KEY	NANO_CONTROL_H
#define NANO_TAB_KEY		NANO_CONTROL_I
#define NANO_SUSPEND_KEY	NANO_CONTROL_Z
#define NANO_ENTER_KEY		NANO_CONTROL_M
#define NANO_FROMSEARCHTOGOTO_KEY NANO_CONTROL_T
#define NANO_TOFILES_KEY	NANO_CONTROL_T
#define NANO_APPEND_KEY		NANO_ALT_A
#define NANO_PREPEND_KEY	NANO_ALT_P
#define NANO_OPENPREV_KEY	NANO_ALT_LCARAT
#define NANO_OPENNEXT_KEY	NANO_ALT_RCARAT
#define NANO_OPENPREV_ALTKEY	NANO_ALT_COMMA
#define NANO_OPENNEXT_ALTKEY	NANO_ALT_PERIOD
#define NANO_BRACKET_KEY	NANO_ALT_BRACKET
#define NANO_EXTCMD_KEY		NANO_CONTROL_X
#ifndef NANO_SMALL
#  define NANO_NEXTWORD_KEY	NANO_CONTROL_SPACE
#  define NANO_PREVWORD_KEY	NANO_ALT_SPACE
#endif

#define TOGGLE_CONST_KEY	NANO_ALT_C
#define TOGGLE_AUTOINDENT_KEY	NANO_ALT_I
#define TOGGLE_SUSPEND_KEY	NANO_ALT_Z
#define TOGGLE_NOHELP_KEY	NANO_ALT_X
#define TOGGLE_PICOMODE_KEY	NANO_ALT_P
#define TOGGLE_MOUSE_KEY	NANO_ALT_M
#define TOGGLE_CUTTOEND_KEY	NANO_ALT_K
#define TOGGLE_REGEXP_KEY	NANO_ALT_E
#define TOGGLE_WRAP_KEY		NANO_ALT_W
#define TOGGLE_BACKWARDS_KEY	NANO_ALT_B
#define TOGGLE_CASE_KEY		NANO_ALT_A
#define TOGGLE_LOAD_KEY		NANO_ALT_F
#define TOGGLE_DOS_KEY		NANO_ALT_D
#define TOGGLE_MAC_KEY		NANO_ALT_O
#define TOGGLE_SMOOTH_KEY	NANO_ALT_S
#define TOGGLE_NOCONVERT_KEY	NANO_ALT_N

#define MAIN_VISIBLE 12

#define VIEW 1
#define NOVIEW 0

#define NONE 3
#define TOP 2
#define CENTER 1
#define BOTTOM 0

/* Minimum editor window rows required for Nano to work correctly */
#define MIN_EDITOR_ROWS 3

/* Default number of characters from end-of-line where text wrapping occurs */
#define CHARS_FROM_EOL 8

/* Minimum fill length (space available for text before wrapping occurs) */
#define MIN_FILL_LENGTH 10

/* Color specific defines */
#ifdef ENABLE_COLOR
typedef struct colorstruct {
    int fg;
    int bg;
    int bold;
    int set;
} colorstruct;

#define FIRST_COLORNUM 16

#define COLOR_TITLEBAR 16
#define COLOR_BOTTOMBARS 17
#define COLOR_STATUSBAR 18
#define COLOR_TEXT 19
#define COLOR_MARKER 20

#define NUM_NCOLORS 5

#endif /* ENABLE_COLOR */

#endif /* ifndef NANO_H */ 
