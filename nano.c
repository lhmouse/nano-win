/* $Id$ */
/**************************************************************************
 *   nano.c                                                               *
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

#ifndef DISABLE_WRAPJUSTIFY
/* Former globals, now static */
int fill = 0;/* Fill - where to wrap lines, basically */
int wrap_at = 0;/* Right justified fill value, allows resize */
#endif

struct termios oldterm;		/* The user's original term settings */
static struct sigaction act;	/* For all our fun signal handlers */

#ifndef DISABLE_HELP
static char *help_text_init = "";	/* Initial message, not including shortcuts */
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

    thanks_for_all_the_fish();

    exit(sigage);
}

/* Die (gracefully?) */
void die(char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    /* Restore the old term settings */
    tcsetattr(0, TCSANOW, &oldterm);

    clear();
    refresh();
    resetty();
    endwin();

    fprintf(stderr, msg);

    /* save the currently loaded file if it's been modified */
    if (ISSET(MODIFIED))
	die_save_file(filename);

#ifdef ENABLE_MULTIBUFFER
    /* then save all of the other modified loaded files, if any */
    if (open_files) {
	openfilestruct *tmp;

	tmp = open_files;

	while (open_files->prev)
	    open_files = open_files->prev;

	while (open_files->next) {

	    /* if we already saved the file above (i. e. if it was the
	       currently loaded file), don't save it again */
	    if (tmp != open_files) {
		fileage = open_files->fileage;
		/* save the file if it's been modified */
		if (open_files->file_modified)
		    die_save_file(open_files->filename);
	    }

	    open_files = open_files->next;
	}

    }
#endif

    exit(1);			/* We have a problem: exit w/ errorlevel(1) */
}

void die_save_file(char *die_filename)
{
    char *name, *ret;
    int i = -1;

    /* if we can't save we have REAL bad problems,
     * but we might as well TRY. */
    if (die_filename[0] == '\0') {
	name = "nano.save";
	ret = get_next_filename(name);
	if (strcmp(ret, ""))
	    i = write_file(ret, 1, 0, 0);
	name = ret;
    }
    else {
	char *buf = charalloc(strlen(die_filename) + 6);
	strcpy(buf, die_filename);
	strcat(buf, ".save");
	ret = get_next_filename(buf);
	if (strcmp(ret, ""))
	    i = write_file(ret, 1, 0, 0);
	name = ret;
	free(buf);
    }

    if (i != -1)
	fprintf(stderr, _("\nBuffer written to %s\n"), name);
    else
	fprintf(stderr, _("\nNo %s written (too many backup files?)\n"), name);

    free(ret);
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
    filename = charalloc(1);
    filename[0] = 0;
}

/* Initialize global variables - no better way for now.  If
   save_cutbuffer is nonzero, don't set cutbuffer to NULL. */
void global_init(int save_cutbuffer)
{
    current_x = 0;
    current_y = 0;

    if ((editwinrows = LINES - 5 + no_help()) < MIN_EDITOR_ROWS)
	die_too_small();

    fileage = NULL;
    if (!save_cutbuffer)
	cutbuffer = NULL;
    current = NULL;
    edittop = NULL;
    editbot = NULL;
    totlines = 0;
    totsize = 0;
    placewewant = 0;

#ifndef DISABLE_WRAPJUSTIFY
    if (wrap_at)
	fill = COLS + wrap_at;
    else if (!fill)
	fill = COLS - CHARS_FROM_EOL;

    if (fill < MIN_FILL_LENGTH)
	die_too_small();
#endif

    hblank = charalloc(COLS + 1);
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

/* Make a copy of a node to a pointer (space will be malloc()ed).  This
   does NOT copy the data members used only by open_files. */
filestruct *copy_node(filestruct * src)
{
    filestruct *dst;

    dst = nmalloc(sizeof(filestruct));
    dst->data = charalloc(strlen(src->data) + 1);

    dst->next = src->next;
    dst->prev = src->prev;

    strcpy(dst->data, src->data);
    dst->lineno = src->lineno;

    return dst;
}

/* Unlink a node from the rest of the filestruct. */
void unlink_node(filestruct * fileptr)
{
    if (fileptr->prev != NULL)
	fileptr->prev->next = fileptr->next;

    if (fileptr->next != NULL)
	fileptr->next->prev = fileptr->prev;
}

#ifdef ENABLE_MULTIBUFFER
/* Unlink a node from the rest of the openfilestruct. */
void unlink_opennode(openfilestruct * fileptr)
{
    if (fileptr->prev != NULL)
	fileptr->prev->next = fileptr->next;

    if (fileptr->next != NULL)
	fileptr->next->prev = fileptr->prev;
}
#endif

/* Delete a node from the filestruct. */
void delete_node(filestruct * fileptr)
{
    if (fileptr == NULL)
	return;

    if (fileptr->data != NULL)
	free(fileptr->data);
    free(fileptr);
}

#ifdef ENABLE_MULTIBUFFER
/* Delete a node from the openfilestruct. */
void delete_opennode(openfilestruct * fileptr)
{
    if (fileptr == NULL)
	return;

    if (fileptr->filename != NULL)
	free(fileptr->filename);
    if (fileptr->fileage != NULL)
	free_filestruct(fileptr->fileage);
    free(fileptr);
}
#endif

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

/* Frees a filestruct. */
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

#ifdef ENABLE_MULTIBUFFER
/* Frees an openfilestruct. */
int free_openfilestruct(openfilestruct * src)
{
    openfilestruct *fileptr = src;

    if (src == NULL)
	return 0;

    while (fileptr->next != NULL) {
	fileptr = fileptr->next;
	delete_opennode(fileptr->prev);

#ifdef DEBUG
	fprintf(stderr, _("delete_opennode(): free'd a node, YAY!\n"));
#endif
    }
    delete_opennode(fileptr);
#ifdef DEBUG
    fprintf(stderr, _("delete_opennode(): free'd last node.\n"));
#endif

    return 1;
}
#endif

int renumber_all(void)
{
    filestruct *temp;
    int i = 1;

    assert(fileage==NULL || fileage!=fileage->next);
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
    assert(fileptr==NULL || fileptr!=fileptr->next);
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
void null_at(char **data, int index)
{

    /* Ahh!  Damn dereferencing */
    (*data)[index] = 0;
    align(data);
}


/* Print one usage string to the screen, removes lots of duplicate 
   strings to translate and takes out the parts that shouldn't be 
   translatable (the flag names) */
void print1opt(char *shortflag, char *longflag, char *desc)
{
    printf(" %s\t", shortflag);
    if (strlen(shortflag) < 8)
	printf("\t");

#ifdef HAVE_GETOPT_LONG
    printf("%s\t", longflag);
    if (strlen(longflag) < 8)
	printf("\t\t");
    else if (strlen(longflag) < 16)
	printf("\t");
#endif

    printf("%s\n", desc);
}

void usage(void)
{
#ifdef HAVE_GETOPT_LONG
    printf(_("Usage: nano [GNU long option] [option] +LINE <file>\n\n"));
    printf(_("Option		Long option		Meaning\n"));
#else
    printf(_("Usage: nano [option] +LINE <file>\n\n"));
    printf(_("Option		Meaning\n"));
#endif /* HAVE_GETOPT_LONG */

#ifndef NANO_SMALL
    print1opt("-D", "--dos", _("Write file in DOS format"));
#endif
#ifdef ENABLE_MULTIBUFFER
    print1opt("-F", "--multibuffer", _("Enable multiple file buffers"));
#endif
    print1opt("-K", "--keypad", _("Use alternate keypad routines"));
#ifndef NANO_SMALL
    print1opt("-M", "--mac", _("Write file in Mac format"));
    print1opt("-N", "--noconvert", _("Don't convert files from DOS/Mac format"));
#endif
#ifndef DISABLE_JUSTIFY
    print1opt(_("-Q [str]"), _("--quotestr [str]"), _("Quoting string, default \"> \""));
#endif
#ifndef NANO_SMALL
    print1opt("-S", "--smooth", _("Smooth scrolling"));
#endif
    print1opt(_("-T [num]"), _("--tabsize=[num]"), _("Set width of a tab to num"));
    print1opt("-V", "--version", _("Print version information and exit"));
    print1opt("-c", "--const", _("Constantly show cursor position"));
    print1opt("-h", "--help", _("Show this message"));
#ifndef NANO_SMALL
    print1opt("-i", "--autoindent", _("Automatically indent new lines"));
    print1opt("-k", "--cut", _("Let ^K cut from cursor to end of line"));
#endif
    print1opt("-l", "--nofollow", _("Don't follow symbolic links, overwrite"));
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
    print1opt("-m", "--mouse", _("Enable mouse"));
#endif
#endif
#ifndef DISABLE_OPERATINGDIR
    print1opt(_("-o [dir]"), _("--operatingdir=[dir]"), _("Set operating directory"));
#endif
    print1opt("-p", "--pico", _("Emulate Pico as closely as possible"));

#ifndef DISABLE_WRAPJUSTIFY
    print1opt(_("-r [#cols]"), _("--fill=[#cols]"), _("Set fill cols to (wrap lines at) #cols"));
#endif
#ifndef DISABLE_SPELLER
    print1opt(_("-s [prog]"), _("--speller=[prog]"), _("Enable alternate speller"));
#endif
    print1opt("-t", "--tempfile", _("Auto save on exit, don't prompt"));
    print1opt("-v", "--view", _("View (read only) mode"));
#ifndef DISABLE_WRAPPING
    print1opt("-w", "--nowrap", _("Don't wrap long lines"));
#endif
    print1opt("-x", "--nohelp", _("Don't show help window"));
    print1opt("-z", "--suspend", _("Enable suspend"));
    print1opt(_("+LINE"), "", _("Start at line number LINE"));

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
#ifdef ENABLE_MULTIBUFFER
    printf(" --enable-multibuffer");
#endif
#ifdef ENABLE_NANORC
    printf(" --enable-nanorc");
#endif
#ifdef ENABLE_COLOR
    printf(" --enable-color");
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
#ifdef DISABLE_MOUSE
    printf(" --disable-mouse");
#endif
#ifdef DISABLE_OPERATINGDIR
    printf(" --disable-operatingdir");
#endif
#endif /* NANO_SMALL */

#ifdef DISABLE_WRAPPING
    printf(" --disable-wrapping");
#endif
#ifdef USE_SLANG
    printf(" --with-slang");
#endif
    printf("\n");

}

/* Create a new filestruct node. */
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

#ifdef ENABLE_MULTIBUFFER
/* Create a new openfilestruct node. */
openfilestruct *make_new_opennode(openfilestruct * prevnode)
{
    openfilestruct *newnode;

    newnode = nmalloc(sizeof(openfilestruct));
    newnode->filename = NULL;
    newnode->fileage = NULL;

    newnode->prev = prevnode;
    newnode->next = NULL;

    return newnode;
}
#endif

/* Splice a node into an existing filestruct. */
void splice_node(filestruct * begin, filestruct * newnode,
		 filestruct * end)
{
    newnode->next = end;
    newnode->prev = begin;
    begin->next = newnode;
    if (end != NULL)
	end->prev = newnode;
}

#ifdef ENABLE_MULTIBUFFER
/* Splice a node into an existing openfilestruct. */
void splice_opennode(openfilestruct * begin, openfilestruct * newnode,
		     openfilestruct * end)
{
    newnode->next = end;
    newnode->prev = begin;
    begin->next = newnode;
    if (end != NULL)
	end->prev = newnode;
}
#endif

int do_mark(void)
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

    /* note that current_x has already been incremented */
    if (current == mark_beginbuf && mark_beginx >= current_x)
	mark_beginx++;

#ifdef ENABLE_COLOR
    edit_refresh();
#endif

#ifndef DISABLE_WRAPPING
    if (!ISSET(NO_WRAP) && (ch != '\t'))
	check_wrap(current);
#endif

    set_modified();
    check_statblank();
    UNSET(KEEP_CUTBUFFER);
    totsize++;

}

/* Someone hits return *gasp!* */
int do_enter(filestruct * inptr)
{
    filestruct *newnode;
    char *tmp;
#ifndef NANO_SMALL
    char *spc;
    int extra = 0;
#endif

    newnode = make_new_node(inptr);
    tmp = &current->data[current_x];
    current_x = 0;

#ifndef NANO_SMALL
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
	    newnode->data = charalloc(strlen(tmp) + extra + 1);
	    strncpy(newnode->data, current->data, extra);
	    strcpy(&newnode->data[extra], tmp);
	}
    } else 
