/* $Id$ */
/**************************************************************************
 *   files.c                                                              *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <pwd.h>

#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/* statics for here */
#ifndef NANO_SMALL
static int fileformat = 0;	/* 0 = *nix, 1 = DOS, 2 = Mac */
#endif

/* Load file into edit buffer - takes data from file struct */
void load_file(int quiet)
{
    current = fileage;

#ifdef ENABLE_MULTIBUFFER
    /* if quiet is zero, add a new entry to the open_files structure;
       otherwise, update the current entry (the latter is needed in the
       case of the alternate spell checker) */
    add_open_file(quiet);
#endif

    wmove(edit, current_y, current_x);
}

/* What happens when there is no file to open? aiee! */
void new_file(void)
{
    fileage = nmalloc(sizeof(filestruct));
    fileage->data = charalloc(1);
    strcpy(fileage->data, "");
    fileage->prev = NULL;
    fileage->next = NULL;
    fileage->lineno = 1;
    filebot = fileage;
    edittop = fileage;
    editbot = fileage;
    current = fileage;
    totlines = 1;
    totsize = 0;

#ifdef ENABLE_MULTIBUFFER
    /* if there aren't any entries in open_files, create the entry for
       this new file; without this, if nano is started without a filename
       on the command line, a new file will be created, but it will be
       given no open_files entry, leading to problems later on */
    if (!open_files) {
	add_open_file(0);
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

}

int read_byte(int fd, char *filename, char *input)
{
    static char buf[BUFSIZ];
    static int index = 0;
    static int size = 0;

    if (index == size) {
	index = 0;
	size = read(fd, buf, BUFSIZ);
	if (size == -1) {
	    clear();
	    refresh();
	    resetty();
	    endwin();
	    perror(filename);
	    total_refresh();
	    return -1;
	}
	if (!size)
	    return 0;
    }
    *input = buf[index++];
    return 1;
}

filestruct *read_line(char *buf, filestruct * prev, int *line1ins)
{
    filestruct *fileptr;

    fileptr = nmalloc(sizeof(filestruct));
    fileptr->data = charalloc(strlen(buf) + 2);
    strcpy(fileptr->data, buf);

#ifndef NANO_SMALL
    /* If it's a DOS file (CRLF), and file conversion isn't disabled,
       strip out the CR part */
    if (!ISSET(NO_CONVERT) && buf[strlen(buf) - 1] == '\r') {
	fileptr->data[strlen(buf) - 1] = 0;
	totsize--;

	if (!fileformat)
	    fileformat = 1;
    }
#endif

    if (*line1ins) {
	/* Special case, insert with cursor on 1st line. */
	fileptr->prev = NULL;
	fileptr->next = fileage;
	fileptr->lineno = 1;
	*line1ins = 0;
	/* If we're inserting into the first line of the file, then
	   we want to make sure that our edit buffer stays on the
	   first line (and that fileage stays up to date!) */
	fileage = fileptr;
	edittop = fileptr;
    } else if (fileage == NULL) {
	fileage = fileptr;
	fileage->lineno = 1;
	fileage->next = fileage->prev = NULL;
	fileptr = filebot = fileage;
    } else if (prev) {
	fileptr->prev = prev;
	fileptr->next = NULL;
	fileptr->lineno = prev->lineno + 1;
	prev->next = fileptr;
    } else {
	die(_("read_line: not on first line and prev is NULL"));
    }

    return fileptr;
}

int read_file(int fd, char *filename, int quiet)
{
    long size;
    int num_lines = 0;
    char input;		/* current input character */
    char *buf;
    long i = 0, bufx = 128;
    filestruct *fileptr = current, *tmp = NULL;
    int line1ins = 0;

    buf = charalloc(bufx);
    buf[0] = '\0';

    if (fileptr != NULL && fileptr->prev != NULL) {
	fileptr = fileptr->prev;
	tmp = fileptr;
    } else if (fileptr != NULL && fileptr->prev == NULL) {
	tmp = fileage;
	current = fileage;
	line1ins = 1;
    }
    /* Read the entire file into file struct */
    while ((size = read_byte(fd, filename, &input)) > 0) {

	if (input == '\n') {
	    fileptr = read_line(buf, fileptr, &line1ins);
	    num_lines++;
	    buf[0] = 0;
	    i = 0;
#ifndef NANO_SMALL
	 } else if (!ISSET(NO_CONVERT) && input >= 0 && input <= 31 
			&& input != '\t' && input != '\r'
			&& input != '\n') 
	    /* If the file has binary chars in it, don't stupidly
		assume it's a DOS or Mac formatted file! */
	    SET(NO_CONVERT);

	/* If it's a Mac file (no LF just a CR), and file conversion
	   isn't disabled, handle it! */
	else if (!ISSET(NO_CONVERT) && i > 0 && buf[i-1] == '\r') {
	    fileformat = 2;
	    fileptr = read_line(buf, fileptr, &line1ins);
	    num_lines++;
	    buf[0] = input;
	    buf[1] = 0;
	    i = 1;
#endif
	} else {
	    /* Now we allocate a bigger buffer 128 characters at a time.
	       If we allocate a lot of space for one line, we may indeed 
	       have to use a buffer this big later on, so we don't
	       decrease it at all.  We do free it at the end, though. */

	    if (i >= bufx - 1) {
		buf = nrealloc(buf, bufx + 128);
		bufx += 128;
	    }
	    buf[i] = input;
	    buf[i + 1] = 0;
	    i++;
	}
	totsize += size;
    }

    /* Did we not get a newline but still have stuff to do? */
    if (buf[0]) {
	fileptr = read_line(buf, fileptr, &line1ins);
	num_lines++;
	buf[0] = 0;
    }

    /* Did we even GET a file if we don't already have one? */
    if (totsize == 0 || fileptr == NULL) {
	new_file();
	statusbar(_("Read %d lines"), num_lines);
	return 1;
    }

    /* Did we try to insert a file of 0 bytes? */
    if (num_lines == 0)
    {
	statusbar(_("Read %d lines"), 0);
	return 1;
    }

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

	/* Update the edit buffer */
	load_file(quiet);
    }

#ifndef NANO_SMALL
    if (fileformat == 2)
	statusbar(_("Read %d lines (Converted from Mac format)"), num_lines);
    else if (fileformat == 1)
	statusbar(_("Read %d lines (Converted from DOS format)"), num_lines);
    else
#endif
	statusbar(_("Read %d lines"), num_lines);

    totlines += num_lines;

    free(buf);
    close(fd);

    return 1;
}

