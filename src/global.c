/* $Id$ */
/**************************************************************************
 *   global.c                                                             *
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
#include <assert.h>
#include "proto.h"

/* Global variables */

#ifndef DISABLE_WRAPJUSTIFY
/* wrap_at might be set in rcfile.c or nano.c. */
ssize_t wrap_at = -CHARS_FROM_EOL;	/* Right justified fill value,
					   allows resize */
#endif

char *last_search = NULL;	/* Last string we searched for */
char *last_replace = NULL;	/* Last replacement string */
int search_last_line;		/* Is this the last search line? */

unsigned long flags = 0;	/* Our flag containing many options */
WINDOW *topwin;			/* Top buffer */
WINDOW *edit;			/* The file portion of the editor */
WINDOW *bottomwin;		/* Bottom buffer */
char *filename = NULL;		/* Name of the file */

#ifndef NANO_SMALL
struct stat originalfilestat;	/* Stat for the file as we loaded it */
#endif

int editwinrows = 0;		/* How many rows long is the edit
				   window? */
filestruct *current;		/* Current buffer pointer */
size_t current_x = 0;		/* Current x-coordinate in the edit
				   window */
int current_y = 0;		/* Current y-coordinate in the edit
				   window */
filestruct *fileage = NULL;	/* Our file buffer */
filestruct *edittop = NULL;	/* Pointer to the top of the edit
				   buffer with respect to the
				   file struct */
filestruct *filebot = NULL;	/* Last node in the file struct */
filestruct *cutbuffer = NULL;	/* A place to store cut text */
#ifndef DISABLE_JUSTIFY
filestruct *jusbuffer = NULL;	/* A place to store unjustified text */
#endif
partition *filepart = NULL;	/* A place to store a portion of the
				   file struct */

#ifdef ENABLE_MULTIBUFFER
openfilestruct *open_files = NULL;	/* The list of open file
					   buffers */
#endif

#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
char *whitespace = NULL;	/* Characters used when displaying
				   the first characters of tabs and
				   spaces. */
int whitespace_len[2];		/* The length of the characters. */
#endif

#ifndef DISABLE_JUSTIFY
char *punct = NULL;		/* Closing punctuation that can end
				   sentences. */
char *brackets = NULL;		/* Closing brackets that can follow
				   closing punctuation and can end
				   sentences. */
char *quotestr = NULL;		/* Quote string.  The default value is
				   set in main(). */
#ifdef HAVE_REGEX_H
regex_t quotereg;		/* Compiled quotestr regular expression. */
int quoterc;			/* Did it compile? */
char *quoteerr = NULL;		/* The error message. */
#else
size_t quotelen;		/* strlen(quotestr) */
#endif
#endif

#ifndef NANO_SMALL
char *backup_dir = NULL;	/* Backup directory. */
#endif

char *answer = NULL;		/* Answer str to many questions */
int totlines = 0;		/* Total number of lines in the file */
size_t totsize = 0;		/* Total number of characters in the
				   file */
size_t placewewant = 0;		/* The column we'd like the cursor
				   to jump to when we go to the
				   next or previous line */

ssize_t tabsize = -1;		/* Our internal tabsize variable.  The
				   default value is set in main(). */

char *hblank = NULL;		/* A horizontal blank line */
#ifndef DISABLE_HELP
char *help_text;		/* The text in the help window */
#endif

/* More stuff for the marker select */

#ifndef NANO_SMALL
filestruct *mark_beginbuf;	/* The begin marker buffer */
size_t mark_beginx;		/* X value in the string to start */
#endif

#ifndef DISABLE_OPERATINGDIR
char *operating_dir = NULL;	/* Operating directory, which we can't */
char *full_operating_dir = NULL;/* go higher than */
#endif

#ifndef DISABLE_SPELLER
char *alt_speller = NULL;		/* Alternative spell command */
#endif

shortcut *main_list = NULL;
shortcut *whereis_list = NULL;
shortcut *replace_list = NULL;
shortcut *replace_list_2 = NULL; 	/* 2nd half of replace dialog */
shortcut *gotoline_list = NULL;
shortcut *writefile_list = NULL;
shortcut *insertfile_list = NULL;
#ifndef DISABLE_HELP
shortcut *help_list = NULL;
#endif
#ifndef DISABLE_SPELLER
shortcut *spell_list = NULL;
#endif
#ifndef NANO_SMALL
shortcut *extcmd_list = NULL;
#endif
#ifndef DISABLE_BROWSER
shortcut *browser_list = NULL;
shortcut *gotodir_list = NULL;
#endif

#ifdef ENABLE_COLOR
const colortype *colorstrings = NULL;
syntaxtype *syntaxes = NULL;
char *syntaxstr = NULL;
#endif

const shortcut *currshortcut;	/* Current shortcut list we're using */

#ifndef NANO_SMALL
toggle *toggles = NULL;
#endif

#ifndef NANO_SMALL
filestruct *search_history = NULL;
filestruct *searchage = NULL;
filestruct *searchbot = NULL;
filestruct *replace_history = NULL;
filestruct *replaceage = NULL;
filestruct *replacebot = NULL;
#endif

/* Regular expressions */
#ifdef HAVE_REGEX_H
regex_t search_regexp;		/* Global to store compiled search regexp */
regmatch_t regmatches[10];	/* Match positions for parenthetical
				   subexpressions, max of 10 */
