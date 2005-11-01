/* $Id$ */
/**************************************************************************
 *   prompt.c                                                             *
 *                                                                        *
 *   Copyright (C) 2005 Chris Allegretta                                  *
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "proto.h"

static char *prompt = NULL;
				/* The prompt string for statusbar
				 * questions. */
static size_t statusbar_x = (size_t)-1;
				/* The cursor position in answer. */
static bool reset_statusbar_x = FALSE;
				/* Should we reset the cursor position
				 * at the statusbar prompt? */

int do_statusbar_input(bool *meta_key, bool *func_key, bool *s_or_t,
	bool *ran_func, bool *finished, bool allow_funcs)
{
    int input;
	/* The character we read in. */
    static int *kbinput = NULL;
	/* The input buffer. */
    static size_t kbinput_len = 0;
	/* The length of the input buffer. */
    const shortcut *s;
    bool have_shortcut;

    *s_or_t = FALSE;
    *ran_func = FALSE;
    *finished = FALSE;

    /* Read in a character. */
    input = get_kbinput(bottomwin, meta_key, func_key);

#ifndef DISABLE_MOUSE
    /* If we got a mouse click and it was on a shortcut, read in the
     * shortcut character. */
    if (allow_funcs && *func_key == TRUE && input == KEY_MOUSE) {
	if (do_statusbar_mouse())
	    input = get_kbinput(bottomwin, meta_key, func_key);
	else
	    input = ERR;
    }
#endif

    /* Check for a shortcut in the current list. */
    s = get_shortcut(currshortcut, &input, meta_key, func_key);

    /* If we got a shortcut from the current list, or a "universal"
     * statusbar prompt shortcut, set have_shortcut to TRUE. */
    have_shortcut = (s != NULL || input == NANO_REFRESH_KEY ||
	input == NANO_HOME_KEY || input == NANO_END_KEY ||
	input == NANO_FORWARD_KEY || input == NANO_BACK_KEY ||
	input == NANO_BACKSPACE_KEY || input == NANO_DELETE_KEY ||
	input == NANO_CUT_KEY ||
#ifndef NANO_SMALL
		input == NANO_NEXTWORD_KEY ||
#endif
		(*meta_key == TRUE && (
#ifndef NANO_SMALL
		input == NANO_PREVWORD_KEY ||
#endif
		input == NANO_VERBATIM_KEY)));

    /* Set s_or_t to TRUE if we got a shortcut. */
    *s_or_t = have_shortcut;

    if (allow_funcs) {
	/* If we got a character, and it isn't a shortcut or toggle,
	 * it's a normal text character.  Display the warning if we're
	 * in view mode, or add the character to the input buffer if
	 * we're not. */
	if (input != ERR && *s_or_t == FALSE) {
	    /* If we're using restricted mode, the filename isn't blank,
	     * and we're at the "Write File" prompt, disable text
	     * input. */
	    if (!ISSET(RESTRICTED) || openfile->filename[0] == '\0' ||
		currshortcut != writefile_list) {
		kbinput_len++;
		kbinput = (int *)nrealloc(kbinput, kbinput_len *
			sizeof(int));
		kbinput[kbinput_len - 1] = input;
	    }
	}

	/* If we got a shortcut, or if there aren't any other characters
	 * waiting after the one we read in, we need to display all the
	 * characters in the input buffer if it isn't empty. */
	 if (*s_or_t == TRUE || get_key_buffer_len() == 0) {
	    if (kbinput != NULL) {

		/* Display all the characters in the input buffer at
		 * once, filtering out control characters. */
		char *output = charalloc(kbinput_len + 1);
		size_t i;
		bool got_enter;
			/* Whether we got the Enter key. */

		for (i = 0; i < kbinput_len; i++)
		    output[i] = (char)kbinput[i];
		output[i] = '\0';

		do_statusbar_output(output, kbinput_len, &got_enter,
			FALSE);

		free(output);

		/* Empty the input buffer. */
		kbinput_len = 0;
		free(kbinput);
		kbinput = NULL;
	    }
	}

	if (have_shortcut) {
	    switch (input) {
		/* Handle the "universal" statusbar prompt shortcuts. */
		case NANO_REFRESH_KEY:
		    total_refresh();
		    break;
		case NANO_HOME_KEY:
		    do_statusbar_home();
		    break;
		case NANO_END_KEY:
		    do_statusbar_end();
		    break;
		case NANO_FORWARD_KEY:
		    do_statusbar_right();
		    break;
		case NANO_BACK_KEY:
		    do_statusbar_left();
		    break;
		case NANO_BACKSPACE_KEY:
		    /* If we're using restricted mode, the filename
		     * isn't blank, and we're at the "Write File"
		     * prompt, disable Backspace. */
		    if (!ISSET(RESTRICTED) || openfile->filename[0] ==
			'\0' || currshortcut != writefile_list)
			do_statusbar_backspace();
		    break;
		case NANO_DELETE_KEY:
		    /* If we're using restricted mode, the filename
		     * isn't blank, and we're at the "Write File"
		     * prompt, disable Delete. */
		    if (!ISSET(RESTRICTED) || openfile->filename[0] ==
			'\0' || currshortcut != writefile_list)
			do_statusbar_delete();
		    break;
		case NANO_CUT_KEY:
		    /* If we're using restricted mode, the filename
		     * isn't blank, and we're at the "Write File"
		     * prompt, disable Cut. */
		    if (!ISSET(RESTRICTED) || openfile->filename[0] ==
			'\0' || currshortcut != writefile_list)
			do_statusbar_cut_text();
		    break;
#ifndef NANO_SMALL
		case NANO_NEXTWORD_KEY:
		    do_statusbar_next_word(FALSE);
		    break;
		case NANO_PREVWORD_KEY:
		    if (*meta_key == TRUE)
			do_statusbar_prev_word(FALSE);
		    break;
#endif
		case NANO_VERBATIM_KEY:
		    if (*meta_key == TRUE) {
			/* If we're using restricted mode, the filename
			 * isn't blank, and we're at the "Write File"
			 * prompt, disable verbatim input. */
			if (!ISSET(RESTRICTED) ||
				openfile->filename[0] == '\0' ||
				currshortcut != writefile_list) {
			    bool got_enter;
				/* Whether we got the Enter key. */

			    do_statusbar_verbatim_input(&got_enter);

			    /* If we got the Enter key, set input to the
			     * key value for Enter, and set finished to
			     * TRUE to indicate that we're done. */
			    if (got_enter) {
				input = NANO_ENTER_KEY;
				*finished = TRUE;
			    }
			}
			break;
		    }
		/* Handle the normal statusbar prompt shortcuts, setting
		 * ran_func to TRUE if we try to run their associated
		 * functions and setting finished to TRUE to indicate
		 * that we're done after trying to run their associated
		 * functions. */
		default:
		    if (s->func != NULL) {
			*ran_func = TRUE;
			if (!ISSET(VIEW_MODE) || s->viewok)
			    s->func();
		    }
		    *finished = TRUE;
	    }
	}
    }

    return input;
}