#endif
    {
	newnode->data = charalloc(strlen(tmp) + 1);
	strcpy(newnode->data, tmp);
    }
    *tmp = 0;

    if (inptr->next == NULL) {
	filebot = newnode;
	editbot = newnode;
    }
    splice_node(inptr, newnode, inptr->next);

    totsize++;
    renumber(current);
    current = newnode;
    align(&current->data);

    /* The logic here is as follows:
     *    -> If we are at the bottom of the buffer, we want to recenter
     *       (read: rebuild) the screen and forcibly move the cursor.
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

#ifndef NANO_SMALL
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

/* the same thing for backwards */
void do_prev_word(void)
{
    filestruct *fileptr, *old;
    int i;

    if (current == NULL)
	return;

    old = current;
    i = current_x;
    for (fileptr = current; fileptr != NULL; fileptr = fileptr->prev) {
	if (fileptr == current) {
	    while (isalnum((int) fileptr->data[i])
		   && i != 0)
		i--;

	    if (i == 0) {
		if (fileptr->prev != NULL)
		    i = strlen(fileptr->prev->data);
		else if (fileptr == fileage && filebot != NULL) {
		    current_x = 0;
		    return;
		}
		continue;
	    }
	}

	while (!isalnum((int) fileptr->data[i]) && i != 0)
	    i--;

	if (i > 0) {
	    i--;

	    while (isalnum((int) fileptr->data[i]) && i != 0)
		i--;

	    if (!isalnum((int) fileptr->data[i]))
		i++;

	    if (i != 0 || i != current_x)
		break;

	}
	if (fileptr->prev != NULL)
	    i = strlen(fileptr->prev->data);
	else if (fileptr == fileage && filebot != NULL) {
	    current_x = 0;
	    return;
	}
    }
    if (fileptr == NULL)
	current = fileage;
    else
	current = fileptr;

    current_x = i;
    placewewant = xplustabs();

    if (current->lineno <= edittop->lineno)
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
#endif /* NANO_SMALL */

#ifndef DISABLE_WRAPPING
/* We wrap the given line.  Precondition: we assume the cursor has been 
 * moved forward since the last typed character. */
void do_wrap(filestruct * inptr)
{
    int len = strlen(inptr->data);	/* length of the line we wrap */
    int i;			/* generic loop variable */
    int wrap_loc = -1;		/* index of inptr->data where we wrap */
    int word_back = -1;
#ifndef NANO_SMALL
    char *indentation = NULL;	/* indentation to prepend to the new line */
    int indent_len = 0;		/* strlen(indentation) */
#endif
    char *after_break;		/* text after the wrap point */
    int after_break_len;	/* strlen(after_break) */
    int wrapping = 0;		/* do we prepend to the next line? */
    char *wrap_line = NULL;	/* the next line, minus indentation */
    int wrap_line_len = 0;	/* strlen(wrap_line) */
    char *newline = NULL;	/* the line we create */
    int new_line_len = 0;	/* eventual length of newline */

/* There are three steps.  First, we decide where to wrap.  Then, we
 * create the new wrap line.  Finally, we clean up. */

    /* We need the following assertion, since otherwise we would wrap the
     * last word to the next line regardless. */
    assert(strlenpt(inptr->data) > fill);

/* Step 1, finding where to wrap.  We are going to replace a white-space
 * character with a new-line.  In this step, we set wrap_loc as the
 * location of this replacement.
 *
 * Where should we break the line?  We need the last "legal wrap point"
 * such that the last word before it ended at or before fill.  If there
 * is no such point, we settle for the first legal wrap point.
 *
 * A "legal wrap point" is a white-space character that is not the last
 * typed character and is not followed by white-space.
 *
 * If there is no legal wrap point or we found the last character of the
 * line, we should return without wrapping.
 *
 * Note that the initial indentation does not count as a legal wrap
 * point if we are going to auto-indent!
 *
 * Note that the code below could be optimised, by not calling strlenpt
 * so often, and by not calling isspace(inptr->data[i+1]) and then in
 * the next loop calling isspace(inptr->data[i]).  Oh well, fixing the
 * first point would entail expanding the definition of strnlenpt, which
 * I won't do since it will probably change soon.  Fixing the second
 * point would entail nested loops.
 */

    i = 0;
#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	while (inptr->data[i] == ' ' || inptr->data[i] == '\t')
	    i++;
    }
#endif
    for(; i<len; i++) {
	/* record where the last word ended */
	if (!isspace((int) inptr->data[i]))
	    word_back = i;
	/* if we have found a "legal wrap point" and the current word
	 * extends too far, then we stop */
	if (wrap_loc != -1 && strnlenpt(inptr->data,word_back) > fill)
	    break;
	/* we record the latest "legal wrap point" */
	if (i != (current_x - 1) && isspace((int) inptr->data[i]) &&
	   (i == (len - 1) || !isspace((int)inptr->data[i + 1]))) {
	    wrap_loc = i;
	}
    }
    if (wrap_loc < 0 || wrap_loc == (len - 1))
	return;


/* Step 2, making the new wrap line.  It will consist of indentation +
 * after_break + " " + wrap_line (although indentation and wrap_line are
 * conditional on flags and #defines). */

    /* after_break is the text that will be moved to the next line. */
    after_break = inptr->data + wrap_loc + 1;
    after_break_len = len - wrap_loc - 1;
    assert(after_break_len == strlen(after_break));

    /* new_line_len will later be increased by the lengths of indentation
     * and wrap_line. */
    new_line_len = after_break_len;

    /* We prepend the wrapped text to the next line, if the flag is set,
     * and there is a next line, and prepending would not make the line
     * too long. */
    if (ISSET(SAMELINEWRAP) && inptr->next) {
	wrap_line = inptr->next->data;
	wrap_line_len = strlen(wrap_line);

	/* +1 for the space between after_break and wrap_line */
	if ((new_line_len + 1 + wrap_line_len) <= fill) {
	    wrapping = 1;
	    new_line_len += (1 + wrap_line_len);
	}
    }

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	/* indentation comes from the next line if wrapping, else from
	 * this line */
	indentation = (wrapping ? wrap_line : inptr->data);
	while (indentation[indent_len] == ' ' ||
		indentation[indent_len] == '\t')
	    indent_len++;
	if (wrapping)
	    /* The wrap_line text should not duplicate indentation.  Note
	     * in this case we need not increase new_line_len. */
	    wrap_line += indent_len;
	else
	    new_line_len += indent_len;
    }
