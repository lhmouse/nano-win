/**************************************************************************
 *   nano.c                                                               *
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <limits.h>
#include <assert.h>

#include "config.h"
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

/* Former globals, now static */
char *last_search;		/* Last string we searched for */
char *last_replace;		/* Last replacement string */
int temp_opt = 0;		/* Editing temp file (pico -t option) */
int fill = 0;			/* Fill - where to wrap lines, basically */
static char *alt_speller;	/* Alternative spell command */
struct termios oldterm;		/* The user's original term settings */
static char *help_text_init = "";
				/* Initial message, not including shortcuts */

/* What we do when we're all set to exit */
RETSIGTYPE finish(int sigage)
{
    if (!ISSET(NO_HELP)) {
	mvwaddstr(bottomwin, 1, 0, hblank);
	mvwaddstr(bottomwin, 2, 0, hblank);
    } else
	mvwaddstr(bottomwin, 0, 0, hblank);

    wrefresh(bottomwin);
    endwin();

    /* Restore the old term settings */
    tcsetattr(0, TCSANOW, &oldterm);

    exit(sigage);
}

/* Die (gracefully?) */
void die(char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    /* if we can't save we have REAL bad problems,
     * but we might as well TRY.  FIXME: This should probabally base it
     * off of the current filename */
    write_file("nano.save", 0);

    /* Restore the old term settings */
    tcsetattr(0, TCSANOW, &oldterm);

    clear();
    refresh();
    resetty();
    endwin();

    fprintf(stderr, msg);
    fprintf(stderr, _("\nBuffer written to 'nano.save'\n"));

    exit(1);			/* We have a problem: exit w/ errorlevel(1) */
}

void print_view_warning(void)
{
    statusbar(_("Key illegal in VIEW mode"));
}

/* Initialize global variables - no better way for now */
void global_init(void)
{
    int i;

    center_x = COLS / 2;
    center_y = LINES / 2;
    current_x = 0;
    current_y = 0;
    editwinrows = LINES - 5 + no_help();
    fileage = NULL;
    cutbuffer = NULL;
    current = NULL;
    edittop = NULL;
    editbot = NULL;
    totlines = 0;
    placewewant = 0;
    if (!fill)
	fill = COLS - 8;
    hblank = nmalloc(COLS + 1);

    /* Thanks BG for this bit... */
    for (i = 0; i <= COLS - 1; i++)
	hblank[i] = ' ';
    hblank[i] = 0;
    last_search = nmalloc(132);
    last_replace = nmalloc(132);
    answer = nmalloc(132);

}

void init_help_msg(void)
{

#ifndef NANO_SMALL

    help_text_init =
	_(" nano help text\n\n "
	  "The nano editor is designed to emulate the functionality and "
	  "ease-of-use of the UW Pico text editor.  There are four main "
	  "sections of the editor: The top line shows the program "
	  "version, the current filename being edited, and whether "
	  "or not the file has been modified.  Next is the main editor "
	  "window showing the file being edited.  The status line is "
	  "the third line from the bottom and shows important messages. "
	  "The bottom two lines show the most commonly used shortcuts "
	  "in the editor.\n\n "
	  "The notation for shortcuts is as follows: Control-key "
	  "sequences are notated with a caret (^) symbol.  Alt-key "
	  "sequences are notated with an at (@) symbol.  The following "
	  "keystrokes are available in the main editor window. "
	  "Optional keys are shown in parentheses:\n\n");
#endif

}

/* Make a copy of a node to a pointer (space will be malloc()ed) */
filestruct *copy_node(filestruct * src)
{
    filestruct *dst;

    dst = nmalloc(sizeof(filestruct));
    dst->data = nmalloc(strlen(src->data) + 1);

    dst->next = src->next;
    dst->prev = src->prev;

    strcpy(dst->data, src->data);
    dst->lineno = src->lineno;

    return dst;
}

/* Unlink a node from the rest of the struct */
void unlink_node(filestruct * fileptr)
{
    if (fileptr->prev != NULL)
	fileptr->prev->next = fileptr->next;

    if (fileptr->next != NULL)
	fileptr->next->prev = fileptr->prev;
}

void delete_node(filestruct * fileptr)
{
    if (fileptr->data != NULL)
	free(fileptr->data);
    free(fileptr);
}

/* Okay, now let's duplicate a whole struct! */
filestruct *copy_filestruct(filestruct * src)
{
    filestruct *dst, *tmp, *head, *prev;

    head = copy_node(src);
    dst = head;			/* Else we barf on copying just one line */
    head->prev = NULL;
    tmp = src->next;
    prev = head;

    while (tmp != NULL) {
	dst = copy_node(tmp);
	dst->prev = prev;
	prev->next = dst;

	prev = dst;
	tmp = tmp->next;
    }

    dst->next = NULL;
    return head;
}

/* Free() a single node */
int free_node(filestruct * src)
{
    if (src == NULL)
	return 0;

    if (src->next != NULL)
	free(src->data);
    free(src);
    return 1;
}

int free_filestruct(filestruct * src)
{
    filestruct *fileptr = src;

    if (src == NULL)
	return 0;

    while (fileptr->next != NULL) {
	fileptr = fileptr->next;
	free_node(fileptr->prev);

#ifdef DEBUG
	fprintf(stderr, _("free_node(): free'd a node, YAY!\n"));
#endif
    }
    free_node(fileptr);
#ifdef DEBUG
    fprintf(stderr, _("free_node(): free'd last node.\n"));
#endif

    return 1;
}

int renumber_all(void)
{
    filestruct *temp;
    long i = 1;

    for (temp = fileage; temp != NULL; temp = temp->next) {
	temp->lineno = i++;
    }

    return 0;
}

int renumber(filestruct * fileptr)
{
    filestruct *temp;

    if (fileptr == NULL || fileptr->prev == NULL || fileptr == fileage) {
	renumber_all();
	return 0;
    }
    for (temp = fileptr; temp != NULL; temp = temp->next) {
	temp->lineno = temp->prev->lineno + 1;
    }

    return 0;
}

/* Fix the memory allocation for a string */
void align(char **strp)
{
    /* There was a serious bug here:  the new address was never
       stored anywhere... */

    *strp = nrealloc(*strp, strlen(*strp) + 1);
}

