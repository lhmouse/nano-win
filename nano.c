/* $Id$ */
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
#include <setjmp.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
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
int fill = 0;			/* Fill - where to wrap lines, basically */

#ifndef DISABLE_SPELLER
static char *alt_speller;	/* Alternative spell command */
#endif

struct termios oldterm;		/* The user's original term settings */
static struct sigaction act;	/* For all out fun signal handlers */

#ifndef DISABLE_HELP
static char *help_text_init = ""; /* Initial message, not including shortcuts */
#endif

char *last_search = NULL;	/* Last string we searched for */
char *last_replace = NULL;	/* Last replacement string */
int search_last_line;		/* Is this the last search line? */

static sigjmp_buf jmpbuf;	/* Used to return to mainloop after SIGWINCH */

/* What we do when we're all set to exit */
RETSIGTYPE finish(int sigage)
{

    keypad(edit, TRUE);
    keypad(bottomwin, TRUE);

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
    char *name;
    int i;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    /* if we can't save we have REAL bad problems,
     * but we might as well TRY. */
    if (filename[0] == '\0') {
	name = "nano.save";
	i = write_file(name, 1);
    } else {
	
	char *buf = nmalloc(strlen(filename) + 6);
	strcpy(buf, filename);
	strcat(buf, ".save");
	i = write_file(buf, 1);
	name = buf;
    }
    /* Restore the old term settings */
    tcsetattr(0, TCSANOW, &oldterm);

    clear();
    refresh();
    resetty();
    endwin();

    fprintf(stderr, msg);
    if (i != -1)
	fprintf(stderr, _("\nBuffer written to %s\n"), name);
    else
	fprintf(stderr, _("\nNo %s written (file exists?)\n"), name);

    exit(1);			/* We have a problem: exit w/ errorlevel(1) */
}

/* Die with an error message that the screen was too small if, well, the
   screen is too small */
void die_too_small(void)
{
    char *too_small_msg = _("Window size is too small for Nano...");

    die(too_small_msg);

}

void print_view_warning(void)
{
    statusbar(_("Key illegal in VIEW mode"));
}

void clear_filename(void)
{
    if (filename != NULL)
	free(filename);
    filename = nmalloc(1);
    filename[0] = 0;
}

/* Initialize global variables - no better way for now */
void global_init(void)
{
    current_x = 0;
    current_y = 0;

    if ((editwinrows = LINES - 5 + no_help()) < MIN_EDITOR_ROWS)
	die_too_small();

    fileage = NULL;
    cutbuffer = NULL;
    current = NULL;
    edittop = NULL;
    editbot = NULL;
    totlines = 0;
    placewewant = 0;

    if (!fill)
	fill = COLS - CHARS_FROM_EOL;

    if (fill < MIN_FILL_LENGTH)
	die_too_small();

    hblank = nmalloc(COLS + 1);
    memset(hblank, ' ', COLS);
    hblank[COLS] = 0;
}

#ifndef DISABLE_HELP
void init_help_msg(void)
{

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
	  "sequences are notated with a caret (^) symbol and are entered "
	  "with the Control (Ctrl) key.  Escape-key sequences are notated "
	  "with the Meta (M) symbol and can be entered using either the "
	  "Esc, Alt or Meta key depending on your keyboard setup.  The "
	  "following keystrokes are available in the main editor window. "
	  "Optional keys are shown in parentheses:\n\n");

}
#endif

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
    if (fileptr == NULL)
	return;

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

