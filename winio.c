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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include "config.h"
#include "proto.h"
#include "nano.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

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


/* Like xplustabs, but for a specific index of a specific filestruct */
int xpt(const filestruct *fileptr, int index)
{
    int i, tabs = 0;

    if (fileptr == NULL || fileptr->data == NULL)
	return 0;

    for (i = 0; i < index && fileptr->data[i] != 0; i++) {
	tabs++;

	if (fileptr->data[i] == NANO_CONTROL_I) {
	    if (tabs % tabsize == 0);
	    else
		tabs += tabsize - (tabs % tabsize);
	} else if (is_cntrl_char((int)fileptr->data[i]))
	    tabs++;
	else if (fileptr->data[i] & 0x80)
	    /* Make 8 bit chars only 1 column! */
	    ;
    }

    return tabs;
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

/* And so start the display update routines.  Given a column, this
 * returns the "page" it is on.  "page", in the case of the display
 * columns, means which set of 80 characters is viewable (e.g. page 1
 * shows from 1 to COLS). */
int get_page_from_virtual(int virtual)
{
    int page = 2;

    if (virtual <= COLS - 2)
	return 1;
    virtual -= (COLS - 2);

    while (virtual > COLS - 2 - 7) {
	virtual -= (COLS - 2 - 7);
	page++;
    }

    return page;
}

/* The inverse of the above function */
int get_page_start_virtual(int page)
{
    int virtual;
    virtual = --page * (COLS - 7);
    if (page)
	virtual -= 2 * page - 1;
    return virtual;
}

int get_page_end_virtual(int page)
{
    return get_page_start_virtual(page) + COLS - 1;
}

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

#ifndef NANO_SMALL
/* This takes care of the case where there is a mark that covers only
 * the current line.  It expects a line with no tab characters (i.e.
 * the type that edit_add() deals with. */
void add_marked_sameline(int begin, int end, filestruct *fileptr, int y,
			 int virt_cur_x, int this_page)
{
    /*
     * The general idea is to break the line up into 3 sections: before
     * the mark, the mark, and after the mark.  We then paint each in
     * turn (for those that are currently visible, of course)
     *
     * 3 start points: 0 -> begin, begin->end, end->strlen(data)
     *    in data    :    pre          sel           post        
     */
    int this_page_start = get_page_start_virtual(this_page),
	this_page_end = get_page_end_virtual(this_page);

    /* likewise, 3 data lengths */
    int pre_data_len = begin, sel_data_len = end - begin, post_data_len = 0;	/* Determined from the other two */

    /* now fix the start locations & lengths according to the cursor's 
     * position (i.e.: our page) */
    if (pre_data_len < this_page_start)
	pre_data_len = 0;
    else
	pre_data_len -= this_page_start;

    if (begin < this_page_start)
	begin = this_page_start;

    if (end < this_page_start)
	end = this_page_start;

    if (begin > this_page_end)
	begin = this_page_end;

    if (end > this_page_end)
	end = this_page_end;

    /* Now calculate the lengths */
    sel_data_len = end - begin;
    post_data_len = this_page_end - end;

    wattron(edit, A_REVERSE);
    mvwaddnstr(edit, y, begin - this_page_start,
	       &fileptr->data[begin], sel_data_len);

    wattroff(edit, A_REVERSE);

}
#endif

/* edit_add() takes care of the job of actually painting a line into
 * the edit window.  Called only from update_line().  Expects a
 * converted-to-not-have-tabs line. */
void edit_add(filestruct *fileptr, int yval, int start, int virt_cur_x,
	      int virt_mark_beginx, int this_page)
{

#ifdef ENABLE_COLOR
    const colortype *tmpcolor = NULL;
    int k, paintlen;
    filestruct *e, *s;
    regoff_t ematch, smatch;
#endif

    /* Just paint the string in any case (we'll add color or reverse on
       just the text that needs it */
    mvwaddnstr(edit, yval, 0, &fileptr->data[start],
	       get_page_end_virtual(this_page) - start + 1);

#ifdef ENABLE_COLOR
    if (colorstrings != NULL)
	for (tmpcolor = colorstrings; tmpcolor != NULL;
	     tmpcolor = tmpcolor->next) {

	    if (tmpcolor->end == NULL) {

		/* First, highlight all single-line regexes */
		k = start;
		regcomp(&color_regexp, tmpcolor->start, REG_EXTENDED);
		while (!regexec(&color_regexp, &fileptr->data[k], 1,
				colormatches, 0)) {

		    if (colormatches[0].rm_eo - colormatches[0].rm_so < 1) {
			statusbar(_("Refusing 0 length regex match"));
			break;
		    }
#ifdef DEBUG
		    fprintf(stderr, _("Match! (%d chars) \"%s\"\n"),
			    colormatches[0].rm_eo - colormatches[0].rm_so,
			    &fileptr->data[k + colormatches[0].rm_so]);
#endif
		    if (colormatches[0].rm_so < COLS - 1) {
			if (tmpcolor->bright)
			    wattron(edit, A_BOLD);
			wattron(edit, COLOR_PAIR(tmpcolor->pairnum));

			if (colormatches[0].rm_eo + k <= COLS) {
			    paintlen =
				colormatches[0].rm_eo - colormatches[0].rm_so;
#ifdef DEBUG
			    fprintf(stderr, _("paintlen (%d) = eo (%d) - so (%d)\n"), 
				paintlen, colormatches[0].rm_eo, colormatches[0].rm_so);
#endif

			}
			else {
			    paintlen = COLS - k - colormatches[0].rm_so - 1;
#ifdef DEBUG
			    fprintf(stderr, _("paintlen (%d) = COLS (%d) - k (%d), - rm.so (%d) - 1\n"), 
					paintlen, COLS, k, colormatches[0].rm_so);
#endif
			}

			mvwaddnstr(edit, yval, colormatches[0].rm_so + k,
				   &fileptr->data[k + colormatches[0].rm_so],
				   paintlen);

		    }

		    if (tmpcolor->bright)
			wattroff(edit, A_BOLD);
		    wattroff(edit, COLOR_PAIR(tmpcolor->pairnum));

		    k += colormatches[0].rm_eo;

		}
		regfree(&color_regexp);

	    }
	    /* Now, if there's an 'end' somewhere below, and a 'start'
	       somewhere above, things get really fun.  We have to look
	       down for an end, make sure there's not a start before 
	       the end after us, and then look up for a start, 
	       and see if there's an end after the start, before us :) */
	    else {

		s = fileptr;
		while (s != NULL) {
		    regcomp(&color_regexp, tmpcolor->start, REG_EXTENDED);
		    if (!regexec
			(&color_regexp, s->data, 1, colormatches, 0)) {
			regfree(&color_regexp);
			break;
		    }
		    s = s->prev;
		    regfree(&color_regexp);
		}

		if (s != NULL) {
		    /* We found a start, mark it */
		    smatch = colormatches[0].rm_so;

		    e = s;
		    while (e != NULL && e != fileptr) {
			regcomp(&color_regexp, tmpcolor->end, REG_EXTENDED);
			if (!regexec
			    (&color_regexp, e->data, 1, colormatches, 0)) {
			    regfree(&color_regexp);
			    break;
			}
			e = e->next;
			regfree(&color_regexp);
		    }

		    if (e != fileptr)
			continue;	/* There's an end before us */
		    else {	/* Keep looking for an end */
			while (e != NULL) {
			    regcomp(&color_regexp, tmpcolor->end, REG_EXTENDED);
			    if (!regexec
				(&color_regexp, e->data, 1, colormatches,
				 0)) {
				regfree(&color_regexp);
				break;
			    }
			    e = e->next;
			    regfree(&color_regexp);
			}

			if (e == NULL)
			    continue;	/* There's no start before the end :) */
			else {	/* Okay, we found an end, mark it! */
			    ematch = colormatches[0].rm_eo;

			    while (e != NULL) {
				regcomp(&color_regexp, tmpcolor->end, REG_EXTENDED);
				if (!regexec
				    (&color_regexp, e->data, 1,
				     colormatches, 0)) {
				    regfree(&color_regexp);
				    break;
				} e = e->next;
				regfree(&color_regexp);
			    }

			    if (e == NULL)
				continue;	/* No end, oh well :) */

			    /* Didn't find another end, we must be in the 
			       middle of a highlighted bit */

			    if (tmpcolor->bright)
				wattron(edit, A_BOLD);

			    wattron(edit, COLOR_PAIR(tmpcolor->pairnum));

			    if (s == fileptr && e == fileptr && ematch < COLS) {
				mvwaddnstr(edit, yval, start + smatch, 
					&fileptr->data[start + smatch],
					ematch - smatch);
#ifdef DEBUG
			fprintf(stderr, _("start = %d, smatch = %d, ematch = %d\n"), start,
				smatch, ematch);
#endif

		    	    } else if (s == fileptr)
				mvwaddnstr(edit, yval, start + smatch, 
					&fileptr->data[start + smatch],
					COLS - smatch);
			    else if (e == fileptr)
				mvwaddnstr(edit, yval, start, 
					&fileptr->data[start],
					COLS - start);
			    else
				mvwaddnstr(edit, yval, start, 
					&fileptr->data[start],
					COLS);

			    if (tmpcolor->bright)
				wattroff(edit, A_BOLD);

			    wattroff(edit, COLOR_PAIR(tmpcolor->pairnum));

			}

		    }

		    /* Else go to the next string, yahoo! =) */
		}

	    }

	}

#endif				/* ENABLE_COLOR */
#ifndef NANO_SMALL

    /* There are quite a few cases that could take place; we'll deal
     * with them each in turn */
    if (ISSET(MARK_ISSET) &&
	!((fileptr->lineno > mark_beginbuf->lineno
	   && fileptr->lineno > current->lineno)
	  || (fileptr->lineno < mark_beginbuf->lineno
	      && fileptr->lineno < current->lineno))) {
	/* If we get here we are on a line that is at least
	 * partially selected.  The lineno checks above determined
	 * that */
	if (fileptr != mark_beginbuf && fileptr != current) {
	    /* We are on a completely marked line, paint it all
	     * inverse */

	    wattron(edit, A_REVERSE);

	    mvwaddnstr(edit, yval, 0, fileptr->data, COLS);

	    wattroff(edit, A_REVERSE);

	} else if (fileptr == mark_beginbuf && fileptr == current) {
	    /* Special case, we're still on the same line we started
	     * marking -- so we call our helper function */
	    if (virt_cur_x < virt_mark_beginx) {
		/* To the right of us is marked */
		add_marked_sameline(virt_cur_x, virt_mark_beginx,
				    fileptr, yval, virt_cur_x, this_page);
	    } else {
		/* To the left of us is marked */
		add_marked_sameline(virt_mark_beginx, virt_cur_x,
				    fileptr, yval, virt_cur_x, this_page);
	    }
	} else if (fileptr == mark_beginbuf) {
	    /*
	     * We're updating the line that was first marked,
	     * but we're not currently on it.  So we want to
	     * figure out which half to invert based on our
	     * relative line numbers.
	     *
	     * I.e. if we're above the "beginbuf" line, we want to
	     * mark the left side.  Otherwise, we're below, so we
	     * mark the right.
	     */
	    int target;

	    if (mark_beginbuf->lineno > current->lineno) {

		wattron(edit, A_REVERSE);

		target =
		    (virt_mark_beginx <
		     COLS - 1) ? virt_mark_beginx : COLS - 1;

		mvwaddnstr(edit, yval, 0, fileptr->data, target);

		wattroff(edit, A_REVERSE);

	    }

	    if (mark_beginbuf->lineno < current->lineno) {

		wattron(edit, A_REVERSE);
		target = (COLS - 1) - virt_mark_beginx;

		if (target < 0)
		    target = 0;

		mvwaddnstr(edit, yval, virt_mark_beginx,
			   &fileptr->data[virt_mark_beginx], target);

		wattroff(edit, A_REVERSE);
	    }

	} else if (fileptr == current) {
	    /* We're on the cursor's line, but it's not the first
	     * one we marked.  Similar to the previous logic. */
	    int this_page_start = get_page_start_virtual(this_page),
		this_page_end = get_page_end_virtual(this_page);

	    if (mark_beginbuf->lineno < current->lineno) {

		wattron(edit, A_REVERSE);

		if (virt_cur_x > COLS - 2) {
		    mvwaddnstr(edit, yval, 0,
			       &fileptr->data[this_page_start],
			       virt_cur_x - this_page_start);
		} else
		    mvwaddnstr(edit, yval, 0, fileptr->data, virt_cur_x);

		wattroff(edit, A_REVERSE);

	    }

	    if (mark_beginbuf->lineno > current->lineno) {

		wattron(edit, A_REVERSE);
		if (virt_cur_x > COLS - 2)
		    mvwaddnstr(edit, yval, virt_cur_x - this_page_start,
			       &fileptr->data[virt_cur_x],
			       this_page_end - virt_cur_x);
		else
		    mvwaddnstr(edit, yval, virt_cur_x,
			       &fileptr->data[virt_cur_x],
			       COLS - virt_cur_x);

		wattroff(edit, A_REVERSE);

	    }
	}
    }
#endif

}

/*
 * Just update one line in the edit buffer.  Basically a wrapper for
 * edit_add().  index gives us a place in the string to update starting
 * from.  Likely args are current_x or 0.
 */
void update_line(filestruct *fileptr, int index)
{
    filestruct *filetmp;
    int line = 0, col = 0;
    int virt_cur_x = current_x, virt_mark_beginx = mark_beginx;
    char *realdata, *tmp;
    int i, pos, len, page;

    if (!fileptr)
	return;

    /* First, blank out the line (at a minimum) */
    for (filetmp = edittop; filetmp != fileptr && filetmp != editbot;
	 filetmp = filetmp->next)
	line++;

    mvwaddstr(edit, line, 0, hblank);

    /* Next, convert all the tabs to spaces, so everything else is easy */
    index = xpt(fileptr, index);

    realdata = fileptr->data;
    len = strlen(realdata);
    fileptr->data = charalloc(xpt(fileptr, len) + 1);

    pos = 0;
    for (i = 0; i < len; i++) {
	if (realdata[i] == '\t') {
	    do {
		fileptr->data[pos++] = ' ';
		if (i < current_x)
		    virt_cur_x++;
		if (i < mark_beginx)
		    virt_mark_beginx++;
	    } while (pos % tabsize);
	    /* must decrement once to account for tab-is-one-character */
	    if (i < current_x)
		virt_cur_x--;
	    if (i < mark_beginx)
		virt_mark_beginx--;
	} else if (realdata[i] == 127) {
	    /* Treat delete characters (ASCII 127's) as ^?'s */
	    fileptr->data[pos++] = '^';
	    fileptr->data[pos++] = '?';
	    if (i < current_x)
		virt_cur_x++;
	    if (i < mark_beginx)
		virt_mark_beginx++;
	} else if (realdata[i] == 10) {
	    /* Treat newlines (ASCII 10's) embedded in a line as encoded
	       nulls (ASCII 0's); the line in question should be run
	       through unsunder() before reaching here */
	    fileptr->data[pos++] = '^';
	    fileptr->data[pos++] = '@';
	    if (i < current_x)
		virt_cur_x++;
	    if (i < mark_beginx)
		virt_mark_beginx++;
	} else if (is_cntrl_char(realdata[i])) {
	    /* Treat control characters as ^symbol's */
	    fileptr->data[pos++] = '^';
	    fileptr->data[pos++] = realdata[i] + 64;
	    if (i < current_x)
		virt_cur_x++;
	    if (i < mark_beginx)
		virt_mark_beginx++;
	} else {
	    fileptr->data[pos++] = realdata[i];
	}
    }

    fileptr->data[pos] = '\0';

    /* Now, paint the line */
    if (current == fileptr && index > COLS - 2) {
	/* This handles when the current line is beyond COLS */
	/* It requires figuring out what page we're on      */
	page = get_page_from_virtual(index);
	col = get_page_start_virtual(page);

	edit_add(filetmp, line, col, virt_cur_x, virt_mark_beginx, page);
	mvwaddch(edit, line, 0, '$');

	if (strlenpt(fileptr->data) > get_page_end_virtual(page) + 1)
	    mvwaddch(edit, line, COLS - 1, '$');
    } else {
	/* It's not the current line means that it's at x=0 and page=1 */
	/* If it is the current line, then we're in the same boat      */
	edit_add(filetmp, line, 0, virt_cur_x, virt_mark_beginx, 1);

	if (strlenpt(&filetmp->data[col]) > COLS)
	    mvwaddch(edit, line, COLS - 1, '$');
    }

    /* Clean up our mess */
    tmp = fileptr->data;
    fileptr->data = realdata;
    free(tmp);
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
#define CREDIT_LEN 52
#define XLCREDIT_LEN 8

void do_credits(void)
{
    int i, j = 0, k, place = 0, start_x;

    char *what;
    char *xlcredits[XLCREDIT_LEN];

    char *credits[CREDIT_LEN] = { 
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
	"www.nano-editor.org"
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