void usage(void)
{
#ifdef HAVE_GETOPT_LONG
    printf(_("Usage: nano [GNU long option] [option] +LINE <file>\n\n"));
    printf(_("Option		Long option		Meaning\n"));
#ifdef HAVE_TABSIZE
    printf(_
	   (" -T 		--tabsize=[num]		Set width of a tab to num\n"));
#endif
    printf
	(_
	 (" -V 		--version		Print version information and exit\n"));
    printf(_
	   (" -c 		--const			Constantly show cursor position\n"));
    printf(_
	   (" -h 		--help			Show this message\n"));
    printf(_
	   (" -i 		--autoindent		Automatically indent new lines\n"));
    printf(_
	   (" -l 		--nofollow		Don't follow symbolic links, overwrite.\n"));
#ifndef NANO_SMALL
#ifdef NCURSES_MOUSE_VERSION
    printf(_(" -m 		--mouse			Enable mouse\n"));
#endif
#endif
    printf
	(_
	 (" -r [#cols] 	--fill=[#cols]		Set fill cols to (wrap lines at) #cols\n"));
    printf(_
	   (" -p	 	--pico			Make bottom 2 lines more Pico-like\n"));
    printf(_
	   (" -s [prog] 	--speller=[prog]	Enable alternate speller\n"));
    printf(_
	   (" -t 		--tempfile		Auto save on exit, don't prompt\n"));
    printf(_
	   (" -v 		--view			View (read only) mode\n"));
    printf(_
	   (" -w 		--nowrap		Don't wrap long lines\n"));
    printf(_
	   (" -x 		--nohelp		Don't show help window\n"));
    printf(_
	   (" -z 		--suspend		Enable suspend\n"));
    printf(_
	   (" +LINE					Start at line number LINE\n"));
#else
    printf(_("Usage: nano [option] +LINE <file>\n\n"));
    printf(_("Option		Meaning\n"));
#ifdef HAVE_TABSIZE
    printf(_(" -T [num]	Set width of a tab to num\n"));
#endif
    printf(_(" -V 		Print version information and exit\n"));
    printf(_(" -c 		Constantly show cursor position\n"));
    printf(_(" -h 		Show this message\n"));
    printf(_(" -i 		Automatically indent new lines\n"));
    printf(_
	   (" -l 		Don't follow symbolic links, overwrite.\n"));
#ifndef NANO_SMALL
#ifdef NCURSES_MOUSE_VERSION
    printf(_(" -m 		Enable mouse\n"));
#endif
#endif
    printf(_
	   (" -r [#cols] 	Set fill cols to (wrap lines at) #cols\n"));
    printf(_(" -s [prog]  	Enable alternate speller\n"));
    printf(_(" -p 		Make bottom 2 lines more Pico-like\n"));
    printf(_(" -t 		Auto save on exit, don't prompt\n"));
    printf(_(" -v 		View (read only) mode\n"));
    printf(_(" -w 		Don't wrap long lines\n"));
    printf(_(" -x 		Don't show help window\n"));
    printf(_(" -z 		Enable suspend\n"));
    printf(_(" +LINE		Start at line number LINE\n"));
#endif
    exit(0);
}

void version(void)
{
    printf(_(" nano version %s by Chris Allegretta (compiled %s, %s)\n"),
	   VERSION, __TIME__, __DATE__);
    printf(_(" Email: nano@asty.org	Web: http://www.asty.org/nano\n"));
}

filestruct *make_new_node(filestruct * prevnode)
{
    filestruct *newnode;

    newnode = nmalloc(sizeof(filestruct));
    newnode->data = NULL;

    newnode->prev = prevnode;
    newnode->next = NULL;

    if (prevnode != NULL)
	newnode->lineno = prevnode->lineno + 1;

    return newnode;
}

int do_mark()
{
#ifdef NANO_SMALL
    nano_small_msg();
#else
    if (!ISSET(MARK_ISSET)) {
	statusbar(_("Mark Set"));
	SET(MARK_ISSET);
	mark_beginbuf = current;
	mark_beginx = current_x;
    } else {
	statusbar(_("Mark UNset"));
	UNSET(MARK_ISSET);
	mark_beginbuf = NULL;
	mark_beginx = 0;

	edit_refresh();
    }
#endif
    return 1;
}

int no_help(void)
{
    if ISSET
	(NO_HELP)
	    return 2;
    else
	return 0;
}

void nano_small_msg(void)
{
    statusbar("Sorry, this function not available with nano-tiny option");
}

/* The user typed a printable character; add it to the edit buffer */
void do_char(char ch)
{
    /* More dangerousness fun =) */
    current->data = nrealloc(current->data, strlen(current->data) + 2);
    memmove(&current->data[current_x + 1],
	    &current->data[current_x],
	    strlen(current->data) - current_x + 1);
    current->data[current_x] = ch;
    do_right();

    if (!ISSET(NO_WRAP) && (ch != '\t'))
	check_wrap(current, ch);
    set_modified();
    check_statblank();
    UNSET(KEEP_CUTBUFFER);
    totsize++;

}

/* Someone hits return *gasp!* */
int do_enter(filestruct * inptr)
{
    filestruct *new;
    char *tmp, *spc;
    int extra = 0;

    new = make_new_node(inptr);
    tmp = &current->data[current_x];
    current_x = 0;

    /* Do auto-indenting, like the neolithic Turbo Pascal editor */
    if (ISSET(AUTOINDENT)) {
	spc = current->data;
	if (spc) {
	    while ((*spc == ' ') || (*spc == '\t')) {
		extra++;
		spc++;
		current_x++;
	    }
	    new->data = nmalloc(strlen(tmp) + extra + 1);
	    strncpy(new->data, current->data, extra);
	    strcpy(&new->data[extra], tmp);
	}
    } else {
	new->data = nmalloc(strlen(tmp) + 1);
	strcpy(new->data, tmp);
    }
    *tmp = 0;

    new->next = inptr->next;
    new->prev = inptr;
    inptr->next = new;
    if (new->next != NULL)
	new->next->prev = new;
    else {
	filebot = new;
	editbot = new;
    }

    totsize++;
    renumber(current);
    current = new;
    align(&current->data);

    if (current_y == editwinrows - 1) {
	edit_update(current);

	/* FIXME - figure out why the hell this is needed =) */
	reset_cursor();
    } else
	current_y++;

    totlines++;
    set_modified();

    update_cursor();
    edit_refresh();
    placewewant = xplustabs();
    return 1;
}

