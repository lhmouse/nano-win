/* $Id$ */
/**************************************************************************
 *   files.c                                                              *
 *                                                                        *
 *   Copyright (C) 1999-2004 Chris Allegretta                             *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

/* Set a default value for PATH_MAX, so we can use it below in lines like
	path = getcwd(NULL, PATH_MAX + 1); */
#ifndef PATH_MAX
#define PATH_MAX -1
#endif

#ifndef NANO_SMALL
static int fileformat = 0;	/* 0 = *nix, 1 = DOS, 2 = Mac */
#endif

/* Load file into edit buffer -- takes data from file struct. */
void load_file(int update)
{
    current = fileage;

#ifdef ENABLE_MULTIBUFFER
    /* if update is zero, add a new entry to the open_files structure;
       otherwise, update the current entry (the latter is needed in the
       case of the alternate spell checker) */
    add_open_file(update);
#endif

#ifdef ENABLE_COLOR
    update_color();
    if (ISSET(COLOR_SYNTAX))
	edit_refresh();
#endif
}

/* What happens when there is no file to open? aiee! */
void new_file(void)
{
    fileage = make_new_node(NULL);
    fileage->data = charalloc(1);
    fileage->data[0] = '\0';
    filebot = fileage;
    edittop = fileage;
    current = fileage;
    current_x = 0;
    totlines = 1;
    totsize = 0;

#ifdef ENABLE_MULTIBUFFER
    /* if there aren't any entries in open_files, create the entry for
       this new file; without this, if nano is started without a filename
       on the command line, a new file will be created, but it will be
       given no open_files entry */
    if (open_files == NULL) {
	add_open_file(FALSE);
	/* turn off view mode in this case; this is for consistency
	   whether multibuffers are compiled in or not */
	UNSET(VIEW_MODE);
    }
#else
    /* if multibuffers haven't been compiled in, turn off view mode
       unconditionally; otherwise, don't turn them off (except in the
       above case), so that we can view multiple files properly */
    UNSET(VIEW_MODE);
#endif

#ifdef ENABLE_COLOR
    update_color();
    if (ISSET(COLOR_SYNTAX))
	edit_refresh();
#endif
}

filestruct *read_line(char *buf, filestruct *prev, int *line1ins, size_t
	len)
{
    filestruct *fileptr = (filestruct *)nmalloc(sizeof(filestruct));

    /* nulls to newlines; len is the string's real length here */
    unsunder(buf, len);

    assert(strlen(buf) == len);

    fileptr->data = mallocstrcpy(NULL, buf);

#ifndef NANO_SMALL
    /* If it's a DOS file (CRLF), and file conversion isn't disabled,
       strip out the CR part */
    if (!ISSET(NO_CONVERT) && len > 0 && buf[len - 1] == '\r') {
	fileptr->data[len - 1] = '\0';
	totsize--;

	if (fileformat == 0)
	    fileformat = 1;
    }
#endif

    if (*line1ins != 0 || fileage == NULL) {
	/* Special case, insert with cursor on 1st line. */
	fileptr->prev = NULL;
	fileptr->next = fileage;
	fileptr->lineno = 1;
	if (*line1ins != 0) {
	    *line1ins = 0;
	    /* If we're inserting into the first line of the file, then
	       we want to make sure that our edit buffer stays on the
	       first line (and that fileage stays up to date!) */
	    edittop = fileptr;
	} else
	    filebot = fileptr;
	fileage = fileptr;
    } else {
	assert(prev != NULL);
	fileptr->prev = prev;
	fileptr->next = NULL;
	fileptr->lineno = prev->lineno + 1;
	prev->next = fileptr;
    }

    return fileptr;
}

void read_file(FILE *f, const char *filename, int quiet)
{
    int num_lines = 0, len = 0;
    char input = '\0';		/* current input character */
    char *buf;
    long i = 0, bufx = 128;
    filestruct *fileptr = current, *tmp = NULL;
#ifndef NANO_SMALL
    int old_no_convert = ISSET(NO_CONVERT);
#endif
    int line1ins = 0;
    int input_int;

    buf = charalloc(bufx);
    buf[0] = '\0';

    if (current != NULL) {
	if (current == fileage)
	    line1ins = 1;
	else
	    fileptr = current->prev;
	tmp = fileptr;
    }

    /* For the assertion in read_line(), it must be true that if current is
     * NULL then so is fileage. */
    assert(current != NULL || fileage == NULL);

    /* Read the entire file into file struct. */
    while ((input_int = getc(f)) != EOF) {
        input = (char)input_int;
#ifndef NANO_SMALL
	/* If the file has binary chars in it, don't stupidly
	   assume it's a DOS or Mac formatted file if it hasn't been
	   detected as one already! */
	if (fileformat == 0 && !ISSET(NO_CONVERT)
		&& is_cntrl_char(input) && input != '\t'
		&& input != '\r' && input != '\n')
	    SET(NO_CONVERT);
#endif

	if (input == '\n') {

	    /* read in the line properly */
	    fileptr = read_line(buf, fileptr, &line1ins, len);

	    /* reset the line length, in preparation for the next line */
	    len = 0;

	    num_lines++;
	    buf[0] = '\0';
	    i = 0;
#ifndef NANO_SMALL
	/* If it's a Mac file (no LF just a CR), and file conversion
	   isn't disabled, handle it! */
	} else if (!ISSET(NO_CONVERT) && i > 0 && buf[i - 1] == '\r') {
	    fileformat = 2;

	    /* read in the line properly */
	    fileptr = read_line(buf, fileptr, &line1ins, len);

	    /* reset the line length, in preparation for the next line;
	       since we've already read in the next character, reset it
	       to 1 instead of 0 */
	    len = 1;

	    num_lines++;
	    totsize++;
	    buf[0] = input;
	    buf[1] = '\0';
	    i = 1;
#endif
	} else {

	    /* Calculate the total length of the line; it might have
	       nulls in it, so we can't just use strlen(). */
	    len++;

	    /* Now we allocate a bigger buffer 128 characters at a time.
	       If we allocate a lot of space for one line, we may indeed
	       have to use a buffer this big later on, so we don't
	       decrease it at all.  We do free it at the end, though. */
	    if (i >= bufx - 1) {
		bufx += 128;
		buf = charealloc(buf, bufx);
	    }
	    buf[i] = input;
	    buf[i + 1] = '\0';
	    i++;
	}
	totsize++;
    }

    /* This conditional duplicates previous read_byte() behavior;
       perhaps this could use some better handling. */
    if (ferror(f))
	nperror(filename);
    fclose(f);

    /* Did we not get a newline but still have stuff to do? */
    if (len > 0) {
#ifndef NANO_SMALL
	/* If file conversion isn't disabled, the last character in
	   this file is a CR and fileformat isn't set yet, make sure
	   it's set to Mac format */
	if (!ISSET(NO_CONVERT) && buf[len - 1] == '\r' && fileformat == 0)
	    fileformat = 2;
#endif

	/* read in the LAST line properly */
	fileptr = read_line(buf, fileptr, &line1ins, len);

	num_lines++;
	totsize++;
	buf[0] = '\0';
    }
#ifndef NANO_SMALL
    else if (!ISSET(NO_CONVERT) && input == '\r') {
	/* If file conversion isn't disabled and the last character in
	   this file is a CR, read it in properly as a (Mac format)
	   line */
	buf[0] = input;
	buf[1] = '\0';
	len = 1;
	fileptr = read_line(buf, fileptr, &line1ins, len);
	num_lines++;
	totsize++;
	buf[0] = '\0';
    }
#endif

    free(buf);

#ifndef NANO_SMALL
    /* If NO_CONVERT wasn't set before we read the file, but it is now,
       unset it again. */
    if (!old_no_convert && ISSET(NO_CONVERT))
	UNSET(NO_CONVERT);
#endif

    /* Did we even GET a file if we don't already have one? */
    if (totsize == 0 || fileptr == NULL)
	new_file();

    /* Did we try to insert a file of 0 bytes? */
    if (num_lines != 0) {
	if (current != NULL) {
	    fileptr->next = current;
	    current->prev = fileptr;
	    renumber(current);
	    current_x = 0;
	    placewewant = 0;
	} else if (fileptr->next == NULL) {
	    filebot = fileptr;
	    new_magicline();
	    totsize--;
	    load_file(quiet);
	}
    }

#ifndef NANO_SMALL
    if (fileformat == 2)
	statusbar(P_("Read %d line (Converted from Mac format)",
			"Read %d lines (Converted from Mac format)",
			num_lines), num_lines);
    else if (fileformat == 1)
	statusbar(P_("Read %d line (Converted from DOS format)",
			"Read %d lines (Converted from DOS format)",
			num_lines), num_lines);
    else
#endif
	statusbar(P_("Read %d line", "Read %d lines", num_lines),
			num_lines);

#ifndef NANO_SMALL
    /* Set fileformat back to 0, now that we've read the file in and
       possibly converted it from DOS/Mac format. */
    fileformat = 0;
#endif

    totlines += num_lines;

    return;
}

/* Open the file (and decide if it exists).  Return TRUE on success,
 * FALSE on failure. */
bool open_file(const char *filename, int insert, int quiet)
{
    int fd;
    FILE *f;
    struct stat fileinfo;

    if (filename[0] == '\0' || stat(filename, &fileinfo) == -1) {
	if (insert && !quiet) {
	    statusbar(_("\"%s\" not found"), filename);
	    return FALSE;
	} else {
	    /* We have a new file */
	    statusbar(_("New File"));
	    new_file();
	}
    } else if (S_ISDIR(fileinfo.st_mode) || S_ISCHR(fileinfo.st_mode) ||
		S_ISBLK(fileinfo.st_mode)) {
	/* Don't open character or block files.  Sorry, /dev/sndstat! */
	statusbar(S_ISDIR(fileinfo.st_mode) ? _("\"%s\" is a directory") :
			_("File \"%s\" is a device file"), filename);
	if (!insert)
	    new_file();
	return FALSE;
    } else if ((fd = open(filename, O_RDONLY)) == -1) {
	/* If we're in multibuffer mode, don't be quiet when an error
	   occurs while opening a file */
	if (!quiet
#ifdef ENABLE_MULTIBUFFER
		|| ISSET(MULTIBUFFER)
#endif
		)
	    statusbar("%s: %s", strerror(errno), filename);
	if (!insert)
	    new_file();
	return FALSE;
    } else {			/* File is A-OK */
	if (!quiet)
	    statusbar(_("Reading File"));
	f = fdopen(fd, "rb"); /* Binary for our own line-end munging */
	if (f == NULL) {
	    nperror("fdopen");
	    close(fd);
	    return FALSE;
	}
	read_file(f, filename, quiet);
#ifndef NANO_SMALL
	stat(filename, &originalfilestat);
#endif
    }

    return TRUE;
}

/* This function will return the name of the first available extension
 * of a filename (starting with the filename, then filename.1, etc).
 * Memory is allocated for the return value.  If no writable extension
 * exists, we return "". */
char *get_next_filename(const char *name)
{
    int i = 0;
    char *buf = NULL;
    struct stat fs;

    buf = charalloc(strlen(name) + num_of_digits(INT_MAX) + 2);
    strcpy(buf, name);

    while (TRUE) {

	if (stat(buf, &fs) == -1)
	    return buf;
	if (i == INT_MAX)
	    break;

	i++;
	strcpy(buf, name);
	sprintf(&buf[strlen(name)], ".%d", i);
    }

    /* We get here only if there is no possible save file. */
    buf[0] = '\0';

    return buf;
}

