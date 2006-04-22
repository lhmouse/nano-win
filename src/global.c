/* $Id$ */
/**************************************************************************
 *   global.c                                                             *
 *                                                                        *
 *   Copyright (C) 1999-2004 Chris Allegretta                             *
 *   Copyright (C) 2005-2006 David Lawrence Ramsey                        *
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

#include "proto.h"

/* Global variables */
#ifndef DISABLE_WRAPJUSTIFY
ssize_t fill = 0;
	/* The column where we will wrap lines. */
ssize_t wrap_at = -CHARS_FROM_EOL;
	/* The position where we will wrap lines.  fill is equal to this
	 * if it's greater than zero, and equal to (COLS + this) if it
	 * isn't. */
#endif

char *last_search = NULL;
	/* The last string we searched for. */
char *last_replace = NULL;
	/* The last replacement string we searched for. */

long flags = 0;
	/* Our flag containing the states of all global options. */
WINDOW *topwin;
	/* The top portion of the window, where we display the version
	 * number of nano, the name of the current file, and whether the
	 * current file has been modified. */
WINDOW *edit;
	/* The middle portion of the window, i.e, the edit window, where
	 * we display the current file we're editing. */
WINDOW *bottomwin;
	/* The bottom portion of the window, where we display statusbar
	 * messages, the statusbar prompt, and a list of shortcuts. */
int editwinrows = 0;
	/* How many rows does the edit window take up? */

filestruct *cutbuffer = NULL;
	/* The buffer where we store cut text. */
#ifndef DISABLE_JUSTIFY
filestruct *jusbuffer = NULL;
	/* The buffer where we store unjustified text. */
#endif
partition *filepart = NULL;
	/* The partition where we store a portion of the current
	 * file. */
openfilestruct *openfile = NULL;
	/* The list of all open file buffers. */

#ifndef NANO_TINY
char *matchbrackets = NULL;
	/* The opening and closing brackets that can be found by bracket
	 * searches. */
#endif

#if !defined(NANO_TINY) && defined(ENABLE_NANORC)
char *whitespace = NULL;
	/* The characters used when displaying the first characters of
	 * tabs and spaces. */
int whitespace_len[2];
	/* The length of these characters. */
#endif

#ifndef DISABLE_JUSTIFY
char *punct = NULL;
	/* The closing punctuation that can end sentences. */
char *brackets = NULL;
	/* The closing brackets that can follow closing punctuation and
	 * can end sentences. */
char *quotestr = NULL;
	/* The quoting string.  The default value is set in main(). */
#ifdef HAVE_REGEX_H
regex_t quotereg;
	/* The compiled regular expression from the quoting string. */
int quoterc;
	/* Whether it actually compiled. */
char *quoteerr = NULL;
	/* The error message, if it didn't. */
#else
size_t quotelen;
	/* The length of the quoting string in bytes. */
#endif
#endif

char *answer = NULL;
	/* The answer string used in the statusbar prompt. */

ssize_t tabsize = -1;
	/* The width of a tab in spaces.  The default value is set in
	 * main(). */

#ifndef NANO_TINY
char *backup_dir = NULL;
	/* The directory where we store backup files. */
#endif
#ifndef DISABLE_OPERATINGDIR
char *operating_dir = NULL;
	/* The relative path to the operating directory, which we can't
	 * move outside of. */
char *full_operating_dir = NULL;
	/* The full path to it. */
#endif

#ifndef DISABLE_SPELLER
char *alt_speller = NULL;
	/* The command to use for the alternate spell checker. */
#endif

shortcut *main_list = NULL;
	/* The main shortcut list. */
shortcut *whereis_list = NULL;
	/* The "Search" shortcut list. */
shortcut *replace_list = NULL;
	/* The "Search (to replace)" shortcut list. */
shortcut *replace_list_2 = NULL;
	/* The "Replace with" shortcut list. */
shortcut *gotoline_list = NULL;
	/* The "Enter line number, column number" shortcut list. */
shortcut *writefile_list = NULL;
	/* The "File Name to Write" shortcut list. */
shortcut *insertfile_list = NULL;
	/* The "File to insert" shortcut list. */
#ifndef NANO_TINY
shortcut *extcmd_list = NULL;
	/* The "Command to execute" shortcut list. */
#endif
#ifndef DISABLE_HELP
shortcut *help_list = NULL;
	/* The help text shortcut list. */
#endif
#ifndef DISABLE_SPELLER
shortcut *spell_list = NULL;
	/* The internal spell checker shortcut list. */
#endif
#ifndef DISABLE_BROWSER
shortcut *browser_list = NULL;
	/* The file browser shortcut list. */
shortcut *whereis_file_list = NULL;
	/* The file browser "Search" shortcut list. */
shortcut *gotodir_list = NULL;
	/* The "Go To Directory" shortcut list. */
#endif

#ifdef ENABLE_COLOR
syntaxtype *syntaxes = NULL;
	/* The global list of color syntaxes. */
char *syntaxstr = NULL;
	/* The color syntax name specified on the command line. */
#endif

const shortcut *currshortcut;
	/* The current shortcut list we're using. */
#ifndef NANO_TINY
toggle *toggles = NULL;
	/* The global toggle list. */
#endif

#ifndef NANO_TINY
filestruct *search_history = NULL;
	/* The search string history list. */
filestruct *searchage = NULL;
	/* The top of the search string history list. */
filestruct *searchbot = NULL;
	/* The bottom of the search string history list. */
filestruct *replace_history = NULL;
	/* The replace string history list. */
filestruct *replaceage = NULL;
	/* The top of the replace string history list. */
filestruct *replacebot = NULL;
	/* The bottom of the replace string history list. */
#endif

/* Regular expressions */
#ifdef HAVE_REGEX_H
regex_t search_regexp;
	/* The compiled regular expression to use in searches. */
regmatch_t regmatches[10];
	/* The match positions for parenthetical subexpressions, 10
	 * maximum, used in regular expression searches. */
#endif

int reverse_attr = A_REVERSE;
	/* The curses attribute we use for reverse video. */
