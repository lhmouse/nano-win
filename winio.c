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
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif


/* winio.c statics */
static int statblank = 0;	/* Number of keystrokes left after
				   we call statusbar(), before we
				   actually blank the statusbar */

/* Local Function Prototypes for only winio.c */
inline int get_page_from_virtual(int virtual);
inline int get_page_start_virtual(int page);
inline int get_page_end_virtual(int page);

/* Window I/O */

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
int xpt(filestruct * fileptr, int index)
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
	} else if (fileptr->data[i] & 0x80)
	    /* Make 8 bit chars only 1 column! */
	    ;
	else if (fileptr->data[i] < 32)
	    tabs++;
    }

    return tabs;
}


/* Return the actual place on the screen of current->data[current_x], which 
   should always be > current_x */
int xplustabs(void)
{
    return xpt(current, current_x);
}


/* Return what current_x should be, given xplustabs() for the line, 
 * given a start position in the filestruct's data */
int actual_x_from_start(filestruct * fileptr, int xplus, int start)
{
    int i, tot = 1;

    if (fileptr == NULL || fileptr->data == NULL)
	return 0;

    for (i = start; tot <= xplus && fileptr->data[i] != 0; i++, tot++)
	if (fileptr->data[i] == NANO_CONTROL_I) {
	    if (tot % tabsize == 0)
		tot++;
	    else
		tot += tabsize - (tot % tabsize);
	} else if (fileptr->data[i] & 0x80)
	    tot++;		/* Make 8 bit chars only 1 column (again) */
	else if (fileptr->data[i] < 32)
	    tot += 2;

#ifdef DEBUG
    fprintf(stderr, _("actual_x_from_start for xplus=%d returned %d\n"),
	    xplus, i);
#endif
    return i - start;
}

/* Opposite of xplustabs */
int actual_x(filestruct * fileptr, int xplus)
{
    return actual_x_from_start(fileptr, xplus, 0);
}

/* a strlen with tabs factored in, similar to xplustabs() */
int strnlenpt(char *buf, int size)
{
    int i, tabs = 0;

    if (buf == NULL)
	return 0;

    for (i = 0; i < size; i++) {
	tabs++;

	if (buf[i] == NANO_CONTROL_I) {
	    if (tabs % tabsize == 0);
	    else
		tabs += tabsize - (tabs % tabsize);
	} else if (buf[i] & 0x80)
	    /* Make 8 bit chars only 1 column! */
	    ;
	else if (buf[i] < 32)
	    tabs++;
    }

    return tabs;
}

int strlenpt(char *buf)
{
    return strnlenpt(buf, strlen(buf));
}


/* resets current_y, based on the position of current, and puts the cursor at 
   (current_y, current_x) */
void reset_cursor(void)
{
    filestruct *ptr = edittop;
    int x;

    current_y = 0;

    while (ptr != current && ptr != editbot && ptr->next != NULL) {
	ptr = ptr->next;
	current_y++;
    }

    x = xplustabs();
    if (x <= COLS - 2)
	wmove(edit, current_y, x);
    else
	wmove(edit, current_y, x -
	      get_page_start_virtual(get_page_from_virtual(x)));

}

