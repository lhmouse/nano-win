/**************************************************************************
 *   winio.c                                                              *
 *                                                                        *
 *   Copyright (C) 1999 Chris Allegretta                                  *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 1, or (at your option)  *
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
#include "config.h"
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

static int statblank = 0;	/* Number of keystrokes left after
				   we call statubar() before we
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
    edit_update(current);
    return 1;
}

int do_last_line(void)
{
    current = filebot;
    placewewant = 0;
    current_x = 0;
    edit_update(current);
    return 1;
}

/* Like xplustabs, but for a specifc index of a speficific filestruct */
int xpt(filestruct * fileptr, int index)
{
    int i, tabs = 0;

    if (fileptr == NULL || fileptr->data == NULL)
	return 0;

    for (i = 0; i < index && fileptr->data[i] != 0; i++) {
	tabs++;

	if (fileptr->data[i] == NANO_CONTROL_I) {
	    if (tabs % TABSIZE == 0);
	    else
		tabs += TABSIZE - (tabs % TABSIZE);
	} else if (fileptr->data[i] & 0x80)
	    /* Make 8 bit chars only 1 collumn! */
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
	    if (tot % TABSIZE == 0)
		tot++;
	    else
		tot += TABSIZE - (tot % TABSIZE);
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
inline int actual_x(filestruct * fileptr, int xplus)
{
    return actual_x_from_start(fileptr, xplus, 0);
}

/* a strlen with tabs factored in, similar to xplustabs() */
int strlenpt(char *buf)
{
    int i, tabs = 0;

    if (buf == NULL)
	return 0;

    for (i = 0; buf[i] != 0; i++) {
	tabs++;

	if (buf[i] == NANO_CONTROL_I) {
	    if (tabs % TABSIZE == 0);
	    else
		tabs += TABSIZE - (tabs % TABSIZE);
	} else if (buf[i] & 0x80)
	    /* Make 8 bit chars only 1 collumn! */
	    ;
	else if (buf[i] < 32)
	    tabs++;
    }

    return tabs;
}


/* resets current_y based on the position of current and puts the cursor at 
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
    wrefresh(edit);
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

/* Get the input from the kb, this should only be called from statusq */
int nanogetstr(char *buf, char *def, shortcut s[], int slen, int start_x)
{
    int kbinput = 0, j = 0, x = 0, xend;
    int x_left = 0;
    char inputstr[132], inputbuf[132] = "";

    blank_statusbar();
    mvwaddstr(bottomwin, 0, 0, buf);
    if (strlen(def) > 0)
	waddstr(bottomwin, def);
    wrefresh(bottomwin);

    x_left = strlen(buf);
    x = strlen(def) + x_left;

    /* Get the input! */
    if (strlen(def) > 0) {
	strcpy(answer, def);
	strcpy(inputbuf, def);
    }

    while ((kbinput = wgetch(bottomwin)) != 13) {
	for (j = 0; j <= slen - 1; j++) {
	    if (kbinput == s[j].val) {
		strcpy(answer, "");
		return s[j].val;
	    }
	}
	xend = strlen(buf) + strlen(inputbuf);

	switch (kbinput) {
	case KEY_HOME:
	    x = x_left;
	    blank_statusbar();
	    mvwaddstr(bottomwin, 0, 0, buf);
	    waddstr(bottomwin, inputbuf);
	    wmove(bottomwin, 0, x);
	    break;
	case KEY_END:
	    x = x_left + strlen(inputbuf);
	    blank_statusbar();
	    mvwaddstr(bottomwin, 0, 0, buf);
	    waddstr(bottomwin, inputbuf);
	    wmove(bottomwin, 0, x);
	    break;
	case KEY_RIGHT:

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
	    blank_statusbar();
	    mvwaddstr(bottomwin, 0, 0, buf);
	    waddstr(bottomwin, inputbuf);
	    wmove(bottomwin, 0, x);
	    break;
	case NANO_CONTROL_K:
	case NANO_CONTROL_U:
	    *inputbuf = 0;
	    x = x_left;
	    blank_statusbar();
	    mvwaddstr(bottomwin, 0, 0, buf);
	    waddstr(bottomwin, inputbuf);
	    wmove(bottomwin, 0, x);
	    break;
	case KEY_BACKSPACE:
	case KEY_DC:
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
	    blank_statusbar();
	    mvwaddstr(bottomwin, 0, 0, buf);
	    waddstr(bottomwin, inputbuf);
	case KEY_LEFT:
	    if (x > strlen(buf))
		x--;
	    wmove(bottomwin, 0, x);
	    break;
	case KEY_UP:
	case KEY_DOWN:
	    break;

	case 27:
	    switch (kbinput = wgetch(edit)) {
	    case 79:
		switch (kbinput = wgetch(edit)) {
		case 70:
		    x = x_left + strlen(inputbuf);
		    blank_statusbar();
		    mvwaddstr(bottomwin, 0, 0, buf);
		    waddstr(bottomwin, inputbuf);
		    wmove(bottomwin, 0, x);
		    break;
		case 72:
		    x = x_left;
		    blank_statusbar();
		    mvwaddstr(bottomwin, 0, 0, buf);
		    waddstr(bottomwin, inputbuf);
		    wmove(bottomwin, 0, x);
		    break;
		}
		break;
	    case 91:
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
		case 49:
		    x = x_left;
		    blank_statusbar();
		    mvwaddstr(bottomwin, 0, 0, buf);
		    waddstr(bottomwin, inputbuf);
		    wmove(bottomwin, 0, x);
		    goto skip_126;
		case 51:
		    if (strlen(inputbuf) > 0
			&& (x - x_left) != strlen(inputbuf)) {
			memmove(inputbuf + (x - x_left),
				inputbuf + (x - x_left) + 1,
				strlen(inputbuf) - (x - x_left) - 1);
			inputbuf[strlen(inputbuf) - 1] = 0;
		    }
		    blank_statusbar();
		    mvwaddstr(bottomwin, 0, 0, buf);
		    waddstr(bottomwin, inputbuf);
		    wmove(bottomwin, 0, x);
		    goto skip_126;
		case 52:
		    x = x_left + strlen(inputbuf);
		    blank_statusbar();
		    mvwaddstr(bottomwin, 0, 0, buf);
		    waddstr(bottomwin, inputbuf);
		    wmove(bottomwin, 0, x);
		    goto skip_126;
		  skip_126:
		    nodelay(edit, TRUE);
		    kbinput = wgetch(edit);
		    if (kbinput == 126 || kbinput == ERR)
			kbinput = -1;
		    nodelay(edit, FALSE);
		    break;
		}
	    }
	    blank_statusbar();
	    mvwaddstr(bottomwin, 0, 0, buf);
	    waddstr(bottomwin, inputbuf);
	    wmove(bottomwin, 0, x);
	    break;


	default:
	    if (kbinput < 32)
		break;
	    strcpy(inputstr, inputbuf);
	    inputstr[x - strlen(buf)] = kbinput;
	    strcpy(&inputstr[x - strlen(buf) + 1],
		   &inputbuf[x - strlen(buf)]);
	    strcpy(inputbuf, inputstr);
	    x++;

	    mvwaddstr(bottomwin, 0, 0, buf);
	    waddstr(bottomwin, inputbuf);
	    wmove(bottomwin, 0, x);

#ifdef DEBUG
	    fprintf(stderr, _("input \'%c\' (%d)\n"), kbinput, kbinput);
#endif
	}
	wrefresh(bottomwin);
    }

    strncpy(answer, inputbuf, 132);

    if (!strcmp(answer, ""))
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

void titlebar(void)
{
    int namelen, space;

    horizbar(topwin, 0);
    wattron(topwin, A_REVERSE);
    mvwaddstr(topwin, 0, 3, VERMSG);

    space = COLS - strlen(VERMSG) - strlen(VERSION) - 21;

    namelen = strlen(filename);

    if (!strcmp(filename, ""))
	mvwaddstr(topwin, 0, center_x - 6, _("New Buffer"));
    else {
	if (namelen > space) {
	    waddstr(topwin, _("  File: ..."));
	    waddstr(topwin, &filename[namelen - space]);
	} else {
	    mvwaddstr(topwin, 0, center_x - (namelen / 2 + 1), "File: ");
	    waddstr(topwin, filename);
	}
    }
    if (ISSET(MODIFIED))
	mvwaddstr(topwin, 0, COLS - 10, _("Modified"));
    wattroff(topwin, A_REVERSE);
    wrefresh(topwin);
    reset_cursor();
}

void onekey(char *keystroke, char *desc)
{
    char description[80];

    snprintf(description, 12, " %-11s", desc);
    wattron(bottomwin, A_REVERSE);
    waddstr(bottomwin, keystroke);
    wattroff(bottomwin, A_REVERSE);
    waddstr(bottomwin, description);
}

void clear_bottomwin(void)
{
    if (ISSET(NO_HELP))
	return;

    mvwaddstr(bottomwin, 1, 0, hblank);
    mvwaddstr(bottomwin, 2, 0, hblank);
    wrefresh(bottomwin);
}

void bottombars(shortcut s[], int slen)
{
    int i, j, k;
    char keystr[10];

    if (ISSET(NO_HELP))
	return;

    /* Determine how many extra spaces are needed to fill the bottom of the screen */
    k = COLS / 6 - 13;

    clear_bottomwin();
    wmove(bottomwin, 1, 0);
    for (i = 0; i <= slen - 1; i += 2) {
	sprintf(keystr, "^%c", s[i].val + 64);
	onekey(keystr, s[i].desc);

	for (j = 0; j < k; j++)
	    waddch(bottomwin, ' ');
    }

    wmove(bottomwin, 2, 0);
    for (i = 1; i <= slen - 1; i += 2) {
	sprintf(keystr, "^%c", s[i].val + 64);
	onekey(keystr, s[i].desc);

	for (j = 0; j < k; j++)
	    waddch(bottomwin, ' ');
    }

    wrefresh(bottomwin);

}

/* If modified is not already set, set it and update titlebar */
void set_modified(void)
{
    if (!ISSET(MODIFIED)) {
	SET(MODIFIED);
	titlebar();
	wrefresh(topwin);
    }
}

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
/* Called only from edit_add, and therefore expects a
 * converted-to-only-spaces line */
void add_marked_sameline(int begin, int end, filestruct * fileptr, int y,
			 int virt_cur_x, int this_page)
{
    int this_page_start = get_page_start_virtual(this_page),
	this_page_end = get_page_end_virtual(this_page);

    /* 3 start points: 0 -> begin, begin->end, end->strlen(data) */
    /*    in data    :    pre          sel           post        */
    /* likewise, 3 data lengths */
    int pre_data_len = begin, sel_data_len = end - begin, post_data_len = 0;	/* Determined from the other two */

    /* now fix the start locations & lengths according to the cursor's 
     * position (ie: our page) */
    if (pre_data_len < this_page_start)
	pre_data_len = 0;
    else
	pre_data_len -= this_page_start;

    if (begin < this_page_start)
	begin = this_page_start;

    if (end < this_page_start)
	end = this_page_start;

    /* we don't care about end, because it will just get cropped
     * due to length */
    if (begin > this_page_end)
	begin = this_page_end;

    if (end > this_page_end)
	end = this_page_end;

    sel_data_len = end - begin;
    post_data_len = this_page_end - end;

    mvwaddnstr(edit, y, 0, &fileptr->data[this_page_start], pre_data_len);
    wattron(edit, A_REVERSE);
    mvwaddnstr(edit, y, begin - this_page_start,
	       &fileptr->data[begin], sel_data_len);
    wattroff(edit, A_REVERSE);
    mvwaddnstr(edit, y, end - this_page_start,
	       &fileptr->data[end], post_data_len);
}
#endif

/* Called only from update_line.  Expects a converted-to-not-have-tabs
 * line */
void edit_add(filestruct * fileptr, int yval, int start, int virt_cur_x,
	      int virt_mark_beginx, int this_page)
{
#ifndef NANO_SMALL
    if (ISSET(MARK_ISSET)) {
	if ((fileptr->lineno > mark_beginbuf->lineno
	     && fileptr->lineno > current->lineno)
	    || (fileptr->lineno < mark_beginbuf->lineno
		&& fileptr->lineno < current->lineno)) {

	    /* We're on a normal, unselected line */
	    mvwaddnstr(edit, yval, 0, fileptr->data, COLS);
	} else {
	    if (fileptr != mark_beginbuf && fileptr != current) {
		/* We're on selected text */
		wattron(edit, A_REVERSE);
		mvwaddnstr(edit, yval, 0, fileptr->data, COLS);
		wattroff(edit, A_REVERSE);
	    } else if (fileptr == mark_beginbuf && fileptr == current) {
		/* Special case, we're still on the same line we started
		 * marking */
		if (virt_cur_x < virt_mark_beginx)
		    add_marked_sameline(virt_cur_x, virt_mark_beginx,
					fileptr, yval, virt_cur_x,
					this_page);
		else
		    add_marked_sameline(virt_mark_beginx, virt_cur_x,
					fileptr, yval, virt_cur_x,
					this_page);
	    } else if (fileptr == mark_beginbuf) {
		/* we're updating the line that was first marked */
		int target;

		if (mark_beginbuf->lineno > current->lineno)
		    wattron(edit, A_REVERSE);

		target =
		    (virt_mark_beginx <
		     COLS - 1) ? virt_mark_beginx : COLS - 1;

		mvwaddnstr(edit, yval, 0, fileptr->data, target);

		if (mark_beginbuf->lineno < current->lineno)
		    wattron(edit, A_REVERSE);
		else
		    wattroff(edit, A_REVERSE);

		target = (COLS - 1) - virt_mark_beginx;
		if (target < 0)
		    target = 0;

		mvwaddnstr(edit, yval, virt_mark_beginx,
			   &fileptr->data[virt_mark_beginx], target);

		if (mark_beginbuf->lineno < current->lineno)
		    wattroff(edit, A_REVERSE);

	    } else if (fileptr == current) {
		/* we're on the cursors line, but it's not the first
		 * one we marked... */
		int this_page_start = get_page_start_virtual(this_page),
		    this_page_end = get_page_end_virtual(this_page);

		if (mark_beginbuf->lineno < current->lineno)
		    wattron(edit, A_REVERSE);

		if (virt_cur_x > COLS - 2) {
		    mvwaddnstr(edit, yval, 0,
			       &fileptr->data[this_page_start],
			       virt_cur_x - this_page_start);
		} else {
		    mvwaddnstr(edit, yval, 0, fileptr->data, virt_cur_x);
		}

		if (mark_beginbuf->lineno > current->lineno)
		    wattron(edit, A_REVERSE);
		else
		    wattroff(edit, A_REVERSE);

		if (virt_cur_x > COLS - 2)
		    mvwaddnstr(edit, yval, virt_cur_x - this_page_start,
			       &fileptr->data[virt_cur_x],
			       this_page_end - virt_cur_x);
		else
		    mvwaddnstr(edit, yval, virt_cur_x,
			       &fileptr->data[virt_cur_x],
			       COLS - virt_cur_x);

		if (mark_beginbuf->lineno > current->lineno)
		    wattroff(edit, A_REVERSE);
	    }
	}

    } else
#endif
	mvwaddnstr(edit, yval, 0, &fileptr->data[start],
		   get_page_end_virtual(this_page) - start);
}

/*
 * Just update one line in the edit buffer 
 *
 * index gives is a place in the string to update starting from.
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

    /* Next, convert all the tabs to spaces so everything else is easy */
    index = xpt(fileptr, index);

    realdata = fileptr->data;
    len = strlen(realdata);
    fileptr->data = nmalloc(xpt(fileptr, len) + 1);

    pos = 0;
    for (i = 0; i < len; i++) {
	if (realdata[i] == '\t') {
	    do {
		fileptr->data[pos++] = ' ';
		if (i < current_x)
		    virt_cur_x++;
		if (i < mark_beginx)
		    virt_mark_beginx++;
	    } while (pos % TABSIZE);
	    /* must decrement once to account for tab-is-one-character */
	    if (i < current_x)
		virt_cur_x--;
	    if (i < mark_beginx)
		virt_mark_beginx--;
	} else {
	    fileptr->data[pos++] = realdata[i];
	}
    }

    fileptr->data[pos] = '\0';

    /* Now, Paint the line */
    if (current == fileptr && index > COLS - 2) {
	/* This handles when the current line is beyond COLS */
	/* It requires figureing out what page we're at      */
	page = get_page_from_virtual(index);
	col = get_page_start_virtual(page);

	edit_add(filetmp, line, col, virt_cur_x, virt_mark_beginx, page);
	mvwaddch(edit, line, 0, '$');

	if (strlenpt(fileptr->data) > get_page_end_virtual(page))
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
    int lines = 0, i = 0;
    filestruct *temp, *hold = current;

    if (current == NULL)
	return;

    temp = edittop;

    while (lines <= editwinrows - 1 && lines <= totlines && temp != NULL) {
	hold = temp;
	update_line(temp, current_x);
	temp = temp->next;
	lines++;
    }

    if (lines <= editwinrows - 1)
	while (lines <= editwinrows - 1) {
	    mvwaddstr(edit, lines, i, hblank);
	    lines++;
	}
    if (temp == NULL)
	editbot = hold;
    else
	editbot = temp;
}

/*
 * Nice generic routine to update the edit buffer given a pointer to the
 * file struct =) 
 */
void edit_update(filestruct * fileptr)
{

    int lines = 0, i = 0;
    filestruct *temp;

    if (fileptr == NULL)
	return;

    temp = fileptr;
    while (i <= editwinrows / 2 && temp->prev != NULL) {
	i++;
	temp = temp->prev;
    }
    edittop = temp;

    while (lines <= editwinrows - 1 && lines <= totlines && temp != NULL
	   && temp != filebot) {
	temp = temp->next;
	lines++;
    }
    editbot = temp;

    edit_refresh();
}

/* Now we want to update the screen using this struct as the top of the edit buffer */
void edit_update_top(filestruct * fileptr)
{
    int i;
    filestruct *temp = fileptr;

    if (fileptr->next == NULL || fileptr == NULL)
	return;

    i = 0;
    while (i <= editwinrows / 2 && temp->next != NULL) {
	i++;
	temp = temp->next;
    }
    edit_update(temp);
}

/* And also for the bottom... */
void edit_update_bot(filestruct * fileptr)
{
    int i;
    filestruct *temp = fileptr;

    i = 0;
    while (i <= editwinrows / 2 - 1 && temp->prev != NULL) {
	i++;
	temp = temp->prev;
    }
    edit_update(temp);
}

/* This function updates current based on where current_y is, reset_cursor 
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
 * otherwise, the valid shortcut key caught, Def is any editable text we
 * want to put up by default.
 */
int statusq(shortcut s[], int slen, char *def, char *msg, ...)
{
    va_list ap;
    char foo[133];
    int ret;

    bottombars(s, slen);

    va_start(ap, msg);
    vsnprintf(foo, 132, msg, ap);
    strncat(foo, ": ", 132);
    va_end(ap);

    wattron(bottomwin, A_REVERSE);
    ret = nanogetstr(foo, def, s, slen, (strlen(foo) + 3));
    wattroff(bottomwin, A_REVERSE);

    switch (ret) {

    case NANO_FIRSTLINE_KEY:
	do_first_line();
	break;
    case NANO_LASTLINE_KEY:
	do_last_line();
	break;
    case NANO_CANCEL_KEY:
	return -1;
    default:
	blank_statusbar_refresh();
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
    int kbinput, ok = -1;

    /* Write the bottom of the screen */
    clear_bottomwin();
    wattron(bottomwin, A_REVERSE);
    blank_statusbar_refresh();
    wattroff(bottomwin, A_REVERSE);

    /* Remove gettext call for keybindings until we clear the thing up */
    if (!ISSET(NO_HELP)) {
	wmove(bottomwin, 1, 0);
	onekey(" Y", _("Yes"));
	if (all)
	    onekey(" A", _("All"));
	wmove(bottomwin, 2, 0);
	onekey(" N", _("No"));
	onekey("^C", _("Cancel"));
    }
    va_start(ap, msg);
    vsnprintf(foo, 132, msg, ap);
    va_end(ap);
    wattron(bottomwin, A_REVERSE);
    mvwaddstr(bottomwin, 0, 0, foo);
    wattroff(bottomwin, A_REVERSE);
    wrefresh(bottomwin);

    if (leavecursor == 1)
	reset_cursor();

    while (ok == -1) {
	kbinput = wgetch(edit);

	switch (kbinput) {
	case 'Y':
	case 'y':
	    ok = 1;
	    break;
	case 'N':
	case 'n':
	    ok = 0;
	    break;
	case 'A':
	case 'a':
	    if (all)
		ok = 2;
	    break;
	case NANO_CONTROL_C:
	    ok = -2;
	    break;
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

    start_x = center_x - strlen(foo) / 2 - 1;

    /* Blank out line */
    blank_statusbar();

    wmove(bottomwin, 0, start_x);

    wattron(bottomwin, A_REVERSE);

    waddstr(bottomwin, "[ ");
    waddstr(bottomwin, foo);
    waddstr(bottomwin, " ]");
    wattroff(bottomwin, A_REVERSE);
    wrefresh(bottomwin);

    if (ISSET(CONSTUPDATE))
	statblank = 1;
    else
	statblank = 25;
}

void display_main_list(void)
{
    bottombars(main_list, MAIN_VISIBLE);
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

    return 1;
}

void previous_line(void)
{
    if (current_y > 0)
	current_y--;
}

int do_cursorpos(void)
{
    filestruct *fileptr;
    float linepct, bytepct;
    int i, tot = 0;

    if (current == NULL || fileage == NULL)
	return 0;

    for (fileptr = fileage; fileptr != current && fileptr != NULL;
	 fileptr = fileptr->next)
	tot += strlen(fileptr->data) + 1;

    if (fileptr == NULL)
	return -1;

    i = tot + current_x;;

    for (fileptr = current->next; fileptr != NULL; fileptr = fileptr->next)
	tot += strlen(fileptr->data) + 1;

    if (totlines > 0)
	linepct = 100 * current->lineno / totlines;
    else
	linepct = 0;

    if (totsize > 0)
	bytepct = 100 * i / totsize;
    else
	bytepct = 0;

#ifdef DEBUG
    fprintf(stderr, _("do_cursorpos: linepct = %f, bytepct = %f\n"),
	    linepct, bytepct);
#endif

    statusbar(_("line %d of %d (%.0f%%), character %d of %d (%.0f%%)"),
	      current->lineno, totlines, linepct, i, totsize, bytepct);
    reset_cursor();
    return 1;
}

/* Our broken, non-shortcut list compliant help function.
   But hey, it's better than nothing, and it's dynamic! */
int do_help(void)
{
#ifndef NANO_SMALL
    char *ptr = help_text, *end;
    int i, j, row = 0, page = 1, kbinput = 0, no_more = 0;
    int no_help_flag = 0;

    blank_edit();
    curs_set(0);
    blank_statusbar();

    if (ISSET(NO_HELP)) {

	no_help_flag = 1;
	delwin(bottomwin);
	bottomwin = newwin(3, COLS, LINES - 3, 0);
	keypad(bottomwin, TRUE);

	editwinrows -= no_help();
	UNSET(NO_HELP);
	bottombars(help_list, HELP_LIST_LEN);
    } else
	bottombars(help_list, HELP_LIST_LEN);

    do {
	ptr = help_text;
	switch (kbinput) {
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

	/* Calculate where in the text we should be based on the page */
	for (i = 1; i < page; i++) {
	    row = 0;
	    j = 0;
	    while (row < editwinrows && *ptr != '\0') {
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
	    end = ptr;
	    while (*end != '\n' && *end != '\0' && j != COLS - 5) {
		end++;
		j++;
	    }
	    if (j == COLS - 5) {

		/* Don't print half a word if we've run of of space */
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
    } while ((kbinput = wgetch(edit)) != NANO_EXIT_KEY);

    if (no_help_flag) {
	werase(bottomwin);
	wrefresh(bottomwin);
	delwin(bottomwin);
	SET(NO_HELP);
	bottomwin = newwin(3 - no_help(), COLS, LINES - 3 + no_help(), 0);
	keypad(bottomwin, TRUE);
	editwinrows += no_help();
    } else
	display_main_list();

    curs_set(1);
    edit_refresh();
#else
    nano_small_msg();
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
	fprintf(stderr, "(%ld) %s\n", fileptr->lineno, fileptr->data);
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
	fprintf(stderr, "(%ld) %s\n", fileptr->lineno, fileptr->data);
	fflush(stderr);
	fileptr = fileptr->prev;
    }
#endif				/* DEBUG */
}