void do_insertfile(int loading_file)
{
    int i, old_current_x = current_x;
    bool opened;
	/* TRUE if the file opened successfully. */
    char *realname = NULL;
    static char *inspath = NULL;

    if (inspath == NULL) {
	inspath = charalloc(1);
	inspath[0] = '\0';
    }

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

  start_again:
#if !defined(DISABLE_BROWSER) || !defined(DISABLE_MOUSE)
    currshortcut = insertfile_list;
#endif

#ifndef DISABLE_OPERATINGDIR
    if (operating_dir != NULL && strcmp(operating_dir, ".") != 0)
#ifdef ENABLE_MULTIBUFFER 
	if (ISSET(MULTIBUFFER))
	    i = statusq(TRUE, insertfile_list, inspath,
#ifndef NANO_SMALL
		NULL,
#endif
		_("File to insert into new buffer [from %s] "),
		operating_dir);
	else
#endif
	    i = statusq(TRUE, insertfile_list, inspath,
#ifndef NANO_SMALL
		NULL,
#endif
		_("File to insert [from %s] "),
		operating_dir);

    else
#endif
#ifdef ENABLE_MULTIBUFFER 
	if (ISSET(MULTIBUFFER))
	    i = statusq(TRUE, insertfile_list, inspath,
#ifndef NANO_SMALL
		NULL,
#endif
		_("File to insert into new buffer [from ./] "));
	else
#endif /* ENABLE_MULTIBUFFER */
	    i = statusq(TRUE, insertfile_list, inspath,
#ifndef NANO_SMALL
		NULL,
#endif
		_("File to insert [from ./] "));

    if (i != -1) {
	inspath = mallocstrcpy(inspath, answer);
#ifdef DEBUG
	fprintf(stderr, "filename is %s\n", answer);
#endif

#ifndef NANO_SMALL
#ifdef ENABLE_MULTIBUFFER
	if (i == TOGGLE_MULTIBUFFER_KEY) {
	    /* Don't allow toggling if we're in view mode. */
	    if (!ISSET(VIEW_MODE))
		TOGGLE(MULTIBUFFER);
	    loading_file = ISSET(MULTIBUFFER);
	    goto start_again;
	}
#endif /* ENABLE_MULTIBUFFER */

	if (i == NANO_EXTCMD_KEY) {
	    char *ans = mallocstrcpy(NULL, answer);
	    int ts = statusq(TRUE, extcmd_list, ans, NULL, 
		_("Command to execute"));

	    free(ans);

	    if (ts  == -1 || answer == NULL || answer[0] == '\0') {
		statusbar(_("Cancelled"));
		display_main_list();
		return;
	    }
	}
#endif /* !NANO_SMALL */
#ifndef DISABLE_BROWSER
	if (i == NANO_TOFILES_KEY) {
	    char *tmp = do_browse_from(answer);

	    if (tmp != NULL) {
		free(answer);
		answer = tmp;
		resetstatuspos = 1;
	    } else
		goto start_again;
	}
#endif

#ifndef DISABLE_OPERATINGDIR
	if (
#ifndef NANO_SMALL
		i != NANO_EXTCMD_KEY &&
#endif
		check_operating_dir(answer, FALSE) != 0) {
	    statusbar(_("Can't insert file from outside of %s"),
		operating_dir);
	    return;
	}
#endif

#ifdef ENABLE_MULTIBUFFER
	if (loading_file) {
	    /* update the current entry in the open_files structure */
	    add_open_file(TRUE);
	    new_file();
	    UNSET(MODIFIED);
#ifndef NANO_SMALL
	    UNSET(MARK_ISSET);
#endif
	}
#endif

#ifndef NANO_SMALL
	if (i == NANO_EXTCMD_KEY) {
	    realname = mallocstrcpy(realname, "");
	    opened = open_pipe(answer);
	} else {
#endif
	    realname = real_dir_from_tilde(answer);
	    opened = open_file(realname, TRUE, loading_file);
#ifndef NANO_SMALL
	}
#endif

#ifdef ENABLE_MULTIBUFFER
	if (loading_file) {
	    /* if there was an error opening the file, free() realname,
	       free() fileage (which now points to the new buffer we
	       created to hold the file), reload the buffer we had open
	       before, and skip the insertion; otherwise, save realname
	       in filename and continue the insertion */
	    if (!opened) {
		free(realname);
		free(fileage);
		load_open_file();
		goto skip_insert;
	    } else
		filename = mallocstrcpy(filename, realname);
	}
#endif

	free(realname);

#ifdef DEBUG
	dump_buffer(fileage);
#endif

#ifdef ENABLE_MULTIBUFFER
	if (loading_file)
	    load_file(FALSE);
	else
#endif
	    set_modified();

#ifdef ENABLE_MULTIBUFFER
	/* If we've loaded another file, update the titlebar's contents */
	if (loading_file) {
	    clearok(topwin, FALSE);
	    titlebar(NULL);

	    /* And re-init the shortcut list */
	    shortcut_init(FALSE);
	} else
#endif
	    /* Restore the old x-coordinate position */
	    current_x = old_current_x;

	/* If we've gone off the bottom, recenter; otherwise, just redraw */
	edit_refresh();

    } else {
	statusbar(_("Cancelled"));
	i = 0;
    }

#ifdef ENABLE_MULTIBUFFER
  skip_insert:
#endif

    free(inspath);
    inspath = NULL;

    display_main_list();
}

void do_insertfile_void(void)
{
#ifdef ENABLE_MULTIBUFFER
    if (ISSET(VIEW_MODE)) {
	if (ISSET(MULTIBUFFER))
	    do_insertfile(TRUE);
	else
	    statusbar(_("Key illegal in non-multibuffer mode"));
    }
    else
	do_insertfile(ISSET(MULTIBUFFER));
#else
    do_insertfile(FALSE);
#endif

    display_main_list();
}

#ifdef ENABLE_MULTIBUFFER
/* Create a new openfilestruct node. */
openfilestruct *make_new_opennode(openfilestruct *prevnode)
{
    openfilestruct *newnode = (openfilestruct *)nmalloc(sizeof(openfilestruct));

    newnode->filename = NULL;
    newnode->fileage = NULL;
    newnode->filebot = NULL;

    newnode->prev = prevnode;
    newnode->next = NULL;

    return newnode;
}

/* Splice a node into an existing openfilestruct. */
void splice_opennode(openfilestruct *begin, openfilestruct *newnode,
	openfilestruct *end)
{
    newnode->next = end;
    newnode->prev = begin;
    begin->next = newnode;
    if (end != NULL)
	end->prev = newnode;
}

/* Unlink a node from the rest of the openfilestruct. */
void unlink_opennode(const openfilestruct *fileptr)
{
    assert(fileptr != NULL);

    if (fileptr->prev != NULL)
	fileptr->prev->next = fileptr->next;

    if (fileptr->next != NULL)
	fileptr->next->prev = fileptr->prev;
}

/* Delete a node from the openfilestruct. */
void delete_opennode(openfilestruct *fileptr)
{
    if (fileptr != NULL) {
	if (fileptr->filename != NULL)
	    free(fileptr->filename);
	if (fileptr->fileage != NULL)
	    free_filestruct(fileptr->fileage);
	free(fileptr);
    }
}

/* Deallocate all memory associated with this and later files,
 * including the lines of text. */
void free_openfilestruct(openfilestruct *src)
{
    if (src != NULL) {
	while (src->next != NULL) {
	    src = src->next;
	    delete_opennode(src->prev);
#ifdef DEBUG
	    fprintf(stderr, "%s: free'd a node, YAY!\n", "delete_opennode()");
#endif
	}
	delete_opennode(src);
#ifdef DEBUG
	fprintf(stderr, "%s: free'd last node.\n", "delete_opennode()");
#endif
    }
}

/*
 * Add/update an entry to the open_files openfilestruct.  If update is
 * FALSE, a new entry is created; otherwise, the current entry is
 * updated.
 */
void add_open_file(int update)
{
    openfilestruct *tmp;

    if (fileage == NULL || current == NULL || filename == NULL)
	return;

    /* if no entries, make the first one */
    if (open_files == NULL)
	open_files = make_new_opennode(NULL);

    else if (!update) {

	/* otherwise, if we're not updating, make a new entry for
	   open_files and splice it in after the current one */

#ifdef DEBUG
	fprintf(stderr, "filename is %s\n", open_files->filename);
#endif

	tmp = make_new_opennode(NULL);
	splice_opennode(open_files, tmp, open_files->next);
	open_files = open_files->next;
    }

    /* save current filename */
    open_files->filename = mallocstrcpy(open_files->filename, filename);

#ifndef NANO_SMALL
    /* save the file's stat */
    open_files->originalfilestat = originalfilestat;
#endif

    /* save current total number of lines */
    open_files->file_totlines = totlines;

    /* save current total size */
    open_files->file_totsize = totsize;

    /* save current x-coordinate position */
    open_files->file_current_x = current_x;

    /* save current y-coordinate position */
    open_files->file_current_y = current_y;

    /* save current place we want */
    open_files->file_placewewant = placewewant;

    /* save current line number */
    open_files->file_lineno = current->lineno;

    /* start with default modification status: unmodified (and marking
       status, if available: unmarked) */
    open_files->file_flags = 0;

    /* if we're updating, save current modification status (and marking
       status, if available) */
    if (update) {
	if (ISSET(MODIFIED))
	    open_files->file_flags |= MODIFIED;
#ifndef NANO_SMALL
	if (ISSET(MARK_ISSET)) {
	    open_files->file_mark_beginbuf = mark_beginbuf;
	    open_files->file_mark_beginx = mark_beginx;
	    open_files->file_flags |= MARK_ISSET;
	}
#endif
    }

    /* if we're not in view mode and not updating, the file contents
       might have changed, so save the filestruct; otherwise, don't */
    if (!(ISSET(VIEW_MODE) && !update)) {
	/* save current file buffer */
	open_files->fileage = fileage;
	open_files->filebot = filebot;
    }

#ifdef DEBUG
    fprintf(stderr, "filename is %s\n", open_files->filename);
#endif
}

/*
 * Read the current entry in the open_files structure and set up the
 * currently open file using that entry's information.
 */
void load_open_file(void)
{
    if (open_files == NULL)
	return;

    /* set up the filename, the file buffer, the total number of lines in
       the file, and the total file size */
    filename = mallocstrcpy(filename, open_files->filename);
#ifndef NANO_SMALL
    originalfilestat = open_files->originalfilestat;
#endif
    fileage = open_files->fileage;
    current = fileage;
    filebot = open_files->filebot;
    totlines = open_files->file_totlines;
    totsize = open_files->file_totsize;

    /* restore modification status */
    if (open_files->file_flags & MODIFIED)
	SET(MODIFIED);
    else
	UNSET(MODIFIED);

#ifndef NANO_SMALL
    /* restore marking status */
    if (open_files->file_flags & MARK_ISSET) {
	mark_beginbuf = open_files->file_mark_beginbuf;
	mark_beginx = open_files->file_mark_beginx;
	SET(MARK_ISSET);
    } else
	UNSET(MARK_ISSET);
#endif

#ifdef ENABLE_COLOR
    update_color();
#endif

    /* restore full file position: line number, x-coordinate, y-
       coordinate, place we want */
    do_gotopos(open_files->file_lineno, open_files->file_current_x,
	open_files->file_current_y, open_files->file_placewewant);

    /* update the titlebar */
    clearok(topwin, FALSE);
    titlebar(NULL);
}