int do_enter_void(void)
{
    return do_enter(current);
}

void do_next_word(void)
{
    filestruct *fileptr;
    int i;

    if (current == NULL)
	return;

    i = current_x;
    for (fileptr = current; fileptr != NULL; fileptr = fileptr->next) {
	if (fileptr == current) {
	    while (isalnum((int) fileptr->data[i])
		   && fileptr->data[i] != 0)
		i++;

	    if (fileptr->data[i] == 0) {
		i = 0;
		continue;
	    }
	}
	while (!isalnum((int) fileptr->data[i]) && fileptr->data[i] != 0)
	    i++;

	if (fileptr->data[i] != 0)
	    break;

	i = 0;
    }
    if (fileptr == NULL)
	current = filebot;
    else
	current = fileptr;

    current_x = i;
    placewewant = xplustabs();
    if (current->lineno >= editbot->lineno)
	edit_update(current);

}

void do_wrap(filestruct * inptr, char input_char)
{
    int i = 0;			/* Index into ->data for line. */
    int i_tabs = 0;		/* Screen position of ->data[i]. */
    int last_word_end = -1;	/* Location of end of last word found. */
    int current_word_start = -1;	/* Location of start of current word. */
    int current_word_start_t = -1;	/* Location of start of current word screen position. */
    int current_word_end = -1;	/* Location of end   of current word */
    int current_word_end_t = -1;	/* Location of end   of current word screen position. */
    int len = strlen(inptr->data);

    int down = 0;
    int right = 0;
    struct filestruct *temp = NULL;

    assert(strlenpt(inptr->data) > fill);

    for (i = 0, i_tabs = 0; i < len; i++, i_tabs++) {
	if (!isspace(inptr->data[i])) {
	    last_word_end = current_word_end;

	    current_word_start = i;
	    current_word_start_t = i_tabs;

	    while (!isspace(inptr->data[i]) && inptr->data[i]) {
		i++;
		i_tabs++;
		if (inptr->data[i] < 32)
		    i_tabs++;
	    }

	    if (inptr->data[i]) {
		current_word_end = i;
		current_word_end_t = i_tabs;
	    } else {
		current_word_end = i - 1;
		current_word_end_t = i_tabs - 1;
	    }
	}

	if (inptr->data[i] == NANO_CONTROL_I) {
	    if (i_tabs % TABSIZE != 0);
	    i_tabs += TABSIZE - (i_tabs % TABSIZE);
	}

	if (current_word_end_t > fill)
	    break;
    }

    assert(current_word_end_t > fill);

    /* There are a few (ever changing) cases of what the line could look like.
     * 1) only one word on the line before wrap point.
     *    a) one word takes up the whole line with no starting spaces.
     *         - do nothing and return.
     *    b) cursor is on word or before word at wrap point and there are spaces at beginning.
     *         - word starts new line.
     *         - keep white space on original line up to the cursor.
     *    *) cursor is after word at wrap point
     *         - either it's all white space after word, and this routine isn't called.
     *         - or we are actually in case 2 (2 words).
     * 2) Two or more words on the line before wrap point.
     *    a) cursor is at a word or space before wrap point
     *         - word at wrap point starts a new line.
     *         - white space at end of original line is cleared, unless
     *           it is all spaces between previous word and next word which appears after fill.
     *    b) cursor is at the word at the wrap point.
     *         - word at wrap point starts a new line.
     *         1. pressed a space and at first character of wrap point word.
     *            - white space on original line is kept to where cursor was.
     *         2. pressed non space (or space elsewhere).
     *            - white space at end of original line is cleared.
     *    c) cursor is past the word at the wrap point.
     *         - word at wrap point starts a new line.
     *            - white space at end of original line is cleared
     */

    temp = nmalloc(sizeof(filestruct));

    /* Category 1a: one word taking up the whole line with no beginning spaces. */
    if ((last_word_end == -1) && (!isspace(inptr->data[0]))) {
	for (i = current_word_end; i < len; i++) {
	    if (!isspace(inptr->data[i]) && i < len) {
		current_word_start = i;
		while (!isspace(inptr->data[i]) && (i < len)) {
		    i++;
		}
		last_word_end = current_word_end;
		current_word_end = i;
		break;
	    }
	}

	if (last_word_end == -1) {
	    free(temp);
	    return;
	}
	if (current_x >= last_word_end) {
	    right = (current_x - current_word_start) + 1;
	    current_x = last_word_end;
	    down = 1;
	}

	temp->data = nmalloc(strlen(&inptr->data[current_word_start]) + 1);
	strcpy(temp->data, &inptr->data[current_word_start]);
	inptr->data = nrealloc(inptr->data, last_word_end + 2);
	inptr->data[last_word_end + 1] = 0;
    } else
	/* Category 1b: one word on the line and word not taking up whole line
	   (i.e. there are spaces at the beginning of the line) */
    if (last_word_end == -1) {
	temp->data = nmalloc(strlen(&inptr->data[current_word_start]) + 1);
	strcpy(temp->data, &inptr->data[current_word_start]);

	/* Inside word, remove it from original, and move cursor to right spot. */
	if (current_x >= current_word_start) {
	    right = current_x - current_word_start;
	    current_x = 0;
	    down = 1;
	}

	inptr->data = nrealloc(inptr->data, current_x + 1);
	inptr->data[current_x] = 0;

	if (ISSET(MARK_ISSET) && (mark_beginbuf == inptr)) {
	    mark_beginbuf = temp;
	    mark_beginx = 0;
	}
    }

    /* Category 2: two or more words on the line. */
    else {

	/* Case 2a: cursor before word at wrap point. */
	if (current_x < current_word_start) {
	    temp->data =
		nmalloc(strlen(&inptr->data[current_word_start]) + 1);
	    strcpy(temp->data, &inptr->data[current_word_start]);

	    if (!isspace(input_char)) {
		i = current_word_start - 1;
		while (isspace(inptr->data[i])) {
		    i--;
		    assert(i >= 0);
		}
	    } else if (current_x <= last_word_end)
		i = last_word_end - 1;
	    else
		i = current_x;

	    inptr->data = nrealloc(inptr->data, i + 2);
	    inptr->data[i + 1] = 0;
	}


	/* Case 2b: cursor at word at wrap point. */
	else if ((current_x >= current_word_start)
		 && (current_x <= (current_word_end + 1))) {
	    temp->data =
		nmalloc(strlen(&inptr->data[current_word_start]) + 1);
	    strcpy(temp->data, &inptr->data[current_word_start]);

	    down = 1;

	    right = current_x - current_word_start;
	    i = current_word_start - 1;
	    if (isspace(input_char) && (current_x == current_word_start)) {
		current_x = current_word_start;

		inptr->data =
		    nrealloc(inptr->data, current_word_start + 1);
		inptr->data[current_word_start] = 0;
	    } else {

		while (isspace(inptr->data[i])) {
		    i--;
		    assert(i >= 0);
		}
		inptr->data = nrealloc(inptr->data, i + 2);
		inptr->data[i + 1] = 0;
	    }
	}


	/* Case 2c: cursor past word at wrap point. */
	else {
	    temp->data =
		nmalloc(strlen(&inptr->data[current_word_start]) + 1);
	    strcpy(temp->data, &inptr->data[current_word_start]);

	    down = 1;
	    right = current_x - current_word_start;

	    current_x = current_word_start;
	    i = current_word_start - 1;

	    while (isspace(inptr->data[i])) {
		i--;
		assert(i >= 0);
		inptr->data = nrealloc(inptr->data, i + 2);
		inptr->data[i + 1] = 0;
	    }
	}
    }

    /* We pre-pend wrapped part to next line. */
    if (ISSET(SAMELINEWRAP) && inptr->next) {
	/* Plus one for the space which concatenates the two lines together plus 1 for \0. */
	char *p =
	    nmalloc(strlen(temp->data) + strlen(inptr->next->data) + 2);
	int old_x = current_x, old_y = current_y;

	strcpy(p, temp->data);
	strcat(p, " ");
	strcat(p, inptr->next->data);

	free(inptr->next->data);
	inptr->next->data = p;

	free(temp->data);
	free(temp);


	/* The next line line may need to be wrapped as well. */
	current_y = old_y + 1;
	current_x = strlen(inptr->next->data);
	while (current_x >= 0) {
	    if (isspace(inptr->next->data[current_x])
		&& (current_x < fill)) break;
	    current_x--;
	}
	if (current_x >= 0)
	    check_wrap(inptr->next, ' ');

	current_x = old_x;
	current_y = old_y;
    }
    /* Else we start a new line. */
    else {
	temp->prev = inptr;
	temp->next = inptr->next;

	if (inptr->next)
	    inptr->next->prev = temp;
	inptr->next = temp;

	if (!temp->next)
	    filebot = temp;

	SET(SAMELINEWRAP);
    }


    totlines++;
    totsize++;

    renumber(inptr);
    edit_update_top(edittop);


    /* Move the cursor to the new line if appropriate. */
    if (down) {
	do_right();
    }

    /* Move the cursor to the correct spot in the line if appropriate. */
    while (right--) {
	do_right();
    }

    edit_update_top(edittop);
    reset_cursor();
    edit_refresh();
}