bool curses_ended = FALSE;
	/* Whether endwin() has ended curses mode and statusbar()
	 * should hence write to stderr instead of displaying on the
	 * statusbar. */

char *homedir = NULL;
		/* The user's home directory, from $HOME or
		 * /etc/passwd. */

/* Return the number of entries in the shortcut list s. */
size_t length_of_list(const shortcut *s)
{
    size_t i = 0;

    for (; s != NULL; s = s->next)
	i++;
    return i;
}

/* Add a new shortcut to the end of the shortcut list. */
void sc_init_one(shortcut **shortcutage, int ctrlval, const char *desc,
#ifndef DISABLE_HELP
	const char *help,
#endif
	bool blank_after, int metaval, int funcval, int miscval, bool
	view, void (*func)(void))
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
    s->desc = (desc == NULL) ? "" : _(desc);
#ifndef DISABLE_HELP
    s->help = (help == NULL) ? "" : _(help);
#endif
    s->blank_after = blank_after;
    s->metaval = metaval;
    s->funcval = funcval;
    s->miscval = miscval;
    s->viewok = view;
    s->func = func;
    s->next = NULL;
}

/* Initialize all shortcut lists.  If unjustify is TRUE, replace the
 * Uncut shortcut in the main shortcut list with UnJustify. */