/*
 * Open the previous entry in the open_files structure.  If closing_file
 * is FALSE, update the current entry before switching from it.
 * Otherwise, we are about to close that entry, so don't bother doing
 * so.
 */
void open_prevfile(int closing_file)
{
    if (open_files == NULL)
	return;

    /* if we're not about to close the current entry, update it before
       doing anything */
    if (!closing_file)
	add_open_file(TRUE);

    if (open_files->prev == NULL && open_files->next == NULL) {

	/* only one file open */
	if (!closing_file)
	    statusbar(_("No more open file buffers"));
	return;
    }

    if (open_files->prev != NULL) {
	open_files = open_files->prev;

#ifdef DEBUG
	fprintf(stderr, "filename is %s\n", open_files->filename);
#endif

    }

    else if (open_files->next != NULL) {

	/* if we're at the beginning, wrap around to the end */
	while (open_files->next != NULL)
	    open_files = open_files->next;

#ifdef DEBUG
	    fprintf(stderr, "filename is %s\n", open_files->filename);
#endif

    }

    load_open_file();

    statusbar(_("Switched to %s"),
      ((open_files->filename[0] == '\0') ? "New Buffer" :
	open_files->filename));

#ifdef DEBUG
    dump_buffer(current);
#endif
}

void open_prevfile_void(void)
{
    open_prevfile(FALSE);
}

/*
 * Open the next entry in the open_files structure.  If closing_file is
 * FALSE, update the current entry before switching from it.  Otherwise,
 * we are about to close that entry, so don't bother doing so.
 */
void open_nextfile(int closing_file)
{
    if (open_files == NULL)
	return;

    /* if we're not about to close the current entry, update it before
       doing anything */
    if (!closing_file)
	add_open_file(TRUE);

    if (open_files->prev == NULL && open_files->next == NULL) {

	/* only one file open */
	if (!closing_file)
	    statusbar(_("No more open file buffers"));
	return;
    }

    if (open_files->next != NULL) {
	open_files = open_files->next;

#ifdef DEBUG
	fprintf(stderr, "filename is %s\n", open_files->filename);
#endif

    }
    else if (open_files->prev != NULL) {

	/* if we're at the end, wrap around to the beginning */
	while (open_files->prev != NULL) {
	    open_files = open_files->prev;

#ifdef DEBUG
	    fprintf(stderr, "filename is %s\n", open_files->filename);
#endif

	}
    }

    load_open_file();

    statusbar(_("Switched to %s"),
      ((open_files->filename[0] == '\0') ? "New Buffer" :
	open_files->filename));

#ifdef DEBUG
    dump_buffer(current);
#endif
}

void open_nextfile_void(void)
{
    open_nextfile(FALSE);
}

/*
 * Delete an entry from the open_files filestruct.  After deletion of an
 * entry, the next or previous entry is opened, whichever is found first.
 * Return 0 on success or 1 on error.
 */
int close_open_file(void)
{
    openfilestruct *tmp;

    if (open_files == NULL)
	return 1;

    /* make sure open_files->fileage and fileage, and open_files->filebot
       and filebot, are in sync; they might not be if lines have been cut
       from the top or bottom of the file */
    open_files->fileage = fileage;
    open_files->filebot = filebot;

    tmp = open_files;
    if (open_files->next != NULL)
	open_nextfile(TRUE);
    else if (open_files->prev != NULL)
	open_prevfile(TRUE);
    else
	return 1;

    unlink_opennode(tmp);
    delete_opennode(tmp);

    shortcut_init(FALSE);
    display_main_list();
    return 0;
}
#endif /* MULTIBUFFER */

#if !defined(DISABLE_SPELLER) || !defined(DISABLE_OPERATINGDIR) || !defined(NANO_SMALL)
/*
 * When passed "[relative path]" or "[relative path][filename]" in
 * origpath, return "[full path]" or "[full path][filename]" on success,
 * or NULL on error.  This is still done if the file doesn't exist but
 * the relative path does (since the file could exist in memory but not
 * yet on disk); it is not done if the relative path doesn't exist (since
 * the first call to chdir() will fail then).
 */
char *get_full_path(const char *origpath)
{
    char *newpath = NULL, *last_slash, *d_here, *d_there, *d_there_file, tmp;
    int path_only, last_slash_index;
    struct stat fileinfo;
    char *expanded_origpath;

    /* first, get the current directory, and tack a slash onto the end of
       it, unless it turns out to be "/", in which case leave it alone */

#ifdef PATH_MAX
    d_here = getcwd(NULL, PATH_MAX + 1);
#else
    d_here = getcwd(NULL, 0);
#endif

    if (d_here != NULL) {

	align(&d_here);
	if (strcmp(d_here, "/") != 0) {
	    d_here = charealloc(d_here, strlen(d_here) + 2);
	    strcat(d_here, "/");
	}

	/* stat origpath; if stat() fails, assume that origpath refers to
	   a new file that hasn't been saved to disk yet (i. e. set
	   path_only to 0); if stat() succeeds, set path_only to 0 if
	   origpath doesn't refer to a directory, or to 1 if it does */
	path_only = !stat(origpath, &fileinfo) && S_ISDIR(fileinfo.st_mode);

	expanded_origpath = real_dir_from_tilde(origpath);
	/* save the value of origpath in both d_there and d_there_file */
	d_there = mallocstrcpy(NULL, expanded_origpath);
	d_there_file = mallocstrcpy(NULL, expanded_origpath);
	free(expanded_origpath);

	/* if we have a path but no filename, tack slashes onto the ends
	   of both d_there and d_there_file, if they don't end in slashes
	   already */
	if (path_only) {
	    tmp = d_there[strlen(d_there) - 1];
	    if (tmp != '/') {
		d_there = charealloc(d_there, strlen(d_there) + 2);
		strcat(d_there, "/");
		d_there_file = charealloc(d_there_file, strlen(d_there_file) + 2);
		strcat(d_there_file, "/");
	    }
	}

	/* search for the last slash in d_there */
	last_slash = strrchr(d_there, '/');

	/* if we didn't find one, copy d_here into d_there; all data is
	   then set up */
	if (last_slash == NULL)
	    d_there = mallocstrcpy(d_there, d_here);
	else {
	    /* otherwise, remove all non-path elements from d_there
	       (i. e. everything after the last slash) */
	    last_slash_index = strlen(d_there) - strlen(last_slash);
	    null_at(&d_there, last_slash_index + 1);

	    /* and remove all non-file elements from d_there_file (i. e.
	       everything before and including the last slash); if we
	       have a path but no filename, don't do anything */
	    if (!path_only) {
		last_slash = strrchr(d_there_file, '/');
		last_slash++;
		strcpy(d_there_file, last_slash);
		align(&d_there_file);
	    }

	    /* now go to the path specified in d_there */
	    if (chdir(d_there) != -1) {
		/* get the full pathname, and save it back in d_there,
		   tacking a slash on the end if we have a path but no
		   filename; if the saving fails, get out */

		free(d_there);

#ifdef PATH_MAX
		d_there = getcwd(NULL, PATH_MAX + 1);
#else
		d_there = getcwd(NULL, 0);
#endif

		align(&d_there);
		if (d_there != NULL) {

		    /* add a slash to d_there, unless it's "/", in which
		       case we don't need it */
		    if (strcmp(d_there, "/") != 0) {
			d_there = charealloc(d_there, strlen(d_there) + 2);
			strcat(d_there, "/");
		    }
		}
		else
		    return NULL;
	    }

	    /* finally, go back to where we were before, d_here (no error
	       checking is done on this chdir(), because we can do
	       nothing if it fails) */
	    chdir(d_here);
	}
	
	/* all data is set up; fill in newpath */

	/* if we have a path and a filename, newpath = d_there +
	   d_there_file; otherwise, newpath = d_there */
	if (!path_only) {
	    newpath = charalloc(strlen(d_there) + strlen(d_there_file) + 1);
	    strcpy(newpath, d_there);
	    strcat(newpath, d_there_file);
	}
	else {
	    newpath = charalloc(strlen(d_there) + 1);
	    strcpy(newpath, d_there);
	}

	/* finally, clean up */
	free(d_there_file);
	free(d_there);
	free(d_here);
    }

    return newpath;
}
#endif /* !DISABLE_SPELLER || !DISABLE_OPERATINGDIR */

#ifndef DISABLE_SPELLER
/*
 * This function accepts a path and returns the full path (via
 * get_full_path()).  On error, if the path doesn't reference a
 * directory, or if the directory isn't writable, it returns NULL.
 */
char *check_writable_directory(const char *path)
{
    char *full_path = get_full_path(path);
    int writable;
    struct stat fileinfo;

    /* if get_full_path() failed, return NULL */
    if (full_path == NULL)
	return NULL;

    /* otherwise, stat() the full path to see if it's writable by the
       user; set writable to 1 if it is, or 0 if it isn't */
    writable = !stat(full_path, &fileinfo) && (fileinfo.st_mode & S_IWUSR);

    /* if the full path doesn't end in a slash (meaning get_full_path()
       found that it isn't a directory) or isn't writable, free full_path
       and return NULL */
    if (full_path[strlen(full_path) - 1] != '/' || writable == 0) {
	free(full_path);
	return NULL;
    }

    /* otherwise, return the full path */
    return full_path;
}

/*
 * This function accepts a directory name and filename prefix the same
 * way that tempnam() does, determines the location for its temporary
 * file the same way that tempnam() does, safely creates the temporary
 * file there via mkstemp(), and returns the name of the temporary file
 * the same way that tempnam() does.  It does not reference the value of
 * TMP_MAX because the total number of random filenames that it can
 * generate using one prefix is equal to 256**6, which is a sufficiently
 * large number to handle most cases.  Since the behavior after tempnam()
 * generates TMP_MAX random filenames is implementation-defined, my
 * implementation is to go on generating random filenames regardless of
 * it.
 */