#endif

    /* Now we allocate the new line and copy into it. */
    newline = charalloc(new_line_len + 1);  /* +1 for \0 */
    *newline = '\0';

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT))
	strncpy(newline, indentation, indent_len);
#endif
    strcat(newline, after_break);
    after_break = NULL;
    /* We end the old line at wrap_loc.  Note this eats the space. */
    null_at(&inptr->data, wrap_loc);
    if (wrapping) {
	/* In this case, totsize does not change.  We ate a space in the
	 * null_at() above, but we add a space between after_break and
	 * wrap_line below. */
	strcat(newline, " ");
	strcat(newline, wrap_line);
	free(inptr->next->data);
	inptr->next->data = newline;
    } else {
	filestruct *temp = (filestruct *)nmalloc(sizeof(filestruct));
	/* In this case, the file size changes by -1 for the eaten
	 * space, +1 for the new line, and +indent_len for the new
	 * indentation. */
#ifndef NANO_SMALL
	totsize += indent_len;
#endif
	totlines++;
	temp->data = newline;
	temp->prev = inptr;
	temp->next = inptr->next;
	temp->prev->next = temp;
	/* If !temp->next, then temp is the last line of the file, so we
	 * must set filebot */
	if (temp->next)
	    temp->next->prev = temp;
	else
	    filebot = temp;
    }


/* Step 3, clean up.  Here we reposition the cursor and mark, and do some
 * other sundry things. */

    /* later wraps of this line will be prepended to the next line. */
    SET(SAMELINEWRAP);

    /* Each line knows its line number.  We recalculate these if we
     * inserted a new line. */
    if (!wrapping)
	renumber(inptr);
    edit_update(edittop, TOP);

    /* if the cursor was after the break point, we must move it */
    if (current_x > wrap_loc) {
	/* We move it right by the number of characters that come before
	 * its corresponding position in the new line.  That is,
	 * current_x - wrap_loc + indent_len.  We actually need to go one
	 * further for the new line, but remember that current_x has
	 * already been incremented. */
	int right =
#ifndef NANO_SMALL
		indent_len +
#endif
		current_x - wrap_loc;

	/* note that do_right depends on the value of current_x */
	current_x = wrap_loc;
	while (right--)
	    do_right();
    }

    /* If the mark was on this line after the wrap point, we move it down.
     * If it was on the next line and we wrapped, we must move it right.
     */
    if (mark_beginbuf == inptr && mark_beginx > wrap_loc) {
	mark_beginbuf = inptr->next;
	mark_beginx -= wrap_loc;
    } else if (wrapping && mark_beginbuf == inptr->next) {
	mark_beginx += after_break_len;
    }

/* The following lines are all copied from do_wrap() in version 1.1.7.  It
 * is not clear whether they are necessary.  It looks like do_right()
 * takes care of these things.  It also appears that do_right() is very
 * inefficient. */
    /* Perhaps the global variable editbot, the last visible line in the
     * editor, needs to change. */
    fix_editbot();
    /* Place the cursor. */
    reset_cursor();
    /* Display the changes on the screen. */
    edit_refresh();
}

/* Check to see if we've just caused the line to wrap to a new line */
void check_wrap(filestruct * inptr)
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
	    do_wrap(inptr);
    }
}
#endif				/* DISABLE_WRAPPING */

/* Stuff we do when we abort from programs and want to clean up the
 * screen.  This doesn't do much right now.
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

    /* blbf -> blank line before filebot (see below) */
    int blbf = 0;

    if (current->next == filebot && !strcmp(current->data, ""))
	blbf = 1;

    if (current_x != strlen(current->data)) {
	/* Let's get dangerous */
	memmove(&current->data[current_x], &current->data[current_x + 1],
		strlen(current->data) - current_x);

	align(&current->data);

	/* Now that we have a magic line again, we can check for both being
	   on the line before filebot as well as at filebot; it's a special
	   case if we're on the line before filebot and it's blank, since we
	   should be able to delete it */
    } else if (current->next != NULL && (current->next != filebot || blbf)) {
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
    int i = 0, j = 0, beginx, beginx_top, reverse_search_set;

    /* save where we are */
    begin = current;
    beginx = current_x + 1;

    /* Make sure Spell Check goes forward only */
    reverse_search_set = ISSET(REVERSE_SEARCH);
    UNSET(REVERSE_SEARCH);

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

    while (1) {

	/* make sure word is still mis-spelt (i.e. when multi-errors) */
	if (findnextstr(TRUE, FALSE, fileage, beginx_top, prevanswer) != NULL) {

	    /* find wholewords only */
	    if (!is_whole_word(current_x, current, prevanswer))
		continue;

	    do_replace_highlight(TRUE, prevanswer);

	    /* allow replace word to be corrected */
	    i = statusq(0, spell_list, last_replace, _("Edit a replacement"));

	    do_replace_highlight(FALSE, prevanswer);

	    /* start from the start of this line again */
	    current = fileage;
	    current_x = beginx_top;

	    search_last_line = FALSE;

	    if (strcmp(prevanswer,answer) != 0) {
		j = i;
		do_replace_loop(prevanswer, fileage, &beginx_top, TRUE, &j);
	    }
	}

	break;
    }

    /* restore the search/replace strings */
    free(last_search);    last_search=save_search;
    free(last_replace);   last_replace=save_replace;
    free(prevanswer);

    /* restore where we were */
    current = begin;
    current_x = beginx - 1;

    /* restore Search/Replace direction */
    if (reverse_search_set)
	SET(REVERSE_SEARCH);

    edit_update(current, CENTER);

    if (i == -1)
	return FALSE;

    return TRUE;
}

