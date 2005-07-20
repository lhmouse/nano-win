/* $Id$ */
/**************************************************************************
 *   nano.c                                                               *
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <assert.h>
#include "proto.h"

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifndef NANO_SMALL
#include <setjmp.h>
#endif

#ifndef DISABLE_WRAPJUSTIFY
static ssize_t fill = 0;	/* Fill - where to wrap lines */
#endif
#ifndef DISABLE_WRAPPING
static bool same_line_wrap = FALSE;	/* Whether wrapped text should
					   be prepended to the next
					   line */
#endif

static struct termios oldterm;	/* The user's original term settings */
static struct sigaction act;	/* For all our fun signal handlers */

#ifndef NANO_SMALL
static sigjmp_buf jmpbuf;	/* Used to return to main() after a
				   SIGWINCH. */
static pid_t pid;		/* The PID of the newly forked process
				 * in execute_command().  It must be
				 * global because the signal handler
				 * needs it. */
#endif

#ifndef DISABLE_JUSTIFY
static filestruct *jusbottom = NULL;
	/* Pointer to end of justify buffer. */
#endif

/* Create a new filestruct node.  Note that we specifically do not set
 * prevnode->next equal to the new line. */
filestruct *make_new_node(filestruct *prevnode)
{
    filestruct *newnode = (filestruct *)nmalloc(sizeof(filestruct));

    newnode->data = NULL;
    newnode->prev = prevnode;
    newnode->next = NULL;
    newnode->lineno = (prevnode != NULL) ? prevnode->lineno + 1 : 1;

    return newnode;
}

/* Make a copy of a filestruct node. */
filestruct *copy_node(const filestruct *src)
{
    filestruct *dst;

    assert(src != NULL);

    dst = (filestruct *)nmalloc(sizeof(filestruct));

    dst->data = mallocstrcpy(NULL, src->data);
    dst->next = src->next;
    dst->prev = src->prev;
    dst->lineno = src->lineno;

    return dst;
}

/* Splice a node into an existing filestruct. */
void splice_node(filestruct *begin, filestruct *newnode, filestruct
	*end)
{
    assert(newnode != NULL && begin != NULL);

    newnode->next = end;
    newnode->prev = begin;
    begin->next = newnode;
    if (end != NULL)
	end->prev = newnode;
}

/* Unlink a node from the rest of the filestruct. */
void unlink_node(const filestruct *fileptr)
{
    assert(fileptr != NULL);

    if (fileptr->prev != NULL)
	fileptr->prev->next = fileptr->next;
    if (fileptr->next != NULL)
	fileptr->next->prev = fileptr->prev;
}

/* Delete a node from the filestruct. */
void delete_node(filestruct *fileptr)
{
    assert(fileptr != NULL && fileptr->data != NULL);

    if (fileptr->data != NULL)
	free(fileptr->data);

    free(fileptr);
}

/* Duplicate a whole filestruct. */
filestruct *copy_filestruct(const filestruct *src)
{
    filestruct *head, *copy;

    assert(src != NULL);

    copy = copy_node(src);
    copy->prev = NULL;
    head = copy;
    src = src->next;

    while (src != NULL) {
	copy->next = copy_node(src);
	copy->next->prev = copy;
	copy = copy->next;

	src = src->next;
    }

    copy->next = NULL;

    return head;
}

/* Free a filestruct. */
void free_filestruct(filestruct *src)
{
    assert(src != NULL);

    while (src->next != NULL) {
	src = src->next;
	delete_node(src->prev);
    }

    delete_node(src);
}

/* Renumber all entries in a filestruct, starting with fileptr. */
void renumber(filestruct *fileptr)
{
    ssize_t line;

    assert(fileptr != NULL && fileptr->prev != NULL);

    line = (fileptr->prev == NULL) ? 0 : fileptr->prev->lineno;

    assert(fileptr != fileptr->next);

    for (; fileptr != NULL; fileptr = fileptr->next)
	fileptr->lineno = ++line;
}

/* Partition a filestruct so it begins at (top, top_x) and ends at (bot,
 * bot_x). */
partition *partition_filestruct(filestruct *top, size_t top_x,
	filestruct *bot, size_t bot_x)
{
    partition *p;

    assert(top != NULL && bot != NULL && openfile->fileage != NULL && openfile->filebot != NULL);

    /* Initialize the partition. */
    p = (partition *)nmalloc(sizeof(partition));

    /* If the top and bottom of the partition are different from the top
     * and bottom of the filestruct, save the latter and then set them
     * to top and bot. */
    if (top != openfile->fileage) {
	p->fileage = openfile->fileage;
	openfile->fileage = top;
    } else
	p->fileage = NULL;
    if (bot != openfile->filebot) {
	p->filebot = openfile->filebot;
	openfile->filebot = bot;
    } else
	p->filebot = NULL;

    /* Save the line above the top of the partition, detach the top of
     * the partition from it, and save the text before top_x in
     * top_data. */
    p->top_prev = top->prev;
    top->prev = NULL;
    p->top_data = mallocstrncpy(NULL, top->data, top_x + 1);
    p->top_data[top_x] = '\0';

    /* Save the line below the bottom of the partition, detach the
     * bottom of the partition from it, and save the text after bot_x in
     * bot_data. */
    p->bot_next = bot->next;
    bot->next = NULL;
    p->bot_data = mallocstrcpy(NULL, bot->data + bot_x);

    /* Remove all text after bot_x at the bottom of the partition. */
    null_at(&bot->data, bot_x);

    /* Remove all text before top_x at the top of the partition. */
    charmove(top->data, top->data + top_x, strlen(top->data) -
	top_x + 1);
    align(&top->data);

    /* Return the partition. */
    return p;
}

/* Unpartition a filestruct so it begins at (fileage, 0) and ends at
 * (filebot, strlen(filebot)) again. */
void unpartition_filestruct(partition **p)
{
    char *tmp;

    assert(p != NULL && openfile->fileage != NULL && openfile->filebot != NULL);

    /* Reattach the line above the top of the partition, and restore the
     * text before top_x from top_data.  Free top_data when we're done
     * with it. */
    tmp = mallocstrcpy(NULL, openfile->fileage->data);
    openfile->fileage->prev = (*p)->top_prev;
    if (openfile->fileage->prev != NULL)
	openfile->fileage->prev->next = openfile->fileage;
    openfile->fileage->data = charealloc(openfile->fileage->data,
	strlen((*p)->top_data) + strlen(openfile->fileage->data) + 1);
    strcpy(openfile->fileage->data, (*p)->top_data);
    free((*p)->top_data);
    strcat(openfile->fileage->data, tmp);
    free(tmp);

    /* Reattach the line below the bottom of the partition, and restore
     * the text after bot_x from bot_data.  Free bot_data when we're
     * done with it. */
    openfile->filebot->next = (*p)->bot_next;
    if (openfile->filebot->next != NULL)
	openfile->filebot->next->prev = openfile->filebot;
    openfile->filebot->data = charealloc(openfile->filebot->data,
	strlen(openfile->filebot->data) + strlen((*p)->bot_data) + 1);
    strcat(openfile->filebot->data, (*p)->bot_data);
    free((*p)->bot_data);

    /* Restore the top and bottom of the filestruct, if they were
     * different from the top and bottom of the partition. */
    if ((*p)->fileage != NULL)
	openfile->fileage = (*p)->fileage;
    if ((*p)->filebot != NULL)
	openfile->filebot = (*p)->filebot;

    /* Uninitialize the partition. */
    free(*p);
    *p = NULL;
}

/* Move all the text between (top, top_x) and (bot, bot_x) in the
 * current filestruct to a filestruct beginning with file_top and ending
 * with file_bot.  If no text is between (top, top_x) and (bot, bot_x),
 * don't do anything. */
void move_to_filestruct(filestruct **file_top, filestruct **file_bot,
	filestruct *top, size_t top_x, filestruct *bot, size_t bot_x)
{
    filestruct *top_save;
    bool at_edittop;
#ifndef NANO_SMALL
    bool mark_inside = FALSE;
#endif

    assert(file_top != NULL && file_bot != NULL && top != NULL && bot != NULL);

    /* If (top, top_x)-(bot, bot_x) doesn't cover any text, get out. */
    if (top == bot && top_x == bot_x)
	return;

    /* Partition the filestruct so that it contains only the text from
     * (top, top_x) to (bot, bot_x), keep track of whether the top of
     * the partition is the top of the edit window, and keep track of
     * whether the mark begins inside the partition. */
    filepart = partition_filestruct(top, top_x, bot, bot_x);
    at_edittop = (openfile->fileage == openfile->edittop);
#ifndef NANO_SMALL
    if (openfile->mark_set)
	mark_inside = (openfile->mark_begin->lineno >=
		openfile->fileage->lineno &&
		openfile->mark_begin->lineno <=
		openfile->filebot->lineno &&
		(openfile->mark_begin != openfile->fileage ||
		openfile->mark_begin_x >= top_x) &&
		(openfile->mark_begin != openfile->filebot ||
		openfile->mark_begin_x <= bot_x));
#endif

    /* Get the number of characters in the text, and subtract it from
     * totsize. */
    openfile->totsize -= get_totsize(top, bot);

    if (*file_top == NULL) {
	/* If file_top is empty, just move all the text directly into
	 * it.  This is equivalent to tacking the text in top onto the
	 * (lack of) text at the end of file_top. */
	*file_top = openfile->fileage;
	*file_bot = openfile->filebot;

	/* Renumber starting with file_top. */
	renumber(*file_top);
    } else {
	filestruct *file_bot_save = *file_bot;

	/* Otherwise, tack the text in top onto the text at the end of
	 * file_bot. */
	(*file_bot)->data = charealloc((*file_bot)->data,
		strlen((*file_bot)->data) +
		strlen(openfile->fileage->data) + 1);
	strcat((*file_bot)->data, openfile->fileage->data);

	/* Attach the line after top to the line after file_bot.  Then,
	 * if there's more than one line after top, move file_bot down
	 * to bot. */
	(*file_bot)->next = openfile->fileage->next;
	if ((*file_bot)->next != NULL) {
	    (*file_bot)->next->prev = *file_bot;
	    *file_bot = openfile->filebot;
	}

	/* Renumber starting with the line after the original
	 * file_bot. */
	if (file_bot_save->next != NULL)
	    renumber(file_bot_save->next);
    }

    /* Since the text has now been saved, remove it from the filestruct.
     * If the top of the partition was the top of the edit window, set
     * edittop to where the text used to start.  If the mark began
     * inside the partition, set the beginning of the mark to where the
     * text used to start. */
    openfile->fileage = (filestruct *)nmalloc(sizeof(filestruct));
    openfile->fileage->data = mallocstrcpy(NULL, "");
    openfile->filebot = openfile->fileage;
    if (at_edittop)
	openfile->edittop = openfile->fileage;
#ifndef NANO_SMALL
    if (mark_inside) {
	openfile->mark_begin = openfile->fileage;
	openfile->mark_begin_x = top_x;
    }
#endif

    /* Restore the current line and cursor position. */
    openfile->current = openfile->fileage;
    openfile->current_x = top_x;

    top_save = openfile->fileage;

    /* Unpartition the filestruct so that it contains all the text
     * again, minus the saved text. */
    unpartition_filestruct(&filepart);

    /* Renumber starting with the beginning line of the old
     * partition. */
    renumber(top_save);

    if (openfile->filebot->data[0] != '\0')
	new_magicline();

    /* Set totlines to the new number of lines in the file. */
    openfile->totlines = openfile->filebot->lineno;
}

/* Copy all the text from the filestruct beginning with file_top and
 * ending with file_bot to the current filestruct at the current cursor
 * position. */
void copy_from_filestruct(filestruct *file_top, filestruct *file_bot)
{
    filestruct *top_save;
    bool at_edittop;

    assert(file_top != NULL && file_bot != NULL);

    /* Partition the filestruct so that it contains no text, and keep
     * track of whether the top of the partition is the top of the edit
     * window. */
    filepart = partition_filestruct(openfile->current,
	openfile->current_x, openfile->current, openfile->current_x);
    at_edittop = (openfile->fileage == openfile->edittop);

    /* Put the top and bottom of the filestruct at copies of file_top
     * and file_bot. */
    openfile->fileage = copy_filestruct(file_top);
    openfile->filebot = openfile->fileage;
    while (openfile->filebot->next != NULL)
	openfile->filebot = openfile->filebot->next;

    /* Restore the current line and cursor position. */
    openfile->current = openfile->filebot;
    openfile->current_x = strlen(openfile->filebot->data);
    if (openfile->fileage == openfile->filebot)
	openfile->current_x += strlen(filepart->top_data);

    /* Get the number of characters in the text, and add it to
     * totsize. */
    openfile->totsize += get_totsize(openfile->fileage,
	openfile->filebot);

    /* If the top of the partition was the top of the edit window, set
     * edittop to where the saved text now starts, and update the
     * current y-coordinate to account for the number of lines it
     * has, less one since the first line will be tacked onto the
     * current line. */
    if (at_edittop)
	openfile->edittop = openfile->fileage;
    openfile->current_y += openfile->filebot->lineno - 1;

    top_save = openfile->fileage;

    /* Unpartition the filestruct so that it contains all the text
     * again, minus the saved text. */
    unpartition_filestruct(&filepart);

    /* Renumber starting with the beginning line of the old
     * partition. */
    renumber(top_save);

    if (openfile->filebot->data[0] != '\0')
	new_magicline();

    /* Set totlines to the new number of lines in the file. */
    openfile->totlines = openfile->filebot->lineno;
}

/* Create a new openfilestruct node. */
openfilestruct *make_new_opennode(void)
{
    openfilestruct *newnode =
	(openfilestruct *)nmalloc(sizeof(openfilestruct));

    newnode->filename = NULL;
    newnode->fileage = NULL;
    newnode->filebot = NULL;
    newnode->edittop = NULL;
    newnode->current = NULL;

    return newnode;
}

/* Splice a node into an existing openfilestruct. */
void splice_opennode(openfilestruct *begin, openfilestruct *newnode,
	openfilestruct *end)
{
    assert(newnode != NULL && begin != NULL);

    newnode->next = end;
    newnode->prev = begin;
    begin->next = newnode;

    if (end != NULL)
	end->prev = newnode;
}

/* Unlink a node from the rest of the openfilestruct, and delete it. */
void unlink_opennode(openfilestruct *fileptr)
{
    assert(fileptr != NULL && fileptr->prev != NULL && fileptr->next != NULL && fileptr != fileptr->prev && fileptr != fileptr->next);

    fileptr->prev->next = fileptr->next;
    fileptr->next->prev = fileptr->prev;

    delete_opennode(fileptr);
}

/* Delete a node from the openfilestruct. */
void delete_opennode(openfilestruct *fileptr)
{
    assert(fileptr != NULL && fileptr->filename != NULL && fileptr->fileage != NULL);

    free(fileptr->filename);
    free_filestruct(fileptr->fileage);
#ifndef NANO_SMALL
    if (fileptr->current_stat != NULL)
	free(fileptr->current_stat);
#endif

    free(fileptr);
}

#ifdef DEBUG
/* Deallocate all memory associated with this and later files, including
 * the lines of text. */
void free_openfilestruct(openfilestruct *src)
{
    assert(src != NULL);

    while (src != src->next) {
	src = src->next;
	delete_opennode(src->prev);
    }

    delete_opennode(src);
}
#endif

void print_view_warning(void)
{
    statusbar(_("Key illegal in VIEW mode"));
}

/* What we do when we're all set to exit. */
void finish(void)
{
    if (!ISSET(NO_HELP))
	blank_bottombars();
    else
	blank_statusbar();

    wrefresh(bottomwin);
    endwin();

    /* Restore the old terminal settings. */
    tcsetattr(0, TCSANOW, &oldterm);

#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
    if (!ISSET(NO_RCFILE) && ISSET(HISTORYLOG))
	save_history();
#endif

#ifdef DEBUG
    thanks_for_all_the_fish();
#endif

    exit(0);
}

/* Die (gracefully?). */
void die(const char *msg, ...)
{
    va_list ap;

    endwin();
    curses_ended = TRUE;

    /* Restore the old terminal settings. */
    tcsetattr(0, TCSANOW, &oldterm);

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    /* Save the current file buffer if it's been modified. */
    if (openfile->modified) {
	/* If we've partitioned the filestruct, unpartition it now. */
	if (filepart != NULL)
	    unpartition_filestruct(&filepart);

	die_save_file(openfile->filename);
    }

#ifdef ENABLE_MULTIBUFFER
    /* Save all of the other modified file buffers, if any. */
    if (openfile != NULL) {
	openfilestruct *tmp = openfile;

	while (tmp != openfile->next) {
	    openfile = openfile->next;

	    /* Save the current file buffer if it's been modified. */
	    if (openfile->modified)
		die_save_file(openfile->filename);
	}
    }
#endif

    /* Get out. */
    exit(1);
}

void die_save_file(const char *die_filename)
{
    char *retval;
    bool failed = TRUE;

    /* If we're using restricted mode, don't write any emergency backup
     * files, since that would allow reading from or writing to files
     * not specified on the command line. */
    if (ISSET(RESTRICTED))
	return;

    /* If we can't save, we have REAL bad problems, but we might as well
     * TRY. */
    if (die_filename[0] == '\0')
	die_filename = "nano";

    retval = get_next_filename(die_filename, ".save");
    if (retval[0] != '\0')
	failed = (write_file(retval, NULL, TRUE, FALSE, TRUE) == -1);

    if (!failed)
	fprintf(stderr, _("\nBuffer written to %s\n"), retval);
    else if (retval[0] != '\0')
	fprintf(stderr, _("\nBuffer not written to %s: %s\n"), retval,
		strerror(errno));
    else
	fprintf(stderr, _("\nBuffer not written: %s\n"),
		_("Too many backup files?"));

    free(retval);
}