#ifndef NANO_SMALL
int open_pipe(char *command)
{
    int fd[2], pid;
    int fork_status;
  
  /* Make our pipes. */

    if (pipe(fd) == -1) {
	statusbar("Could not pipe");
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

    read_file(fd[0],"stdin",0);
    set_modified();

    wait(&fork_status);

    return 0;
}
#endif /* NANO_SMALL */

/* Open the file (and decide if it exists) */
int open_file(char *filename, int insert, int quiet)
{
    int fd;
    struct stat fileinfo;

    if (!strcmp(filename, "") || stat(filename, &fileinfo) == -1) {
	if (insert && !quiet) {
	    statusbar(_("\"%s\" not found"), filename);
	    return -1;
	} else {
	    /* We have a new file */
	    statusbar(_("New File"));
	    new_file();
	}
    } else if ((fd = open(filename, O_RDONLY)) == -1) {
	if (!quiet)
	    statusbar("%s: %s", strerror(errno), filename);
	if (!insert)
	    new_file();
	return -1;
    } else {			/* File is A-OK */
	if (S_ISDIR(fileinfo.st_mode) || S_ISCHR(fileinfo.st_mode) || 
		S_ISBLK(fileinfo.st_mode)) {
	    if (S_ISDIR(fileinfo.st_mode))
		statusbar(_("File \"%s\" is a directory"), filename);
	    else
		/* Don't open character or block files.  Sorry, /dev/sndstat! */
		statusbar(_("File \"%s\" is a device file"), filename);


	    if (!insert)
		new_file();
	    return -1;
	}
	if (!quiet)
	    statusbar(_("Reading File"));
	read_file(fd, filename, quiet);
    }

    return 1;
}


/* This function will return the name of the first available extension
   of a filename (starting with the filename, then filename.1, etc).
   Memory is allocated for the return value.  If no writable extension 
   exists we return "" */
char *get_next_filename(char *name)
{
    int i = 0;
    char *buf = NULL;
    struct stat fs;

    buf = charalloc(strlen(name) + num_of_digits(INT_MAX) + 2);
    strcpy(buf, name);

    while(1) {

	if (stat(buf, &fs) == -1)
	    break;
	if (i == INT_MAX)
	    break;

	i++;
	strcpy(buf, name);
	sprintf(&buf[strlen(name)], ".%d", i);
    }

    if (i == INT_MAX)
	buf[0] = '\0';

    return buf;
}

int do_insertfile(int loading_file)
{
    int i;
    char *realname = NULL;

    wrap_reset();

#if !defined(DISABLE_BROWSER) || !defined(DISABLE_MOUSE)
    currshortcut = insertfile_list;
#endif

#ifndef DISABLE_OPERATINGDIR
    if ((operating_dir) && (strcmp(operating_dir,"."))){
	i = statusq(1, insertfile_list, "", _("File to insert [from %s] "),
		operating_dir);
    } else {
#endif
    i = statusq(1, insertfile_list, "", _("File to insert [from ./] "));
#ifndef DISABLE_OPERATINGDIR
    }
#endif

    if (i != -1) {

#ifdef DEBUG
	fprintf(stderr, _("filename is %s"), answer);
#endif

#ifndef DISABLE_TABCOMP
	realname = real_dir_from_tilde(answer);
#else
	realname = mallocstrcpy(realname, answer);
#endif

#ifndef DISABLE_BROWSER
	if (i == NANO_TOFILES_KEY) {
	    
	    char *tmp = do_browse_from(realname);
#if !defined(DISABLE_HELP) || !defined(DISABLE_MOUSE)
	    currshortcut = insertfile_list;
#endif

#ifdef DISABLE_TABCOMP
	    realname = NULL;
#endif
	    if 	(tmp != NULL)
		realname = mallocstrcpy(realname, tmp);
	    else
		return do_insertfile(loading_file);
	}
#endif

#ifndef DISABLE_OPERATINGDIR
	if (operating_dir) {
	    if (check_operating_dir(realname, 0)) {
		statusbar(_("Can't insert file from outside of %s"), operating_dir);
		return 0;
	    }
	}
#endif

#ifdef ENABLE_MULTIBUFFER
	if (loading_file) {

	    /* update the current entry in the open_files structure */
	    add_open_file(1);

	    new_file();
	    UNSET(MODIFIED);
	}
#endif

#ifndef NANO_SMALL
	if (i == NANO_EXTCMD_KEY) {
	    i = statusq(1, extcmd_list, "", _("Command to execute "));
	    if (i == -1) {
		statusbar(_("Cancelled"));
		UNSET(KEEP_CUTBUFFER);
		display_main_list();
		return 0;
	    }
	    if (answer != NULL) {
		i = open_pipe(answer);
	    }
	}
	else 
#endif /* NANO_SMALL */
	{
	    i = open_file(realname, 1, loading_file);
	}

#ifdef ENABLE_MULTIBUFFER
	if (loading_file)
	    filename = mallocstrcpy(filename, realname);
#endif

	free(realname);

	dump_buffer(fileage);

#ifdef ENABLE_MULTIBUFFER
	if (loading_file)
	    load_file(0);
	else
#endif

	    set_modified();

	/* Here we want to rebuild the edit window */
	fix_editbot();

#ifdef ENABLE_MULTIBUFFER
	/* If we've loaded another file, update the titlebar's contents */
	if (loading_file) {
	    clearok(topwin, FALSE);
	    titlebar(NULL);

	    /* And re-init the shortcut list */
	    shortcut_init(0);
	}
#endif

	/* If we've gone off the bottom, recenter; otherwise, just redraw */
	if (current->lineno > editbot->lineno)
	    edit_update(current, CENTER);
	else
	    edit_refresh();

	UNSET(KEEP_CUTBUFFER);
	display_main_list();
	return i;
    } else {
	statusbar(_("Cancelled"));
	UNSET(KEEP_CUTBUFFER);
	display_main_list();
	return 0;
    }
}

int do_insertfile_void(void)
{
    int result = 0;
#ifdef ENABLE_MULTIBUFFER
    if (ISSET(VIEW_MODE)) {
	if (ISSET(MULTIBUFFER))
	    result = do_insertfile(1);
	else
	    statusbar(_("Key illegal in non-multibuffer mode"));
    }
    else
	result = do_insertfile(ISSET(MULTIBUFFER));
#else
    result = do_insertfile(0);
#endif

    display_main_list();
    return result;
}

#ifdef ENABLE_MULTIBUFFER
/*
 * Add/update an entry to the open_files filestruct.  If update is
 * zero, a new entry is created; otherwise, the current entry is updated.
 * Return 0 on success or 1 on error.
 */
int add_open_file(int update)
{
    filestruct *tmp;

    if (!fileage || !current || !filename)
	return 1;

    /* if no entries, make the first one */
    if (!open_files) {
	open_files = make_new_node(NULL);

	/* if open_files->file is NULL at the nrealloc() below, we get a
	   segfault
	open_files->file = open_files; */
	open_files->file = NULL;
    }

    else if (!update) {

	/* otherwise, if we're not updating, make a new entry for
	   open_files and splice it in after the current one */

#ifdef DEBUG
	    fprintf(stderr, _("filename is %s"), open_files->data);
#endif

	tmp = make_new_node(NULL);
	splice_node(open_files, tmp, open_files->next);
	open_files = open_files->next;

	/* if open_files->file is NULL at the nrealloc() below, we get a
	   segfault
	open_files->file = open_files; */
	open_files->file = NULL;
    }

    /* save current filename */
    open_files->data = mallocstrcpy(open_files->data, filename);

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
    open_files->lineno = current->lineno;

    /* if we're in view mode and updating, the file contents won't
       have changed, so we won't bother resaving the filestruct
       then; otherwise, we will */
    if (!(ISSET(VIEW_MODE) && !update)) {
	/* save current filestruct and restore full file position
	   afterward */
	open_files->file = fileage; 
	do_gotopos(open_files->lineno, open_files->file_current_x, open_files->file_current_y, open_files->file_placewewant);
    }

    /* save current modification status */
    open_files->file_modified = ISSET(MODIFIED);

#ifdef DEBUG
    fprintf(stderr, _("filename is %s"), open_files->data);
#endif

    return 0;
}

/*
 * Update only the filename and full path stored in the current entry.
 * Return 0 on success or 1 on error.
 */ 
int open_file_change_name(void)
{
    if (!open_files || !filename)
	return 1;

    /* save current filename */
    open_files->data = mallocstrcpy(open_files->data, filename);

    return 0;
}

/*
 * Read the current entry in the open_files structure and set up the
 * currently open file using that entry's information.  Return 0 on
 * success or 1 on error.
 */
int load_open_file(void)
{
    if (!open_files)
	return 1;

    /* set up the filename, the file buffer, the total number of lines in
       the file, and the total file size */
    filename = mallocstrcpy(filename, open_files->data);
    fileage = open_files->file;
    current = fileage;
    totlines = open_files->file_totlines;
    totsize = open_files->file_totsize;

    /* Unset the marker because nano can't (yet) handle marked text flipping between
	open files */
    UNSET(MARK_ISSET);

    /* restore full file position: line number, x-coordinate, y-
       coordinate, place we want */
    do_gotopos(open_files->lineno, open_files->file_current_x, open_files->file_current_y, open_files->file_placewewant);

    /* restore the bottom of the file */
    filebot = current;
    while (filebot->next)
	filebot = filebot->next;

    /* set up modification status and update the titlebar */
    if (open_files->file_modified)
	SET(MODIFIED);
    else
	UNSET(MODIFIED);
    clearok(topwin, FALSE);
    titlebar(NULL);

    /* if we're constantly displaying the cursor position, update it (and do so
       unconditionally, in the rare case that the character count is the same
       but the line count isn't) */
    if (ISSET(CONSTUPDATE))
	do_cursorpos(0);

    /* now we're done */
    return 0;
}

/*
 * Open the previous entry in the open_files structure.  If closing_file
 * is zero, update the current entry before switching from it.
 * Otherwise, we are about to close that entry, so don't bother doing so.
 * Return 0 on success and 1 on error.
 */
int open_prevfile(int closing_file)
{
    if (!open_files)
	return 1;

    /* if we're not about to close the current entry, update it before
       doing anything */
    if (!closing_file)
	add_open_file(1);

    if (!open_files->prev && !open_files->next) {

	/* only one file open */
	if (!closing_file)
	    statusbar(_("No more open files"));
	return 1;
    }

    if (open_files->prev) {
	open_files = open_files->prev;

#ifdef DEBUG
	fprintf(stderr, _("filename is %s"), open_files->data);
#endif

    }

    else if (open_files->next) {

	/* if we're at the beginning, wrap around to the end */
	while (open_files->next)
	    open_files = open_files->next;

#ifdef DEBUG
	    fprintf(stderr, _("filename is %s"), open_files->data);
#endif

    }

/*    free_filestruct(fileage);  // delete this before reloading */
    load_open_file();

    statusbar(_("Switched to %s"),
      ((open_files->data[0] == '\0') ? "New Buffer" : open_files->data ));

#ifdef DEBUG
    dump_buffer(current);
#endif

    return 0;
}

/* This function is used by the shortcut list. */
int open_prevfile_void(void)
{
    open_prevfile(0);
    return 0;
}

/*
 * Open the next entry in the open_files structure.  If closing_file is
 * zero, update the current entry before switching from it.  Otherwise, we
 * are about to close that entry, so don't bother doing so.  Return 0 on
 * success and 1 on error.
 */
int open_nextfile(int closing_file)
{
    if (!open_files)
	return 1;

    /* if we're not about to close the current entry, update it before
       doing anything */
    if (!closing_file)
	add_open_file(1);

    if (!open_files->prev && !open_files->next) {

	/* only one file open */
	if (!closing_file)
	    statusbar(_("No more open files"));
	return 1;
    }

    if (open_files->next) {
	open_files = open_files->next;

#ifdef DEBUG
	fprintf(stderr, _("filename is %s"), open_files->data);
#endif

    }
    else if (open_files->prev) {

	/* if we're at the end, wrap around to the beginning */
	while (open_files->prev) {
	    open_files = open_files->prev;

#ifdef DEBUG
	    fprintf(stderr, _("filename is %s"), open_files->data);
#endif

	}
    }

    load_open_file();

    statusbar(_("Switched to %s"),
      ((open_files->data[0] == '\0') ? "New Buffer" : open_files->data ));

#ifdef DEBUG
    dump_buffer(current);
#endif

    return 0;
}

/* This function is used by the shortcut list. */
int open_nextfile_void(void)
{
    open_nextfile(0);
    return 0;
}

/*
 * Delete an entry from the open_files filestruct.  After deletion of an
 * entry, the next or previous entry is opened, whichever is found first.
 * Return 0 on success or 1 on error.
 */
int close_open_file(void)
{
    filestruct *tmp;

    if (!open_files)
	return 1;

    tmp = open_files;
    if (open_nextfile(1)) {
	if (open_prevfile(1))
	    return 1;
    }

    unlink_node(tmp);
    free_filestruct(tmp->file);
    delete_node(tmp);

    shortcut_init(0);
    display_main_list();
    return 0;
}
#endif /* MULTIBUFFER */

#if !defined (DISABLE_SPELLER) || !defined (DISABLE_OPERATINGDIR)
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

    /* first, get the current directory, and tack a slash onto the end of
       it, unless it turns out to be "/", in which case leave it alone */

#ifdef PATH_MAX
    d_here = getcwd(NULL, PATH_MAX + 1);
#else
    d_here = getcwd(NULL, 0);
#endif

    if (d_here) {

	align(&d_here);
	if (strcmp(d_here, "/")) {
	    d_here = nrealloc(d_here, strlen(d_here) + 2);
	    strcat(d_here, "/");
	}

	/* stat origpath; if stat() fails, assume that origpath refers to
	   a new file that hasn't been saved to disk yet (i. e. set
	   path_only to 0); if stat() succeeds, set path_only to 0 if
	   origpath doesn't refer to a directory, or to 1 if it does */
	path_only = stat(origpath, &fileinfo);
	if (path_only == -1)
		path_only = 0;
	else {
	    if (S_ISDIR(fileinfo.st_mode))
		path_only = 1;
	    else
		path_only = 0;
	}

	/* save the value of origpath in both d_there and d_there_file */
	d_there = charalloc(strlen(origpath) + 1);
	d_there_file = charalloc(strlen(origpath) + 1);
	strcpy(d_there, origpath);
	strcpy(d_there_file, origpath);

	/* if we have a path but no filename, tack slashes onto the ends
	   of both d_there and d_there_file, if they don't end in slashes
	   already */
	if (path_only) {
	    tmp = d_there[strlen(d_there) - 1];
	    if (tmp != '/') {
		d_there = nrealloc(d_there, strlen(d_there) + 2);
		strcat(d_there, "/");
		d_there_file = nrealloc(d_there_file, strlen(d_there_file) + 2);
		strcat(d_there_file, "/");
	    }
	}

	/* search for the last slash in d_there */
	last_slash = strrchr(d_there, '/');

	/* if we didn't find one, copy d_here into d_there; all data is
	   then set up */
	if (!last_slash) {
	    d_there = nrealloc(d_there, strlen(d_here) + 1);
	    strcpy(d_there, d_here);
	}

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
		if (d_there) {

		    /* add a slash to d_there, unless it's "/", in which
		       case we don't need it */
		    if (strcmp(d_there, "/")) {
			d_there = nrealloc(d_there, strlen(d_there) + 2);
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
char *check_writable_directory(const char *path) {

    char *full_path = get_full_path(path);
    int writable;
    struct stat fileinfo;

    /* if get_full_path() failed, return NULL */
    if (!full_path)
	return NULL;

    /* otherwise, stat() the full path to see if it's writable by the
       user; set writable to 1 if it is, or 0 if it isn't */
    stat(full_path, &fileinfo);
    if (fileinfo.st_mode & S_IWUSR)
	writable = 1;
    else
	writable = 0;

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
char *safe_tempnam(const char *dirname, const char *filename_prefix) {

    char *buf, *tempdir = NULL, *full_tempdir = NULL;
    int filedesc;

    /* if $TMPDIR is set and non-empty, set tempdir to it, run it through
       get_full_path(), and save the result in full_tempdir; otherwise,
       leave full_tempdir set to to NULL */
    if (getenv("TMPDIR") && strcmp(getenv("TMPDIR"),"")) {

	/* store the value of $TMPDIR in tempdir, run its value through
	   get_full_path(), and save the result in full_tempdir */
	tempdir = charalloc(strlen(getenv("TMPDIR")) + 1);
	sprintf(tempdir, "%s", getenv("TMPDIR"));
	full_tempdir = check_writable_directory(tempdir);

	/* we don't need the value of tempdir anymore */
	free(tempdir);
    }

    if (!full_tempdir) {

	/* if $TMPDIR is blank or isn't set, or isn't a writable
	   directory, and dirname isn't NULL, try it; otherwise, leave
	   full_tempdir set to NULL */
	if (dirname) {
	    tempdir = charalloc(strlen(dirname) + 1);
	    strcpy(tempdir, dirname);
	    full_tempdir = check_writable_directory(tempdir);

	    /* we don't need the value of tempdir anymore */
	    free(tempdir);
	}
    }

    /* if $TMPDIR is blank or isn't set, or if it isn't a writable
       directory, and dirname is NULL, try P_tmpdir instead */
    if (!full_tempdir) {
	tempdir = charalloc(strlen(P_tmpdir) + 1);
	strcpy(tempdir, P_tmpdir);
	full_tempdir = check_writable_directory(tempdir);

	/* we don't need the value of tempdir anymore */
	free(tempdir);
    }

    /* if P_tmpdir didn't work, use /tmp instead */
    if (!full_tempdir) {
	full_tempdir = charalloc(6);
	strcpy(full_tempdir, "/tmp/");
    }

    buf = charalloc(strlen(full_tempdir) + 12);
    sprintf(buf, "%s", full_tempdir);

    /* like tempnam(), use only the first 5 characters of the prefix */
    strncat(buf, filename_prefix, 5);

    strcat(buf, "XXXXXX");
    filedesc = mkstemp(buf);

    /* if mkstemp() failed, get out */
    if (filedesc == -1)
	return NULL;

    /* otherwise, close the resulting file; afterwards, it'll be 0 bytes
       long, so delete it; finally, return the filename (all that's left
       of it) */
    else {
	close(filedesc);
	unlink(buf);
	return buf;
    }
}
#endif /* !DISABLE_SPELLER */

#ifndef DISABLE_OPERATINGDIR
/*
 * Check to see if we're inside the operating directory.  Return 0 if we
 * are, or 1 otherwise.  If allow_tabcomp is nonzero, allow incomplete
 * names that would be matches for the operating directory, so that tab
 * completion will work.
 */
int check_operating_dir(char *currpath, int allow_tabcomp)
{
    /* The char *full_operating_dir is global for mem cleanup, and
       therefore we only need to get it the first time this function
       is called; also, a relative operating directory path will
       only be handled properly if this is done */

    char *fullpath, *whereami1, *whereami2 = NULL;

    /* if no operating directory is set, don't bother doing anything */
    if (!operating_dir)
	return 0;

    /* if the operating directory is "/", that's the same as having no
       operating directory, so discard it and get out */
    if (!strcmp(operating_dir, "/")) {
	operating_dir = NULL;
	return 0;
    }

    /* get the full operating (if we don't have it already) and current
       directories, and then search the current for the operating (for
       normal usage) and the operating for the current (for tab
       completion, if we're allowing it); if the current directory's path
       doesn't exist, assume we're outside the operating directory */
    if (!full_operating_dir) {
	full_operating_dir = get_full_path(operating_dir);

	/* if get_full_path() failed, discard the operating directory */
	if (!full_operating_dir) {
	    operating_dir = NULL;
	    return 0;
	}

	/* if the full operating directory is "/", that's the same as
	   having no operating directory, so discard it and get out */
	if (!strcmp(full_operating_dir, "/")) {
	    free(full_operating_dir);
	    operating_dir = NULL;
	    return 0;
	}
    }

    fullpath = get_full_path(currpath);
    if (!fullpath)
	return 1;

    whereami1 = strstr(fullpath, full_operating_dir);
    if (allow_tabcomp)
	whereami2 = strstr(full_operating_dir, fullpath);

    /* if both searches failed, we're outside the operating directory */
    if (!whereami1 && !whereami2)
	return 1;

    /* check the search results; if the full operating directory path is
       not at the beginning of the full current path (for normal usage)
       and vice versa (for tab completion, if we're allowing it), we're
       outside the operating directory */
    if (whereami1 != fullpath && whereami2 != full_operating_dir)
	return 1;

    /* otherwise, we're still inside it */
    return 0;
}
#endif

/*
 * Write a file out.  If tmp is nonzero, we set the umask to 0600,
 * we don't set the global variable filename to its name, and don't
 * print out how many lines we wrote on the statusbar.
 * 
 * tmp means we are writing a tmp file in a secure fashion.  We use
 * it when spell checking or dumping the file on an error.
 *
 * append means, not surprisingly, whether we are appending instead
 * of overwriting.
 *
 * nonamechange means don't change the current filename, it is ignored
 * if tmp == 1.
 */
int write_file(char *name, int tmp, int append, int nonamechange)
{
    long size, lineswritten = 0;
    char *buf = NULL;
    filestruct *fileptr;
    int fd, mask = 0, realexists, anyexists;
    struct stat st, lst;
    char *realname = NULL;

    if (!strcmp(name, "")) {
	statusbar(_("Cancelled"));
	return -1;
    }
    titlebar(NULL);
    fileptr = fileage;

    if (realname != NULL)
	free(realname);

    if (buf != NULL)
	free(buf);

#ifndef DISABLE_TABCOMP
    realname = real_dir_from_tilde(name);
#else
    realname = mallocstrcpy(realname, name);
#endif

#ifndef DISABLE_OPERATINGDIR
    if (!tmp && operating_dir) {
	/* if we're writing a temporary file, we're going outside the
	   operating directory, so skip the operating directory test */
	if (check_operating_dir(realname, 0)) {
	    statusbar(_("Can't write outside of %s"), operating_dir);

	    return -1;
	}
    }
#endif

    /* Save the state of file at the end of the symlink (if there is one) */
    realexists = stat(realname, &st);

    /* Stat the link itself for the check... */
    anyexists = lstat(realname, &lst);

    /* New case: if the file exists, just give up */
    if (tmp && anyexists != -1)
	return -1;
    /* NOTE: If you change this statement, you MUST CHANGE the if 
       statement below (that says:
		if (realexists == -1 || tmp || (!ISSET(FOLLOW_SYMLINKS) &&
		S_ISLNK(lst.st_mode))) {
       to reflect whether or not to link/unlink/rename the file */
    else if (ISSET(FOLLOW_SYMLINKS) || !S_ISLNK(lst.st_mode) || tmp) {
	/* Use O_EXCL if tmp == 1.  This is now copied from joe, because
	   wiggy says so *shrug*. */
	if (append)
	    fd = open(realname, O_WRONLY | O_APPEND, (S_IRUSR|S_IWUSR));
	else if (tmp)
	    fd = open(realname, O_WRONLY | O_CREAT | O_EXCL, (S_IRUSR|S_IWUSR));
	else
	    fd = open(realname, O_WRONLY | O_CREAT | O_TRUNC, (S_IRUSR|S_IWUSR));

	/* First, just give up if we couldn't even open the file */
	if (fd == -1) {
	    if (!tmp && ISSET(TEMP_OPT)) {
		UNSET(TEMP_OPT);
		return do_writeout(filename, 1, 0);
	    }
	    statusbar(_("Could not open file for writing: %s"),
		      strerror(errno));
	    return -1;
	}

    }
    /* Don't follow symlink.  Create new file. */
    else {
	buf = charalloc(strlen(realname) + 8);
	strncpy(buf, realname, strlen(realname)+1);
	strcat(buf, ".XXXXXX");
	if ((fd = mkstemp(buf)) == -1) {
	    if (ISSET(TEMP_OPT)) {
		UNSET(TEMP_OPT);
		return do_writeout(filename, 1, 0);
	    }
	    statusbar(_("Could not open file for writing: %s"),
		      strerror(errno));
	    return -1;
	}
    }

    dump_buffer(fileage);
    while (fileptr != NULL && fileptr->next != NULL) {
	/* Next line is so we discount the "magic line" */
	if (filebot == fileptr && fileptr->data[0] == '\0')
	    break;

	size = write(fd, fileptr->data, strlen(fileptr->data));
	if (size == -1) {
	    statusbar(_("Could not open file for writing: %s"),
		      strerror(errno));
	    return -1;
	} else {
#ifdef DEBUG
	    fprintf(stderr, _("Wrote >%s\n"), fileptr->data);
#endif
	}
#ifndef NANO_SMALL
	if (ISSET(DOS_FILE) || ISSET(MAC_FILE))
	    write(fd, "\r", 1);

	if (!ISSET(MAC_FILE))
#endif
	    write(fd, "\n", 1);

	fileptr = fileptr->next;
	lineswritten++;
    }

    if (fileptr != NULL) {
	size = write(fd, fileptr->data, strlen(fileptr->data));
	if (size == -1) {
	    statusbar(_("Could not open file for writing: %s"),
		      strerror(errno));
	    return -1;
	} else if (size > 0) {
#ifndef NANO_SMALL
	    if (ISSET(DOS_FILE) || ISSET(MAC_FILE)) {
		size = write(fd, "\r", 1);
		lineswritten++;
		if (size == -1) {
		    statusbar(_("Could not open file for writing: %s"),
			  strerror(errno));
		    return -1;
		}
	    }

	    if (!ISSET(MAC_FILE))
#endif
	    {
		size = write(fd, "\n", 1);
		lineswritten++;
		if (size == -1) {
		    statusbar(_("Could not open file for writing: %s"),
			  strerror(errno));
		    return -1;
		}
	    }
	}
    }


    if (close(fd) == -1) {
	statusbar(_("Could not close %s: %s"), realname, strerror(errno));
	unlink(buf);
	return -1;
    }

    if (realexists == -1 || tmp ||
	(!ISSET(FOLLOW_SYMLINKS) && S_ISLNK(lst.st_mode))) {

	/* Use default umask as file permissions if file is a new file. */
	mask = umask(0);
	umask(mask);

	if (tmp)		/* We don't want anyone reading our temporary file! */
	    mask = 0600;
	else
	    mask = 0666 & ~mask;
    } else
	/* Use permissions from file we are overwriting. */
	mask = st.st_mode;

    if (!tmp && (!ISSET(FOLLOW_SYMLINKS) && S_ISLNK(lst.st_mode))) {
	if (unlink(realname) == -1) {
	    if (errno != ENOENT) {
		statusbar(_("Could not open %s for writing: %s"),
			  realname, strerror(errno));
		unlink(buf);
		return -1;
	    }
	}
	if (link(buf, realname) != -1)
	    unlink(buf);
	else if (errno != EPERM) {
	    statusbar(_("Could not open %s for writing: %s"),
		      name, strerror(errno));
	    unlink(buf);
	    return -1;
	} else if (rename(buf, realname) == -1) {	/* Try a rename?? */
	    statusbar(_("Could not open %s for writing: %s"),
		      realname, strerror(errno));
	    unlink(buf);
	    return -1;
	}
    }
    if (chmod(realname, mask) == -1)
	statusbar(_("Could not set permissions %o on %s: %s"),
		  mask, realname, strerror(errno));

    if (!tmp) {
	if (!nonamechange)
	    filename = mallocstrcpy(filename, realname);

	statusbar(_("Wrote %d lines"), lineswritten);
	UNSET(MODIFIED);
	titlebar(NULL);
    }
    return 1;
}

int do_writeout(char *path, int exiting, int append)
{
    int i = 0;
    char *formatstr = NULL;

#ifdef NANO_EXTRA
    static int did_cred = 0;
#endif

#if !defined(DISABLE_BROWSER) || !defined(DISABLE_MOUSE)
    currshortcut = writefile_list;
#endif

    answer = mallocstrcpy(answer, path);

    if ((exiting) && (ISSET(TEMP_OPT))) {
	if (filename[0]) {
	    i = write_file(answer, 0, 0, 0);
	    display_main_list();
	    return i;
	} else {
	    UNSET(TEMP_OPT);
	    do_exit();

	    /* They cancelled, abort quit */
	    return -1;
	}
    }

    while (1) {

#ifndef NANO_SMALL

	if (ISSET(MAC_FILE))
	   formatstr = _(" [Mac Format]");
	else if (ISSET(DOS_FILE))
	   formatstr = _(" [DOS Format]");
	else
	   formatstr = "";

	/* Be nice to the translation folks */
	if (ISSET(MARK_ISSET) && !exiting) {
	    if (append)
		i = statusq(1, writefile_list, "",
		    "%s%s", _("Append Selection to File"), formatstr);
	    else
		i = statusq(1, writefile_list, "",
		    "%s%s", _("Write Selection to File"), formatstr);
	} else
#endif
	{
	    if (append)
		i = statusq(1, writefile_list, answer,
		    "%s%s", _("File Name to Append"), formatstr);
	    else
		i = statusq(1, writefile_list, answer,
		    "%s%s", _("File Name to Write"), formatstr);

	}

	if (i != -1) {

#ifndef DISABLE_BROWSER
	if (i == NANO_TOFILES_KEY) {

	    char *tmp = do_browse_from(answer);

#if !defined(DISABLE_BROWSER) || !defined(DISABLE_MOUSE)
	    currshortcut = writefile_list;
#endif

	    if (tmp != NULL) {
		answer = mallocstrcpy(answer, tmp);
	    } else
		return do_writeout(answer, exiting, append);
	} else
#endif
	if (i == TOGGLE_DOS_KEY) {
	    UNSET(MAC_FILE);
	    TOGGLE(DOS_FILE);
	    return(do_writeout(answer, exiting, append));
	} else if (i == TOGGLE_MAC_KEY) {
	    UNSET(DOS_FILE);
	    TOGGLE(MAC_FILE);
	    return(do_writeout(answer, exiting, append));
	} else if (i == NANO_APPEND_KEY)
	    return(do_writeout(answer, exiting, 1 - append));

#ifdef DEBUG
	    fprintf(stderr, _("filename is %s"), answer);
#endif

#ifdef NANO_EXTRA
	    if (exiting && !ISSET(TEMP_OPT) && !strcasecmp(answer, "zzy")
		&& !did_cred) {
		do_credits();
		did_cred = 1;
		return -1;
	    }
#endif
	    if (!append && strcmp(answer, filename)) {
		struct stat st;
		if (!stat(answer, &st)) {
		    i = do_yesno(0, 0, _("File exists, OVERWRITE ?"));

		    if (!i || (i == -1))
			continue;
		}
	    }
#ifndef NANO_SMALL

	/* Here's where we allow the selected text to be written to 
	   a separate file. */
	if (ISSET(MARK_ISSET) && !exiting) {
	    filestruct *fileagebak = fileage;	
	    filestruct *filebotbak = filebot;
	    filestruct *cutback = cutbuffer;
	    int oldmod = 0;
	    cutbuffer = NULL;

	    /* Okay, since write_file changes the filename, back it up */
	    if (ISSET(MODIFIED))
		oldmod = 1;

	    /* Now, non-destructively add the marked text to the
	       cutbuffer, and write the file out using the cutbuffer ;) */
	    if (current->lineno <= mark_beginbuf->lineno)
		cut_marked_segment(current, current_x, mark_beginbuf,
				mark_beginx, 0);
	    else
		cut_marked_segment(mark_beginbuf, mark_beginx, current,
				current_x, 0);

	    fileage = cutbuffer;
	    for (filebot = cutbuffer; filebot->next != NULL; 
			filebot = filebot->next)
		;
	    i = write_file(answer, 0, append, 1);

	    /* Now restore everything */
	    fileage = fileagebak;
	    filebot = filebotbak;
	    cutbuffer = cutback;
	    if (oldmod)
		set_modified();
	} else
#endif
	    i = write_file(answer, 0, append, 0);

#ifdef ENABLE_MULTIBUFFER
	    /* if we're not about to exit, update the current entry in
	       the open_files structure */
	    if (!exiting) {

		/* first, if the filename was changed during the save,
		   update the filename stored in the current entry, and
		   then update the current entry */
		if (strcmp(open_files->data, filename)) {
		    open_file_change_name();
		    add_open_file(1);
		}
		else {

		    /* otherwise, just update the current entry */
		    add_open_file(1);
		}
	    }
#endif

	    display_main_list();
	    return i;
	} else {
	    statusbar(_("Cancelled"));
	    display_main_list();
	    return 0;
	}
    }
}

int do_writeout_void(void)
{
    return do_writeout(filename, 0, 0);
}

#ifndef DISABLE_TABCOMP

/* Return a malloc()ed string containing the actual directory, used
 * to convert ~user and ~/ notation...
 */
char *real_dir_from_tilde(char *buf)
{
    char *dirtmp = NULL, *find_user = NULL;
    int i = 1;
    struct passwd *userdata;

    /* set a default value for dirtmp, in the case user home dir not found */
    dirtmp = mallocstrcpy(dirtmp, buf);

    if (buf[0] == '~') {
	if (buf[1] == 0 || buf[1] == '/') {
	    if (getenv("HOME") != NULL) {

		free(dirtmp);
		dirtmp = charalloc(strlen(buf) + 2 + strlen(getenv("HOME")));

		sprintf(dirtmp, "%s%s", getenv("HOME"), &buf[1]);

	    }
	}
	else {

	    /* Figure how how much of the str we need to compare */
	    for (i = 1; buf[i] != '/' && buf[i] != 0; i++)
		;

	    find_user = mallocstrcpy(find_user, &buf[1]);
	    find_user[i - 1] = 0;

	    for (userdata = getpwent(); userdata != NULL && 
		  strcmp(userdata->pw_name, find_user); 
		  userdata = getpwent());

	    free(find_user);

	    if (userdata != NULL) {  /* User found */

	        free(dirtmp);
		dirtmp = charalloc(strlen(buf) + 2 + strlen(userdata->pw_dir));
		sprintf(dirtmp, "%s%s", userdata->pw_dir, &buf[i]);

	    }

	    endpwent();
	}
    }

    return dirtmp;
}

/* Tack a slash onto the string we're completing if it's a directory */
int append_slash_if_dir(char *buf, int *lastWasTab, int *place)
{
    char *dirptr;
    struct stat fileinfo;
    int ret = 0;

    dirptr = real_dir_from_tilde(buf);

    if (stat(dirptr, &fileinfo) == -1)
	ret = 0;
    else if (S_ISDIR(fileinfo.st_mode)) {
	strncat(buf, "/", 1);
	*place += 1;
	/* now we start over again with # of tabs so far */
	*lastWasTab = 0;
	ret = 1;
    }

    if (dirptr != buf)
	free(dirptr);

    return ret;
}

/*
 * These functions (username_tab_completion, cwd_tab_completion, and
 * input_tab were taken from busybox 0.46 (cmdedit.c).  Here is the notice
 * from that file:
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
    char **matches = (char **) NULL;
    char *matchline = NULL;
    struct passwd *userdata;

    *num_matches = 0;
    matches = nmalloc(BUFSIZ * sizeof(char *));

    strcat(buf, "*");

    while ((userdata = getpwent()) != NULL) {

	if (check_wildcard_match(userdata->pw_name, &buf[1]) == TRUE) {

	    /* Cool, found a match.  Add it to the list
	     * This makes a lot more sense to me (Chris) this way...
	     */

#ifndef DISABLE_OPERATINGDIR
	    /* ...unless the match exists outside the operating
               directory, in which case just go to the next match */

	    if (operating_dir) {
		if (check_operating_dir(userdata->pw_dir, 1))
		    continue;
	    }
#endif

	    matchline = charalloc(strlen(userdata->pw_name) + 2);
	    sprintf(matchline, "~%s", userdata->pw_name);
	    matches[*num_matches] = matchline;
	    ++*num_matches;

	    /* If there's no more room, bail out */
	    if (*num_matches == BUFSIZ)
		break;
	}
    }
    endpwent();

    return (matches);
}

/* This was originally called exe_n_cwd_tab_completion, but we're not
   worried about executables, only filenames :> */

char **cwd_tab_completion(char *buf, int *num_matches)
{
    char *dirName, *dirtmp = NULL, *tmp = NULL, *tmp2 = NULL;
    char **matches = (char **) NULL;
    DIR *dir;
    struct dirent *next;

    matches = nmalloc(BUFSIZ * sizeof(char *));

    /* Stick a wildcard onto the buf, for later use */
    strcat(buf, "*");

    /* Okie, if there's a / in the buffer, strip out the directory part */
    if (strcmp(buf, "") && strstr(buf, "/")) {
	dirName = charalloc(strlen(buf) + 1);
	tmp = buf + strlen(buf);
	while (*tmp != '/' && tmp != buf)
	    tmp--;

	tmp++;

	strncpy(dirName, buf, tmp - buf + 1);
	dirName[tmp - buf] = 0;

    } else {

#ifdef PATH_MAX
	if ((dirName = getcwd(NULL, PATH_MAX+1)) == NULL)
#else
	/* The better, but apparently segfault-causing way */
	if ((dirName = getcwd(NULL, 0)) == NULL)
#endif /* PATH_MAX */
	    return matches;
	else
	    tmp = buf;
    }

#ifdef DEBUG
    fprintf(stderr, "\nDir = %s\n", dirName);
    fprintf(stderr, "\nbuf = %s\n", buf);
    fprintf(stderr, "\ntmp = %s\n", tmp);
#endif

    dirtmp = real_dir_from_tilde(dirName);
    free(dirName);
    dirName = dirtmp;

#ifdef DEBUG
    fprintf(stderr, "\nDir = %s\n", dirName);
    fprintf(stderr, "\nbuf = %s\n", buf);
    fprintf(stderr, "\ntmp = %s\n", tmp);
#endif


    dir = opendir(dirName);
    if (!dir) {
	/* Don't print an error, just shut up and return */
	*num_matches = 0;
	beep();
	return (matches);
    }
    while ((next = readdir(dir)) != NULL) {

#ifdef DEBUG
	fprintf(stderr, "Comparing \'%s\'\n", next->d_name);
#endif
	/* See if this matches */
	if (check_wildcard_match(next->d_name, tmp) == TRUE) {

	    /* Cool, found a match.  Add it to the list
	     * This makes a lot more sense to me (Chris) this way...
	     */

#ifndef DISABLE_OPERATINGDIR
	    /* ...unless the match exists outside the operating
               directory, in which case just go to the next match; to
	       properly do operating directory checking, we have to add the
	       directory name to the beginning of the proposed match
	       before we check it */

	    if (operating_dir) {
		tmp2 = charalloc(strlen(dirName) + strlen(next->d_name) + 2);
		strcpy(tmp2, dirName);
		strcat(tmp2, "/");
		strcat(tmp2, next->d_name);
		if (check_operating_dir(tmp2, 1)) {
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

    return (matches);
}

/* This function now has an arg which refers to how much the 
 * statusbar (place) should be advanced, i.e. the new cursor pos.
 */
char *input_tab(char *buf, int place, int *lastWasTab, int *newplace, int *list)
{
    /* Do TAB completion */
    static int num_matches = 0, match_matches = 0;
    static char **matches = (char **) NULL;
    int pos = place, i = 0, col = 0, editline = 0;
    int longestname = 0, is_dir = 0;
    char *foo;

    *list = 0;

    if (*lastWasTab == FALSE) {
	char *tmp, *copyto, *matchBuf;

	*lastWasTab = 1;

	/* Make a local copy of the string -- up to the position of the
	   cursor */
	matchBuf = (char *) nmalloc((strlen(buf) + 2) * sizeof(char));
	memset(matchBuf, '\0', (strlen(buf) + 2));

	strncpy(matchBuf, buf, place);
	tmp = matchBuf;

	/* skip any leading white space */
	while (*tmp && isspace((int) *tmp))
	    ++tmp;

	/* Free up any memory already allocated */
	if (matches != NULL) {
	    for (i = i; i < num_matches; i++)
		free(matches[i]);
	    free(matches);
	    matches = (char **) NULL;
	    num_matches = 0;
	}

	/* If the word starts with `~' and there is no slash in the word, 
	 * then try completing this word as a username. */

	/* FIXME -- this check is broken! */
	if (*tmp == '~' && !strchr(tmp, '/'))
	    matches = username_tab_completion(tmp, &num_matches);

	/* Try to match everything in the current working directory that
	 * matches.  */
	if (!matches)
	    matches = cwd_tab_completion(tmp, &num_matches);

	/* Don't leak memory */
	free(matchBuf);

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

	    buf = nrealloc(buf, strlen(buf) + strlen(matches[0]) + 1);

	    if (strcmp(buf, "") && strstr(buf, "/")) {
		for (tmp = buf + strlen(buf); *tmp != '/' && tmp != buf;
		     tmp--);
		tmp++;
	    } else
		tmp = buf;

	    if (!strcmp(tmp, matches[0]))
		is_dir = append_slash_if_dir(buf, lastWasTab, newplace);

	    if (is_dir)
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
	    append_slash_if_dir(buf, lastWasTab, newplace);

	    break;
	default:
	    /* Check to see if all matches share a beginning, and, if so,
	       tack it onto buf and then beep */

	    if (strcmp(buf, "") && strstr(buf, "/")) {
		for (tmp = buf + strlen(buf); *tmp != '/' && tmp != buf;
		     tmp--);
		tmp++;
	    } else
		tmp = buf;

	    for (pos = 0; *tmp == matches[0][pos] && *tmp != 0 &&
		 pos <= strlen(matches[0]); pos++)
		tmp++;

	    while (1) {
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
		    buf = nrealloc(buf, strlen(buf) + 2);
		    strncat(buf, matches[0] + pos, 1);
		    *newplace += 1;
		    pos++;
		} else {
		    beep();
		    break;
		}
	    }
	    break;
	}
    } else {
	/* Ok -- the last char was a TAB.  Since they
	 * just hit TAB again, print a list of all the
	 * available choices... */
	if (matches && num_matches > 1) {

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
		if (col > (COLS - longestname) && matches[i + 1] != NULL) {
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
	    *list = 1;
	} else
	    beep();

    }

    /* Only refresh the edit window if we don't have a list of filename
       matches on it */
    if (*list == 0)
	edit_refresh();
    curs_set(1);
    return buf;
}
#endif

#ifndef DISABLE_BROWSER

/* Return the stat of the file pointed to by path */
struct stat filestat(const char *path) {
    struct stat st;

    stat(path, &st);
    return st;
}

/* Our sort routine for file listings - sort directories before
 * files, and then alphabetically
 */ 
int diralphasort(const void *va, const void *vb) {
    struct stat file1info, file2info;
    char *a = *(char **)va, *b = *(char **)vb;
    int aisdir, bisdir;

    aisdir = (stat(a, &file1info) != -1) && S_ISDIR(file1info.st_mode);
    bisdir = (stat(b, &file2info) != -1) && S_ISDIR(file2info.st_mode);

    if (aisdir && !bisdir) return -1;
    if (!aisdir && bisdir) return 1;

#ifdef HAVE_STRCASECMP
    return(strcasecmp(a,b));
#else
    return(strcmp(a,b));
#endif

}

/* Initialize the browser code, including the list of files in *path */
char **browser_init(char *path, int *longest, int *numents)
{
    DIR *dir;
    struct dirent *next;
    char **filelist = (char **) NULL;
    int i = 0;

    dir = opendir(path);
    if (!dir) 
	return NULL;

    *numents = 0;
    while ((next = readdir(dir)) != NULL) {
	if (!strcmp(next->d_name, "."))
	   continue;
	(*numents)++;
	if (strlen(next->d_name) > *longest)
	    *longest = strlen(next->d_name);
    }
    rewinddir(dir);
    *longest += 10;

    filelist = nmalloc(*numents * sizeof (char *));

    while ((next = readdir(dir)) != NULL) {
	if (!strcmp(next->d_name, "."))
	   continue;
	filelist[i] = charalloc(strlen(next->d_name) + strlen(path) + 2);

	if (!strcmp(path, "/"))
	    snprintf(filelist[i], strlen(next->d_name) + strlen(path) + 1, 
			"%s%s", path, next->d_name);
	else
	    snprintf(filelist[i], strlen(next->d_name) + strlen(path) + 2, 
			"%s/%s", path, next->d_name);
	i++;
    }

    if (*longest > COLS - 1)
	*longest = COLS - 1;

    return filelist;
}

/* Free our malloc()ed memory */
void free_charptrarray(char **array, int len)
{
    int i;

    for (i = 0; i < len; i++)
	free(array[i]);
    free(array);
}

/* only print the last part of a path; isn't there a shell 
   command for this? */
char *tail(char *foo)
{
    char *tmp = NULL;

    tmp = foo + strlen(foo);
    while (*tmp != '/' && tmp != foo)
	tmp--;

    tmp++;

    return tmp;
}

/* Strip one dir from the end of a string */
void striponedir(char *foo)
{
    char *tmp = NULL;

    /* Don't strip the root dir */
    if (!strcmp(foo, "/"))
	return;

    tmp = foo + strlen(foo);
    if (*tmp == '/')
	tmp--;

    while (*tmp != '/' && tmp != foo)
	tmp--;

    if (tmp != foo)
	*tmp = 0;
    else
    { /* SPK may need to make a 'default' path here */
        if (*tmp != '/') *(tmp) = '.';
	*(tmp+1) = 0;
    }

    return;
}

/* Our browser function.  inpath is the path to start browsing from */
char *do_browser(char *inpath)
{
    struct stat st;
    char *foo, *retval = NULL;
    static char *path = NULL;
    int numents = 0, i = 0, j = 0, kbinput = 0, longest = 0, abort = 0;
    int col = 0, selected = 0, editline = 0, width = 0, filecols = 0;
    int lineno = 0, kb;
    char **filelist = (char **) NULL;
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
    MEVENT mevent;
#endif
#endif

    /* If path isn't the same as inpath, we are being passed a new
	dir as an arg.  We free it here so it will be copied from 
	inpath below */
    if (path != NULL && strcmp(path, inpath)) {
	free(path);
	path = NULL;
    }

    /* if path doesn't exist, make it so */
    if (path == NULL)
	path = mallocstrcpy(path, inpath);

    filelist = browser_init(path, &longest, &numents);
    foo = charalloc(longest + 8);

    /* Sort the list by directory first, then alphabetically */
    qsort(filelist, numents, sizeof(char *), diralphasort);

    kb = keypad_on(edit, 1);
    titlebar(path);
    bottombars(browser_list);
    curs_set(0);
    wmove(edit, 0, 0);
    i = 0;
    width = 0;
    filecols = 0;

    /* Loop invariant: Microsoft sucks. */
    do {
	DIR *test_dir;

	blank_statusbar_refresh();

#if !defined DISABLE_HELP || !defined(DISABLE_MOUSE)
	currshortcut = browser_list;
#endif

 	editline = 0;
	col = 0;
	    
	/* Compute line number we're on now, so we don't divide by zero later */
	if (width == 0)
	    lineno = selected;
	else
	    lineno = selected / width;

	switch (kbinput) {

#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
        case KEY_MOUSE:
	    if (getmouse(&mevent) == ERR)
	        return retval;
 
	    /* If they clicked in the edit window, they probably clicked
		on a file */
 	    if (wenclose(edit, mevent.y, mevent.x)) { 
		int selectedbackup = selected;

		mevent.y -= 2;

		/* If we're on line 0, don't toy with finding out what
			page we're on */
		if (lineno / editwinrows == 0)
		    selected = mevent.y * width + mevent.x / longest;
		else
		    selected = (lineno / editwinrows) * editwinrows * width 
			+ mevent.y * width + mevent.x / longest;

		/* If we're off the screen, reset to the last item.
		   If we clicked where we did last time, select this name! */
		if (selected > numents - 1)
		    selected = numents - 1;
		else if (selectedbackup == selected) {
		    ungetch('s');	/* Unget the 'select' key */
		    break;
		}
	    } else	/* Must be clicking a shortcut */
		do_mouse();

            break;
#endif
#endif
	case NANO_UP_KEY:
	case KEY_UP:
	case 'u':
	    if (selected - width >= 0)
		selected -= width;
	    break;
	case NANO_BACK_KEY:
	case KEY_LEFT:
	case NANO_BACKSPACE_KEY:
	case 127:
	case 'l':
	    if (selected > 0)
		selected--;
	    break;
	case KEY_DOWN:
	case NANO_DOWN_KEY:
	case 'd':
	    if (selected + width <= numents - 1)
		selected += width;
	    break;
	case KEY_RIGHT:
	case NANO_FORWARD_KEY:
	case 'r':
	    if (selected < numents - 1)
		selected++;
	    break;
	case NANO_PREVPAGE_KEY:
	case NANO_PREVPAGE_FKEY:
	case KEY_PPAGE:
	case '-':

	    if (lineno % editwinrows == 0) {
		if (selected - (editwinrows * width) >= 0)
		    selected -= editwinrows * width; 
		else
		    selected = 0;
	    }
	    else if (selected - (editwinrows + 
		lineno % editwinrows) * width  >= 0)

		selected -= (editwinrows + lineno % editwinrows) * width; 
	    else
		selected = 0;
	    break;
	case NANO_NEXTPAGE_KEY:
	case NANO_NEXTPAGE_FKEY:
	case KEY_NPAGE:	
	case ' ':
	    if (lineno % editwinrows == 0) {
		if (selected + (editwinrows * width) <= numents - 1)
		    selected += editwinrows * width; 
		else
		    selected = numents - 1;
	    }
	    else if (selected + (editwinrows - 
			lineno %  editwinrows) * width <= numents - 1)
 		selected += (editwinrows - lineno % editwinrows) * width; 
 	    else
		selected = numents - 1;
	    break;
	case NANO_HELP_KEY:
	case NANO_HELP_FKEY:
	     do_help();
	     break;
	case KEY_ENTER:
	case NANO_ENTER_KEY:
	case 's': /* More Pico compatibility */
	case 'S':

	    /* You can't cd up from / */
	    if (!strcmp(filelist[selected], "/..") && !strcmp(path, "/")) {
		statusbar(_("Can't move up a directory"));
		break;
	    }

	    path = mallocstrcpy(path, filelist[selected]);

#ifndef DISABLE_OPERATINGDIR
	    /* Note: The case of the user's being completely outside the
	       operating directory is handled elsewhere, before this
	       point */
	    if (operating_dir) {
		if (check_operating_dir(path, 0)) {
		    statusbar(_("Can't visit parent in restricted mode"));
		    beep();
		    break;
		}
	    }
#endif

	    /* SPK for '.' path, get the current path via getcwd */
	    if (!strcmp(path, "./..")) {
		free(path);
		path = getcwd(NULL, 0);
		striponedir(path);		    
		align(&path);
		free_charptrarray(filelist, numents);
		free(foo);
		return do_browser(path);
	    }

	    st = filestat(path);
	    if (S_ISDIR(st.st_mode)) {
		if ((test_dir = opendir(path)) == NULL) {
		    /* We can't open this dir for some reason.  Complain */
		    statusbar(_("Can't open \"%s\": %s"), path, strerror(errno));
		    striponedir(path);
		    align(&path);
		    break;
		} 
		closedir(test_dir);

		if (!strcmp("..", tail(path))) {
		    /* They want to go up a level, so strip off .. and the
			current dir */
		    striponedir(path);
		    striponedir(path);
		    align(&path);
		}

		/* Start over again with the new path value */
		free_charptrarray(filelist, numents);
		free(foo);
		return do_browser(path);
	    } else {
		retval = path;
		abort = 1;
	    }
	    break;
	/* Goto a specific directory */
	case 'g':	/* Pico compatibility */
	case 'G':
	case NANO_GOTO_KEY:

	    curs_set(1);
	    j = statusq(0, gotodir_list, "", _("Goto Directory"));
	    bottombars(browser_list);
	    curs_set(0);

#ifndef DISABLE_OPERATINGDIR
	    if (operating_dir) {
		if (check_operating_dir(answer, 0)) {
		    statusbar(_("Can't go outside of %s in restricted mode"), operating_dir);
		    break;
		}
	    }
#endif

	    if (j < 0) {
		statusbar(_("Goto Cancelled"));
		break;
	    }

	    if (answer[0] != '/') {
		char *saveanswer = NULL;

		saveanswer = mallocstrcpy(saveanswer, answer);
		answer = nrealloc(answer, strlen(path) + strlen(saveanswer) + 2);
		sprintf(answer, "%s/%s", path, saveanswer);
		free(saveanswer);
	    }

	    if ((test_dir = opendir(answer)) == NULL) {
		/* We can't open this dir for some reason.  Complain */
		statusbar(_("Can't open \"%s\": %s"), answer, strerror(errno));
		break;
	    } 
	    closedir(test_dir);

	    /* Start over again with the new path value */
	    path = mallocstrcpy(path, answer);
	    return do_browser(path);

	/* Stuff we want to abort the browser */
	case 'q':
	case 'Q':
	case 'e':	/* Pico compatibility, yeech */
	case 'E':
	case NANO_CANCEL_KEY:
	case NANO_EXIT_FKEY:
		abort = 1;
		break;
	}
	if (abort)
	    break;

	blank_edit();

	if (width)
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
		    st = filestat(filelist[j]);
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
		else /* Its more than 1 k and less than a meg */
		    sprintf(foo + longest - 7, "%4d KB", 
			(int) st.st_size >> 10);
	    }

	    /* Hilight the currently selected file/dir */
	    if (j == selected) {
#ifdef ENABLE_COLOR
		color_on(edit, COLOR_STATUSBAR);
#else
		wattron(edit, A_REVERSE);

#endif
	    }
	    waddnstr(edit, foo, strlen(foo));
	    if (j == selected) {
#ifdef ENABLE_COLOR
		color_off(edit, COLOR_STATUSBAR);
#else
		wattroff(edit, A_REVERSE);
#endif
	    }

	    /* And add some space between the cols */
	    waddstr(edit, "  ");
	    col += 2;

	    /* And if the next entry isn't going to fit on the
		line, move to the next one */
	    if (col > (COLS - longest)) {
		editline++;
		wmove(edit, editline, 0);
		col = 0;
		if (width == 0)
		    width = filecols;
	    }
	}
 	wrefresh(edit);
    } while ((kbinput = wgetch(edit)) != NANO_EXIT_KEY);
    curs_set(1);
    blank_edit();
    titlebar(NULL); 
    edit_refresh();
    kb = keypad_on(edit, kb);

    /* cleanup */
    free_charptrarray(filelist, numents);
    free(foo);
    return retval;
}

/* Browser front end, checks to see if inpath has a dir in it and, if so,
 starts do_browser from there, else from the current dir */
char *do_browse_from(char *inpath)
{
    struct stat st;
    char *tmp = NULL;

    tmp = mallocstrcpy(tmp, inpath);


    /* If there's no / in the string, we may as well start from . */
    if (tmp == NULL || *tmp == '\0' || !strstr(tmp, "/")) {
#ifdef PATH_MAX
	char *from = getcwd(NULL, PATH_MAX+1);
#else
	char *from = getcwd(NULL, 0);
#endif /* PATH_MAX */
	return do_browser(from ? from : "./");
    }

    /* If the string is a directory, pass do_browser that */
    st = filestat(tmp);
    if (S_ISDIR(st.st_mode))
	return do_browser(tmp);

    /* Okay, there's a dir in there, but not at the end of the string... 
       try stripping it off */
    striponedir(tmp);
    align(&tmp);
    return do_browser(tmp);

}
#endif