/* Integrated spell checking using 'spell' program */
int do_int_speller(char *tempfile_name)
{
    char *read_buff, *read_buff_ptr, *read_buff_word;
    size_t pipe_buff_size, read_buff_size, read_buff_read, bytesread;
    int in_fd[2], tempfile_fd;
    int spell_status;
    pid_t pid_spell;

    /* Create a pipe to spell program */

    if (pipe(in_fd) == -1)
	return FALSE;

    /* A new process to run spell in */

    if ((pid_spell = fork()) == 0) {

	/* Child continues, (i.e. future spell process) */

	close(in_fd[0]);

	/* replace the standard in with the tempfile */

	if ((tempfile_fd = open(tempfile_name, O_RDONLY)) == -1) {

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

    if ((pipe_buff_size = fpathconf(in_fd[0], _PC_PIPE_BUF)) < 1) {

	close(in_fd[0]);
	return FALSE;
    }

    /* Read-in the returned spelling errors */

    read_buff_read = 0;
    read_buff_size = pipe_buff_size + 1;
    read_buff = read_buff_ptr = charalloc(read_buff_size);

    while ((bytesread = read(in_fd[0], read_buff_ptr, pipe_buff_size)) > 0) {

	read_buff_read += bytesread;
	read_buff_size += pipe_buff_size;
	read_buff = read_buff_ptr = nrealloc(read_buff, read_buff_size);
	read_buff_ptr += read_buff_read;
    }

    *read_buff_ptr = (char) NULL;
    close(in_fd[0]);

    /* Process the spelling errors */

    read_buff_word = read_buff_ptr = read_buff;

    while (*read_buff_ptr) {

	if ((*read_buff_ptr == '\n') || (*read_buff_ptr == '\r')) {
	    *read_buff_ptr = (char) NULL;
	    if (read_buff_word != read_buff_ptr) {
		if (!do_int_spell_fix(read_buff_word)) {
		    read_buff_word = read_buff_ptr;
		    break;
		}
	    }

	    read_buff_word = read_buff_ptr + 1;
	}

	read_buff_ptr++;
    }

    /* special case where last word doesn't end with \n or \r */
    if (read_buff_word != read_buff_ptr)
	do_int_spell_fix(read_buff_word);

    free(read_buff);
    replace_abort();

    /* Process end of spell process */

    wait(&spell_status);
    if (WIFEXITED(spell_status)) {
	if (WEXITSTATUS(spell_status) != 0)
	    return FALSE;
    } else
	return FALSE;

    return TRUE;
}

/* External spell checking */
int do_alt_speller(char *file_name)
{
    int alt_spell_status, lineno_cur = current->lineno;
    int x_cur = current_x, y_cur = current_y, pww_cur = placewewant;

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
    if ((pid_spell = fork()) == 0) {

	/* Start alternate spell program; we are using the PATH here!?!? */
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
    } else
	return FALSE;

    refresh();
    free_filestruct(fileage);
    global_init(1);
    open_file(file_name, 0, 1);

    /* go back to the old position, mark the file as modified, and make
       sure that the titlebar is refreshed */
    do_gotopos(lineno_cur, x_cur, y_cur, pww_cur);
    set_modified();
    clearok(topwin, FALSE);
    titlebar(NULL);

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

    if ((temp = safe_tempnam(0, "nano.")) == NULL) {
	statusbar(_("Could not create a temporary filename: %s"),
		  strerror(errno));
	return 0;
    }

    if (write_file(temp, 1, 0, 0) == -1) {
	statusbar(_("Spell checking failed: unable to write temp file!"));
	free(temp);
	return 0;
    }

#ifdef ENABLE_MULTIBUFFER
    /* update the current open_files entry before spell-checking, in case
       any problems occur; the case of there being no open_files entries
       is handled elsewhere (before we reach this point) */
    add_open_file(1);
#endif

    if (alt_speller)
	spell_res = do_alt_speller(temp);
    else
	spell_res = do_int_speller(temp);

    remove(temp);

    if (spell_res)
	statusbar(_("Finished checking spelling"));
    else
	statusbar(_("Spell checking failed"));

    free(temp);
    return spell_res;

#endif
}

#ifndef NANO_SMALL
static int pid;		/* this is the PID of the newly forked process below.
			 * It must be global since the signal handler needs it.
			 */

RETSIGTYPE cancel_fork(int signal) {
    if (kill(pid, SIGKILL)==-1) nperror("kill");
}

int open_pipe(char *command)
{
    int fd[2];
    FILE *f;
    struct sigaction oldaction, newaction;
			/* original and temporary handlers for SIGINT */
#ifdef _POSIX_VDISABLE
    struct termios term, newterm;
#endif   /* _POSIX_VDISABLE */
    int cancel_sigs = 0;
    /* cancel_sigs==1 means that sigaction failed without changing the
     * signal handlers.  cancel_sigs==2 means the signal handler was
     * changed, but the tcsetattr didn't succeed.
     * I use this variable since it is important to put things back when
     * we finish, even if we get errors.
     */

  /* Make our pipes. */

    if (pipe(fd) == -1) {
	statusbar(_("Could not pipe"));
	return 1;
    }

    /* Fork a child */

    if ((pid = fork()) == 0) {
	close(fd[0]);
	dup2(fd[1], fileno(stdout));
	dup2(fd[1], fileno(stderr));
	/* If execl() returns at all, there was an error. */
      
	execl("/bin/sh","sh","-c",command,0);
	exit(0);
    }

    /* Else continue as parent */

    close(fd[1]);

    if (pid == -1) {
	close(fd[0]);
	statusbar(_("Could not fork"));
	return 1;
    }

    /* before we start reading the forked command's output, we set
     * things up so that ^C will cancel the new process.
     */
    if (sigaction(SIGINT, NULL, &newaction)==-1) {
	cancel_sigs = 1;
	nperror("sigaction");
    } else {
	newaction.sa_handler = cancel_fork;
	if (sigaction(SIGINT, &newaction, &oldaction)==-1) {
	    cancel_sigs = 1;
	    nperror("sigaction");
	}
    }
    /* note that now oldaction is the previous SIGINT signal handler, to
     * be restored later */

    /* if the platform supports disabling individual control characters */
#ifdef _POSIX_VDISABLE
    if (!cancel_sigs && tcgetattr(0, &term) == -1) {
	cancel_sigs = 2;
	nperror("tcgetattr");
    }
    if (!cancel_sigs) {
	newterm = term;
	/* Grab oldterm's VINTR key :-) */
	newterm.c_cc[VINTR] = oldterm.c_cc[VINTR];
	if (tcsetattr(0, TCSANOW, &newterm) == -1) {
	    cancel_sigs = 2;
	    nperror("tcsetattr");
	}
    }
#endif   /* _POSIX_VDISABLE */

    f = fdopen(fd[0], "rb");
    if (!f)
      nperror("fdopen");
    
    read_file(f, "stdin", 0);
    set_modified();

    if (wait(NULL) == -1)
	nperror("wait");

#ifdef _POSIX_VDISABLE
    if (!cancel_sigs && tcsetattr(0, TCSANOW, &term) == -1)
	nperror("tcsetattr");
#endif   /* _POSIX_VDISABLE */

    if (cancel_sigs!=1 && sigaction(SIGINT, &oldaction, NULL) == -1)
	nperror("sigaction");

    return 0;
}
#endif /* NANO_SMALL */

int do_exit(void)
{
    int i;

    if (!ISSET(MODIFIED)) {

#ifdef ENABLE_MULTIBUFFER
	if (!close_open_file()) {
	    display_main_list();
	    return 1;
	}
	else
#endif

	    finish(0);
    }

    if (ISSET(TEMP_OPT)) {
	i = 1;
    } else {
	i = do_yesno(0, 0,
		     _
		     ("Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES) ? "));
    }

#ifdef DEBUG
    dump_buffer(fileage);
#endif

    if (i == 1) {
	if (do_writeout(filename, 1, 0) > 0) {

#ifdef ENABLE_MULTIBUFFER
	    if (!close_open_file()) {
		display_main_list();
		return 1;
	    }
	    else
#endif

		finish(0);
	}
    } else if (i == 0) {

#ifdef ENABLE_MULTIBUFFER
	if (!close_open_file()) {
	    display_main_list();
	    return 1;
	}
	else
#endif

	    finish(0);
    } else
	statusbar(_("Cancelled"));

    display_main_list();
    return 1;
}

#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
void do_mouse(void)
{
    MEVENT mevent;
    int foo = 0, tab_found = 0;
    int currslen;
    shortcut *s = currshortcut;

    if (getmouse(&mevent) == ERR)
	return;

    /* If mouse not in edit or bottom window, return */
    if (wenclose(edit, mevent.y, mevent.x)) {

	/* Don't let people screw with the marker when they're in a
	   subfunction */
	if (currshortcut != main_list)
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
    } else if (wenclose(bottomwin, mevent.y, mevent.x) && !ISSET(NO_HELP)) {

	int i, k;

	if (currshortcut == main_list)
	    currslen = MAIN_VISIBLE;
	else
	    currslen = length_of_list(currshortcut);

	if (currslen < 2)
	    k = COLS / 6;
	else 
	    k = COLS / ((currslen + (currslen %2)) / 2);

	/* Determine what shortcut list was clicked */
	mevent.y -= (editwinrows + 3);

	if (mevent.y < 0) /* They clicked on the statusbar */
	    return;

	/* Don't select stuff beyond list length */
	if (mevent.x / k >= currslen)	
	    return;

	for (i = 0; i < (mevent.x / k) * 2 + mevent.y; i++)
	    s = s->next;

	/* And ungetch that value */
	ungetch(s->val);

	/* And if it's an alt-key sequence, we should probably send alt
	   too ;-) */
	if (s->val >= 'a' && s->val <= 'z')
	   ungetch(27);
    }
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
    endwin();
    printf("\n\n\n\n\nUse \"fg\" to return to nano\n");
    fflush(stdout);

    /* Restore the terminal settings for the disabled keys */
    tcsetattr(0, TCSANOW, &oldterm);

    /* We used to re-enable the default SIG_DFL and raise SIGTSTP, but 
	then we could be (and were) interrupted in the middle of the call.
	So we do it the mutt way instead */
    kill(0, SIGSTOP);
}

/* Restore the suspend handler when we come back into the prog */
RETSIGTYPE do_cont(int signal)
{

    /* Now we just update the screen instead of having to reenable the
	SIGTSTP handler */

    doupdate();
    /* The Hurd seems to need this, otherwise a ^Y after a ^Z will
	start suspending again */
   signal_init();
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

#ifndef DISABLE_WRAPJUSTIFY
    if ((fill = COLS - CHARS_FROM_EOL) < MIN_FILL_LENGTH)
	die_too_small();
#endif

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
#ifdef _POSIX_VDISABLE
    struct termios term;
#endif

    /* Trap SIGINT and SIGQUIT cuz we want them to do useful things. */
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);

    /* Trap SIGHUP cuz we want to write the file out. */
    act.sa_handler = handle_hup;
    sigaction(SIGHUP, &act, NULL);

    act.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &act, NULL);


#ifdef _POSIX_VDISABLE
    tcgetattr(0, &term);

#ifdef VDSUSP
    term.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif /* VDSUSP */

#endif /* _POSIX_VDISABLE */

    if (!ISSET(SUSPEND)) {

/* Insane! */
#ifdef _POSIX_VDISABLE
	term.c_cc[VSUSP] = _POSIX_VDISABLE;
#else
	act.sa_handler = SIG_IGN;
	sigaction(SIGTSTP, &act, NULL);
#endif

    } else {
	/* if we don't do this, it seems other stuff interrupts the
	   suspend handler!  Try using nano with mutt without this line */
	sigfillset(&act.sa_mask);

	act.sa_handler = do_suspend;
	sigaction(SIGTSTP, &act, NULL);

	act.sa_handler = do_cont;
	sigaction(SIGCONT, &act, NULL);
    }


#ifdef _POSIX_VDISABLE
    tcsetattr(0, TCSANOW, &term);
#endif


}