void blank_bottombars(void)
{
    int i = no_help()? 3 : 1;

    for (; i <= 2; i++)
	mvwaddstr(bottomwin, i, 0, hblank);

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

/* Repaint the statusbar when getting a character in nanogetstr */
void nanoget_repaint(char *buf, char *inputbuf, int x)
{
    int len = strlen(buf);
    int wid = COLS - len;

#ifdef ENABLE_COLOR
    color_on(bottomwin, COLOR_STATUSBAR);
#else
    wattron(bottomwin, A_REVERSE);
#endif
    blank_statusbar();

    if (x <= COLS - 1) {
	/* Black magic */
	buf[len - 1] = ' ';

	mvwaddstr(bottomwin, 0, 0, buf);
	waddnstr(bottomwin, inputbuf, wid);
	wmove(bottomwin, 0, (x % COLS));
    } else {
	/* Black magic */
	buf[len - 1] = '$';

	mvwaddstr(bottomwin, 0, 0, buf);
	waddnstr(bottomwin, &inputbuf[wid * ((x - len) / (wid))], wid);
	wmove(bottomwin, 0, ((x - len) % wid) + len);
    }

#ifdef ENABLE_COLOR
    color_off(bottomwin, COLOR_STATUSBAR);
#else
    wattroff(bottomwin, A_REVERSE);
#endif
}

/* Get the input from the kb; this should only be called from statusq */
int nanogetstr(int allowtabs, char *buf, char *def, shortcut *s,
	       int start_x, int list)
{
    int kbinput = 0, x = 0, xend, slen;
    int x_left = 0, inputlen, tabbed = 0;
    char *inputbuf;
    shortcut *t;
#ifndef DISABLE_TABCOMP
    int shift = 0;
#endif

    slen = length_of_list(s);
    inputbuf = charalloc(strlen(def) + 1);
    inputbuf[0] = 0;

    x_left = strlen(buf);
    x = strlen(def) + x_left;

#if !defined(DISABLE_HELP) || !defined(DISABLE_MOUSE)
    currshortcut = s;
#endif

    /* Get the input! */
    if (strlen(def) > 0)
	strcpy(inputbuf, def);

    nanoget_repaint(buf, inputbuf, x);

    /* Make sure any editor screen updates are displayed before getting input */
    wrefresh(edit);

    while ((kbinput = wgetch(bottomwin)) != 13) {
	for (t = s; t != NULL; t = t->next) {
#ifdef DEBUG
	    fprintf(stderr, _("Aha! \'%c\' (%d)\n"), kbinput, kbinput);
#endif

	    if (kbinput == t->val && kbinput < 32) {

#ifndef DISABLE_HELP
		/* Have to do this here, it would be too late to do it in statusq */
		if (kbinput == NANO_HELP_KEY || kbinput == NANO_HELP_FKEY) {
		    do_help();
		    break;
		}
#endif

		/* We shouldn't discard the answer it gave, just because
		   we hit a keystroke, GEEZ! */
		answer = mallocstrcpy(answer, inputbuf);
		free(inputbuf);
		return t->val;
	    }
	}
	xend = strlen(buf) + strlen(inputbuf);

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
	    x = x_left;
	    break;
	case NANO_END_KEY:
	case KEY_END:
	    x = x_left + strlen(inputbuf);
	    break;
	case KEY_RIGHT:
	case NANO_FORWARD_KEY:

	    if (x < xend)
		x++;
	    wmove(bottomwin, 0, x);
	    break;
	case NANO_CONTROL_D:
	    if (strlen(inputbuf) > 0 && (x - x_left) != strlen(inputbuf)) {
		memmove(inputbuf + (x - x_left),
			inputbuf + (x - x_left) + 1,
			strlen(inputbuf) - (x - x_left) - 1);
		inputbuf[strlen(inputbuf) - 1] = 0;
	    }
	    break;
	case NANO_CONTROL_K:
	case NANO_CONTROL_U:
	    *inputbuf = 0;
	    x = x_left;
	    break;
	case KEY_BACKSPACE:
	case 127:
	case NANO_CONTROL_H:
	    if (strlen(inputbuf) > 0) {
		if (x == (x_left + strlen(inputbuf)))
		    inputbuf[strlen(inputbuf) - 1] = 0;
		else if (x - x_left) {
		    memmove(inputbuf + (x - x_left) - 1,
			    inputbuf + (x - x_left),
			    strlen(inputbuf) - (x - x_left));
		    inputbuf[strlen(inputbuf) - 1] = 0;
		}
	    }
	    if (x > strlen(buf))
		x--;
	    break;
#ifndef DISABLE_TABCOMP
	case NANO_CONTROL_I:
	    if (allowtabs) {
		shift = 0;
		inputbuf = input_tab(inputbuf, (x - x_left),
				     &tabbed, &shift, &list);
		x += shift;
		if (x - x_left > strlen(inputbuf))
		    x = strlen(inputbuf) + x_left;
	    }
	    break;
#endif
	case KEY_LEFT:
	case NANO_BACK_KEY:
	    if (x > strlen(buf))
		x--;
	    wmove(bottomwin, 0, x);
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
		    x = x_left + strlen(inputbuf);
		    break;
		case 'H':
		    x = x_left;
		    break;
		}
		break;
	    case '[':
		switch (kbinput = wgetch(edit)) {
		case 'C':
		    if (x < xend)
			x++;
		    wmove(bottomwin, 0, x);
		    break;
		case 'D':
		    if (x > strlen(buf))
			x--;
		    wmove(bottomwin, 0, x);
		    break;
		case '1':
		case '7':
		    x = x_left;
		    goto skip_tilde;
		case '3':
		  do_deletekey:
		    if (strlen(inputbuf) > 0
			&& (x - x_left) != strlen(inputbuf)) {
			memmove(inputbuf + (x - x_left),
				inputbuf + (x - x_left) + 1,
				strlen(inputbuf) - (x - x_left) - 1);
			inputbuf[strlen(inputbuf) - 1] = 0;
		    }
		    goto skip_tilde;
		case '4':
		case '8':
		    x = x_left + strlen(inputbuf);
		    goto skip_tilde;
		  skip_tilde:
		    nodelay(edit, TRUE);
		    kbinput = wgetch(edit);
		    if (kbinput == '~' || kbinput == ERR)
			kbinput = -1;
		    nodelay(edit, FALSE);
		    break;
		}
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
			answer = mallocstrcpy(answer, inputbuf);
			free(inputbuf);
			return t->val;
		    }
		}

	    }
	    break;

	default:

	    if (kbinput < 32)
		break;

	    inputlen = strlen(inputbuf);
	    inputbuf = nrealloc(inputbuf, inputlen + 2);

	    memmove(&inputbuf[x - x_left + 1],
		    &inputbuf[x - x_left], inputlen - (x - x_left) + 1);
	    inputbuf[x - x_left] = kbinput;

	    x++;