#ifndef DISABLE_MOUSE
bool do_statusbar_mouse(void)
{
    int mouse_x, mouse_y;
    bool retval = get_mouseinput(&mouse_x, &mouse_y, TRUE);

    if (!retval) {
	/* We can click in the statusbar window text to move the
	 * cursor. */
	if (wenclose(bottomwin, mouse_y, mouse_x)) {
	    size_t start_col;

	    assert(prompt != NULL);

	    start_col = strlenpt(prompt) + 1;

	    /* Subtract out the sizes of topwin and edit. */
	    mouse_y -= (2 - no_more_space()) + editwinrows;

	    /* Move to where the click occurred. */
	    if (mouse_x > start_col && mouse_y == 0) {
		statusbar_x = actual_x(answer,
			get_statusbar_page_start(start_col, start_col +
			statusbar_xplustabs()) + mouse_x - start_col -
			1);
		nanoget_repaint(answer, statusbar_x);
	    }
	}
    }

    return retval;
}
#endif

/* The user typed output_len multibyte characters.  Add them to the
 * statusbar prompt, setting got_enter to TRUE if we get a newline, and
 * filtering out all control characters if allow_cntrls is TRUE. */
void do_statusbar_output(char *output, size_t output_len, bool
	*got_enter, bool allow_cntrls)
{
    size_t answer_len, i = 0;
    char *char_buf = charalloc(mb_cur_max());
    int char_buf_len;

    assert(answer != NULL);

    answer_len = strlen(answer);
    *got_enter = FALSE;

    while (i < output_len) {
	/* If allow_cntrls is FALSE, filter out nulls and newlines,
	 * since they're control characters. */
	if (allow_cntrls) {
	    /* Null to newline, if needed. */
	    if (output[i] == '\0')
		output[i] = '\n';
	    /* Newline to Enter, if needed. */
	    else if (output[i] == '\n') {
		/* Set got_enter to TRUE to indicate that we got the
		 * Enter key, put back the rest of the characters in
		 * output so that they can be parsed and output again,
		 * and get out. */
		*got_enter = TRUE;
		unparse_kbinput(output + i, output_len - i);
		return;
	    }
	}

	/* Interpret the next multibyte character. */
	char_buf_len = parse_mbchar(output + i, char_buf, NULL);

	i += char_buf_len;

	/* If allow_cntrls is FALSE, filter out a control character. */
	if (!allow_cntrls && is_cntrl_mbchar(output + i - char_buf_len))
	    continue;

	/* More dangerousness fun =) */
	answer = charealloc(answer, answer_len + (char_buf_len * 2));

	assert(statusbar_x <= answer_len);

	charmove(&answer[statusbar_x + char_buf_len],
		&answer[statusbar_x], answer_len - statusbar_x +
		char_buf_len);
	strncpy(&answer[statusbar_x], char_buf, char_buf_len);
	answer_len += char_buf_len;

	statusbar_x += char_buf_len;
    }

    free(char_buf);
}