/* Check to see if we've just caused the line to wrap to a new line */
void check_wrap(filestruct * inptr, char ch)
{
    int len = strlenpt(inptr->data);
#ifdef DEBUG
    fprintf(stderr, _("check_wrap called with inptr->data=\"%s\"\n"),
	    inptr->data);
#endif

    if (len <= fill)
	return;
    else {
	int i = actual_x(inptr, fill);

	/* Do not wrap if there are no words on or after wrap point. */
	int char_found = 0;

	while (isspace(inptr->data[i]) && inptr->data[i])
	    i++;

	if (!inptr->data[i])
	    return;

	/* String must be at least 1 character long. */
	for (i = strlen(inptr->data) - 1; i >= 0; i--) {
	    if (isspace(inptr->data[i])) {
		if (!char_found)
		    continue;
		char_found = 2;	/* 2 for yes do wrap. */
		break;
	    } else
		char_found = 1;	/* 1 for yes found a word, but must check further. */
	}

	if (char_found == 2)
	    do_wrap(inptr, ch);
    }
}

/* Stuff we do when we abort from programs and want to clean up the
 * screen.  This doesnt do much right now.
 */
void do_early_abort(void)
{
    blank_statusbar_refresh();
}

int page_up(void)
{
    wrap_reset();
    current_x = 0;
    placewewant = 0;

    if (current == fileage)
	return 0;

    current_y = 0;
    edit_update_bot(edittop);
    update_cursor();

    UNSET(KEEP_CUTBUFFER);
    check_statblank();
    return 1;
}

void delete_buffer(filestruct * inptr)
{
    if (inptr != NULL) {
	delete_buffer(inptr->next);
	free(inptr->data);
	free(inptr);
    }
}

int do_backspace(void)
{
    filestruct *previous, *tmp;

    if (current_x != 0) {
	/* Let's get dangerous */
	memmove(&current->data[current_x - 1], &current->data[current_x],
		strlen(current->data) - current_x + 1);
#ifdef DEBUG
	fprintf(stderr, _("current->data now = \"%s\"\n"), current->data);
#endif
	align(&current->data);
	do_left();
    } else {
	if (current == fileage)
	    return 0;		/* Can't delete past top of file */

	previous = current->prev;
	current_x = strlen(previous->data);
	previous->data = nrealloc(previous->data,
				  strlen(previous->data) +
				  strlen(current->data) + 1);
	strcat(previous->data, current->data);

	tmp = current;
	unlink_node(current);
	delete_node(current);
	if (current == edittop) {
	    if (previous->next)
		current = previous->next;
	    else
		current = previous;
	    page_up();
	} else {
	    if (previous->next)
		current = previous->next;
	    else
		current = previous;
	    update_line(current, current_x);
	}

	/* Ooops, sanity check */
	if (tmp == filebot) {
	    filebot = current;
	    editbot = current;
	}

	current = previous;
	renumber(current);
	previous_line();
	totlines--;

#ifdef DEBUG
	fprintf(stderr, _("After, data = \"%s\"\n"), current->data);
#endif

    }

    totsize--;
    set_modified();
    UNSET(KEEP_CUTBUFFER);
    edit_refresh();
    return 1;
}