void shortcut_init(bool unjustify)
{
    /* TRANSLATORS: Try to keep this and following strings at most 10 characters. */
    const char *cancel_msg = N_("Cancel");
    const char *get_help_msg = N_("Get Help");
    const char *exit_msg = N_("Exit");
    const char *whereis_msg = N_("Where Is");
    const char *prev_page_msg = N_("Prev Page");
    const char *next_page_msg = N_("Next Page");
    const char *go_to_line_msg = N_("Go To Line");
    /* TRANSLATORS: Try to keep this and previous strings at most 10 characters. */
    const char *replace_msg = N_("Replace");
#ifndef NANO_TINY
    /* TRANSLATORS: Try to keep this at most 16 characters. */
    const char *whereis_next_msg = N_("Where Is Next");
#endif
    /* TRANSLATORS: Try to keep this and following strings at most 10 characters. */
    const char *refresh_msg = N_("Refresh");
    const char *first_line_msg = N_("First Line");
    const char *last_line_msg = N_("Last Line");
#ifndef NANO_TINY
    const char *cut_till_end_msg = N_("CutTillEnd");
#endif
#ifndef DISABLE_JUSTIFY
    const char *beg_of_par_msg = N_("Beg of Par");
    const char *end_of_par_msg = N_("End of Par");
    const char *fulljstify_msg = N_("FullJstify");
#endif
#ifndef NANO_TINY
    const char *case_sens_msg = N_("Case Sens");
    const char *backwards_msg = N_("Backwards");
#endif
#ifdef HAVE_REGEX_H
    const char *regexp_msg = N_("Regexp");
#endif
#ifndef NANO_TINY
    /* TRANSLATORS: Try to keep this and previous strings at most 10 characters. */
    const char *history_msg = N_("History");
#ifdef ENABLE_MULTIBUFFER
    /* TRANSLATORS: Try to keep this at most 16 characters. */
    const char *new_buffer_msg = N_("New Buffer");
#endif
#endif
#ifndef DISABLE_BROWSER
    /* TRANSLATORS: Try to keep this and following strings at most 16 characters. */
    const char *to_files_msg = N_("To Files");
    const char *first_file_msg = N_("First File");
    /* TRANSLATORS: Try to keep this and previous strings at most 16 characters. */
    const char *last_file_msg = N_("Last File");
#endif
#ifndef DISABLE_HELP
    const char *nano_cancel_msg = N_("Cancel the current function");
    const char *nano_help_msg = N_("Display this help text");
    const char *nano_exit_msg =
#ifdef ENABLE_MULTIBUFFER
	N_("Close the current file buffer/Exit from nano")
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
	N_("Display the position of the cursor");
    const char *nano_spell_msg =
	N_("Invoke the spell checker, if available");
    const char *nano_gotoline_msg = N_("Go to line and column number");
    const char *nano_replace_msg = N_("Replace text within the editor");
#ifndef NANO_TINY
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
    const char *nano_delete_msg =
	N_("Delete the character under the cursor");
    const char *nano_backspace_msg =
	N_("Delete the character to the left of the cursor");
    const char *nano_tab_msg =
	N_("Insert a tab character at the cursor position");
    const char *nano_enter_msg =
	N_("Insert a carriage return at the cursor position");
    const char *nano_refresh_msg =
	N_("Refresh (redraw) the current screen");
#ifndef NANO_TINY
    const char *nano_nextword_msg = N_("Move forward one word");
    const char *nano_prevword_msg = N_("Move backward one word");
    const char *nano_wordcount_msg =
	N_("Count the number of words, lines, and characters");
    const char *nano_scrollup_msg =
	N_("Scroll up one line without scrolling the cursor");
    const char *nano_scrolldown_msg =
	N_("Scroll down one line without scrolling the cursor");
#endif
#ifndef DISABLE_JUSTIFY
    const char *nano_parabegin_msg =
	N_("Go to the beginning of the current paragraph");
    const char *nano_paraend_msg =
	N_("Go to the end of the current paragraph");
#endif
#ifdef ENABLE_MULTIBUFFER
    const char *nano_prevfile_msg =
	N_("Switch to the previous file buffer");
    const char *nano_nextfile_msg =
	N_("Switch to the next file buffer");
#endif
    const char *nano_verbatim_msg = N_("Insert character(s) verbatim");
#ifndef NANO_TINY
    const char *nano_cut_till_end_msg =
	N_("Cut from the cursor position to the end of the file");
#endif
#ifndef DISABLE_JUSTIFY
    const char *nano_fulljustify_msg = N_("Justify the entire file");
#endif
#ifndef NANO_TINY
    const char *nano_bracket_msg = N_("Find matching bracket");
#endif
    const char *nano_firstline_msg =
	N_("Go to the first line of the file");
    const char *nano_lastline_msg =
	N_("Go to the last line of the file");
#ifndef NANO_TINY
    const char *nano_case_msg =
	N_("Make the current search/replace case (in)sensitive");
    const char *nano_reverse_msg =
	N_("Make the current search/replace go backwards");
#endif
#ifdef HAVE_REGEX_H
    const char *nano_regexp_msg = N_("Use regular expressions");
#endif
#ifndef NANO_TINY
    const char *nano_history_msg =
	N_("Edit the previous search/replace strings");
#endif
#ifndef DISABLE_BROWSER
    const char *nano_tofiles_msg = N_("Go to file browser");
#endif
#ifndef NANO_TINY
    const char *nano_dos_msg = N_("Write file out in DOS format");
    const char *nano_mac_msg = N_("Write file out in Mac format");
#endif
    const char *nano_append_msg = N_("Append to the current file");
    const char *nano_prepend_msg = N_("Prepend to the current file");
#ifndef NANO_TINY
    const char *nano_backup_msg =
	N_("Back up original file when saving");
    const char *nano_execute_msg = N_("Execute external command");
#endif
#if !defined(NANO_TINY) && defined(ENABLE_MULTIBUFFER)
    const char *nano_multibuffer_msg = N_("Insert into new buffer");
#endif
#ifndef DISABLE_BROWSER
    const char *nano_exitbrowser_msg = N_("Exit from the file browser");
    const char *nano_firstfile_msg =
	N_("Go to the first file in the list");
    const char *nano_lastfile_msg =
	N_("Go to the last file in the list");
    const char *nano_gotodir_msg = N_("Go to directory");
#endif
#endif /* !DISABLE_HELP */

/* The following macro is to be used in calling sc_init_one().  The
 * point is that sc_init_one() takes 10 arguments, unless DISABLE_HELP
 * is defined, when the 4th one should not be there. */
#ifndef DISABLE_HELP
#define IFHELP(help, nextvar) help, nextvar
#else
#define IFHELP(help, nextvar) nextvar
#endif

    free_shortcutage(&main_list);

    sc_init_one(&main_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&main_list, NANO_EXIT_KEY,
#ifdef ENABLE_MULTIBUFFER
	/* TRANSLATORS: Try to keep this at most 10 characters. */
	openfile != NULL && openfile != openfile->next ? N_("Close") :
#endif
	exit_msg, IFHELP(nano_exit_msg, FALSE), NANO_NO_KEY,
	NANO_EXIT_FKEY, NANO_NO_KEY, VIEW, do_exit);

    /* TRANSLATORS: Try to keep this at most 10 characters. */
    sc_init_one(&main_list, NANO_WRITEOUT_KEY, N_("WriteOut"),
	IFHELP(nano_writeout_msg, FALSE), NANO_NO_KEY,
	NANO_WRITEOUT_FKEY, NANO_NO_KEY, NOVIEW, do_writeout_void);

    /* TRANSLATORS: Try to keep this at most 10 characters. */
    sc_init_one(&main_list, NANO_JUSTIFY_KEY, N_("Justify"),
	IFHELP(nano_justify_msg, TRUE), NANO_NO_KEY, NANO_JUSTIFY_FKEY,
	NANO_NO_KEY, NOVIEW,
#ifndef DISABLE_JUSTIFY
	do_justify_void
#else
	nano_disabled_msg
#endif
	);

    /* We allow inserting files in view mode if multibuffers are
     * available, so that we can view multiple files.  If we're using
     * restricted mode, inserting files is disabled, since it allows
     * reading from or writing to files not specified on the command
     * line. */

    /* TRANSLATORS: Try to keep this at most 10 characters. */
    sc_init_one(&main_list, NANO_INSERTFILE_KEY, N_("Read File"),
	IFHELP(nano_insert_msg, FALSE), NANO_NO_KEY,
	NANO_INSERTFILE_FKEY, NANO_NO_KEY,
#ifdef ENABLE_MULTIBUFFER
	VIEW
#else
	NOVIEW
#endif
	, !ISSET(RESTRICTED) ? do_insertfile_void : nano_disabled_msg);

    sc_init_one(&main_list, NANO_WHEREIS_KEY, whereis_msg,
	IFHELP(nano_whereis_msg, FALSE), NANO_NO_KEY, NANO_WHEREIS_FKEY,
	NANO_NO_KEY, VIEW, do_search);

    sc_init_one(&main_list, NANO_PREVPAGE_KEY, prev_page_msg,
	IFHELP(nano_prevpage_msg, FALSE), NANO_NO_KEY,
	NANO_PREVPAGE_FKEY, NANO_NO_KEY, VIEW, do_page_up);

    sc_init_one(&main_list, NANO_NEXTPAGE_KEY, next_page_msg,
	IFHELP(nano_nextpage_msg, TRUE), NANO_NO_KEY,
	NANO_NEXTPAGE_FKEY, NANO_NO_KEY, VIEW, do_page_down);

    /* TRANSLATORS: Try to keep this at most 10 characters. */
    sc_init_one(&main_list, NANO_CUT_KEY, N_("Cut Text"),
	IFHELP(nano_cut_msg, FALSE), NANO_NO_KEY, NANO_CUT_FKEY,
	NANO_NO_KEY, NOVIEW, do_cut_text);

    if (unjustify)
    /* TRANSLATORS: Try to keep this at most 10 characters. */
	sc_init_one(&main_list, NANO_UNJUSTIFY_KEY, N_("UnJustify"),
		IFHELP(NULL, FALSE), NANO_NO_KEY, NANO_UNJUSTIFY_FKEY,
		NANO_NO_KEY, NOVIEW, NULL);
    else
    /* TRANSLATORS: Try to keep this at most 10 characters. */
	sc_init_one(&main_list, NANO_UNCUT_KEY, N_("UnCut Txt"),
		IFHELP(nano_uncut_msg, FALSE), NANO_NO_KEY,
		NANO_UNCUT_FKEY, NANO_NO_KEY, NOVIEW, do_uncut_text);

    /* TRANSLATORS: Try to keep this at most 10 characters. */
    sc_init_one(&main_list, NANO_CURSORPOS_KEY, N_("Cur Pos"),
	IFHELP(nano_cursorpos_msg, FALSE), NANO_NO_KEY,
	NANO_CURSORPOS_FKEY, NANO_NO_KEY, VIEW, do_cursorpos_void);

    /* If we're using restricted mode, spell checking is disabled
     * because it allows reading from or writing to files not specified
     * on the command line. */
    /* TRANSLATORS: Try to keep this at most 10 characters. */
    sc_init_one(&main_list, NANO_SPELL_KEY, N_("To Spell"),
	IFHELP(nano_spell_msg, TRUE), NANO_NO_KEY, NANO_SPELL_FKEY,
	NANO_NO_KEY, NOVIEW,
#ifndef DISABLE_SPELLER
	!ISSET(RESTRICTED) ? do_spell :
#endif
	nano_disabled_msg);

#ifndef NANO_TINY
    sc_init_one(&main_list, NANO_NO_KEY, whereis_next_msg,
	IFHELP(nano_whereis_next_msg, FALSE), NANO_WHEREIS_NEXT_KEY,
	NANO_WHEREIS_NEXT_FKEY, NANO_NO_KEY, VIEW, do_research);
#endif

    sc_init_one(&main_list, NANO_REPLACE_KEY, replace_msg,
	IFHELP(nano_replace_msg, FALSE), NANO_ALT_REPLACE_KEY,
	NANO_REPLACE_FKEY, NANO_NO_KEY, NOVIEW, do_replace);

#ifndef NANO_TINY
    /* TRANSLATORS: Try to keep this at most 16 characters. */
    sc_init_one(&main_list, NANO_MARK_KEY, N_("Mark Text"),
	IFHELP(nano_mark_msg, FALSE), NANO_MARK_ALTKEY, NANO_MARK_FKEY,
	NANO_NO_KEY, VIEW, do_mark);
#endif

#ifndef NANO_TINY
    sc_init_one(&main_list, NANO_NO_KEY, cut_till_end_msg,
	IFHELP(nano_cut_till_end_msg, TRUE), NANO_CUTTILLEND_ALTKEY,
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_cut_till_end);
#endif

    sc_init_one(&main_list, NANO_FORWARD_KEY, N_("Forward"),
	IFHELP(nano_forward_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_right);

    sc_init_one(&main_list, NANO_BACK_KEY, N_("Back"),
	IFHELP(nano_back_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_left);

#ifndef NANO_TINY
    sc_init_one(&main_list, NANO_NEXTWORD_KEY, N_("Next Word"),
	IFHELP(nano_nextword_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_next_word_void);

    sc_init_one(&main_list, NANO_NO_KEY, N_("Prev Word"),
	IFHELP(nano_prevword_msg, FALSE), NANO_PREVWORD_KEY,
	NANO_NO_KEY, NANO_NO_KEY, VIEW, do_prev_word_void);
#endif

    sc_init_one(&main_list, NANO_PREVLINE_KEY, N_("Prev Line"),
	IFHELP(nano_prevline_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_up);

    sc_init_one(&main_list, NANO_NEXTLINE_KEY, N_("Next Line"),
	IFHELP(nano_nextline_msg, TRUE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_down);

    sc_init_one(&main_list, NANO_HOME_KEY, N_("Home"),
	IFHELP(nano_home_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_home);

    sc_init_one(&main_list, NANO_END_KEY, N_("End"),
	IFHELP(nano_end_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_end);

#ifndef DISABLE_JUSTIFY
    sc_init_one(&main_list, NANO_NO_KEY, beg_of_par_msg,
	IFHELP(nano_parabegin_msg, FALSE), NANO_PARABEGIN_ALTKEY,
	NANO_NO_KEY, NANO_PARABEGIN_ALTKEY2, VIEW, do_para_begin_void);

    sc_init_one(&main_list, NANO_NO_KEY, end_of_par_msg,
	IFHELP(nano_paraend_msg, FALSE), NANO_PARAEND_ALTKEY,
	NANO_NO_KEY, NANO_PARAEND_ALTKEY2, VIEW, do_para_end_void);
#endif

    sc_init_one(&main_list, NANO_NO_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, FALSE), NANO_FIRSTLINE_ALTKEY,
	NANO_NO_KEY, NANO_FIRSTLINE_ALTKEY2, VIEW, do_first_line);

    sc_init_one(&main_list, NANO_NO_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, TRUE), NANO_LASTLINE_ALTKEY,
	NANO_NO_KEY, NANO_LASTLINE_ALTKEY2, VIEW, do_last_line);

#ifndef NANO_TINY
    sc_init_one(&main_list, NANO_NO_KEY, N_("Find Other Bracket"),
	IFHELP(nano_bracket_msg, FALSE), NANO_BRACKET_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, do_find_bracket);
#endif

    sc_init_one(&main_list, NANO_GOTOLINE_KEY, go_to_line_msg,
	IFHELP(nano_gotoline_msg, FALSE), NANO_GOTOLINE_ALTKEY,
	NANO_GOTOLINE_FKEY, NANO_NO_KEY, VIEW,
	do_gotolinecolumn_void);

#ifndef NANO_TINY
    sc_init_one(&main_list, NANO_NO_KEY, N_("Scroll Up"),
	IFHELP(nano_scrollup_msg, FALSE), NANO_SCROLLUP_KEY,
	NANO_NO_KEY, NANO_SCROLLUP_ALTKEY, VIEW, do_scroll_up);

    sc_init_one(&main_list, NANO_NO_KEY, N_("Scroll Down"),
	IFHELP(nano_scrolldown_msg, TRUE), NANO_SCROLLDOWN_KEY,
	NANO_NO_KEY, NANO_SCROLLDOWN_ALTKEY, VIEW, do_scroll_down);
#endif

#ifdef ENABLE_MULTIBUFFER
    sc_init_one(&main_list, NANO_NO_KEY, N_("Previous File"),
	IFHELP(nano_prevfile_msg, FALSE), NANO_PREVFILE_KEY,
	NANO_NO_KEY, NANO_PREVFILE_ALTKEY, VIEW,
	switch_to_prev_buffer_void);

    sc_init_one(&main_list, NANO_NO_KEY, N_("Next File"),
	IFHELP(nano_nextfile_msg, TRUE), NANO_NEXTFILE_KEY, NANO_NO_KEY,
	NANO_NEXTFILE_ALTKEY, VIEW, switch_to_next_buffer_void);
#endif

    sc_init_one(&main_list, NANO_NO_KEY, N_("Verbatim Input"),
	IFHELP(nano_verbatim_msg, FALSE), NANO_VERBATIM_KEY,
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_verbatim_input);

    sc_init_one(&main_list, NANO_TAB_KEY, N_("Tab"),
	IFHELP(nano_tab_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_tab);

    sc_init_one(&main_list, NANO_ENTER_KEY, N_("Enter"),
	IFHELP(nano_enter_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_enter);

    sc_init_one(&main_list, NANO_DELETE_KEY, N_("Delete"),
	IFHELP(nano_delete_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_delete);

    sc_init_one(&main_list, NANO_BACKSPACE_KEY, N_("Backspace"),
	IFHELP(nano_backspace_msg, TRUE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, NOVIEW, do_backspace);

#ifndef DISABLE_JUSTIFY
    sc_init_one(&main_list, NANO_NO_KEY, fulljstify_msg,
	IFHELP(nano_fulljustify_msg, FALSE), NANO_FULLJUSTIFY_ALTKEY,
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_full_justify);
#endif

#ifndef NANO_TINY
    sc_init_one(&main_list, NANO_NO_KEY, N_("Word Count"),
	IFHELP(nano_wordcount_msg, FALSE), NANO_WORDCOUNT_KEY,
	NANO_NO_KEY, NANO_NO_KEY, VIEW, do_wordlinechar_count);
#endif

    sc_init_one(&main_list, NANO_REFRESH_KEY, refresh_msg,
#ifndef NANO_TINY
	IFHELP(nano_wordcount_msg, TRUE)
#else
	IFHELP(nano_wordcount_msg, FALSE)
#endif
	, NANO_NO_KEY, NANO_NO_KEY, NANO_NO_KEY, VIEW, total_refresh);

    free_shortcutage(&whereis_list);

    sc_init_one(&whereis_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&whereis_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&whereis_list, NANO_FIRSTLINE_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, FALSE), NANO_FIRSTLINE_ALTKEY,
	NANO_FIRSTLINE_FKEY, NANO_FIRSTLINE_ALTKEY2, VIEW,
	do_first_line);

    sc_init_one(&whereis_list, NANO_LASTLINE_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, FALSE), NANO_LASTLINE_ALTKEY,
	NANO_LASTLINE_FKEY, NANO_LASTLINE_ALTKEY2, VIEW, do_last_line);

    sc_init_one(&whereis_list, NANO_TOOTHERSEARCH_KEY, replace_msg,
	IFHELP(nano_replace_msg, FALSE), NANO_NO_KEY, NANO_REPLACE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&whereis_list, NANO_TOGOTOLINE_KEY, go_to_line_msg,
	IFHELP(nano_gotoline_msg, FALSE), NANO_NO_KEY,
	NANO_GOTOLINE_FKEY, NANO_NO_KEY, VIEW, NULL);

#ifndef DISABLE_JUSTIFY
    sc_init_one(&whereis_list, NANO_PARABEGIN_KEY, beg_of_par_msg,
	IFHELP(nano_parabegin_msg, FALSE), NANO_PARABEGIN_ALTKEY,
	NANO_NO_KEY, NANO_PARABEGIN_ALTKEY2, VIEW, do_para_begin_void);

    sc_init_one(&whereis_list, NANO_PARAEND_KEY, end_of_par_msg,
	IFHELP(nano_paraend_msg, FALSE), NANO_PARAEND_ALTKEY,
	NANO_NO_KEY, NANO_PARAEND_ALTKEY2, VIEW, do_para_end_void);
#endif

#ifndef NANO_TINY
    sc_init_one(&whereis_list, NANO_NO_KEY, case_sens_msg,
	IFHELP(nano_case_msg, FALSE), TOGGLE_CASE_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&whereis_list, NANO_NO_KEY, backwards_msg,
	IFHELP(nano_reverse_msg, FALSE), TOGGLE_BACKWARDS_KEY,
	NANO_NO_KEY, NANO_NO_KEY, VIEW, NULL);
#endif

#ifdef HAVE_REGEX_H
    sc_init_one(&whereis_list, NANO_NO_KEY, regexp_msg,
	IFHELP(nano_regexp_msg, FALSE), NANO_REGEXP_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifndef NANO_TINY
    sc_init_one(&whereis_list, NANO_PREVLINE_KEY, history_msg,
	IFHELP(nano_history_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&whereis_list, NANO_CUTTILLEND_KEY, cut_till_end_msg,
	IFHELP(nano_cut_till_end_msg, FALSE), NANO_CUTTILLEND_ALTKEY,
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_cut_till_end);
#endif

#ifndef DISABLE_JUSTIFY
    sc_init_one(&whereis_list, NANO_FULLJUSTIFY_KEY, fulljstify_msg,
	IFHELP(nano_fulljustify_msg, FALSE), NANO_FULLJUSTIFY_ALTKEY,
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, do_full_justify);
#endif

    free_shortcutage(&replace_list);

    sc_init_one(&replace_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&replace_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&replace_list, NANO_FIRSTLINE_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, FALSE), NANO_FIRSTLINE_ALTKEY,
	NANO_FIRSTLINE_FKEY, NANO_FIRSTLINE_ALTKEY2, VIEW,
	do_first_line);

    sc_init_one(&replace_list, NANO_LASTLINE_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, FALSE), NANO_LASTLINE_ALTKEY,
	NANO_LASTLINE_FKEY, NANO_LASTLINE_ALTKEY2, VIEW, do_last_line);

    /* TRANSLATORS: Try to keep this at most 12 characters. */
    sc_init_one(&replace_list, NANO_TOOTHERSEARCH_KEY, N_("No Replace"),
	IFHELP(nano_whereis_msg, FALSE), NANO_NO_KEY, NANO_REPLACE_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&replace_list, NANO_TOGOTOLINE_KEY, go_to_line_msg,
	IFHELP(nano_gotoline_msg, FALSE), NANO_NO_KEY,
	NANO_GOTOLINE_FKEY, NANO_NO_KEY, VIEW, NULL);

#ifndef NANO_TINY
    sc_init_one(&replace_list, NANO_NO_KEY, case_sens_msg,
	IFHELP(nano_case_msg, FALSE), TOGGLE_CASE_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&replace_list, NANO_NO_KEY, backwards_msg,
	IFHELP(nano_reverse_msg, FALSE), TOGGLE_BACKWARDS_KEY,
	NANO_NO_KEY, NANO_NO_KEY, VIEW, NULL);
#endif

#ifdef HAVE_REGEX_H
    sc_init_one(&replace_list, NANO_NO_KEY, regexp_msg,
	IFHELP(nano_regexp_msg, FALSE), NANO_REGEXP_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifndef NANO_TINY
    sc_init_one(&replace_list, NANO_PREVLINE_KEY, history_msg,
	IFHELP(nano_history_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

    free_shortcutage(&replace_list_2);

    sc_init_one(&replace_list_2, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&replace_list_2, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&replace_list_2, NANO_FIRSTLINE_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, FALSE), NANO_FIRSTLINE_ALTKEY,
	NANO_FIRSTLINE_FKEY, NANO_FIRSTLINE_ALTKEY2, VIEW,
	do_first_line);

    sc_init_one(&replace_list_2, NANO_LASTLINE_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, FALSE), NANO_LASTLINE_ALTKEY,
	NANO_LASTLINE_FKEY, NANO_LASTLINE_ALTKEY2, VIEW, do_last_line);

#ifndef NANO_TINY
    sc_init_one(&replace_list_2, NANO_PREVLINE_KEY, history_msg,
	IFHELP(nano_history_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

    free_shortcutage(&gotoline_list);

    sc_init_one(&gotoline_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&gotoline_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&gotoline_list, NANO_FIRSTLINE_KEY, first_line_msg,
	IFHELP(nano_firstline_msg, FALSE), NANO_FIRSTLINE_ALTKEY,
	NANO_FIRSTLINE_FKEY, NANO_FIRSTLINE_ALTKEY2, VIEW,
	do_first_line);

    sc_init_one(&gotoline_list, NANO_LASTLINE_KEY, last_line_msg,
	IFHELP(nano_lastline_msg, FALSE), NANO_LASTLINE_ALTKEY,
	NANO_LASTLINE_FKEY, NANO_LASTLINE_ALTKEY2, VIEW, do_last_line);

    sc_init_one(&gotoline_list, NANO_TOOTHERWHEREIS_KEY,
	N_("Go To Text"), IFHELP(nano_whereis_msg, FALSE), NANO_NO_KEY,
	NANO_NO_KEY, NANO_NO_KEY, VIEW, NULL);

    free_shortcutage(&writefile_list);

    sc_init_one(&writefile_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&writefile_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

#ifndef DISABLE_BROWSER
    /* If we're using restricted mode, the file browser is disabled.
     * It's useless since inserting files is disabled. */

    if (!ISSET(RESTRICTED))
	sc_init_one(&writefile_list, NANO_TOFILES_KEY, to_files_msg,
		IFHELP(nano_tofiles_msg, FALSE), NANO_NO_KEY,
		NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);
#endif

#ifndef NANO_TINY
    /* If we're using restricted mode, the DOS format, Mac format,
     * append, prepend, and backup toggles are disabled.  The first and
     * second are useless since inserting files is disabled, the third
     * and fourth are disabled because they allow writing to files not
     * specified on the command line, and the fifth is useless since
     * backups are disabled. */

    if (!ISSET(RESTRICTED))
	/* TRANSLATORS: Try to keep this at most 16 characters. */
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("DOS Format"),
		IFHELP(nano_dos_msg, FALSE), TOGGLE_DOS_KEY,
		NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);

    if (!ISSET(RESTRICTED))
	/* TRANSLATORS: Try to keep this at most 16 characters. */
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("Mac Format"),
		IFHELP(nano_mac_msg, FALSE), TOGGLE_MAC_KEY,
		NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);
#endif

    if (!ISSET(RESTRICTED))
	/* TRANSLATORS: Try to keep this at most 16 characters. */
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("Append"),
		IFHELP(nano_append_msg, FALSE), NANO_APPEND_KEY,
		NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);

    if (!ISSET(RESTRICTED))
	/* TRANSLATORS: Try to keep this at most 16 characters. */
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("Prepend"),
		IFHELP(nano_prepend_msg, FALSE), NANO_PREPEND_KEY,
		NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);

#ifndef NANO_TINY
    if (!ISSET(RESTRICTED))
	/* TRANSLATORS: Try to keep this at most 16 characters. */
	sc_init_one(&writefile_list, NANO_NO_KEY, N_("Backup File"),
		IFHELP(nano_backup_msg, FALSE), TOGGLE_BACKUP_KEY,
		NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);
#endif

    free_shortcutage(&insertfile_list);

    sc_init_one(&insertfile_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&insertfile_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

#ifndef DISABLE_BROWSER
    /* If we're using restricted mode, the file browser is disabled.
     * It's useless since inserting files is disabled. */
    if (!ISSET(RESTRICTED))
	sc_init_one(&insertfile_list, NANO_TOFILES_KEY, to_files_msg,
		IFHELP(nano_tofiles_msg, FALSE), NANO_NO_KEY,
		NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);
#endif

#ifndef NANO_TINY
    /* If we're using restricted mode, command execution is disabled.
     * It's useless since inserting files is disabled. */

    if (!ISSET(RESTRICTED))
	sc_init_one(&insertfile_list, NANO_TOOTHERINSERT_KEY,
		/* TRANSLATORS: Try to keep this at most 22 characters. */
		N_("Execute Command"), IFHELP(nano_execute_msg, FALSE),
		NANO_NO_KEY, NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);

#ifdef ENABLE_MULTIBUFFER
    /* If we're using restricted mode, the multibuffer toggle is
     * disabled.  It's useless since inserting files is disabled. */

    if (!ISSET(RESTRICTED))
	sc_init_one(&insertfile_list, NANO_NO_KEY, new_buffer_msg,
		IFHELP(nano_multibuffer_msg, FALSE),
		TOGGLE_MULTIBUFFER_KEY, NANO_NO_KEY, NANO_NO_KEY,
		NOVIEW, NULL);
#endif
#endif

#ifndef NANO_TINY
    free_shortcutage(&extcmd_list);

    sc_init_one(&extcmd_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&extcmd_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&extcmd_list, NANO_TOOTHERINSERT_KEY, N_("Insert File"),
	IFHELP(nano_insert_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

#ifdef ENABLE_MULTIBUFFER
    sc_init_one(&extcmd_list, NANO_NO_KEY, new_buffer_msg,
	IFHELP(nano_multibuffer_msg, FALSE), TOGGLE_MULTIBUFFER_KEY,
	NANO_NO_KEY, NANO_NO_KEY, NOVIEW, NULL);
#endif
#endif

#ifndef DISABLE_HELP
    free_shortcutage(&help_list);

    sc_init_one(&help_list, NANO_REFRESH_KEY, refresh_msg,
	IFHELP(nano_refresh_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_EXIT_KEY, exit_msg,
	IFHELP(nano_exit_msg, FALSE), NANO_NO_KEY, NANO_EXIT_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_PREVPAGE_KEY, prev_page_msg,
	IFHELP(nano_prevpage_msg, FALSE), NANO_NO_KEY,
	NANO_PREVPAGE_FKEY, NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_NEXTPAGE_KEY, next_page_msg,
	IFHELP(nano_nextpage_msg, FALSE), NANO_NO_KEY,
	NANO_NEXTPAGE_FKEY, NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_PREVLINE_KEY, N_("Prev Line"),
	IFHELP(nano_prevline_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&help_list, NANO_NEXTLINE_KEY, N_("Next Line"),
	IFHELP(nano_nextline_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifndef DISABLE_SPELLER
    free_shortcutage(&spell_list);

    sc_init_one(&spell_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&spell_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifndef DISABLE_BROWSER
    free_shortcutage(&browser_list);

    sc_init_one(&browser_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&browser_list, NANO_EXIT_KEY, exit_msg,
	IFHELP(nano_exitbrowser_msg, FALSE), NANO_NO_KEY,
	NANO_EXIT_FKEY, NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&browser_list, NANO_PREVPAGE_KEY, prev_page_msg,
	IFHELP(nano_prevpage_msg, FALSE), NANO_NO_KEY,
	NANO_PREVPAGE_FKEY, NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&browser_list, NANO_NEXTPAGE_KEY, next_page_msg,
	IFHELP(nano_nextpage_msg, FALSE), NANO_NO_KEY,
	NANO_NEXTPAGE_FKEY, NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&browser_list, NANO_WHEREIS_KEY, whereis_msg,
	IFHELP(nano_whereis_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&browser_list, NANO_NO_KEY, whereis_next_msg,
	IFHELP(nano_whereis_next_msg, FALSE), NANO_WHEREIS_NEXT_KEY,
	NANO_WHEREIS_NEXT_FKEY, NANO_NO_KEY, VIEW, NULL);

    /* TRANSLATORS: Try to keep this at most 22 characters. */
    sc_init_one(&browser_list, NANO_GOTOLINE_KEY, N_("Go To Dir"),
	IFHELP(nano_gotodir_msg, FALSE), NANO_GOTOLINE_ALTKEY,
	NANO_GOTOLINE_FKEY, NANO_NO_KEY, VIEW, NULL);

    free_shortcutage(&whereis_file_list);

    sc_init_one(&whereis_file_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_browser_help
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&whereis_file_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&whereis_file_list, NANO_FIRSTFILE_KEY, first_file_msg,
	IFHELP(nano_firstfile_msg, FALSE), NANO_FIRSTFILE_ALTKEY,
	NANO_FIRSTFILE_FKEY, NANO_FIRSTFILE_ALTKEY2, VIEW,
	do_first_file);

    sc_init_one(&whereis_file_list, NANO_LASTFILE_KEY, last_file_msg,
	IFHELP(nano_lastfile_msg, FALSE), NANO_LASTFILE_ALTKEY,
	NANO_LASTFILE_FKEY, NANO_LASTFILE_ALTKEY2, VIEW, do_last_file);

#ifndef NANO_SMALL
    sc_init_one(&whereis_file_list, NANO_NO_KEY, case_sens_msg,
	IFHELP(nano_case_msg, FALSE), TOGGLE_CASE_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);

    sc_init_one(&whereis_file_list, NANO_NO_KEY, backwards_msg,
	IFHELP(nano_reverse_msg, FALSE), TOGGLE_BACKWARDS_KEY,
	NANO_NO_KEY, NANO_NO_KEY, VIEW, NULL);
#endif

#ifdef HAVE_REGEX_H
    sc_init_one(&whereis_file_list, NANO_NO_KEY, regexp_msg,
	IFHELP(nano_regexp_msg, FALSE), NANO_REGEXP_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

#ifndef NANO_SMALL
    sc_init_one(&whereis_file_list, NANO_PREVLINE_KEY, history_msg,
	IFHELP(nano_history_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

    free_shortcutage(&gotodir_list);

    sc_init_one(&gotodir_list, NANO_HELP_KEY, get_help_msg,
	IFHELP(nano_help_msg, FALSE), NANO_NO_KEY, NANO_HELP_FKEY,
	NANO_NO_KEY, VIEW,
#ifndef DISABLE_HELP
	do_help_void
#else
	nano_disabled_msg
#endif
	);

    sc_init_one(&gotodir_list, NANO_CANCEL_KEY, cancel_msg,
	IFHELP(nano_cancel_msg, FALSE), NANO_NO_KEY, NANO_NO_KEY,
	NANO_NO_KEY, VIEW, NULL);
#endif

    currshortcut = main_list;

#ifndef NANO_TINY
    toggle_init();
#endif
}

/* Free the given shortcut. */
void free_shortcutage(shortcut **shortcutage)
{
    assert(shortcutage != NULL);

    while (*shortcutage != NULL) {
	shortcut *ps = *shortcutage;
	*shortcutage = (*shortcutage)->next;
	free(ps);
    }
}

#ifndef NANO_TINY
/* Add a new toggle to the end of the global toggle list. */
void toggle_init_one(int val, const char *desc, bool blank_after, long
	flag)
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
    u->desc = (desc == NULL) ? "" : _(desc);
    u->blank_after = blank_after;
    u->flag = flag;
    u->next = NULL;
}

/* Initialize the global toggle list. */
void toggle_init(void)
{
    /* There is no need to reinitialize the toggles.  They can't
     * change. */
    if (toggles != NULL)
	return;

    toggle_init_one(TOGGLE_NOHELP_KEY, N_("Help mode"), FALSE, NO_HELP);

    toggle_init_one(TOGGLE_CONST_KEY,
	N_("Constant cursor position display"), FALSE, CONST_UPDATE);

    toggle_init_one(TOGGLE_MORESPACE_KEY,
	N_("Use of more space for editing"), FALSE, MORE_SPACE);

    toggle_init_one(TOGGLE_SMOOTH_KEY, N_("Smooth scrolling"), FALSE,
	SMOOTH_SCROLL);

#ifdef ENABLE_NANORC
    toggle_init_one(TOGGLE_WHITESPACE_KEY, N_("Whitespace display"),
	FALSE, WHITESPACE_DISPLAY);
#endif

#ifdef ENABLE_COLOR
    toggle_init_one(TOGGLE_SYNTAX_KEY, N_("Color syntax highlighting"),
	TRUE, NO_COLOR_SYNTAX);
#endif

    toggle_init_one(TOGGLE_SMARTHOME_KEY, N_("Smart home key"), FALSE,
	SMART_HOME);

    toggle_init_one(TOGGLE_AUTOINDENT_KEY, N_("Auto indent"), FALSE,
	AUTOINDENT);

    toggle_init_one(TOGGLE_CUTTOEND_KEY, N_("Cut to end"), FALSE,
	CUT_TO_END);

#ifndef DISABLE_WRAPPING
    toggle_init_one(TOGGLE_WRAP_KEY, N_("Long line wrapping"), FALSE,
	NO_WRAP);
#endif

    toggle_init_one(TOGGLE_TABSTOSPACES_KEY,
	N_("Conversion of typed tabs to spaces"), TRUE, TABS_TO_SPACES);

    /* If we're using restricted mode, the backup toggle is disabled.
     * It's useless since backups are disabled. */
    if (!ISSET(RESTRICTED))
	toggle_init_one(TOGGLE_BACKUP_KEY, N_("Backup files"), FALSE,
		BACKUP_FILE);

#ifdef ENABLE_MULTIBUFFER
    /* If we're using restricted mode, the multibuffer toggle is
     * disabled.  It's useless since inserting files is disabled. */
    if (!ISSET(RESTRICTED))
	toggle_init_one(TOGGLE_MULTIBUFFER_KEY,
		N_("Multiple file buffers"), FALSE, MULTIBUFFER);
#endif

#ifndef DISABLE_MOUSE
    toggle_init_one(TOGGLE_MOUSE_KEY, N_("Mouse support"), FALSE,
	USE_MOUSE);
#endif

    /* If we're using restricted mode, the DOS/Mac conversion toggle is
     * disabled.  It's useless since inserting files is disabled. */
    if (!ISSET(RESTRICTED))
	toggle_init_one(TOGGLE_NOCONVERT_KEY,
		N_("No conversion from DOS/Mac format"), FALSE,
		NO_CONVERT);

    /* If we're using restricted mode, the suspend toggle is disabled.
     * It's useless since suspending is disabled. */
    if (!ISSET(RESTRICTED))
	toggle_init_one(TOGGLE_SUSPEND_KEY, N_("Suspend"), FALSE,
	SUSPEND);
}
#endif /* !NANO_TINY */

#ifdef DEBUG
/* This function is used to gracefully return all the memory we've used.
 * It should be called just before calling exit().  Practically, the
 * only effect is to cause a segmentation fault if the various data
 * structures got bolloxed earlier.  Thus, we don't bother having this
 * function unless debugging is turned on. */
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
#ifndef NANO_TINY
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
#ifndef DISABLE_SPELLER
    if (alt_speller != NULL)
	free(alt_speller);
#endif
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
#ifndef NANO_TINY
    free_shortcutage(&extcmd_list);
#endif
#ifndef DISABLE_HELP
    free_shortcutage(&help_list);
#endif
#ifndef DISABLE_SPELLER
    free_shortcutage(&spell_list);
#endif
#ifndef DISABLE_BROWSER
    free_shortcutage(&browser_list);
    free_shortcutage(&gotodir_list);
#endif
#ifndef NANO_TINY
    /* Free the memory associated with each toggle. */
    while (toggles != NULL) {
	toggle *t = toggles;

	toggles = toggles->next;
	free(t);
    }
#endif
    /* Free the memory associated with each open file buffer. */
    if (openfile != NULL)
	free_openfilestruct(openfile);
#ifdef ENABLE_COLOR
    if (syntaxstr != NULL)
	free(syntaxstr);
    while (syntaxes != NULL) {
	syntaxtype *bill = syntaxes;

	free(syntaxes->desc);
	while (syntaxes->extensions != NULL) {
	    exttype *bob = syntaxes->extensions;

	    syntaxes->extensions = bob->next;
	    free(bob->ext_regex);
	    if (bob->ext != NULL) {
		regfree(bob->ext);
		free(bob->ext);
	    }
	    free(bob);
	}
	while (syntaxes->color != NULL) {
	    colortype *bob = syntaxes->color;

	    syntaxes->color = bob->next;
	    free(bob->start_regex);
	    if (bob->start != NULL) {
		regfree(bob->start);
		free(bob->start);
	    }
	    if (bob->end_regex != NULL)
		free(bob->end_regex);
	    if (bob->end != NULL) {
		regfree(bob->end);
		free(bob->end);
	    }
	    free(bob);
	}
	syntaxes = syntaxes->next;
	free(bill);
    }
#endif /* ENABLE_COLOR */
#ifndef NANO_TINY
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