char *safe_tempnam(const char *dirname, const char *filename_prefix)
{
    char *full_tempdir = NULL;
    const char *TMPDIR_env;
    int filedesc;

      /* if $TMPDIR is set and non-empty, set tempdir to it, run it through
         get_full_path(), and save the result in full_tempdir; otherwise,
         leave full_tempdir set to NULL */
    TMPDIR_env = getenv("TMPDIR");
    if (TMPDIR_env != NULL && TMPDIR_env[0] != '\0')
	full_tempdir = check_writable_directory(TMPDIR_env);

    /* if $TMPDIR is blank or isn't set, or isn't a writable
       directory, and dirname isn't NULL, try it; otherwise, leave
       full_tempdir set to NULL */
    if (full_tempdir == NULL && dirname != NULL)
	full_tempdir = check_writable_directory(dirname);

    /* if $TMPDIR is blank or isn't set, or if it isn't a writable
       directory, and dirname is NULL, try P_tmpdir instead */
    if (full_tempdir == NULL)
	full_tempdir = check_writable_directory(P_tmpdir);

    /* if P_tmpdir didn't work, use /tmp instead */
    if (full_tempdir == NULL) {
	full_tempdir = charalloc(6);
	strcpy(full_tempdir, "/tmp/");
    }

    full_tempdir = charealloc(full_tempdir, strlen(full_tempdir) + 12);

    /* like tempnam(), use only the first 5 characters of the prefix */
    strncat(full_tempdir, filename_prefix, 5);
    strcat(full_tempdir, "XXXXXX");
    filedesc = mkstemp(full_tempdir);

    /* if mkstemp succeeded, close the resulting file; afterwards, it'll be
       0 bytes long, so delete it; finally, return the filename (all that's
       left of it) */
    if (filedesc != -1) {
	close(filedesc);
	unlink(full_tempdir);
	return full_tempdir;
    }

    free(full_tempdir);
    return NULL;
}
#endif /* !DISABLE_SPELLER */

#ifndef DISABLE_OPERATINGDIR
/* Initialize full_operating_dir based on operating_dir. */
void init_operating_dir(void)
{
    assert(full_operating_dir == NULL);

    if (operating_dir == NULL)
	return;

    full_operating_dir = get_full_path(operating_dir);

    /* If get_full_path() failed or the operating directory is
     * inaccessible, unset operating_dir. */
    if (full_operating_dir == NULL || chdir(full_operating_dir) == -1) {
	free(full_operating_dir);
	full_operating_dir = NULL;
	free(operating_dir);
	operating_dir = NULL;
    }
}

/* Check to see if we're inside the operating directory.  Return 0 if we
 * are, or 1 otherwise.  If allow_tabcomp is nonzero, allow incomplete
 * names that would be matches for the operating directory, so that tab
 * completion will work. */
int check_operating_dir(const char *currpath, int allow_tabcomp)
{
    /* The char *full_operating_dir is global for mem cleanup.  It
     * should have already been initialized by init_operating_dir().
     * Also, a relative operating directory path will only be handled
     * properly if this is done. */

    char *fullpath;
    int retval = 0;
    const char *whereami1, *whereami2 = NULL;

    /* If no operating directory is set, don't bother doing anything. */
    if (operating_dir == NULL)
	return 0;

    assert(full_operating_dir != NULL);

    fullpath = get_full_path(currpath);

    /* fullpath == NULL means some directory in the path doesn't exist
     * or is unreadable.  If allow_tabcomp is zero, then currpath is
     * what the user typed somewhere.  We don't want to report a
     * non-existent directory as being outside the operating directory,
     * so we return 0.  If allow_tabcomp is nonzero, then currpath
     * exists, but is not executable.  So we say it isn't in the
     * operating directory. */
    if (fullpath == NULL)
	return allow_tabcomp;

    whereami1 = strstr(fullpath, full_operating_dir);
    if (allow_tabcomp)
	whereami2 = strstr(full_operating_dir, fullpath);

    /* If both searches failed, we're outside the operating directory.
     * Otherwise, check the search results; if the full operating
     * directory path is not at the beginning of the full current path
     * (for normal usage) and vice versa (for tab completion, if we're
     * allowing it), we're outside the operating directory. */
    if (whereami1 != fullpath && whereami2 != full_operating_dir)
	retval = 1;
    free(fullpath);	

    /* Otherwise, we're still inside it. */
    return retval;
}
#endif

#ifndef NANO_SMALL
void init_backup_dir(void)
{
    char *full_backup_dir;

    if (backup_dir == NULL)
	return;

    full_backup_dir = get_full_path(backup_dir);

    /* If get_full_path() failed or the backup directory is
     * inaccessible, unset backup_dir. */
    if (full_backup_dir == NULL ||
	full_backup_dir[strlen(full_backup_dir) - 1] != '/') {
	free(full_backup_dir);
	free(backup_dir);
	backup_dir = NULL;
    } else {
	free(backup_dir);
	backup_dir = full_backup_dir;
    }
}
#endif

/* Read from inn, write to out.  We assume inn is opened for reading,
 * and out for writing.  We return 0 on success, -1 on read error, -2 on
 * write error. */
int copy_file(FILE *inn, FILE *out)
{
    char buf[BUFSIZ];
    size_t charsread;
    int retval = 0;

    assert(inn != NULL && out != NULL);
    do {
	charsread = fread(buf, sizeof(char), BUFSIZ, inn);
	if (charsread == 0 && ferror(inn)) {
	    retval = -1;
	    break;
	}
	if (fwrite(buf, sizeof(char), charsread, out) < charsread) {
	    retval = -2;
	    break;
	}
    } while (charsread > 0);
    if (fclose(inn) == EOF)
	retval = -1;
    if (fclose(out) == EOF)
	retval = -2;
    return retval;
}

/* Write a file out.  If tmp is FALSE, we set the umask to disallow
 * anyone else from accessing the file, we don't set the global variable
 * filename to its name, and we don't print out how many lines we wrote
 * on the statusbar.
 *
 * tmp means we are writing a temporary file in a secure fashion.  We
 * use it when spell checking or dumping the file on an error.
 *
 * append == 1 means we are appending instead of overwriting.
 * append == 2 means we are prepending instead of overwriting.
 *
 * nonamechange means don't change the current filename.  It is ignored
 * if tmp is FALSE or if we're appending/prepending.
 *
 * Return -1 on error, 1 on success. */
