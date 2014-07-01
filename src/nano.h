/* $Id$ */
/**************************************************************************
 *   nano.h                                                               *
 *                                                                        *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,  *
 *   2008, 2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.    *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 3, or (at your option)  *
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

#ifndef NANO_H
#define NANO_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef NEED_XOPEN_SOURCE_EXTENDED
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif
#endif

#ifdef __TANDEM
/* Tandem NonStop Kernel support. */
#include <floss.h>
#define NANO_ROOT_UID 65535
#else
#define NANO_ROOT_UID 0
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

/* Suppress warnings for __attribute__((warn_unused_result)). */
#define IGNORE_CALL_RESULT(call) do { if (call) {} } while(0)

/* Macros for flags, indexing each bit in a small array. */
#define FLAGS(flag) flags[((flag) / (sizeof(unsigned) * 8))]
#define FLAGMASK(flag) (1 << ((flag) % (sizeof(unsigned) * 8)))
#define SET(flag) FLAGS(flag) |= FLAGMASK(flag)
#define UNSET(flag) FLAGS(flag) &= ~FLAGMASK(flag)
#define ISSET(flag) ((FLAGS(flag) & FLAGMASK(flag)) != 0)
#define TOGGLE(flag) FLAGS(flag) ^= FLAGMASK(flag)

/* Macros for character allocation and more. */
#define charalloc(howmuch) (char *)nmalloc((howmuch) * sizeof(char))
#define charealloc(ptr, howmuch) (char *)nrealloc(ptr, (howmuch) * sizeof(char))
#define charmove(dest, src, n) memmove(dest, src, (n) * sizeof(char))
#define charset(dest, src, n) memset(dest, src, (n) * sizeof(char))

/* Set a default value for PATH_MAX if there isn't one. */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef USE_SLANG
/* Slang support. */
#include <slcurses.h>
/* Slang curses emulation brain damage, part 3: Slang doesn't define the
 * curses equivalents of the Insert or Delete keys. */
#define KEY_DC SL_KEY_DELETE
#define KEY_IC SL_KEY_IC
/* Ncurses support. */
#elif defined(HAVE_NCURSESW_NCURSES_H)
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
/* Curses support. */
#include <curses.h>
#endif /* CURSES_H */

#ifdef ENABLE_NLS
/* Native language support. */
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#define _(string) gettext(string)
#define P_(singular, plural, number) ngettext(singular, plural, number)
#else
#define _(string) (string)
#define P_(singular, plural, number) (number == 1 ? singular : plural)
#endif
#define gettext_noop(string) (string)
#define N_(string) gettext_noop(string)
	/* Mark a string that will be sent to gettext() later. */

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#ifndef NANO_TINY
#include <setjmp.h>
#endif
#include <assert.h>

/* If no vsnprintf(), use the version from glib 2.x. */
#ifndef HAVE_VSNPRINTF
#include <glib.h>
#define vsnprintf g_vsnprintf
#endif

/* If no isblank(), iswblank(), strcasecmp(), strncasecmp(),
 * strcasestr(), strnlen(), getdelim(), or getline(), use the versions
 * we have. */
#ifndef HAVE_ISBLANK
#define isblank nisblank
#endif
#ifndef HAVE_ISWBLANK
#define iswblank niswblank
#endif
#ifndef HAVE_STRCASECMP
#define strcasecmp nstrcasecmp
#endif
#ifndef HAVE_STRNCASECMP
#define strncasecmp nstrncasecmp
#endif
#ifndef HAVE_STRCASESTR
#define strcasestr nstrcasestr
#endif
#ifndef HAVE_STRNLEN
#define strnlen nstrnlen
#endif
#ifndef HAVE_GETDELIM
#define getdelim ngetdelim
#endif
#ifndef HAVE_GETLINE
#define getline ngetline
#endif

/* If we aren't using ncurses with mouse support, turn the mouse support
 * off, as it's useless then. */
#ifndef NCURSES_MOUSE_VERSION
#define DISABLE_MOUSE 1
#endif

#if defined(DISABLE_WRAPPING) && defined(DISABLE_JUSTIFY)
#define DISABLE_WRAPJUSTIFY 1
#endif

/* Enumeration types. */
typedef enum {
    NIX_FILE, DOS_FILE, MAC_FILE
} file_format;