void do_statusbar_home(void)
{
#ifndef NANO_SMALL
    if (ISSET(SMART_HOME)) {
	size_t statusbar_x_save = statusbar_x;

	statusbar_x = indent_length(answer);

	if (statusbar_x == statusbar_x_save ||
		statusbar_x == strlen(answer))
	    statusbar_x = 0;
    } else
#endif
	statusbar_x = 0;
}

void do_statusbar_end(void)
{
    statusbar_x = strlen(answer);
}

void do_statusbar_right(void)
{
    if (statusbar_x < strlen(answer))
	statusbar_x = move_mbright(answer, statusbar_x);
}

void do_statusbar_left(void)
{
    if (statusbar_x > 0)
	statusbar_x = move_mbleft(answer, statusbar_x);
}

void do_statusbar_backspace(void)
{
    if (statusbar_x > 0) {
	do_statusbar_left();
	do_statusbar_delete();
    }
}

void do_statusbar_delete(void)
{
    if (answer[statusbar_x] != '\0') {
	int char_buf_len = parse_mbchar(answer + statusbar_x, NULL,
		NULL);
	size_t line_len = strlen(answer + statusbar_x);

	assert(statusbar_x < strlen(answer));

	charmove(answer + statusbar_x, answer + statusbar_x +
		char_buf_len, strlen(answer) - statusbar_x -
		char_buf_len + 1);

	null_at(&answer, statusbar_x + line_len - char_buf_len);
    }
}

/* Move text from the statusbar prompt into oblivion. */
void do_statusbar_cut_text(void)
{
    assert(answer != NULL);

#ifndef NANO_SMALL
    if (ISSET(CUT_TO_END))
	null_at(&answer, statusbar_x);
    else {
#endif
	null_at(&answer, 0);
	statusbar_x = 0;
#ifndef NANO_SMALL
    }
#endif
}

#ifndef NANO_SMALL
/* Move to the next word at the statusbar prompt.  If allow_punct is
 * TRUE, treat punctuation as part of a word.  Return TRUE if we started
 * on a word, and FALSE otherwise. */
bool do_statusbar_next_word(bool allow_punct)
{
    char *char_mb;
    int char_mb_len;
    bool end_line = FALSE, started_on_word = FALSE;

    assert(answer != NULL);

    char_mb = charalloc(mb_cur_max());

    /* Move forward until we find the character after the last letter of
     * the current word. */
    while (!end_line) {
	char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

	/* If we've found it, stop moving forward through the current
	 * line. */
	if (!is_word_mbchar(char_mb, allow_punct))
	    break;

	/* If we haven't found it, then we've started on a word, so set
	 * started_on_word to TRUE. */
	started_on_word = TRUE;

	if (answer[statusbar_x] == '\0')
	    end_line = TRUE;
	else
	    statusbar_x += char_mb_len;
    }

    /* Move forward until we find the first letter of the next word. */
    if (answer[statusbar_x] == '\0')
	end_line = TRUE;
    else
	statusbar_x += char_mb_len;

    while (!end_line) {
	char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

	/* If we've found it, stop moving forward through the current
	 * line. */
	if (is_word_mbchar(char_mb, allow_punct))
	    break;

	if (answer[statusbar_x] == '\0')
	    end_line = TRUE;
	else
	    statusbar_x += char_mb_len;
    }

    free(char_mb);

    /* Return whether we started on a word. */
    return started_on_word;
}

