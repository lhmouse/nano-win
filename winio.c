/* $Id$ */
/**************************************************************************
 *   winio.c                                                              *
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

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

static int statblank = 0;	/* Number of keystrokes left after
				   we call statusbar(), before we
				   actually blank the statusbar */

int do_first_line(void)
{
    current = fileage;
    placewewant = 0;
    current_x = 0;
    edit_update(current, CENTER);
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

/* Return what current_x should be, given xplustabs() for the line. */
size_t actual_x(const filestruct *fileptr, size_t xplus)
{
    size_t i = 0;
	/* the position in fileptr->data, returned */
    size_t length = 0;
	/* the screen display width to data[i] */
    char *c;
	/* fileptr->data + i */

    assert(fileptr != NULL && fileptr->data != NULL);

    for (c = fileptr->data; length < xplus && *c != '\0'; i++, c++) {
	if (*c == '\t')
	    length += tabsize - length % tabsize;
	else if (is_cntrl_char((int)*c))
	    length += 2;
	else
	    length++;
    }
    assert(length == strnlenpt(fileptr->data, i));
    assert(i <= strlen(fileptr->data));

    if (length > xplus)
	i--;

#ifdef DEBUG
    fprintf(stderr, _("actual_x for xplus=%d returns %d\n"), xplus, i);
#endif

    return i;
}

/* A strlen with tabs factored in, similar to xplustabs(). */
size_t strnlenpt(const char *buf, size_t size)
{
    size_t length = 0;

    if (buf != NULL)
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

size_t strlenpt(const char *buf)
{
    return strnlenpt(buf, -1);
}

void blank_bottombars(void)
{
    if (!no_help()) {
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
    for (i = 0; i <= editwinrows - 1; i++)
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

/* Repaint the statusbar when getting a character in nanogetstr.  buf
 * should be no longer than COLS - 4.
 *
 * Note that we must turn on A_REVERSE here, since do_help turns it
 * off! */
void nanoget_repaint(const char *buf, const char *inputbuf, int x)
{
    int len = strlen(buf) + 2;
    int wid = COLS - len;

    assert(wid >= 2);
    assert(0 <= x && x <= strlen(inputbuf));

    wattron(bottomwin, A_REVERSE);
    blank_statusbar();
    mvwaddstr(bottomwin, 0, 0, buf);
    waddch(bottomwin, ':');
    waddch(bottomwin, x < wid ? ' ' : '$');
    waddnstr(bottomwin, &inputbuf[wid * (x / wid)], wid);
    wmove(bottomwin, 0, (x % wid) + len);
    wattroff(bottomwin, A_REVERSE);
}

/* Get the input from the kb; this should only be called from
 * statusq(). */
int nanogetstr(int allowtabs, const char *buf, const char *def,
		const shortcut *s
#ifndef DISABLE_TABCOMP
		, int *list
#endif
		)
{
    int kbinput;
    int x;
	/* the cursor position in 'answer' */
    int xend;
	/* length of 'answer', the status bar text */
    int tabbed = 0;
	/* used by input_tab() */
    const shortcut *t;

    xend = strlen(def);
    x = xend;
    answer = (char *)nrealloc(answer, xend + 1);
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

    while ((kbinput = wgetch(bottomwin)) != 13) {
	for (t = s; t != NULL; t = t->next) {
#ifdef DEBUG
	    fprintf(stderr, _("Aha! \'%c\' (%d)\n"), kbinput, kbinput);
#endif

	    if (kbinput == t->val && kbinput < 32) {

#ifndef DISABLE_HELP
		/* Have to do this here, it would be too late to do it
		   in statusq() */
		if (kbinput == NANO_HELP_KEY || kbinput == NANO_HELP_FKEY) {
		    do_help();
		    break;
		}
#endif
		return t->val;
	    }
	}
	assert(0 <= x && x <= xend && xend == strlen(answer));

	if (kbinput != '\t')
	    tabbed = 0;

	switch (kbinput) {

	    /* Stuff we want to equate with <enter>, ASCII 13 */
	case 343:
	    ungetch(13);	/* Enter on iris-ansi $TERM, sometimes */
	    break;
	    /* Stuff we want to ignore */
#ifdef PDCURSES
	case 541:
	case 542:
	case 543:		/* Right ctrl again */
	case 544:
	case 545:		/* Right alt again */
	    break;
#endif
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
	case KEY_MOUSE:
	    do_mouse();
	    break;
#endif
#endif
	case NANO_HOME_KEY:
	case KEY_HOME:
	    x = 0;
	    break;
	case NANO_END_KEY:
	case KEY_END:
	    x = xend;
	    break;
	case KEY_RIGHT:
	case NANO_FORWARD_KEY:
	    if (x < xend)
		x++;
	    break;
	case NANO_CONTROL_D:
	    if (x < xend) {
		memmove(answer + x, answer + x + 1, xend - x);
		xend--;
	    }
	    break;
	case NANO_CONTROL_K:
	case NANO_CONTROL_U:
	    null_at(&answer, 0);
	    xend = 0;
	    x = 0;
	    break;
	case KEY_BACKSPACE:
	case 127:
	case NANO_CONTROL_H:
	    if (x > 0) {
		memmove(answer + x - 1, answer + x, xend - x + 1);
		x--;
		xend--;
	    }
	    break;
#ifndef DISABLE_TABCOMP
	case NANO_CONTROL_I:
	    if (allowtabs) {
		int shift = 0;

		answer = input_tab(answer, x, &tabbed, &shift, list);
		xend = strlen(answer);
		x += shift;
		if (x > xend)
		    x = xend;
	    }
	    break;
#endif
	case KEY_LEFT:
	case NANO_BACK_KEY:
	    if (x > 0)
		x--;
	    break;
	case KEY_UP:
	case KEY_DOWN:
	    break;

	case KEY_DC:
	    goto do_deletekey;

	case 27:
	    switch (kbinput = wgetch(edit)) {
	    case 'O':
		switch (kbinput = wgetch(edit)) {
		case 'F':
		    x = xend;
		    break;
		case 'H':
		    x = 0;
		    break;
		}
		break;
	    case '[':
		switch (kbinput = wgetch(edit)) {
		case 'C':
		    if (x < xend)
			x++;
		    break;
		case 'D':
		    if (x > 0)
			x--;
		    break;
		case '1':
		case '7':
		    x = 0;
		    goto skip_tilde;
		case '3':
		  do_deletekey:
		    if (x < xend) {
			memmove(answer + x, answer + x + 1, xend - x);
			xend--;
		    }
		    goto skip_tilde;
		case '4':
		case '8':
		    x = xend;
		    goto skip_tilde;
		  skip_tilde:
		    nodelay(edit, TRUE);
		    kbinput = wgetch(edit);
		    if (kbinput == '~' || kbinput == ERR)
			kbinput = -1;
		    nodelay(edit, FALSE);
		    break;
		}
		break;
	    default:

		for (t = s; t != NULL; t = t->next) {
#ifdef DEBUG
		    fprintf(stderr, _("Aha! \'%c\' (%d)\n"), kbinput,
			    kbinput);
#endif
		    if (kbinput == t->val || kbinput == t->val - 32) {
			/* We hit an Alt key.   Do like above.  We don't
			   just ungetch the letter and let it get caught
			   above cause that screws the keypad... */
			return t->val;
		    }
		}
	    }
	    break;

	default:
	    if (kbinput < 32)
		break;
	    answer = nrealloc(answer, xend + 2);
	    memmove(answer + x + 1, answer + x, xend - x + 1);
	    xend++;
	    answer[x] = kbinput;
	    x++;

#ifdef DEBUG
	    fprintf(stderr, _("input \'%c\' (%d)\n"), kbinput, kbinput);
#endif
	}
	nanoget_repaint(buf, answer, x);
	wrefresh(bottomwin);
    } /* while (kbinput ...) */

    /* In Pico mode, just check for a blank answer here */
    if (ISSET(PICO_MODE) && answer[0] == '\0')
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

    space = COLS - sizeof(VERMSG) - 22;

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
    char keystr[4];
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

#ifndef NANO_SMALL
	    if (s->val == NANO_CONTROL_SPACE)
		strcpy(keystr, "^ ");
	    else
#endif /* !NANO_SMALL */
	    if (s->val > 0) {
		if (s->val < 64)
		    sprintf(keystr, "^%c", s->val + 64);
		else
		    sprintf(keystr, "M-%c", s->val - 32);
	    } else if (s->altval > 0)
		sprintf(keystr, "M-%c", s->altval);

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
  * window when the cursor is at the given column. */
int get_page_start(int column)
{
    assert(COLS > 9);
    return column < COLS - 1 ? 0 : column - 7 - (column - 8) % (COLS - 9);
}

/* Resets current_y, based on the position of current, and puts the
 * cursor at (current_y, current_x). */
void reset_cursor(void)
{
    const filestruct *ptr = edittop;
    size_t x;

    /* Yuck.  This condition can be true after open_file when opening the
     * first file. */
    if (edittop == NULL)
	return;

    current_y = 0;

    while (ptr != current && ptr != editbot && ptr->next != NULL) {
	ptr = ptr->next;
	current_y++;
    }

    x = xplustabs();
    wmove(edit, current_y, x - get_page_start(x));
}

/* edit_add() takes care of the job of actually painting a line into
 * the edit window.  Called only from update_line().  Expects a
 * converted-to-not-have-tabs line. */
void edit_add(const filestruct *fileptr, int yval, int start
#ifndef NANO_SMALL
		, int virt_mark_beginx,	int virt_cur_x
#endif
		)
{
#ifdef DEBUG
    fprintf(stderr, "Painting line %d, current is %d\n", fileptr->lineno,
		current->lineno);
#endif

    /* Just paint the string in any case (we'll add color or reverse on
       just the text that needs it */
    mvwaddnstr(edit, yval, 0, &fileptr->data[start], COLS);

#ifdef ENABLE_COLOR
    if (colorstrings != NULL && ISSET(COLOR_SYNTAX)) {
	const colortype *tmpcolor = colorstrings;

	for (; tmpcolor != NULL; tmpcolor = tmpcolor->next) {
	    int x_start;
		/* Starting column for mvwaddnstr.  Zero-based. */
	    int paintlen;
		/* number of chars to paint on this line.  There are COLS
		 * characters on a whole line. */
	    regex_t start_regexp;	/* Compiled search regexp */
	    regmatch_t startmatch;	/* match position for start_regexp*/
	    regmatch_t endmatch;	/* match position for end_regexp*/

	    regcomp(&start_regexp, tmpcolor->start, REG_EXTENDED);

	    if (tmpcolor->bright)
		wattron(edit, A_BOLD);
	    wattron(edit, COLOR_PAIR(tmpcolor->pairnum));
	    /* Two notes about regexec.  Return value 0 means there is a
	     * match.  Also, rm_eo is the first non-matching character
	     * after the match. */

	    /* First case, tmpcolor is a single-line expression. */
	    if (tmpcolor->end == NULL) {
		size_t k = 0;

		/* We increment k by rm_eo, to move past the end of the
		   last match.  Even though two matches may overlap, we
		   want to ignore them, so that we can highlight C-strings
		   correctly. */
		while (k < start + COLS) {
		    /* Note the fifth parameter to regexec.  It says not to
		     * match the beginning-of-line character unless
		     * k == 0.  If regexec returns non-zero, there are
		     * no more matches in the line. */
		    if (regexec(&start_regexp, &fileptr->data[k], 1,
				&startmatch, k == 0 ? 0 : REG_NOTBOL))
			break;
		    /* Translate the match to the beginning of the line. */
		    startmatch.rm_so += k;
		    startmatch.rm_eo += k;
		    if (startmatch.rm_so == startmatch.rm_eo) {
			startmatch.rm_eo++;
			statusbar(_("Refusing 0 length regex match"));
		    } else if (startmatch.rm_so < start + COLS &&
				startmatch.rm_eo > start) {
			x_start = startmatch.rm_so - start;
			if (x_start < 0)
			    x_start = 0;
			paintlen = startmatch.rm_eo - start - x_start;
			if (paintlen > COLS - x_start)
			    paintlen = COLS - x_start;

			assert(0 <= x_start && 0 < paintlen &&
				x_start + paintlen <= COLS);
			mvwaddnstr(edit, yval, x_start,
				fileptr->data + start + x_start, paintlen);
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

		regex_t end_regexp;	/* Compiled search regexp */
		const filestruct *start_line = fileptr->prev;
		    /* the first line before fileptr matching start*/
		regoff_t start_col;
		    /* where it starts in that line */
		const filestruct *end_line;
		int searched_later_lines = 0;
		    /* Used in step 2.  Have we looked for an end on
		     * lines after fileptr? */

		regcomp(&end_regexp, tmpcolor->end, REG_EXTENDED);

		while (start_line != NULL &&
			regexec(&start_regexp, start_line->data, 1,
				&startmatch, 0)) {
		    /* If there is an end on this line, there is no need
		     * to look for starts on earlier lines. */
		    if (!regexec(&end_regexp, start_line->data, 1,
				&endmatch, 0))
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
		while (1) {
		    start_col += startmatch.rm_so;
		    startmatch.rm_eo -= startmatch.rm_so;
		    if (regexec(&end_regexp,
			    start_line->data + start_col + startmatch.rm_eo,
			    1, &endmatch,
			    start_col + startmatch.rm_eo == 0 ? 0 : REG_NOTBOL))
			/* No end found after this start */
			break;
		    start_col++;
		    if (regexec(&start_regexp,
			    start_line->data + start_col, 1, &startmatch,
			    REG_NOTBOL))
			/* No later start on this line. */
			goto step_two;
		}
		/* Indeed, there is a start not followed on this line by an
		 * end. */

		/* We have already checked that there is no end before
		 * fileptr and after the start.  Is there an end after
		 * the start at all?  We don't paint unterminated starts. */
		end_line = fileptr;
		while (end_line != NULL &&
			regexec(&end_regexp, end_line->data, 1,
				&endmatch, 0))
		    end_line = end_line->next;

		/* No end found, or it is too early. */
		if (end_line == NULL ||
			end_line->lineno < fileptr->lineno ||
			(end_line == fileptr && endmatch.rm_eo <= start))
		    goto step_two;

		/* Now paint the start of fileptr. */
		paintlen = end_line != fileptr
				? COLS : endmatch.rm_eo - start;
		if (paintlen > COLS)
		    paintlen = COLS;

		assert(0 < paintlen && paintlen <= COLS);
		mvwaddnstr(edit, yval, 0, fileptr->data + start, paintlen);

		/* We have already painted the whole line. */
		if (paintlen == COLS)
		    goto skip_step_two;


  step_two:	/* Second step, we look for starts on this line. */
		start_col = 0;
		while (start_col < start + COLS) {
		    if (regexec(&start_regexp, fileptr->data + start_col, 1,
				&startmatch, start_col == 0 ? 0 : REG_NOTBOL)
			    || start_col + startmatch.rm_so >= start + COLS)
			/* No more starts on this line. */
			break;
		    /* Translate the match to be relative to the
		     * beginning of the line. */
		    startmatch.rm_so += start_col;
		    startmatch.rm_eo += start_col;

		    x_start = startmatch.rm_so - start;
		    if (x_start < 0) {
			x_start = 0;
			startmatch.rm_so = start;
		    }
		    if (!regexec(&end_regexp, fileptr->data + startmatch.rm_eo,
				1, &endmatch,
				startmatch.rm_eo == 0 ? 0 : REG_NOTBOL)) {
			/* Translate the end match to be relative to the
			   beginning of the line. */
			endmatch.rm_so += startmatch.rm_eo;
			endmatch.rm_eo += startmatch.rm_eo;
			/* There is an end on this line.  But does it
			   appear on this page, and is the match more than
			   zero characters long? */
			if (endmatch.rm_eo > start &&
				endmatch.rm_eo > startmatch.rm_so) {
			    paintlen = endmatch.rm_eo - start - x_start;
			    if (x_start + paintlen > COLS)
				paintlen = COLS - x_start;

			    assert(0 <= x_start && 0 < paintlen &&
				    x_start + paintlen <= COLS);
			    mvwaddnstr(edit, yval, x_start,
				fileptr->data + start + x_start, paintlen);
			}
		    } else if (!searched_later_lines) {
			searched_later_lines = 1;
			/* There is no end on this line.  But we haven't
			 * yet looked for one on later lines. */
			end_line = fileptr->next;
			while (end_line != NULL &&
				regexec(&end_regexp, end_line->data, 1,
				&endmatch, 0))
			    end_line = end_line->next;
			if (end_line != NULL) {
			    assert(0 <= x_start && x_start < COLS);
			    mvwaddnstr(edit, yval, x_start,
			    		fileptr->data + start + x_start,
			    		COLS - x_start);
			    /* We painted to the end of the line, so
			     * don't bother checking any more starts. */
			    break;
			}
		    }
		    start_col = startmatch.rm_so + 1;
		} /* while start_col < start + COLS */

  skip_step_two:
		regfree(&end_regexp);
	    } /* if (tmp_color->end != NULL) */

	    regfree(&start_regexp);
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

	int x_start;
	    /* Starting column for mvwaddnstr.  Zero-based. */
	int paintlen;
	    /* number of chars to paint on this line.  There are COLS
	     * characters on a whole line. */

	if (mark_beginbuf == fileptr && current == fileptr) {
	    x_start = virt_mark_beginx < virt_cur_x ? virt_mark_beginx
	    					    : virt_cur_x;
	    paintlen = abs(virt_mark_beginx - virt_cur_x);
	} else {
	    if (mark_beginbuf->lineno < fileptr->lineno ||
		    current->lineno < fileptr->lineno)
		x_start = 0;
	    else
		x_start = mark_beginbuf == fileptr ? virt_mark_beginx
						   : virt_cur_x;

	    if (mark_beginbuf->lineno > fileptr->lineno ||
		    current->lineno > fileptr->lineno)
		paintlen = start + COLS;
	    else
		paintlen = mark_beginbuf == fileptr ? virt_mark_beginx
						    : virt_cur_x;
	}
	x_start -= start;
	if (x_start < 0) {
	    paintlen += x_start;
	    x_start = 0;
	}
	if (x_start + paintlen > COLS)
	    paintlen = COLS - x_start;
	if (paintlen > 0) {
	    wattron(edit, A_REVERSE);
	    assert(x_start >= 0 && paintlen > 0 && x_start + paintlen <= COLS);
	    mvwaddnstr(edit, yval, x_start,
			fileptr->data + start + x_start, paintlen);
	    wattroff(edit, A_REVERSE);
	}
    }
#endif /* !NANO_SMALL */
}

/* Just update one line in the edit buffer.  Basically a wrapper for
 * edit_add().  If fileptr != current, then index is considered 0.
 * The line will be displayed starting with fileptr->data[index].
 * Likely args are current_x or 0. */
void update_line(filestruct *fileptr, int index)
{
    int line;
	/* line in the edit window for CURSES calls */
#ifndef NANO_SMALL
    int virt_cur_x;
    int virt_mark_beginx;
#endif
    char *original;
	/* The original string fileptr->data. */
    char *converted;
	/* fileptr->data converted to have tabs and control characters
	 * expanded. */
    size_t pos;
    size_t page_start;

    if (!fileptr)
	return;

    line = fileptr->lineno - edittop->lineno;

    /* We assume the line numbers are valid.  Is that really true? */
    assert(line < 0 || line == check_linenumbers(fileptr));

    if (line < 0 || line >= editwinrows)
	return;

    /* First, blank out the line (at a minimum) */
    mvwaddstr(edit, line, 0, hblank);

    original = fileptr->data;
    converted = charalloc(strlenpt(original) + 1);
    
    /* Next, convert all the tabs to spaces, so everything else is easy. 
     * Note the internal speller sends us index == -1. */
    index = fileptr == current && index > 0 ? strnlenpt(original, index) : 0;
#ifndef NANO_SMALL
    virt_cur_x = fileptr == current ? strnlenpt(original, current_x) : current_x;
    virt_mark_beginx = fileptr == mark_beginbuf ? strnlenpt(original, mark_beginx) : mark_beginx;
#endif

    pos = 0;
    for (; *original != '\0'; original++) {
	if (*original == '\t')
	    do {
		converted[pos++] = ' ';
	    } while (pos % tabsize);
	else if (is_cntrl_char(*original)) {
	    converted[pos++] = '^';
	    if (*original == 127)
		converted[pos++] = '?';
	    else if (*original == '\n')
		/* Treat newlines (ASCII 10's) embedded in a line as encoded
	   	 * nulls (ASCII 0's); the line in question should be run
		 * through unsunder() before reaching here */
		converted[pos++] = '@';
	    else
		converted[pos++] = *original + 64;
	} else
	    converted[pos++] = *original;
    }
    converted[pos] = '\0';

    /* Now, paint the line */
    original = fileptr->data;
    fileptr->data = converted;
    page_start = get_page_start(index);
    edit_add(fileptr, line, page_start
#ifndef NANO_SMALL
		, virt_mark_beginx, virt_cur_x
#endif
		);
    free(converted);
    fileptr->data = original;

    if (page_start > 0)
	mvwaddch(edit, line, 0, '$');
    if (pos > page_start + COLS)
	mvwaddch(edit, line, COLS - 1, '$');
}

/* This function updates current, based on where current_y is;
 * reset_cursor() does the opposite. */
void update_cursor(void)
{
    int i = 0;

#ifdef DEBUG
    fprintf(stderr, _("Moved to (%d, %d) in edit buffer\n"), current_y,
	    current_x);
#endif

    current = edittop;
    while (i < current_y && current->next != NULL) {
	current = current->next;
	i++;
    }

#ifdef DEBUG
    fprintf(stderr, _("current->data = \"%s\"\n"), current->data);
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
    static int noloop = 0;
    int nlines = 0, currentcheck = 0;

    /* Neither of these conditions should occur, but they do.  edittop is
     * NULL when you open an existing file on the command line, and
     * ENABLE_COLOR is defined.  Yuck. */
    if (current == NULL)
	return;
    if (edittop == NULL)
	edittop = current;

    /* Don't make the cursor jump around the screen whilst updating */
    leaveok(edit, TRUE);

    editbot = edittop;
    while (nlines < editwinrows) {
	update_line(editbot, current_x);
	if (editbot == current)
	    currentcheck = 1;

	nlines++;

	if (editbot->next == NULL)
	    break;
	editbot = editbot->next;
    }

    /* If noloop == 1, then we already did an edit_update without finishing
       this function.  So we don't run edit_update again */
    if (!currentcheck && !noloop) {
		/* Then current has run off the screen... */
	edit_update(current, CENTER);
	noloop = 1;
    } else if (noloop)
	noloop = 0;

    while (nlines < editwinrows) {
	mvwaddstr(edit, nlines, 0, hblank);
	nlines++;
    }

    /* What the hell are we expecting to update the screen if this isn't 
       here? Luck?? */
    wrefresh(edit);
    leaveok(edit, FALSE);
}

/*
 * Same as above, but touch the window first, so everything is redrawn.
 */
void edit_refresh_clearok(void)
{
    clearok(edit, TRUE);
    edit_refresh();
    clearok(edit, FALSE);
}

/*
 * Nice generic routine to update the edit buffer, given a pointer to the
 * file struct =) 
 */
void edit_update(filestruct *fileptr, topmidbotnone location)
{
    if (fileptr == NULL)
	return;

    if (location != TOP) {
	int goal = location == NONE ? current_y - 1 : editwinrows / 2;

	for (; goal >= 0 && fileptr->prev != NULL; goal--)
	    fileptr = fileptr->prev;
    }
    edittop = fileptr;
    fix_editbot();

    edit_refresh();
}

/*
 * Ask a question on the statusbar.  Answer will be stored in answer
 * global.  Returns -1 on aborted enter, -2 on a blank string, and 0
 * otherwise, the valid shortcut key caught.  Def is any editable text we
 * want to put up by default.
 *
 * New arg tabs tells whether or not to allow tab completion.
 */
int statusq(int tabs, const shortcut *s, const char *def,
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

#ifndef DISABLE_TABCOMP
    ret = nanogetstr(tabs, foo, def, s, &list);
#else
    ret = nanogetstr(tabs, foo, def, s);
#endif
    free(foo);

    switch (ret) {
    case NANO_FIRSTLINE_KEY:
	do_first_line();
	break;
    case NANO_LASTLINE_KEY:
	do_last_line();
	break;
    case NANO_CANCEL_KEY:
	ret = -1;
	break;
    default:
	blank_statusbar();
    }

#ifdef DEBUG
    fprintf(stderr, _("I got \"%s\"\n"), answer);
#endif

#ifndef DISABLE_TABCOMP
	/* if we've done tab completion, there might be a list of
	   filename matches on the edit window at this point; make sure
	   they're cleared off */
	if (list)
	    edit_refresh();
#endif

    return ret;
}

/*
 * Ask a simple yes/no question on the statusbar.  Returns 1 for Y, 0
 * for N, 2 for All (if all is non-zero when passed in) and -1 for
 * abort (^C).
 */
int do_yesno(int all, int leavecursor, const char *msg, ...)
{
    va_list ap;
    char foo[133];
    int kbinput, ok = -1, i;
    const char *yesstr;		/* String of yes characters accepted */
    const char *nostr;		/* Same for no */
    const char *allstr;		/* And all, surprise! */
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
    MEVENT mevent;
#endif
#endif

    /* Yes, no and all are strings of any length.  Each string consists of
       all characters accepted as a valid character for that value.
       The first value will be the one displayed in the shortcuts. */
    yesstr = _("Yy");
    nostr = _("Nn");
    allstr = _("Aa");

    /* Write the bottom of the screen */
    blank_bottomwin();

    /* Remove gettext call for keybindings until we clear the thing up */
    if (!ISSET(NO_HELP)) {
	char shortstr[3];		/* Temp string for Y, N, A */

	wmove(bottomwin, 1, 0);

	sprintf(shortstr, " %c", yesstr[0]);
	onekey(shortstr, _("Yes"), 16);

	if (all) {
	    shortstr[1] = allstr[0];
	    onekey(shortstr, _("All"), 16);
	}
	wmove(bottomwin, 2, 0);

	shortstr[1] = nostr[0];
	onekey(shortstr, _("No"), 16);

	onekey("^C", _("Cancel"), 16);
    }
    va_start(ap, msg);
    vsnprintf(foo, 132, msg, ap);
    va_end(ap);

    wattron(bottomwin, A_REVERSE);

    blank_statusbar();
    mvwaddstr(bottomwin, 0, 0, foo);

    wattroff(bottomwin, A_REVERSE);

    wrefresh(bottomwin);

    if (leavecursor == 1)
	reset_cursor();

    while (ok == -1) {
	kbinput = wgetch(edit);

	switch (kbinput) {
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
	case KEY_MOUSE:

	    /* Look ma!  We get to duplicate lots of code from do_mouse!! */
	    if (getmouse(&mevent) == ERR)
		break;
	    if (!wenclose(bottomwin, mevent.y, mevent.x) || ISSET(NO_HELP))
		break;
	    mevent.y -= editwinrows + 3;
	    if (mevent.y < 0)
		break;
	    else {

		/* Rather than a bunch of if statements, set up a matrix
		   of possible return keystrokes based on the x and y
		   values */ 
		char yesnosquare[2][2];
		yesnosquare[0][0] = yesstr[0];
		if (all)
		    yesnosquare[0][1] = allstr[0];
		else
		    yesnosquare[0][1] = '\0';
		yesnosquare[1][0] = nostr[0];
		yesnosquare[1][1] = NANO_CONTROL_C;
		ungetch(yesnosquare[mevent.y][mevent.x / (COLS / 6)]);
	    }
	    break;
#endif
#endif
	case NANO_CONTROL_C:
	    ok = -2;
	    break;
	default:

	    /* Look for the kbinput in the yes, no and (optimally) all str */
	    for (i = 0; yesstr[i] != 0 && yesstr[i] != kbinput; i++);
	    if (yesstr[i] != 0) {
		ok = 1;
		break;
	    }

	    for (i = 0; nostr[i] != 0 && nostr[i] != kbinput; i++);
	    if (nostr[i] != 0) {
		ok = 0;
		break;
	    }

	    if (all) {
		for (i = 0; allstr[i] != 0 && allstr[i] != kbinput; i++);
		if (allstr[i] != 0) {
		    ok = 2;
		    break;
		}
	    }
	}
    }

    /* Then blank the screen */
    blank_statusbar_refresh();

    if (ok == -2)
	return -1;
    else
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
    char *foo;
    int start_x = 0;
    size_t foo_len;

    assert(COLS >= 4);
    foo = charalloc(COLS - 3);

    va_start(ap, msg);
    vsnprintf(foo, COLS - 3, msg, ap);
    va_end(ap);

    foo[COLS - 4] = '\0';
    foo_len = strlen(foo);
    start_x = (COLS - foo_len - 4) / 2;

    /* Blank out line */
    blank_statusbar();

    wmove(bottomwin, 0, start_x);

    wattron(bottomwin, A_REVERSE);

    waddstr(bottomwin, "[ ");
    waddstr(bottomwin, foo);
    free(foo);
    waddstr(bottomwin, " ]");

    wattroff(bottomwin, A_REVERSE);

    wrefresh(bottomwin);

    if (ISSET(CONSTUPDATE))
	statblank = 1;
    else
	statblank = 25;
}

int do_cursorpos(int constant)
{
    filestruct *fileptr;
    float linepct = 0.0, bytepct = 0.0, colpct = 0.0;
    long i = 0, j = 0;
    static long old_i = -1, old_totsize = -1;

    if (current == NULL || fileage == NULL)
	return 0;

    if (old_i == -1)
	old_i = i;

    if (old_totsize == -1)
	old_totsize = totsize;

    colpct = 100 * (xplustabs() + 1) / (strlenpt(current->data) + 1);

    for (fileptr = fileage; fileptr != current && fileptr != NULL;
	 fileptr = fileptr->next)
	i += strlen(fileptr->data) + 1;

    if (fileptr == NULL)
	return -1;

    i += current_x;

    j = totsize;

    if (totsize > 0)
	bytepct = 100 * i / totsize;

    if (totlines > 0)
	 linepct = 100 * current->lineno / totlines;

#ifdef DEBUG
    fprintf(stderr, _("do_cursorpos: linepct = %f, bytepct = %f\n"),
	    linepct, bytepct);
#endif

    /* if constant is zero, display the position on the statusbar
       unconditionally; otherwise, only display the position when the
       character values have changed */
    if (!constant || (old_i != i || old_totsize != totsize)) {
	statusbar(_
		  ("line %d/%d (%.0f%%), col %ld/%ld (%.0f%%), char %ld/%ld (%.0f%%)"),
		  current->lineno, totlines, linepct, xplustabs() + 1, 
		  strlenpt(current->data) + 1, colpct, i, j, bytepct);
    }

    old_i = i;
    old_totsize = totsize;

    reset_cursor();
    return 1;
}

int do_cursorpos_void(void)
{
    return do_cursorpos(0);
}

/* Our shortcut-list-compliant help function, which is
 * better than nothing, and dynamic! */
int do_help(void)
{
#ifndef DISABLE_HELP
    int i, j, row = 0, page = 1, kbinput = 0, no_more = 0, kp, kp2;
    int no_help_flag = 0;
    const shortcut *oldshortcut;

    blank_edit();
    curs_set(0);
    wattroff(bottomwin, A_REVERSE);
    blank_statusbar();

    /* set help_text as the string to display */
    help_init();
    assert(help_text != NULL);

    oldshortcut = currshortcut;

    currshortcut = help_list;

    kp = keypad_on(edit, 1);
    kp2 = keypad_on(bottomwin, 1);

    if (ISSET(NO_HELP)) {

	/* Well, if we're going to do this, we should at least
	   do it the right way */
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
#ifdef NCURSES_MOUSE_VERSION
	case KEY_MOUSE:
	    do_mouse();
	    break;
#endif
#endif
	case 27:
	    kbinput = wgetch(edit);
	    switch(kbinput) {
	    case '[':
		kbinput = wgetch(edit);
		switch(kbinput) {
		    case '5':	/* Alt-[-5 = Page Up */
			wgetch(edit);
			goto do_pageupkey;
			break;
		    case 'V':	/* Alt-[-V = Page Up in Hurd Console */
		    case 'I':	/* Alt-[-I = Page Up - FreeBSD Console */
			goto do_pageupkey;
			break;
		    case '6':	/* Alt-[-6 = Page Down */
			wgetch(edit);
			goto do_pagedownkey;
			break;
		    case 'U':	/* Alt-[-U = Page Down in Hurd Console */
		    case 'G':	/* Alt-[-G = Page Down - FreeBSD Console */
			goto do_pagedownkey;
			break;
		}
		break;
	    }
	    break;
	case NANO_NEXTPAGE_KEY:
	case NANO_NEXTPAGE_FKEY:
	case KEY_NPAGE:
	  do_pagedownkey:
	    if (!no_more) {
		blank_edit();
		page++;
	    }
	    break;
	case NANO_PREVPAGE_KEY:
	case NANO_PREVPAGE_FKEY:
	case KEY_PPAGE:
	  do_pageupkey:
	    if (page > 1) {
		no_more = 0;
		blank_edit();
		page--;
	    }
	    break;
	}

	/* Calculate where in the text we should be, based on the page */
	for (i = 1; i < page; i++) {
	    row = 0;
	    j = 0;

	    while (row < editwinrows - 2 && *ptr != '\0') {
		if (*ptr == '\n' || j == COLS - 5) {
		    j = 0;
		    row++;
		}
		ptr++;
		j++;
	    }
	}

	i = 0;
	j = 0;
	while (i < editwinrows && *ptr != '\0') {
	    const char *end = ptr;
	    while (*end != '\n' && *end != '\0' && j != COLS - 5) {
		end++;
		j++;
	    }
	    if (j == COLS - 5) {

		/* Don't print half a word if we've run out of space */
		while (*end != ' ' && *end != '\0') {
		    end--;
		    j--;
		}
	    }
	    mvwaddnstr(edit, i, 0, ptr, j);
	    j = 0;
	    i++;
	    if (*end == '\n')
		end++;
	    ptr = end;
	}
	if (*ptr == '\0') {
	    no_more = 1;
	    continue;
	}
    } while ((kbinput = wgetch(edit)) != NANO_EXIT_KEY &&
	     kbinput != NANO_EXIT_FKEY);

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
    kp = keypad_on(edit, kp);
    kp2 = keypad_on(bottomwin, kp2);

    /* The help_init() at the beginning allocated help_text, which has
       now been written to screen. */
    free(help_text);
    help_text = NULL;

#elif defined(DISABLE_HELP)
    nano_disabled_msg();
#endif

    return 1;
}

int keypad_on(WINDOW * win, int newval)
{
/* This is taken right from aumix.  Don't sue me. */
#ifdef HAVE_USEKEYPAD
    int old = win->_use_keypad;
    keypad(win, newval);
    return old;
#else
    keypad(win, newval);
    return 1;
#endif /* HAVE_USEKEYPAD */
}

/* Highlight the current word being replaced or spell checked. */
void do_replace_highlight(int highlight_flag, const char *word)
{
    char *highlight_word = NULL;
    int x, y, word_len;

    highlight_word =
	mallocstrcpy(highlight_word, &current->data[current_x]);

#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP))
	/* if we're using regexps, the highlight is the length of the
	   search result, not the length of the regexp string */
	word_len = regmatches[0].rm_eo - regmatches[0].rm_so;
    else
#endif
	word_len = strlen(word);

    highlight_word[word_len] = '\0';

    /* adjust output when word extends beyond screen */

    x = xplustabs();
    y = get_page_start(x) + COLS;

    if ((COLS - (y - x) + word_len) > COLS) {
	highlight_word[y - x - 1] = '$';
	highlight_word[y - x] = '\0';
    }

    /* OK display the output */

    reset_cursor();

    if (highlight_flag)
	wattron(edit, A_REVERSE);

    waddstr(edit, highlight_word);

    if (highlight_flag)
	wattroff(edit, A_REVERSE);

    free(highlight_word);
}

/* Fix editbot, based on the assumption that edittop is correct. */
void fix_editbot(void)
{
    int i;

    editbot = edittop;
    for (i = 0; i < editwinrows && editbot->next != NULL; i++)
	editbot = editbot->next;
}

#ifdef DEBUG
/* Dump the current file structure to stderr */
void dump_buffer(const filestruct *inptr) {
    if (inptr == fileage)
	fprintf(stderr, _("Dumping file buffer to stderr...\n"));
    else if (inptr == cutbuffer)
	fprintf(stderr, _("Dumping cutbuffer to stderr...\n"));
    else
	fprintf(stderr, _("Dumping a buffer to stderr...\n"));

    while (inptr != NULL) {
	fprintf(stderr, "(%d) %s\n", inptr->lineno, inptr->data);
	inptr = inptr->next;
    }
}
#endif /* DEBUG */

#ifdef DEBUG
void dump_buffer_reverse(void) {
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

void do_credits(void)
{
    int i, j = 0, k, place = 0, start_x;

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
	"(c) 1999-2002 Chris Allegretta",
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
	    usleep(700000);
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