/* Die with an error message that the screen was too small if, well, the
 * screen is too small. */
void check_die_too_small(void)
{
    editwinrows = LINES - 5 + no_more_space() + no_help();
    if (editwinrows < MIN_EDITOR_ROWS)
	die(_("Window size is too small for nano...\n"));
}

/* Make sure the window size isn't too small, and reinitialize the fill
 * variable, since it depends on the window size. */
void window_size_init(void)
{
    check_die_too_small();

#ifndef DISABLE_WRAPJUSTIFY
    fill = wrap_at;
    if (fill <= 0)
	fill += COLS;
    if (fill < 0)
	fill = 0;
#endif
}

void window_init(void)
{
    check_die_too_small();

    if (topwin != NULL)
	delwin(topwin);
    if (edit != NULL)
	delwin(edit);
    if (bottomwin != NULL)
	delwin(bottomwin);

    /* Set up the windows. */
    topwin = newwin(2 - no_more_space(), COLS, 0, 0);
    edit = newwin(editwinrows, COLS, 2 - no_more_space(), 0);
    bottomwin = newwin(3 - no_help(), COLS, editwinrows +
	(2 - no_more_space()), 0);

    /* Turn the keypad back on. */
    keypad(edit, TRUE);
    keypad(bottomwin, TRUE);
}

#ifndef DISABLE_MOUSE
void mouse_init(void)
{
    if (ISSET(USE_MOUSE)) {
	mousemask(BUTTON1_RELEASED, NULL);
	mouseinterval(50);
    } else
	mousemask(0, NULL);
}
#endif

#ifndef DISABLE_HELP
/* This function allocates help_text, and stores the help string in it. 
 * help_text should be NULL initially. */