int write_file(const char *name, int tmp, int append, int nonamechange)
{
    int retval = -1;
	/* Instead of returning in this function, you should always
	 * merely set retval and then goto cleanup_and_exit. */
    size_t lineswritten = 0;
    const filestruct *fileptr = fileage;
    int fd;
	/* The file descriptor we use. */
    mode_t original_umask = 0;
	/* Our umask, from when nano started. */
    int realexists;
	/* The result of stat().  TRUE if the file exists, FALSE
	 * otherwise.  If name is a link that points nowhere, realexists
	 * is FALSE. */
    struct stat st;
	/* The status fields filled in by stat(). */
    int anyexists;
	/* The result of lstat().  Same as realexists unless name is a
	 * link. */
    struct stat lst;
	/* The status fields filled in by lstat(). */
    char *realname;
	/* name after tilde expansion. */
    FILE *f;
	/* The actual file, realname, we are writing to. */
    char *tempname = NULL;
	/* The temp file name we write to on prepend. */

    assert(name != NULL);
    if (name[0] == '\0') {
	statusbar(_("Cancelled"));
	return -1;
    }
    if (!tmp)
	titlebar(NULL);

    realname = real_dir_from_tilde(name);

#ifndef DISABLE_OPERATINGDIR
    /* If we're writing a temporary file, we're probably going outside
     * the operating directory, so skip the operating directory test. */
    if (!tmp && check_operating_dir(realname, FALSE) != 0) {
	statusbar(_("Can't write outside of %s"), operating_dir);
	goto cleanup_and_exit;
    }
#endif

    anyexists = lstat(realname, &lst) != -1;
    /* New case: if the file exists, just give up. */
    if (tmp && anyexists)
	goto cleanup_and_exit;
    /* If NOFOLLOW_SYMLINKS is set, it doesn't make sense to prepend or
     * append to a symlink.  Here we warn about the contradiction. */
    if (ISSET(NOFOLLOW_SYMLINKS) && anyexists && S_ISLNK(lst.st_mode)) {
	statusbar(
		_("Cannot prepend or append to a symlink with --nofollow set"));
	goto cleanup_and_exit;
    }

    /* Save the state of file at the end of the symlink (if there is
     * one). */
    realexists = stat(realname, &st) != -1;

#ifndef NANO_SMALL
    /* We backup only if the backup toggle is set, the file isn't
     * temporary, and the file already exists.  Furthermore, if we
     * aren't appending, prepending, or writing a selection, we backup
     * only if the file has not been modified by someone else since nano
     * opened it. */
    if (ISSET(BACKUP_FILE) && !tmp && realexists != 0 &&
	(append != 0 || ISSET(MARK_ISSET) ||
	originalfilestat.st_mtime == st.st_mtime)) {

	FILE *backup_file;
	char *backupname;
	struct utimbuf filetime;
	int copy_status;

	/* Save the original file's access and modification times. */
	filetime.actime = originalfilestat.st_atime;
	filetime.modtime = originalfilestat.st_mtime;

	/* Open the original file to copy to the backup. */
	f = fopen(realname, "rb");
	if (f == NULL) {
	    statusbar(_("Error reading %s: %s"), realname,
		strerror(errno));
	    goto cleanup_and_exit;
	}

	/* If backup_dir is set, we set backupname to
	 * backup_dir/backupname~, where backupnae is the canonicalized
	 * absolute pathname of realname with every '/' replaced with a
	 * '!'.  This means that /home/foo/file is backed up in
	 * backup_dir/!home!foo!file~. */
	if (backup_dir != NULL) {
	    char *canon_realname = get_full_path(realname);
	    size_t i;

	    if (canon_realname == NULL)
		/* If get_full_path() failed, we don't have a
		 * canonicalized absolute pathname, so just use the
		 * filename portion of the pathname.  We use tail() so
		 * that e.g. ../backupname will be backed up in
		 * backupdir/backupname~ instead of
		 * backupdir/../backupname~. */
		canon_realname = mallocstrcpy(NULL, tail(realname));
	    else {
		for (i = 0; canon_realname[i] != '\0'; i++) {
		    if (canon_realname[i] == '/')
			canon_realname[i] = '!';
		}
	    }

	    backupname = charalloc(strlen(backup_dir) +
		strlen(canon_realname) + 2);
	    sprintf(backupname, "%s%s~", backup_dir, canon_realname);
	    free(canon_realname);
	} else {
	    backupname = charalloc(strlen(realname) + 2);
	    sprintf(backupname, "%s~", realname);
	}

	/* Open the destination backup file.  Before we write to it, we
	 * set its permissions, so no unauthorized person can read it as
	 * we write. */
	backup_file = fopen(backupname, "wb");
	if (backup_file == NULL ||
		chmod(backupname, originalfilestat.st_mode) == -1) {
	    statusbar(_("Error writing %s: %s"), backupname, strerror(errno));
	    free(backupname);
	    if (backup_file != NULL)
		fclose(backup_file);
	    fclose(f);
	    goto cleanup_and_exit;
	}

#ifdef DEBUG
	fprintf(stderr, "Backing up %s to %s\n", realname, backupname);
#endif

	/* Copy the file. */
	copy_status = copy_file(f, backup_file);
	/* And set metadata. */
	if (copy_status != 0 || chown(backupname, originalfilestat.st_uid,
		originalfilestat.st_gid) == -1 ||
		utime(backupname, &filetime) == -1) {
	    free(backupname);
	    if (copy_status == -1)
		statusbar(_("Error reading %s: %s"), realname, strerror(errno));
	    else
		statusbar(_("Error writing %s: %s"), backupname,
			strerror(errno));
	    goto cleanup_and_exit;
	}
	free(backupname);
    }
#endif /* !NANO_SMALL */

    /* If NOFOLLOW_SYMLINKS and the file is a link, we aren't doing
     * prepend or append.  So we delete the link first, and just
     * overwrite. */
    if (ISSET(NOFOLLOW_SYMLINKS) && anyexists && S_ISLNK(lst.st_mode) &&
	unlink(realname) == -1) {
	statusbar(_("Error writing %s: %s"), realname, strerror(errno));
	goto cleanup_and_exit;
    }

    original_umask = umask(0);
    umask(original_umask);

    /* If we create a temp file, we don't let anyone else access it.  We
     * create a temp file if tmp is TRUE or if we're prepending. */
    if (tmp || append == 2)
	umask(S_IRWXG | S_IRWXO);

    /* If we're prepending, copy the file to a temp file. */
    if (append == 2) {
	int fd_source;
	FILE *f_source = NULL;

	tempname = charalloc(strlen(realname) + 8);
	strcpy(tempname, realname);
	strcat(tempname, ".XXXXXX");
	fd = mkstemp(tempname);
	f = NULL;
	if (fd != -1) {
	    f = fdopen(fd, "wb");
	    if (f == NULL)
		close(fd);
	}
	if (f == NULL) {
	    statusbar(_("Error writing %s: %s"), tempname, strerror(errno));
	    unlink(tempname);
	    goto cleanup_and_exit;
	}

	fd_source = open(realname, O_RDONLY | O_CREAT);
	if (fd_source != -1) {
	    f_source = fdopen(fd_source, "rb");
	    if (f_source == NULL)
		close(fd_source);
	}
	if (f_source == NULL) {
	    statusbar(_("Error reading %s: %s"), realname, strerror(errno));
	    fclose(f);
	    unlink(tempname);
	    goto cleanup_and_exit;
	}

	if (copy_file(f_source, f) != 0) {
	    statusbar(_("Error writing %s: %s"), tempname, strerror(errno));
	    unlink(tempname);
	    goto cleanup_and_exit;
	}
    }

    /* Now open the file in place.  Use O_EXCL if tmp is TRUE.  This is
     * now copied from joe, because wiggy says so *shrug*. */
    fd = open(realname, O_WRONLY | O_CREAT |
	(append == 1 ? O_APPEND : (tmp ? O_EXCL : O_TRUNC)),
	S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    /* Set the umask back to the user's original value. */
    umask(original_umask);

    /* First, just give up if we couldn't even open the file. */
    if (fd == -1) {
	statusbar(_("Error writing %s: %s"), realname, strerror(errno));
	unlink(tempname);
	goto cleanup_and_exit;
    }

    f = fdopen(fd, append == 1 ? "ab" : "wb");
    if (f == NULL) {
	statusbar(_("Error writing %s: %s"), realname, strerror(errno));
	close(fd);
	goto cleanup_and_exit;
    }

    /* There might not be a magicline.  There won't be when writing out
     * a selection. */
    assert(fileage != NULL && filebot != NULL);
    while (fileptr != filebot) {
	size_t data_len = strlen(fileptr->data);
	size_t size;

	/* Newlines to nulls, just before we write to disk. */
	sunder(fileptr->data);

	size = fwrite(fileptr->data, sizeof(char), data_len, f);

	/* Nulls to newlines; data_len is the string's real length. */
	unsunder(fileptr->data, data_len);

	if (size < data_len) {
	    statusbar(_("Error writing %s: %s"), realname, strerror(errno));
	    fclose(f);
	    goto cleanup_and_exit;
	}
#ifndef NANO_SMALL
	if (ISSET(DOS_FILE) || ISSET(MAC_FILE))
	    if (putc('\r', f) == EOF) {
		statusbar(_("Error writing %s: %s"), realname, strerror(errno));
		fclose(f);
		goto cleanup_and_exit;
	    }

	if (!ISSET(MAC_FILE))
#endif
	    if (putc('\n', f) == EOF) {
		statusbar(_("Error writing %s: %s"), realname, strerror(errno));
		fclose(f);
		goto cleanup_and_exit;
	    }

	fileptr = fileptr->next;
	lineswritten++;
    }

    /* If we're prepending, open the temp file, and append it to f. */
    if (append == 2) {
	int fd_source;
	FILE *f_source = NULL;

	fd_source = open(tempname, O_RDONLY | O_CREAT);
	if (fd_source != -1) {
	    f_source = fdopen(fd_source, "rb");
	    if (f_source == NULL)
		close(fd_source);
	}
	if (f_source == NULL) {
	    statusbar(_("Error reading %s: %s"), tempname, strerror(errno));
	    fclose(f);
	    goto cleanup_and_exit;
	}

	if (copy_file(f_source, f) == -1
		|| unlink(tempname) == -1) {
	    statusbar(_("Error writing %s: %s"), realname, strerror(errno));
	    goto cleanup_and_exit;
	}
    } else if (fclose(f) == EOF) {
	statusbar(_("Error writing %s: %s"), realname, strerror(errno));
	unlink(tempname);
	goto cleanup_and_exit;
    }

    if (!tmp && append == 0) {
	if (!nonamechange) {
	    filename = mallocstrcpy(filename, realname);
#ifdef ENABLE_COLOR
	    update_color();
	    if (ISSET(COLOR_SYNTAX))
		edit_refresh();
#endif
	}

#ifndef NANO_SMALL
	/* Update originalfilestat to reference the file as it is now. */
	stat(filename, &originalfilestat);
#endif
	statusbar(P_("Wrote %u line", "Wrote %u lines", lineswritten),
		lineswritten);
	UNSET(MODIFIED);
	titlebar(NULL);
    }

    retval = 1;

  cleanup_and_exit:
    free(realname);
    free(tempname);
    return retval;
}

#ifndef NANO_SMALL
/* Write a marked selection from a file out.  First, set fileage and
 * filebot as the top and bottom of the mark, respectively.  Then call
 * write_file() with the values of name, temp, and append, and with
 * nonamechange set to TRUE so that we don't change the current
 * filename.  Finally, set fileage and filebot back to their old values
 * and return. */
int write_marked(const char *name, int tmp, int append)
{
    int retval = -1;
    filestruct *fileagebak = fileage;
    filestruct *filebotbak = filebot;
    int oldmod = ISSET(MODIFIED);
	/* write_file() unsets the MODIFIED flag. */
    size_t topx;
	/* The column of the beginning of the mark. */
    char origchar;
	/* We replace the character at the end of the mark with '\0'.
	 * We save the original character, to restore it. */
    char *origcharloc;
	/* The location of the character we nulled. */

    if (!ISSET(MARK_ISSET))
	return -1;

    /* Set fileage as the top of the mark, and filebot as the bottom. */
    if (current->lineno > mark_beginbuf->lineno ||
		(current->lineno == mark_beginbuf->lineno &&
		current_x > mark_beginx)) {
	fileage = mark_beginbuf;
	topx = mark_beginx;
	filebot = current;
	origcharloc = current->data + current_x;
    } else {
	fileage = current;
	topx = current_x;
	filebot = mark_beginbuf;
	origcharloc = mark_beginbuf->data + mark_beginx;
    }
    origchar = *origcharloc;
    *origcharloc = '\0';
    fileage->data += topx;

    /* If the line at filebot is blank, treat it as the magicline and
     * hence the end of the file.  Otherwise, treat the line after
     * filebot as the end of the file. */
    if (filebot->data[0] != '\0' && filebot->next != NULL)
	filebot = filebot->next;

    retval = write_file(name, tmp, append, TRUE);

    /* Now restore everything. */
    fileage->data -= topx;
    *origcharloc = origchar;
    fileage = fileagebak;
    filebot = filebotbak;
    if (oldmod)
	set_modified();

    return retval;
}
#endif /* !NANO_SMALL */

int do_writeout(int exiting)
{
    int i;
    int append = 0;
#ifdef NANO_EXTRA
    static int did_cred = FALSE;
#endif

#if !defined(DISABLE_BROWSER) || !defined(DISABLE_MOUSE)
    currshortcut = writefile_list;
#endif

    if (exiting && filename[0] != '\0' && ISSET(TEMP_FILE)) {
	i = write_file(filename, FALSE, 0, FALSE);
	if (i == 1) {
	    /* Write succeeded. */
	    display_main_list();
	    return 1;
	}
    }

#ifndef NANO_SMALL
    if (ISSET(MARK_ISSET) && !exiting)
	answer = mallocstrcpy(answer, "");
    else
#endif
	answer = mallocstrcpy(answer, filename);

    while (TRUE) {
	const char *msg;
#ifndef NANO_SMALL
	char *ans = mallocstrcpy(NULL, answer);
	const char *formatstr, *backupstr;

	if (ISSET(MAC_FILE))
	   formatstr = N_(" [Mac Format]");
	else if (ISSET(DOS_FILE))
	   formatstr = N_(" [DOS Format]");
	else
	   formatstr = "";

	if (ISSET(BACKUP_FILE))
	   backupstr = N_(" [Backup]");
	else
	   backupstr = "";

	/* Be nice to the translation folks. */
	if (ISSET(MARK_ISSET) && !exiting) {
	    if (append == 2)
		msg = N_("Prepend Selection to File");
	    else if (append == 1)
		msg = N_("Append Selection to File");
	    else
		msg = N_("Write Selection to File");
	} else
#endif /* !NANO_SMALL */
	if (append == 2)
	    msg = N_("File Name to Prepend to");
	else if (append == 1)
	    msg = N_("File Name to Append to");
	else
	    msg = N_("File Name to Write");

	/* If we're using restricted mode, the filename isn't blank,
	 * and we're at the "Write File" prompt, disable tab
	 * completion. */
	i = statusq(!ISSET(RESTRICTED) || filename[0] == '\0' ? TRUE :
		FALSE, writefile_list,
#ifndef NANO_SMALL
		ans, NULL, "%s%s%s", _(msg), formatstr, backupstr
#else
		filename, "%s", _(msg)
#endif
		);

#ifndef NANO_SMALL
	free(ans);
#endif

	if (i == -1) {
	    statusbar(_("Cancelled"));
	    display_main_list();
	    return -1;
	}

#ifndef DISABLE_BROWSER
	if (i == NANO_TOFILES_KEY) {
	    char *tmp = do_browse_from(answer);

	    currshortcut = writefile_list;
	    if (tmp == NULL)
		continue;
	    free(answer);
	    answer = tmp;
	} else
#endif /* !DISABLE_BROWSER */
#ifndef NANO_SMALL
	if (i == TOGGLE_DOS_KEY) {
	    UNSET(MAC_FILE);
	    TOGGLE(DOS_FILE);
	    continue;
	} else if (i == TOGGLE_MAC_KEY) {
	    UNSET(DOS_FILE);
	    TOGGLE(MAC_FILE);
	    continue;
	} else if (i == TOGGLE_BACKUP_KEY) {
	    TOGGLE(BACKUP_FILE);
	    continue;
	} else
#endif /* !NANO_SMALL */
	if (i == NANO_PREPEND_KEY) {
	    append = append == 2 ? 0 : 2;
	    continue;
	} else if (i == NANO_APPEND_KEY) {
	    append = append == 1 ? 0 : 1;
	    continue;
	}

#ifdef DEBUG
	fprintf(stderr, "filename is %s\n", answer);
#endif

#ifdef NANO_EXTRA
	if (exiting && !ISSET(TEMP_FILE) && strcasecmp(answer, "zzy") == 0
		&& !did_cred) {
	    do_credits();
	    did_cred = TRUE;
	    return -1;
	}
#endif
	if (append == 0 && strcmp(answer, filename) != 0) {
	    struct stat st;

	    if (!stat(answer, &st)) {
		i = do_yesno(FALSE, _("File exists, OVERWRITE ? "));
		if (i == 0 || i == -1)
		    continue;
	    /* If we're using restricted mode, we aren't allowed to
	     * change the name of a file once it has one because that
	     * would allow reading from or writing to files not
	     * specified on the command line.  In this case, don't
	     * bother showing the "Different Name" prompt. */
	    } else if (!ISSET(RESTRICTED) && filename[0] != '\0'
#ifndef NANO_SMALL
		&& (exiting || !ISSET(MARK_ISSET))
#endif
		) {
		i = do_yesno(FALSE, _("Save file under DIFFERENT NAME ? "));
		if (i == 0 || i == -1)
		    continue;
	    }
	}

#ifndef NANO_SMALL
	/* Here's where we allow the selected text to be written to a
	 * separate file.  If we're using restricted mode, this is
	 * disabled since it allows reading from or writing to files not
	 * specified on the command line. */
	if (!ISSET(RESTRICTED) && !exiting && ISSET(MARK_ISSET))
	    i = write_marked(answer, FALSE, append);
	else
#endif /* !NANO_SMALL */
	    i = write_file(answer, FALSE, append, FALSE);

#ifdef ENABLE_MULTIBUFFER
	/* If we're not about to exit, update the current entry in
	 * the open_files structure. */
	if (!exiting)
	    add_open_file(TRUE);
#endif
	display_main_list();
	return i;
    } /* while (TRUE) */
}

void do_writeout_void(void)
{
    do_writeout(FALSE);
}

/* Return a malloc()ed string containing the actual directory, used
 * to convert ~user and ~/ notation... */
char *real_dir_from_tilde(const char *buf)
{
    char *dirtmp = NULL;

    if (buf[0] == '~') {
	size_t i;
	const struct passwd *userdata;

	/* Figure how how much of the str we need to compare */
	for (i = 1; buf[i] != '/' && buf[i] != '\0'; i++)
	    ;

	/* Determine home directory using getpwuid() or getpwent(),
	   don't rely on $HOME */
	if (i == 1)
	    userdata = getpwuid(geteuid());
	else {
	    do {
		userdata = getpwent();
	    } while (userdata != NULL &&
			strncmp(userdata->pw_name, buf + 1, i - 1) != 0);
	}
	endpwent();

	if (userdata != NULL) {	/* User found */
	    dirtmp = charalloc(strlen(userdata->pw_dir) + strlen(buf + i) + 1);
	    sprintf(dirtmp, "%s%s", userdata->pw_dir, &buf[i]);
	}
    }

    if (dirtmp == NULL)
	dirtmp = mallocstrcpy(dirtmp, buf);

    return dirtmp;
}

#ifndef DISABLE_TABCOMP
/* Tack a slash onto the string we're completing if it's a directory.  We
 * assume there is room for one more character on the end of buf.  The
 * return value says whether buf is a directory. */
int append_slash_if_dir(char *buf, bool *lastwastab, int *place)
{
    char *dirptr = real_dir_from_tilde(buf);
    struct stat fileinfo;
    int ret = 0;

    assert(dirptr != buf);

    if (stat(dirptr, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode)) {
	strncat(buf, "/", 1);
	(*place)++;
	/* now we start over again with # of tabs so far */
	*lastwastab = FALSE;
	ret = 1;
    }

    free(dirptr);
    return ret;
}

/*
 * These functions (username_tab_completion(), cwd_tab_completion(), and
 * input_tab()) were taken from busybox 0.46 (cmdedit.c).  Here is the
 * notice from that file:
 *
 * Termios command line History and Editting, originally
 * intended for NetBSD sh (ash)
 * Copyright (c) 1999
 *      Main code:            Adam Rogoyski <rogoyski@cs.utexas.edu>
 *      Etc:                  Dave Cinege <dcinege@psychosis.com>
 *  Majorly adjusted/re-written for busybox:
 *                            Erik Andersen <andersee@debian.org>
 *
 * You may use this code as you wish, so long as the original author(s)
 * are attributed in any redistributions of the source code.
 * This code is 'as is' with no warranty.
 * This code may safely be consumed by a BSD or GPL license.
 */

char **username_tab_completion(char *buf, int *num_matches)
{
    char **matches = (char **)NULL;
    char *matchline = NULL;
    struct passwd *userdata;

    *num_matches = 0;
    matches = (char **)nmalloc(BUFSIZ * sizeof(char *));

    strcat(buf, "*");

    while ((userdata = getpwent()) != NULL) {

	if (check_wildcard_match(userdata->pw_name, &buf[1])) {

	    /* Cool, found a match.  Add it to the list
	     * This makes a lot more sense to me (Chris) this way...
	     */

#ifndef DISABLE_OPERATINGDIR
	    /* ...unless the match exists outside the operating
               directory, in which case just go to the next match */

	    if (operating_dir != NULL) {
		if (check_operating_dir(userdata->pw_dir, TRUE) != 0)
		    continue;
	    }
#endif

	    matchline = charalloc(strlen(userdata->pw_name) + 2);
	    sprintf(matchline, "~%s", userdata->pw_name);
	    matches[*num_matches] = matchline;
	    ++(*num_matches);

	    /* If there's no more room, bail out */
	    if (*num_matches == BUFSIZ)
		break;
	}
    }
    endpwent();

    return matches;
}

/* This was originally called exe_n_cwd_tab_completion, but we're not
   worried about executables, only filenames :> */

char **cwd_tab_completion(char *buf, int *num_matches)
{
    char *dirname, *dirtmp = NULL, *tmp = NULL, *tmp2 = NULL;
    char **matches = (char **)NULL;
    DIR *dir;
    struct dirent *next;

    matches = (char **)nmalloc(BUFSIZ * sizeof(char *));

    /* Stick a wildcard onto the buf, for later use */
    strcat(buf, "*");

    /* Okie, if there's a / in the buffer, strip out the directory part */
    if (buf[0] != '\0' && strstr(buf, "/") != NULL) {
	dirname = charalloc(strlen(buf) + 1);
	tmp = buf + strlen(buf);
	while (*tmp != '/' && tmp != buf)
	    tmp--;

	tmp++;

	strncpy(dirname, buf, tmp - buf + 1);
	dirname[tmp - buf] = '\0';

    } else {

#ifdef PATH_MAX
	if ((dirname = getcwd(NULL, PATH_MAX + 1)) == NULL)
#else
	/* The better, but apparently segfault-causing way */
	if ((dirname = getcwd(NULL, 0)) == NULL)
#endif /* PATH_MAX */
	    return matches;
	else
	    tmp = buf;
    }

#ifdef DEBUG
    fprintf(stderr, "\nDir = %s\n", dirname);
    fprintf(stderr, "\nbuf = %s\n", buf);
    fprintf(stderr, "\ntmp = %s\n", tmp);
#endif

    dirtmp = real_dir_from_tilde(dirname);
    free(dirname);
    dirname = dirtmp;

#ifdef DEBUG
    fprintf(stderr, "\nDir = %s\n", dirname);
    fprintf(stderr, "\nbuf = %s\n", buf);
    fprintf(stderr, "\ntmp = %s\n", tmp);
#endif


    dir = opendir(dirname);
    if (dir == NULL) {
	/* Don't print an error, just shut up and return */
	*num_matches = 0;
	beep();
	return matches;
    }
    while ((next = readdir(dir)) != NULL) {

#ifdef DEBUG
	fprintf(stderr, "Comparing \'%s\'\n", next->d_name);
#endif
	/* See if this matches */
	if (check_wildcard_match(next->d_name, tmp)) {

	    /* Cool, found a match.  Add it to the list
	     * This makes a lot more sense to me (Chris) this way...
	     */

#ifndef DISABLE_OPERATINGDIR
	    /* ...unless the match exists outside the operating
               directory, in which case just go to the next match; to
	       properly do operating directory checking, we have to add the
	       directory name to the beginning of the proposed match
	       before we check it */

	    if (operating_dir != NULL) {
		tmp2 = charalloc(strlen(dirname) + strlen(next->d_name) + 2);
		strcpy(tmp2, dirname);
		strcat(tmp2, "/");
		strcat(tmp2, next->d_name);
		if (check_operating_dir(tmp2, TRUE) != 0) {
		    free(tmp2);
		    continue;
		}
	        free(tmp2);
	    }
#endif

	    tmp2 = NULL;
	    tmp2 = charalloc(strlen(next->d_name) + 1);
	    strcpy(tmp2, next->d_name);
	    matches[*num_matches] = tmp2;
	    ++*num_matches;

	    /* If there's no more room, bail out */
	    if (*num_matches == BUFSIZ)
		break;
	}
    }
    closedir(dir);
    free(dirname);

    return matches;
}

/* This function now has an arg which refers to how much the statusbar
 * (place) should be advanced, i.e. the new cursor pos. */
char *input_tab(char *buf, int place, bool *lastwastab, int *newplace,
	bool *list)
{
    /* Do TAB completion */
    static int num_matches = 0, match_matches = 0;
    static char **matches = (char **)NULL;
    int pos = place, i = 0, col = 0, editline = 0;
    int longestname = 0, is_dir = 0;
    char *foo;

    *list = FALSE;

    if (*lastwastab == FALSE) {
	char *tmp, *copyto, *matchbuf;

	*lastwastab = TRUE;

	/* Make a local copy of the string -- up to the position of the
	   cursor */
	matchbuf = charalloc(strlen(buf) + 2);
	memset(matchbuf, '\0', strlen(buf) + 2);

	strncpy(matchbuf, buf, place);
	tmp = matchbuf;

	/* skip any leading white space */
	while (*tmp && isblank(*tmp))
	    ++tmp;

	/* Free up any memory already allocated */
	if (matches != NULL) {
	    for (i = i; i < num_matches; i++)
		free(matches[i]);
	    free(matches);
	    matches = (char **)NULL;
	    num_matches = 0;
	}

	/* If the word starts with `~' and there is no slash in the word, 
	 * then try completing this word as a username. */

	/* If the original string begins with a tilde, and the part
	   we're trying to tab-complete doesn't contain a slash, copy
	   the part we're tab-completing into buf, so tab completion
	   will result in buf's containing only the tab-completed
	   username. */
	if (buf[0] == '~' && strchr(tmp, '/') == NULL) {
	    buf = mallocstrcpy(buf, tmp);
	    matches = username_tab_completion(tmp, &num_matches);
	}
	/* If we're in the middle of the original line, copy the string
	   only up to the cursor position into buf, so tab completion
	   will result in buf's containing only the tab-completed
	   path/filename. */
	else if (strlen(buf) > strlen(tmp))
	    buf = mallocstrcpy(buf, tmp);

	/* Try to match everything in the current working directory that
	 * matches.  */
	if (matches == NULL)
	    matches = cwd_tab_completion(tmp, &num_matches);

	/* Don't leak memory */
	free(matchbuf);

#ifdef DEBUG
	fprintf(stderr, "%d matches found...\n", num_matches);
#endif
	/* Did we find exactly one match? */
	switch (num_matches) {
	case 0:
	    blank_edit();
	    wrefresh(edit);
	    break;
	case 1:

	    buf = charealloc(buf, strlen(buf) + strlen(matches[0]) + 1);

	    if (buf[0] != '\0' && strstr(buf, "/") != NULL) {
		for (tmp = buf + strlen(buf); *tmp != '/' && tmp != buf;
		     tmp--);
		tmp++;
	    } else
		tmp = buf;

	    if (strcmp(tmp, matches[0]) == 0)
		is_dir = append_slash_if_dir(buf, lastwastab, newplace);

	    if (is_dir != 0)
		break;

	    copyto = tmp;
	    for (pos = 0; *tmp == matches[0][pos] &&
		 pos <= strlen(matches[0]); pos++)
		tmp++;

	    /* write out the matched name */
	    strncpy(copyto, matches[0], strlen(matches[0]) + 1);
	    *newplace += strlen(matches[0]) - pos;

	    /* if an exact match is typed in and Tab is pressed,
	       *newplace will now be negative; in that case, make it
	       zero, so that the cursor will stay where it is instead of
	       moving backward */
	    if (*newplace < 0)
		*newplace = 0;

	    /* Is it a directory? */
	    append_slash_if_dir(buf, lastwastab, newplace);

	    break;
	default:
	    /* Check to see if all matches share a beginning, and, if so,
	       tack it onto buf and then beep */

	    if (buf[0] != '\0' && strstr(buf, "/") != NULL) {
		for (tmp = buf + strlen(buf); *tmp != '/' && tmp != buf;
		     tmp--);
		tmp++;
	    } else
		tmp = buf;

	    for (pos = 0; *tmp == matches[0][pos] && *tmp != '\0' &&
		 pos <= strlen(matches[0]); pos++)
		tmp++;

	    while (TRUE) {
		match_matches = 0;

		for (i = 0; i < num_matches; i++) {
		    if (matches[i][pos] == 0)
			break;
		    else if (matches[i][pos] == matches[0][pos])
			match_matches++;
		}
		if (match_matches == num_matches &&
		    (i == num_matches || matches[i] != 0)) {
		    /* All the matches have the same character at pos+1,
		       so paste it into buf... */
		    buf = charealloc(buf, strlen(buf) + 2);
		    strncat(buf, matches[0] + pos, 1);
		    *newplace += 1;
		    pos++;
		} else {
		    beep();
		    break;
		}
	    }
	}
    } else {
	/* Ok -- the last char was a TAB.  Since they
	 * just hit TAB again, print a list of all the
	 * available choices... */
	if (matches != NULL && num_matches > 1) {

	    /* Blank the edit window, and print the matches out there */
	    blank_edit();
	    wmove(edit, 0, 0);

	    editline = 0;

	    /* Figure out the length of the longest filename */
	    for (i = 0; i < num_matches; i++)
		if (strlen(matches[i]) > longestname)
		    longestname = strlen(matches[i]);

	    if (longestname > COLS - 1)
		longestname = COLS - 1;

	    foo = charalloc(longestname + 5);

	    /* Print the list of matches */
	    for (i = 0, col = 0; i < num_matches; i++) {

		/* make each filename shown be the same length as the longest
		   filename, with two spaces at the end */
		snprintf(foo, longestname + 1, matches[i]);
		while (strlen(foo) < longestname)
		    strcat(foo, " ");

		strcat(foo, "  ");

		/* Disable el cursor */
		curs_set(0);
		/* now, put the match on the screen */
		waddnstr(edit, foo, strlen(foo));
		col += strlen(foo);

		/* And if the next match isn't going to fit on the
		   line, move to the next one */
		if (col > COLS - longestname && i + 1 < num_matches) {
		    editline++;
		    wmove(edit, editline, 0);
		    if (editline == editwinrows - 1) {
			waddstr(edit, _("(more)"));
			break;
		    }
		    col = 0;
		}
	    }
	    free(foo);
	    wrefresh(edit);
	    *list = TRUE;
	} else
	    beep();
    }

    /* Only refresh the edit window if we don't have a list of filename
       matches on it */
    if (*list == FALSE)
	edit_refresh();
    curs_set(1);
    return buf;
}
#endif /* !DISABLE_TABCOMP */

/* Only print the last part of a path; isn't there a shell
 * command for this? */
const char *tail(const char *foo)
{
    const char *tmp = foo + strlen(foo);

    while (*tmp != '/' && tmp != foo)
	tmp--;

    if (*tmp == '/')
	tmp++;

    return tmp;
}

#ifndef DISABLE_BROWSER
/* Our sort routine for file listings -- sort directories before
 * files, and then alphabetically. */ 
int diralphasort(const void *va, const void *vb)
{
    struct stat fileinfo;
    const char *a = *(char *const *)va, *b = *(char *const *)vb;
    int aisdir = stat(a, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode);
    int bisdir = stat(b, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode);

    if (aisdir != 0 && bisdir == 0)
	return -1;
    if (aisdir == 0 && bisdir != 0)
	return 1;

    return strcasecmp(a, b);
}

/* Free our malloc()ed memory */
void free_charptrarray(char **array, size_t len)
{
    for (; len > 0; len--)
	free(array[len - 1]);
    free(array);
}

/* Strip one dir from the end of a string. */
void striponedir(char *foo)
{
    char *tmp;

    assert(foo != NULL);
    /* Don't strip the root dir */
    if (*foo == '\0' || strcmp(foo, "/") == 0)
	return;

    tmp = foo + strlen(foo) - 1;
    assert(tmp >= foo);
    if (*tmp == '/')
	*tmp = '\0';

    while (*tmp != '/' && tmp != foo)
	tmp--;

    if (tmp != foo)
	*tmp = '\0';
    else { /* SPK may need to make a 'default' path here */
        if (*tmp != '/')
	    *tmp = '.';
	*(tmp + 1) = '\0';
    }
}

int readable_dir(const char *path)
{
    DIR *dir = opendir(path);

    /* If dir is NULL, don't do closedir(), since that changes errno. */
    if (dir != NULL)
	closedir(dir);
    return dir != NULL;
}

/* Initialize the browser code, including the list of files in *path */
char **browser_init(const char *path, int *longest, int *numents)
{
    DIR *dir;
    struct dirent *next;
    char **filelist;
    int i = 0;
    size_t path_len;

    dir = opendir(path);
    if (dir == NULL)
	return NULL;

    *numents = 0;
    while ((next = readdir(dir)) != NULL) {
	if (strcmp(next->d_name, ".") == 0)
	   continue;
	(*numents)++;
	if (strlen(next->d_name) > *longest)
	    *longest = strlen(next->d_name);
    }
    rewinddir(dir);
    *longest += 10;

    filelist = (char **)nmalloc(*numents * sizeof (char *));

    if (strcmp(path, "/") == 0)
	path = "";
    path_len = strlen(path);

    while ((next = readdir(dir)) != NULL) {
	if (strcmp(next->d_name, ".") == 0)
	   continue;

	filelist[i] = charalloc(strlen(next->d_name) + path_len + 2);
	sprintf(filelist[i], "%s/%s", path, next->d_name);
	i++;
    }
    closedir(dir);

    if (*longest > COLS - 1)
	*longest = COLS - 1;

    return filelist;
}

/* Our browser function.  inpath is the path to start browsing from */
char *do_browser(const char *inpath)
{
    struct stat st;
    char *foo, *retval = NULL;
    static char *path = NULL;
    int numents = 0, i = 0, j = 0, kbinput = ERR, meta_key, longest = 0;
    int abort = 0, col = 0, selected = 0, editline = 0, width = 0;
    int filecols = 0, lineno = 0;
    char **filelist = (char **)NULL;
#ifndef DISABLE_MOUSE
    MEVENT mevent;
#endif

    assert(inpath != NULL);

    /* If path isn't the same as inpath, we are being passed a new
	dir as an arg.  We free it here so it will be copied from 
	inpath below */
    if (path != NULL && strcmp(path, inpath) != 0) {
	free(path);
	path = NULL;
    }

    /* if path doesn't exist, make it so */
    if (path == NULL)
	path = mallocstrcpy(NULL, inpath);

    filelist = browser_init(path, &longest, &numents);
    foo = charalloc(longest + 8);

    /* Sort the list by directory first, then alphabetically */
    qsort(filelist, numents, sizeof(char *), diralphasort);

    titlebar(path);
    bottombars(browser_list);
    curs_set(0);
    wmove(edit, 0, 0);
    i = 0;
    width = 0;
    filecols = 0;

    /* Loop invariant: Microsoft sucks. */
    do {
	char *new_path;
	    /* Used by the Go To Directory prompt. */

	check_statblank();

#if !defined(DISABLE_HELP) || !defined(DISABLE_MOUSE)
	currshortcut = browser_list;
#endif

 	editline = 0;
	col = 0;
	    
	/* Compute line number we're on now, so we don't divide by zero later */
	lineno = selected;
	if (width != 0)
	    lineno /= width;

	switch (kbinput) {

#ifndef DISABLE_MOUSE
	case KEY_MOUSE:
	    if (getmouse(&mevent) == ERR)
		return retval;
 
	    /* If they clicked in the edit window, they probably clicked
		on a file */
	    if (wenclose(edit, mevent.y, mevent.x)) { 
		int selectedbackup = selected;

		mevent.y -= 2;

		/* Longest is the width of each column.  There are two
		 * spaces between each column. */
		selected = (lineno / editwinrows) * editwinrows * width
			+ mevent.y * width + mevent.x / (longest + 2);

		/* If they clicked beyond the end of a row, select the
		 * end of that row. */
		if (mevent.x > width * (longest + 2))
		    selected--;

		/* If we're off the screen, reset to the last item.
		   If we clicked where we did last time, select this name! */
		if (selected > numents - 1)
		    selected = numents - 1;
		else if (selectedbackup == selected)
		    ungetch('s');	/* Unget the 'select' key */
	    } else	/* Must be clicking a shortcut */
		do_mouse();

            break;
#endif
	case NANO_PREVLINE_KEY:
	    if (selected - width >= 0)
		selected -= width;
	    break;
	case NANO_BACK_KEY:
	    if (selected > 0)
		selected--;
	    break;
	case NANO_NEXTLINE_KEY:
	    if (selected + width <= numents - 1)
		selected += width;
	    break;
	case NANO_FORWARD_KEY:
	    if (selected < numents - 1)
		selected++;
	    break;
	case NANO_PREVPAGE_KEY:
	case NANO_PREVPAGE_FKEY:
	case '-': /* Pico compatibility */
	    if (selected >= (editwinrows + lineno % editwinrows) * width)
		selected -= (editwinrows + lineno % editwinrows) * width;
	    else
		selected = 0;
	    break;
	case NANO_NEXTPAGE_KEY:
	case NANO_NEXTPAGE_FKEY:
	case ' ': /* Pico compatibility */
	    selected += (editwinrows - lineno % editwinrows) * width;
	    if (selected >= numents)
		selected = numents - 1;
	    break;
	case NANO_HELP_KEY:
	case NANO_HELP_FKEY:
	case '?': /* Pico compatibility */
#ifndef DISABLE_HELP
	    do_help();
	    curs_set(0);
#else
	    nano_disabled_msg();
#endif
	    break;
	case NANO_ENTER_KEY:
	case 'S': /* Pico compatibility */
	case 's':
	    /* You can't cd up from / */
	    if (strcmp(filelist[selected], "/..") == 0 &&
		strcmp(path, "/") == 0) {
		statusbar(_("Can't move up a directory"));
		beep();
		break;
	    }

#ifndef DISABLE_OPERATINGDIR
	    /* Note: the selected file can be outside the operating
	     * directory if it is .. or if it is a symlink to 
	     * directory outside the operating directory. */
	    if (check_operating_dir(filelist[selected], FALSE) != 0) {
		statusbar(_("Can't go outside of %s in restricted mode"),
			operating_dir);
		beep();
		break;
	    }
#endif

	    if (stat(filelist[selected], &st) == -1) {
		statusbar(_("Can't open \"%s\": %s"), filelist[selected],
			strerror(errno));
		beep();
		break;
	    }

	    if (!S_ISDIR(st.st_mode)) {
		retval = mallocstrcpy(retval, filelist[selected]);
		abort = 1;
		break;
	    }

	    new_path = mallocstrcpy(NULL, filelist[selected]);

	    if (strcmp("..", tail(new_path)) == 0) {
		/* They want to go up a level, so strip off .. and the
		   current dir */
		striponedir(new_path);
		/* SPK for '.' path, get the current path via getcwd */
		if (strcmp(new_path, ".") == 0) {
		    free(new_path);
		    new_path = getcwd(NULL, PATH_MAX + 1);
		}
		striponedir(new_path);
	    }

	    if (!readable_dir(new_path)) {
		/* We can't open this dir for some reason.  Complain */
		statusbar(_("Can't open \"%s\": %s"), new_path,
			strerror(errno));
		free(new_path);
		break;
	    }

	    free_charptrarray(filelist, numents);
	    free(foo);
	    free(path);
	    path = new_path;
	    return do_browser(path);

	/* Goto a specific directory */
	case NANO_GOTO_KEY:
	case NANO_GOTO_FKEY:
	case 'G': /* Pico compatibility */
	case 'g':
	    curs_set(1);
	    j = statusq(FALSE, gotodir_list, "",
#ifndef NANO_SMALL
		NULL,
#endif
		_("Goto Directory"));
	    bottombars(browser_list);
	    curs_set(0);

	    if (j < 0) {
		statusbar(_("Goto Cancelled"));
		break;
	    }

	    new_path = real_dir_from_tilde(answer);

	    if (new_path[0] != '/') {
		new_path = charealloc(new_path, strlen(path) + strlen(answer) + 2);
		sprintf(new_path, "%s/%s", path, answer);
	    }

#ifndef DISABLE_OPERATINGDIR
	    if (check_operating_dir(new_path, FALSE) != 0) {
		statusbar(_("Can't go outside of %s in restricted mode"), operating_dir);
		free(new_path);
		break;
	    }
#endif

	    if (!readable_dir(new_path)) {
		/* We can't open this dir for some reason.  Complain */
		statusbar(_("Can't open \"%s\": %s"), answer, strerror(errno));
		free(new_path);
		break;
	    }

	    /* Start over again with the new path value */
	    free_charptrarray(filelist, numents);
	    free(foo);
	    free(path);
	    path = new_path;
	    return do_browser(path);

	/* Stuff we want to abort the browser */
	case NANO_CANCEL_KEY:
	case NANO_EXIT_KEY:
	case NANO_EXIT_FKEY:
	case 'E': /* Pico compatibility */
	case 'e':
	    abort = 1;
	    break;
	}
	if (abort)
	    break;

	blank_edit();

	if (width != 0)
	    i = width * editwinrows * ((selected / width) / editwinrows);
	else
	    i = 0;

	wmove(edit, 0, 0);
	for (j = i; j < numents && editline <= editwinrows - 1; j++) {
	    filecols++;

	    strncpy(foo, tail(filelist[j]), strlen(tail(filelist[j])) + 1);
	    while (strlen(foo) < longest)
		strcat(foo, " ");
	    col += strlen(foo);

	    /* Put file info in the string also */
	    /* We use lstat here to detect links; then, if we find a
		symlink, we examine it via stat() to see if it is a
		directory or just a file symlink */
	    lstat(filelist[j], &st);
	    if (S_ISDIR(st.st_mode))
		strcpy(foo + longest - 5, "(dir)");
	    else {
		if (S_ISLNK(st.st_mode)) {
		     /* Aha!  It's a symlink!  Now, is it a dir?  If so,
			mark it as such */
		    stat(filelist[j], &st);
		    if (S_ISDIR(st.st_mode))
			strcpy(foo + longest - 5, "(dir)");
		    else
			strcpy(foo + longest - 2, "--");
		} else if (st.st_size < (1 << 10)) /* less than 1 K */
		    sprintf(foo + longest - 7, "%4d  B", 
			(int) st.st_size);
		else if (st.st_size >= (1 << 30)) /* at least 1 gig */
		    sprintf(foo + longest - 7, "%4d GB", 
			(int) st.st_size >> 30);
		else if (st.st_size >= (1 << 20)) /* at least 1 meg */
		    sprintf(foo + longest - 7, "%4d MB", 
			(int) st.st_size >>     20);
		else /* It's more than 1 k and less than a meg */
		    sprintf(foo + longest - 7, "%4d KB", 
			(int) st.st_size >> 10);
	    }

	    /* Highlight the currently selected file/dir */
	    if (j == selected)
		wattron(edit, A_REVERSE);
	    waddstr(edit, foo);
	    if (j == selected)
		wattroff(edit, A_REVERSE);

	    /* And add some space between the cols */
	    waddstr(edit, "  ");
	    col += 2;

	    /* And if the next entry isn't going to fit on the
		line, move to the next one */
	    if (col > COLS - longest) {
		editline++;
		wmove(edit, editline, 0);
		col = 0;
		if (width == 0)
		    width = filecols;
	    }
	}
 	wrefresh(edit);
    } while ((kbinput = get_kbinput(edit, &meta_key)) != NANO_EXIT_KEY && kbinput != NANO_EXIT_FKEY);
    curs_set(1);
    blank_edit();
    titlebar(NULL);
    edit_refresh();

    /* cleanup */
    free_charptrarray(filelist, numents);
    free(foo);
    return retval;
}

/* Browser front end, checks to see if inpath has a dir in it and, if so,
 starts do_browser from there, else from the current dir */
char *do_browse_from(const char *inpath)
{
    struct stat st;
    char *bob;
	/* The result of do_browser; the selected file name. */
    char *path;
	/* inpath, tilde expanded. */

    assert(inpath != NULL);

    path = real_dir_from_tilde(inpath);

    /*
     * Perhaps path is a directory.  If so, we will pass that to
     * do_browser.  Otherwise, perhaps path is a directory / a file.  So
     * we try stripping off the last path element.  If it still isn't a
     * directory, just use the current directory. */

    if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
	striponedir(path);
	if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
	    free(path);
	    path = getcwd(NULL, PATH_MAX + 1);
	}
    }