typedef enum {
    OVERWRITE, APPEND, PREPEND
} append_type;

typedef enum {
    UPWARD, DOWNWARD
} scroll_dir;

typedef enum {
    CENTER, NONE
} update_type;

typedef enum {
    CONTROL, META, FKEY, RAWINPUT
}  key_type;

typedef enum {
    ADD, DEL, BACK, CUT, CUT_EOF, REPLACE,
#ifndef DISABLE_WRAPPING
    SPLIT_BEGIN, SPLIT_END,
#endif
    JOIN, PASTE, INSERT, ENTER, OTHER
} undo_type;

typedef struct color_pair {
    int pairnum;
	/* The color pair number used for this foreground color and
	 * background color. */
    bool bright;
	/* Is this color A_BOLD? */
} color_pair;

#ifndef DISABLE_COLOR
typedef struct colortype {
    short fg;
	/* This syntax's foreground color. */
    short bg;
	/* This syntax's background color. */
    bool bright;
	/* Is this color A_BOLD? */
    bool icase;
	/* Is this regex string case insensitive? */
    int pairnum;
	/* The color pair number used for this foreground color and
	 * background color. */
    char *start_regex;
	/* The start (or all) of the regex string. */
    regex_t *start;
	/* The compiled start (or all) of the regex string. */
    char *end_regex;
	/* The end (if any) of the regex string. */
    regex_t *end;
	/* The compiled end (if any) of the regex string. */
    struct colortype *next;
	/* Next set of colors. */
    int id;
	/* Basic id for assigning to lines later. */
} colortype;

typedef struct regexlisttype {
    char *ext_regex;
	/* The regexstrings for the things that match this syntax. */
    regex_t *ext;
	/* The compiled regexes. */
    struct regexlisttype *next;
	/* Next set of regexes. */
} regexlisttype;

typedef struct syntaxtype {
    char *desc;
	/* The name of this syntax. */
    regexlisttype *extensions;
	/* The list of extensions that this syntax applies to. */
    regexlisttype *headers;
	/* The list of headerlines that this syntax applies to. */
    regexlisttype *magics;
	/* The list of libmagic results that this syntax applies to. */
    colortype *color;
	/* The colors used in this syntax. */
    char *linter;
	/* The command to lint this type of file. */
    int nmultis;
	/* How many multi-line strings this syntax has. */
    struct syntaxtype *next;
	/* Next syntax. */
} syntaxtype;

typedef struct lintstruct {
    ssize_t lineno;
	/* Line number of the error. */
    ssize_t colno;
	/* Column # of the error. */
    char *msg;
	/* Error message text. */
    char *filename;
	/* Filename. */
    struct lintstruct *next;
	/* Next error. */
    struct lintstruct *prev;
	/* Previous error. */
} lintstruct;


#define CNONE		(1<<1)
	/* Yay, regex doesn't apply to this line at all! */
#define CBEGINBEFORE	(1<<2)
	/* Regex starts on an earlier line, ends on this one. */
#define CENDAFTER	(1<<3)
	/* Regex starts on this line and ends on a later one. */
#define CWHOLELINE	(1<<4)
	/* Whole line engulfed by the regex, start < me, end > me. */
#define CSTARTENDHERE	(1<<5)
	/* Regex starts and ends within this line. */
#define CWTF		(1<<6)
	/* Something else. */

#endif /* !DISABLE_COLOR */


/* Structure types. */
typedef struct filestruct {
    char *data;
	/* The text of this line. */
    ssize_t lineno;
	/* The number of this line. */
    struct filestruct *next;
	/* Next node. */
    struct filestruct *prev;
	/* Previous node. */
#ifndef DISABLE_COLOR
    short *multidata;
	/* Array of which multi-line regexes apply to this line. */
#endif
} filestruct;

typedef struct partition {
    filestruct *fileage;
	/* The top line of this portion of the file. */
    filestruct *top_prev;
	/* The line before the top line of this portion of the file. */
    char *top_data;
	/* The text before the beginning of the top line of this portion
	 * of the file. */
    filestruct *filebot;
	/* The bottom line of this portion of the file. */
    filestruct *bot_next;
	/* The line after the bottom line of this portion of the
	 * file. */
    char *bot_data;
	/* The text after the end of the bottom line of this portion of
	 * the file. */
} partition;