#endif

bool curses_ended = FALSE;	/* Indicates to statusbar() to simply
				 * write to stderr, since endwin() has
				 * ended curses mode. */

char *homedir = NULL;		/* $HOME or from /etc/passwd. */

size_t length_of_list(const shortcut *s)
{
    size_t i = 0;

    for (; s != NULL; s = s->next)
	i++;
    return i;
}

/* Initialize a struct *without* our lovely braces =( */
void sc_init_one(shortcut **shortcutage, int ctrlval, const char *desc,
#ifndef DISABLE_HELP
	const char *help,
#endif
	int metaval, int funcval, int miscval, bool view, void
	(*func)(void))
{
    shortcut *s;

    if (*shortcutage == NULL) {
	*shortcutage = (shortcut *)nmalloc(sizeof(shortcut));
	s = *shortcutage;
    } else {
	for (s = *shortcutage; s->next != NULL; s = s->next)
	    ;
	s->next = (shortcut *)nmalloc(sizeof(shortcut));
	s = s->next; 
    }

    s->ctrlval = ctrlval;
    s->desc = _(desc);
#ifndef DISABLE_HELP
    s->help = _(help);
#endif
    s->metaval = metaval;
    s->funcval = funcval;
    s->miscval = miscval;
    s->viewok = view;
    s->func = func;
    s->next = NULL;
}

void shortcut_init(bool unjustify)
{
    const char *get_help_msg = N_("Get Help");
    const char *exit_msg = N_("Exit");
    const char *prev_page_msg = N_("Prev Page");
    const char *next_page_msg = N_("Next Page");
    const char *replace_msg = N_("Replace");
    const char *go_to_line_msg = N_("Go To Line");
    const char *cancel_msg = N_("Cancel");
    const char *first_line_msg = N_("First Line");
    const char *last_line_msg = N_("Last Line");
    const char *refresh_msg = N_("Refresh");
#ifndef NANO_SMALL
    const char *cut_till_end_msg = N_("CutTillEnd");
#endif
#ifndef DISABLE_JUSTIFY
    const char *beg_of_par_msg = N_("Beg of Par");
    const char *end_of_par_msg = N_("End of Par");
    const char *fulljstify_msg = N_("FullJstify");
#endif
#ifndef NANO_SMALL
    const char *case_sens_msg = N_("Case Sens");
    const char *direction_msg = N_("Direction");
#endif
#ifdef HAVE_REGEX_H
    const char *regexp_msg = N_("Regexp");
#endif
#ifndef NANO_SMALL
    const char *history_msg = N_("History");
#ifdef ENABLE_MULTIBUFFER
    const char *new_buffer_msg = N_("New Buffer");
#endif
#endif
#ifndef DISABLE_BROWSER
    const char *to_files_msg = N_("To Files");
#endif
#ifndef DISABLE_HELP
    const char *nano_help_msg = N_("Invoke the help menu");
    const char *nano_exit_msg =
#ifdef ENABLE_MULTIBUFFER
	N_("Close currently loaded file/Exit from nano")
#else
   	N_("Exit from nano")
#endif
	;
    const char *nano_writeout_msg =
	N_("Write the current file to disk");
    const char *nano_justify_msg = N_("Justify the current paragraph");
    const char *nano_insert_msg =
	N_("Insert another file into the current one");
    const char *nano_whereis_msg =
	N_("Search for text within the editor");
    const char *nano_prevpage_msg = N_("Move to the previous screen");
    const char *nano_nextpage_msg = N_("Move to the next screen");
    const char *nano_cut_msg =
	N_("Cut the current line and store it in the cutbuffer");
    const char *nano_uncut_msg =
	N_("Uncut from the cutbuffer into the current line");
    const char *nano_cursorpos_msg =
	N_("Show the position of the cursor");
    const char *nano_spell_msg =
	N_("Invoke the spell checker, if available");
    const char *nano_gotoline_msg =
	N_("Go to a specific line number and column number");
    const char *nano_replace_msg = N_("Replace text within the editor");
#ifndef NANO_SMALL
    const char *nano_mark_msg = N_("Mark text at the cursor position");
    const char *nano_whereis_next_msg = N_("Repeat last search");
#endif
    const char *nano_prevline_msg = N_("Move to the previous line");
    const char *nano_nextline_msg = N_("Move to the next line");
    const char *nano_forward_msg = N_("Move forward one character");
    const char *nano_back_msg = N_("Move back one character");
    const char *nano_home_msg =
	N_("Move to the beginning of the current line");
    const char *nano_end_msg =
	N_("Move to the end of the current line");
    const char *nano_refresh_msg =
	N_("Refresh (redraw) the current screen");
    const char *nano_delete_msg =
	N_("Delete the character under the cursor");
    const char *nano_backspace_msg =
	N_("Delete the character to the left of the cursor");
    const char *nano_tab_msg =
	N_("Insert a tab character at the cursor position");
    const char *nano_enter_msg =
	N_("Insert a carriage return at the cursor position");
#ifndef NANO_SMALL
    const char *nano_nextword_msg = N_("Move forward one word");
    const char *nano_prevword_msg = N_("Move backward one word");
#endif
#ifndef DISABLE_JUSTIFY
    const char *nano_parabegin_msg =
	N_("Go to the beginning of the current paragraph");
    const char *nano_paraend_msg =
	N_("Go to the end of the current paragraph");
#endif
#ifdef ENABLE_MULTIBUFFER
    const char *nano_openprev_msg =
	N_("Switch to the previous file buffer");
    const char *nano_opennext_msg =
	N_("Switch to the next file buffer");
#endif
    const char *nano_verbatim_msg = N_("Insert character(s) verbatim");
#ifndef NANO_SMALL
    const char *nano_cut_till_end_msg =
	N_("Cut from the cursor position to the end of the file");
#endif
#ifndef DISABLE_JUSTIFY
    const char *nano_fulljustify_msg = N_("Justify the entire file");
#endif
#if !defined(NANO_SMALL) && defined(HAVE_REGEX_H)
    const char *nano_bracket_msg = N_("Find other bracket");
#endif
    const char *nano_cancel_msg = N_("Cancel the current function");
    const char *nano_firstline_msg =
	N_("Go to the first line of the file");
    const char *nano_lastline_msg =
	N_("Go to the last line of the file");
#ifndef NANO_SMALL
    const char *nano_case_msg =
	N_("Make the current search/replace case (in)sensitive");
    const char *nano_reverse_msg =
	N_("Make the current search/replace go backwards");
#endif
#ifdef HAVE_REGEX_H
    const char *nano_regexp_msg = N_("Use regular expressions");
#endif
#ifndef NANO_SMALL
    const char *nano_history_msg =
	N_("Edit the previous search/replace strings");
#endif
#ifndef DISABLE_BROWSER
    const char *nano_tofiles_msg = N_("Go to file browser");
#endif
#ifndef NANO_SMALL
    const char *nano_dos_msg = N_("Write file out in DOS format");
    const char *nano_mac_msg = N_("Write file out in Mac format");
#endif
    const char *nano_append_msg = N_("Append to the current file");
    const char *nano_prepend_msg = N_("Prepend to the current file");
#ifndef NANO_SMALL
    const char *nano_backup_msg =
	N_("Back up original file when saving");
    const char *nano_execute_msg = N_("Execute external command");
#endif
#if !defined(NANO_SMALL) && defined(ENABLE_MULTIBUFFER)
    const char *nano_multibuffer_msg = N_("Insert into new buffer");
#endif
#ifndef DISABLE_BROWSER
    const char *nano_exitbrowser_msg = N_("Exit from the file browser");
    const char *nano_gotodir_msg = N_("Go to directory");
#endif
#endif /* !DISABLE_HELP */

/* The following macro is to be used in calling sc_init_one().  The
 * point is that sc_init_one() takes 9 arguments, unless DISABLE_HELP is
 * defined, when the 4th one should not be there. */
#ifndef DISABLE_HELP
#define IFHELP(help, nextvar) help, nextvar
#else
#define IFHELP(help, nextvar) nextvar
#endif

    free_shortcutage(&main_list);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_EXIT_KEY,
#ifdef ENABLE_MULTIBUFFER
	open_files != NULL && open_files != open_files->next ?
	N_("Close") :
#endif
	exit_msg, IFHELP(nano_exit_msg, NANO_NO_KEY), NANO_EXIT_FKEY,
	NANO_NO_KEY, VIEW, do_exit);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_WRITEOUT_KEY, N_("WriteOut"),
	IFHELP(nano_writeout_msg, NANO_NO_KEY), NANO_WRITEOUT_FKEY,
	NANO_NO_KEY, NOVIEW, do_writeout_void);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_JUSTIFY_KEY, N_("Justify"),
	IFHELP(nano_justify_msg, NANO_NO_KEY),
	NANO_JUSTIFY_FKEY, NANO_NO_KEY, NOVIEW,