#ifndef DISABLE_OPERATINGDIR
    /* If the resulting path isn't in the operating directory, use that. */
    if (check_operating_dir(path, FALSE) != 0)
	path = mallocstrcpy(path, operating_dir);
#endif

    if (!readable_dir(path)) {
	beep();
	bob = NULL;
    } else
	bob = do_browser(path);
    free(path);
    return bob;
}
#endif /* !DISABLE_BROWSER */

#ifndef NANO_SMALL
#ifdef ENABLE_NANORC
void load_history(void)
{
    FILE *hist;
    const struct passwd *userage = NULL;
    static char *nanohist;
    char *buf, *ptr;
    char *homenv = getenv("HOME");
    historyheadtype *history = &search_history;


    if (homenv != NULL) {
        nanohist = charealloc(nanohist, strlen(homenv) + 15);
        sprintf(nanohist, "%s/.nano_history", homenv);
    } else {
	userage = getpwuid(geteuid());
	endpwent();
        nanohist = charealloc(nanohist, strlen(userage->pw_dir) + 15);
        sprintf(nanohist, "%s/.nano_history", userage->pw_dir);
    }

    /* assume do_rcfile has reported missing home dir */

    if (homenv != NULL || userage != NULL) {
	hist = fopen(nanohist, "r");
	if (hist == NULL) {
            if (errno != ENOENT) {
		/* Don't save history when we quit. */
		UNSET(HISTORYLOG);
		rcfile_error(N_("Unable to open ~/.nano_history file: %s\n"), strerror(errno));
	    }
	    free(nanohist);
	} else {
	    buf = charalloc(1024);
	    while (fgets(buf, 1023, hist) != 0) {
		ptr = buf;
		while (*ptr != '\n' && *ptr != '\0' && ptr < buf + 1023)
		    ptr++;
		*ptr = '\0';
		if (strlen(buf))
		    update_history(history, buf);
		else
		    history = &replace_history;
	    }
	    fclose(hist);
	    free(buf);
	    free(nanohist);
	    UNSET(HISTORY_CHANGED);
	}
    }
}