int do_delete(void)
{
    filestruct *foo;

    if (current_x != strlen(current->data)) {
	/* Let's get dangerous */
	memmove(&current->data[current_x], &current->data[current_x + 1],
		strlen(current->data) - current_x);

	align(&current->data);

    } else if (current->next != NULL) {
	current->data = nrealloc(current->data,
				 strlen(current->data) +
				 strlen(current->next->data) + 1);
	strcat(current->data, current->next->data);

	foo = current->next;
	if (filebot == foo) {
	    filebot = current;
	    editbot = current;
	}

	unlink_node(foo);
	delete_node(foo);
	update_line(current, current_x);

	renumber(current);
	totlines--;

    } else
	return 0;

    totsize--;
    set_modified();
    UNSET(KEEP_CUTBUFFER);
    edit_refresh();
    return 1;
}

void wrap_reset(void)
{
    UNSET(SAMELINEWRAP);
}

/* Stuff we want to do when we exit the spell program one of its many ways */
void exit_spell(char *tmpfilename, char *foo)
{
    free(foo);

    if (remove(tmpfilename) == -1)
	statusbar(_("Error deleting tempfile, ack!"));
}

/*
 * This is Chris' very ugly spell function.  Someone please make this
 * better =-)
 */
int do_oldspell(void)
{
    char *temp, *foo;
    int i;

    if ((temp = tempnam(0, "nano.")) == NULL) {
	statusbar(_("Could not create a temporary filename: %s"),
		  strerror(errno));
	return 0;
    }
    if (write_file(temp, 1) == -1)
	return 0;

    if (alt_speller) {
	foo = nmalloc(strlen(temp) + strlen(alt_speller) + 2);
	sprintf(foo, "%s %s", alt_speller, temp);
    } else {

	/* For now, we only try ispell because we're not capable of
	   handling the normal spell program (yet...) */
	foo = nmalloc(strlen(temp) + 8);
	sprintf(foo, "ispell %s", temp);
    }

    endwin();
    resetty();
    if (alt_speller) {
	if ((i = system(foo)) == -1 || i == 32512) {
	    statusbar(_("Could not invoke spell program \"%s\""),
		      alt_speller);
	    exit_spell(temp, foo);
	    return 0;
	}
    } else if ((i = system(foo)) == -1 || i == 32512) {	/* Why 32512? I dont know! */
	statusbar(_("Could not invoke \"ispell\""));
	exit_spell(temp, foo);
	return 0;
    }
    initscr();

    free_filestruct(fileage);
    global_init();
    open_file(temp, 0, 1);
    edit_update(fileage);
    set_modified();
    exit_spell(temp, foo);
    statusbar(_("Finished checking spelling"));
    return 1;
}

int do_spell(void)
{
    char *temp, *foo;
    int i;

    if ((temp = tempnam(0, "nano.")) == NULL) {
	statusbar(_("Could not create a temporary filename: %s"),
		  strerror(errno));
	return 0;
    }
    if (write_file(temp, 1) == -1)
	return 0;

    if (alt_speller) {
	foo = nmalloc(strlen(temp) + strlen(alt_speller) + 2);
	sprintf(foo, "%s %s", alt_speller, temp);
    } else {

	/* For now, we only try ispell because we're not capable of
	   handling the normal spell program (yet...) */
	foo = nmalloc(strlen(temp) + 8);
	sprintf(foo, "ispell %s", temp);
    }

    endwin();
    resetty();
    if (alt_speller) {
	if ((i = system(foo)) == -1 || i == 32512) {
	    statusbar(_("Could not invoke spell program \"%s\""),
		      alt_speller);
	    exit_spell(temp, foo);
	    return 0;
	}
    } else if ((i = system(foo)) == -1 || i == 32512) {	/* Why 32512? I dont know! */
	statusbar(_("Could not invoke \"ispell\""));
	exit_spell(temp, foo);
	return 0;
    }
    initscr();

    free_filestruct(fileage);
    global_init();
    open_file(temp, 0, 1);
    edit_update(fileage);
    set_modified();
    exit_spell(temp, foo);
    statusbar(_("Finished checking spelling"));
    return 1;
}

int do_exit(void)
{
    int i;

    if (!ISSET(MODIFIED))
	finish(0);

    if (temp_opt) {
	i = 1;
    } else {
	i =
	    do_yesno(0, 0,
		     _
		     ("Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES) ? "));
    }

#ifdef DEBUG
    dump_buffer(fileage);
#endif

    if (i == 1) {
	if (do_writeout(1) > 0)
	    finish(0);
    } else if (i == 0)
	finish(0);
    else
	statusbar(_("Cancelled"));

    display_main_list();
    return 1;
}

#ifndef NANO_SMALL
#ifdef NCURSES_MOUSE_VERSION
void do_mouse(void)
{
    MEVENT mevent;

    if (getmouse(&mevent) == ERR)
	return;

    /* If mouse not in edit window, return (add help selection later). */
    if (!wenclose(edit, mevent.y, mevent.x))
	return;

    /* Subtract out size of topwin.  Perhaps we need a constant somewhere? */
    mevent.y -= 2;

    /* Selecting where the cursor is sets the mark.
     * Selecting beyond the line length with the cursor at the end of the
     * line sets the mark as well. 
     */
    if ((mevent.y == current_y) &&
	((mevent.x == current_x) || (current_x == strlen(current->data)
				     && (mevent.x >
					 strlen(current->data))))) {
	if (ISSET(VIEW_MODE)) {
	    print_view_warning();
	    return;
	}
	do_mark();
    } else if (mevent.y > current_y) {
	while (mevent.y > current_y) {
	    if (current->next != NULL)
		current = current->next;
	    else
		break;
	    current_y++;
	}
    } else if (mevent.y < current_y) {
	while (mevent.y < current_y) {
	    if (current->prev != NULL)
		current = current->prev;
	    else
		break;
	    current_y--;
	}
    }
    current_x = mevent.x;
    if (current_x > strlen(current->data))
	current_x = strlen(current->data);

    update_cursor();
    edit_refresh();

}
#endif
#endif