#ifndef DISABLE_JUSTIFY
		do_justify_void
#else
		nano_disabled_msg
#endif
		);

    /* We allow inserting files in view mode if multibuffers are
     * available, so that we can view multiple files. */
    /* If we're using restricted mode, inserting files is disabled since
     * it allows reading from or writing to files not specified on the
     * command line. */
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_INSERTFILE_KEY, N_("Read File"),
	IFHELP(nano_insert_msg, NANO_NO_KEY), NANO_INSERTFILE_FKEY,
	NANO_NO_KEY,
#ifdef ENABLE_MULTIBUFFER
		VIEW
#else
		NOVIEW
#endif
		, !ISSET(RESTRICTED) ? do_insertfile_void :
		nano_disabled_msg);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_WHEREIS_KEY, N_("Where Is"),
	IFHELP(nano_whereis_msg, NANO_NO_KEY), NANO_WHEREIS_FKEY,
	NANO_NO_KEY, VIEW, do_search);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_PREVPAGE_KEY, prev_page_msg,
	IFHELP(nano_prevpage_msg, NANO_NO_KEY), NANO_PREVPAGE_FKEY,
	NANO_NO_KEY, VIEW, do_page_up);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_NEXTPAGE_KEY, next_page_msg,
	IFHELP(nano_nextpage_msg, NANO_NO_KEY), NANO_NEXTPAGE_FKEY,
	NANO_NO_KEY, VIEW, do_page_down);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_CUT_KEY, N_("Cut Text"),
	IFHELP(nano_cut_msg, NANO_NO_KEY), NANO_CUT_FKEY,
	NANO_NO_KEY, NOVIEW, do_cut_text);

    if (unjustify)
    /* Translators: try to keep this string under 10 characters long */
	sc_init_one(&main_list, NANO_UNJUSTIFY_KEY, N_("UnJustify"),
		IFHELP(NULL, NANO_NO_KEY), NANO_UNJUSTIFY_FKEY,
		NANO_NO_KEY, NOVIEW, NULL);
    else
    /* Translators: try to keep this string under 10 characters long */
	sc_init_one(&main_list, NANO_UNCUT_KEY, N_("UnCut Txt"),
		IFHELP(nano_uncut_msg, NANO_NO_KEY), NANO_UNCUT_FKEY,
		NANO_NO_KEY, NOVIEW, do_uncut_text);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_CURSORPOS_KEY, N_("Cur Pos"),
	IFHELP(nano_cursorpos_msg, NANO_NO_KEY), NANO_CURSORPOS_FKEY,
	NANO_NO_KEY, VIEW, do_cursorpos_void);

    /* If we're using restricted mode, spell checking is disabled
     * because it allows reading from or writing to files not specified
     * on the command line. */
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_SPELL_KEY, N_("To Spell"),
		IFHELP(nano_spell_msg, NANO_NO_KEY), NANO_SPELL_FKEY,
		NANO_NO_KEY, NOVIEW,