#ifndef NANO_TINY
typedef struct undo {
    ssize_t lineno;
    undo_type type;
	/* What type of undo this was. */
    size_t begin;
	/* Where did this action begin or end. */
    char *strdata;
	/* String type data we will use for copying the affected line back. */
    int xflags;
	/* Some flag data we need. */

    /* Cut-specific stuff we need. */
    filestruct *cutbuffer;
	/* Copy of the cutbuffer. */
    filestruct *cutbottom;
	/* Copy of cutbottom. */
    bool mark_set;
	/* Was the marker set when we cut? */
    ssize_t mark_begin_lineno;
	/* copy copy copy */
    size_t mark_begin_x;
	/* Another shadow variable. */
    struct undo *next;
} undo;
#endif /* !NANO_TINY */

#ifndef DISABLE_HISTORIES
typedef struct poshiststruct {
    char *filename;
	/* The file. */
    ssize_t lineno;
	/* Line number we left off on. */
    ssize_t xno;
	/* x position in the file we left off on. */
    struct poshiststruct *next;
} poshiststruct;
#endif

typedef struct openfilestruct {
    char *filename;
	/* The current file's name. */
    filestruct *fileage;
	/* The current file's first line. */
    filestruct *filebot;
	/* The current file's last line. */
    filestruct *edittop;
	/* The current top of the edit window. */
    filestruct *current;
	/* The current file's current line. */
    size_t totsize;
	/* The current file's total number of characters. */
    size_t current_x;
	/* The current file's x-coordinate position. */
    size_t placewewant;
	/* The current file's x position we would like. */
    ssize_t current_y;
	/* The current file's y-coordinate position. */
    bool modified;
	/* Whether the current file has been modified. */
#ifndef NANO_TINY
    bool mark_set;
	/* Whether the mark is on in the current file. */
    filestruct *mark_begin;
	/* The current file's beginning marked line, if any. */
    size_t mark_begin_x;
	/* The current file's beginning marked line's x-coordinate
	 * position, if any. */
    file_format fmt;
	/* The current file's format. */
    struct stat *current_stat;
	/* The current file's stat. */
    undo *undotop;
	/* Top of the undo list. */
    undo *current_undo;
	/* The current (i.e. next) level of undo. */
    undo_type last_action;
    const char *lock_filename;
	/* The path of the lockfile, if we created one. */
#endif
#ifndef DISABLE_COLOR
    syntaxtype *syntax;
	/* The  syntax struct for this file, if any. */
    colortype *colorstrings;
	/* The current file's associated colors. */
#endif
    struct openfilestruct *next;
	/* Next node. */
    struct openfilestruct *prev;
	/* Previous node. */
} openfilestruct;

typedef struct shortcut {
    const char *desc;
	/* The function's description, e.g. "Page Up". */
#ifndef DISABLE_HELP
    const char *help;
	/* The help file entry text for this function. */
    bool blank_after;
	/* Whether there should be a blank line after the help entry
	 * text for this function. */
#endif
    bool viewok;
	/* Is this function allowed when in view mode? */
    void (*func)(void);
	/* The function to call when we get this key. */
    struct shortcut *next;
	/* Next shortcut. */
} shortcut;

#ifndef DISABLE_NANORC
typedef struct rcoption {
   const char *name;
	/* The name of the rcfile option. */
   long flag;
	/* The flag associated with it, if any. */
} rcoption;
#endif

typedef struct sc {
    char *keystr;
	/* The shortcut key for a function, ASCII version. */
    key_type type;
	/* What kind of command key it is, for convenience later. */
    int seq;
	/* The actual sequence to check on the type is determined. */
    int menu;
	/* What list this applies to. */
    void (*scfunc)(void);
	/* The function we're going to run. */
    int toggle;
	/* If a toggle, what we're toggling. */
    bool execute;
	/* Whether to execute the function in question or just return
	 * so the sequence can be caught by the calling code. */
    struct sc *next;
	/* Next in the list. */
} sc;