void window_init(void)
{
    if ((editwinrows = LINES - 5 + no_help()) < MIN_EDITOR_ROWS)
	die_too_small();

    /* Set up the main text window */
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
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
    if (ISSET(USE_MOUSE)) {
	keypad_on(edit, 1);
	keypad_on(bottomwin, 1);

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

    /* Skip leading whitespace. */
    for (i = 0; i < len; i++) {
	if (!isspace((int) data[i]))
	    break;
    }

    i++;			/* (i) is now at least 2. */

    /* No double spaces allowed unless following a period.  Tabs -> space.  No double tabs. */
    for (; i < len; i++) {
	if (isspace((int) data[i]) && isspace((int) data[i - 1])
	    && (data[i - 2] != '.')
            && (data[i-2]!='!') && (data[i-2]!='?')) {
	    memmove(data + i, data + i + 1, len - i);
	    len--;
	    i--;
	}
    }
   /* Skip trailing whitespace.
    * i<=len iff there was a non-space in the line.  In that case, we
    * strip spaces from the end of the line.  Note that "line" means the
    * whole paragraph. */
  if (i<=len) {
    for(i=len-1; i>0 && isspace((int) data[i]); i--);
    data[i+1] = '\0';
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
    int initial_y, kbinput = 0;
    long totbak;
    filestruct *initial = NULL, *tmpjust = NULL, *cutbak, *tmptop, *tmpbot;
    filestruct *samecheck = current;
    int qdepth = 0;

    /* Compute quote depth level */
    while (!strncmp(&current->data[qdepth], quotestr, strlen(quotestr)))
	qdepth += strlen(quotestr);

    if (empty_line(&current->data[qdepth])) {
	/* Justify starting at first non-empty line. */
	do {
	    if (!current->next)
		return 1;

	    current = current->next;
	    current_y++;
	}
	while (strlen(current->data) >= qdepth 
		&& !strncmp(current->data, samecheck->data, qdepth) 
		&& empty_line(&current->data[qdepth]));

    } else {
	/* Search back for the beginning of the paragraph, where
	 *   Paragraph is  1)  A line with leading whitespace
	 *             or  2)  A line following an empty line.
	 */
	while (current->prev != NULL) {
	    if (strncmp(current->data, samecheck->data, qdepth)

		/* Don't keep going back if the previous line is more 
			intented quotestr-wise than samecheck */
		|| !strncmp(&current->data[qdepth], quotestr, strlen(quotestr))
		|| isspace((int) current->data[qdepth]) 
		|| empty_line(&current->data[qdepth]))
		break;

	    current = current->prev;
	    current_y--;
	}

	/* First line with leading whitespace may be empty. */
	if (strncmp(current->data, samecheck->data, qdepth)
		|| !strncmp(&current->data[qdepth], quotestr, strlen(quotestr))
		|| empty_line(&current->data[qdepth])) {
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
    cutbak = cutbuffer;		/* Got to like cutbak ;) */
    totbak = totsize;
    cutbuffer = NULL;

    tmptop = current;
    tmpjust = copy_node(current);
    samecheck = tmpjust;

    /* This is annoying because it mucks with totsize */
    add_to_cutbuffer(tmpjust);

    /* Put the whole paragraph into one big line. */
    while (current->next && !isspace((int) current->next->data[0])
	   && !strncmp(current->next->data, samecheck->data, qdepth)

	   /* Don't continue if current->next is indented more! */
	   && strncmp(&current->next->data[qdepth], quotestr, strlen(quotestr))
	   && !empty_line(&current->next->data[qdepth])) {
	filestruct *tmpnode = current->next;
	int len = strlen(current->data);
	int len2 = strlen(current->next->data) - qdepth;

	tmpjust = NULL;
	tmpjust = copy_node(current->next);
	add_to_cutbuffer(tmpjust);

	/* Wiping out a newline */
	totsize--;

	/* length of both strings plus space between strings and ending \0. */
	current->data = nrealloc(current->data, len + len2 + 2);
	current->data[len++] = ' ';
	current->data[len] = '\0';

	strncat(current->data, &current->next->data[qdepth], len2);

	unlink_node(tmpnode);
	delete_node(tmpnode);
    }

    justify_format(current->data);

    slen = strlen(current->data);
    totsize += slen;

    while (strlenpt(current->data) > fill
	    && !no_spaces(current->data + qdepth)) {
	int i = 0, j = 0;
	filestruct *tmpline = nmalloc(sizeof(filestruct));

/* The following code maybe could be better.  In particular, can we 
 * merely increment instead of calling strnlenpt for each new character?  
 * In fact, can we assume the only tabs are at the beginning of the line?
 */
/* Note that we CAN break before the first word, since that is how 
 * pico does it. */
	int last_space = -1;  /* index of the last breakpoint */

	for(i=qdepth; i<slen; i++) {
	    if (isspace((int) current->data[i]))
		last_space = i;
	    /* Note we must look at the length of the first i+1 chars. */
	    if (last_space!=-1 &&
		    strnlenpt(current->data,i+1) > fill) {
		i = last_space;
		break;
	    }
	}
/* Now data[i] is a space.  We want to break at the LAST space in this
 * group.  Probably, the only possibility is two in a row, but let's be 
 * generic.  Note that we actually replace this final space with \0.  Is
 * this okay?  It seems to work fine. */
	for(; i<slen-1 && isspace((int) current->data[i+1]); i++)
	    ;

	current->data[i] = '\0';

	slen -= i + 1 - qdepth;   /* note i > qdepth */
	tmpline->data = charalloc(slen + 1);

	for (j = 0; j < qdepth; j += strlen(quotestr))
	    strcpy(&tmpline->data[j], quotestr);

	/* Skip the white space in current. */
	memcpy(&tmpline->data[qdepth], current->data + i + 1, slen-qdepth);
	tmpline->data[slen] = '\0';

	current->data = nrealloc(current->data, i + 1);

	tmpline->prev = current;
	tmpline->next = current->next;
	if (current->next != NULL)
	    current->next->prev = tmpline;

	current->next = tmpline;
	current = tmpline;
	current_y++;
    } /* end of while (!no_spaces) */
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

    /* Now get a keystroke and see if it's unjustify; if not, unget the keystroke 
       and return */

#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION

    /* If it was a mouse click, parse it with do_mouse and it might become
	the unjustify key.  Else give it back to the input stream.  */
    if ((kbinput = wgetch(edit)) == KEY_MOUSE)
	do_mouse();
    else
	ungetch(kbinput);
#endif
#endif

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

	/* Restore totsize from before justify */
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
    int i, sofar = 0, meta_shortcut = 0, helplen;
    long allocsize = 1;		/* How much space we're gonna need for the help text */
    char buf[BUFSIZ] = "", *ptr = NULL;
    toggle *t;
    shortcut *s;

    helplen = length_of_list(currshortcut);

    /* First set up the initial help text for the current function */
    if (currshortcut == whereis_list || currshortcut == replace_list
	     || currshortcut == replace_list_2)
	ptr = _("Search Command Help Text\n\n "
		"Enter the words or characters you would like to search "
		"for, then hit enter.  If there is a match for the text you "
		"entered, the screen will be updated to the location of the "
		"nearest match for the search string.\n\n "
		"If using Pico Mode via the -p or --pico flags, using the "
		"Meta-P toggle or using a nanorc file, the previous search "
		"string will be shown in brackets after the Search: prompt.  "
		"Hitting enter without entering any text will perform the "
		"previous search. Otherwise, the previous string will be "
		"placed in front of the cursor, and can be edited or deleted "
		"before hitting enter.\n\n The following functions keys are "
		"available in Search mode:\n\n");
    else if (currshortcut == goto_list)
	ptr = _("Goto Line Help Text\n\n "
		"Enter the line number that you wish to go to and hit "
		"Enter.  If there are fewer lines of text than the "
		"number you entered, you will be brought to the last line "
		"of the file.\n\n The following functions keys are "
		"available in Goto Line mode:\n\n");
    else if (currshortcut == insertfile_list)
	ptr = _("Insert File Help Text\n\n "
		"Type in the name of a file to be inserted into the current "
		"file buffer at the current cursor location.\n\n "
		"If you have compiled nano with multiple file buffer "
		"support, and enable multiple buffers with the -F "
		"or --multibuffer command line flags, the Meta-F toggle or "
		"using a nanorc file, inserting a file will cause it to be "
		"loaded into a separate buffer (use Meta-< and > to switch "
		"between file buffers).\n\n If you need another blank "
		"buffer, do not enter any filename, or type in a "
		"nonexistent filename at the prompt and press "
		"Enter.\n\n The following function keys are "
		"available in Insert File mode:\n\n");
    else if (currshortcut == writefile_list)
	ptr = _("Write File Help Text\n\n "
		"Type the name that you wish to save the current file "
		"as and hit enter to save the file.\n\n "
		"If you are using the marker code with Ctrl-^ and have "
		"selected text, you will be prompted to save only the "
		"selected portion to a separate file.  To reduce the "
		"chance of overwriting the current file with just a portion "
		"of it, the current filename is not the default in this "
		"mode.\n\n The following function keys are available in "
		"Write File mode:\n\n");
#ifndef DISABLE_BROWSER
    else if (currshortcut == browser_list)
	ptr = _("File Browser Help Text\n\n "
		"The file browser is used to visually browse the "
		"directory structure to select a file for reading "
		"or writing.  You may use the arrow keys or Page Up/"
		"Down to browse through the files, and S or Enter to "
		"choose the selected file or enter the selected "
		"directory. To move up one level, select the directory "
		"called \"..\" at the top of the file list.\n\n The "
		"following functions keys are available in the file "
		"browser:\n\n");
    else if (currshortcut == gotodir_list)
	ptr = _("Browser Goto Directory Help Text\n\n "
		"Enter the name of the directory you would like to "
		"browse to.\n\n If tab completion has not been disabled, "
		"you can use the TAB key to (attempt to) automatically "
		"complete the directory name.\n\n  The following function "
		"keys are available in Browser GotoDir mode:\n\n");
#endif
    else if (currshortcut == spell_list)
	ptr = _("Spell Check Help Text\n\n "
		"The spell checker checks the spelling of all text "
		"in the current file.  When an unknown word is "
		"encountered, it is highlighted and a replacement can "
		"be edited.  It will then prompt to replace every "
		"instance of the given misspelled word in the "
		"current file.\n\n The following other functions are "
		"available in Spell Check mode:\n\n");
#ifndef NANO_SMALL
    else if (currshortcut == extcmd_list)
	ptr = _("External Command Help Text\n\n "
		"This menu allows you to insert the output of a command "
		"run by the shell into the current buffer (or a new "
		"buffer in multibuffer mode).\n\n The following keys are "
		"available in this mode:\n\n");
#endif
    else /* Default to the main help list */
	ptr = help_text_init;

    /* Compute the space needed for the shortcut lists - we add 15 to
       have room for the shortcut abbrev and its possible alternate keys */
    s = currshortcut;
    for (i = 0; i <= helplen - 1; i++) {
	if (s->help != NULL)
	    allocsize += strlen(s->help) + 15;
	s = s->next;
    }

    /* If we're on the main list, we also allocate space for toggle help text. */
    if (currshortcut == main_list) {
	for (t = toggles; t != NULL; t = t->next)
	    if (t->desc != NULL)
		allocsize += strlen(t->desc) + 30;
    }

    allocsize += strlen(ptr);

    if (help_text != NULL)
	free(help_text);

    /* Allocate space for the help text */
    help_text = charalloc(allocsize);

    /* Now add the text we want */
    strcpy(help_text, ptr);

    /* Now add our shortcut info */
    s = currshortcut;
    for (i = 0; i <= helplen - 1; i++) {
	if (s->val > 0 && s->val < 'a')
	    sofar = snprintf(buf, BUFSIZ, "^%c	", s->val + 64);
	else {
	    if (s->altval > 0) {
		sofar = 0;
		meta_shortcut = 1;
	    }
	    else
		sofar = snprintf(buf, BUFSIZ, "	");
	}

	if (!meta_shortcut) {
	    if (s->misc1 > KEY_F0 && s->misc1 <= KEY_F(64))
		sofar += snprintf(&buf[sofar], BUFSIZ - sofar, "(F%d)	",
				  s->misc1 - KEY_F0);
	    else
		sofar += snprintf(&buf[sofar], BUFSIZ - sofar, "	");
	}

	if (s->altval > 0 && s->altval < 91 
		&& (s->altval - 32) > 32)
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar,
	    (meta_shortcut ? "M-%c	" : "(M-%c)	"),
	    s->altval - 32);
	else if (s->altval >= 'a')
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar,
	    (meta_shortcut ? "M-%c	" : "(M-%c)	"),
	    s->altval - 32);
	else if (s->altval > 0)
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar,
	    (meta_shortcut ? "M-%c	" : "(M-%c)	"),
	    s->altval);
	/* Hack */
	else if (s->val >= 'a')
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar,
	    (meta_shortcut ? "(M-%c)	" : "M-%c	"),
	    s->val - 32);
	else
	    sofar += snprintf(&buf[sofar], BUFSIZ - sofar, "	");

	if (meta_shortcut) {
	    if (s->misc1 > KEY_F0 && s->misc1 <= KEY_F(64))
		sofar += snprintf(&buf[sofar], BUFSIZ - sofar,
			"(F%d)		", s->misc1 - KEY_F0);
	    else
		sofar += snprintf(&buf[sofar], BUFSIZ - sofar,
			"		");
	}

	if (s->help != NULL)
	    snprintf(&buf[sofar], BUFSIZ - sofar, "%s", s->help);

	strcat(help_text, buf);
	strcat(help_text, "\n");

	s = s->next;
    }

    /* And the toggles... */
    if (currshortcut == main_list)
	for (t = toggles; t != NULL; t = t->next) {
		sofar = snprintf(buf, BUFSIZ,
			     "M-%c			", t->val - 32);
	    if (t->desc != NULL) {
		    snprintf(&buf[sofar], BUFSIZ - sofar, _("%s enable/disable"),
			 t->desc);
	}
	strcat(help_text, buf);
	strcat(help_text, "\n");
    }

}
#endif