#ifndef DISABLE_SPELLER
		!ISSET(RESTRICTED) ? do_spell :
#endif
		nano_disabled_msg);

    sc_init_one(&main_list, NANO_GOTOLINE_KEY, go_to_line_msg,
	IFHELP(nano_gotoline_msg, NANO_GOTOLINE_ALTKEY),
	NANO_GOTOLINE_FKEY, NANO_NO_KEY, VIEW,
	do_gotolinecolumn_void);

    sc_init_one(&main_list, NANO_REPLACE_KEY, replace_msg,
	IFHELP(nano_replace_msg, NANO_ALT_REPLACE_KEY),
	NANO_REPLACE_FKEY, NANO_NO_KEY, NOVIEW, do_replace);

#ifndef NANO_SMALL
    sc_init_one(&main_list, NANO_MARK_KEY, N_("Mark Text"),
	IFHELP(nano_mark_msg, NANO_MARK_ALTKEY), NANO_MARK_FKEY,
	NANO_NO_KEY, NOVIEW, do_mark);

    sc_init_one(&main_list, NANO_NO_KEY, N_("Where Is Next"),
	IFHELP(nano_whereis_next_msg, NANO_WHEREIS_NEXT_KEY),
	NANO_WHEREIS_NEXT_FKEY, NANO_NO_KEY, VIEW, do_research);