typedef struct subnfunc {
    void (*scfunc)(void);
	/* What function this is. */
    int menus;
	/* In what menus this function applies. */
    const char *desc;
	/* The function's description, e.g. "Page Up". */
#ifndef DISABLE_HELP
    const char *help;
	/* The help file entry text for this function. */
    bool blank_after;
	/* Whether there should be a blank line after the help entry
	 * text for this function. */
#endif
    bool viewok;
	/* Is this function allowed when in view mode? */
    long toggle;
	/* If this is a toggle, if nonzero what toggle to set. */
    struct subnfunc *next;
	/* Next item in the list. */
} subnfunc;

/* The elements of the interface that can be colored differently. */
enum
{
    TITLE_BAR = 0,
    STATUS_BAR,
    KEY_COMBO,
    FUNCTION_TAG,
    NUMBER_OF_ELEMENTS
};

/* Enumeration used in the flags array.  See the definition of FLAGMASK. */
enum
{
    DONTUSE,
    CASE_SENSITIVE,
    CONST_UPDATE,
    NO_HELP,
    NOFOLLOW_SYMLINKS,
    SUSPEND,
    NO_WRAP,
    AUTOINDENT,
    VIEW_MODE,
    USE_MOUSE,
    USE_REGEXP,
    TEMP_FILE,
    CUT_TO_END,
    BACKWARDS_SEARCH,
    MULTIBUFFER,
    SMOOTH_SCROLL,
    REBIND_DELETE,
    REBIND_KEYPAD,
    NO_CONVERT,
    BACKUP_FILE,
    INSECURE_BACKUP,
    NO_COLOR_SYNTAX,
    PRESERVE,
    HISTORYLOG,
    RESTRICTED,
    SMART_HOME,
    WHITESPACE_DISPLAY,
    MORE_SPACE,
    TABS_TO_SPACES,
    QUICK_BLANK,
    WORD_BOUNDS,
    NO_NEWLINES,
    BOLD_TEXT,
    QUIET,
    SOFTWRAP,
    POS_HISTORY,
    LOCKING,
    NOREAD_MODE
};

/* Flags for the menus in which a given function should be present. */
#define MMAIN			(1<<0)
#define MWHEREIS		(1<<1)
#define MREPLACE		(1<<2)
#define MREPLACEWITH		(1<<3)
#define MGOTOLINE		(1<<4)
#define MWRITEFILE		(1<<5)
#define MINSERTFILE		(1<<6)
#define MEXTCMD			(1<<7)
#define MHELP			(1<<8)
#define MSPELL			(1<<9)
#define MBROWSER		(1<<10)
#define MWHEREISFILE		(1<<11)
#define MGOTODIR		(1<<12)
#define MYESNO			(1<<13)
#define MLINTER			(1<<14)
/* This is an abbreviation for all menus except Help and YesNo. */
#define MMOST	(MMAIN|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MWRITEFILE|MINSERTFILE|MEXTCMD|MBROWSER|MWHEREISFILE|MGOTODIR|MSPELL|MLINTER)

/* Control key sequences.  Changing these would be very, very bad. */
#define NANO_CONTROL_SPACE 0
#define NANO_CONTROL_I 9
#define NANO_CONTROL_3 27
#define NANO_CONTROL_7 31
#define NANO_CONTROL_8 127

/* Codes for "modified" Arrow keys.  Chosen like this because some
 * terminals produce them, and they are beyond KEY_MAX of ncurses. */
#define CONTROL_LEFT 539
#define CONTROL_RIGHT 554

#ifndef NANO_TINY
/* Extra bits for the undo function. */
#define UNdel_del		(1<<0)
#define UNdel_backspace		(1<<1)
#define UNcut_marked_backwards	(1<<2)
#define UNcut_cutline		(1<<3)
#endif /* !NANO_TINY */

/* The maximum number of entries displayed in the main shortcut list. */
#define MAIN_VISIBLE (((COLS + 40) / 20) * 2)

/* The minimum editor window columns and rows required for nano to work
 * correctly. */
#define MIN_EDITOR_COLS 4
#define MIN_EDITOR_ROWS 1

/* The default number of characters from the end of the line where
 * wrapping occurs. */
#define CHARS_FROM_EOL 8

/* The default width of a tab in spaces. */
#define WIDTH_OF_TAB 8

/* The maximum number of search/replace history strings saved, not
 * counting the blank lines at their ends. */
#define MAX_SEARCH_HISTORY 100

/* The maximum number of bytes buffered at one time. */
#define MAX_BUF_SIZE 128

#endif /* !NANO_H */