void help_init(void)
{
    size_t allocsize = 0;	/* Space needed for help_text. */
    const char *htx[3];		/* Untranslated help message.  We break
				 * it up into three chunks in case the
				 * full string is too long for the
				 * compiler to handle. */
    char *ptr;
    const shortcut *s;
#ifndef NANO_SMALL
    const toggle *t;
#ifdef ENABLE_NANORC
    bool old_whitespace = ISSET(WHITESPACE_DISPLAY);

    UNSET(WHITESPACE_DISPLAY);
#endif
#endif

    /* First, set up the initial help text for the current function. */
    if (currshortcut == whereis_list || currshortcut == replace_list
	     || currshortcut == replace_list_2) {
	htx[0] = N_("Search Command Help Text\n\n "
		"Enter the words or characters you would like to "
		"search for, and then press Enter.  If there is a "
		"match for the text you entered, the screen will be "
		"updated to the location of the nearest match for the "
		"search string.\n\n The previous search string will be "
		"shown in brackets after the search prompt.  Hitting "
		"Enter without entering any text will perform the "
		"previous search.  ");
	htx[1] = N_("If you have selected text with the mark and then "
		"search to replace, only matches in the selected text "
		"will be replaced.\n\n The following function keys are "
		"available in Search mode:\n\n");
	htx[2] = NULL;
    } else if (currshortcut == gotoline_list) {
	htx[0] = N_("Go To Line Help Text\n\n "
		"Enter the line number that you wish to go to and hit "
		"Enter.  If there are fewer lines of text than the "
		"number you entered, you will be brought to the last "
		"line of the file.\n\n The following function keys are "
		"available in Go To Line mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    } else if (currshortcut == insertfile_list) {
	htx[0] = N_("Insert File Help Text\n\n "
		"Type in the name of a file to be inserted into the "
		"current file buffer at the current cursor "
		"location.\n\n If you have compiled nano with multiple "
		"file buffer support, and enable multiple file buffers "
		"with the -F or --multibuffer command line flags, the "
		"Meta-F toggle, or a nanorc file, inserting a file "
		"will cause it to be loaded into a separate buffer "
		"(use Meta-< and > to switch between file buffers).  ");
	htx[1] = N_("If you need another blank buffer, do not enter "
		"any filename, or type in a nonexistent filename at "
		"the prompt and press Enter.\n\n The following "
		"function keys are available in Insert File mode:\n\n");
	htx[2] = NULL;
    } else if (currshortcut == writefile_list) {
	htx[0] = N_("Write File Help Text\n\n "
		"Type the name that you wish to save the current file "
		"as and press Enter to save the file.\n\n If you have "
		"selected text with the mark, you will be prompted to "
		"save only the selected portion to a separate file.  To "
		"reduce the chance of overwriting the current file with "
		"just a portion of it, the current filename is not the "
		"default in this mode.\n\n The following function keys "
		"are available in Write File mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#ifndef DISABLE_BROWSER
    else if (currshortcut == browser_list) {
	htx[0] = N_("File Browser Help Text\n\n "
		"The file browser is used to visually browse the "
		"directory structure to select a file for reading "
		"or writing.  You may use the arrow keys or Page Up/"
		"Down to browse through the files, and S or Enter to "
		"choose the selected file or enter the selected "
		"directory.  To move up one level, select the "
		"directory called \"..\" at the top of the file "
		"list.\n\n The following function keys are available "
		"in the file browser:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    } else if (currshortcut == gotodir_list) {
	htx[0] = N_("Browser Go To Directory Help Text\n\n "
		"Enter the name of the directory you would like to "
		"browse to.\n\n If tab completion has not been "
		"disabled, you can use the Tab key to (attempt to) "
		"automatically complete the directory name.\n\n The "
		"following function keys are available in Browser Go "
		"To Directory mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#endif
#ifndef DISABLE_SPELLER
    else if (currshortcut == spell_list) {
	htx[0] = N_("Spell Check Help Text\n\n "
		"The spell checker checks the spelling of all text in "
		"the current file.  When an unknown word is "
		"encountered, it is highlighted and a replacement can "
		"be edited.  It will then prompt to replace every "
		"instance of the given misspelled word in the current "
		"file, or, if you have selected text with the mark, in "
		"the selected text.\n\n The following other functions "
		"are available in Spell Check mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#endif
#ifndef NANO_SMALL
    else if (currshortcut == extcmd_list) {
	htx[0] = N_("Execute Command Help Text\n\n "
		"This menu allows you to insert the output of a "
		"command run by the shell into the current buffer (or "
		"a new buffer in multiple file buffer mode). If you "
		"need another blank buffer, do not enter any "
		"command.\n\n The following keys are available in "
		"Execute Command mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#endif
    else {
	/* Default to the main help list. */
	htx[0] = N_(" nano help text\n\n "
		"The nano editor is designed to emulate the "
		"functionality and ease-of-use of the UW Pico text "
		"editor.  There are four main sections of the editor.  "
		"The top line shows the program version, the current "
		"filename being edited, and whether or not the file "
		"has been modified.  Next is the main editor window "
		"showing the file being edited.  The status line is "
		"the third line from the bottom and shows important "
		"messages.  The bottom two lines show the most "
		"commonly used shortcuts in the editor.\n\n ");
	htx[1] = N_("The notation for shortcuts is as follows: "
		"Control-key sequences are notated with a caret (^) "
		"symbol and can be entered either by using the Control "
		"(Ctrl) key or pressing the Escape (Esc) key twice.  "
		"Escape-key sequences are notated with the Meta (M) "
		"symbol and can be entered using either the Esc, Alt, "
		"or Meta key depending on your keyboard setup.  ");
	htx[2] = N_("Also, pressing Esc twice and then typing a "
		"three-digit decimal number from 000 to 255 will enter "
		"the character with the corresponding value.  The "
		"following keystrokes are available in the main editor "
		"window.  Alternative keys are shown in "
		"parentheses:\n\n");
    }

    htx[0] = _(htx[0]);
    if (htx[1] != NULL)
	htx[1] = _(htx[1]);
    if (htx[2] != NULL)
	htx[2] = _(htx[2]);

    allocsize += strlen(htx[0]);
    if (htx[1] != NULL)
	allocsize += strlen(htx[1]);
    if (htx[2] != NULL)
	allocsize += strlen(htx[2]);

    /* The space needed for the shortcut lists, at most COLS characters,
     * plus '\n'. */
    allocsize += (COLS < 24 ? (24 * mb_cur_max()) :
	((COLS + 1) * mb_cur_max())) * length_of_list(currshortcut);

#ifndef NANO_SMALL
    /* If we're on the main list, we also count the toggle help text.
     * Each line has "M-%c\t\t\t", which fills 24 columns, plus a space,
     * plus translated text, plus '\n'. */
    if (currshortcut == main_list) {
	size_t endis_len = strlen(_("enable/disable"));

	for (t = toggles; t != NULL; t = t->next)
	    allocsize += 8 + strlen(t->desc) + endis_len;
    }
#endif

    /* help_text has been freed and set to NULL unless the user resized
     * while in the help screen. */
    free(help_text);

    /* Allocate space for the help text. */
    help_text = charalloc(allocsize + 1);

    /* Now add the text we want. */
    strcpy(help_text, htx[0]);
    if (htx[1] != NULL)
	strcat(help_text, htx[1]);
    if (htx[2] != NULL)
	strcat(help_text, htx[2]);

    ptr = help_text + strlen(help_text);

    /* Now add our shortcut info.  Assume that each shortcut has, at the
     * very least, an equivalent control key, an equivalent primary meta
     * key sequence, or both.  Also assume that the meta key values are
     * not control characters.  We can display a maximum of 3 shortcut
     * entries. */
    for (s = currshortcut; s != NULL; s = s->next) {
	int entries = 0;

	/* Control key. */
	if (s->ctrlval != NANO_NO_KEY) {
	    entries++;
	    /* Yucky sentinel values that we can't handle a better
	     * way. */
	    if (s->ctrlval == NANO_CONTROL_SPACE) {
		char *space_ptr = display_string(_("Space"), 0, 6,
			FALSE);

		ptr += sprintf(ptr, "^%s", space_ptr);

		free(space_ptr);
	    } else if (s->ctrlval == NANO_CONTROL_8)
		ptr += sprintf(ptr, "^?");
	    /* Normal values. */
	    else
		ptr += sprintf(ptr, "^%c", s->ctrlval + 64);
	    *(ptr++) = '\t';
	}

	/* Function key. */
	if (s->funcval != NANO_NO_KEY) {
	    entries++;
	    /* If this is the first entry, put it in the middle. */
	    if (entries == 1) {
		entries++;
		*(ptr++) = '\t';
	    }
	    ptr += sprintf(ptr, "(F%d)", s->funcval - KEY_F0);
	    *(ptr++) = '\t';
	}

	/* Primary meta key sequence.  If it's the first entry, don't
	 * put parentheses around it. */
	if (s->metaval != NANO_NO_KEY) {
	    entries++;
	    /* If this is the last entry, put it at the end. */
	    if (entries == 2 && s->miscval == NANO_NO_KEY) {
		entries++;
		*(ptr++) = '\t';
	    }
	    /* Yucky sentinel values that we can't handle a better
	     * way. */
	    if (s->metaval == NANO_ALT_SPACE && entries == 1) {
		char *space_ptr = display_string(_("Space"), 0, 5,
			FALSE);

		ptr += sprintf(ptr, "M-%s", space_ptr);

		free(space_ptr);
	    } else
		/* Normal values. */
		ptr += sprintf(ptr, (entries == 1) ? "M-%c" : "(M-%c)",
			toupper(s->metaval));
	    *(ptr++) = '\t';
	}

	/* Miscellaneous meta key sequence. */
	if (entries < 3 && s->miscval != NANO_NO_KEY) {
	    entries++;
	    /* If this is the last entry, put it at the end. */
	    if (entries == 2) {
		entries++;
		*(ptr++) = '\t';
	    }
	    ptr += sprintf(ptr, "(M-%c)", toupper(s->miscval));
	    *(ptr++) = '\t';
	}

	/* Make sure all the help text starts at the same place. */
	while (entries < 3) {
	    entries++;
	    *(ptr++) = '\t';
	}

	assert(s->help != NULL);

	if (COLS > 24) {
	    char *help_ptr = display_string(s->help, 0, COLS - 24,
		FALSE);

	    ptr += sprintf(ptr, help_ptr);

	    free(help_ptr);
	}

	ptr += sprintf(ptr, "\n");
    }

#ifndef NANO_SMALL
    /* And the toggles... */
    if (currshortcut == main_list) {
	for (t = toggles; t != NULL; t = t->next) {

	    assert(t->desc != NULL);

	    ptr += sprintf(ptr, "M-%c\t\t\t%s %s\n", toupper(t->val),
		t->desc, _("enable/disable"));
	}
    }

#ifdef ENABLE_NANORC
    if (old_whitespace)
	SET(WHITESPACE_DISPLAY);
#endif
#endif

    /* If all went well, we didn't overwrite the allocated space for
     * help_text. */
    assert(strlen(help_text) <= allocsize + 1);
}
#endif

#ifdef HAVE_GETOPT_LONG
#define print1opt(shortflag, longflag, desc) print1opt_full(shortflag, longflag, desc)
#else
#define print1opt(shortflag, longflag, desc) print1opt_full(shortflag, desc)
#endif

/* Print one usage string to the screen.  This cuts down on duplicate
 * strings to translate, and leaves out the parts that shouldn't be
 * translatable (the flag names). */
void print1opt_full(const char *shortflag
#ifdef HAVE_GETOPT_LONG
	, const char *longflag
#endif
	, const char *desc)
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

    if (desc != NULL)
	printf("%s", _(desc));
    printf("\n");
}

void usage(void)
{
#ifdef HAVE_GETOPT_LONG
    printf(
	_("Usage: nano [+LINE,COLUMN] [GNU long option] [option] [file]\n\n"));
    printf(_("Option\t\tLong option\t\tMeaning\n"));
#else
    printf(_("Usage: nano [+LINE,COLUMN] [option] [file]\n\n"));
    printf(_("Option\t\tMeaning\n"));
#endif

    print1opt("-h, -?", "--help", N_("Show this message"));
    print1opt(_("+LINE,COLUMN"), "",
	N_("Start at line LINE, column COLUMN"));
#ifndef NANO_SMALL
    print1opt("-A", "--smarthome", N_("Enable smart home key"));
    print1opt("-B", "--backup", N_("Save backups of existing files"));
    print1opt(_("-C [dir]"), _("--backupdir=[dir]"),
	N_("Directory for saving unique backup files"));
    print1opt("-E", "--tabstospaces",
	N_("Convert typed tabs to spaces"));
#endif
#ifdef ENABLE_MULTIBUFFER
    print1opt("-F", "--multibuffer", N_("Enable multiple file buffers"));
#endif
#ifdef ENABLE_NANORC
#ifndef NANO_SMALL
    print1opt("-H", "--historylog",
	N_("Log & read search/replace string history"));
#endif
    print1opt("-I", "--ignorercfiles",
	N_("Don't look at nanorc files"));
#endif
#ifndef NANO_SMALL
    print1opt("-N", "--noconvert",
	N_("Don't convert files from DOS/Mac format"));
#endif
    print1opt("-O", "--morespace", N_("Use more space for editing"));
#ifndef DISABLE_JUSTIFY
    print1opt(_("-Q [str]"), _("--quotestr=[str]"),
	N_("Quoting string"));
#endif
    print1opt("-R", "--restricted", N_("Restricted mode"));
#ifndef NANO_SMALL
    print1opt("-S", "--smooth", N_("Smooth scrolling"));
#endif
    print1opt(_("-T [#cols]"), _("--tabsize=[#cols]"),
	N_("Set width of a tab in cols to #cols"));
#ifndef NANO_SMALL
    print1opt("-U", _("--quickblank"),
	N_("Do quick statusbar blanking"));
#endif
    print1opt("-V", "--version",
	N_("Print version information and exit"));
#ifdef ENABLE_COLOR
    print1opt(_("-Y [str]"), _("--syntax=[str]"),
	N_("Syntax definition to use"));
#endif
    print1opt("-c", "--const", N_("Constantly show cursor position"));
    print1opt("-d", "--rebinddelete",
	N_("Fix Backspace/Delete confusion problem"));
#ifndef NANO_SMALL
    print1opt("-i", "--autoindent",
	N_("Automatically indent new lines"));
    print1opt("-k", "--cut", N_("Cut from cursor to end of line"));
#endif
    print1opt("-l", "--nofollow",
	N_("Don't follow symbolic links, overwrite"));
#ifndef DISABLE_MOUSE
    print1opt("-m", "--mouse", N_("Enable mouse"));
#endif
#ifndef DISABLE_OPERATINGDIR
    print1opt(_("-o [dir]"), _("--operatingdir=[dir]"),
	N_("Set operating directory"));
#endif
    print1opt("-p", "--preserve",
	N_("Preserve XON (^Q) and XOFF (^S) keys"));
#ifndef DISABLE_WRAPJUSTIFY
    print1opt(_("-r [#cols]"), _("--fill=[#cols]"),
	N_("Set fill cols to (wrap lines at) #cols"));
#endif
#ifndef DISABLE_SPELLER
    print1opt(_("-s [prog]"), _("--speller=[prog]"),
	N_("Enable alternate speller"));
#endif
    print1opt("-t", "--tempfile",
	N_("Auto save on exit, don't prompt"));
    print1opt("-v", "--view", N_("View (read only) mode"));
#ifndef DISABLE_WRAPPING
    print1opt("-w", "--nowrap", N_("Don't wrap long lines"));
#endif
    print1opt("-x", "--nohelp", N_("Don't show help window"));
    print1opt("-z", "--suspend", N_("Enable suspend"));

    /* This is a special case. */
    print1opt("-a, -b, -e,", "", NULL);
    print1opt("-f, -g, -j", "", N_("(ignored, for Pico compatibility)"));

    exit(0);
}

void version(void)
{
    printf(_(" GNU nano version %s (compiled %s, %s)\n"), VERSION,
	__TIME__, __DATE__);
    printf(
	_(" Email: nano@nano-editor.org	Web: http://www.nano-editor.org/"));
    printf(_("\n Compiled options:"));

#ifdef DISABLE_BROWSER
    printf(" --disable-browser");
#endif
#ifdef DISABLE_HELP
    printf(" --disable-help");
#endif
#ifdef DISABLE_JUSTIFY
    printf(" --disable-justify");
#endif
#ifdef DISABLE_MOUSE
    printf(" --disable-mouse");
#endif
#ifndef ENABLE_NLS
    printf(" --disable-nls");
#endif
#ifdef DISABLE_OPERATINGDIR
    printf(" --disable-operatingdir");
#endif
#ifdef DISABLE_SPELLER
    printf(" --disable-speller");
#endif
#ifdef DISABLE_TABCOMP
    printf(" --disable-tabcomp");
#endif
#ifdef DISABLE_WRAPPING
    printf(" --disable-wrapping");
#endif
#ifdef DISABLE_ROOTWRAP
    printf(" --disable-wrapping-as-root");
#endif
#ifdef ENABLE_COLOR
    printf(" --enable-color");
#endif
#ifdef DEBUG
    printf(" --enable-debug");
#endif
#ifdef NANO_EXTRA
    printf(" --enable-extra");
#endif
#ifdef ENABLE_MULTIBUFFER
    printf(" --enable-multibuffer");
#endif
#ifdef ENABLE_NANORC
    printf(" --enable-nanorc");
#endif
#ifdef NANO_SMALL
    printf(" --enable-tiny");
#endif
#ifdef ENABLE_UTF8
    printf(" --enable-utf8");
#endif
#ifdef USE_SLANG
    printf(" --with-slang");
#endif
    printf("\n");
}

int no_more_space(void)
{
    return ISSET(MORE_SPACE) ? 1 : 0;
}

int no_help(void)
{
    return ISSET(NO_HELP) ? 2 : 0;
}

void nano_disabled_msg(void)
{
    statusbar(_("Sorry, support for this function has been disabled"));
}

#ifndef NANO_SMALL
void cancel_command(int signal)
{
    if (kill(pid, SIGKILL) == -1)
	nperror("kill");
}

/* Return TRUE on success. */
bool execute_command(const char *command)
{
    int fd[2];
    FILE *f;
    struct sigaction oldaction, newaction;
	/* Original and temporary handlers for SIGINT. */
    bool sig_failed = FALSE;
	/* Did sigaction() fail without changing the signal handlers? */

    /* Make our pipes. */
    if (pipe(fd) == -1) {
	statusbar(_("Could not pipe"));
	return FALSE;
    }

    /* Fork a child. */
    if ((pid = fork()) == 0) {
	close(fd[0]);
	dup2(fd[1], fileno(stdout));
	dup2(fd[1], fileno(stderr));

	/* If execl() returns at all, there was an error. */
	execl("/bin/sh", "sh", "-c", command, NULL);
	exit(0);
    }

    /* Else continue as parent. */
    close(fd[1]);

    if (pid == -1) {
	close(fd[0]);
	statusbar(_("Could not fork"));
	return FALSE;
    }

    /* Before we start reading the forked command's output, we set
     * things up so that Ctrl-C will cancel the new process. */

    /* Enable interpretation of the special control keys so that we get
     * SIGINT when Ctrl-C is pressed. */
    enable_signals();

    if (sigaction(SIGINT, NULL, &newaction) == -1) {
	sig_failed = TRUE;
	nperror("sigaction");
    } else {
	newaction.sa_handler = cancel_command;
	if (sigaction(SIGINT, &newaction, &oldaction) == -1) {
	    sig_failed = TRUE;
	    nperror("sigaction");
	}
    }
    /* Note that now oldaction is the previous SIGINT signal handler,
     * to be restored later. */

    f = fdopen(fd[0], "rb");
    if (f == NULL)
	nperror("fdopen");

    read_file(f, "stdin");

    /* If multibuffer mode is on, we could be here in view mode.  If so,
     * don't set the modification flag. */
    if (!ISSET(VIEW_MODE))
	set_modified();

    if (wait(NULL) == -1)
	nperror("wait");

    if (!sig_failed && sigaction(SIGINT, &oldaction, NULL) == -1)
	nperror("sigaction");

    /* Disable interpretation of the special control keys so that we can
     * use Ctrl-C for other things. */
    disable_signals();

    return TRUE;
}
#endif /* !NANO_SMALL */

void do_verbatim_input(void)
{
    int *kbinput;
    size_t kbinput_len, i;
    char *output;

    statusbar(_("Verbatim Input"));

    /* If constant cursor position display is on, make sure the current
     * cursor position will be properly displayed on the statusbar. */
    if (ISSET(CONST_UPDATE))
	do_cursorpos(TRUE);

    /* Read in all the verbatim characters. */
    kbinput = get_verbatim_kbinput(edit, &kbinput_len);

    /* Display all the verbatim characters at once, not filtering out
     * control characters. */
    output = charalloc(kbinput_len + 1);

    for (i = 0; i < kbinput_len; i++)
	output[i] = (char)kbinput[i];
    output[i] = '\0';

    do_output(output, kbinput_len, TRUE);

    free(output);
}

void do_backspace(void)
{
    if (openfile->current != openfile->fileage ||
	openfile->current_x > 0) {
	do_left(FALSE);
	do_delete();
    }
}

void do_delete(void)
{
    bool do_refresh = FALSE;
	/* Do we have to call edit_refresh(), or can we get away with
	 * update_line()? */

    assert(openfile->current != NULL && openfile->current->data != NULL && openfile->current_x <= strlen(openfile->current->data));

    openfile->placewewant = xplustabs();

    if (openfile->current->data[openfile->current_x] != '\0') {
	int char_buf_len = parse_mbchar(openfile->current->data +
		openfile->current_x, NULL, NULL, NULL);
	size_t line_len = strlen(openfile->current->data +
		openfile->current_x);

	assert(openfile->current_x < strlen(openfile->current->data));

	/* Let's get dangerous. */
	charmove(&openfile->current->data[openfile->current_x],
		&openfile->current->data[openfile->current_x +
		char_buf_len], line_len - char_buf_len + 1);

	null_at(&openfile->current->data, openfile->current_x +
		line_len - char_buf_len);
#ifndef NANO_SMALL
	if (openfile->mark_set && openfile->mark_begin ==
		openfile->current && openfile->current_x <
		openfile->mark_begin_x)
	    openfile->mark_begin_x -= char_buf_len;
#endif
	openfile->totsize--;
    } else if (openfile->current != openfile->filebot &&
	(openfile->current->next != openfile->filebot ||
	openfile->current->data[0] == '\0')) {
	/* We can delete the line before filebot only if it is blank: it
	 * becomes the new magicline then. */
	filestruct *foo = openfile->current->next;

	assert(openfile->current_x == strlen(openfile->current->data));

	/* If we're deleting at the end of a line, we need to call
	 * edit_refresh(). */
	if (openfile->current->data[openfile->current_x] == '\0')
	    do_refresh = TRUE;

	openfile->current->data = charealloc(openfile->current->data,
		openfile->current_x + strlen(foo->data) + 1);
	strcpy(openfile->current->data + openfile->current_x,
		foo->data);
#ifndef NANO_SMALL
	if (openfile->mark_set && openfile->mark_begin ==
		openfile->current->next) {
	    openfile->mark_begin = openfile->current;
	    openfile->mark_begin_x += openfile->current_x;
	}
#endif
	if (openfile->filebot == foo)
	    openfile->filebot = openfile->current;

	unlink_node(foo);
	delete_node(foo);
	renumber(openfile->current);
	openfile->totlines--;
	openfile->totsize--;
#ifndef DISABLE_WRAPPING
	wrap_reset();
#endif
    } else
	return;

    set_modified();

#ifdef ENABLE_COLOR
    /* If color syntaxes are available and turned on, we need to call
     * edit_refresh(). */
    if (openfile->colorstrings != NULL && !ISSET(NO_COLOR_SYNTAX))
	do_refresh = TRUE;
#endif

    if (do_refresh)
	edit_refresh();
    else
	update_line(openfile->current, openfile->current_x);
}

void do_tab(void)
{
#ifndef NANO_SMALL
    if (ISSET(TABS_TO_SPACES)) {
	char *output;
	size_t output_len = 0, new_pww = openfile->placewewant;

	do {
	    new_pww++;
	    output_len++;
	} while (new_pww % tabsize != 0);

	output = charalloc(output_len + 1);

	charset(output, ' ', output_len);
	output[output_len] = '\0';

	do_output(output, output_len, TRUE);

	free(output);
    } else {
#endif
	do_output("\t", 1, TRUE);
#ifndef NANO_SMALL
    }
#endif
}

/* Someone hits Return *gasp!* */
void do_enter(void)
{
    filestruct *newnode = make_new_node(openfile->current);
    size_t extra = 0;

    assert(openfile->current != NULL && openfile->current->data != NULL);

#ifndef NANO_SMALL
    /* Do auto-indenting, like the neolithic Turbo Pascal editor. */
    if (ISSET(AUTOINDENT)) {
	/* If we are breaking the line in the indentation, the new
	 * indentation should have only current_x characters, and
	 * current_x should not change. */
	extra = indent_length(openfile->current->data);
	if (extra > openfile->current_x)
	    extra = openfile->current_x;
    }
#endif
    newnode->data = charalloc(strlen(openfile->current->data +
	openfile->current_x) + extra + 1);
    strcpy(&newnode->data[extra], openfile->current->data +
	openfile->current_x);
#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	strncpy(newnode->data, openfile->current->data, extra);
	openfile->totsize += mbstrlen(newnode->data);
    }
#endif
    null_at(&openfile->current->data, openfile->current_x);
#ifndef NANO_SMALL
    if (openfile->mark_set && openfile->current ==
	openfile->mark_begin && openfile->current_x <
	openfile->mark_begin_x) {
	openfile->mark_begin = newnode;
	openfile->mark_begin_x += extra - openfile->current_x;
    }
#endif
    openfile->current_x = extra;

    if (openfile->current == openfile->filebot)
	openfile->filebot = newnode;
    splice_node(openfile->current, newnode,
	openfile->current->next);

    renumber(openfile->current);
    openfile->current = newnode;

    edit_refresh();

    openfile->totlines++;
    openfile->totsize++;
    set_modified();
    openfile->placewewant = xplustabs();
}

#ifndef NANO_SMALL
/* Move to the next word in the current filestruct.  If allow_punct is
 * TRUE, treat punctuation as part of a word.  If allow_update is TRUE,
 * update the screen afterward.  Return TRUE if we started on a word,
 * and FALSE otherwise. */
bool do_next_word(bool allow_punct, bool allow_update)
{
    size_t pww_save = openfile->placewewant;
    const filestruct *current_save = openfile->current;
    char *char_mb;
    int char_mb_len;
    bool end_line = FALSE, started_on_word = FALSE;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    char_mb = charalloc(mb_cur_max());

    /* Move forward until we find the character after the last letter of
     * the current word. */
    while (!end_line) {
	char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL, NULL);

	/* If we've found it, stop moving forward through the current
	 * line. */
	if (!is_word_mbchar(char_mb, allow_punct))
	    break;

	/* If we haven't found it, then we've started on a word, so set
	 * started_on_word to TRUE. */
	started_on_word = TRUE;

	if (openfile->current->data[openfile->current_x] == '\0')
	    end_line = TRUE;
	else
	    openfile->current_x += char_mb_len;
    }

    /* Move forward until we find the first letter of the next word. */
    if (openfile->current->data[openfile->current_x] == '\0')
	end_line = TRUE;
    else
	openfile->current_x += char_mb_len;

    for (; openfile->current != NULL;
	openfile->current = openfile->current->next) {
	while (!end_line) {
	    char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL, NULL);

	    /* If we've found it, stop moving forward through the
	     * current line. */
	    if (is_word_mbchar(char_mb, allow_punct))
		break;

	    if (openfile->current->data[openfile->current_x] == '\0')
		end_line = TRUE;
	    else
		openfile->current_x += char_mb_len;
	}

	/* If we've found it, stop moving forward to the beginnings of
	 * subsequent lines. */
	if (!end_line)
	    break;

	if (openfile->current->next != NULL) {
	    end_line = FALSE;
	    openfile->current_x = 0;
	}
    }

    free(char_mb);

    /* If we haven't found it, leave the cursor at the end of the
     * file. */
    if (openfile->current == NULL)
	openfile->current = openfile->filebot;

    openfile->placewewant = xplustabs();

    /* If allow_update is TRUE, update the screen. */
    if (allow_update)
	edit_redraw(current_save, pww_save);

    /* Return whether we started on a word. */
    return started_on_word;
}

/* Move to the next word in the current filestruct, not counting
 * punctuation as part of a word, and updating the screen afterward. */
void do_next_word_void(void)
{
    do_next_word(FALSE, TRUE);
}

/* Move to the previous word in the current filestruct.  If allow_punct
 * is TRUE, treat punctuation as part of a word.  If allow_update is
 * TRUE, update the screen afterward.  Return TRUE if we started on a
 * word, and FALSE otherwise. */
bool do_prev_word(bool allow_punct, bool allow_update)
{
    size_t pww_save = openfile->placewewant;
    const filestruct *current_save = openfile->current;
    char *char_mb;
    int char_mb_len;
    bool begin_line = FALSE, started_on_word = FALSE;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    char_mb = charalloc(mb_cur_max());

    /* Move backward until we find the character before the first letter
     * of the current word. */
    while (!begin_line) {
	char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL, NULL);

	/* If we've found it, stop moving backward through the current
	 * line. */
	if (!is_word_mbchar(char_mb, allow_punct))
	    break;

	/* If we haven't found it, then we've started on a word, so set
	 * started_on_word to TRUE. */
	started_on_word = TRUE;

	if (openfile->current_x == 0)
	    begin_line = TRUE;
	else
	    openfile->current_x = move_mbleft(openfile->current->data,
		openfile->current_x);
    }

    /* Move backward until we find the last letter of the previous
     * word. */
    if (openfile->current_x == 0)
	begin_line = TRUE;
    else
	openfile->current_x = move_mbleft(openfile->current->data,
		openfile->current_x);

    for (; openfile->current != NULL;
	openfile->current = openfile->current->prev) {
	while (!begin_line) {
	    char_mb_len = parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL, NULL);

	    /* If we've found it, stop moving backward through the
	     * current line. */
	    if (is_word_mbchar(char_mb, allow_punct))
		break;

	    if (openfile->current_x == 0)
		begin_line = TRUE;
	    else
		openfile->current_x =
			move_mbleft(openfile->current->data,
			openfile->current_x);
	}

	/* If we've found it, stop moving backward to the ends of
	 * previous lines. */
	if (!begin_line)
	    break;

	if (openfile->current->prev != NULL) {
	    begin_line = FALSE;
	    openfile->current_x = strlen(openfile->current->prev->data);
	}
    }

    /* If we haven't found it, leave the cursor at the beginning of the
     * file. */
    if (openfile->current == NULL)
	openfile->current = openfile->fileage;
    /* If we've found it, move backward until we find the character
     * before the first letter of the previous word. */
    else if (!begin_line) {
	if (openfile->current_x == 0)
	    begin_line = TRUE;
	else
	    openfile->current_x = move_mbleft(openfile->current->data,
		openfile->current_x);

	while (!begin_line) {
	    char_mb_len =
		parse_mbchar(openfile->current->data +
		openfile->current_x, char_mb, NULL, NULL);

	    /* If we've found it, stop moving backward through the
	     * current line. */
	    if (!is_word_mbchar(char_mb, allow_punct))
		break;

	    if (openfile->current_x == 0)
		begin_line = TRUE;
	    else
		openfile->current_x =
			move_mbleft(openfile->current->data,
			openfile->current_x);
	}

	/* If we've found it, move forward to the first letter of the
	 * previous word. */
	if (!begin_line)
	    openfile->current_x += char_mb_len;
    }

    free(char_mb);

    openfile->placewewant = xplustabs();

    /* If allow_update is TRUE, update the screen. */
    if (allow_update)
	edit_redraw(current_save, pww_save);

    /* Return whether we started on a word. */
    return started_on_word;
}

/* Move to the previous word in the current filestruct, not counting
 * punctuation as part of a word, and updating the screen afterward. */
void do_prev_word_void(void)
{
    do_prev_word(FALSE, TRUE);
}

void do_word_count(void)
{
    size_t words = 0, current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    filestruct *current_save = openfile->current;
    bool old_mark_set = openfile->mark_set;
    bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
    filestruct *top, *bot;
    size_t top_x, bot_x;

    if (old_mark_set) {
	/* If the mark is on, partition the filestruct so that it
	 * contains only the marked text, keep track of whether the text
	 * will need a magicline added while we're counting words, add
	 * the magicline if necessary, and turn the mark off. */
	mark_order((const filestruct **)&top, &top_x,
	    (const filestruct **)&bot, &bot_x, NULL);
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	if ((added_magicline = (openfile->filebot->data[0] != '\0')))
	    new_magicline();
	openfile->mark_set = FALSE;
    }

    /* Start at the top of the file. */
    openfile->current = openfile->fileage;
    openfile->current_x = 0;
    openfile->placewewant = 0;

    /* Keep moving to the next word (counting punctuation characters as
     * part of a word so that we match the output of "wc -w"), without
     * updating the screen, until we reach the end of the file,
     * incrementing the total word count whenever we're on a word just
     * before moving. */
    while (openfile->current != openfile->filebot ||
	openfile->current_x != 0) {
	if (do_next_word(TRUE, FALSE))
	    words++;
    }

    if (old_mark_set) {
	/* If the mark was on and we added a magicline, remove it
	 * now. */
	if (added_magicline)
	    remove_magicline();

	/* Unpartition the filestruct so that it contains all the text
	 * again, and turn the mark back on. */
	unpartition_filestruct(&filepart);
	openfile->mark_set = TRUE;
    }

    /* Restore where we were. */
    openfile->current = current_save;
    openfile->current_x = current_x_save;
    openfile->placewewant = pww_save;

    /* Display the total word count on the statusbar. */
    statusbar("%s: %lu", old_mark_set ? _("Word Count in Selection") :
	_("Word Count"), (unsigned long)words);
}

void do_mark(void)
{
    openfile->mark_set = !openfile->mark_set;
    if (openfile->mark_set) {
	statusbar(_("Mark Set"));
	openfile->mark_begin = openfile->current;
	openfile->mark_begin_x = openfile->current_x;
    } else {
	statusbar(_("Mark UNset"));
	openfile->mark_begin = NULL;
	openfile->mark_begin_x = 0;
	edit_refresh();
    }
}
#endif /* !NANO_SMALL */

#ifndef DISABLE_WRAPPING
void wrap_reset(void)
{
    same_line_wrap = FALSE;
}
#endif

#ifndef DISABLE_WRAPPING
/* We wrap the given line.  Precondition: we assume the cursor has been
 * moved forward since the last typed character.  Return value: whether
 * we wrapped. */
bool do_wrap(filestruct *line)
{
    size_t line_len;
	/* Length of the line we wrap. */
    ssize_t wrap_loc;
	/* Index of line->data where we wrap. */
#ifndef NANO_SMALL
    const char *indent_string = NULL;
	/* Indentation to prepend to the new line. */
    size_t indent_len = 0;	/* The length of indent_string. */
#endif
    const char *after_break;	/* The text after the wrap point. */
    size_t after_break_len;	/* The length of after_break. */
    bool wrapping = FALSE;	/* Do we prepend to the next line? */
    const char *next_line = NULL;
	/* The next line, minus indentation. */
    size_t next_line_len = 0;	/* The length of next_line. */
    char *new_line = NULL;	/* The line we create. */
    size_t new_line_len = 0;	/* The eventual length of new_line. */

    /* There are three steps.  First, we decide where to wrap.  Then, we
     * create the new wrap line.  Finally, we clean up. */

    /* Step 1, finding where to wrap.  We are going to add a new line
     * after a blank character.  In this step, we call break_line() to
     * get the location of the last blank we can break the line at, and
     * and set wrap_loc to the location of the character after it, so
     * that the blank is preserved at the end of the line.
     *
     * If there is no legal wrap point, or we reach the last character
     * of the line while trying to find one, we should return without
     * wrapping.  Note that if autoindent is turned on, we don't break
     * at the end of it! */

    assert(line != NULL && line->data != NULL);

    /* Save the length of the line. */
    line_len = strlen(line->data);

    /* Find the last blank where we can break the line. */
    wrap_loc = break_line(line->data, fill, FALSE);

    /* If we couldn't break the line, or we've reached the end of it, we
     * don't wrap. */
    if (wrap_loc == -1 || line->data[wrap_loc] == '\0')
	return FALSE;

    /* Otherwise, move forward to the character just after the blank. */
    wrap_loc += move_mbright(line->data + wrap_loc, 0);

    /* If we've reached the end of the line, we don't wrap. */
    if (line->data[wrap_loc] == '\0')
	return FALSE;

#ifndef NANO_SMALL
    /* If autoindent is turned on, and we're on the character just after
     * the indentation, we don't wrap. */
    if (ISSET(AUTOINDENT)) {
	/* Get the indentation of this line. */
	indent_string = line->data;
	indent_len = indent_length(indent_string);

	if (wrap_loc == indent_len)
	    return FALSE;
    }
#endif

    /* Step 2, making the new wrap line.  It will consist of indentation
     * followed by the text after the wrap point, optionally followed by
     * a space (if the text after the wrap point doesn't end in a blank)
     * and the text of the next line, if they can fit without
     * wrapping, the next line exists, and the same_line_wrap flag is
     * set. */

    /* after_break is the text that will be wrapped to the next line. */
    after_break = line->data + wrap_loc;
    after_break_len = line_len - wrap_loc;

    assert(strlen(after_break) == after_break_len);

    /* We prepend the wrapped text to the next line, if the
     * same_line_wrap flag is set, there is a next line, and prepending
     * would not make the line too long. */
    if (same_line_wrap && line->next != NULL) {
	const char *end = after_break + move_mbleft(after_break,
		after_break_len);

	/* If after_break doesn't end in a blank, make sure it ends in a
	 * space. */
	if (!is_blank_mbchar(end)) {
	    line_len++;
	    line->data = charealloc(line->data, line_len + 1);
	    line->data[line_len - 1] = ' ';
	    line->data[line_len] = '\0';
	    after_break = line->data + wrap_loc;
	    after_break_len++;
	    openfile->totsize++;
	}

	next_line = line->next->data;
	next_line_len = strlen(next_line);

	if (after_break_len + next_line_len <= fill) {
	    wrapping = TRUE;
	    new_line_len += next_line_len;
	}
    }

    /* new_line_len is now the length of the text that will be wrapped
     * to the next line, plus (if we're prepending to it) the length of
     * the text of the next line. */
    new_line_len += after_break_len;

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	if (wrapping) {
	    /* If we're wrapping, the indentation will come from the
	     * next line. */
	    indent_string = next_line;
	    indent_len = indent_length(indent_string);
	    next_line += indent_len;
	} else {
	    /* Otherwise, it will come from this line, in which case
	     * we should increase new_line_len to make room for it. */
	    new_line_len += indent_len;
	    openfile->totsize += mbstrnlen(indent_string, indent_len);
	}
    }
#endif

    /* Now we allocate the new line and copy the text into it. */
    new_line = charalloc(new_line_len + 1);
    new_line[0] = '\0';

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	/* Copy the indentation. */
	strncpy(new_line, indent_string, indent_len);
	new_line[indent_len] = '\0';
	new_line_len += indent_len;
    }
#endif

    /* Copy all the text after the wrap point of the current line. */
    strcat(new_line, after_break);

    /* Break the current line at the wrap point. */
    null_at(&line->data, wrap_loc);

    if (wrapping) {
	/* If we're wrapping, copy the text from the next line, minus
	 * the indentation that we already copied above. */
	strcat(new_line, next_line);

	free(line->next->data);
	line->next->data = new_line;
    } else {
	/* Otherwise, make a new line and copy the text after where we
	 * broke this line to the beginning of the new line. */
	splice_node(openfile->current, make_new_node(openfile->current),
		openfile->current->next);

	openfile->current->next->data = new_line;

	openfile->totlines++;
	openfile->totsize++;
    }

    /* Step 3, clean up.  Reposition the cursor and mark, and do some
     * other sundry things. */

    /* Set the same_line_wrap flag, so that later wraps of this line
     * will be prepended to the next line. */
    same_line_wrap = TRUE;

    /* Each line knows its line number.  We recalculate these if we
     * inserted a new line. */
    if (!wrapping)
	renumber(line);

    /* If the cursor was after the break point, we must move it.  We
     * also clear the same_line_wrap flag in this case. */
    if (openfile->current_x > wrap_loc) {
	same_line_wrap = FALSE;
	openfile->current = openfile->current->next;
	openfile->current_x -= wrap_loc
#ifndef NANO_SMALL
		- indent_len
#endif
		;
	openfile->placewewant = xplustabs();
    }

#ifndef NANO_SMALL
    /* If the mark was on this line after the wrap point, we move it
     * down.  If it was on the next line and we wrapped onto that line,
     * we move it right. */
    if (openfile->mark_set) {
	if (openfile->mark_begin == line && openfile->mark_begin_x >
		wrap_loc) {
	    openfile->mark_begin = line->next;
	    openfile->mark_begin_x -= wrap_loc - indent_len + 1;
	} else if (wrapping && openfile->mark_begin == line->next)
	    openfile->mark_begin_x += after_break_len;
    }
#endif

    return TRUE;
}
#endif /* !DISABLE_WRAPPING */

#ifndef DISABLE_SPELLER
/* A word is misspelled in the file.  Let the user replace it.  We
 * return FALSE if the user cancels. */
bool do_int_spell_fix(const char *word)
{
    char *save_search, *save_replace;
    size_t match_len, current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    filestruct *edittop_save = openfile->edittop;
    filestruct *current_save = openfile->current;
	/* Save where we are. */
    bool canceled = FALSE;
	/* The return value. */
    bool case_sens_set = ISSET(CASE_SENSITIVE);
#ifndef NANO_SMALL
    bool backwards_search_set = ISSET(BACKWARDS_SEARCH);
#endif
#ifdef HAVE_REGEX_H
    bool regexp_set = ISSET(USE_REGEXP);
#endif
#ifndef NANO_SMALL
    bool old_mark_set = openfile->mark_set;
    bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
    bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */
    filestruct *top, *bot;
    size_t top_x, bot_x;
#endif

    /* Make sure spell-check is case sensitive. */
    SET(CASE_SENSITIVE);

#ifndef NANO_SMALL
    /* Make sure spell-check goes forward only. */
    UNSET(BACKWARDS_SEARCH);
#endif
#ifdef HAVE_REGEX_H
    /* Make sure spell-check doesn't use regular expressions. */
    UNSET(USE_REGEXP);
#endif

    /* Save the current search/replace strings. */
    search_init_globals();
    save_search = last_search;
    save_replace = last_replace;

    /* Set the search/replace strings to the misspelled word. */
    last_search = mallocstrcpy(NULL, word);
    last_replace = mallocstrcpy(NULL, word);

#ifndef NANO_SMALL
    if (old_mark_set) {
	/* If the mark is on, partition the filestruct so that it
	 * contains only the marked text, keep track of whether the text
	 * will have a magicline added when we're done correcting
	 * misspelled words, and turn the mark off. */
	mark_order((const filestruct **)&top, &top_x,
	    (const filestruct **)&bot, &bot_x, &right_side_up);
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	added_magicline = (openfile->filebot->data[0] != '\0');
	openfile->mark_set = FALSE;
    }
#endif

    /* Start from the top of the file. */
    openfile->edittop = openfile->fileage;
    openfile->current = openfile->fileage;
    openfile->current_x = (size_t)-1;
    openfile->placewewant = 0;

    /* Find the first whole-word occurrence of word. */
    findnextstr_wrap_reset();
    while (findnextstr(TRUE, TRUE, FALSE, openfile->fileage, 0, word,
	&match_len)) {
	if (is_whole_word(openfile->current_x, openfile->current->data,
		word)) {
	    size_t xpt = xplustabs();
	    char *exp_word = display_string(openfile->current->data,
		xpt, strnlenpt(openfile->current->data,
		openfile->current_x + match_len) - xpt, FALSE);

	    edit_refresh();

	    do_replace_highlight(TRUE, exp_word);

	    /* Allow all instances of the word to be corrected. */
	    canceled = (statusq(FALSE, spell_list, word,
#ifndef NANO_SMALL
			NULL,
#endif
			 _("Edit a replacement")) == -1);

	    do_replace_highlight(FALSE, exp_word);

	    free(exp_word);

	    if (!canceled && strcmp(word, answer) != 0) {
		openfile->current_x--;
		do_replace_loop(word, openfile->current,
			&openfile->current_x, TRUE, &canceled);
	    }

	    break;
	}
    }

#ifndef NANO_SMALL
    if (old_mark_set) {
	/* If the mark was on and we added a magicline, remove it
	 * now. */
	if (added_magicline)
	    remove_magicline();

	/* Put the beginning and the end of the mark at the beginning
	 * and the end of the spell-checked text. */
	if (openfile->fileage == openfile->filebot)
	    bot_x += top_x;
	if (right_side_up) {
	    openfile->mark_begin_x = top_x;
	    current_x_save = bot_x;
	} else {
	    current_x_save = top_x;
	    openfile->mark_begin_x = bot_x;
	}

	/* Unpartition the filestruct so that it contains all the text
	 * again, and turn the mark back on. */
	unpartition_filestruct(&filepart);
	openfile->mark_set = TRUE;
    }
#endif

    /* Restore the search/replace strings. */
    free(last_search);
    last_search = save_search;
    free(last_replace);
    last_replace = save_replace;

    /* Restore where we were. */
    openfile->edittop = edittop_save;
    openfile->current = current_save;
    openfile->current_x = current_x_save;
    openfile->placewewant = pww_save;

    /* Restore case sensitivity setting. */
    if (!case_sens_set)
	UNSET(CASE_SENSITIVE);

#ifndef NANO_SMALL
    /* Restore search/replace direction. */
    if (backwards_search_set)
	SET(BACKWARDS_SEARCH);
#endif
#ifdef HAVE_REGEX_H
    /* Restore regular expression usage setting. */
    if (regexp_set)
	SET(USE_REGEXP);
#endif

    return !canceled;
}

/* Integrated spell checking using the spell program, filtered through
 * the sort and uniq programs.  Return NULL for normal termination,
 * and the error string otherwise. */
const char *do_int_speller(const char *tempfile_name)
{
    char *read_buff, *read_buff_ptr, *read_buff_word;
    size_t pipe_buff_size, read_buff_size, read_buff_read, bytesread;
    int spell_fd[2], sort_fd[2], uniq_fd[2], tempfile_fd = -1;
    pid_t pid_spell, pid_sort, pid_uniq;
    int spell_status, sort_status, uniq_status;

    /* Create all three pipes up front. */
    if (pipe(spell_fd) == -1 || pipe(sort_fd) == -1 ||
	pipe(uniq_fd) == -1)
	return _("Could not create pipe");

    statusbar(_("Creating misspelled word list, please wait..."));

    /* A new process to run spell in. */
    if ((pid_spell = fork()) == 0) {
	/* Child continues (i.e, future spell process). */
	close(spell_fd[0]);

	/* Replace the standard input with the temp file. */
	if ((tempfile_fd = open(tempfile_name, O_RDONLY)) == -1)
	    goto close_pipes_and_exit;

	if (dup2(tempfile_fd, STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(tempfile_fd);

	/* Send spell's standard output to the pipe. */
	if (dup2(spell_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(spell_fd[1]);

	/* Start the spell program; we are using PATH. */
	execlp("spell", "spell", NULL);

	/* This should not be reached if spell is found. */
	exit(1);
    }

    /* Parent continues here. */
    close(spell_fd[1]);

    /* A new process to run sort in. */
    if ((pid_sort = fork()) == 0) {
	/* Child continues (i.e, future spell process).  Replace the
	 * standard input with the standard output of the old pipe. */
	if (dup2(spell_fd[0], STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(spell_fd[0]);

	/* Send sort's standard output to the new pipe. */
	if (dup2(sort_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(sort_fd[1]);

	/* Start the sort program.  Use -f to remove mixed case.  If
	 * this isn't portable, let me know. */
	execlp("sort", "sort", "-f", NULL);

	/* This should not be reached if sort is found. */
	exit(1);
    }

    close(spell_fd[0]);
    close(sort_fd[1]);

    /* A new process to run uniq in. */
    if ((pid_uniq = fork()) == 0) {
	/* Child continues (i.e, future uniq process).  Replace the
	 * standard input with the standard output of the old pipe. */
	if (dup2(sort_fd[0], STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(sort_fd[0]);

	/* Send uniq's standard output to the new pipe. */
	if (dup2(uniq_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(uniq_fd[1]);

	/* Start the uniq program; we are using PATH. */
	execlp("uniq", "uniq", NULL);

	/* This should not be reached if uniq is found. */
	exit(1);
    }

    close(sort_fd[0]);
    close(uniq_fd[1]);

    /* The child process was not forked successfully. */
    if (pid_spell < 0 || pid_sort < 0 || pid_uniq < 0) {
	close(uniq_fd[0]);
	return _("Could not fork");
    }

    /* Get the system pipe buffer size. */
    if ((pipe_buff_size = fpathconf(uniq_fd[0], _PC_PIPE_BUF)) < 1) {
	close(uniq_fd[0]);
	return _("Could not get size of pipe buffer");
    }

    /* Read in the returned spelling errors. */
    read_buff_read = 0;
    read_buff_size = pipe_buff_size + 1;
    read_buff = read_buff_ptr = charalloc(read_buff_size);

    while ((bytesread = read(uniq_fd[0], read_buff_ptr,
	pipe_buff_size)) > 0) {
	read_buff_read += bytesread;
	read_buff_size += pipe_buff_size;
	read_buff = read_buff_ptr = charealloc(read_buff,
		read_buff_size);
	read_buff_ptr += read_buff_read;
    }

    *read_buff_ptr = '\0';
    close(uniq_fd[0]);

    /* Process the spelling errors. */
    read_buff_word = read_buff_ptr = read_buff;

    while (*read_buff_ptr != '\0') {
	if ((*read_buff_ptr == '\r') || (*read_buff_ptr == '\n')) {
	    *read_buff_ptr = '\0';
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

    /* Special case: the last word doesn't end with '\r' or '\n'. */
    if (read_buff_word != read_buff_ptr)
	do_int_spell_fix(read_buff_word);

    free(read_buff);
    replace_abort();
    edit_refresh();

    /* Process the end of the spell process. */
    waitpid(pid_spell, &spell_status, 0);
    waitpid(pid_sort, &sort_status, 0);
    waitpid(pid_uniq, &uniq_status, 0);

    if (WIFEXITED(spell_status) == 0 || WEXITSTATUS(spell_status))
	return _("Error invoking \"spell\"");

    if (WIFEXITED(sort_status)  == 0 || WEXITSTATUS(sort_status))
	return _("Error invoking \"sort -f\"");

    if (WIFEXITED(uniq_status) == 0 || WEXITSTATUS(uniq_status))
	return _("Error invoking \"uniq\"");

    /* Otherwise... */
    return NULL;

  close_pipes_and_exit:
    /* Don't leak any handles. */
    close(tempfile_fd);
    close(spell_fd[0]);
    close(spell_fd[1]);
    close(sort_fd[0]);
    close(sort_fd[1]);
    close(uniq_fd[0]);
    close(uniq_fd[1]);
    exit(1);
}

/* External spell checking.  Return value: NULL for normal termination,
 * otherwise the error string. */
const char *do_alt_speller(char *tempfile_name)
{
    int alt_spell_status;
    char *filename_save;
    size_t current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    ssize_t current_y_save = openfile->current_y;
    ssize_t lineno_save = openfile->current->lineno;
    pid_t pid_spell;
    char *ptr;
    static int arglen = 3;
    static char **spellargs = NULL;
    FILE *f;
#ifndef NANO_SMALL
    bool old_mark_set = openfile->mark_set;
    bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
    bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */
    filestruct *top, *bot;
    size_t top_x, bot_x;
    ssize_t mb_lineno_save = 0;
	/* We're going to close the current file, and open the output of
	 * the alternate spell command.  The line that mark_begin points
	 * to will be freed, so we save the line number and restore it
	 * afterwards. */
    size_t totsize_save = openfile->totsize;
	/* Our saved value of totsize, used when we spell-check a marked
	 * selection. */

    if (old_mark_set) {
	/* If the mark is on, save the number of the line it starts on,
	 * and then turn the mark off. */
	mb_lineno_save = openfile->mark_begin->lineno;
	openfile->mark_set = FALSE;
    }
#endif

    endwin();

    /* Set up an argument list to pass execvp(). */
    if (spellargs == NULL) {
	spellargs = (char **)nmalloc(arglen * sizeof(char *));

	spellargs[0] = strtok(alt_speller, " ");
	while ((ptr = strtok(NULL, " ")) != NULL) {
	    arglen++;
	    spellargs = (char **)nrealloc(spellargs, arglen *
		sizeof(char *));
	    spellargs[arglen - 3] = ptr;
	}
	spellargs[arglen - 1] = NULL;
    }
    spellargs[arglen - 2] = tempfile_name;

    /* Start a new process for the alternate speller. */
    if ((pid_spell = fork()) == 0) {
	/* Start alternate spell program; we are using PATH. */
	execvp(spellargs[0], spellargs);

	/* Should not be reached, if alternate speller is found!!! */
	exit(1);
    }

    /* Could not fork?? */
    if (pid_spell < 0)
	return _("Could not fork");

    /* Wait for alternate speller to complete. */
    wait(&alt_spell_status);

    refresh();

    /* Restore the terminal to its previous state. */
    terminal_init();

    /* Turn the cursor back on for sure. */
    curs_set(1);

    if (!WIFEXITED(alt_spell_status) ||
		WEXITSTATUS(alt_spell_status) != 0) {
	char *altspell_error;
	char *invoke_error = _("Error invoking \"%s\"");

#ifndef NANO_SMALL
	/* Turn the mark back on if it was on before. */
	openfile->mark_set = old_mark_set;
#endif

	altspell_error =
		charalloc(strlen(invoke_error) +
		strlen(alt_speller) + 1);
	sprintf(altspell_error, invoke_error, alt_speller);
	return altspell_error;
    }

#ifndef NANO_SMALL
    if (old_mark_set) {
	/* If the mark was on, partition the filestruct so that it
	 * contains only the marked text, and keep track of whether the
	 * temp file (which should contain the spell-checked marked
	 * text) will have a magicline added when it's reloaded. */
	mark_order((const filestruct **)&top, &top_x,
		(const filestruct **)&bot, &bot_x, &right_side_up);
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	added_magicline = (openfile->filebot->data[0] != '\0');

	/* Get the number of characters in the marked text, and subtract
	 * it from the saved value of totsize. */
	totsize_save -= get_totsize(top, bot);
    }
#endif

    /* Set up the window size. */
    window_size_init();

    /* Save the current filename. */
    filename_save = mallocstrcpy(NULL, openfile->filename);

    /* Reinitialize the current buffer. */
    reinitialize_buffer();

    /* Restore the current filename. */
    openfile->filename = filename_save;

    /* Reload the temp file.  Open it, read it into the current buffer,
     * and move back to the first line of the buffer. */
    open_file(tempfile_name, FALSE, &f);
    read_file(f, tempfile_name);
    openfile->current = openfile->fileage;

#ifndef NANO_SMALL
    if (old_mark_set) {
	filestruct *top_save = openfile->fileage;

	/* If the mark was on and we added a magicline, remove it
	 * now. */
	if (added_magicline)
	    remove_magicline();

	/* Put the beginning and the end of the mark at the beginning
	 * and the end of the spell-checked text. */
	if (openfile->fileage == openfile->filebot)
	    bot_x += top_x;
	if (right_side_up) {
	    openfile->mark_begin_x = top_x;
	    current_x_save = bot_x;
	} else {
	    current_x_save = top_x;
	    openfile->mark_begin_x = bot_x;
	}

	/* Unpartition the filestruct so that it contains all the text
	 * again.  Note that we've replaced the marked text originally
	 * in the partition with the spell-checked marked text in the
	 * temp file. */
	unpartition_filestruct(&filepart);

	/* Renumber starting with the beginning line of the old
	 * partition.  Also set totlines to the new number of lines in
	 * the file, add the number of characters in the spell-checked
	 * marked text to the saved value of totsize, and then make that
	 * saved value the actual value. */
	renumber(top_save);
	openfile->totlines = openfile->filebot->lineno;
	totsize_save += openfile->totsize;
	openfile->totsize = totsize_save;

	/* Assign mark_begin to the line where the mark began before. */
	do_gotopos(mb_lineno_save, openfile->mark_begin_x,
		current_y_save, 0);
	openfile->mark_begin = openfile->current;

	/* Assign mark_begin_x to the location in mark_begin where the
	 * mark began before, adjusted for any shortening of the
	 * line. */
	openfile->mark_begin_x = openfile->current_x;

	/* Turn the mark back on. */
	openfile->mark_set = TRUE;
    }
#endif

    /* Go back to the old position, and mark the file as modified. */
    do_gotopos(lineno_save, current_x_save, current_y_save, pww_save);
    set_modified();

    return NULL;
}

void do_spell(void)
{
    int i;
    FILE *temp_file;
    char *temp = safe_tempfile(&temp_file);
    const char *spell_msg;

    if (temp == NULL) {
	statusbar(_("Could not create temp file: %s"), strerror(errno));
	return;
    }

#ifndef NANO_SMALL
    if (openfile->mark_set)
	i = write_marked_file(temp, temp_file, TRUE, FALSE);
    else
#endif
	i = write_file(temp, temp_file, TRUE, FALSE, FALSE);

    if (i == -1) {
	statusbar(_("Error writing temp file: %s"), strerror(errno));
	free(temp);
	return;
    }

    spell_msg = (alt_speller != NULL) ? do_alt_speller(temp) :
	do_int_speller(temp);
    unlink(temp);
    free(temp);

    /* If the spell-checker printed any error messages onscreen, make
     * sure that they're cleared off. */
    total_refresh();

    if (spell_msg != NULL) {
	if (errno == 0)
	    /* Don't display an error message of "Success". */
	    statusbar(_("Spell checking failed: %s"), spell_msg);
	else
	    statusbar(_("Spell checking failed: %s: %s"), spell_msg,
		strerror(errno));
    } else
	statusbar(_("Finished checking spelling"));
}
#endif /* !DISABLE_SPELLER */

#if !defined(DISABLE_HELP) || !defined(DISABLE_JUSTIFY) || !defined(DISABLE_WRAPPING)
/* We are trying to break a chunk off line.  We find the last blank such
 * that the display length to there is at most goal + 1.  If there is no
 * such blank, then we find the first blank.  We then take the last
 * blank in that group of blanks.  The terminating '\0' counts as a
 * blank, as does a '\n' if newline is TRUE. */
ssize_t break_line(const char *line, ssize_t goal, bool newline)
{
    ssize_t blank_loc = -1;
	/* Current tentative return value.  Index of the last blank we
	 * found with short enough display width.  */
    ssize_t cur_loc = 0;
	/* Current index in line. */
    int line_len;

    assert(line != NULL);

    while (*line != '\0' && goal >= 0) {
	size_t pos = 0;

	line_len = parse_mbchar(line, NULL, NULL, &pos);

	if (is_blank_mbchar(line) || (newline && *line == '\n')) {
	    blank_loc = cur_loc;

	    if (newline && *line == '\n')
		break;
	}

	goal -= pos;
	line += line_len;
	cur_loc += line_len;
    }

    if (goal >= 0)
	/* In fact, the whole line displays shorter than goal. */
	return cur_loc;

    if (blank_loc == -1) {
	/* No blank was found that was short enough. */
	bool found_blank = FALSE;

	while (*line != '\0') {
	    line_len = parse_mbchar(line, NULL, NULL, NULL);

	    if (is_blank_mbchar(line) || (newline && *line == '\n')) {
		if (!found_blank)
		    found_blank = TRUE;
	    } else if (found_blank)
		return cur_loc - line_len;

	    line += line_len;
	    cur_loc += line_len;
	}

	return -1;
    }

    /* Move to the last blank after blank_loc, if there is one. */
    line -= cur_loc;
    line += blank_loc;
    line_len = parse_mbchar(line, NULL, NULL, NULL);
    line += line_len;

    while (*line != '\0' && (is_blank_mbchar(line) ||
	(newline && *line == '\n'))) {
	line_len = parse_mbchar(line, NULL, NULL, NULL);

	line += line_len;
	blank_loc += line_len;
    }

    return blank_loc;
}
#endif /* !DISABLE_HELP || !DISABLE_JUSTIFY || !DISABLE_WRAPPING */

#if !defined(NANO_SMALL) || !defined(DISABLE_JUSTIFY)
/* The "indentation" of a line is the whitespace between the quote part
 * and the non-whitespace of the line. */
size_t indent_length(const char *line)
{
    size_t len = 0;
    char *blank_mb;
    int blank_mb_len;

    assert(line != NULL);

    blank_mb = charalloc(mb_cur_max());

    while (*line != '\0') {
	blank_mb_len = parse_mbchar(line, blank_mb, NULL, NULL);

	if (!is_blank_mbchar(blank_mb))
	    break;

	line += blank_mb_len;
	len += blank_mb_len;
    }

    free(blank_mb);

    return len;
}
#endif /* !NANO_SMALL || !DISABLE_JUSTIFY */

#ifndef DISABLE_JUSTIFY
/* justify_format() replaces blanks with spaces and multiple spaces by 1
 * (except it maintains up to 2 after a character in punct optionally
 * followed by a character in brackets, and removes all from the end).
 *
 * justify_format() might make paragraph->data shorter, and change the
 * actual pointer with null_at().
 *
 * justify_format() will not look at the first skip characters of
 * paragraph.  skip should be at most strlen(paragraph->data).  The
 * character at paragraph[skip + 1] must not be blank. */
void justify_format(filestruct *paragraph, size_t skip)
{
    char *end, *new_end, *new_paragraph_data;
    size_t shift = 0;
#ifndef NANO_SMALL
    size_t mark_shift = 0;
#endif

    /* These four asserts are assumptions about the input data. */
    assert(paragraph != NULL);
    assert(paragraph->data != NULL);
    assert(skip < strlen(paragraph->data));
    assert(!is_blank_mbchar(paragraph->data + skip));

    end = paragraph->data + skip;
    new_paragraph_data = charalloc(strlen(paragraph->data) + 1);
    strncpy(new_paragraph_data, paragraph->data, skip);
    new_end = new_paragraph_data + skip;

    while (*end != '\0') {
	int end_len;

	/* If this character is blank, make sure that it's a space with
	 * no blanks after it. */
	if (is_blank_mbchar(end)) {
	    end_len = parse_mbchar(end, NULL, NULL, NULL);

	    *new_end = ' ';
	    new_end++;
	    end += end_len;

	    while (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL, NULL);

		end += end_len;
		shift += end_len;

#ifndef NANO_SMALL
		/* Keep track of the change in the current line. */
		if (openfile->mark_set && openfile->mark_begin ==
			paragraph && openfile->mark_begin_x >= end -
			paragraph->data)
		    mark_shift += end_len;
#endif
	    }
	/* If this character is punctuation optionally followed by a
	 * bracket and then followed by blanks, make sure there are no
	 * more than two blanks after it, and make sure that the blanks
	 * are spaces. */
	} else if (mbstrchr(punct, end) != NULL) {
	    end_len = parse_mbchar(end, NULL, NULL, NULL);

	    while (end_len > 0) {
		*new_end = *end;
		new_end++;
		end++;
		end_len--;
	    }

	    if (*end != '\0' && mbstrchr(brackets, end) != NULL) {
		end_len = parse_mbchar(end, NULL, NULL, NULL);

		while (end_len > 0) {
		    *new_end = *end;
		    new_end++;
		    end++;
		    end_len--;
		}
	    }

	    if (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL, NULL);

		*new_end = ' ';
		new_end++;
		end += end_len;
	    }

	    if (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL, NULL);

		*new_end = ' ';
		new_end++;
		end += end_len;
	    }

	    while (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL, NULL);

		end += end_len;
		shift += end_len;

#ifndef NANO_SMALL
		/* Keep track of the change in the current line. */
		if (openfile->mark_set && openfile->mark_begin ==
			paragraph && openfile->mark_begin_x >= end -
			paragraph->data)
		    mark_shift += end_len;
#endif
	    }
	/* If this character is neither blank nor punctuation, leave it
	 * alone. */
	} else {
	    end_len = parse_mbchar(end, NULL, NULL, NULL);

	    while (end_len > 0) {
		*new_end = *end;
		new_end++;
		end++;
		end_len--;
	    }
	}
    }

    assert(*end == '\0');

    *new_end = *end;

    /* Make sure that there are no spaces at the end of the line. */
    while (new_end > new_paragraph_data + skip &&
	*(new_end - 1) == ' ') {
	new_end--;
	shift++;
    }

    if (shift > 0) {
	openfile->totsize -= shift;
	null_at(&new_paragraph_data, new_end - new_paragraph_data);
	free(paragraph->data);
	paragraph->data = new_paragraph_data;

#ifndef NANO_SMALL
	/* Adjust the mark coordinates to compensate for the change in
	 * the current line. */
	if (openfile->mark_set && openfile->mark_begin == paragraph) {
	    openfile->mark_begin_x -= mark_shift;
	    if (openfile->mark_begin_x > new_end - new_paragraph_data)
		openfile->mark_begin_x = new_end - new_paragraph_data;
	}
#endif
    } else
	free(new_paragraph_data);
}

/* The "quote part" of a line is the largest initial substring matching
 * the quote string.  This function returns the length of the quote part
 * of the given line.
 *
 * Note that if !HAVE_REGEX_H then we match concatenated copies of
 * quotestr. */
size_t quote_length(const char *line)
{
#ifdef HAVE_REGEX_H
    regmatch_t matches;
    int rc = regexec(&quotereg, line, 1, &matches, 0);

    if (rc == REG_NOMATCH || matches.rm_so == (regoff_t)-1)
	return 0;
    /* matches.rm_so should be 0, since the quote string should start
     * with the caret ^. */
    return matches.rm_eo;
#else	/* !HAVE_REGEX_H */
    size_t qdepth = 0;

    /* Compute quote depth level. */
    while (strncmp(line + qdepth, quotestr, quotelen) == 0)
	qdepth += quotelen;
    return qdepth;
#endif	/* !HAVE_REGEX_H */
}

/* a_line and b_line are lines of text.  The quotation part of a_line is
 * the first a_quote characters.  Check that the quotation part of
 * b_line is the same. */
bool quotes_match(const char *a_line, size_t a_quote, const char
	*b_line)
{
    /* Here is the assumption about a_quote. */
    assert(a_quote == quote_length(a_line));

    return (a_quote == quote_length(b_line) &&
	strncmp(a_line, b_line, a_quote) == 0);
}

/* We assume a_line and b_line have no quote part.  Then, we return
 * whether b_line could follow a_line in a paragraph. */
bool indents_match(const char *a_line, size_t a_indent, const char
	*b_line, size_t b_indent)
{
    assert(a_indent == indent_length(a_line));
    assert(b_indent == indent_length(b_line));

    return (b_indent <= a_indent &&
	strncmp(a_line, b_line, b_indent) == 0);
}

/* Is foo the beginning of a paragraph?
 *
 *   A line of text consists of a "quote part", followed by an
 *   "indentation part", followed by text.  The functions quote_length()
 *   and indent_length() calculate these parts.
 *
 *   A line is "part of a paragraph" if it has a part not in the quote
 *   part or the indentation.
 *
 *   A line is "the beginning of a paragraph" if it is part of a
 *   paragraph and
 *	1) it is the top line of the file, or
 *	2) the line above it is not part of a paragraph, or
 *	3) the line above it does not have precisely the same quote
 *	   part, or
 *	4) the indentation of this line is not an initial substring of
 *	   the indentation of the previous line, or
 *	5) this line has no quote part and some indentation, and
 *	   autoindent isn't turned on.
 *   The reason for number 5) is that if autoindent isn't turned on,
 *   then an indented line is expected to start a paragraph, as in
 *   books.  Thus, nano can justify an indented paragraph only if
 *   autoindent is turned on. */
bool begpar(const filestruct *const foo)
{
    size_t quote_len;
    size_t indent_len;
    size_t temp_id_len;

    /* Case 1). */
    if (foo->prev == NULL)
	return TRUE;

    quote_len = quote_length(foo->data);
    indent_len = indent_length(foo->data + quote_len);

    /* Not part of a paragraph. */
    if (foo->data[quote_len + indent_len] == '\0')
	return FALSE;

    /* Case 3). */
    if (!quotes_match(foo->data, quote_len, foo->prev->data))
	return TRUE;

    temp_id_len = indent_length(foo->prev->data + quote_len);

    /* Case 2) or 5) or 4). */
    if (foo->prev->data[quote_len + temp_id_len] == '\0' ||
	(quote_len == 0 && indent_len > 0
#ifndef NANO_SMALL
	&& !ISSET(AUTOINDENT)
#endif
	) || !indents_match(foo->prev->data + quote_len, temp_id_len,
	foo->data + quote_len, indent_len))
	return TRUE;

    return FALSE;
}

/* Is foo inside a paragraph? */
bool inpar(const filestruct *const foo)
{
    size_t quote_len;

    if (foo == NULL)
	return FALSE;

    quote_len = quote_length(foo->data);

    return foo->data[quote_len + indent_length(foo->data +
	quote_len)] != '\0';
}

/* Put the next par_len lines, starting with first_line, into the
 * justify buffer, leaving copies of those lines in place.  Assume there
 * are enough lines after first_line.  Return the new copy of
 * first_line. */
filestruct *backup_lines(filestruct *first_line, size_t par_len, size_t
	quote_len)
{
    filestruct *top = first_line;
	/* The top of the paragraph we're backing up. */
    filestruct *bot = first_line;
	/* The bottom of the paragraph we're backing up. */
    size_t i;
	/* Generic loop variable. */
    size_t current_x_save = openfile->current_x;
    ssize_t fl_lineno_save = first_line->lineno;
    ssize_t edittop_lineno_save = openfile->edittop->lineno;
    ssize_t current_lineno_save = openfile->current->lineno;
#ifndef NANO_SMALL
    bool old_mark_set = openfile->mark_set;
    ssize_t mb_lineno_save = 0;
    size_t mark_begin_x_save = 0;

    if (old_mark_set) {
	mb_lineno_save = openfile->mark_begin->lineno;
	mark_begin_x_save = openfile->mark_begin_x;
    }
#endif

    /* Move bot down par_len lines to the newline after the last line of
     * the paragraph. */
    for (i = par_len; i > 0; i--)
	bot = bot->next;

    /* Move the paragraph from the current buffer's filestruct to the
     * justify buffer. */
    move_to_filestruct(&jusbuffer, &jusbottom, top, 0, bot, 0);

    /* Copy the paragraph back to the current buffer's filestruct from
     * the justify buffer. */
    copy_from_filestruct(jusbuffer, jusbottom);

    /* Move upward from the last line of the paragraph to the first
     * line, putting first_line, edittop, current, and mark_begin at the
     * same lines in the copied paragraph that they had in the original
     * paragraph. */
    top = openfile->current->prev;
    for (i = par_len; i > 0; i--) {
	if (top->lineno == fl_lineno_save)
	    first_line = top;
	if (top->lineno == edittop_lineno_save)
	    openfile->edittop = top;
	if (top->lineno == current_lineno_save)
	    openfile->current = top;
#ifndef NANO_SMALL
	if (old_mark_set && top->lineno == mb_lineno_save) {
	    openfile->mark_begin = top;
	    openfile->mark_begin_x = mark_begin_x_save;
	}
#endif
	top = top->prev;
    }

    /* Put current_x at the same place in the copied paragraph that it
     * had in the original paragraph. */
    openfile->current_x = current_x_save;

    set_modified();

    return first_line;
}

/* Find the beginning of the current paragraph if we're in one, or the
 * beginning of the next paragraph if we're not.  Afterwards, save the
 * quote length and paragraph length in *quote and *par.  Return TRUE if
 * we found a paragraph, or FALSE if there was an error or we didn't
 * find a paragraph.
 *
 * See the comment at begpar() for more about when a line is the
 * beginning of a paragraph. */
bool find_paragraph(size_t *const quote, size_t *const par)
{
    size_t quote_len;
	/* Length of the initial quotation of the paragraph we search
	 * for. */
    size_t par_len;
	/* Number of lines in the paragraph we search for. */
    filestruct *current_save;
	/* The line at the beginning of the paragraph we search for. */
    ssize_t current_y_save;
	/* The y-coordinate at the beginning of the paragraph we search
	 * for. */

#ifdef HAVE_REGEX_H
    if (quoterc != 0) {
	statusbar(_("Bad quote string %s: %s"), quotestr, quoteerr);
	return FALSE;
    }
#endif

    assert(openfile->current != NULL);

    /* Move back to the beginning of the current line. */
    openfile->current_x = 0;
    openfile->placewewant = 0;

    /* Find the first line of the current or next paragraph.  First, if
     * the current line isn't in a paragraph, move forward to the line
     * after the last line of the next paragraph.  If we end up on the
     * same line, or the line before that isn't in a paragraph, it means
     * that there aren't any paragraphs left, so get out.  Otherwise,
     * move back to the last line of the paragraph.  If the current line
     * is in a paragraph and it isn't the first line of that paragraph,
     * move back to the first line. */
    if (!inpar(openfile->current)) {
	current_save = openfile->current;

	do_para_end(FALSE);
	if (openfile->current == current_save ||
		!inpar(openfile->current->prev))
	    return FALSE;
	if (openfile->current->prev != NULL)
	    openfile->current = openfile->current->prev;
    }
    if (!begpar(openfile->current))
	do_para_begin(FALSE);

    /* Now current is the first line of the paragraph.  Set quote_len to
     * the quotation length of that line, and set par_len to the number
     * of lines in this paragraph. */
    quote_len = quote_length(openfile->current->data);
    current_save = openfile->current;
    current_y_save = openfile->current_y;
    do_para_end(FALSE);
    par_len = openfile->current->lineno - current_save->lineno;
    openfile->current = current_save;
    openfile->current_y = current_y_save;

    /* Save the values of quote_len and par_len. */
    assert(quote != NULL && par != NULL);

    *quote = quote_len;
    *par = par_len;

    return TRUE;
}

/* If full_justify is TRUE, justify the entire file.  Otherwise, justify
 * the current paragraph. */
void do_justify(bool full_justify)
{
    filestruct *first_par_line = NULL;
	/* Will be the first line of the resulting justified paragraph.
	 * For restoring after unjustify. */
    filestruct *last_par_line;
	/* Will be the line containing the newline after the last line
	 * of the result.  Also for restoring after unjustify. */

    /* We save these variables to be restored if the user unjustifies.
     * Note that we don't need to save totlines. */
    size_t current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    ssize_t current_y_save = openfile->current_y;
    bool modified_save = openfile->modified;
    size_t totsize_save = openfile->totsize;
    filestruct *edittop_save = openfile->edittop;
    filestruct *current_save = openfile->current;
#ifndef NANO_SMALL
    filestruct *mark_begin_save = openfile->mark_begin;
    size_t mark_begin_x_save = openfile->mark_begin_x;
#endif
    int kbinput;
    bool meta_key, func_key, s_or_t, ran_func, finished;

    /* If we're justifying the entire file, start at the beginning. */
    if (full_justify)
	openfile->current = openfile->fileage;

    last_par_line = openfile->current;

    while (TRUE) {
	size_t i;
	    /* Generic loop variable. */
	size_t quote_len;
	    /* Length of the initial quotation of the paragraph we
	     * justify. */
	size_t indent_len;
	    /* Length of the initial indentation of the paragraph we
	     * justify. */
	size_t par_len;
	    /* Number of lines in the paragraph we justify. */
	ssize_t break_pos;
	    /* Where we will break lines. */
	char *indent_string;
	    /* The first indentation that doesn't match the initial
	     * indentation of the paragraph we justify.  This is put at
	     * the beginning of every line broken off the first
	     * justified line of the paragraph.  (Note that this works
	     * because a paragraph can only contain two indentations at
	     * most: the initial one, and a different one starting on a
	     * line after the first.  See the comment at begpar() for
	     * more about when a line is part of a paragraph.) */

	/* Find the first line of the paragraph to be justified.  That
	 * is the start of this paragraph if we're in one, or the start
	 * of the next otherwise.  Save the quote length and paragraph
	 * length (number of lines).  Don't refresh the screen yet,
	 * since we'll do that after we justify.
	 *
	 * If the search failed, we do one of two things.  If we're
	 * justifying the whole file, we've found at least one
	 * paragraph, and the search didn't leave us on the last line of
	 * the file, it means that we should justify all the way to the
	 * last line of the file, so set the last line of the text to be
	 * justified to the last line of the file and break out of the
	 * loop.  Otherwise, it means that there are no paragraph(s) to
	 * justify, so refresh the screen and get out. */
	if (!find_paragraph(&quote_len, &par_len)) {
	    if (full_justify && first_par_line != NULL &&
		first_par_line != openfile->filebot) {
		last_par_line = openfile->filebot;
		break;
	    } else {
		edit_refresh();
		return;
	    }
	}

	/* If we haven't already done it, copy the original paragraph(s)
	 * to the justify buffer. */
	if (first_par_line == NULL)
	    first_par_line = backup_lines(openfile->current,
		full_justify ? openfile->filebot->lineno -
		openfile->current->lineno : par_len, quote_len);

	/* Initialize indent_string to a blank string. */
	indent_string = mallocstrcpy(NULL, "");

	/* Find the first indentation in the paragraph that doesn't
	 * match the indentation of the first line, and save it in
	 * indent_string.  If all the indentations are the same, save
	 * the indentation of the first line in indent_string. */
	{
	    const filestruct *indent_line = openfile->current;
	    bool past_first_line = FALSE;

	    for (i = 0; i < par_len; i++) {
		indent_len = quote_len +
			indent_length(indent_line->data + quote_len);

		if (indent_len != strlen(indent_string)) {
		    indent_string = mallocstrncpy(indent_string,
			indent_line->data, indent_len + 1);
		    indent_string[indent_len] = '\0';

		    if (past_first_line)
			break;
		}

		if (indent_line == openfile->current)
		    past_first_line = TRUE;

		indent_line = indent_line->next;
	    }
	}

	/* Now tack all the lines of the paragraph together, skipping
	 * the quoting and indentation on all lines after the first. */
	for (i = 0; i < par_len - 1; i++) {
	    filestruct *next_line = openfile->current->next;
	    size_t line_len = strlen(openfile->current->data);
	    size_t next_line_len =
		strlen(openfile->current->next->data);

	    indent_len = quote_len +
		indent_length(openfile->current->next->data +
		quote_len);

	    next_line_len -= indent_len;
	    openfile->totsize -= indent_len;

	    /* We're just about to tack the next line onto this one.  If
	     * this line isn't empty, make sure it ends in a space. */
	    if (line_len > 0 &&
		openfile->current->data[line_len - 1] != ' ') {
		line_len++;
		openfile->current->data =
			charealloc(openfile->current->data,
			line_len + 1);
		openfile->current->data[line_len - 1] = ' ';
		openfile->current->data[line_len] = '\0';
		openfile->totsize++;
	    }

	    openfile->current->data =
		charealloc(openfile->current->data, line_len +
		next_line_len + 1);
	    strcat(openfile->current->data, next_line->data +
		indent_len);

	    /* Don't destroy edittop! */
	    if (openfile->edittop == next_line)
		openfile->edittop = openfile->current;

#ifndef NANO_SMALL
	    /* Adjust the mark coordinates to compensate for the change
	     * in the next line. */
	    if (openfile->mark_set && openfile->mark_begin ==
		next_line) {
		openfile->mark_begin = openfile->current;
		openfile->mark_begin_x += line_len - indent_len;
	    }
#endif

	    unlink_node(next_line);
	    delete_node(next_line);

	    /* If we've removed the next line, we need to go through
	     * this line again. */
	    i--;

	    par_len--;
	    openfile->totlines--;
	    openfile->totsize--;
	}

	/* Call justify_format() on the paragraph, which will remove
	 * excess spaces from it and change all blank characters to
	 * spaces. */
	justify_format(openfile->current, quote_len +
		indent_length(openfile->current->data + quote_len));

	while (par_len > 0 &&
		strlenpt(openfile->current->data) > fill) {
	    size_t line_len = strlen(openfile->current->data);

	    indent_len = strlen(indent_string);

	    /* If this line is too long, try to wrap it to the next line
	     * to make it short enough. */
	    break_pos =
		break_line(openfile->current->data + indent_len, fill -
		strnlenpt(openfile->current->data, indent_len), FALSE);

	    /* We can't break the line, or don't need to, so get out. */
	    if (break_pos == -1 || break_pos + indent_len == line_len)
		break;

	    /* Move forward to the character after the indentation and
	     * just after the space. */
	    break_pos += indent_len + 1;

	    assert(break_pos <= line_len);

	    /* Make a new line, and copy the text after where we're
	     * going to break this line to the beginning of the new
	     * line. */
	    splice_node(openfile->current,
		make_new_node(openfile->current),
		openfile->current->next);

	    /* If this paragraph is non-quoted, and autoindent isn't
	     * turned on, set the indentation length to zero so that the
	     * indentation is treated as part of the line. */
	    if (quote_len == 0
#ifndef NANO_SMALL
		&& !ISSET(AUTOINDENT)
#endif
		)
		indent_len = 0;

	    /* Copy the text after where we're going to break the
	     * current line to the next line. */
	    openfile->current->next->data = charalloc(indent_len + 1 +
		line_len - break_pos);
	    strncpy(openfile->current->next->data, indent_string,
		indent_len);
	    strcpy(openfile->current->next->data + indent_len,
		openfile->current->data + break_pos);

	    par_len++;
	    openfile->totlines++;
	    openfile->totsize += indent_len + 1;

#ifndef NANO_SMALL
	    /* Adjust the mark coordinates to compensate for the change
	     * in the current line. */
	    if (openfile->mark_set && openfile->mark_begin ==
		openfile->current && openfile->mark_begin_x >
		break_pos) {
		openfile->mark_begin = openfile->current->next;
		openfile->mark_begin_x -= break_pos - indent_len;
	    }
#endif

	    /* Break the current line. */
	    null_at(&openfile->current->data, break_pos);

	    /* Go to the next line. */
	    par_len--;
	    openfile->current_y++;
	    openfile->current = openfile->current->next;
	}

	/* We're done breaking lines, so we don't need indent_string
	 * anymore. */
	free(indent_string);

	/* Go to the next line, the line after the last line of the
	 * paragraph. */
	openfile->current_y++;
	openfile->current = openfile->current->next;

	/* We've just justified a paragraph. If we're not justifying the
	 * entire file, break out of the loop.  Otherwise, continue the
	 * loop so that we justify all the paragraphs in the file. */
	if (!full_justify)
	    break;
    }

    /* We are now done justifying the paragraph or the file, so clean
     * up.  totlines, totsize, and current_y have been maintained above.
     * Set last_par_line to the new end of the paragraph, update
     * fileage, and renumber since edit_refresh() needs the line numbers
     * to be right (but only do the last two if we actually justified
     * something). */
    last_par_line = openfile->current;
    if (first_par_line != NULL) {
	if (first_par_line->prev == NULL)
	    openfile->fileage = first_par_line;
	renumber(first_par_line);
    }

    edit_refresh();

    statusbar(_("Can now UnJustify!"));

    /* If constant cursor position display is on, make sure the current
     * cursor position will be properly displayed on the statusbar. */
    if (ISSET(CONST_UPDATE))
	do_cursorpos(TRUE);

    /* Display the shortcut list with UnJustify. */
    shortcut_init(TRUE);
    display_main_list();

    /* Now get a keystroke and see if it's unjustify.  If not, put back
     * the keystroke and return. */
    kbinput = do_input(&meta_key, &func_key, &s_or_t, &ran_func,
	&finished, FALSE);

    if (!meta_key && !func_key && s_or_t &&
	kbinput == NANO_UNJUSTIFY_KEY) {
	/* Restore the justify we just did (ungrateful user!). */
	openfile->current = current_save;
	openfile->current_x = current_x_save;
	openfile->placewewant = pww_save;
	openfile->current_y = current_y_save;
	openfile->edittop = edittop_save;

	/* Splice the justify buffer back into the file, but only if we
	 * actually justified something. */
	if (first_par_line != NULL) {
	    filestruct *top_save;

	    /* Partition the filestruct so that it contains only the
	     * text of the justified paragraph. */
	    filepart = partition_filestruct(first_par_line, 0,
		last_par_line, 0);

	    /* Remove the text of the justified paragraph, and
	     * put the text in the justify buffer in its place. */
	    free_filestruct(openfile->fileage);
	    openfile->fileage = jusbuffer;
	    openfile->filebot = jusbottom;

	    top_save = openfile->fileage;

	    /* Unpartition the filestruct so that it contains all the
	     * text again.  Note that the justified paragraph has been
	     * replaced with the unjustified paragraph. */
	    unpartition_filestruct(&filepart);

	     /* Renumber starting with the beginning line of the old
	      * partition. */
	    renumber(top_save);

	    /* Restore variables from before the justify. */
	    openfile->totsize = totsize_save;
	    openfile->totlines = openfile->filebot->lineno;
#ifndef NANO_SMALL
	    if (openfile->mark_set) {
		openfile->mark_begin = mark_begin_save;
		openfile->mark_begin_x = mark_begin_x_save;
	    }
#endif
	    openfile->modified = modified_save;

	    /* Clear the justify buffer. */
	    jusbuffer = NULL;

	    if (!openfile->modified)
		titlebar(NULL);
	    edit_refresh();
	}
    } else {
	unget_kbinput(kbinput, meta_key, func_key);

	/* Blow away the text in the justify buffer. */
	free_filestruct(jusbuffer);
	jusbuffer = NULL;
    }

    blank_statusbar();

    /* Display the shortcut list with UnCut. */
    shortcut_init(FALSE);
    display_main_list();
}

void do_justify_void(void)
{
    do_justify(FALSE);
}

void do_full_justify(void)
{
    do_justify(TRUE);
}
#endif /* !DISABLE_JUSTIFY */

void do_exit(void)
{
    int i;

    if (!openfile->modified)
	i = 0;		/* Pretend the user chose not to save. */
    else if (ISSET(TEMP_FILE))
	i = 1;
    else
	i = do_yesno(FALSE,
		_("Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES) ? "));

#ifdef DEBUG
    dump_filestruct(openfile->fileage);
#endif

    if (i == 0 || (i == 1 && do_writeout(TRUE) == 0)) {
#ifdef ENABLE_MULTIBUFFER
	/* Exit only if there are no more open file buffers. */
	if (!close_buffer())
#endif
	    finish();
    } else if (i != 1)
	statusbar(_("Cancelled"));

    display_main_list();
}

void signal_init(void)
{
    /* Trap SIGINT and SIGQUIT because we want them to do useful
     * things. */
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    /* Trap SIGHUP and SIGTERM because we want to write the file out. */
    act.sa_handler = handle_hupterm;
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

#ifndef NANO_SMALL
    /* Trap SIGWINCH because we want to handle window resizes. */
    act.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &act, NULL);
    allow_pending_sigwinch(FALSE);
#endif

    /* Trap normal suspend (^Z) so we can handle it ourselves. */
    if (!ISSET(SUSPEND)) {
	act.sa_handler = SIG_IGN;
	sigaction(SIGTSTP, &act, NULL);
    } else {
	/* Block all other signals in the suspend and continue handlers.
	 * If we don't do this, other stuff interrupts them! */
	sigfillset(&act.sa_mask);

	act.sa_handler = do_suspend;
	sigaction(SIGTSTP, &act, NULL);

	act.sa_handler = do_cont;
	sigaction(SIGCONT, &act, NULL);
    }
}

/* Handler for SIGHUP (hangup) and SIGTERM (terminate). */
void handle_hupterm(int signal)
{
    die(_("Received SIGHUP or SIGTERM\n"));
}

/* Handler for SIGTSTP (suspend). */
void do_suspend(int signal)
{
    endwin();
    printf("\n\n\n\n\n%s\n", _("Use \"fg\" to return to nano"));
    fflush(stdout);

    /* Restore the old terminal settings. */
    tcsetattr(0, TCSANOW, &oldterm);

    /* Trap SIGHUP and SIGTERM so we can properly deal with them while
     * suspended. */
    act.sa_handler = handle_hupterm;
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    /* Do what mutt does: send ourselves a SIGSTOP. */
    kill(0, SIGSTOP);
}

/* Handler for SIGCONT (continue after suspend). */
void do_cont(int signal)
{
#ifndef NANO_SMALL
    /* Perhaps the user resized the window while we slept.  Handle it
     * and update the screen in the process. */
    handle_sigwinch(0);
#else
    /* Just update the screen. */
    doupdate();
#endif
}

#ifndef NANO_SMALL
void handle_sigwinch(int s)
{
    const char *tty = ttyname(0);
    int fd, result = 0;
    struct winsize win;

    if (tty == NULL)
	return;
    fd = open(tty, O_RDWR);
    if (fd == -1)
	return;
    result = ioctl(fd, TIOCGWINSZ, &win);
    close(fd);
    if (result == -1)
	return;

    /* Could check whether the COLS or LINES changed, and return
     * otherwise.  EXCEPT, that COLS and LINES are ncurses global
     * variables, and in some cases ncurses has already updated them. 
     * But not in all cases, argh. */
    COLS = win.ws_col;
    LINES = win.ws_row;

    /* Reinitialize the window size variables. */
    window_size_init();

    /* If we've partitioned the filestruct, unpartition it now. */
    if (filepart != NULL)
	unpartition_filestruct(&filepart);

#ifndef DISABLE_JUSTIFY
    /* If the justify buffer isn't empty, blow away the text in it and
     * display the shortcut list with UnCut. */
    if (jusbuffer != NULL) {
	free_filestruct(jusbuffer);
	jusbuffer = NULL;
	shortcut_init(FALSE);
    }
#endif

#ifdef USE_SLANG
    /* Slang curses emulation brain damage, part 1: If we just do what
     * curses does here, it'll only work properly if the resize made the
     * window smaller.  Do what mutt does: Leave and immediately reenter
     * Slang screen management mode. */
    SLsmg_reset_smg();
    SLsmg_init_smg();
#else
    /* Do the equivalent of what Minimum Profit does: Leave and
     * immediately reenter curses mode. */
    endwin();
    refresh();
#endif

    /* Restore the terminal to its previous state. */
    terminal_init();

    /* Turn the cursor back on for sure. */
    curs_set(1);

    /* Do the equivalent of what both mutt and Minimum Profit do:
     * Reinitialize all the windows based on the new screen
     * dimensions. */
    window_init();

    /* Redraw the contents of the windows that need it. */
    blank_statusbar();
    currshortcut = main_list;
    total_refresh();

    /* Reset all the input routines that rely on character sequences. */
    reset_kbinput();

    /* Jump back to the main loop. */
    siglongjmp(jmpbuf, 1);
}

void allow_pending_sigwinch(bool allow)
{
    sigset_t winch;
    sigemptyset(&winch);
    sigaddset(&winch, SIGWINCH);
    sigprocmask(allow ? SIG_UNBLOCK : SIG_BLOCK, &winch, NULL);
}
#endif /* !NANO_SMALL */

#ifndef NANO_SMALL
void do_toggle(const toggle *which)
{
    bool enabled;

    TOGGLE(which->flag);

    switch (which->val) {
#ifndef DISABLE_MOUSE
	case TOGGLE_MOUSE_KEY:
	    mouse_init();
	    break;
#endif
	case TOGGLE_MORESPACE_KEY:
	case TOGGLE_NOHELP_KEY:
	    window_init();
	    total_refresh();
	    break;
	case TOGGLE_SUSPEND_KEY:
	    signal_init();
	    break;
#ifdef ENABLE_NANORC
	case TOGGLE_WHITESPACE_KEY:
	    titlebar(NULL);
	    edit_refresh();
	    break;
#endif
#ifdef ENABLE_COLOR
	case TOGGLE_SYNTAX_KEY:
	    edit_refresh();
	    break;
#endif
    }

    enabled = ISSET(which->flag);

    if (which->val == TOGGLE_NOHELP_KEY
#ifndef DISABLE_WRAPPING
	|| which->val == TOGGLE_WRAP_KEY
#endif
#ifdef ENABLE_COLOR
	|| which->val == TOGGLE_SYNTAX_KEY
#endif
	)
	enabled = !enabled;

    statusbar("%s %s", which->desc, enabled ? _("enabled") :
	_("disabled"));
}
#endif /* !NANO_SMALL */

void disable_extended_io(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_lflag &= ~IEXTEN;
    term.c_oflag &= ~OPOST;
    tcsetattr(0, TCSANOW, &term);
}

void disable_signals(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_lflag &= ~ISIG;
    tcsetattr(0, TCSANOW, &term);
}

#ifndef NANO_SMALL
void enable_signals(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_lflag |= ISIG;
    tcsetattr(0, TCSANOW, &term);
}
#endif

void disable_flow_control(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_iflag &= ~IXON;
    tcsetattr(0, TCSANOW, &term);
}

void enable_flow_control(void)
{
    struct termios term;

    tcgetattr(0, &term);
    term.c_iflag |= IXON;
    tcsetattr(0, TCSANOW, &term);
}

/* Set up the terminal state.  Put the terminal in cbreak mode (read one
 * character at a time and interpret the special control keys), disable
 * translation of carriage return (^M) into newline (^J) so that we can
 * tell the difference between the Enter key and Ctrl-J, and disable
 * echoing of characters as they're typed.  Finally, disable extended
 * input and output processing, disable interpretation of the special
 * control keys, and if we're not in preserve mode, disable
 * interpretation of the flow control characters too. */
void terminal_init(void)
{
    cbreak();
    nonl();
    noecho();
    disable_extended_io();
    disable_signals();
    if (!ISSET(PRESERVE))
	disable_flow_control();
}

int do_input(bool *meta_key, bool *func_key, bool *s_or_t, bool
	*ran_func, bool *finished, bool allow_funcs)
{
    int input;
	/* The character we read in. */
    static int *kbinput = NULL;
	/* The input buffer. */
    static size_t kbinput_len = 0;
	/* The length of the input buffer. */
    const shortcut *s;
    bool have_shortcut;
#ifndef NANO_SMALL
    const toggle *t;
    bool have_toggle;
#endif

    *s_or_t = FALSE;
    *ran_func = FALSE;
    *finished = FALSE;

    /* Read in a character. */
    input = get_kbinput(edit, meta_key, func_key);

#ifndef DISABLE_MOUSE
    /* If we got a mouse click and it was on a shortcut, read in the
     * shortcut character. */
    if (allow_funcs && *func_key == TRUE && input == KEY_MOUSE) {
	if (do_mouse())
	    input = get_kbinput(edit, meta_key, func_key);
	else
	    input = ERR;
    }
#endif

    /* Check for a shortcut in the main list. */
    s = get_shortcut(main_list, &input, meta_key, func_key);

    /* If we got a shortcut from the main list, or a "universal"
     * edit window shortcut, set have_shortcut to TRUE. */
    have_shortcut = (s != NULL || input == NANO_XON_KEY ||
	input == NANO_XOFF_KEY || input == NANO_SUSPEND_KEY);

#ifndef NANO_SMALL
    /* Check for a toggle in the main list. */
    t = get_toggle(input, *meta_key);

    /* If we got a toggle from the main list, set have_toggle to
     * TRUE. */
    have_toggle = (t != NULL);
#endif

    /* Set s_or_t to TRUE if we got a shortcut or toggle. */
    *s_or_t = (have_shortcut
#ifndef NANO_SMALL
	|| have_toggle
#endif
	);

    if (allow_funcs) {
	/* If we got a character, and it isn't a shortcut or toggle,
	 * it's a normal text character.  Display the warning if we're
	 * in view mode, or add the character to the input buffer if
	 * we're not. */
	if (input != ERR && *s_or_t == FALSE) {
	    if (ISSET(VIEW_MODE))
		print_view_warning();
	    else {
		kbinput_len++;
		kbinput = (int *)nrealloc(kbinput, kbinput_len *
			sizeof(int));
		kbinput[kbinput_len - 1] = input;
	    }
	}

	/* If we got a shortcut or toggle, or if there aren't any other
	 * characters waiting after the one we read in, we need to
	 * display all the characters in the input buffer if it isn't
	 * empty.  Note that it should be empty if we're in view
	 * mode. */
	 if (*s_or_t == TRUE || get_key_buffer_len() == 0) {
	    if (kbinput != NULL) {
		/* Display all the characters in the input buffer at
		 * once, filtering out control characters. */
		char *output = charalloc(kbinput_len + 1);
		size_t i;

		for (i = 0; i < kbinput_len; i++)
		    output[i] = (char)kbinput[i];
		output[i] = '\0';

		do_output(output, kbinput_len, FALSE);

		free(output);

		/* Empty the input buffer. */
		kbinput_len = 0;
		free(kbinput);
		kbinput = NULL;
	    }
	}

	if (have_shortcut) {
	    switch (input) {
		/* Handle the "universal" edit window shortcuts. */
		case NANO_XON_KEY:
		    statusbar(_("XON ignored, mumble mumble."));
		    break;
		case NANO_XOFF_KEY:
		    statusbar(_("XOFF ignored, mumble mumble."));
		    break;
#ifndef NANO_SMALL
		case NANO_SUSPEND_KEY:
		    if (ISSET(SUSPEND))
			do_suspend(0);
		    break;
#endif
		/* Handle the normal edit window shortcuts, setting
		 * ran_func to TRUE if we try to run their associated
		 * functions and setting finished to TRUE to indicate
		 * that we're done after trying to run their associated
		 * functions. */
		default:
		    /* Blow away the text in the cutbuffer if we aren't
		     * cutting text. */
		    if (s->func != do_cut_text)
			cutbuffer_reset();

		    if (s->func != NULL) {
			*ran_func = TRUE;
			if (ISSET(VIEW_MODE) && !s->viewok)
			    print_view_warning();
			else
			    s->func();
		    }
		    *finished = TRUE;
		    break;
	    }
	}
#ifndef NANO_SMALL
	else if (have_toggle) {
	    /* Blow away the text in the cutbuffer, since we aren't
	     * cutting text. */
	    cutbuffer_reset();
	    /* Toggle the flag associated with this shortcut. */
	    if (allow_funcs)
		do_toggle(t);
	}
#endif
	else
	    /* Blow away the text in the cutbuffer, since we aren't
	     * cutting text. */
	    cutbuffer_reset();
    }

    return input;
}

#ifndef DISABLE_MOUSE
bool do_mouse(void)
{
    int mouse_x, mouse_y;
    bool retval;

    retval = get_mouseinput(&mouse_x, &mouse_y, TRUE);

    if (!retval) {
	/* We can click in the edit window to move the cursor. */
	if (wenclose(edit, mouse_y, mouse_x)) {
	    bool sameline;
		/* Did they click on the line with the cursor?  If they
		 * clicked on the cursor, we set the mark. */
	    size_t xcur;
		/* The character they clicked on. */

	    /* Subtract out the size of topwin. */
	    mouse_y -= 2 - no_more_space();

	    sameline = (mouse_y == openfile->current_y);

	    /* Move to where the click occurred. */
	    for (; openfile->current_y < mouse_y &&
		openfile->current->next != NULL; openfile->current_y++)
		openfile->current = openfile->current->next;
	    for (; openfile->current_y > mouse_y &&
		openfile->current->prev != NULL; openfile->current_y--)
		openfile->current = openfile->current->prev;

	    xcur = actual_x(openfile->current->data,
		get_page_start(xplustabs()) + mouse_x);

#ifndef NANO_SMALL
	    /* Clicking where the cursor is toggles the mark, as does
	     * clicking beyond the line length with the cursor at the
	     * end of the line. */
	    if (sameline && xcur == openfile->current_x) {
		if (ISSET(VIEW_MODE)) {
		    print_view_warning();
		    return retval;
		}
		do_mark();
	    }
#endif

	    openfile->current_x = xcur;
	    openfile->placewewant = xplustabs();
	    edit_refresh();
	}
    }

    return retval;
}
#endif /* !DISABLE_MOUSE */

/* The user typed ouuput_len multibyte characters.  Add them to the edit
 * buffer, filtering out all control characters if allow_cntrls is
 * TRUE. */
void do_output(char *output, size_t output_len, bool allow_cntrls)
{
    size_t current_len, i = 0;
    bool old_const_update = ISSET(CONST_UPDATE);
    bool do_refresh = FALSE;
	/* Do we have to call edit_refresh(), or can we get away with
	 * update_line()? */

    char *char_buf = charalloc(mb_cur_max());
    int char_buf_len;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    current_len = strlen(openfile->current->data);

    /* Turn off constant cursor position display. */
    UNSET(CONST_UPDATE);

    while (i < output_len) {
	/* If allow_cntrls is FALSE, filter out nulls and newlines,
	 * since they're control characters. */
	if (allow_cntrls) {
	    /* Null to newline, if needed. */
	    if (output[i] == '\0')
		output[i] = '\n';
	    /* Newline to Enter, if needed. */
	    else if (output[i] == '\n') {
		do_enter();
		i++;
		continue;
	    }
	}

	/* Interpret the next multibyte character.  If it's an invalid
	 * multibyte character, interpret it as though it's a byte
	 * character. */
	char_buf_len = parse_mbchar(output + i, char_buf, NULL, NULL);

	i += char_buf_len;

	/* If allow_cntrls is FALSE, filter out a control character. */
	if (!allow_cntrls && is_cntrl_mbchar(output + i - char_buf_len))
	    continue;

	/* When a character is inserted on the current magicline, it
	 * means we need a new one! */
	if (openfile->filebot == openfile->current)
	    new_magicline();

	/* More dangerousness fun =) */
	openfile->current->data = charealloc(openfile->current->data,
		current_len + (char_buf_len * 2));

	assert(openfile->current_x <= current_len);

	charmove(&openfile->current->data[openfile->current_x +
		char_buf_len],
		&openfile->current->data[openfile->current_x],
		current_len - openfile->current_x + char_buf_len);
	strncpy(&openfile->current->data[openfile->current_x], char_buf,
		char_buf_len);
	current_len += char_buf_len;
	openfile->totsize++;
	set_modified();

#ifndef NANO_SMALL
	/* Note that current_x has not yet been incremented. */
	if (openfile->mark_set && openfile->current ==
		openfile->mark_begin && openfile->current_x <
		openfile->mark_begin_x)
	    openfile->mark_begin_x += char_buf_len;
#endif

	openfile->current_x += char_buf_len;

#ifndef DISABLE_WRAPPING
	/* If we're wrapping text, we need to call edit_refresh(). */
	if (!ISSET(NO_WRAP)) {
	    bool do_refresh_save = do_refresh;

	    do_refresh = do_wrap(openfile->current);

	    /* If we needed to call edit_refresh() before this, we'll
	     * still need to after this. */
	    if (do_refresh_save)
		do_refresh = TRUE;
	}
#endif

#ifdef ENABLE_COLOR
	/* If color syntaxes are available and turned on, we need to
	 * call edit_refresh(). */
	if (openfile->colorstrings != NULL && !ISSET(NO_COLOR_SYNTAX))
	    do_refresh = TRUE;
#endif
    }

    /* Turn constant cursor position display back on if it was on
     * before. */
    if (old_const_update)
	SET(CONST_UPDATE);

    free(char_buf);

    openfile->placewewant = xplustabs();

    if (do_refresh)
	edit_refresh();
    else
	update_line(openfile->current, openfile->current_x);
}

int main(int argc, char **argv)
{
    int optchr;
    ssize_t startline = 1;
	/* Line to try and start at. */
    ssize_t startcol = 1;
	/* Column to try and start at. */
#ifndef DISABLE_WRAPJUSTIFY
    bool fill_flag_used = FALSE;
	/* Was the fill option used? */
#endif
#ifdef ENABLE_MULTIBUFFER
    bool old_multibuffer;
	/* The old value of the multibuffer option, restored after we
	 * load all files on the command line. */
#endif
#ifdef HAVE_GETOPT_LONG
    const struct option long_options[] = {
	{"help", 0, NULL, 'h'},
#ifdef ENABLE_MULTIBUFFER
	{"multibuffer", 0, NULL, 'F'},
#endif
#ifdef ENABLE_NANORC
	{"ignorercfiles", 0, NULL, 'I'},
#endif
	{"morespace", 0, NULL, 'O'},
#ifndef DISABLE_JUSTIFY
	{"quotestr", 1, NULL, 'Q'},
#endif
	{"restricted", 0, NULL, 'R'},
	{"tabsize", 1, NULL, 'T'},
	{"version", 0, NULL, 'V'},
#ifdef ENABLE_COLOR
	{"syntax", 1, NULL, 'Y'},
#endif
	{"const", 0, NULL, 'c'},
	{"rebinddelete", 0, NULL, 'd'},
	{"nofollow", 0, NULL, 'l'},
#ifndef DISABLE_MOUSE
	{"mouse", 0, NULL, 'm'},
#endif
#ifndef DISABLE_OPERATINGDIR
	{"operatingdir", 1, NULL, 'o'},
#endif
	{"preserve", 0, NULL, 'p'},
#ifndef DISABLE_WRAPJUSTIFY
	{"fill", 1, NULL, 'r'},
#endif
#ifndef DISABLE_SPELLER
	{"speller", 1, NULL, 's'},
#endif
	{"tempfile", 0, NULL, 't'},
	{"view", 0, NULL, 'v'},
#ifndef DISABLE_WRAPPING
	{"nowrap", 0, NULL, 'w'},
#endif
	{"nohelp", 0, NULL, 'x'},
	{"suspend", 0, NULL, 'z'},
#ifndef NANO_SMALL
	{"smarthome", 0, NULL, 'A'},
	{"backup", 0, NULL, 'B'},
	{"backupdir", 1, NULL, 'C'},
	{"tabstospaces", 0, NULL, 'E'},
	{"historylog", 0, NULL, 'H'},
	{"noconvert", 0, NULL, 'N'},
	{"smooth", 0, NULL, 'S'},
	{"quickblank", 0, NULL, 'U'},
	{"autoindent", 0, NULL, 'i'},
	{"cut", 0, NULL, 'k'},
#endif
	{NULL, 0, NULL, 0}
    };
#endif

#ifdef ENABLE_UTF8
    {
	/* If the locale set exists and includes the case-insensitive
	 * string "UTF8" or "UTF-8", we should use UTF-8. */
	char *locale = setlocale(LC_ALL, "");

	if (locale != NULL && (strcasestr(locale, "UTF8") != NULL ||
		strcasestr(locale, "UTF-8") != NULL)) {
	    SET(USE_UTF8);
#ifdef USE_SLANG
	    SLutf8_enable(TRUE);
#endif
	}
    }
#else
    setlocale(LC_ALL, "");
#endif

#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#if !defined(ENABLE_NANORC) && defined(DISABLE_ROOTWRAP) && !defined(DISABLE_WRAPPING)
    /* If we don't have rcfile support, we're root, and
     * --disable-wrapping-as-root is used, turn wrapping off. */
    if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif

    while ((optchr =
#ifdef HAVE_GETOPT_LONG
	getopt_long(argc, argv,
		"h?ABC:EFHINOQ:RST:UVY:abcdefgijklmo:pr:s:tvwxz",
		long_options, NULL)
#else
	getopt(argc, argv,
		"h?ABC:EFHINOQ:RST:UVY:abcdefgijklmo:pr:s:tvwxz")
#endif
		) != -1) {

	switch (optchr) {
	    case 'a':
	    case 'b':
	    case 'e':
	    case 'f':
	    case 'g':
	    case 'j':
		/* Pico compatibility flags. */
		break;
#ifndef NANO_SMALL
	    case 'A':
		SET(SMART_HOME);
		break;
	    case 'B':
		SET(BACKUP_FILE);
		break;
	    case 'C':
		backup_dir = mallocstrcpy(backup_dir, optarg);
		break;
	    case 'E':
		SET(TABS_TO_SPACES);
		break;
#endif
#ifdef ENABLE_MULTIBUFFER
	    case 'F':
		SET(MULTIBUFFER);
		break;
#endif
#ifdef ENABLE_NANORC
#ifndef NANO_SMALL
	    case 'H':
		SET(HISTORYLOG);
		break;
#endif
	    case 'I':
		SET(NO_RCFILE);
		break;
#endif
#ifndef NANO_SMALL
	    case 'N':
		SET(NO_CONVERT);
		break;
#endif
	    case 'O':
		SET(MORE_SPACE);
		break;
#ifndef DISABLE_JUSTIFY
	    case 'Q':
		quotestr = mallocstrcpy(quotestr, optarg);
		break;
#endif
	    case 'R':
		SET(RESTRICTED);
		break;
#ifndef NANO_SMALL
	    case 'S':
		SET(SMOOTH_SCROLL);
		break;
#endif
	    case 'T':
		if (!parse_num(optarg, &tabsize) || tabsize <= 0) {
		    fprintf(stderr, _("Requested tab size %s invalid"), optarg);
		    fprintf(stderr, "\n");
		    exit(1);
		}
		break;
#ifndef NANO_SMALL
	    case 'U':
		SET(QUICK_BLANK);
		break;
#endif
	    case 'V':
		version();
		exit(0);
#ifdef ENABLE_COLOR
	    case 'Y':
		syntaxstr = mallocstrcpy(syntaxstr, optarg);
		break;
#endif
	    case 'c':
		SET(CONST_UPDATE);
		break;
	    case 'd':
		SET(REBIND_DELETE);
		break;
#ifndef NANO_SMALL
	    case 'i':
		SET(AUTOINDENT);
		break;
	    case 'k':
		SET(CUT_TO_END);
		break;
#endif
	    case 'l':
		SET(NOFOLLOW_SYMLINKS);
		break;
#ifndef DISABLE_MOUSE
	    case 'm':
		SET(USE_MOUSE);
		break;
#endif
#ifndef DISABLE_OPERATINGDIR
	    case 'o':
		operating_dir = mallocstrcpy(operating_dir, optarg);
		break;
#endif
	    case 'p':
		SET(PRESERVE);
		break;
#ifndef DISABLE_WRAPJUSTIFY
	    case 'r':
		if (!parse_num(optarg, &wrap_at)) {
		    fprintf(stderr, _("Requested fill size %s invalid"), optarg);
		    fprintf(stderr, "\n");
		    exit(1);
		}
		fill_flag_used = TRUE;
		break;
#endif
#ifndef DISABLE_SPELLER
	    case 's':
		alt_speller = mallocstrcpy(alt_speller, optarg);
		break;
#endif
	    case 't':
		SET(TEMP_FILE);
		break;
	    case 'v':
		SET(VIEW_MODE);
		break;
#ifndef DISABLE_WRAPPING
	    case 'w':
		SET(NO_WRAP);
		break;
#endif
	    case 'x':
		SET(NO_HELP);
		break;
	    case 'z':
		SET(SUSPEND);
		break;
	    default:
		usage();
	}
    }

    /* If the executable filename starts with 'r', we use restricted
     * mode. */
    if (*(tail(argv[0])) == 'r')
	SET(RESTRICTED);

    /* If we're using restricted mode, disable suspending, backups, and
     * reading rcfiles, since they all would allow reading from or
     * writing to files not specified on the command line. */
    if (ISSET(RESTRICTED)) {
	UNSET(SUSPEND);
	UNSET(BACKUP_FILE);
	SET(NO_RCFILE);
    }

/* We've read through the command line options.  Now back up the flags
 * and values that are set, and read the rcfile(s).  If the values
 * haven't changed afterward, restore the backed-up values. */
#ifdef ENABLE_NANORC
    if (!ISSET(NO_RCFILE)) {
#ifndef DISABLE_OPERATINGDIR
	char *operating_dir_cpy = operating_dir;
#endif
#ifndef DISABLE_WRAPJUSTIFY
	ssize_t wrap_at_cpy = wrap_at;
#endif
#ifndef NANO_SMALL
	char *backup_dir_cpy = backup_dir;
#endif
#ifndef DISABLE_JUSTIFY
	char *quotestr_cpy = quotestr;
#endif
#ifndef DISABLE_SPELLER
	char *alt_speller_cpy = alt_speller;
#endif
	ssize_t tabsize_cpy = tabsize;
	long flags_cpy = flags;

#ifndef DISABLE_OPERATINGDIR
	operating_dir = NULL;
#endif
#ifndef NANO_SMALL
	backup_dir = NULL;
#endif
#ifndef DISABLE_JUSTIFY
	quotestr = NULL;
#endif
#ifndef DISABLE_SPELLER
	alt_speller = NULL;
#endif

	do_rcfile();

#ifndef DISABLE_OPERATINGDIR
	if (operating_dir_cpy != NULL) {
	    free(operating_dir);
	    operating_dir = operating_dir_cpy;
	}
#endif
#ifndef DISABLE_WRAPJUSTIFY
	if (fill_flag_used)
	    wrap_at = wrap_at_cpy;
#endif
#ifndef NANO_SMALL
	if (backup_dir_cpy != NULL) {
	    free(backup_dir);
	    backup_dir = backup_dir_cpy;
	}
#endif	
#ifndef DISABLE_JUSTIFY
	if (quotestr_cpy != NULL) {
	    free(quotestr);
	    quotestr = quotestr_cpy;
	}
#endif
#ifndef DISABLE_SPELLER
	if (alt_speller_cpy != NULL) {
	    free(alt_speller);
	    alt_speller = alt_speller_cpy;
	}
#endif
	if (tabsize_cpy != -1)
	    tabsize = tabsize_cpy;
	flags |= flags_cpy;
    }
#if defined(DISABLE_ROOTWRAP) && !defined(DISABLE_WRAPPING)
    else if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif
#endif /* ENABLE_NANORC */

#ifndef NANO_SMALL
    history_init();
#ifdef ENABLE_NANORC
    if (!ISSET(NO_RCFILE) && ISSET(HISTORYLOG))
	load_history();
#endif
#endif

#ifndef NANO_SMALL
    /* Set up the backup directory (unless we're using restricted mode,
     * in which case backups are disabled, since they would allow
     * reading from or writing to files not specified on the command
     * line).  This entails making sure it exists and is a directory, so
     * that backup files will be saved there. */
    if (!ISSET(RESTRICTED))
	init_backup_dir();
#endif

#ifndef DISABLE_OPERATINGDIR
    /* Set up the operating directory.  This entails chdir()ing there,
     * so that file reads and writes will be based there. */
    init_operating_dir();
#endif

#ifndef DISABLE_JUSTIFY
    if (punct == NULL)
	punct = mallocstrcpy(NULL, ".?!");

    if (brackets == NULL)
	brackets = mallocstrcpy(NULL, "'\")}]>");

    if (quotestr == NULL)
	quotestr = mallocstrcpy(NULL,
#ifdef HAVE_REGEX_H
		"^([ \t]*[|>:}#])+"
#else
		"> "
#endif
		);
#ifdef HAVE_REGEX_H
    quoterc = regcomp(&quotereg, quotestr, REG_EXTENDED);

    if (quoterc == 0) {
	/* We no longer need quotestr, just quotereg. */
	free(quotestr);
	quotestr = NULL;
    } else {
	size_t size = regerror(quoterc, &quotereg, NULL, 0);

	quoteerr = charalloc(size);
	regerror(quoterc, &quotereg, quoteerr, size);
    }
#else
    quotelen = strlen(quotestr);
#endif /* !HAVE_REGEX_H */
#endif /* !DISABLE_JUSTIFY */

#ifndef DISABLE_SPELLER
    /* If we don't have an alternative spell checker after reading the
     * command line and/or rcfile(s), check $SPELL for one, as Pico
     * does (unless we're using restricted mode, in which case spell
     * checking is disabled, since it would allow reading from or
     * writing to files not specified on the command line). */
    if (!ISSET(RESTRICTED) && alt_speller == NULL) {
	char *spellenv = getenv("SPELL");
	if (spellenv != NULL)
	    alt_speller = mallocstrcpy(NULL, spellenv);
    }
#endif

#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
    /* If whitespace wasn't specified, set its default value. */
    if (whitespace == NULL) {
	whitespace = mallocstrcpy(NULL, "  ");
	whitespace_len[0] = 1;
	whitespace_len[1] = 1;
    }
#endif

    /* If tabsize wasn't specified, set its default value. */
    if (tabsize == -1)
	tabsize = WIDTH_OF_TAB;

    /* Back up the old terminal settings so that they can be restored. */
    tcgetattr(0, &oldterm);

    /* Initialize curses mode. */
    initscr();

    /* Set up the terminal state. */
    terminal_init();

    /* Turn the cursor on for sure. */
    curs_set(1);

    /* Initialize the window size variables. */
    window_size_init();

    /* Set up the shortcuts. */
    shortcut_init(FALSE);

    /* Set up the signal handlers. */
    signal_init();

#ifdef DEBUG
    fprintf(stderr, "Main: set up windows\n");
#endif

    window_init();
#ifndef DISABLE_MOUSE
    mouse_init();
#endif

#ifdef DEBUG
    fprintf(stderr, "Main: open file\n");
#endif

    /* If there's a +LINE or +LINE,COLUMN flag here, it is the first
     * non-option argument, and it is followed by at least one other
     * argument, the filename it applies to. */
    if (0 < optind && optind < argc - 1 && argv[optind][0] == '+') {
	parse_line_column(&argv[optind][1], &startline, &startcol);
	optind++;
    }

#ifdef ENABLE_MULTIBUFFER
    old_multibuffer = ISSET(MULTIBUFFER);
    SET(MULTIBUFFER);

    /* Read all the files after the first one on the command line into
     * new buffers. */
    {
	int i = optind + 1;
	ssize_t iline = 1, icol = 1;

	for (; i < argc; i++) {
	    /* If there's a +LINE or +LINE,COLUMN flag here, it is
	     * followed by at least one other argument, the filename it
	     * applies to. */
	    if (i < argc - 1 && argv[i][0] == '+' && iline == 1 &&
		icol == 1)
		parse_line_column(&argv[i][1], &iline, &icol);
	    else {
		open_buffer(argv[i]);

		if (iline > 1 || icol > 1) {
		    do_gotolinecolumn(iline, icol, FALSE, FALSE, FALSE,
			FALSE);
		    iline = 1;
		    icol = 1;
		}
	    }
	}
    }
#endif

    /* Read the first file on the command line into either the current
     * buffer or a new buffer, depending on whether multibuffer mode is
     * enabled. */
    if (optind < argc)
	open_buffer(argv[optind]);

    /* We didn't open any files if all the command line arguments were
     * invalid files like directories or if there were no command line
     * arguments given.  In this case, we have to load a blank buffer.
     * Also, we unset view mode to allow editing. */
    if (openfile == NULL) {
	open_buffer("");
	UNSET(VIEW_MODE);
    }

#ifdef ENABLE_MULTIBUFFER
    if (!old_multibuffer)
	UNSET(MULTIBUFFER);
#endif

#ifdef DEBUG
    fprintf(stderr, "Main: top and bottom win\n");
#endif

    if (startline > 1 || startcol > 1)
	do_gotolinecolumn(startline, startcol, FALSE, FALSE, FALSE,
		FALSE);

    display_main_list();

#ifndef NANO_SMALL
    /* Return here after a SIGWINCH. */
    sigsetjmp(jmpbuf, 1);
#endif

    display_buffer();    

    while (TRUE) {
	bool meta_key, func_key, s_or_t, ran_func, finished;

	/* Make sure the cursor is in the edit window. */
	reset_cursor();

	/* If constant cursor position display is on, display the
	 * current cursor position on the statusbar. */
	if (ISSET(CONST_UPDATE))
	    do_cursorpos(TRUE);

	currshortcut = main_list;

	/* Read in and interpret characters. */
	do_input(&meta_key, &func_key, &s_or_t, &ran_func, &finished,
		TRUE);
    }

    assert(FALSE);
}