void do_toggle(toggle *which)
{
#ifdef NANO_SMALL
    nano_disabled_msg();
#else
    char *enabled = _("enabled");
    char *disabled = _("disabled");

    switch (which->val) {
    case TOGGLE_BACKWARDS_KEY:
    case TOGGLE_CASE_KEY:
    case TOGGLE_REGEXP_KEY:
	return;
    }

    /* Even easier! */
    TOGGLE(which->flag);

    switch (which->val) {
    case TOGGLE_PICOMODE_KEY:
	shortcut_init(0);
	SET(CLEAR_BACKUPSTRING);
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
    case TOGGLE_DOS_KEY:
	UNSET(MAC_FILE);
	break;
    case TOGGLE_MAC_KEY:
	UNSET(DOS_FILE);
	break;
    }

    if (!ISSET(which->flag)) {
	if (which->val == TOGGLE_NOHELP_KEY ||
	    which->val == TOGGLE_WRAP_KEY)
	    statusbar("%s %s", which->desc, enabled);
	else
	    statusbar("%s %s", which->desc, disabled);
    } else {
	if (which->val == TOGGLE_NOHELP_KEY ||
	    which->val == TOGGLE_WRAP_KEY)
	    statusbar("%s %s", which->desc, disabled);
	else
	    statusbar("%s %s", which->desc, enabled);
    }

#endif
}