#endif

    sc_init_one(&main_list, NANO_PREVLINE_KEY, N_("Prev Line"),
	IFHELP(nano_prevline_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_up);

    sc_init_one(&main_list, NANO_NEXTLINE_KEY, N_("Next Line"),
	IFHELP(nano_nextline_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_down);

    sc_init_one(&main_list, NANO_FORWARD_KEY, N_("Forward"),
	IFHELP(nano_forward_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_right_void);

    sc_init_one(&main_list, NANO_BACK_KEY, N_("Back"),
	IFHELP(nano_back_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_left_void);

    sc_init_one(&main_list, NANO_HOME_KEY, N_("Home"),
	IFHELP(nano_home_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_home);

    sc_init_one(&main_list, NANO_END_KEY, N_("End"),
	IFHELP(nano_end_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_end);

    sc_init_one(&main_list, NANO_REFRESH_KEY, refresh_msg,
	IFHELP(nano_refresh_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, total_refresh);

    sc_init_one(&main_list, NANO_DELETE_KEY, N_("Delete"),
	IFHELP(nano_delete_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_delete);

    sc_init_one(&main_list, NANO_BACKSPACE_KEY, N_("Backspace"),
	IFHELP(nano_backspace_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_backspace);

    sc_init_one(&main_list, NANO_TAB_KEY, N_("Tab"),
	IFHELP(nano_tab_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_tab);

    sc_init_one(&main_list, NANO_ENTER_KEY, N_("Enter"),
	IFHELP(nano_enter_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_enter);

#ifndef NANO_SMALL
    sc_init_one(&main_list, NANO_NEXTWORD_KEY, N_("Next Word"),
	IFHELP(nano_nextword_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_next_word);

    sc_init_one(&main_list, NANO_NO_KEY, N_("Prev Word"),
	IFHELP(nano_prevword_msg, NANO_PREVWORD_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_prev_word);
#endif

#ifndef DISABLE_JUSTIFY
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_NO_KEY, beg_of_par_msg,
	IFHELP(nano_parabegin_msg, NANO_PARABEGIN_ALTKEY1), NANO_NO_KEY,
	NANO_PARABEGIN_ALTKEY2, VIEW, do_para_begin_void);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_NO_KEY, end_of_par_msg,
	IFHELP(nano_paraend_msg, NANO_PARAEND_ALTKEY1), NANO_NO_KEY,
	NANO_PARAEND_ALTKEY2, VIEW, do_para_end_void);
#endif

#ifdef ENABLE_MULTIBUFFER
    sc_init_one(&main_list, NANO_NO_KEY, N_("Previous File"),
	IFHELP(nano_openprev_msg, NANO_OPENPREV_KEY), NANO_NO_KEY,
	NANO_OPENPREV_ALTKEY, VIEW, open_prevfile_void);

    sc_init_one(&main_list, NANO_NO_KEY, N_("Next File"),
	IFHELP(nano_opennext_msg, NANO_OPENNEXT_KEY), NANO_NO_KEY,
	NANO_OPENNEXT_ALTKEY, VIEW, open_nextfile_void);
#endif

    sc_init_one(&main_list, NANO_NO_KEY, N_("Verbatim Input"),
	IFHELP(nano_verbatim_msg, NANO_VERBATIM_KEY), NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_verbatim_input);

#ifndef NANO_SMALL
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_NO_KEY, cut_till_end_msg,
	IFHELP(nano_cut_till_end_msg, NANO_CUTTILLEND_ALTKEY),
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_cut_till_end);
#endif

#ifndef DISABLE_JUSTIFY
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&main_list, NANO_NO_KEY, fulljstify_msg,
	IFHELP(nano_fulljustify_msg, NANO_FULLJUSTIFY_ALTKEY),
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_full_justify);
#endif

#if !defined(NANO_SMALL) && defined(HAVE_REGEX_H)
    sc_init_one(&main_list, NANO_NO_KEY, N_("Find Other Bracket"),
	IFHELP(nano_bracket_msg, NANO_BRACKET_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_find_bracket);
#endif

    free_shortcutage(&whereis_list);

    sc_init_one(&whereis_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_FIRSTLINE_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, NANO_NO_KEY), NANO_FIRSTLINE_FKEY,
	NANO_NO_KEY, VIEW, do_first_line);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_LASTLINE_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, NANO_NO_KEY), NANO_LASTLINE_FKEY,
	NANO_NO_KEY, VIEW, do_last_line);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_TOOTHERSEARCH_KEY, replace_msg,
	IFHELP(nano_replace_msg, NANO_NO_KEY), NANO_REPLACE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_TOGOTOLINE_KEY, go_to_line_msg,
	IFHELP(nano_gotoline_msg, NANO_NO_KEY), NANO_GOTOLINE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

#ifndef DISABLE_JUSTIFY
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_PARABEGIN_KEY, beg_of_par_msg,
	IFHELP(nano_parabegin_msg, NANO_PARABEGIN_ALTKEY1), NANO_NO_KEY,
	NANO_PARABEGIN_ALTKEY2, VIEW, do_para_begin_void);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_PARAEND_KEY, end_of_par_msg,
	IFHELP(nano_paraend_msg, NANO_PARAEND_ALTKEY1), NANO_NO_KEY,
	NANO_PARAEND_ALTKEY2, VIEW, do_para_end_void);
#endif

#ifndef NANO_SMALL
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_NO_KEY, case_sens_msg,
	IFHELP(nano_case_msg, TOGGLE_CASE_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_NO_KEY, direction_msg,
	IFHELP(nano_reverse_msg, TOGGLE_BACKWARDS_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifdef HAVE_REGEX_H
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_NO_KEY, regexp_msg,
	IFHELP(nano_regexp_msg, NANO_REGEXP_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifndef NANO_SMALL
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_PREVLINE_KEY, history_msg,
	IFHELP(nano_history_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_CUTTILLEND_KEY, cut_till_end_msg,
	IFHELP(nano_cut_till_end_msg, NANO_CUTTILLEND_ALTKEY),
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_cut_till_end);
#endif

#ifndef DISABLE_JUSTIFY
    /* Translators: try to keep this string under 10 characters long */
    sc_init_one(&whereis_list, NANO_FULLJUSTIFY_KEY, fulljstify_msg,
	IFHELP(nano_fulljustify_msg, NANO_FULLJUSTIFY_ALTKEY),
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_full_justify);
#endif

    free_shortcutage(&replace_list);

    sc_init_one(&replace_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&replace_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&replace_list, NANO_FIRSTLINE_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, NANO_NO_KEY), NANO_FIRSTLINE_FKEY,
	NANO_NO_KEY, VIEW, do_first_line);

    sc_init_one(&replace_list, NANO_LASTLINE_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, NANO_NO_KEY), NANO_LASTLINE_FKEY,
	NANO_NO_KEY, VIEW, do_last_line);

    /* Translators: try to keep this string under 12 characters long */
    sc_init_one(&replace_list, NANO_TOOTHERSEARCH_KEY, N_("No Replace"),
	IFHELP(nano_whereis_msg, NANO_NO_KEY), NANO_REPLACE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&replace_list, NANO_TOGOTOLINE_KEY, go_to_line_msg,
	IFHELP(nano_gotoline_msg, NANO_NO_KEY), NANO_GOTOLINE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

#ifndef NANO_SMALL
    sc_init_one(&replace_list, NANO_NO_KEY, case_sens_msg,
	IFHELP(nano_case_msg, TOGGLE_CASE_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&replace_list, NANO_NO_KEY, direction_msg,
	IFHELP(nano_reverse_msg, TOGGLE_BACKWARDS_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifdef HAVE_REGEX_H
    sc_init_one(&replace_list, NANO_NO_KEY, regexp_msg,
	IFHELP(nano_regexp_msg, NANO_REGEXP_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifndef NANO_SMALL
    sc_init_one(&replace_list, NANO_PREVLINE_KEY, history_msg,
	IFHELP(nano_history_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

    free_shortcutage(&replace_list_2);

    sc_init_one(&replace_list_2, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&replace_list_2, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&replace_list_2, NANO_FIRSTLINE_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_first_line);

    sc_init_one(&replace_list_2, NANO_LASTLINE_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_last_line);

#ifndef NANO_SMALL
    sc_init_one(&replace_list_2, NANO_PREVLINE_KEY, history_msg,
	IFHELP(nano_history_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

    free_shortcutage(&gotoline_list);

    sc_init_one(&gotoline_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&gotoline_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&gotoline_list, NANO_FIRSTLINE_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_first_line);

    sc_init_one(&gotoline_list, NANO_LASTLINE_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_last_line);

    sc_init_one(&gotoline_list, NANO_TOOTHERWHEREIS_KEY,
	N_("Go To Text"), IFHELP(nano_whereis_msg, NANO_NO_KEY),
	NANO_NO_KEY, NANO_NO_KEY, VIEW, NULL);

#ifndef DISABLE_HELP
    free_shortcutage(&help_list);

    sc_init_one(&help_list, NANO_REFRESH_KEY, refresh_msg,
	IFHELP(nano_refresh_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_EXIT_KEY, exit_msg,
	IFHELP(nano_exit_msg, NANO_NO_KEY), NANO_EXIT_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_PREVPAGE_KEY, prev_page_msg,
	IFHELP(nano_prevpage_msg, NANO_NO_KEY), NANO_PREVPAGE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_NEXTPAGE_KEY, next_page_msg,
	IFHELP(nano_nextpage_msg, NANO_NO_KEY), NANO_NEXTPAGE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_PREVLINE_KEY, N_("Prev Line"),
	IFHELP(nano_prevline_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_NEXTLINE_KEY, N_("Next Line"),
	IFHELP(nano_nextline_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

    free_shortcutage(&writefile_list);

    sc_init_one(&writefile_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&writefile_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

#ifndef DISABLE_BROWSER
    /* If we're using restricted mode, the file browser is disabled.
     * It's useless since inserting files is disabled. */
    /* Translators: try to keep this string under 16 characters long */
    if (!ISSET(RESTRICTED))
	sc_init_one(&writefile_list, NANO_TOFILES_KEY, to_files_msg,
		IFHELP(nano_tofiles_msg, NANO_NO_KEY), NANO_NO_KEY,
		NANO_NO_KEY, NOVIEW, NULL);
#endif

#ifndef NANO_SMALL
    /* If we're using restricted mode, the DOS format, Mac format,
     * append, prepend, and backup toggles are disabled.  The first and
     * second are useless since inserting files is disabled, the third
     * and fourth are disabled because they allow writing to files not
     * specified on the command line, and the fifth is useless since
     * backups are disabled. */
    /* Translators: try to keep this string under 16 characters long */
    if (!ISSET(RESTRICTED))
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("DOS Format"),
		IFHELP(nano_dos_msg, TOGGLE_DOS_KEY), NANO_NO_KEY,
		NANO_NO_KEY, NOVIEW, NULL);

    /* Translators: try to keep this string under 16 characters long */
    if (!ISSET(RESTRICTED))
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("Mac Format"),
		IFHELP(nano_mac_msg, TOGGLE_MAC_KEY), NANO_NO_KEY,
		NANO_NO_KEY, NOVIEW, NULL);
#endif

    /* Translators: try to keep this string under 16 characters long */
    if (!ISSET(RESTRICTED))
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("Append"),
		IFHELP(nano_append_msg, NANO_APPEND_KEY), NANO_NO_KEY,
		NANO_NO_KEY, NOVIEW, NULL);

    /* Translators: try to keep this string under 16 characters long */
    if (!ISSET(RESTRICTED))
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("Prepend"),
		IFHELP(nano_prepend_msg, NANO_PREPEND_KEY), NANO_NO_KEY,
		NANO_NO_KEY, NOVIEW, NULL);

#ifndef NANO_SMALL
    /* Translators: try to keep this string under 16 characters long */
    if (!ISSET(RESTRICTED))
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("Backup File"),
		IFHELP(nano_backup_msg, TOGGLE_BACKUP_KEY), NANO_NO_KEY,
		NANO_NO_KEY, NOVIEW, NULL);
#endif

    free_shortcutage(&insertfile_list);

    sc_init_one(&insertfile_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&insertfile_list, NANO_CANCEL_KEY, cancel_msg,
		IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
		NANO_NO_KEY, VIEW, NULL);

#ifndef DISABLE_BROWSER
    /* If we're using restricted mode, the file browser is disabled.
     * It's useless since inserting files is disabled. */
    if (!ISSET(RESTRICTED))
	sc_init_one(&insertfile_list, NANO_TOFILES_KEY, to_files_msg,
		IFHELP(nano_tofiles_msg, NANO_NO_KEY), NANO_NO_KEY,
		NANO_NO_KEY, NOVIEW, NULL);
#endif

#ifndef NANO_SMALL
    /* If we're using restricted mode, command execution is disabled.
     * It's useless since inserting files is disabled. */
    /* Translators: try to keep this string under 22 characters long */
    if (!ISSET(RESTRICTED))
	sc_init_one(&insertfile_list, NANO_TOOTHERINSERT_KEY,
		N_("Execute Command"), IFHELP(nano_execute_msg,
		NANO_NO_KEY), NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);

#ifdef ENABLE_MULTIBUFFER
    /* If we're using restricted mode, the multibuffer toggle is
     * disabled.  It's useless since inserting files is disabled. */
    /* Translators: try to keep this string under 22 characters long */
    if (!ISSET(RESTRICTED))
	sc_init_one(&insertfile_list, NANO_NO_KEY, new_buffer_msg,
		IFHELP(nano_multibuffer_msg, TOGGLE_MULTIBUFFER_KEY),
		NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);
#endif
#endif

#ifndef DISABLE_SPELLER
    free_shortcutage(&spell_list);

    sc_init_one(&spell_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&spell_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifndef NANO_SMALL
    free_shortcutage(&extcmd_list);

    sc_init_one(&extcmd_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&extcmd_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&extcmd_list, NANO_TOOTHERINSERT_KEY, N_("Insert File"),
	IFHELP(nano_insert_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

#ifdef ENABLE_MULTIBUFFER
    sc_init_one(&extcmd_list, NANO_NO_KEY, new_buffer_msg,
	IFHELP(nano_multibuffer_msg, TOGGLE_MULTIBUFFER_KEY),
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);
#endif
#endif

#ifndef DISABLE_BROWSER
    free_shortcutage(&browser_list);

    sc_init_one(&browser_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&browser_list, NANO_EXIT_KEY, exit_msg,
	IFHELP(nano_exitbrowser_msg, NANO_NO_KEY), NANO_EXIT_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&browser_list, NANO_PREVPAGE_KEY, prev_page_msg,
	IFHELP(nano_prevpage_msg, NANO_NO_KEY), NANO_PREVPAGE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&browser_list, NANO_NEXTPAGE_KEY, next_page_msg,
	IFHELP(nano_nextpage_msg, NANO_NO_KEY), NANO_NEXTPAGE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    /* Translators: try to keep this string under 22 characters long */
    sc_init_one(&browser_list, NANO_GOTOLINE_KEY, N_("Go To Dir"),
	IFHELP(nano_gotodir_msg, NANO_GOTOLINE_ALTKEY),
	NANO_GOTOLINE_FKEY, NANO_NO_KEY, VIEW, NULL);

    free_shortcutage(&gotodir_list);

    sc_init_one(&gotodir_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, NANO_NO_KEY), NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
		do_help
#else
		nano_disabled_msg
#endif
		);

    sc_init_one(&gotodir_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, NANO_NO_KEY), NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

    currshortcut = main_list;

#ifndef NANO_SMALL
    toggle_init();
#endif
}

/* Deallocate the given shortcut. */
void free_shortcutage(shortcut **shortcutage)
{
    assert(shortcutage != NULL);

    while (*shortcutage != NULL) {
	shortcut *ps = *shortcutage;
	*shortcutage = (*shortcutage)->next;
	free(ps);
    }
}

#ifndef NANO_SMALL
/* Create one new toggle structure, at the end of the toggles linked
 * list. */
void toggle_init_one(int val, const char *desc, long flag)
{
    toggle *u;

    if (toggles == NULL) {
	toggles = (toggle *)nmalloc(sizeof(toggle));
	u = toggles;
    } else {
	for (u = toggles; u->next != NULL; u = u->next)
	    ;
	u->next = (toggle *)nmalloc(sizeof(toggle));
	u = u->next;
    }

    u->val = val;
    u->desc = _(desc);
    u->flag = flag;
    u->next = NULL;
}

void toggle_init(void)
{
    /* There is no need to reinitialize the toggles.  They can't
     * change. */
    if (toggles != NULL)
	return;

    toggle_init_one(TOGGLE_NOHELP_KEY, N_("Help mode"), NO_HELP);
#ifdef ENABLE_MULTIBUFFER
    /* If we're using restricted mode, the multibuffer toggle is
     * disabled.  It's useless since inserting files is disabled. */
    if (!ISSET(RESTRICTED))
	toggle_init_one(TOGGLE_MULTIBUFFER_KEY,
		N_("Multiple file buffers"), MULTIBUFFER);
#endif
    toggle_init_one(TOGGLE_CONST_KEY, N_("Constant cursor position"),
	CONSTUPDATE);
    toggle_init_one(TOGGLE_AUTOINDENT_KEY, N_("Auto indent"),
	AUTOINDENT);
#ifndef DISABLE_WRAPPING
    toggle_init_one(TOGGLE_WRAP_KEY, N_("Auto line wrap"), NO_WRAP);
#endif
    toggle_init_one(TOGGLE_CUTTOEND_KEY, N_("Cut to end"), CUT_TO_END);
    /* If we're using restricted mode, the suspend toggle is disabled.
     * It's useless since suspending is disabled. */
    if (!ISSET(RESTRICTED))
	toggle_init_one(TOGGLE_SUSPEND_KEY, N_("Suspend"), SUSPEND);
#ifndef DISABLE_MOUSE
    toggle_init_one(TOGGLE_MOUSE_KEY, N_("Mouse support"), USE_MOUSE);
#endif
    /* If we're using restricted mode, the DOS/Mac conversion toggle is
     * disabled.  It's useless since inserting files is disabled. */
    if (!ISSET(RESTRICTED))
	toggle_init_one(TOGGLE_NOCONVERT_KEY,
		N_("No conversion from DOS/Mac format"), NO_CONVERT);
    /* If we're using restricted mode, the backup toggle is disabled.
     * It's useless since backups are disabled. */
    if (!ISSET(RESTRICTED))
	toggle_init_one(TOGGLE_BACKUP_KEY, N_("Backup files"),
		BACKUP_FILE);
    toggle_init_one(TOGGLE_SMOOTH_KEY, N_("Smooth scrolling"),
	SMOOTHSCROLL);
    toggle_init_one(TOGGLE_SMARTHOME_KEY, N_("Smart home key"),
	SMART_HOME);
#ifdef ENABLE_COLOR
    toggle_init_one(TOGGLE_SYNTAX_KEY, N_("Color syntax highlighting"),
	NO_COLOR_SYNTAX);
#endif
#ifdef ENABLE_NANORC
    toggle_init_one(TOGGLE_WHITESPACE_KEY, N_("Whitespace display"),
	WHITESPACE_DISPLAY);
#endif
    toggle_init_one(TOGGLE_MORESPACE_KEY,
	N_("Use of more space for editing"), MORE_SPACE);
}
#endif /* !NANO_SMALL */

/* This function is called just before calling exit().  Practically, the
 * only effect is to cause a segmentation fault if the various data
 * structures got bolloxed earlier.  Thus, we don't bother having this
 * function unless debugging is turned on. */
#ifdef DEBUG
/* Added by SPK for memory cleanup; gracefully return our malloc()s. */
void thanks_for_all_the_fish(void)
{
    delwin(topwin);
    delwin(edit);
    delwin(bottomwin);

#ifndef DISABLE_JUSTIFY
    if (quotestr != NULL)
	free(quotestr);
#ifdef HAVE_REGEX_H
    regfree(&quotereg);
    if (quoteerr != NULL)
	free(quoteerr);
#endif
#endif
#ifndef NANO_SMALL
    if (backup_dir != NULL)
        free(backup_dir);
#endif
#ifndef DISABLE_OPERATINGDIR
    if (operating_dir != NULL)
	free(operating_dir);
    if (full_operating_dir != NULL)
	free(full_operating_dir);
#endif
    if (last_search != NULL)
	free(last_search);
    if (last_replace != NULL)
	free(last_replace);
    if (hblank != NULL)
	free(hblank);
#ifndef DISABLE_SPELLER
    if (alt_speller != NULL)
	free(alt_speller);
#endif
#ifndef DISABLE_HELP
    if (help_text != NULL)
	free(help_text);
#endif
    if (filename != NULL)
	free(filename);
    if (answer != NULL)
	free(answer);
    if (cutbuffer != NULL)
	free_filestruct(cutbuffer);
#ifndef DISABLE_JUSTIFY
    if (jusbuffer != NULL)
	free_filestruct(jusbuffer);
#endif
    free_shortcutage(&main_list);
    free_shortcutage(&whereis_list);
    free_shortcutage(&replace_list);
    free_shortcutage(&replace_list_2);
    free_shortcutage(&gotoline_list);
    free_shortcutage(&writefile_list);
    free_shortcutage(&insertfile_list);
#ifndef DISABLE_HELP
    free_shortcutage(&help_list);
#endif
#ifndef DISABLE_SPELLER
    free_shortcutage(&spell_list);
#endif
#ifndef NANO_SMALL
    free_shortcutage(&extcmd_list);
#endif
#ifndef DISABLE_BROWSER
    free_shortcutage(&browser_list);
    free_shortcutage(&gotodir_list);
#endif
#ifndef NANO_SMALL
    /* Free the memory associated with each toggle. */
    while (toggles != NULL) {
	toggle *t = toggles;

	toggles = toggles->next;
	free(t);
    }
#endif
#ifdef ENABLE_MULTIBUFFER
    /* Free the memory associated with each open file buffer. */
    if (open_files != NULL) {
	/* Make sure open_files->fileage is up to date, in case we've
	 * cut the top line of the file. */
	open_files->fileage = fileage;

	free_openfilestruct(open_files);
    }
#else
    if (fileage != NULL)
	free_filestruct(fileage);
#endif
#ifdef ENABLE_COLOR
    if (syntaxstr != NULL)
	free(syntaxstr);
    while (syntaxes != NULL) {
	syntaxtype *bill = syntaxes;

	free(syntaxes->desc);
	while (syntaxes->extensions != NULL) {
	    exttype *bob = syntaxes->extensions;

	    syntaxes->extensions = bob->next;
	    regfree(&bob->val);
	    free(bob);
	}
	while (syntaxes->color != NULL) {
	    colortype *bob = syntaxes->color;

	    syntaxes->color = bob->next;
	    regfree(&bob->start);
	    if (bob->end != NULL)
		regfree(bob->end);
	    free(bob->end);
	    free(bob);
	}
	syntaxes = syntaxes->next;
	free(bill);
    }
#endif /* ENABLE_COLOR */
#ifndef NANO_SMALL
    /* Free the search and replace history lists. */
    if (searchage != NULL)
	free_filestruct(searchage);
    if (replaceage != NULL)
	free_filestruct(replaceage);
#endif
#ifdef ENABLE_NANORC
    if (homedir != NULL)
	free(homedir);
#endif
}
#endif /* DEBUG */