/* Move to the previous word at the statusbar prompt.  If allow_punct is
 * TRUE, treat punctuation as part of a word.  Return TRUE if we started
 * on a word, and FALSE otherwise. */
bool do_statusbar_prev_word(bool allow_punct)
{
    char *char_mb;
    int char_mb_len;
    bool begin_line = FALSE, started_on_word = FALSE;

    assert(answer != NULL);

    char_mb = charalloc(mb_cur_max());

    /* Move backward until we find the character before the first letter
     * of the current word. */
    while (!begin_line) {
	char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

	/* If we've found it, stop moving backward through the current
	 * line. */
	if (!is_word_mbchar(char_mb, allow_punct))
	    break;

	/* If we haven't found it, then we've started on a word, so set
	 * started_on_word to TRUE. */
	started_on_word = TRUE;

	if (statusbar_x == 0)
	    begin_line = TRUE;
	else
	    statusbar_x = move_mbleft(answer, statusbar_x);
    }

    /* Move backward until we find the last letter of the previous
     * word. */
    if (statusbar_x == 0)
	begin_line = TRUE;
    else
	statusbar_x = move_mbleft(answer, statusbar_x);

    while (!begin_line) {
	char_mb_len = parse_mbchar(answer + statusbar_x, char_mb, NULL);

	/* If we've found it, stop moving backward through the current
	 * line. */
	if (is_word_mbchar(char_mb, allow_punct))
	    break;

	if (statusbar_x == 0)
	    begin_line = TRUE;
	else
	    statusbar_x = move_mbleft(answer, statusbar_x);
    }

    /* If we've found it, move backward until we find the character
     * before the first letter of the previous word. */
    if (!begin_line) {
	if (statusbar_x == 0)
	    begin_line = TRUE;
	else
	    statusbar_x = move_mbleft(answer, statusbar_x);

	while (!begin_line) {
	    char_mb_len = parse_mbchar(answer + statusbar_x, char_mb,
		NULL);

	    /* If we've found it, stop moving backward through the
	     * current line. */
	    if (!is_word_mbchar(char_mb, allow_punct))
		break;

	    if (statusbar_x == 0)
		begin_line = TRUE;
	    else
		statusbar_x = move_mbleft(answer, statusbar_x);
	}

	/* If we've found it, move forward to the first letter of the
	 * previous word. */
	if (!begin_line)
	    statusbar_x += char_mb_len;
    }

    free(char_mb);

    /* Return whether we started on a word. */
    return started_on_word;
}
#endif /* !NANO_SMALL */

void do_statusbar_verbatim_input(bool *got_enter)
{
    int *kbinput;
    size_t kbinput_len, i;
    char *output;

    *got_enter = FALSE;

    /* Read in all the verbatim characters. */
    kbinput = get_verbatim_kbinput(bottomwin, &kbinput_len);

    /* Display all the verbatim characters at once, not filtering out
     * control characters. */
    output = charalloc(kbinput_len + 1);

    for (i = 0; i < kbinput_len; i++)
	output[i] = (char)kbinput[i];
    output[i] = '\0';

    do_statusbar_output(output, kbinput_len, got_enter, TRUE);

    free(output);
}

/* Return the placewewant associated with statusbar_x, i.e, the
 * zero-based column position of the cursor.  The value will be no
 * smaller than statusbar_x. */
size_t statusbar_xplustabs(void)
{
    return strnlenpt(answer, statusbar_x);
}

/* nano scrolls horizontally within a line in chunks.  This function
 * returns the column number of the first character displayed in the
 * statusbar prompt when the cursor is at the given column with the
 * prompt ending at start_col.  Note that (0 <= column -
 * get_statusbar_page_start(column) < COLS). */
size_t get_statusbar_page_start(size_t start_col, size_t column)
{
    if (column == start_col || column < COLS - 1)
	return 0;
    else
	return column - start_col - (column - start_col) % (COLS -
		start_col - 1);
}

/* Repaint the statusbar when getting a character in nanogetstr().  Note
 * that we must turn on A_REVERSE here, since do_help() turns it off! */