/* Handler for SIGHUP */
RETSIGTYPE handle_hup(int signal)
{
    write_file("nano.save", 0);
    finish(1);
}


void handle_sigwinch(int s)
{
#ifndef NANO_SMALL
    char *tty = NULL;
    int fd = 0;
    int result = 0;
    int i = 0;
    struct winsize win;

    tty = ttyname(0);
    if (!tty)
	return;
    fd = open(tty, O_RDWR);
    if (fd == -1)
	return;
    result = ioctl(fd, TIOCGWINSZ, &win);
    if (result == -1)
	return;


    COLS = win.ws_col;
    LINES = win.ws_row;

    center_x = COLS / 2;
    center_y = LINES / 2;
    editwinrows = LINES - 5 + no_help();
    fill = COLS - 8;

    free(hblank);
    hblank = nmalloc(COLS + 1);

    for (i = 0; i <= COLS - 1; i++)
	hblank[i] = ' ';
    hblank[i] = 0;

#ifdef HAVE_NCURSES_H
    resizeterm(LINES, COLS);
#ifdef HAVE_WRESIZE
    if (wresize(topwin, 2, COLS) == ERR)
	die(_("Cannot resize top win"));
    if (mvwin(topwin, 0, 0) == ERR)
	die(_("Cannot move top win"));
    if (wresize(edit, editwinrows, COLS) == ERR)
	die(_("Cannot resize edit win"));
    if (mvwin(edit, 2, 0) == ERR)
	die(_("Cannot move edit win"));
    if (wresize(bottomwin, 3 - no_help(), COLS) == ERR)
	die(_("Cannot resize bottom win"));
    if (mvwin(bottomwin, LINES - 3 + no_help(), 0) == ERR)
	die(_("Cannot move bottom win"));
#endif				/* HAVE_WRESIZE */
#endif				/* HAVE_NCURSES_H */

    editbot = edittop;

    for (i = 0; (i <= editwinrows - 1) && (editbot->next != NULL)
	 && (editbot->next != filebot); i++)
	editbot = editbot->next;

    if (current_y > editwinrows - 1) {
	edit_update(editbot);
    }
    erase();

    /* Do these b/c width may have changed... */
    refresh();
    titlebar();
    edit_refresh();
    display_main_list();
    total_refresh();
#endif
}

int do_tab(void)
{
    do_char('\t');
    return 1;
}

#ifndef NANO_SMALL
int empty_line(const char *data)
{
    while (*data) {
	if (!isspace(*data))
	    return 0;

	data++;
    }

    return 1;
}

int no_spaces(const char *data)
{
    while (*data) {
	if (isspace(*data))
	    return 0;

	data++;
    }

    return 1;
}

void justify_format(char *data)
{
    int i = 0;
    int len = strlen(data);

    /* Skip first character regardless and leading whitespace. */
    for (i = 1; i < len; i++) {
	if (!isspace(data[i]))
	    break;
    }

    i++;			/* (i) is now at least 2. */

    /* No double spaces allowed unless following a period.  Tabs -> space.  No double tabs. */
    for (; i < len; i++) {
	if (isspace(data[i]) && isspace(data[i - 1])
	    && (data[i - 2] != '.')) {
	    memmove(data + i, data + i + 1, len - i);
	    len--;
	    i--;
	}
    }
}
#endif

int do_justify(void)
{
#ifndef NANO_SMALL
    int slen = 0;		/* length of combined lines on one line. */
    int initial_y;
    filestruct *initial = NULL;

    if (empty_line(current->data)) {
	/* Justify starting at first non-empty line. */
	do {
	    if (!current->next)
		return 1;

	    current = current->next;
	    current_y++;
	}
	while (empty_line(current->data));
    } else {
	/* Search back for the beginning of the paragraph, where
	 *   Paragraph is  1)  A line with leading whitespace
	 *             or  2)  A line following an empty line.
	 */
	while (current->prev != NULL) {
	    if (isspace(current->data[0]) || !current->data[0])
		break;

	    current = current->prev;
	    current_y--;
	}

	/* First line with leading whitespace may be empty. */
	if (empty_line(current->data)) {
	    if (current->next) {
		current = current->next;
		current_y++;
	    } else
		return 1;
	}
    }
    initial = current;
    initial_y = current_y;

    set_modified();
    /* Put the whole paragraph into one big line. */
    while (current->next && !isspace(current->next->data[0])
	   && current->next->data[0]) {
	filestruct *tmpnode = current->next;
	int len = strlen(current->data);
	int len2 = strlen(current->next->data);

	/* length of both strings plus space between strings and ending \0. */
	current->data = nrealloc(current->data, len + len2 + 2);
	current->data[len++] = ' ';
	current->data[len] = '\0';

	strncat(current->data, current->next->data, len2);

	unlink_node(tmpnode);
	delete_node(tmpnode);

    }

    justify_format(current->data);

    slen = strlen(current->data);
    while ((strlenpt(current->data) > (fill))
	   && !no_spaces(current->data)) {
	int i = 0;
	int len2 = 0;
	filestruct *tmpline = nmalloc(sizeof(filestruct));

	/* Start at fill , unless line isn't that long (but it appears at least
	 * fill long with tabs.
	 */
	if (slen > fill)
	    i = fill;
	else
	    i = slen;
	for (; i > 0; i--) {
	    if (isspace(current->data[i]) &&
		((strlenpt(current->data) - strlen(current->data + i)) <=
		 fill)) break;
	}
	if (!i)
	    break;

	current->data[i] = '\0';

	len2 = strlen(current->data + i + 1);
	tmpline->data = nmalloc(len2 + 1);

	/* Skip the white space in current. */
	memcpy(tmpline->data, current->data + i + 1, len2);
	tmpline->data[len2] = '\0';

	current->data = nrealloc(current->data, i + 1);

	tmpline->prev = current;
	tmpline->next = current->next;
	if (current->next != NULL)
	    current->next->prev = tmpline;

	current->next = tmpline;
	current = tmpline;
	slen -= i + 1;
	current_y++;
    }


    if (current->next)
	current = current->next;
    else
	filebot = current;
    current_x = 0;
    placewewant = 0;

    renumber(initial);
    totlines = filebot->lineno;

    werase(edit);

    if ((current_y < 0) || (current_y >= editwinrows - 1)
	|| (initial_y <= 0)) {
	edit_update(current);
	center_cursor();
    } else {
	int i = 0;

	editbot = edittop;
	for (i = 0; (i <= editwinrows - 1) && (editbot->next != NULL)
	     && (editbot->next != filebot); i++)
	    editbot = editbot->next;
    }


    edit_refresh();
    edit_refresh();		/* XXX FIXME XXX */
    statusbar("Justify Complete");
    return 1;
#else
    nano_small_msg();
    return 1;
#endif
}