int free_filestruct(filestruct * src)
{
    filestruct *fileptr = src;

    if (src == NULL)
	return 0;

    while (fileptr->next != NULL) {
	fileptr = fileptr->next;
	delete_node(fileptr->prev);

#ifdef DEBUG
	fprintf(stderr, _("delete_node(): free'd a node, YAY!\n"));
#endif
    }
    delete_node(fileptr);
#ifdef DEBUG
    fprintf(stderr, _("delete_node(): free'd last node.\n"));
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
	if (temp->prev != NULL)
	    temp->lineno = temp->prev->lineno + 1;
	else
	    temp->lineno = 1;
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

/* Null a string at a certain index and align it */
void null_at(char *data, int index)
{
    data[index] = 0;
    align(&data);
}

void usage(void)
{
#ifdef HAVE_GETOPT_LONG
    printf(_("Usage: nano [GNU long option] [option] +LINE <file>\n\n"));
    printf(_("Option		Long option		Meaning\n"));
    printf(_
	   (" -T [num]	--tabsize=[num]		Set width of a tab to num\n"));
#ifdef HAVE_REGEX_H
    printf(_
	   (" -R		--regexp		Use regular expressions for search\n"));
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
#ifndef NANO_SMALL
    printf(_
	   (" -k 		--cut			Let ^K cut from cursor to end of line\n"));
#endif
    printf(_
	   (" -l 		--nofollow		Don't follow symbolic links, overwrite\n"));
#ifndef NANO_SMALL
#ifdef NCURSES_MOUSE_VERSION
    printf(_(" -m 		--mouse			Enable mouse\n"));
#endif
#endif
    printf(_
	   (" -p	 	--pico			Emulate Pico as closely as possible\n"));
    printf
	(_
	 (" -r [#cols] 	--fill=[#cols]		Set fill cols to (wrap lines at) #cols\n"));
#ifndef DISABLE_SPELLER
    printf(_
	   (" -s [prog] 	--speller=[prog]	Enable alternate speller\n"));
#endif
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
    printf(_(" -T [num]	Set width of a tab to num\n"));
    printf(_(" -R		Use regular expressions for search\n"));
    printf(_(" -V 		Print version information and exit\n"));
    printf(_(" -c 		Constantly show cursor position\n"));
    printf(_(" -h 		Show this message\n"));
    printf(_(" -i 		Automatically indent new lines\n"));
#ifndef NANO_SMALL
    printf(_(" -k 		Let ^K cut from cursor to end of line\n"));
#endif
    printf(_
	   (" -l 		Don't follow symbolic links, overwrite\n"));
#ifndef NANO_SMALL
#ifdef NCURSES_MOUSE_VERSION
    printf(_(" -m 		Enable mouse\n"));
#endif
#endif
    printf(_(" -p 		Emulate Pico as closely as possible\n"));
    printf(_(" -r [#cols] 	Set fill cols to (wrap lines at) #cols\n"));
#ifndef DISABLE_SPELLER
    printf(_(" -s [prog]  	Enable alternate speller\n"));
#endif
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
    printf(_(" GNU nano version %s (compiled %s, %s)\n"),
	   VERSION, __TIME__, __DATE__);
    printf(_
	   (" Email: nano@nano-editor.org	Web: http://www.nano-editor.org"));
    printf(_("\n Compiled options:"));

#ifdef NANO_EXTRA
    printf(" --enable-extra");
#endif

#ifdef NANO_SMALL
    printf(" --enable-tiny");
#else
 #ifdef DISABLE_BROWSER
    printf(" --disable-browser");
 #endif
 #ifdef DISABLE_TABCOMP
    printf(" --disable-tabcomp");
 #endif
 #ifdef DISABLE_JUSTIFY
    printf(" --disable-justify");
 #endif
 #ifdef DISABLE_SPELLER
    printf(" --disable-speller");
 #endif
 #ifdef DISABLE_HELP
    printf(" --disable-help");
 #endif
#endif

#ifdef USE_SLANG
    printf(" --with-slang");
#endif
    printf("\n");

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

/* Splice a node into an existing filestruct */
void splice_node(filestruct * begin, filestruct * new, filestruct * end)
{
    new->next = end;
    new->prev = begin;
    begin->next = new;
    if (end != NULL)
	end->prev = new;
}

int do_mark()
{
#ifdef NANO_SMALL
    nano_disabled_msg();
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

#if defined(DISABLE_JUSTIFY) || defined(DISABLE_SPELLER) || defined(DISABLE_HELP)
void nano_disabled_msg(void)
{
    statusbar("Sorry, support for this function has been disabled");
}
#endif

/* The user typed a printable character; add it to the edit buffer */
void do_char(char ch)
{
    /* magic-line: when a character is inserted on the current magic line,
     * it means we need a new one! */
    if (filebot == current && current->data[0] == '\0') {
	new_magicline();
	fix_editbot();
    }

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
		totsize++;
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

    if (inptr->next == NULL) {
	filebot = new;
	editbot = new;
    }
    splice_node(inptr, new, inptr->next);

    totsize++;
    renumber(current);
    current = new;
    align(&current->data);

    /* The logic here is as follows:
     *    -> If we are at the bottom of the buffer, we want to recenter
     *       (read: rebuild) the screen and forcably move the cursor.
     *    -> otherwise, we want simply to redraw the screen and update
     *       where we think the cursor is.
     */
    if (current_y == editwinrows - 1) {
	edit_update(current, CENTER);
	reset_cursor();
    } else {
	current_y++;
	edit_refresh();
	update_cursor();
    }

    totlines++;
    set_modified();

    placewewant = xplustabs();
    return 1;
}

int do_enter_void(void)
{
    return do_enter(current);
}

void do_next_word(void)
{
    filestruct *fileptr, *old;
    int i;

    if (current == NULL)
	return;

    old = current;
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
	edit_update(current, CENTER);
    else {
	/* If we've jumped lines, refresh the old line.  We can't just use
	 * current->prev here, because we may have skipped over some blank
	 * lines, in which case the previous line is the wrong one.
	 */
	if (current != old)
	    update_line(old, 0);

	update_line(current, current_x);
    }

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
	if (!isspace((int) inptr->data[i])) {
	    last_word_end = current_word_end;

	    current_word_start = i;
	    current_word_start_t = i_tabs;

	    while (!isspace((int) inptr->data[i])
		   && inptr->data[i]) {
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
	    if (i_tabs % tabsize != 0);
	    i_tabs += tabsize - (i_tabs % tabsize);
	}

	if (current_word_end_t > fill)
	    break;
    }

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
    if ((last_word_end == -1) && (!isspace((int) inptr->data[0]))) {
	for (i = current_word_end; i < len; i++) {
	    if (!isspace((int) inptr->data[i]) && i < len) {
		current_word_start = i;
		while (!isspace((int) inptr->data[i]) && (i < len)) {
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
	    if (ISSET(AUTOINDENT)) {
		int i = 0;
		while ((inptr->next->data[i] == ' ' 
	    		|| inptr->next->data[i] == '\t')) {
		    i++;
		    right++;
		}			
	    }
	    down = 1;
	}

	null_at(inptr->data, current_x);

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

	    if (!isspace((int) input_char)) {
		i = current_word_start - 1;
		while (isspace((int) inptr->data[i])) {
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
	    if (ISSET(AUTOINDENT)) {
		int i = 0;
		while ((inptr->next->data[i] == ' ' 
	    		|| inptr->next->data[i] == '\t')) {
		    i++;
		    right++;
		}			
	    }

	    i = current_word_start - 1;
	    if (isspace((int) input_char)
		&& (current_x == current_word_start)) {
		current_x = current_word_start;

		null_at(inptr->data, current_word_start);
	    } else {

		while (isspace((int) inptr->data[i])) {
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

	    while (isspace((int) inptr->data[i])) {
		i--;
		assert(i >= 0);
		inptr->data = nrealloc(inptr->data, i + 2);
		inptr->data[i + 1] = 0;
	    }
	}
    }

    /* We pre-pend wrapped part to next line. */
    if (ISSET(SAMELINEWRAP) && inptr->next) {
	int old_x = current_x, old_y = current_y;

	/* Plus one for the space which concatenates the two lines together plus 1 for \0. */
	char *p = nmalloc((strlen(temp->data) + strlen(inptr->next->data) + 2) 
			* sizeof(char));

	if (ISSET(AUTOINDENT)) {
	    int non = 0;

	     /* Grab the beginning of the next line until it's not a 
		space or tab, then null terminate it so we can strcat it
		to hell */
	    while ((inptr->next->data[non] == ' ' 
	    	|| inptr->next->data[non] == '\t')
	    	&& inptr->next->data[non] != 0)
	    	    p[non] = inptr->next->data[non++];

	    p[non] = 0;
	    strcat(p, temp->data);
	    strcat(p, " ");

	     /* Now tack on the rest of the next line after the spaces and
		tabs */
	    strcat(p, &inptr->next->data[non]);
	} else {
	    strcpy(p, temp->data);
	    strcat(p, " ");
	    strcat(p, inptr->next->data);
	}

	free(inptr->next->data);
	inptr->next->data = p;

	free(temp->data);
	free(temp);

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

	if (ISSET(AUTOINDENT)) {
	    char *spc = inptr->data;
	    char *t = NULL;
	    int extra = 0;
	    if (spc) {
	        while ((*spc == ' ') || (*spc == '\t')) {
		    extra++;
		    spc++;
		    totsize++;
		}
		t = nmalloc(strlen(temp->data) + extra + 1);
		strncpy(t, inptr->data, extra);
		strcpy(t + extra, temp->data);
		free(temp->data);
		temp->data = t;
	    }
	}
    }


    totlines++;
    /* Everything about it makes me want this line here but it causes
     * totsize to be high by one for some reason.  Sigh. (Rob) */
    /* totsize++; */
	
    renumber(inptr);
    edit_update(edittop, TOP);


    /* Move the cursor to the new line if appropriate. */
    if (down) {
	do_right();
    }

    /* Move the cursor to the correct spot in the line if appropriate. */
    while (right--) {
	do_right();
    }

    edit_update(edittop, TOP);
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

	while (isspace((int) inptr->data[i]) && inptr->data[i])
	    i++;

	if (!inptr->data[i])
	    return;

	/* String must be at least 1 character long. */
	for (i = strlen(inptr->data) - 1; i >= 0; i--) {
	    if (isspace((int) inptr->data[i])) {
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
	    page_up_center();
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

	    /* Recreate the magic line if we're deleting it AND if the
	       line we're on now is NOT blank.  if it is blank we
	       can just use IT for the magic line.   This is how Pico
	       appears to do it, in any case */
	    if (strcmp(current->data, "")) {
		new_magicline();
		fix_editbot();
	    }
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

    /* Now that we have a magic lnie again, we can check for both being
       on the line before filebot as well as at filebot */
    } else if (current->next != NULL && current->next != filebot) {
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

	/* Please see the comment in do_backspace if you don't understand
	   this test */
	if (current == filebot && strcmp(current->data, "")) {
	    new_magicline();
	    fix_editbot();
	    totsize++;
	}
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

#ifndef DISABLE_SPELLER

int do_int_spell_fix(char *word)
{
    char *prevanswer = NULL, *save_search = NULL, *save_replace = NULL;
    filestruct *begin;
    int i = 0, j = 0, beginx, beginx_top;

    /* save where we are */
    begin = current;
    beginx = current_x + 1;

    /* save the current search/replace strings */
    search_init_globals();
    save_search = mallocstrcpy(save_search, last_search);
    save_replace = mallocstrcpy(save_replace, last_replace);

    /* set search/replace strings to mis-spelt word */
    prevanswer = mallocstrcpy(prevanswer, word);
    last_search = mallocstrcpy(last_search, word);
    last_replace = mallocstrcpy(last_replace, word);

    /* start from the top of file */
    current = fileage;
    current_x = beginx_top = -1;

    search_last_line = FALSE;

    edit_update(fileage, TOP);

    /* make sure word is still mis-spelt (i.e. when multi-errors) */
    if (findnextstr(TRUE, fileage, beginx_top, prevanswer) != NULL)
    {
	do_replace_highlight(TRUE, prevanswer);

	/* allow replace word to be corrected */
	i = statusq(0, spell_list, SPELL_LIST_LEN, last_replace, 
		_("Edit a replacement"));

	do_replace_highlight(FALSE, prevanswer);

	/* start from the start of this line again */
	current = fileage;
	current_x = beginx_top;

	search_last_line = FALSE;

	j = i;
	do_replace_loop(prevanswer, fileage, &beginx_top, TRUE, &j);
    }

    /* restore the search/replace strings */
    last_search = mallocstrcpy(last_search, save_search);
    last_replace = mallocstrcpy(last_replace, save_replace);

    /* restore where we were */
    current = begin;
    current_x = beginx - 1;

    edit_update(current, CENTER);

    if (i == -1)
	return FALSE;

    return TRUE;
}

/* Integrated spell checking using 'spell' program */
int do_int_speller(char *tempfile_name)
{
    char *read_buff, *read_buff_ptr, *read_buff_word;
    long pipe_buff_size;
    int in_fd[2], tempfile_fd;
    int spell_status;
    pid_t pid_spell;
    ssize_t bytesread;

    /* Create a pipe to spell program */

    if (pipe(in_fd) == -1)
	return FALSE;

    /* A new process to run spell in */

    if ( (pid_spell = fork()) == 0) {

	/* Child continues, (i.e. future spell process) */

	close(in_fd[0]);

	/* replace the standard in with the tempfile */

	if ( (tempfile_fd = open(tempfile_name, O_RDONLY)) == -1) {

	    close(in_fd[1]);
	    exit(1);
	}

	if (dup2(tempfile_fd, STDIN_FILENO) != STDIN_FILENO) {

	    close(tempfile_fd);
	    close(in_fd[1]);
	    exit(1);
	}
	close(tempfile_fd);


	/* send spell's standard out to the pipe */

	if (dup2(in_fd[1], STDOUT_FILENO) != STDOUT_FILENO) {

	    close(in_fd[1]);
	    exit(1);
	}
	close(in_fd[1]);

	/* Start spell program, we are using the PATH here!?!? */
	execlp("spell", "spell", NULL);

	/* Should not be reached, if spell is found!!! */

	exit(1);
    }

    /* Parent continues here */

    close(in_fd[1]);

    /* Child process was not forked successfully */

    if (pid_spell < 0) {

	close(in_fd[0]);
	return FALSE;
    }

    /* Get system pipe buffer size */

    if ( (pipe_buff_size = fpathconf(in_fd[0], _PC_PIPE_BUF)) < 1) {

	close(in_fd[0]);
	return FALSE;
    }

    read_buff = nmalloc( pipe_buff_size + 1 );

    /* Process the returned spelling errors */

    while ( (bytesread = read(in_fd[0], read_buff, pipe_buff_size)) > 0) {

	read_buff[bytesread] = (char) NULL;
	read_buff_word = read_buff_ptr = read_buff;

	while (*read_buff_ptr != (char) NULL) {

	    /* Windows version may need to process additional char '\r' */

	    /* Possible problem here if last word not followed by '\n' */

	    if (*read_buff_ptr == '\n') {
		*read_buff_ptr = (char) NULL;
		if (!do_int_spell_fix(read_buff_word)) { 

		    close(in_fd[0]);
		    free(read_buff);
		    replace_abort();

		    return TRUE;
	        }
		read_buff_word = read_buff_ptr;
		read_buff_word++;
	    }

	    read_buff_ptr++;
	}
    }

    close(in_fd[0]);
    free(read_buff);
    replace_abort();

    /* Process end of spell process */

    wait(&spell_status);
    if (WIFEXITED(spell_status)) {
	if (WEXITSTATUS(spell_status) != 0)
	    return FALSE;
    }
    else
	return FALSE;

    return TRUE;
}

/* External spell checking */
int do_alt_speller(char *file_name)
{
    int alt_spell_status;
    pid_t pid_spell;
    char *ptr;
    static int arglen = 3;
    static char **spellargs = (char **) NULL;

    endwin();

    /* Set up an argument list to pass the execvp function */
    if (spellargs == NULL) {
	spellargs = nmalloc(arglen * sizeof(char *));

	spellargs[0] = strtok(alt_speller, " ");
	while ((ptr = strtok(NULL, " ")) != NULL) {
	    arglen++;
	    spellargs = nrealloc(spellargs, arglen * sizeof(char *));
	    spellargs[arglen - 3] = ptr;
	}
	spellargs[arglen - 1] = NULL;
    }
    spellargs[arglen - 2] = file_name;

    /* Start a new process for the alternate speller */
    if ( (pid_spell = fork()) == 0) {

	/* Start alternate spell program, we are using the PATH here!?!? */
	execvp(spellargs[0], spellargs);

	/* Should not be reached, if alternate speller is found!!! */

	exit(1);
    }

    /* Could not fork?? */

    if (pid_spell < 0)
	return FALSE;

    /* Wait for alternate speller to complete */

    wait(&alt_spell_status);
    if (WIFEXITED(alt_spell_status)) {
	if (WEXITSTATUS(alt_spell_status) != 0)
	    return FALSE;
    }
    else
	return FALSE;

    refresh();
    free_filestruct(fileage);
    global_init();
    open_file(file_name, 0, 1);
    edit_update(fileage, CENTER);
    display_main_list();
    set_modified();

    return TRUE;
}
#endif

int do_spell(void)
{

#ifdef DISABLE_SPELLER
    nano_disabled_msg();
    return (TRUE);
#else
    char *temp;
    int spell_res;

    if ((temp = tempnam(0, "nano.")) == NULL) {
	statusbar(_("Could not create a temporary filename: %s"),
			strerror(errno));
	return 0;
    }

    if (write_file(temp, 1) == -1) {
	statusbar(_("Spell checking failed: unable to write temp file!"));
	return 0;
    }

    if (alt_speller)
	spell_res = do_alt_speller(temp);
    else
	spell_res = do_int_speller(temp);

    remove(temp);

    if (spell_res)
	statusbar(_("Finished checking spelling"));
    else
	statusbar(_("Spell checking failed"));

    return spell_res;

#endif
}

int do_exit(void)
{
    int i;

    if (!ISSET(MODIFIED))
	finish(0);

    if (ISSET(TEMP_OPT)) {
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
	if (do_writeout(filename, 1) > 0)
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
    int foo = 0, tab_found = 0;

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
    placewewant = current_x;
    while (foo < current_x) {
	if (current->data[foo] == NANO_CONTROL_I) {
	    current_x -= tabsize - (foo % tabsize);
	    tab_found = 1;
	} else if (current->data[foo] & 0x80);
	else if (current->data[foo] < 32)
	    current_x--;
	foo++;
    }
    /* This is where tab_found comes in.  I can't figure out why,
     * but without it any line with a tab will place the cursor
     * one character behind.  Whatever, this fixes it. */
    if (tab_found == 1)
	current_x++;

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
    die(_("Received SIGHUP"));
}

/* What do we do when we catch the suspend signal */
RETSIGTYPE do_suspend(int signal)
{

    act.sa_handler = SIG_DFL;
    sigemptyset(&act.sa_mask);
    sigaction(SIGTSTP, &act, NULL);

    endwin();
    fprintf(stderr, "\n\n\n\n\nUse \"fg\" to return to nano\n");
    raise(SIGTSTP);
}

/* Restore the suspend handler when we come back into the prog */
RETSIGTYPE do_cont(int signal)
{

    act.sa_handler = do_suspend;
    sigemptyset(&act.sa_mask);
    sigaction(SIGTSTP, &act, NULL);
    initscr();
    total_refresh();
}

void handle_sigwinch(int s)
{
#ifndef NANO_SMALL
    char *tty = NULL;
    int fd = 0;
    int result = 0;
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

    if ((editwinrows = LINES - 5 + no_help()) < MIN_EDITOR_ROWS)
	die_too_small();

    if ((fill = COLS - CHARS_FROM_EOL) < MIN_FILL_LENGTH)
	die_too_small();

    hblank = nrealloc(hblank, COLS + 1);
    memset(hblank, ' ', COLS);
    hblank[COLS] = 0;

#ifdef HAVE_RESIZETERM
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
#endif				/* HAVE_RESIZETERM */

    fix_editbot();

    if (current_y > editwinrows - 1) {
	edit_update(editbot, CENTER);
    }
    erase();

    /* Do these b/c width may have changed... */
    refresh();
    titlebar(NULL);
    edit_refresh();
    display_main_list();
    blank_statusbar();
    total_refresh();

    /* Turn cursor back on for sure */
    curs_set(1);

    /* Jump back to mainloop */
    siglongjmp(jmpbuf, 1);

#endif
}

void signal_init(void)
{

    /* Trap SIGINT and SIGQUIT  cuz we want them to do useful things. */
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);

    if (!ISSET(SUSPEND)) {
	sigaction(SIGTSTP, &act, NULL);
    } else {
	act.sa_handler = do_suspend;
	sigaction(SIGTSTP, &act, NULL);

	act.sa_handler = do_cont;
	sigaction(SIGCONT, &act, NULL);
    }


    /* Trap SIGHUP  cuz we want to write the file out. */
    act.sa_handler = handle_hup;
    sigaction(SIGHUP, &act, NULL);

    act.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &act, NULL);

}

void window_init(void)
{
    if ((editwinrows = LINES - 5 + no_help()) < MIN_EDITOR_ROWS)
	die_too_small();

    /* Setup up the main text window */
    edit = newwin(editwinrows, COLS, 2, 0);

    /* And the other windows */
    topwin = newwin(2, COLS, 0, 0);
    bottomwin = newwin(3 - no_help(), COLS, LINES - 3 + no_help(), 0);

#ifdef PDCURSES
    /* Oops, I guess we need this again.
       Moved here so the keypad still works after a Meta-X, for example */
    keypad(edit, TRUE);
    keypad(bottomwin, TRUE);
#endif

}

void mouse_init(void)
{
#ifndef NANO_SMALL
#ifdef NCURSES_MOUSE_VERSION
    if (ISSET(USE_MOUSE)) {
	keypad_on(edit, 1);

	mousemask(BUTTON1_RELEASED, NULL);
	mouseinterval(50);

    } else
	mousemask(0, NULL);

#endif
#endif

}

int do_tab(void)
{
    do_char('\t');
    return 1;
}

#ifndef DISABLE_JUSTIFY
int empty_line(const char *data)
{
    while (*data) {
	if (!isspace((int) *data))
	    return 0;

	data++;
    }

    return 1;
}

int no_spaces(const char *data)
{
    while (*data) {
	if (isspace((int) *data))
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
	if (!isspace((int) data[i]))
	    break;
    }

    i++;			/* (i) is now at least 2. */

    /* No double spaces allowed unless following a period.  Tabs -> space.  No double tabs. */
    for (; i < len; i++) {
	if (isspace((int) data[i]) && isspace((int) data[i - 1])
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
#ifdef DISABLE_JUSTIFY
    nano_disabled_msg();
    return 1;
#else
    int slen = 0;		/* length of combined lines on one line. */
    int initial_y, kbinput = 0, totbak;
    filestruct *initial = NULL, *tmpjust = NULL, *cutbak, *tmptop, *tmpbot;

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
	    if (isspace((int) current->data[0]) || !current->data[0])
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
    cutbak = cutbuffer; /* Got to like cutbak ;) */
    totbak = totsize;
    cutbuffer = NULL;

    tmptop = current;
    tmpjust = copy_node(current);

    /* This is annoying because it mucks with totsize */
    add_to_cutbuffer(tmpjust);

    /* Put the whole paragraph into one big line. */
    while (current->next && !isspace((int) current->next->data[0])
	   && current->next->data[0]) {
	filestruct *tmpnode = current->next;
	int len = strlen(current->data);
	int len2 = strlen(current->next->data);

	tmpjust = NULL;
	tmpjust = copy_node(current->next);
	add_to_cutbuffer(tmpjust);

	/* Wiping out a newline */
        totsize--;

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
    totsize += slen;

    if ((strlenpt(current->data) > (fill))
	&& !no_spaces(current->data)) {
	do {
	    int i = 0;
	    int len2 = 0;
	    filestruct *tmpline = nmalloc(sizeof(filestruct));

	    /* Start at fill , unless line isn't that long (but it 
	     * appears at least fill long with tabs.
	     */
	    if (slen > fill)
		i = fill;
	    else
		i = slen;

	    for (; i > 0; i--) {
		if (isspace((int) current->data[i]) &&
		    ((strlenpt(current->data) - strlen(current->data + i))
		     <= fill))
		    break;
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
	} while ((strlenpt(current->data) > (fill))
		 && !no_spaces(current->data));
    }
    tmpbot = current;

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
	edit_update(current, CENTER);
	center_cursor();
    } else {
	fix_editbot();
    }

    edit_refresh();
    statusbar(_("Can now UnJustify!"));
    /* Change the shortcut list to display the unjustify code */
    shortcut_init(1);
    display_main_list();
    reset_cursor();

    /* Now get a keystroke and see if it's unjustify, if not unget the keytroke 
       and return */
    if ((kbinput = wgetch(edit)) != NANO_UNJUSTIFY_KEY) {
	ungetch(kbinput); 
	blank_statusbar_refresh();
    } else {
	/* Else restore the justify we just did (ungrateful user!) */
	if (tmptop->prev != NULL)
	    tmptop->prev->next = tmpbot->next;
	else
	    fileage = current;
	tmpbot->next->prev = tmptop->prev;
 	current = tmpbot->next;
	tmpbot->next = NULL;
	do_uncut_text();
	if (tmptop->prev == NULL)
	    edit_refresh();

	/* Restore totsize from befure justify */
	totsize = totbak;
	free_filestruct(tmptop);
	blank_statusbar_refresh();
    }
    shortcut_init(0);
    display_main_list();
    free_filestruct(cutbuffer);
    cutbuffer = cutbak;
    
    return 1;
#endif
}

#ifndef DISABLE_HELP
void help_init(void)
{
    int i, sofar = 0;
    long allocsize = 1;		/* How much space we're gonna need for the help text */
    char buf[BUFSIZ] = "";

    /* Compute the space needed for the shortcut lists - we add 15 to
       have room for the shortcut abbrev and its possible alternate keys */
    for (i = 0; i <= MAIN_LIST_LEN - 1; i++)
	if (main_list[i].help != NULL)
	    allocsize += strlen(main_list[i].help) + 15;

    /* And for the toggle list, we also allocate space for extra text. */
    for (i = 0; i <= TOGGLE_LEN - 1; i++)
	if (toggles[i].desc != NULL)
	    allocsize += strlen(toggles[i].desc) + 30;

    allocsize += strlen(help_text_init);

    if (help_text != NULL)
	free(help_text);

    /* Allocate space for the help text */
    help_text = nmalloc(allocsize);

    /* Now add the text we want */
    strcpy(help_text, help_text_init);

    /* Now add our shortcut info */
    for (i = 0; i <= MAIN_LIST_LEN - 1; i++) {
	sofar = snprintf(buf, BUFSIZ, "^%c	", main_list[i].val + 64);

	if (main_list[i].misc1 > KEY_F0 && main_list[i].misc1 <= KEY_F(64))
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar, "(F%d)	",
			      main_list[i].misc1 - KEY_F0);
	else
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar, "	");

	if (main_list[i].altval > 0)
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar, "(M-%c)	",
			      main_list[i].altval - 32);
	else
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar, "	");


	if (main_list[i].help != NULL)
	    snprintf(&buf[sofar], BUFSIZ - sofar, "%s", main_list[i].help);


	strcat(help_text, buf);
	strcat(help_text, "\n");
    }

    /* And the toggles... */
    for (i = 0; i <= TOGGLE_LEN - 1; i++) {
	sofar = snprintf(buf, BUFSIZ,
			 "M-%c			", toggles[i].val - 32);

	if (toggles[i].desc != NULL)
	    snprintf(&buf[sofar], BUFSIZ - sofar, _("%s enable/disable"),
		     toggles[i].desc);

	strcat(help_text, buf);
	strcat(help_text, "\n");
    }

}
#endif

void do_toggle(int which)
{
#ifdef NANO_SMALL
    nano_disabled_msg();
#else
    char *enabled = _("enabled");
    char *disabled = _("disabled");

    if (ISSET(toggles[which].flag))
	UNSET(toggles[which].flag);
    else
	SET(toggles[which].flag);

    switch (toggles[which].val) {
    case TOGGLE_PICOMODE_KEY:
	shortcut_init(0);
	display_main_list();
	break;
    case TOGGLE_SUSPEND_KEY:
	signal_init();
	break;
    case TOGGLE_MOUSE_KEY:
	mouse_init();
	break;
    case TOGGLE_NOHELP_KEY:
	wclear(bottomwin);
	wrefresh(bottomwin);
	window_init();
	fix_editbot();
	edit_refresh();
	display_main_list();
	break;
    }

    if (!ISSET(toggles[which].flag)) {
	if (toggles[which].val == TOGGLE_NOHELP_KEY ||
	    toggles[which].val == TOGGLE_WRAP_KEY)
	    statusbar("%s %s", toggles[which].desc, enabled);
	else
	    statusbar("%s %s", toggles[which].desc, disabled);
    } else {
	if (toggles[which].val == TOGGLE_NOHELP_KEY ||
	    toggles[which].val == TOGGLE_WRAP_KEY)
	    statusbar("%s %s", toggles[which].desc, disabled);
	else
	    statusbar("%s %s", toggles[which].desc, enabled);
    }
    SET(DISABLE_CURPOS);

#endif
}

/* If the NumLock key has made the keypad gone awry, print an error
   message, hopefully we can address it later. */
void print_numlock_warning(void)
{
    static int didmsg = 0;
    if (!didmsg) {
	statusbar(_("NumLock glitch detected.  Keypad will malfunction with NumLock off"));
	didmsg = 1;
    }
}

/* This function returns the correct keystroke, given the A,B,C or D
   input key.  This is a common sequence of many terms which send
   Esc-O-[A-D] or Esc-[-[A-D]. */
int ABCD(int input)
{
    switch(input)
    {                
	case 'A':
	    return(KEY_UP);
	case 'B':
	    return(KEY_DOWN);
	case 'C':
	    return(KEY_RIGHT);
	case 'D': 
	    return(KEY_LEFT);
	default:
	    return 0;
    }
}

int main(int argc, char *argv[])
{
    int optchr;
    int kbinput;		/* Input from keyboard */
    long startline = 0;		/* Line to try and start at */
    int keyhandled;		/* Have we handled the keystroke yet? */
    int i, modify_control_seq;
    char *argv0;
#ifdef _POSIX_VDISABLE
    struct termios term;
#endif

#ifdef HAVE_GETOPT_LONG
    int option_index = 0;
    struct option long_options[] = {
#ifdef HAVE_REGEX_H
	{"regexp", 0, 0, 'R'},
#endif
	{"version", 0, 0, 'V'},
	{"const", 0, 0, 'c'},
	{"suspend", 0, 0, 'z'},
	{"nowrap", 0, 0, 'w'},
	{"nohelp", 0, 0, 'x'},
	{"help", 0, 0, 'h'},
	{"view", 0, 0, 'v'},
#ifndef NANO_SMALL
	{"cut", 0, 0, 'k'},
#endif
	{"autoindent", 0, 0, 'i'},
	{"tempfile", 0, 0, 't'},
#ifndef DISABLE_SPELLER
	{"speller", 1, 0, 's'},
#endif
	{"fill", 1, 0, 'r'},
	{"mouse", 0, 0, 'm'},
	{"pico", 0, 0, 'p'},
	{"nofollow", 0, 0, 'l'},
	{"tabsize", 1, 0, 'T'},
	{0, 0, 0, 0}
    };
#endif

    /* Flag inits... */
    SET(FOLLOW_SYMLINKS);

#ifndef NANO_SMALL
#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif
#endif

#ifdef HAVE_GETOPT_LONG
    while ((optchr = getopt_long(argc, argv, "?T:RVbcefhiklmpr:s:tvwxz",
				 long_options, &option_index)) != EOF) {
#else
    while ((optchr = getopt(argc, argv, "h?T:RVbcefiklmpr:s:tvwxz")) != EOF) {
#endif

	switch (optchr) {
	case 'T':
	    tabsize = atoi(optarg);
	    if (tabsize <= 0) {
		usage();	/* To stop bogus data for tab width */
		finish(1);
	    }
	    break;
#ifdef HAVE_REGEX_H
	case 'R':
	    SET(USE_REGEXP);
	    break;
#endif
	case 'V':
	    version();
	    exit(0);
	case 'b':
	case 'e':
	case 'f':
		/* Pico compatibility flags */
		break;
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
#ifndef NANO_SMALL
	case 'k':
	    SET(CUT_TO_END);
	    break;
#endif
	case 'l':
	    UNSET(FOLLOW_SYMLINKS);
	    break;
	case 'm':
	    SET(USE_MOUSE);
	    break;
	case 'p':
	    SET(PICO_MODE);
	    break;
	case 'r':
	    fill = atoi(optarg);
	    if (fill <= 0) {
		usage();	/* To stop bogus data (like a string) */
		finish(1);
	    }
	    break;
#ifndef DISABLE_SPELLER
	case 's':
	    alt_speller = nmalloc(strlen(optarg) + 1);
	    strcpy(alt_speller, optarg);
	    break;
#endif
	case 't':
	    SET(TEMP_OPT);
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
	SET(PICO_MODE);

    /* See if there's a non-option in argv (first non-option is the
       filename, if +LINE is not given) */
    if (argc == 1 || argc <= optind)
	clear_filename();
    else {
	/* Look for the +line flag... */
	if (argv[optind][0] == '+') {
	    startline = atoi(&argv[optind][1]);
	    optind++;
	    if (argc == 1 || argc <= optind)
		clear_filename();
	    else
		filename = mallocstrcpy(filename, argv[optind]);

	} else
	    filename = mallocstrcpy(filename, argv[optind]);
    }


    /* First back up the old settings so they can be restored, duh */
    tcgetattr(0, &oldterm);

#ifdef _POSIX_VDISABLE
    term = oldterm;
    term.c_cc[VINTR] = _POSIX_VDISABLE;
    term.c_cc[VQUIT] = _POSIX_VDISABLE;
    term.c_lflag &= ~IEXTEN;
    tcsetattr(0, TCSANOW, &term);
#endif

    /* now ncurses init stuff... */
    initscr();
    savetty();
    nonl();
    cbreak();
    noecho();

    /* Set up some global variables */
    global_init();
    shortcut_init(0);
#ifndef DISABLE_HELP
    init_help_msg();
    help_init();
#endif
    signal_init();

#ifdef DEBUG
    fprintf(stderr, _("Main: set up windows\n"));
#endif

    window_init();
    mouse_init();

#ifdef DEBUG
    fprintf(stderr, _("Main: bottom win\n"));
#endif
    /* Set up up bottom of window */
    display_main_list();

#ifdef DEBUG
    fprintf(stderr, _("Main: open file\n"));
#endif

    titlebar(NULL);

    /* Now we check to see if argv[optind] is non-null to determine if
       we're dealing with a new file or not, not argc == 1... */
    if (argv[optind] == NULL)
	new_file();
    else
	open_file(filename, 0, 0);

    if (startline > 0)
	do_gotoline(startline);
    else
	edit_update(fileage, CENTER);

    /* return here after a sigwinch */
    sigsetjmp(jmpbuf,1);

    /* Fix clobber-age */
    kbinput = 0;
    keyhandled = 0;
    modify_control_seq = 0;

    edit_refresh();
    reset_cursor();

    while (1) {

#ifndef _POSIX_VDISABLE
	/* We're going to have to do it the old way, i.e. on cygwin */
	raw();
#endif

	kbinput = wgetch(edit);
#ifdef DEBUG
	fprintf(stderr, "AHA!  %c (%d)\n", kbinput, kbinput);
#endif
	if (kbinput == 27) {	/* Grab Alt-key stuff first */
	    switch (kbinput = wgetch(edit)) {
		/* Alt-O, suddenly very important ;) */
	    case 79:
		kbinput = wgetch(edit);
		if (kbinput <= 'D' && kbinput >= 'A')
		   kbinput = ABCD(kbinput);
		else if (kbinput <= 'z' && kbinput >= 'j')
		    print_numlock_warning();
		else if (kbinput <= 'S' && kbinput >= 'P')
		    kbinput = KEY_F(kbinput - 79);
#ifdef DEBUG
		else {
		    fprintf(stderr, _("I got Alt-O-%c! (%d)\n"),
			    kbinput, kbinput);
		    break;
		}
#endif
		break;
	    case 27:
		/* If we get Alt-Alt, the next keystroke should be the same as a
		   control sequence */
		modify_control_seq = 1;
		keyhandled = 1;
		break;
	    case 91:
		switch (kbinput = wgetch(edit)) {
		case '1':	/* Alt-[-1-[0-5,7-9] = F1-F8 in X at least */
		    kbinput = wgetch(edit);
		    if (kbinput >= '1' && kbinput <= '5') {
			kbinput = KEY_F(kbinput - 48);
			wgetch(edit);
		    } else if (kbinput >= '7' && kbinput <= '9') {
			kbinput = KEY_F(kbinput - 49);
			wgetch(edit);
		    } else if (kbinput == 126)
			kbinput = KEY_HOME;

#ifdef DEBUG
		    else {
			fprintf(stderr, _("I got Alt-[-1-%c! (%d)\n"),
				kbinput, kbinput);
			break;
		    }
#endif

		    break;
		case '2':	/* Alt-[-2-[0,1,3,4] = F9-F12 in many terms */
		    kbinput = wgetch(edit);
		    switch (kbinput) {
		    case '0':
			kbinput = KEY_F(9);
			wgetch(edit);
			break;
		    case '1':
			kbinput = KEY_F(10);
			wgetch(edit);
			break;
		    case '3':
			kbinput = KEY_F(11);
			wgetch(edit);
			break;
		    case '4':
			kbinput = KEY_F(12);
			wgetch(edit);
			break;
		    case 126:	/* Hack, make insert key do something 
				   usefile, like insert file */
			do_insertfile();
			keyhandled = 1;
			break;
#ifdef DEBUG
		    default:
			fprintf(stderr, _("I got Alt-[-2-%c! (%d)\n"),
				kbinput, kbinput);
			break;
#endif

		    }
		    break;
		case '3':	/* Alt-[-3 = Delete? */
		    kbinput = NANO_DELETE_KEY;
		    wgetch(edit);
		    break;
		case '4':	/* Alt-[-4 = End? */
		    kbinput = NANO_END_KEY;
		    wgetch(edit);
		    break;
		case '5':	/* Alt-[-5 = Page Up */
		    kbinput = KEY_PPAGE;
		    wgetch(edit);
		    break;
		case '6':	/* Alt-[-6 = Page Down */
		    kbinput = KEY_NPAGE;
		    wgetch(edit);
		    break;
		case '[':	/* Alt-[-[-[A-E], F1-F5 in linux console */
		    kbinput = wgetch(edit);
		    if (kbinput >= 'A' && kbinput <= 'E')
			kbinput = KEY_F(kbinput - 64);
		    break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		    kbinput = ABCD(kbinput);
		    break;
		case 'H':
		    kbinput = KEY_HOME;
		    break;
		case 'F':
		    kbinput = KEY_END;
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
#ifndef NANO_SMALL
		/* And for toggle switches */
		for (i = 0; i <= TOGGLE_LEN - 1 && !keyhandled; i++)
		    if (kbinput == toggles[i].val ||
			kbinput == toggles[i].val - 32) {
			do_toggle(i);
			keyhandled = 1;
			break;
		    }
#endif
#ifdef DEBUG
		fprintf(stderr, _("I got Alt-%c! (%d)\n"), kbinput,
			kbinput);
#endif
		break;
	    }
	}
	/* If the modify_control_seq is set, we received an Alt-Alt 
	   sequence before this, so we make this key a control sequence 
	   by subtracting 64 or 96, depending on its value. */
	if (!keyhandled && modify_control_seq) {
	    if (kbinput >= 'A' && kbinput < 'a')
		kbinput -= 64;
	    else if (kbinput >= 'a' && kbinput <= 'z')
		kbinput -= 96;

	    modify_control_seq = 0;
	}

	/* Look through the main shortcut list to see if we've hit a
	   shortcut key */
	for (i = 0; i < MAIN_LIST_LEN && !keyhandled; i++) {
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
	/* If we're in raw mode or using Alt-Alt-x, we have to catch
	   Control-S and Control-Q */
	if (kbinput == 17 || kbinput == 19)
	    keyhandled = 1;

	/* Catch ^Z by hand when triggered also */
	if (kbinput == 26) {
	    if (ISSET(SUSPEND))
		do_suspend(0);
	    keyhandled = 1;
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
#ifdef PDCURSES
	    case 541:		/* ???? */
	    case 542:		/* Control and alt in Windows *shrug* */
	    case 543:		/* Right ctrl key */
	    case 544:
	    case 545:		/* Right alt key */
#endif

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
	if (ISSET(CONSTUPDATE)) {
	    if (ISSET(DISABLE_CURPOS))
		UNSET(DISABLE_CURPOS);
	    else
		do_cursorpos();
	}

	reset_cursor();
	wrefresh(edit);
	keyhandled = 0;
    }

    getchar();
    finish(0);

}