void nanoget_repaint(const char *buf, size_t x)
{
    size_t start_col, xpt, page_start;
    char *expanded;

    assert(prompt != NULL && x <= strlen(buf));

    start_col = strlenpt(prompt) + 1;
    xpt = strnlenpt(buf, x);
    page_start = get_statusbar_page_start(start_col, start_col + xpt);

    wattron(bottomwin, A_REVERSE);

    blank_statusbar();

    mvwaddnstr(bottomwin, 0, 0, prompt, actual_x(prompt, COLS - 2));
    waddch(bottomwin, ':');
    waddch(bottomwin, (page_start == 0) ? ' ' : '$');

    expanded = display_string(buf, page_start, COLS - start_col - 1,
	FALSE);
    waddstr(bottomwin, expanded);
    free(expanded);

    wmove(bottomwin, 0, start_col + xpt + 1 - page_start);

    wattroff(bottomwin, A_REVERSE);
}

/* Get the input from the keyboard.  This should only be called from
 * statusq(). */
int nanogetstr(bool allow_tabs, const char *curranswer,
#ifndef NANO_SMALL
	filestruct **history_list,
#endif
	const shortcut *s
#ifndef DISABLE_TABCOMP
	, bool *list
#endif
	)
{
    int kbinput;
    bool meta_key, func_key, s_or_t, ran_func, finished;
    size_t curranswer_len;
#ifndef DISABLE_TABCOMP
    bool tabbed = FALSE;
	/* Whether we've pressed Tab. */
#endif
#ifndef NANO_SMALL
    char *history = NULL;
	/* The current history string. */
    char *magichistory = NULL;
	/* The temporary string typed at the bottom of the history, if
	 * any. */
#ifndef DISABLE_TABCOMP
    int last_kbinput = ERR;
	/* The key we pressed before the current key. */
    size_t complete_len = 0;
	/* The length of the original string that we're trying to
	 * tab complete, if any. */
#endif
#endif /* !NANO_SMALL */

    answer = mallocstrcpy(answer, curranswer);
    curranswer_len = strlen(answer);

    /* Only put statusbar_x at the end of the string if it's
     * uninitialized, if it would be past the end of curranswer, or if
     * reset_statusbar_x is TRUE.  Otherwise, leave it alone.  This is
     * so the cursor position stays at the same place if a
     * prompt-changing toggle is pressed. */
    if (statusbar_x == (size_t)-1 || statusbar_x > curranswer_len ||
		reset_statusbar_x)
	statusbar_x = curranswer_len;

    currshortcut = s;

    nanoget_repaint(answer, statusbar_x);

    /* Refresh the edit window and the statusbar before getting
     * input. */
    wnoutrefresh(edit);
    wnoutrefresh(bottomwin);

    /* If we're using restricted mode, we aren't allowed to change the
     * name of a file once it has one because that would allow writing
     * to files not specified on the command line.  In this case,
     * disable all keys that would change the text if the filename isn't
     * blank and we're at the "Write File" prompt. */
    while ((kbinput = do_statusbar_input(&meta_key, &func_key,
	&s_or_t, &ran_func, &finished, TRUE)) != NANO_CANCEL_KEY &&
	kbinput != NANO_ENTER_KEY) {

	assert(statusbar_x <= strlen(answer));

#ifndef DISABLE_TABCOMP
	if (kbinput != NANO_TAB_KEY)
	    tabbed = FALSE;
#endif

	switch (kbinput) {
	    case NANO_TAB_KEY:
#ifndef DISABLE_TABCOMP
#ifndef NANO_SMALL
		if (history_list != NULL) {
		    if (last_kbinput != NANO_TAB_KEY)
			complete_len = strlen(answer);

		    if (complete_len > 0) {
			answer = mallocstrcpy(answer,
				get_history_completion(history_list,
				answer, complete_len));
			statusbar_x = strlen(answer);
		    }
		} else
#endif /* !NANO_SMALL */
		if (allow_tabs)
		    answer = input_tab(answer, &statusbar_x, &tabbed,
			list);
#endif /* !DISABLE_TABCOMP */
		break;
	    case NANO_PREVLINE_KEY:
#ifndef NANO_SMALL
		if (history_list != NULL) {
		    /* If we're scrolling up at the bottom of the
		     * history list and answer isn't blank, save answer
		     * in magichistory. */
		    if ((*history_list)->next == NULL &&
			answer[0] != '\0')
			magichistory = mallocstrcpy(magichistory,
				answer);

		    /* Get the older search from the history list and
		     * save it in answer.  If there is no older search,
		     * don't do anything. */
		    if ((history =
			get_history_older(history_list)) != NULL) {
			answer = mallocstrcpy(answer, history);
			statusbar_x = strlen(answer);
		    }

		    /* This key has a shortcut list entry when it's used
		     * to move to an older search, which means that
		     * finished has been set to TRUE.  Set it back to
		     * FALSE here, so that we aren't kicked out of the
		     * statusbar prompt. */
		    finished = FALSE;
		}
#endif /* !NANO_SMALL */
		break;
	    case NANO_NEXTLINE_KEY:
#ifndef NANO_SMALL
		if (history_list != NULL) {
		    /* Get the newer search from the history list and
		     * save it in answer.  If there is no newer search,
		     * don't do anything. */
		    if ((history =
			get_history_newer(history_list)) != NULL) {
			answer = mallocstrcpy(answer, history);
			statusbar_x = strlen(answer);
		    }

		    /* If, after scrolling down, we're at the bottom of
		     * the history list, answer is blank, and
		     * magichistory is set, save magichistory in
		     * answer. */
		    if ((*history_list)->next == NULL &&
			answer[0] == '\0' && magichistory != NULL) {
			answer = mallocstrcpy(answer, magichistory);
			statusbar_x = strlen(answer);
		    }
		}
#endif /* !NANO_SMALL */
		break;
	}

	/* If we have a shortcut with an associated function, break out
	 * if we're finished after running or trying to run the
	 * function. */
	if (finished)
	    break;

#if !defined(NANO_SMALL) && !defined(DISABLE_TABCOMP)
	last_kbinput = kbinput;
#endif

	nanoget_repaint(answer, statusbar_x);
	wnoutrefresh(bottomwin);
    }

#ifndef NANO_SMALL
    /* Set the current position in the history list to the bottom and
     * free magichistory, if we need to. */
    if (history_list != NULL) {
	history_reset(*history_list);

	if (magichistory != NULL)
	    free(magichistory);
    }
#endif

    /* We finished putting in an answer or ran a normal shortcut's
     * associated function, so reset statusbar_x. */
    if (kbinput == NANO_CANCEL_KEY || kbinput == NANO_ENTER_KEY ||
	ran_func)
	statusbar_x = (size_t)-1;

    return kbinput;
}