void help_init(void)
{
    int i, sofar = 0;
    long allocsize = 1;		/* How much space we're gonna need for the help text */
    char buf[BUFSIZ];

    /* Compute the space needed for the shortcut lists - we add 15 to
       have room for the shortcut abbrev and its possible alternate keys */
    for (i = 0; i < MAIN_LIST_LEN; i++)
	if (main_list[i].help != NULL)
	    allocsize += strlen(main_list[i].help) + 15;

    allocsize += strlen(help_text_init);

    if (help_text != NULL)
	free(help_text);

    /* Allocate space for the help text */
    help_text = nmalloc(allocsize);

    /* Now add the text we want */
    strcpy(help_text, help_text_init);

    /* Now add our shortcut info */
    for (i = 0; i < MAIN_LIST_LEN; i++) {
	sofar = sprintf(buf, "^%c	", main_list[i].val + 64);

	if (main_list[i].misc1 > KEY_F0 && main_list[i].misc1 <= KEY_F(64))
	    sofar += sprintf(&buf[sofar], "(F%d)	",
			     main_list[i].misc1 - KEY_F0);
	else
	    sofar += sprintf(&buf[sofar], "	");

	if (main_list[i].altval > 0)
	    sofar += sprintf(&buf[sofar], "(@%c)	",
			     main_list[i].altval - 32);
	else
	    sofar += sprintf(&buf[sofar], "	");

	if (main_list[i].help != NULL)
	    sprintf(&buf[sofar], "%s\n", main_list[i].help);

	strcat(help_text, buf);
    }

}