/* If the NumLock key has made the keypad go awry, print an error
   message; hopefully we can address it later. */
void print_numlock_warning(void)
{
    static int didmsg = 0;
    if (!didmsg) {
	statusbar(_
		  ("NumLock glitch detected.  Keypad will malfunction with NumLock off"));
	didmsg = 1;
    }
}

/* This function returns the correct keystroke, given the A,B,C or D
   input key.  This is a common sequence of many terms which send
   Esc-O-[A-D] or Esc-[-[A-D]. */
int ABCD(int input)
{
    switch (input) {
    case 'A':
    case 'a':
	return (KEY_UP);
    case 'B':
    case 'b':
	return (KEY_DOWN);
    case 'C':
    case 'c':
	return (KEY_RIGHT);
    case 'D':
    case 'd':
	return (KEY_LEFT);
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
    int modify_control_seq;
    char *argv0;
    shortcut *s;
#ifndef NANO_SMALL
    toggle *t;
#endif

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
	{"dos", 0, 0, 'D'},
	{"mac", 0, 0, 'M'},
	{"noconvert", 0, 0, 'N'},
	{"autoindent", 0, 0, 'i'},
#endif
	{"tempfile", 0, 0, 't'},
#ifndef DISABLE_SPELLER
	{"speller", 1, 0, 's'},
#endif

#ifndef DISABLE_WRAPJUSTIFY
	{"fill", 1, 0, 'r'},
#endif
	{"mouse", 0, 0, 'm'},
#ifndef DISABLE_OPERATINGDIR
	{"operatingdir", 1, 0, 'o'},
#endif
	{"pico", 0, 0, 'p'},
	{"nofollow", 0, 0, 'l'},
	{"tabsize", 1, 0, 'T'},

#ifdef ENABLE_MULTIBUFFER
	{"multibuffer", 0, 0, 'F'},
#endif
#ifndef NANO_SMALL
	{"smooth", 0, 0, 'S'},
#endif
	{"keypad", 0, 0, 'K'},
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

#ifdef ENABLE_NANORC
    do_rcfile();
#endif /* ENABLE_NANORC */

#ifdef HAVE_GETOPT_LONG
    while ((optchr = getopt_long(argc, argv, "h?DFKMNQ:RST:Vabcefgijklmo:pr:s:tvwxz",
				 long_options, &option_index)) != EOF) {
#else
    while ((optchr =
	    getopt(argc, argv, "h?DFKMNQ:RST:Vabcefgijklmo:pr:s:tvwxz")) != EOF) {
#endif

	switch (optchr) {

#ifndef NANO_SMALL
	case 'D':
	    SET(DOS_FILE);
	    break;
#endif
#ifdef ENABLE_MULTIBUFFER
	case 'F':
	    SET(MULTIBUFFER);
	    break;
#endif
	case 'K':
	    SET(ALT_KEYPAD);
	    break;
#ifndef NANO_SMALL
	case 'M':
	    SET(MAC_FILE);
	    break;
	case 'N':
	    SET(NO_CONVERT);
	    break;
#endif
	case 'Q':
#ifndef DISABLE_JUSTIFY
	    quotestr = optarg;
	    break;
#else
	    usage();	/* To stop bogus data for tab width */
	    finish(1);
#endif
#ifdef HAVE_REGEX_H
	case 'R':
	    SET(USE_REGEXP);
	    break;
#endif
#ifndef NANO_SMALL
	case 'S':
	    SET(SMOOTHSCROLL);
	    break;
#endif
	case 'T':
	    tabsize = atoi(optarg);
	    if (tabsize <= 0) {
		usage();	/* To stop bogus data for tab width */
		finish(1);
	    }
	    break;
	case 'V':
	    version();
	    exit(0);
	case 'a':
	case 'b':
	case 'e':
	case 'f':
	case 'g':
	case 'j':
	    /* Pico compatibility flags */
	    break;
	case 'c':
	    SET(CONSTUPDATE);
	    break;
	case 'h':
	case '?':
	    usage();
	    exit(0);
#ifndef NANO_SMALL
	case 'i':
	    SET(AUTOINDENT);
	    break;
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
#ifndef DISABLE_OPERATINGDIR
	case 'o':
	    operating_dir = charalloc(strlen(optarg) + 1);
	    strcpy(operating_dir, optarg);

	    /* make sure we're inside the operating directory */
	    if (check_operating_dir(".", 0)) {
		if (chdir(operating_dir) == -1) {
		    free(operating_dir);
		    operating_dir = NULL;
		}
	    }
	    break;
#endif
	case 'p':
	    SET(PICO_MODE);
	    break;
	case 'r':
#ifndef DISABLE_WRAPJUSTIFY
	    fill = atoi(optarg);
	    if (fill < 0)
		wrap_at = fill;
	    else if (fill == 0) {
		usage();	/* To stop bogus data (like a string) */
		finish(1);
	    }
	    break;
#else
	    usage();
	    exit(0);

#endif
#ifndef DISABLE_SPELLER
	case 's':
	    alt_speller = charalloc(strlen(optarg) + 1);
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
#ifdef DISABLE_WRAPPING
	    usage();
	    exit(0);
#else
	    SET(NO_WRAP);
	    break;
#endif				/* DISABLE_WRAPPING */
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
    global_init(0);
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

    if (!ISSET(ALT_KEYPAD)) {
	keypad(edit, TRUE);
	keypad(bottomwin, TRUE);
    }

#ifdef ENABLE_COLOR
    do_colorinit();

#endif /* ENABLE_COLOR */

#ifdef DEBUG
    fprintf(stderr, _("Main: bottom win\n"));
#endif
    /* Set up bottom of window */
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
	do_gotoline(startline, 0);
    else
	edit_update(fileage, CENTER);

    /* return here after a sigwinch */
    sigsetjmp(jmpbuf, 1);

    /* Fix clobber-age */
    kbinput = 0;
    keyhandled = 0;
    modify_control_seq = 0;

    edit_refresh();
    reset_cursor();

    while (1) {

#ifndef DISABLE_MOUSE
	currshortcut = main_list;
#endif

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
	    case 'O':
		kbinput = wgetch(edit);
		if ((kbinput <= 'D' && kbinput >= 'A') ||
			(kbinput <= 'd' && kbinput >= 'a'))
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
#ifndef NANO_SMALL
	    case ' ':
		/* If control-space is next word, Alt-space should be previous word */
		do_prev_word();
		keyhandled = 1;
		break;
#endif
	    case '[':
		switch (kbinput = wgetch(edit)) {
		case '1':	/* Alt-[-1-[0-5,7-9] = F1-F8 in X at least */
		    kbinput = wgetch(edit);
		    if (kbinput >= '1' && kbinput <= '5') {
			kbinput = KEY_F(kbinput - 48);
			wgetch(edit);
		    } else if (kbinput >= '7' && kbinput <= '9') {
			kbinput = KEY_F(kbinput - 49);
			wgetch(edit);
		    } else if (kbinput == '~')
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
		    case '~':
			goto do_insertkey;
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
		case 'V':	/* Alt-[-V = Page Up in Hurd Console */
		case 'I':	/* Alt-[-I = Page Up - FreeBSD Console */
		    kbinput = KEY_PPAGE;
		    break;
		case '6':	/* Alt-[-6 = Page Down */
		    kbinput = KEY_NPAGE;
		    wgetch(edit);
		    break;
		case 'U':	/* Alt-[-U = Page Down in Hurd Console */
		case 'G':	/* Alt-[-G = Page Down - FreeBSD Console */
		    kbinput = KEY_NPAGE;
		    break;
		case '7':
		    kbinput = KEY_HOME;
		    wgetch(edit);
		    break;
		case '8':
		    kbinput = KEY_END;
		    wgetch(edit);
		    break;
		case '9':	/* Alt-[-9 = Delete in Hurd Console */
		    kbinput = KEY_DC;
		    break;
		case '@':	/* Alt-[-@ = Insert in Hurd Console */
		case 'L':	/* Alt-[-L = Insert - FreeBSD Console */
		    goto do_insertkey;
		case '[':	/* Alt-[-[-[A-E], F1-F5 in Linux console */
		    kbinput = wgetch(edit);
		    if (kbinput >= 'A' && kbinput <= 'E')
			kbinput = KEY_F(kbinput - 64);
		    break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		    kbinput = ABCD(kbinput);
		    break;
		case 'H':
		    kbinput = KEY_HOME;
		    break;
		case 'F':
		case 'Y':		/* End Key in Hurd Console */
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

#ifdef ENABLE_MULTIBUFFER
	    case NANO_OPENPREV_KEY:
	    case NANO_OPENPREV_ALTKEY:
		open_prevfile(0);
		keyhandled = 1;
		break;
	    case NANO_OPENNEXT_KEY:
	    case NANO_OPENNEXT_ALTKEY:
		open_nextfile(0);
		keyhandled = 1;
		break;
#endif

#if !defined (NANO_SMALL) && defined (HAVE_REGEX_H)
	    case NANO_BRACKET_KEY:
		do_find_bracket();
		keyhandled = 1;
		break;
#endif

	    default:
		/* Check for the altkey defs.... */
		for (s = main_list; s != NULL; s = s->next)
		    if (kbinput == s->altval ||
			kbinput == s->altval - 32) {
			kbinput = s->val;
			break;
		    }
#ifndef NANO_SMALL
		/* And for toggle switches */
		for (t = toggles; t != NULL && !keyhandled; t = t->next)
		    if (kbinput == t->val ||
			(t->val > 'a' && 
				kbinput == t->val - 32)) {
			do_toggle(t);
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
        
#if !defined(DISABLE_BROWSER) || !defined(DISABLE_MOUSE) || !defined (DISABLE_HELP)
	for (s = currshortcut; s != NULL && !keyhandled; s = s->next) {
#else
	for (s = main_list; s != NULL && !keyhandled; s = s->next) {
#endif
	    if (kbinput == s->val ||
		(s->misc1 && kbinput == s->misc1) ||
		(s->misc2 && kbinput == s->misc2)) {
		if (ISSET(VIEW_MODE) && !s->viewok)
		    print_view_warning();
		else
		    s->func();
		keyhandled = 1;
	    }
	}
	/* If we're in raw mode or using Alt-Alt-x, we have to catch
	   Control-S and Control-Q */
	if (kbinput == 17 || kbinput == 19)
	    keyhandled = 1;

	/* Catch ^Z by hand when triggered also 
	   407 == ^Z in Linux console when keypad() is used? */
	if (kbinput == 26 || kbinput == 407) {
	    if (ISSET(SUSPEND))
		do_suspend(0);
	    keyhandled = 1;
	}


#ifndef USE_SLANG
	/* Hack, make insert key do something useful, like insert file */
	if (kbinput == KEY_IC) {
#else
	if (0) {
#endif
	  do_insertkey:

#ifdef ENABLE_MULTIBUFFER
	    /* do_insertfile_void() contains the logic needed to
	       handle view mode with the view mode/multibuffer
	       exception, so use it here */
	    do_insertfile_void();
#else
	    print_view_warning();
#endif

	    keyhandled = 1;
	}

	/* Last gasp, stuff that's not in the main lists */
	if (!keyhandled)
	    switch (kbinput) {
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
	    case KEY_MOUSE:
		do_mouse();
		break;
#endif
#endif

	    case 0:		/* Erg */
#ifndef NANO_SMALL
		do_next_word();
		break;
#endif

	    case -1:		/* Stuff that we don't want to do squat */
	    case 410:		/* Must ignore this, it gets sent when we resize */
	    case 29:		/* Ctrl-] */
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
	if (ISSET(DISABLE_CURPOS))
	    UNSET(DISABLE_CURPOS);
	else if (ISSET(CONSTUPDATE))
		do_cursorpos(1);

	reset_cursor();
	wrefresh(edit);
	keyhandled = 0;
    }

    getchar();
    finish(0);

}