#ifdef DEBUG
	    fprintf(stderr, _("input \'%c\' (%d)\n"), kbinput, kbinput);
#endif
	}
	nanoget_repaint(buf, inputbuf, x);
	wrefresh(bottomwin);
    }

    answer = mallocstrcpy(answer, inputbuf);
    free(inputbuf);

    /* In pico mode, just check for a blank answer here */
    if (((ISSET(PICO_MODE)) && !strcmp(answer, "")))
	return -2;
    else
	return 0;
}

void horizbar(WINDOW * win, int y)
{
    wattron(win, A_REVERSE);
    mvwaddstr(win, 0, 0, hblank);
    wattroff(win, A_REVERSE);
}

void titlebar(char *path)
{
    int namelen, space;
    char *what = path;

    if (path == NULL)
	what = filename;

#ifdef ENABLE_COLOR
    color_on(topwin, COLOR_TITLEBAR);
    mvwaddstr(topwin, 0, 0, hblank);
#else
    horizbar(topwin, 0);
    wattron(topwin, A_REVERSE);
#endif


    mvwaddstr(topwin, 0, 3, VERMSG);

    space = COLS - strlen(VERMSG) - strlen(VERSION) - 21;

    namelen = strlen(what);

    if (!strcmp(what, ""))
	mvwaddstr(topwin, 0, COLS / 2 - 6, _("New Buffer"));
    else {
	if (namelen > space) {
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
    }
    if (ISSET(MODIFIED))
	mvwaddstr(topwin, 0, COLS - 10, _("Modified"));


#ifdef ENABLE_COLOR
    color_off(topwin, COLOR_TITLEBAR);
#else
    wattroff(topwin, A_REVERSE);
#endif

    wrefresh(topwin);
    reset_cursor();
}

void onekey(char *keystroke, char *desc, int len)
{
    int i;

    wattron(bottomwin, A_REVERSE);
    waddstr(bottomwin, keystroke);
    wattroff(bottomwin, A_REVERSE);
    waddch(bottomwin, ' ');
    waddnstr(bottomwin, desc, len - 3);
    for (i = strlen(desc); i < len - 3; i++)
        waddch(bottomwin, ' ');
}

void clear_bottomwin(void)
{
    if (ISSET(NO_HELP))
	return;

    mvwaddstr(bottomwin, 1, 0, hblank);
    mvwaddstr(bottomwin, 2, 0, hblank);
}

void bottombars(shortcut *s)
{
    int i, j, numcols;
    char keystr[10];
    shortcut *t;
    int slen;

    if (s == main_list)
	slen = MAIN_VISIBLE;
    else
	slen = length_of_list(s);

    if (ISSET(NO_HELP))
	return;

#ifdef ENABLE_COLOR
    color_on(bottomwin, COLOR_BOTTOMBARS);
    if (!colors[COLOR_BOTTOMBARS - FIRST_COLORNUM].set ||
	colors[COLOR_BOTTOMBARS - FIRST_COLORNUM].fg != COLOR_BLACK)
	wattroff(bottomwin, A_REVERSE);
#endif

    /* Determine how many extra spaces are needed to fill the bottom of the screen */
    if (slen < 2)
	numcols = 6;
    else
	numcols = (slen + (slen % 2)) / 2;

    clear_bottomwin();

    t = s;
    for (i = 0; i < numcols; i++) {
	for (j = 0; j <= 1; j++) {

	    wmove(bottomwin, 1 + j, i * ((COLS - 1) / numcols));

	    if (t->val < 97)
		snprintf(keystr, 10, "^%c", t->val + 64);
	    else
		snprintf(keystr, 10, "M-%c", t->val - 32);

	    onekey(keystr, t->desc, (COLS - 1) / numcols);

	    if (t->next == NULL)
		break;
	    t = t->next;
	}
	
    }

#ifdef ENABLE_COLOR
    color_off(bottomwin, COLOR_BOTTOMBARS);
#endif

    wrefresh(bottomwin);

}

/* If modified is not already set, set it and update titlebar */
void set_modified(void)
{
    if (!ISSET(MODIFIED)) {
	SET(MODIFIED);
	titlebar(NULL);
	wrefresh(topwin);
    }
}

/* And so start the display update routines */
/* Given a column, this returns the "page" it is on  */
/* "page" in the case of the display columns, means which set of 80 */
/* characters is viewable (e.g.: page 1 shows from 1 to COLS) */
inline int get_page_from_virtual(int virtual)
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
inline int get_page_start_virtual(int page)
{
    int virtual;
    virtual = --page * (COLS - 7);
    if (page)
	virtual -= 2 * page - 1;
    return virtual;
}

inline int get_page_end_virtual(int page)
{
    return get_page_start_virtual(page) + COLS - 1;
}

#ifndef NANO_SMALL
/* This takes care of the case where there is a mark that covers only */
/* the current line. */

/* It expects a line with no tab characters (i.e.: the type that edit_add */
/* deals with */
void add_marked_sameline(int begin, int end, filestruct * fileptr, int y,
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

#ifdef ENABLE_COLOR
    color_on(edit, COLOR_MARKER);
#else
    wattron(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

    mvwaddnstr(edit, y, begin - this_page_start,
	       &fileptr->data[begin], sel_data_len);

#ifdef ENABLE_COLOR
    color_off(edit, COLOR_MARKER);
#else
    wattroff(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

}
#endif

/* edit_add takes care of the job of actually painting a line into the
 * edit window.
 * 
 * Called only from update_line.  Expects a converted-to-not-have-tabs
 * line */
void edit_add(filestruct * fileptr, int yval, int start, int virt_cur_x,
	      int virt_mark_beginx, int this_page)
{

#ifdef ENABLE_COLOR
    colortype *tmpcolor = NULL;
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
		regcomp(&search_regexp, tmpcolor->start, 0);
		while (!regexec(&search_regexp, &fileptr->data[k], 1,
				regmatches, 0)) {

		    if (regmatches[0].rm_eo - regmatches[0].rm_so < 1) {
			statusbar("Refusing 0 length regex match");
			break;
		    }
#ifdef DEBUG
		    fprintf(stderr, "Match! (%d chars) \"%s\"\n",
			    regmatches[0].rm_eo - regmatches[0].rm_so,
			    &fileptr->data[k + regmatches[0].rm_so]);
#endif
		    if (regmatches[0].rm_so < COLS - 1) {
			if (tmpcolor->bright)
			    wattron(edit, A_BOLD);
			wattron(edit, COLOR_PAIR(tmpcolor->pairnum));

			if (regmatches[0].rm_eo + k <= COLS)
			    paintlen =
				regmatches[0].rm_eo - regmatches[0].rm_so;
			else
			    paintlen = COLS - k - regmatches[0].rm_so - 1;

			mvwaddnstr(edit, yval, regmatches[0].rm_so + k,
				   &fileptr->data[k + regmatches[0].rm_so],
				   paintlen);

		    }

		    if (tmpcolor->bright)
			wattroff(edit, A_BOLD);
		    wattroff(edit, COLOR_PAIR(tmpcolor->pairnum));

		    k += regmatches[0].rm_eo;

		}
	    }
	    /* Now, if there's an 'end' somewhere below, and a 'start'
	       somewhere above, things get really fun.  We have to look
	       down for an end, make sure there's not a start before 
	       the end after us, and then look up for a start, 
	       and see if there's an end after the start, before us :) */
	    else {

		s = fileptr;
		while (s != NULL) {
		    regcomp(&search_regexp, tmpcolor->start, 0);
		    if (!regexec
			(&search_regexp, s->data, 1, regmatches, 0))
			break;
		    s = s->prev;
		}

		if (s != NULL) {
		    /* We found a start, mark it */
		    smatch = regmatches[0].rm_so;

		    e = s;
		    while (e != NULL && e != fileptr) {
			regcomp(&search_regexp, tmpcolor->end, 0);
			if (!regexec
			    (&search_regexp, e->data, 1, regmatches, 0))
			    break;
			e = e->next;
		    }

		    if (e != fileptr)
			continue;	/* There's an end before us */
		    else {	/* Keep looking for an end */
			while (e != NULL) {
			    regcomp(&search_regexp, tmpcolor->end, 0);
			    if (!regexec
				(&search_regexp, e->data, 1, regmatches,
				 0))
				break;
			    e = e->next;
			}

			if (e == NULL)
			    continue;	/* There's no start before the end :) */
			else {	/* Okay, we found an end, mark it! */
			    ematch = regmatches[0].rm_eo;

			    while (e != NULL) {
				regcomp(&search_regexp, tmpcolor->end, 0);
				if (!regexec
				    (&search_regexp, e->data, 1,
				     regmatches, 0))
				    break;
				e = e->next;
			    }

			    if (e == NULL)
				continue;	/* No end, oh well :) */

			    /* Didn't find another end, we must be in the 
			       middle of a highlighted bit */

			    if (tmpcolor->bright)
				wattron(edit, A_BOLD);

			    wattron(edit, COLOR_PAIR(tmpcolor->pairnum));

			    if (s == fileptr && e == fileptr)
				mvwaddnstr(edit, yval, start + smatch, 
					&fileptr->data[start + smatch],
					ematch - smatch);
		    	    else if (s == fileptr)
				mvwaddnstr(edit, yval, start + smatch, 
					&fileptr->data[start + smatch],
					COLS - smatch);
			    else if (e == fileptr)
				mvwaddnstr(edit, yval, start, 
					&fileptr->data[start],
					ematch - start);
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
#ifdef ENABLE_COLOR
	    color_on(edit, COLOR_MARKER);
#else
	    wattron(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

	    mvwaddnstr(edit, yval, 0, fileptr->data, COLS);

#ifdef ENABLE_COLOR
	    color_off(edit, COLOR_MARKER);
#else
	    wattroff(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

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
#ifdef ENABLE_COLOR
		color_on(edit, COLOR_MARKER);
#else
		wattron(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

		target =
		    (virt_mark_beginx <
		     COLS - 1) ? virt_mark_beginx : COLS - 1;

		mvwaddnstr(edit, yval, 0, fileptr->data, target);

#ifdef ENABLE_COLOR
		color_off(edit, COLOR_MARKER);
#else
		wattroff(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */


	    }

	    if (mark_beginbuf->lineno < current->lineno) {
#ifdef ENABLE_COLOR
		color_on(edit, COLOR_MARKER);
#else
		wattron(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

		target = (COLS - 1) - virt_mark_beginx;

		if (target < 0)
		    target = 0;

		mvwaddnstr(edit, yval, virt_mark_beginx,
			   &fileptr->data[virt_mark_beginx], target);

#ifdef ENABLE_COLOR
		color_off(edit, COLOR_MARKER);
#else
		wattroff(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

	    }

	} else if (fileptr == current) {
	    /* We're on the cursor's line, but it's not the first
	     * one we marked.  Similar to the previous logic. */
	    int this_page_start = get_page_start_virtual(this_page),
		this_page_end = get_page_end_virtual(this_page);

	    if (mark_beginbuf->lineno < current->lineno) {

#ifdef ENABLE_COLOR
		color_on(edit, COLOR_MARKER);
#else
		wattron(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

		if (virt_cur_x > COLS - 2) {
		    mvwaddnstr(edit, yval, 0,
			       &fileptr->data[this_page_start],
			       virt_cur_x - this_page_start);
		} else
		    mvwaddnstr(edit, yval, 0, fileptr->data, virt_cur_x);

#ifdef ENABLE_COLOR
		color_off(edit, COLOR_MARKER);
#else
		wattroff(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

	    }

	    if (mark_beginbuf->lineno > current->lineno) {

#ifdef ENABLE_COLOR
		color_on(edit, COLOR_MARKER);
#else
		wattron(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

		if (virt_cur_x > COLS - 2)
		    mvwaddnstr(edit, yval, virt_cur_x - this_page_start,
			       &fileptr->data[virt_cur_x],
			       this_page_end - virt_cur_x);
		else
		    mvwaddnstr(edit, yval, virt_cur_x,
			       &fileptr->data[virt_cur_x],
			       COLS - virt_cur_x);

#ifdef ENABLE_COLOR
		color_off(edit, COLOR_MARKER);
#else
		wattroff(edit, A_REVERSE);
#endif				/* ENABLE_COLOR */

	    }
	}
    }
#endif

}

/*
 * Just update one line in the edit buffer.  Basically a wrapper for
 * edit_add
 *
 * index gives us a place in the string to update starting from.
 * Likely args are current_x or 0.
 */
void update_line(filestruct * fileptr, int index)
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
	} else if (realdata[i] >= 1 && realdata[i] <= 31) {
	    /* Treat control characters as ^letter */
	    fileptr->data[pos++] = '^';
	    fileptr->data[pos++] = realdata[i] + 64;
	} else {
	    fileptr->data[pos++] = realdata[i];
	}
    }

    fileptr->data[pos] = '\0';

    /* Now, Paint the line */
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

void center_cursor(void)
{
    current_y = editwinrows / 2;
    wmove(edit, current_y, current_x);
}

/* Refresh the screen without changing the position of lines */
void edit_refresh(void)
{
    static int noloop = 0;
    int nlines = 0, i = 0, currentcheck = 0;
    filestruct *temp, *hold = current;

    if (current == NULL)
	return;

    temp = edittop;

    while (nlines <= editwinrows - 1 && nlines <= totlines && temp != NULL) {
	hold = temp;
	update_line(temp, current_x);
	if (temp == current)
	    currentcheck = 1;

	temp = temp->next;
	nlines++;
    }
    /* If noloop == 1, then we already did an edit_update without finishing
       this function.  So we don't run edit_update again */
    if (!currentcheck && !noloop) {	/* Then current has run off the screen... */
	edit_update(current, CENTER);
	noloop = 1;
    } else if (noloop)
	noloop = 0;

    if (nlines <= editwinrows - 1)
	while (nlines <= editwinrows - 1) {
	    mvwaddstr(edit, nlines, i, hblank);
	    nlines++;
	}
    if (temp == NULL)
	editbot = hold;
    else
	editbot = temp;

    /* What the hell are we expecting to update the screen if this isn't 
       here? luck?? */
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
void edit_update(filestruct * fileptr, int topmidbotnone)
{
    int i = 0;
    filestruct *temp;

    if (fileptr == NULL)
	return;

    temp = fileptr;
    if (topmidbotnone == TOP);
    else if (topmidbotnone == NONE)
	for (i = 0; i <= current_y - 1 && temp->prev != NULL; i++)
	    temp = temp->prev;
    else if (topmidbotnone == BOTTOM)
	for (i = 0; i <= editwinrows - 1 && temp->prev != NULL; i++)
	    temp = temp->prev;
    else
	for (i = 0; i <= editwinrows / 2 && temp->prev != NULL; i++)
	    temp = temp->prev;

    edittop = temp;
    fix_editbot();

    edit_refresh();
}

/* This function updates current, based on where current_y is; reset_cursor 
   does the opposite */
void update_cursor(void)
{
    int i = 0;

#ifdef DEBUG
    fprintf(stderr, _("Moved to (%d, %d) in edit buffer\n"), current_y,
	    current_x);
#endif

    current = edittop;
    while (i <= current_y - 1 && current->next != NULL) {
	current = current->next;
	i++;
    }

#ifdef DEBUG
    fprintf(stderr, _("current->data = \"%s\"\n"), current->data);
#endif

}

/*
 * Ask a question on the statusbar.  Answer will be stored in answer
 * global.  Returns -1 on aborted enter, -2 on a blank string, and 0
 * otherwise, the valid shortcut key caught.  Def is any editable text we
 * want to put up by default.
 *
 * New arg tabs tells whether or not to allow tab completion.
 */
int statusq(int tabs, shortcut *s, char *def, char *msg, ...)
{
    va_list ap;
    char foo[133];
    int ret;

#ifndef DISABLE_TABCOMP
    int list = 0;
#endif

    bottombars(s);

    va_start(ap, msg);
    vsnprintf(foo, 132, msg, ap);
    va_end(ap);
    strncat(foo, ": ", 132);

#ifdef ENABLE_COLOR
    color_on(bottomwin, COLOR_STATUSBAR);
#else
    wattron(bottomwin, A_REVERSE);
#endif


#ifndef DISABLE_TABCOMP
    ret = nanogetstr(tabs, foo, def, s, (strlen(foo) + 3), list);
#else
    /* if we've disabled tab completion, the value of list won't be
       used at all, so it's safe to use 0 (NULL) as a placeholder */
    ret = nanogetstr(tabs, foo, def, s, (strlen(foo) + 3), 0);
#endif

#ifdef ENABLE_COLOR
    color_off(bottomwin, COLOR_STATUSBAR);
#else
    wattroff(bottomwin, A_REVERSE);
#endif


    switch (ret) {

    case NANO_FIRSTLINE_KEY:
	do_first_line();
	break;
    case NANO_LASTLINE_KEY:
	do_last_line();
	break;
    case NANO_CANCEL_KEY:
#ifndef DISABLE_TABCOMP
	/* if we've done tab completion, there might be a list of
	   filename matches on the edit window at this point; make sure
	   they're cleared off */
	if (list)
	    edit_refresh();
#endif
	return -1;
    default:
	blank_statusbar();
    }

#ifdef DEBUG
    fprintf(stderr, _("I got \"%s\"\n"), answer);
#endif

    return ret;
}

/*
 * Ask a simple yes/no question on the statusbar.  Returns 1 for Y, 0 for
 * N, 2 for All (if all is non-zero when passed in) and -1 for abort (^C)
 */
int do_yesno(int all, int leavecursor, char *msg, ...)
{
    va_list ap;
    char foo[133];
    int kbinput, ok = -1, i;
    char *yesstr;		/* String of yes characters accepted */
    char *nostr;		/* Same for no */
    char *allstr;		/* And all, surprise! */
    char shortstr[5];		/* Temp string for above */
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
    clear_bottomwin();

#ifdef ENABLE_COLOR
    color_on(bottomwin, COLOR_BOTTOMBARS);
#endif

    /* Remove gettext call for keybindings until we clear the thing up */
    if (!ISSET(NO_HELP)) {
	wmove(bottomwin, 1, 0);

	snprintf(shortstr, 3, " %c", yesstr[0]);
	onekey(shortstr, _("Yes"), 16);

	if (all) {
	    snprintf(shortstr, 3, " %c", allstr[0]);
	    onekey(shortstr, _("All"), 16);
	}
	wmove(bottomwin, 2, 0);

	snprintf(shortstr, 3, " %c", nostr[0]);
	onekey(shortstr, _("No"), 16);

	onekey("^C", _("Cancel"), 16);
    }
    va_start(ap, msg);
    vsnprintf(foo, 132, msg, ap);
    va_end(ap);

#ifdef ENABLE_COLOR
    color_off(bottomwin, COLOR_BOTTOMBARS);
    color_on(bottomwin, COLOR_STATUSBAR);
#else
    wattron(bottomwin, A_REVERSE);
#endif				/* ENABLE_COLOR */

    blank_statusbar();
    mvwaddstr(bottomwin, 0, 0, foo);

#ifdef ENABLE_COLOR
    color_off(bottomwin, COLOR_STATUSBAR);
#else
    wattroff(bottomwin, A_REVERSE);
#endif

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
		   of possible return keystrokes based on the x and y values */
		if (all) {
		    char yesnosquare[2][2] = {
			{yesstr[0], allstr[0]},
			{nostr[0], NANO_CONTROL_C}
		    };

		    ungetch(yesnosquare[mevent.y][mevent.x / (COLS / 6)]);
		} else {
		    char yesnosquare[2][2] = {
			{yesstr[0], '\0'},
			{nostr[0], NANO_CONTROL_C}
		    };

		    ungetch(yesnosquare[mevent.y][mevent.x / (COLS / 6)]);
		}
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

void statusbar(char *msg, ...)
{
    va_list ap;
    char foo[133];
    int start_x = 0;

    va_start(ap, msg);
    vsnprintf(foo, 132, msg, ap);
    va_end(ap);

    start_x = COLS / 2 - strlen(foo) / 2 - 1;

    /* Blank out line */
    blank_statusbar();

    wmove(bottomwin, 0, start_x);

#ifdef ENABLE_COLOR
    color_on(bottomwin, COLOR_STATUSBAR);
#else
    wattron(bottomwin, A_REVERSE);
#endif

    waddstr(bottomwin, "[ ");
    waddstr(bottomwin, foo);
    waddstr(bottomwin, " ]");

#ifdef ENABLE_COLOR
    color_off(bottomwin, COLOR_STATUSBAR);
#else
    wattroff(bottomwin, A_REVERSE);
#endif

    wrefresh(bottomwin);

    if (ISSET(CONSTUPDATE))
	statblank = 1;
    else
	statblank = 25;
}

void display_main_list(void)
{
    bottombars(main_list);
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

void previous_line(void)
{
    if (current_y > 0)
	current_y--;
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

    if (strlen(current->data) == 0)
	colpct = 0;
    else
	colpct = 100 * xplustabs() / xpt(current, strlen(current->data));

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
		  current->lineno, totlines, linepct, xplustabs(), 
		  xpt(current, strlen(current->data)), colpct, i, j, bytepct);
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

/* Our broken, non-shortcut list compliant help function.
   But, hey, it's better than nothing, and it's dynamic! */
int do_help(void)
{
#ifndef DISABLE_HELP
    char *ptr, *end;
    int i, j, row = 0, page = 1, kbinput = 0, no_more = 0, kp, kp2;
    int no_help_flag = 0;
    shortcut *oldshortcut;

    blank_edit();
    curs_set(0);
    wattroff(bottomwin, A_REVERSE);
    blank_statusbar();

    help_init();
    ptr = help_text;

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
	ptr = help_text;
	switch (kbinput) {
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
	case KEY_MOUSE:
	    do_mouse();
	    break;
#endif
#endif
	case NANO_NEXTPAGE_KEY:
	case NANO_NEXTPAGE_FKEY:
	case KEY_NPAGE:
	    if (!no_more) {
		blank_edit();
		page++;
	    }
	    break;
	case NANO_PREVPAGE_KEY:
	case NANO_PREVPAGE_FKEY:
	case KEY_PPAGE:
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

	if (i > 1) {

	}

	i = 0;
	j = 0;
	while (i < editwinrows && *ptr != '\0') {
	    end = ptr;
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

#elif defined(DISABLE_HELP)
    nano_disabled_msg();
#endif

    return 1;
}

/* Dump the current file structure to stderr */
void dump_buffer(filestruct * inptr)
{
#ifdef DEBUG
    filestruct *fileptr;

    if (inptr == fileage)
	fprintf(stderr, _("Dumping file buffer to stderr...\n"));
    else if (inptr == cutbuffer)
	fprintf(stderr, _("Dumping cutbuffer to stderr...\n"));
    else
	fprintf(stderr, _("Dumping a buffer to stderr...\n"));

    fileptr = inptr;
    while (fileptr != NULL) {
	fprintf(stderr, "(%d) %s\n", fileptr->lineno, fileptr->data);
	fflush(stderr);
	fileptr = fileptr->next;
    }
#endif				/* DEBUG */
}

void dump_buffer_reverse(filestruct * inptr)
{
#ifdef DEBUG
    filestruct *fileptr;

    fileptr = filebot;
    while (fileptr != NULL) {
	fprintf(stderr, "(%d) %s\n", fileptr->lineno, fileptr->data);
	fflush(stderr);
	fileptr = fileptr->prev;
    }
#endif				/* DEBUG */
}

/* Fix editbot, based on the assumption that edittop is correct */
void fix_editbot(void)
{
    int i;
    editbot = edittop;
    for (i = 0; (i <= editwinrows - 1) && (editbot->next != NULL)
	 && (editbot != filebot); i++, editbot = editbot->next);
}

/* highlight the current word being replaced or spell checked */
void do_replace_highlight(int highlight_flag, char *word)
{
    char *highlight_word = NULL;
    int x, y;

    highlight_word =
	mallocstrcpy(highlight_word, &current->data[current_x]);
    highlight_word[strlen(word)] = '\0';

    /* adjust output when word extends beyond screen */

    x = xplustabs();
    y = get_page_end_virtual(get_page_from_virtual(x)) + 1;

    if ((COLS - (y - x) + strlen(word)) > COLS) {
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

#ifdef NANO_EXTRA
#define CREDIT_LEN 52
void do_credits(void)
{
    int i, j = 0, k, place = 0, start_x;
    char *what;

    char *nanotext = _("The nano text editor");
    char *version = _("version ");
    char *brought = _("Brought to you by:");
    char *specialthx = _("Special thanks to:");
    char *fsf = _("The Free Software Foundation");
    char *ncurses = _("For ncurses:");
    char *anyonelse = _("and anyone else we forgot...");
    char *thankyou = _("Thank you for using nano!\n");

    char *credits[CREDIT_LEN] = { nanotext,
	version,
	VERSION,
	"",
	brought,
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
	specialthx,
	"Plattsburgh State University",
	"Benet Laboratories",
	"Amy Allegretta",
	"Linda Young",
	"Jeremy Robichaud",
	"Richard Kolb II",
	fsf,
	"Linus Torvalds",
	ncurses,
	"Thomas Dickey",
	"Pavel Curtis",
	"Zeyd Ben-Halim",
	"Eric S. Raymond",
	anyonelse,
	thankyou,
	"", "", "", "",
	"(c) 1999-2002 Chris Allegretta",
	"", "", "", "",
	"www.nano-editor.org"
    };

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

		if (place - (editwinrows / 2 - 1 - i) < CREDIT_LEN)
		    what = credits[place - (editwinrows / 2 - 1 - i)];
		else
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

int keypad_on(WINDOW * win, int newval)
{

/* This is taken right from aumix.  Don't sue me. */
#ifdef HAVE_USEKEYPAD
    int old;

    old = win->_use_keypad;
    keypad(win, newval);
    return old;
#else
    keypad(win, newval);
    return 1;
#endif				/* HAVE_USEKEYPAD */

}