/* Ask a question on the statusbar.  Answer will be stored in answer
 * global.  Returns -1 on aborted enter, -2 on a blank string, and 0
 * otherwise, the valid shortcut key caught.  curranswer is any editable
 * text that we want to put up by default.
 *
 * The allow_tabs parameter indicates whether we should allow tabs to be
 * interpreted. */
int statusq(bool allow_tabs, const shortcut *s, const char *curranswer,
#ifndef NANO_SMALL
	filestruct **history_list,
#endif
	const char *msg, ...)
{
    va_list ap;
    int retval;
#ifndef DISABLE_TABCOMP
    bool list = FALSE;
#endif

    /* prompt has been freed and set to NULL unless the user resized
     * while at the statusbar prompt. */
    if (prompt != NULL)
	free(prompt);

    prompt = charalloc(((COLS - 4) * mb_cur_max()) + 1);

    bottombars(s);

    va_start(ap, msg);
    vsnprintf(prompt, (COLS - 4) * mb_cur_max(), msg, ap);
    va_end(ap);
    null_at(&prompt, actual_x(prompt, COLS - 4));

    retval = nanogetstr(allow_tabs, curranswer,
#ifndef NANO_SMALL
		history_list,
#endif
		s
#ifndef DISABLE_TABCOMP
		, &list
#endif
		);

    free(prompt);
    prompt = NULL;

    reset_statusbar_x = FALSE;

    switch (retval) {
	case NANO_CANCEL_KEY:
	    retval = -1;
	    reset_statusbar_x = TRUE;
	    break;
	case NANO_ENTER_KEY:
	    retval = (answer[0] == '\0') ? -2 : 0;
	    reset_statusbar_x = TRUE;
	    break;
    }

    blank_statusbar();
    wnoutrefresh(bottomwin);

#ifdef DEBUG
    fprintf(stderr, "answer = \"%s\"\n", answer);
#endif

#ifndef DISABLE_TABCOMP
    /* If we've done tab completion, there might be a list of filename
     * matches on the edit window at this point.  Make sure that they're
     * cleared off. */
    if (list)
	edit_refresh();
#endif

    return retval;
}

void statusq_abort(void)
{
    reset_statusbar_x = TRUE;
}