int main(int argc, char *argv[])
{
    int optchr;
    int kbinput;		/* Input from keyboard */
    long startline = 0;		/* Line to try and start at */
    struct sigaction act;	/* For our lovely signals */
    int keyhandled = 0;		/* Have we handled the keystroke yet? */
    int tmpkey = 0, i;
#ifdef HAVE_TABSIZE
    int usrtabsize = 0;		/* User defined tab size */
#endif
    char *argv0;
    struct termios term;

#ifdef HAVE_GETOPT_LONG
    int option_index = 0;
    struct option long_options[] = {
	{"version", 0, 0, 'V'},
	{"const", 0, 0, 'c'},
	{"suspend", 0, 0, 'z'},
	{"nowrap", 0, 0, 'w'},
	{"nohelp", 0, 0, 'x'},
	{"help", 0, 0, 'h'},
	{"autoindent", 0, 0, 'i'},
	{"tempfile", 0, 0, 't'},
	{"speller", 1, 0, 's'},
	{"fill", 1, 0, 'r'},
	{"mouse", 0, 0, 'm'},
	{"pico", 0, 0, 'p'},
	{"nofollow", 0, 0, 'l'},
#ifdef HAVE_TABSIZE
	{"tabsize", 0, 0, 'T'},
#endif
	{0, 0, 0, 0}
    };
#endif

    /* Flag inits... */
    SET(FOLLOW_SYMLINKS);

#ifndef NANO_SMALL
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#ifdef HAVE_GETOPT_LONG
    while ((optchr = getopt_long(argc, argv, "?T:Vchilmpr:s:tvwxz",
				 long_options, &option_index)) != EOF) {
#else
    while ((optchr = getopt(argc, argv, "h?T:Vcilmpr:s:tvwxz")) != EOF) {
#endif

	switch (optchr) {
#ifdef HAVE_TABSIZE
	case 'T':
	    usrtabsize = atoi(optarg);
	    if (usrtabsize <= 0) {
		usage();	/* To stop bogus data for tab width */
		finish(1);
	    }
	    break;
#else
	case 'T':
	    usage();		/* Oops!  You dont really have that option */
	    finish(1);
#endif
	case 'V':
	    version();
	    exit(0);
	case 'c':
	    SET(CONSTUPDATE);
	    break;
	case 'h':
	case '?':
	    usage();
	    exit(0);
	case 'i':
	    SET(AUTOINDENT);
	    break;
	case 'l':
	    UNSET(FOLLOW_SYMLINKS);
	    break;
	case 'm':
	    SET(USE_MOUSE);
	    break;
	case 'p':
	    SET(PICO_MSGS);
	    break;
	case 'r':
	    fill = atoi(optarg);
	    if (fill <= 0) {
		usage();	/* To stop bogus data (like a string) */
		finish(1);
	    }
	    break;
	case 's':
	    alt_speller = nmalloc(strlen(optarg) + 1);
	    strcpy(alt_speller, optarg);
	    break;
	case 't':
	    temp_opt = 1;
	    break;
	case 'v':
	    SET(VIEW_MODE);
	    break;
	case 'w':
	    SET(NO_WRAP);
	    break;
	case 'x':
	    SET(NO_HELP);
	    break;
	case 'z':
	    SET(SUSPEND);
	    break;
	default:
	    usage();
	    exit(0);
	}

    }

    argv0 = strrchr(argv[0], '/');
    if ((argv0 && strstr(argv0, "pico"))
	|| (!argv0 && strstr(argv[0], "pico")))
	SET(PICO_MSGS);

    filename = nmalloc(PATH_MAX);
    strcpy(filename, "");

    /* See if there's a non-option in argv (first non-option is the
       filename, if +LINE is not given) */
    if (argc == 1 || argc <= optind)
	strcpy(filename, "");
    else {
	/* Look for the +line flag... */
	if (argv[optind][0] == '+') {
	    startline = atoi(&argv[optind][1]);
	    optind++;
	    if (argc == 1 || argc <= optind)
		strcpy(filename, "");
	    else
		strncpy(filename, argv[optind], 132);
	} else
	    strncpy(filename, argv[optind], 132);

    }


    /* First back up the old settings so they can be restored, duh */
    tcgetattr(0, &oldterm);

    /* Adam's code to blow away intr character so ^C can show cursor pos */
    tcgetattr(0, &term);
    for (i = 0; i < NCCS; i++) {
	if (term.c_cc[i] == CINTR || term.c_cc[i] == CQUIT)
	    term.c_cc[i] = 0;
    }
    tcsetattr(0, TCSANOW, &term);

    /* now ncurses init stuff... */
    initscr();
    savetty();
    nonl();
    cbreak();
    noecho();
    timeout(0);

    /* Set up some global variables */
    global_init();
    shortcut_init();
    init_help_msg();
    help_init();

    /* Trap SIGINT and SIGQUIT  cuz we want them to do useful things. */
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    if (!ISSET(SUSPEND))
	sigaction(SIGTSTP, &act, NULL);

    /* Trap SIGHUP  cuz we want to write the file out. */
    act.sa_handler = handle_hup;
    sigaction(SIGHUP, &act, NULL);

    act.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &act, NULL);

#ifdef DEBUG
    fprintf(stderr, _("Main: set up windows\n"));
#endif

    /* Setup up the main text window */
    edit = newwin(editwinrows, COLS, 2, 0);
    keypad(edit, TRUE);

#ifndef NANO_SMALL
#ifdef NCURSES_MOUSE_VERSION
    if (ISSET(USE_MOUSE)) {
	mousemask(BUTTON1_RELEASED, NULL);
	mouseinterval(50);
    }
#endif
#endif

    /* And the other windows */
    topwin = newwin(2, COLS, 0, 0);
    bottomwin = newwin(3 - no_help(), COLS, LINES - 3 + no_help(), 0);
    keypad(bottomwin, TRUE);

#ifdef DEBUG
    fprintf(stderr, _("Main: bottom win\n"));
#endif
    /* Set up up bottom of window */
    display_main_list();

#ifdef DEBUG
    fprintf(stderr, _("Main: open file\n"));
#endif

    titlebar();
    if (argc == 1)
	new_file();
    else
	open_file(filename, 0, 0);

    if (startline > 0)
	do_gotoline(startline);
    else
	edit_update(fileage);

    edit_refresh();
    reset_cursor();

#ifdef HAVE_TABSIZE
    if (usrtabsize > 0)
	TABSIZE = usrtabsize;
#endif

    while (1) {
	kbinput = wgetch(edit);
	if (kbinput == 27) {	/* Grab Alt-key stuff first */
	    switch (kbinput = wgetch(edit)) {
	    case 91:

		switch (kbinput = wgetch(edit)) {
		case 'A':
		    kbinput = KEY_UP;
		    break;
		case 'B':
		    kbinput = KEY_DOWN;
		    break;
		case 'C':
		    kbinput = KEY_RIGHT;
		    break;
		case 'D':
		    kbinput = KEY_LEFT;
		    break;
		case 'H':
		    kbinput = KEY_HOME;
		    break;
		case 'F':
		    kbinput = KEY_END;
		    break;
		case 49:	/* X window F-keys */
		    tmpkey = wgetch(edit);
		    kbinput = KEY_F(tmpkey) - 48;
		    wgetch(edit);	/* Junk character */
		    break;
		case 53:	/* page up */
		    kbinput = KEY_PPAGE;
		    if ((kbinput = wgetch(edit)) == 126)
			kbinput = KEY_PPAGE;	/* Ignore extra tilde */
		    else {	/* I guess this could happen ;-) */
			ungetch(kbinput);
			continue;
		    }
		    break;
		case 54:	/* page down */
		    kbinput = KEY_NPAGE;
		    if ((kbinput = wgetch(edit)) == 126)
			kbinput = KEY_NPAGE;	/* Same thing here */
		    else {
			ungetch(kbinput);
			continue;
		    }
		    break;

		default:
#ifdef DEBUG
		    fprintf(stderr, _("I got Alt-[-%c! (%d)\n"),
			    kbinput, kbinput);
#endif
		    break;
		}
		break;
	    default:

		/* Check for the altkey defs.... */
		for (i = 0; i <= MAIN_LIST_LEN - 1; i++)
		    if (kbinput == main_list[i].altval ||
			kbinput == main_list[i].altval - 32) {
			kbinput = main_list[i].val;
			break;
		    }
#ifdef DEBUG
		fprintf(stderr, _("I got Alt-%c! (%d)\n"), kbinput,
			kbinput);
#endif
		break;
	    }
	}
	/* Look through the main shortcut list to see if we've hit a
	   shortcut key */
	for (i = 0; i < MAIN_LIST_LEN; i++) {
	    if (kbinput == main_list[i].val ||
		(main_list[i].misc1 && kbinput == main_list[i].misc1) ||
		(main_list[i].misc2 && kbinput == main_list[i].misc2)) {
		if (ISSET(VIEW_MODE) && !main_list[i].viewok)
		    print_view_warning();
		else
		    main_list[i].func();
		keyhandled = 1;
	    }
	}
	/* Last gasp, stuff that's not in the main lists */
	if (!keyhandled)
	    switch (kbinput) {
#ifndef NANO_SMALL
#ifdef NCURSES_MOUSE_VERSION
	    case KEY_MOUSE:
		do_mouse();
		break;
#endif
#endif
	    case 0:		/* Erg */
		do_next_word();
		break;
	    case 331:		/* Stuff that we don't want to do squat */
	    case -1:
	    case 410:		/* Must ignore this, it gets sent when we resize */
		break;
	    default:
#ifdef DEBUG
		fprintf(stderr, "I got %c (%d)!\n", kbinput, kbinput);
#endif
		/* We no longer stop unhandled sequences so that people with
		   odd character sets can type... */

		if (ISSET(VIEW_MODE)) {
		    print_view_warning();
		    break;
		}
		do_char(kbinput);
	    }
	if (ISSET(CONSTUPDATE))
	    do_cursorpos();

	reset_cursor();
	wrefresh(edit);
	keyhandled = 0;
    }

    getchar();
    finish(0);

}