/* save histories to ~/.nano_history */
void save_history(void)
{
    FILE *hist;
    const struct passwd *userage = NULL;
    char *nanohist = NULL;
    char *homenv = getenv("HOME");
    historytype *h;

    /* don't save unchanged or empty histories */
    if ((search_history.count == 0 && replace_history.count == 0) ||
			!ISSET(HISTORY_CHANGED) || ISSET(VIEW_MODE))
	return;

    if (homenv != NULL) {
	nanohist = charealloc(nanohist, strlen(homenv) + 15);
	sprintf(nanohist, "%s/.nano_history", homenv);
    } else {
	userage = getpwuid(geteuid());
	endpwent();
	nanohist = charealloc(nanohist, strlen(userage->pw_dir) + 15);
	sprintf(nanohist, "%s/.nano_history", userage->pw_dir);
    }

    if (homenv != NULL || userage != NULL) {
	hist = fopen(nanohist, "wb");
	if (hist == NULL)
	    rcfile_error(N_("Unable to write ~/.nano_history file: %s\n"), strerror(errno));
	else {
	    /* set rw only by owner for security ?? */
	    chmod(nanohist, S_IRUSR | S_IWUSR);
	    /* write oldest first */
	    for (h = search_history.tail; h->prev; h = h->prev) {
		h->data = charealloc(h->data, strlen(h->data) + 2);
		strcat(h->data, "\n");
		if (fputs(h->data, hist) == EOF) {
		    rcfile_error(N_("Unable to write ~/.nano_history file: %s\n"), strerror(errno));
		    goto come_from;
		}
	    }
	    if (fputs("\n", hist) == EOF) {
		    rcfile_error(N_("Unable to write ~/.nano_history file: %s\n"), strerror(errno));
		    goto come_from;
	    }
	    for (h = replace_history.tail; h->prev; h = h->prev) {
		h->data = charealloc(h->data, strlen(h->data) + 2);
		strcat(h->data, "\n");
		if (fputs(h->data, hist) == EOF) {
		    rcfile_error(N_("Unable to write ~/.nano_history file: %s\n"), strerror(errno));
		    goto come_from;
		}
	    }
  come_from:
	    fclose(hist);
	}
	free(nanohist);
    }
}
#endif /* ENABLE_NANORC */
#endif /* !NANO_SMALL */
