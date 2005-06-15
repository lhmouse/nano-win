/* $Id$ */
/**************************************************************************
 *   files.c                                                              *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <utime.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <assert.h>
#include "proto.h"

static file_format fmt = NIX_FILE;
	/* The format of the current file. */

/* What happens when there is no file to open? aiee! */
void new_file(void)
{
    fileage = make_new_node(NULL);
    fileage->data = mallocstrcpy(NULL, "");
    filebot = fileage;
    edittop = fileage;
    current = fileage;
    current_x = 0;
    totlines = 1;
    totsize = 0;

#ifdef ENABLE_COLOR
    update_color();
    if (!ISSET(NO_COLOR_SYNTAX))
	edit_refresh();
#endif
}

/* We make a new line of text from buf.  buf is length len.  If
 * first_line_ins is TRUE, then we put the new line at the top of the
 * file.  Otherwise, we assume prevnode is the last line of the file,
 * and put our line after prevnode. */
filestruct *read_line(char *buf, filestruct *prevnode, bool
	*first_line_ins, size_t len)
{
    filestruct *fileptr = (filestruct *)nmalloc(sizeof(filestruct));

    /* Convert nulls to newlines.  len is the string's real length
     * here. */
    unsunder(buf, len);

    assert(strlen(buf) == len);

    fileptr->data = mallocstrcpy(NULL, buf);

#ifndef NANO_SMALL
    /* If it's a DOS file ("\r\n"), and file conversion isn't disabled,
     * strip the '\r' part from fileptr->data. */
    if (!ISSET(NO_CONVERT) && len > 0 && buf[len - 1] == '\r')
	fileptr->data[len - 1] = '\0';
#endif

    if (*first_line_ins == TRUE || fileage == NULL) {
	/* Special case: We're inserting with the cursor on the first
	 * line. */
	fileptr->prev = NULL;
	fileptr->next = fileage;
	fileptr->lineno = 1;
	if (*first_line_ins == TRUE) {
	    *first_line_ins = FALSE;
	    /* If we're inserting into the first line of the file, then
	     * we want to make sure that our edit buffer stays on the
	     * first line and that fileage stays up to date. */
	    edittop = fileptr;
	} else
	    filebot = fileptr;
	fileage = fileptr;
    } else {
	assert(prevnode != NULL);

	fileptr->prev = prevnode;
	fileptr->next = NULL;
	fileptr->lineno = prevnode->lineno + 1;
	prevnode->next = fileptr;
    }

    return fileptr;
}

/* Load a file into the edit buffer.  This takes data from the file
 * struct. */
void load_file(void)
{
    current = fileage;

#ifdef ENABLE_MULTIBUFFER
    /* Add a new entry to the open_files structure. */
    add_open_file(FALSE);

    /* Reinitialize the shortcut list. */
    shortcut_init(FALSE);
#endif
}

void read_file(FILE *f, const char *filename)
{
    size_t num_lines = 0;
	/* The number of lines in the file. */
    size_t num_chars;
	/* The number of characters in the file. */
    size_t len = 0;
	/* The length of the current line of the file. */
    size_t i = 0;
	/* The position in the current line of the file. */
    size_t bufx = MAX_BUF_SIZE;
	/* The size of each chunk of the file that we read. */
    char input = '\0';
	/* The current input character. */
    char *buf;
	/* The buffer where we store chunks of the file. */
    filestruct *fileptr = current;
	/* The current line of the file. */
    bool first_line_ins = FALSE;
	/* Whether we're inserting with the cursor on the first line. */
    int input_int;
	/* The current value we read from the file, whether an input
	 * character or EOF. */
#ifndef NANO_SMALL
    int format = 0;
	/* 0 = *nix, 1 = DOS, 2 = Mac, 3 = both DOS and Mac. */
#endif

    buf = charalloc(bufx);
    buf[0] = '\0';

    if (current != NULL) {
	if (current == fileage)
	    first_line_ins = TRUE;
	else
	    fileptr = current->prev;
    }

    /* For the assertion in read_line(), it must be true that if current
     * is NULL, then so is fileage. */
    assert(current != NULL || fileage == NULL);

#ifndef NANO_SMALL
    /* We don't know which file format we have yet, so assume it's a
     * *nix file for now. */
    fmt = NIX_FILE;
#endif

    /* Read the entire file into the file struct. */
    while ((input_int = getc(f)) != EOF) {
	input = (char)input_int;

	/* If it's a *nix file ("\n") or a DOS file ("\r\n"), and file
	 * conversion isn't disabled, handle it! */
	if (input == '\n') {
#ifndef NANO_SMALL
	    /* If there's a '\r' before the '\n', set format to DOS if
	     * we currently think this is a *nix file, or to both if we
	     * currently think it's a Mac file. */
	    if (!ISSET(NO_CONVERT) && i > 0 && buf[i - 1] == '\r' &&
		(format == 0 || format == 2))
		format++;
#endif

	    /* Read in the line properly. */
	    fileptr = read_line(buf, fileptr, &first_line_ins, len);

	    /* Reset the line length in preparation for the next
	     * line. */
	    len = 0;

	    num_lines++;
	    buf[0] = '\0';
	    i = 0;
#ifndef NANO_SMALL
	/* If it's a Mac file ('\r' without '\n'), and file conversion
	 * isn't disabled, handle it! */
	} else if (!ISSET(NO_CONVERT) && i > 0 && buf[i - 1] == '\r') {

	    /* If we currently think the file is a *nix file, set format
	     * to Mac.  If we currently think the file is a DOS file,
	     * set format to both DOS and Mac. */
	    if (format == 0 || format == 1)
		format += 2;

	    /* Read in the line properly. */
	    fileptr = read_line(buf, fileptr, &first_line_ins, len);

	    /* Reset the line length in preparation for the next line.
	     * Since we've already read in the next character, reset it
	     * to 1 instead of 0. */
	    len = 1;

	    num_lines++;
	    buf[0] = input;
	    buf[1] = '\0';
	    i = 1;
#endif
	} else {
	    /* Calculate the total length of the line.  It might have
	     * nulls in it, so we can't just use strlen() here. */
	    len++;

	    /* Now we allocate a bigger buffer MAX_BUF_SIZE characters
	     * at a time.  If we allocate a lot of space for one line,
	     * we may indeed have to use a buffer this big later on, so
	     * we don't decrease it at all.  We do free it at the end,
	     * though. */
	    if (i >= bufx - 1) {
		bufx += MAX_BUF_SIZE;
		buf = charealloc(buf, bufx);
	    }

	    buf[i] = input;
	    buf[i + 1] = '\0';
	    i++;
	}
    }

    /* Perhaps this could use some better handling. */
    if (ferror(f))
	nperror(filename);
    fclose(f);

#ifndef NANO_SMALL
    /* If file conversion isn't disabled and the last character in this
     * file is '\r', read it in properly as a Mac format line. */
    if (len == 0 && !ISSET(NO_CONVERT) && input == '\r') {
	len = 1;

	buf[0] = input;
	buf[1] = '\0';
    }
#endif

    /* Did we not get a newline and still have stuff to do? */
    if (len > 0) {
#ifndef NANO_SMALL
	/* If file conversion isn't disabled and the last character in
	 * this file is '\r', set format to Mac if we currently think
	 * the file is a *nix file, or to both DOS and Mac if we
	 * currently think the file is a DOS file. */
	if (!ISSET(NO_CONVERT) && buf[len - 1] == '\r' &&
		(format == 0 || format == 1))
	    format += 2;
#endif

	/* Read in the last line properly. */
	fileptr = read_line(buf, fileptr, &first_line_ins, len);
	num_lines++;
    }

    free(buf);

    /* If we didn't get a file and we don't already have one, make a new
     * file. */
    if (fileptr == NULL)
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
	}
    }

    get_totals(fileage, filebot, NULL, &num_chars);
    totsize += num_chars;

#ifndef NANO_SMALL
    if (format == 3)
	statusbar(
		P_("Read %lu line (Converted from DOS and Mac format)",
		"Read %lu lines (Converted from DOS and Mac format)",
		(unsigned long)num_lines), (unsigned long)num_lines);
    else if (format == 2) {
	fmt = MAC_FILE;
	statusbar(P_("Read %lu line (Converted from Mac format)",
		"Read %lu lines (Converted from Mac format)",
		(unsigned long)num_lines), (unsigned long)num_lines);
    } else if (format == 1) {
	fmt = DOS_FILE;
	statusbar(P_("Read %lu line (Converted from DOS format)",
		"Read %lu lines (Converted from DOS format)",
		(unsigned long)num_lines), (unsigned long)num_lines);
    } else
#endif
	statusbar(P_("Read %lu line", "Read %lu lines",
		(unsigned long)num_lines), (unsigned long)num_lines);

    totlines += num_lines;
}

/* Open the file (and decide if it exists).  If newfie is TRUE, display
 * "New File" if the file is missing.  Otherwise, say "[filename] not
 * found".
 *
 * Return -2 if we say "New File".  Otherwise, -1 if the file isn't
 * opened, 0 otherwise.  The file might still have an error while
 * reading with a 0 return value.  *f is set to the opened file. */
int open_file(const char *filename, bool newfie, FILE **f)
{
    int fd;
    struct stat fileinfo;

    assert(f != NULL);

    if (filename == NULL || filename[0] == '\0' ||
	    stat(filename, &fileinfo) == -1) {
	if (newfie) {
	    statusbar(_("New File"));
	    return -2;
	}
	statusbar(_("\"%s\" not found"), filename);
	return -1;
    } else if (S_ISDIR(fileinfo.st_mode) || S_ISCHR(fileinfo.st_mode) ||
		S_ISBLK(fileinfo.st_mode)) {
	/* Don't open character or block files.  Sorry, /dev/sndstat! */
	statusbar(S_ISDIR(fileinfo.st_mode) ? _("\"%s\" is a directory")
		: _("File \"%s\" is a device file"), filename);
	return -1;
    } else if ((fd = open(filename, O_RDONLY)) == -1) {
	statusbar(_("Error reading %s: %s"), filename, strerror(errno));
 	return -1;
     } else {
	/* File is A-OK.  Open it in binary mode for our own end-of-line
	 * character munging. */
	*f = fdopen(fd, "rb");

	if (*f == NULL) {
	    statusbar(_("Error reading %s: %s"), filename,
		strerror(errno));
	    close(fd);
	} else
	    statusbar(_("Reading File"));
    }
    return 0;
}

/* This function will return the name of the first available extension
 * of a filename (starting with [name][suffix], then [name][suffix].1,
 * etc.).  Memory is allocated for the return value.  If no writable
 * extension exists, we return "". */
char *get_next_filename(const char *name, const char *suffix)
{
    unsigned long i = 0;
    char *buf;
    size_t namelen, suffixlen;

    assert(name != NULL && suffix != NULL);

    namelen = strlen(name);
    suffixlen = strlen(suffix);

    buf = charalloc(namelen + suffixlen + digits(ULONG_MAX) + 2);
    sprintf(buf, "%s%s", name, suffix);

    while (TRUE) {
	struct stat fs;

	if (stat(buf, &fs) == -1)
	    return buf;
	if (i == ULONG_MAX)
	    break;

	i++;
	sprintf(buf + namelen + suffixlen, ".%lu", i);
    }

    /* We get here only if there is no possible save file.  Blank out
     * the filename to indicate this. */
    null_at(&buf, 0);

    return buf;
}

#ifndef NANO_SMALL
void execute_command(const char *command)
{
#ifdef ENABLE_MULTIBUFFER
    if (ISSET(MULTIBUFFER)) {
	/* Update the current entry in the open_files structure. */
	add_open_file(TRUE);
	new_file();
	UNSET(MODIFIED);
	UNSET(MARK_ISSET);
    }
#endif /* ENABLE_MULTIBUFFER */
    open_pipe(command);
#ifdef ENABLE_MULTIBUFFER
    /* Add this new entry to the open_files structure. */
    if (ISSET(MULTIBUFFER))
	load_file();
#endif /* ENABLE_MULTIBUFFER */
}
#endif /* !NANO_SMALL */

/* name is a file name to open.  We make a new buffer if necessary, then
 * open and read the file. */
void load_buffer(const char *name)
{
    bool new_buffer = (fileage == NULL
#ifdef ENABLE_MULTIBUFFER
	 || ISSET(MULTIBUFFER)
#endif
	);
	/* new_buffer says whether we load into this buffer or a new
	 * one.  If new_buffer is TRUE, we display "New File" if the
	 * file is not found, and if it is found we set filename and add
	 * a new open_files entry. */
    FILE *f;
    int rc;
	/* rc == -2 means that the statusbar displayed "New File".  -1
	 * means that the open failed.  0 means success. */

#ifndef DISABLE_OPERATINGDIR
    if (check_operating_dir(name, FALSE)) {
	statusbar(_("Can't insert file from outside of %s"),
		operating_dir);
	return;
    }
#endif

#ifdef ENABLE_MULTIBUFFER
    /* Update the current entry in the open_files structure. */
    add_open_file(TRUE);
#endif

    rc = open_file(name, new_buffer, &f);

#ifdef ENABLE_MULTIBUFFER
    if (rc != -1 && ISSET(MULTIBUFFER)) {
	UNSET(MODIFIED);
#ifndef NANO_SMALL
	UNSET(MARK_ISSET);
#endif
    }
#endif

    if (rc != -1 && new_buffer) {
	filename = mallocstrcpy(filename, name);
	new_file();
    }

    if (rc == 0) {
	file_format fmt_save = fmt;

	read_file(f, filename);

	/* If we're not loading into a new buffer, preserve the file
	 * format. */
	if (!new_buffer)
	    fmt = fmt_save;

#ifndef NANO_SMALL
	stat(filename, &originalfilestat);
#endif
    }

    /* Add this new entry to the open_files structure if we have
     * multibuffer support, or to the main filestruct if we don't. */
    if (rc != -1 && new_buffer)
	load_file();
}

void do_insertfile(
#ifndef NANO_SMALL
	bool execute
#else
	void
#endif
	)
{
    int i;
    const char *msg;
    char *ans = mallocstrcpy(NULL, "");
	/* The last answer the user typed on the statusbar. */
    filestruct *edittop_save = edittop;
    int current_y_save = current_y;
    bool at_edittop = FALSE;
	/* Whether we're at the top of the edit window. */

#ifndef DISABLE_WRAPPING
    wrap_reset();
#endif

    while (TRUE) {
#ifndef NANO_SMALL
	if (execute) {
#ifdef ENABLE_MULTIBUFFER
	    if (ISSET(MULTIBUFFER))
		msg = N_("Command to execute in new buffer [from %s] ");
	    else
#endif
		msg = N_("Command to execute [from %s] ");
	} else {
#endif
#ifdef ENABLE_MULTIBUFFER
	    if (ISSET(MULTIBUFFER)) {
		msg = N_("File to insert into new buffer [from %s] ");
	    } else
#endif
		msg = N_("File to insert [from %s] ");
#ifndef NANO_SMALL
	}
#endif

	i = statusq(TRUE,
#ifndef NANO_SMALL
		execute ? extcmd_list :
#endif
		insertfile_list, ans,
#ifndef NANO_SMALL
		NULL,
#endif
		_(msg),
#ifndef DISABLE_OPERATINGDIR
		operating_dir != NULL && strcmp(operating_dir, ".") != 0 ?
		operating_dir :
#endif
		"./");

	/* If we're in multibuffer mode and the filename or command is
	 * blank, open a new buffer instead of canceling. */
	if (i == -1 || (i == -2
#ifdef ENABLE_MULTIBUFFER
		&& !ISSET(MULTIBUFFER)
#endif
		)) {
	    statusbar(_("Cancelled"));
	    break;
	} else {
	    size_t pww_save = placewewant;

	    ans = mallocstrcpy(ans, answer);

#ifndef NANO_SMALL
#ifdef ENABLE_MULTIBUFFER
	    if (i == TOGGLE_MULTIBUFFER_KEY) {
		/* Don't allow toggling if we're in view mode. */
		if (!ISSET(VIEW_MODE))
		    TOGGLE(MULTIBUFFER);
		continue;
	    } else
#endif
	    if (i == NANO_TOOTHERINSERT_KEY) {
		execute = !execute;
		continue;
	    }
#ifndef DISABLE_BROWSER
	    else
#endif
#endif /* !NANO_SMALL */

#ifndef DISABLE_BROWSER
	    if (i == NANO_TOFILES_KEY) {
		char *tmp = do_browse_from(answer);

		if (tmp == NULL)
		    continue;

		free(answer);
		answer = tmp;

		/* We have a file now.  Indicate this and get out of the
		 * statusbar prompt cleanly. */
		i = 0;
		statusq_abort();
	    }
#endif

	    /* If we don't have a file yet, go back to the statusbar
	     * prompt. */
	    if (i != 0
#ifdef ENABLE_MULTIBUFFER
		&& (i != -2 || !ISSET(MULTIBUFFER))
#endif
		)
		continue;

#ifdef ENABLE_MULTIBUFFER
	    if (!ISSET(MULTIBUFFER)) {
#endif
		/* If we're not inserting into a new buffer, partition
		 * the filestruct so that it contains no text and hence
		 * looks like a new buffer, and keep track of whether
		 * the top of the partition is the top of the edit
		 * window. */
		filepart = partition_filestruct(current, current_x,
			current, current_x);
		at_edittop = (fileage == edittop);
#ifdef ENABLE_MULTIBUFFER
	    }
#endif

#ifndef NANO_SMALL
	    if (execute)
		execute_command(answer);
	    else {
#endif
		answer = mallocstrassn(answer,
			real_dir_from_tilde(answer));
		load_buffer(answer);
#ifndef NANO_SMALL
	    }
#endif

#ifdef ENABLE_MULTIBUFFER
	    if (!ISSET(MULTIBUFFER))
#endif
	    {
		filestruct *top_save = fileage;

		/* If we didn't insert into a new buffer, and we were at
		 * the top of the edit window before, set the saved
		 * value of edittop to the new top of the edit window,
		 * and update the current y-coordinate to account for
		 * the number of lines inserted. */
		if (at_edittop)
		    edittop_save = fileage;
		current_y += current_y_save;

		/* If we didn't insert into a new buffer, unpartition
		 * the filestruct so that it contains all the text
		 * again.  Note that we've replaced the non-text
		 * originally in the partition with the text in the
		 * inserted file/executed command output. */
		unpartition_filestruct(&filepart);

		/* Renumber starting with the beginning line of the old
		 * partition. */
		renumber(top_save);

		/* Set edittop back to what it was before. */
		edittop = edittop_save;
	    }

#ifdef ENABLE_MULTIBUFFER
	    if (ISSET(MULTIBUFFER)) {
		/* Update the titlebar. */
		titlebar(NULL);

		/* Reinitialize the shortcut list. */
		shortcut_init(FALSE);
	    } else {
#endif
		/* Mark the file as modified. */
		set_modified();

		/* Restore the old place we want. */
		placewewant = pww_save;
#ifdef ENABLE_MULTIBUFFER
	    }
#endif

	    /* Refresh the screen. */
	    edit_refresh();

	    break;
	}
    }

    free(ans);
}

void do_insertfile_void(void)
{
#ifdef ENABLE_MULTIBUFFER
    if (ISSET(VIEW_MODE) && !ISSET(MULTIBUFFER))
	statusbar(_("Key illegal in non-multibuffer mode"));
    else
#endif
	do_insertfile(
#ifndef NANO_SMALL
		FALSE
#endif
		);

    display_main_list();
}

#ifdef ENABLE_MULTIBUFFER
/* Create a new openfilestruct node. */
openfilestruct *make_new_opennode(void)
{
    openfilestruct *newnode =
	(openfilestruct *)nmalloc(sizeof(openfilestruct));
    newnode->filename = NULL;

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

/* Add/update an entry to the open_files openfilestruct.  If update is
 * FALSE, a new entry is created; otherwise, the current entry is
 * updated. */
void add_open_file(bool update)
{
    if (update && open_files == NULL)
	return;

    /* If there are no entries in open_files, make the first one. */
    if (open_files == NULL) {
	open_files = make_new_opennode();
	splice_opennode(open_files, open_files, open_files);
    /* Otherwise, if we're not updating, make a new entry for
     * open_files and splice it in after the current entry. */
    } else if (!update) {
	splice_opennode(open_files, make_new_opennode(),
		open_files->next);
	open_files = open_files->next;
    }

    /* Save the current filename. */
    open_files->filename = mallocstrcpy(open_files->filename, filename);

#ifndef NANO_SMALL
    /* Save the current file's stat. */
    open_files->originalfilestat = originalfilestat;
#endif

    /* Save the current file buffer. */
    open_files->fileage = fileage;
    open_files->filebot = filebot;

    /* Save the current top of the edit window. */
    open_files->edittop = edittop;

    /* Save the current line. */
    open_files->current = current;

    /* Save the current cursor position. */
    open_files->current_x = current_x;

    /* Save the current place we want. */
    open_files->placewewant = placewewant;

    /* Save the current total number of lines. */
    open_files->totlines = totlines;

    /* Save the current total size. */
    open_files->totsize = totsize;

    /* Start with no flags saved. */
    open_files->flags = 0;

    /* Save the current modification status. */
    if (ISSET(MODIFIED))
	open_files->flags |= MODIFIED;

#ifndef NANO_SMALL
    /* Save the current marking status and mark, if applicable. */
    if (ISSET(MARK_ISSET)) {
	open_files->flags |= MARK_ISSET;
	open_files->mark_beginbuf = mark_beginbuf;
	open_files->mark_beginx = mark_beginx;
    }

    /* Save the current file format. */
    open_files->fmt = fmt;
#endif

#ifdef DEBUG
    fprintf(stderr, "filename is %s\n", open_files->filename);
#endif
}

/* Read the current entry in the open_files structure and set up the
 * currently open file buffer using that entry's information. */
void load_open_file(void)
{
    assert(open_files != NULL);

    /* Restore the current filename. */
    filename = mallocstrcpy(filename, open_files->filename);

#ifndef NANO_SMALL
    /* Restore the current file's stat. */
    originalfilestat = open_files->originalfilestat;
#endif

    /* Restore the current file buffer. */
    fileage = open_files->fileage;
    filebot = open_files->filebot;

    /* Restore the current top of the edit window. */
    edittop = open_files->edittop;

    /* Restore the current line. */
    current = open_files->current;

    /* Restore the current cursor position. */
    current_x = open_files->current_x;

    /* Restore the current place we want. */
    placewewant = open_files->placewewant;

    /* Restore the current total number of lines. */
    totlines = open_files->totlines;

    /* Restore the current total size. */
    totsize = open_files->totsize;

    /* Restore the current modification status. */
    if (open_files->flags & MODIFIED)
	SET(MODIFIED);
    else
	UNSET(MODIFIED);

#ifndef NANO_SMALL
    /* Restore the current marking status and mark, if applicable. */
    if (open_files->flags & MARK_ISSET) {
	mark_beginbuf = open_files->mark_beginbuf;
	mark_beginx = open_files->mark_beginx;
	SET(MARK_ISSET);
    } else
	UNSET(MARK_ISSET);

    /* Restore the current file format. */
    fmt = open_files->fmt;
#endif

#ifdef ENABLE_COLOR
    update_color();
#endif
    edit_refresh();

    /* Update the titlebar. */
    titlebar(NULL);
}

/* Open either the next or previous file buffer. */
void open_prevnext_file(bool next_file)
{
    assert(open_files != NULL);

    add_open_file(TRUE);

    /* If only one file buffer is open, indicate it on the statusbar and
     * get out. */
    if (open_files == open_files->next) {
	statusbar(_("No more open file buffers"));
	return;
    }

    /* Switch to the next or previous file, depending on the value of
     * next. */
    open_files = next_file ? open_files->next : open_files->prev;

#ifdef DEBUG
    fprintf(stderr, "filename is %s\n", open_files->filename);
#endif

    /* Load the file we switched to. */
    load_open_file();

    /* And indicate the switch on the statusbar. */
    statusbar(_("Switched to %s"),
      ((open_files->filename[0] == '\0') ? _("New Buffer") :
	open_files->filename));

#ifdef DEBUG
    dump_buffer(current);
#endif
}

/* Open the previous entry in the open_files structure.  This function
 * is used by the shortcut list. */
void open_prevfile_void(void)
{
    open_prevnext_file(FALSE);
}

/* Open the next entry in the open_files structure.  This function is
 * used by the shortcut list. */
void open_nextfile_void(void)
{
    open_prevnext_file(TRUE);
}

/* Delete an entry from the open_files filestruct.  After deletion of an
 * entry, the next entry is opened.  Return TRUE on success or FALSE if
 * there are no more open file buffers. */
bool close_open_file(void)
{
    assert(open_files != NULL);

    /* If only one file is open, get out. */
    if (open_files == open_files->next)
	return FALSE;

    /* Open the next file. */
    open_nextfile_void();

    /* Close the file we had open before. */
    unlink_opennode(open_files->prev);

    /* Reinitialize the shortcut list. */
    shortcut_init(FALSE);
    display_main_list();

    return TRUE;
}
#endif /* ENABLE_MULTIBUFFER */

/* When passed "[relative path]" or "[relative path][filename]" in
 * origpath, return "[full path]" or "[full path][filename]" on success,
 * or NULL on error.  Do this if the file doesn't exist but the relative
 * path does, since the file could exist in memory but not yet on disk).
 * Don't do this if the relative path doesn't exist, since we won't be
 * able to go there. */
char *get_full_path(const char *origpath)
{
    char *d_here, *d_there = NULL;

    if (origpath == NULL)
    	return NULL;

    /* Get the current directory. */
    d_here = charalloc(PATH_MAX + 1);
    d_here = getcwd(d_here, PATH_MAX + 1);

    if (d_here != NULL) {
	const char *last_slash;
	char *d_there_file = NULL;
	bool path_only;
	struct stat fileinfo;

	align(&d_here);

	/* If the current directory isn't "/", tack a slash onto the end
	 * of it. */
	if (strcmp(d_here, "/") != 0) {
	    d_here = charealloc(d_here, strlen(d_here) + 2);
	    strcat(d_here, "/");
	}

	d_there = real_dir_from_tilde(origpath);

	assert(d_there != NULL);

	/* Stat d_there.  If stat() fails, assume that d_there refers to
	 * a new file that hasn't been saved to disk yet.  Set path_only
	 * to TRUE if d_there refers to a directory, and FALSE if
	 * d_there refers to a file. */
	path_only = !stat(d_there, &fileinfo) &&
		S_ISDIR(fileinfo.st_mode);

	/* If path_only is TRUE, make sure d_there ends in a slash. */
	if (path_only) {
	    size_t d_there_len = strlen(d_there);

	    if (d_there[d_there_len - 1] != '/') {
		d_there = charealloc(d_there, d_there_len + 2);
		strcat(d_there, "/");
	    }
	}

	/* Search for the last slash in d_there. */
	last_slash = strrchr(d_there, '/');

	/* If we didn't find one, then make sure the answer is in the
	 * format "d_here/d_there". */
	if (last_slash == NULL) {
	    assert(!path_only);

	    d_there_file = d_there;
	    d_there = d_here;
	} else {
	    /* If path_only is FALSE, then save the filename portion of
	     * the answer, everything after the last slash, in
	     * d_there_file. */
	    if (!path_only)
		d_there_file = mallocstrcpy(NULL, last_slash + 1);

	    /* And remove the filename portion of the answer from
	     * d_there. */
	    null_at(&d_there, last_slash - d_there + 1);

	    /* Go to the path specified in d_there. */
	    if (chdir(d_there) == -1) {
		free(d_there);
		d_there = NULL;
	    } else {
		/* Get the full path and save it in d_there. */
		free(d_there);

		d_there = charalloc(PATH_MAX + 1);
		d_there = getcwd(d_there, PATH_MAX + 1);

		if (d_there != NULL) {
		    align(&d_there);

		    if (strcmp(d_there, "/") != 0) {
			/* Make sure d_there ends in a slash. */
			d_there = charealloc(d_there,
				strlen(d_there) + 2);
			strcat(d_there, "/");
		    }
		} else
		    /* If we couldn't get the full path, set path_only
		     * to TRUE so that we clean up correctly, free all
		     * allocated memory, and return NULL. */
		    path_only = TRUE;

		/* Finally, go back to the path specified in d_here,
		 * where we were before. */
		chdir(d_here);
	    }

	    /* Free d_here, since we're done using it. */
	    free(d_here);
	}

	/* At this point, if path_only is FALSE and d_there exists,
	 * d_there contains the path portion of the answer and
	 * d_there_file contains the filename portion of the answer.  If
	 * this is the case, tack d_there_file onto the end of
	 * d_there, so that d_there contains the complete answer. */
	if (!path_only && d_there != NULL) {
	    d_there = charealloc(d_there, strlen(d_there) +
		strlen(d_there_file) + 1);
	    strcat(d_there, d_there_file);
 	}

	/* Free d_there_file, since we're done using it. */
	free(d_there_file);
    }

    return d_there;
}

/* Return the full version of path, as returned by get_full_path().  On
 * error, if path doesn't reference a directory, or if the directory
 * isn't writable, return NULL. */
char *check_writable_directory(const char *path)
{
    char *full_path = get_full_path(path);

    /* If get_full_path() fails, return NULL. */
    if (full_path == NULL)
	return NULL;

    /* If we can't write to path or path isn't a directory, return
     * NULL. */
    if (access(full_path, W_OK) != 0 ||
	full_path[strlen(full_path) - 1] != '/') {
	free(full_path);
	return NULL;
    }

    /* Otherwise, return the full path. */
    return full_path;
}

/* This function calls mkstemp(($TMPDIR|P_tmpdir|/tmp/)"nano.XXXXXX").
 * On success, it returns the malloc()ed filename and corresponding FILE
 * stream, opened in "r+b" mode.  On error, it returns NULL for the
 * filename and leaves the FILE stream unchanged. */
char *safe_tempfile(FILE **f)
{
    char *full_tempdir = NULL;
    const char *tmpdir_env;
    int fd;
    mode_t original_umask = 0;

    assert(f != NULL);

    /* If $TMPDIR is set and non-empty, set tempdir to it, run it
     * through get_full_path(), and save the result in full_tempdir.
     * Otherwise, leave full_tempdir set to NULL. */
    tmpdir_env = getenv("TMPDIR");
    if (tmpdir_env != NULL && tmpdir_env[0] != '\0')
	full_tempdir = check_writable_directory(tmpdir_env);

    /* If $TMPDIR is unset, empty, or not a writable directory, and
     * full_tempdir is NULL, try P_tmpdir instead. */
    if (full_tempdir == NULL)
	full_tempdir = check_writable_directory(P_tmpdir);

    /* if P_tmpdir is NULL, use /tmp. */
    if (full_tempdir == NULL)
	full_tempdir = mallocstrcpy(NULL, "/tmp/");

    full_tempdir = charealloc(full_tempdir, strlen(full_tempdir) + 12);
    strcat(full_tempdir, "nano.XXXXXX");

    original_umask = umask(0);
    umask(S_IRWXG | S_IRWXO);

    fd = mkstemp(full_tempdir);

    if (fd != -1)
	*f = fdopen(fd, "r+b");
    else {
	free(full_tempdir);
	full_tempdir = NULL;
    }

    umask(original_umask);

    return full_tempdir;
}

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

/* Check to see if we're inside the operating directory.  Return FALSE
 * if we are, or TRUE otherwise.  If allow_tabcomp is TRUE, allow
 * incomplete names that would be matches for the operating directory,
 * so that tab completion will work. */
bool check_operating_dir(const char *currpath, bool allow_tabcomp)
{
    /* The char *full_operating_dir is global for mem cleanup.  It
     * should have already been initialized by init_operating_dir().
     * Also, a relative operating directory path will only be handled
     * properly if this is done. */

    char *fullpath;
    bool retval = FALSE;
    const char *whereami1, *whereami2 = NULL;

    /* If no operating directory is set, don't bother doing anything. */
    if (operating_dir == NULL)
	return FALSE;

    assert(full_operating_dir != NULL);

    fullpath = get_full_path(currpath);

    /* fullpath == NULL means some directory in the path doesn't exist
     * or is unreadable.  If allow_tabcomp is FALSE, then currpath is
     * what the user typed somewhere.  We don't want to report a
     * non-existent directory as being outside the operating directory,
     * so we return FALSE.  If allow_tabcomp is TRUE, then currpath
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
	retval = TRUE;
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

/* Write a file out.  If f_open isn't NULL, we assume that it is a
 * stream associated with the file, and we don't try to open it
 * ourselves.  If tmp is TRUE, we set the umask to disallow anyone else
 * from accessing the file, we don't set the global variable filename to
 * its name, and we don't print out how many lines we wrote on the
 * statusbar.
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
 * Return 0 on success or -1 on error. */
int write_file(const char *name, FILE *f_open, bool tmp, int append,
	bool nonamechange)
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
    bool realexists;
	/* The result of stat().  TRUE if the file exists, FALSE
	 * otherwise.  If name is a link that points nowhere, realexists
	 * is FALSE. */
    struct stat st;
	/* The status fields filled in by stat(). */
    bool anyexists;
	/* The result of lstat().  Same as realexists unless name is a
	 * link. */
    struct stat lst;
	/* The status fields filled in by lstat(). */
    char *realname;
	/* name after tilde expansion. */
    FILE *f = NULL;
	/* The actual file, realname, we are writing to. */
    char *tempname = NULL;
	/* The temp file name we write to on prepend. */

    assert(name != NULL);

    if (name[0] == '\0')
	return -1;

    if (f_open != NULL)
	f = f_open;

    if (!tmp)
	titlebar(NULL);

    realname = real_dir_from_tilde(name);

#ifndef DISABLE_OPERATINGDIR
    /* If we're writing a temporary file, we're probably going outside
     * the operating directory, so skip the operating directory test. */
    if (!tmp && check_operating_dir(realname, FALSE)) {
	statusbar(_("Can't write outside of %s"), operating_dir);
	goto cleanup_and_exit;
    }
#endif

    anyexists = (lstat(realname, &lst) != -1);

    /* If the temp file exists and isn't already open, give up. */
    if (tmp && anyexists && f_open == NULL)
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
    realexists = (stat(realname, &st) != -1);

#ifndef NANO_SMALL
    /* We backup only if the backup toggle is set, the file isn't
     * temporary, and the file already exists.  Furthermore, if we
     * aren't appending, prepending, or writing a selection, we backup
     * only if the file has not been modified by someone else since nano
     * opened it. */
    if (ISSET(BACKUP_FILE) && !tmp && realexists &&
	(append != 0 || ISSET(MARK_ISSET) ||
	originalfilestat.st_mtime == st.st_mtime)) {
	FILE *backup_file;
	char *backupname;
	struct utimbuf filetime;
	int copy_status;

	/* Save the original file's access and modification times. */
	filetime.actime = originalfilestat.st_atime;
	filetime.modtime = originalfilestat.st_mtime;

	if (f_open == NULL) {
	    /* Open the original file to copy to the backup. */
	    f = fopen(realname, "rb");

	    if (f == NULL) {
		statusbar(_("Error reading %s: %s"), realname,
			strerror(errno));
		goto cleanup_and_exit;
	    }
	}

	/* If backup_dir is set, we set backupname to
	 * backup_dir/backupname~[.number], where backupname is the
	 * canonicalized absolute pathname of realname with every '/'
	 * replaced with a '!'.  This means that /home/foo/file is
	 * backed up in backup_dir/!home!foo!file~[.number]. */
	if (backup_dir != NULL) {
	    char *backuptemp = get_full_path(realname);

	    if (backuptemp == NULL)
		/* If get_full_path() failed, we don't have a
		 * canonicalized absolute pathname, so just use the
		 * filename portion of the pathname.  We use tail() so
		 * that e.g. ../backupname will be backed up in
		 * backupdir/backupname~ instead of
		 * backupdir/../backupname~. */
		backuptemp = mallocstrcpy(NULL, tail(realname));
	    else {
		size_t i = 0;

		for (; backuptemp[i] != '\0'; i++) {
		    if (backuptemp[i] == '/')
			backuptemp[i] = '!';
		}
	    }

	    backupname = charalloc(strlen(backup_dir) +
		strlen(backuptemp) + 1);
	    sprintf(backupname, "%s%s", backup_dir, backuptemp);
	    free(backuptemp);
	    backuptemp = get_next_filename(backupname, "~");
	    if (backuptemp[0] == '\0') {
		statusbar(_("Error writing %s: %s"), backupname,
		    _("Too many backup files?"));
		free(backuptemp);
		free(backupname);
		fclose(f);
		goto cleanup_and_exit;
	    } else {
		free(backupname);
		backupname = backuptemp;
	    }
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
	    statusbar(_("Error writing %s: %s"), backupname,
		strerror(errno));
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
	if (copy_status != 0 ||
		chown(backupname, originalfilestat.st_uid,
		originalfilestat.st_gid) == -1 ||
		utime(backupname, &filetime) == -1) {
	    free(backupname);
	    if (copy_status == -1)
		statusbar(_("Error reading %s: %s"), realname,
			strerror(errno));
	    else
		statusbar(_("Error writing %s: %s"), backupname,
			strerror(errno));
	    goto cleanup_and_exit;
	}

	free(backupname);
    }
#endif /* !NANO_SMALL */

    /* If NOFOLLOW_SYMLINKS is set and the file is a link, we aren't
     * doing prepend or append.  So we delete the link first, and just
     * overwrite. */
    if (ISSET(NOFOLLOW_SYMLINKS) && anyexists && S_ISLNK(lst.st_mode) &&
	unlink(realname) == -1) {
	statusbar(_("Error writing %s: %s"), realname, strerror(errno));
	goto cleanup_and_exit;
    }

    if (f_open == NULL) {
	original_umask = umask(0);

	/* If we create a temp file, we don't let anyone else access it.
	 * We create a temp file if tmp is TRUE. */
	if (tmp)
	    umask(S_IRWXG | S_IRWXO);
	else
	    umask(original_umask);
    }

    /* If we're prepending, copy the file to a temp file. */
    if (append == 2) {
	int fd_source;
	FILE *f_source = NULL;

	tempname = safe_tempfile(&f);

	if (tempname == NULL) {
	    statusbar(_("Prepending to %s failed: %s"), realname,
		strerror(errno));
	    goto cleanup_and_exit;
	}

	if (f_open == NULL) {
	    fd_source = open(realname, O_RDONLY | O_CREAT);

	    if (fd_source != -1) {
		f_source = fdopen(fd_source, "rb");
		if (f_source == NULL) {
		    statusbar(_("Error reading %s: %s"), realname,
			strerror(errno));
		    close(fd_source);
		    fclose(f);
		    unlink(tempname);
		    goto cleanup_and_exit;
		}
	    }
	}

	if (copy_file(f_source, f) != 0) {
	    statusbar(_("Error writing %s: %s"), tempname,
		strerror(errno));
	    unlink(tempname);
	    goto cleanup_and_exit;
	}
    }

    if (f_open == NULL) {
	/* Now open the file in place.  Use O_EXCL if tmp is TRUE.  This
	 * is copied from joe, because wiggy says so *shrug*. */
	fd = open(realname, O_WRONLY | O_CREAT |
		((append == 1) ? O_APPEND : (tmp ? O_EXCL : O_TRUNC)),
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH |
		S_IWOTH);

	/* Set the umask back to the user's original value. */
	umask(original_umask);

	/* If we couldn't open the file, give up. */
	if (fd == -1) {
	    statusbar(_("Error writing %s: %s"), realname,
		strerror(errno));

	    /* tempname has been set only if we're prepending. */
	    if (tempname != NULL)
		unlink(tempname);
	    goto cleanup_and_exit;
	}

	f = fdopen(fd, (append == 1) ? "ab" : "wb");

	if (f == NULL) {
	    statusbar(_("Error writing %s: %s"), realname,
		strerror(errno));
	    close(fd);
	    goto cleanup_and_exit;
	}
    }

    /* There might not be a magicline.  There won't be when writing out
     * a selection. */
    assert(fileage != NULL && filebot != NULL);

    while (fileptr != filebot) {
	size_t data_len = strlen(fileptr->data), size;

	/* Newlines to nulls, just before we write to disk. */
	sunder(fileptr->data);

	size = fwrite(fileptr->data, sizeof(char), data_len, f);

	/* Nulls to newlines; data_len is the string's real length. */
	unsunder(fileptr->data, data_len);

	if (size < data_len) {
	    statusbar(_("Error writing %s: %s"), realname,
		strerror(errno));
	    fclose(f);
	    goto cleanup_and_exit;
	}

#ifndef NANO_SMALL
	if (fmt == DOS_FILE || fmt == MAC_FILE) {
	    if (putc('\r', f) == EOF) {
		statusbar(_("Error writing %s: %s"), realname,
			strerror(errno));
		fclose(f);
		goto cleanup_and_exit;
	    }
	}

	if (fmt != MAC_FILE) {
#endif
	    if (putc('\n', f) == EOF) {
		statusbar(_("Error writing %s: %s"), realname,
			strerror(errno));
		fclose(f);
		goto cleanup_and_exit;
	    }
#ifndef NANO_SMALL
	}
#endif

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
	    statusbar(_("Error reading %s: %s"), tempname,
		strerror(errno));
	    fclose(f);
	    goto cleanup_and_exit;
	}

	if (copy_file(f_source, f) == -1 || unlink(tempname) == -1) {
	    statusbar(_("Error writing %s: %s"), realname,
		strerror(errno));
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
	    if (!ISSET(NO_COLOR_SYNTAX))
		edit_refresh();
#endif
	}

#ifndef NANO_SMALL
	/* Update originalfilestat to reference the file as it is now. */
	stat(filename, &originalfilestat);
#endif
	statusbar(P_("Wrote %lu line", "Wrote %lu lines",
		(unsigned long)lineswritten),
		(unsigned long)lineswritten);
	UNSET(MODIFIED);
	titlebar(NULL);
    }

    retval = 0;

  cleanup_and_exit:
    free(realname);
    free(tempname);

    return retval;
}

#ifndef NANO_SMALL
/* Write a marked selection from a file out.  First, set fileage and
 * filebot as the top and bottom of the mark, respectively.  Then call
 * write_file() with the values of name, f_open, temp, and append, and
 * with nonamechange set to TRUE so that we don't change the current
 * filename.  Finally, set fileage and filebot back to their old values
 * and return. */
int write_marked(const char *name, FILE *f_open, bool tmp, int append)
{
    int retval = -1;
    bool old_modified = ISSET(MODIFIED);
	/* write_file() unsets the MODIFIED flag. */
    bool added_magicline;
	/* Whether we added a magicline after filebot. */
    filestruct *top, *bot;
    size_t top_x, bot_x;

    /* Partition the filestruct so that it contains only the marked
     * text. */
    mark_order((const filestruct **)&top, &top_x,
	(const filestruct **)&bot, &bot_x, NULL);
    filepart = partition_filestruct(top, top_x, bot, bot_x);

    /* If the line at filebot is blank, treat it as the magicline and
     * hence the end of the file.  Otherwise, add a magicline and treat
     * it as the end of the file. */
    added_magicline = (filebot->data[0] != '\0');
    if (added_magicline)
	new_magicline();

    retval = write_file(name, f_open, tmp, append, TRUE);

    /* If we added a magicline, remove it now. */
    if (added_magicline)
	remove_magicline();

    /* Unpartition the filestruct so that it contains all the text
     * again. */
    unpartition_filestruct(&filepart);

    if (old_modified)
	set_modified();

    return retval;
}
#endif /* !NANO_SMALL */

int do_writeout(bool exiting)
{
    int i;
    int retval = 0, append = 0;
    char *ans;
	/* The last answer the user typed on the statusbar. */
#ifdef NANO_EXTRA
    static bool did_cred = FALSE;
#endif

    currshortcut = writefile_list;

    if (exiting && filename[0] != '\0' && ISSET(TEMP_FILE)) {
	retval = write_file(filename, NULL, FALSE, 0, FALSE);

	/* Write succeeded. */
	if (retval == 0)
	    return retval;
    }

#ifndef NANO_SMALL
    if (ISSET(MARK_ISSET) && !exiting)
	ans = mallocstrcpy(NULL, "");
    else
#endif
	ans = mallocstrcpy(NULL, filename);

    while (TRUE) {
	const char *msg;
#ifndef NANO_SMALL
	const char *formatstr, *backupstr;

	if (fmt == DOS_FILE)
	   formatstr = N_(" [DOS Format]");
	else if (fmt == MAC_FILE)
	   formatstr = N_(" [Mac Format]");
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
	i = statusq(!ISSET(RESTRICTED) || filename[0] == '\0',
		writefile_list, ans,
#ifndef NANO_SMALL
		NULL, "%s%s%s", _(msg), formatstr, backupstr
#else
		"%s", _(msg)
#endif
		);

	if (i < 0) {
	    statusbar(_("Cancelled"));
	    retval = -1;
	    break;
	} else {
	    ans = mallocstrcpy(ans, answer);

#ifndef DISABLE_BROWSER
	    if (i == NANO_TOFILES_KEY) {
		char *tmp = do_browse_from(answer);

		currshortcut = writefile_list;

		if (tmp == NULL)
		    continue;
		free(answer);
		answer = tmp;

		/* We have a file now.  Get out of the statusbar prompt
		 * cleanly. */
		statusq_abort();
	    } else
#endif /* !DISABLE_BROWSER */
#ifndef NANO_SMALL
	    if (i == TOGGLE_DOS_KEY) {
		fmt = (fmt == DOS_FILE) ? NIX_FILE : DOS_FILE;
		continue;
	    } else if (i == TOGGLE_MAC_KEY) {
		fmt = (fmt == MAC_FILE) ? NIX_FILE : MAC_FILE;
		continue;
	    } else if (i == TOGGLE_BACKUP_KEY) {
		TOGGLE(BACKUP_FILE);
		continue;
	    } else
#endif /* !NANO_SMALL */
	    if (i == NANO_PREPEND_KEY) {
		append = (append == 2) ? 0 : 2;
		continue;
	    } else if (i == NANO_APPEND_KEY) {
		append = (append == 1) ? 0 : 1;
		continue;
	    }

#ifdef DEBUG
	    fprintf(stderr, "filename is %s\n", answer);
#endif

#ifdef NANO_EXTRA
	    if (exiting && !ISSET(TEMP_FILE) &&
		strcasecmp(answer, "zzy") == 0 && !did_cred) {
		do_credits();
		did_cred = TRUE;
		retval = -1;
		break;
	    }
#endif
	    if (append == 0 && strcmp(answer, filename) != 0) {
		struct stat st;

		if (!stat(answer, &st)) {
		    i = do_yesno(FALSE, _("File exists, OVERWRITE ? "));
		    if (i == 0 || i == -1)
			continue;
		/* If we're using restricted mode, we aren't allowed to
		 * change the name of a file once it has one because
		 * that would allow reading from or writing to files not
		 * specified on the command line.  In this case, don't
		 * bother showing the "Different Name" prompt. */
		} else if (!ISSET(RESTRICTED) && filename[0] != '\0'
#ifndef NANO_SMALL
			&& (exiting || !ISSET(MARK_ISSET))
#endif
			) {
		    i = do_yesno(FALSE,
			_("Save file under DIFFERENT NAME ? "));
		    if (i == 0 || i == -1)
			continue;
		}
	    }

#ifndef NANO_SMALL
	    /* Here's where we allow the selected text to be written to
	     * a separate file.  If we're using restricted mode, this is
	     * disabled since it allows reading from or writing to files
	     * not specified on the command line. */
	    if (!ISSET(RESTRICTED) && !exiting && ISSET(MARK_ISSET))
		retval = write_marked(answer, NULL, FALSE, append);
	    else
#endif /* !NANO_SMALL */
		retval = write_file(answer, NULL, FALSE, append, FALSE);

#ifdef ENABLE_MULTIBUFFER
	    /* If we're not about to exit, update the current entry in
	     * the open_files structure. */
	    if (!exiting)
		add_open_file(TRUE);
#endif

	    break;
	}
    } /* while (TRUE) */

    free(ans);

    return retval;
}

void do_writeout_void(void)
{
    do_writeout(FALSE);
    display_main_list();
}

/* Return a malloc()ed string containing the actual directory, used to
 * convert ~user/ and ~/ notation. */
char *real_dir_from_tilde(const char *buf)
{
    char *dirtmp = NULL;

    if (buf == NULL)
    	return NULL;

    if (buf[0] == '~') {
	size_t i;
	const char *tilde_dir;

	/* Figure out how much of the str we need to compare. */
	for (i = 1; buf[i] != '/' && buf[i] != '\0'; i++)
	    ;

	/* Get the home directory. */
	if (i == 1) {
	    get_homedir();
	    tilde_dir = homedir;
	} else {
	    const struct passwd *userdata;

	    do {
		userdata = getpwent();
	    } while (userdata != NULL &&
		strncmp(userdata->pw_name, buf + 1, i - 1) != 0);
	    endpwent();
	    tilde_dir = userdata->pw_dir;
	}

	if (tilde_dir != NULL) {
	    dirtmp = charalloc(strlen(tilde_dir) + strlen(buf + i) + 1);
	    sprintf(dirtmp, "%s%s", tilde_dir, buf + i);
	}
    }

    /* Set a default value for dirtmp, in case the user's home directory
     * isn't found. */
    if (dirtmp == NULL)
	dirtmp = mallocstrcpy(NULL, buf);

    return dirtmp;
}

#if !defined(DISABLE_TABCOMP) || !defined(DISABLE_BROWSER)
/* Our sort routine for file listings.  Sort alphabetically and
 * case-insensitively, and sort directories before filenames. */
int diralphasort(const void *va, const void *vb)
{
    struct stat fileinfo;
    const char *a = *(const char *const *)va;
    const char *b = *(const char *const *)vb;
    bool aisdir = stat(a, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode);
    bool bisdir = stat(b, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode);

    if (aisdir && !bisdir)
	return -1;
    if (!aisdir && bisdir)
	return 1;

    return strcasecmp(a, b);
}
#endif

#ifndef DISABLE_TABCOMP
/* Is the given file a directory? */
int is_dir(const char *buf)
{
    char *dirptr = real_dir_from_tilde(buf);
    struct stat fileinfo;

    int ret = (stat(dirptr, &fileinfo) != -1 &&
	S_ISDIR(fileinfo.st_mode));

    assert(buf != NULL && dirptr != buf);

    free(dirptr);

    return ret;
}

/* These functions (username_tab_completion(), cwd_tab_completion(), and
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
 * This code may safely be consumed by a BSD or GPL license. */

/* We consider the first buflen characters of buf for ~username tab
 * completion. */
char **username_tab_completion(const char *buf, size_t *num_matches,
	size_t buflen)
{
    char **matches = NULL;
    const struct passwd *userdata;

    assert(buf != NULL && num_matches != NULL && buflen > 0);

    *num_matches = 0;

    while ((userdata = getpwent()) != NULL) {
	if (strncmp(userdata->pw_name, buf + 1, buflen - 1) == 0) {
	    /* Cool, found a match.  Add it to the list.  This makes a
	     * lot more sense to me (Chris) this way... */

#ifndef DISABLE_OPERATINGDIR
	    /* ...unless the match exists outside the operating
	     * directory, in which case just go to the next match. */
	    if (check_operating_dir(userdata->pw_dir, TRUE))
		continue;
#endif

	    matches = (char **)nrealloc(matches,
		(*num_matches + 1) * sizeof(char *));
	    matches[*num_matches] =
		charalloc(strlen(userdata->pw_name) + 2);
	    sprintf(matches[*num_matches], "~%s", userdata->pw_name);
	    ++(*num_matches);
	}
    }
    endpwent();

    return matches;
}

/* This was originally called exe_n_cwd_tab_completion(), but we're not
 * worried about executables, only filenames :> */
char **cwd_tab_completion(const char *buf, size_t *num_matches, size_t
	buflen)
{
    char *dirname = mallocstrcpy(NULL, buf);
    char *filename;
#ifndef DISABLE_OPERATINGDIR
    size_t dirnamelen;
#endif
    size_t filenamelen;
    char **matches = NULL;
    DIR *dir;
    const struct dirent *nextdir;

    assert(dirname != NULL && num_matches != NULL && buflen >= 0);

    *num_matches = 0;
    null_at(&dirname, buflen);

    /* Okie, if there's a / in the buffer, strip out the directory
     * part. */
    filename = strrchr(dirname, '/');
    if (filename != NULL) {
	char *tmpdirname = filename + 1;

	filename = mallocstrcpy(NULL, tmpdirname);
	*tmpdirname = '\0';
	tmpdirname = dirname;
	dirname = real_dir_from_tilde(dirname);
	free(tmpdirname);
    } else {
	filename = dirname;
	dirname = mallocstrcpy(NULL, "./");
    }

    assert(dirname[strlen(dirname) - 1] == '/');

    dir = opendir(dirname);

    if (dir == NULL) {
	/* Don't print an error, just shut up and return. */
	beep();
	free(filename);
	free(dirname);
	return NULL;
    }

#ifndef DISABLE_OPERATINGDIR
    dirnamelen = strlen(dirname);
#endif
    filenamelen = strlen(filename);

    while ((nextdir = readdir(dir)) != NULL) {

#ifdef DEBUG
	fprintf(stderr, "Comparing \'%s\'\n", nextdir->d_name);
#endif
	/* See if this matches. */
	if (strncmp(nextdir->d_name, filename, filenamelen) == 0 &&
		(*filename == '.' ||
		(strcmp(nextdir->d_name, ".") != 0 &&
		strcmp(nextdir->d_name, "..") != 0))) {
	    /* Cool, found a match.  Add it to the list.  This makes a
	     * lot more sense to me (Chris) this way... */

#ifndef DISABLE_OPERATINGDIR
	    /* ...unless the match exists outside the operating
	     * directory, in which case just go to the next match.  To
	     * properly do operating directory checking, we have to add
	     * the directory name to the beginning of the proposed match
	     * before we check it. */
	    char *tmp2 = charalloc(strlen(dirname) +
		strlen(nextdir->d_name) + 1);

	    sprintf(tmp2, "%s%s", dirname, nextdir->d_name);
	    if (check_operating_dir(tmp2, TRUE)) {
		free(tmp2);
		continue;
	    }
	    free(tmp2);
#endif

	    matches = (char **)nrealloc(matches,
		(*num_matches + 1) * sizeof(char *));
	    matches[*num_matches] = mallocstrcpy(NULL, nextdir->d_name);
	    ++(*num_matches);
	}
    }
    closedir(dir);
    free(dirname);
    free(filename);

    return matches;
}

/* Do tab completion.  place refers to how much the statusbar cursor
 * position should be advanced. */
char *input_tab(char *buf, size_t *place, bool *lastwastab, bool *list)
{
    size_t num_matches = 0;
    char **matches = NULL;

    assert(buf != NULL && place != NULL && *place <= strlen(buf) && lastwastab != NULL && list != NULL);

    *list = FALSE;

    /* If the word starts with `~' and there is no slash in the word,
     * then try completing this word as a username. */
    if (*place > 0 && *buf == '~') {
	const char *bob = strchr(buf, '/');

	if (bob == NULL || bob >= buf + *place)
	    matches = username_tab_completion(buf, &num_matches,
		*place);
    }

    /* Match against files relative to the current working directory. */
    if (matches == NULL)
	matches = cwd_tab_completion(buf, &num_matches, *place);

    if (num_matches <= 0)
	beep();
    else {
	size_t match, common_len = 0;
	char *mzero;
	const char *lastslash = revstrstr(buf, "/", buf + *place);
	size_t lastslash_len = (lastslash == NULL) ? 0 :
		lastslash - buf + 1;
	char *match1_mb = charalloc(mb_cur_max() + 1);
	char *match2_mb = charalloc(mb_cur_max() + 1);
	int match1_mb_len, match2_mb_len;

	while (TRUE) {
	    for (match = 1; match < num_matches; match++) {
		match1_mb_len = parse_mbchar(matches[0] + common_len,
			match1_mb, NULL, NULL);
		match2_mb_len = parse_mbchar(matches[match] +
			common_len, match2_mb, NULL, NULL);
		match1_mb[match1_mb_len] = '\0';
		match2_mb[match2_mb_len] = '\0';
		if (strcmp(match1_mb, match2_mb) != 0)
		    break;
	    }

	    if (match < num_matches || matches[0][common_len] == '\0')
		break;

	    common_len += parse_mbchar(buf + common_len, NULL, NULL,
		NULL);
	}

	free(match1_mb);
	free(match2_mb);

	mzero = charalloc(lastslash_len + common_len + 1);
	sprintf(mzero, "%.*s%.*s", lastslash_len, buf, common_len,
		matches[0]);

	common_len += lastslash_len;

	assert(common_len >= *place);

	if (num_matches == 1 && is_dir(mzero)) {
	    mzero[common_len] = '/';
	    common_len++;

	    assert(common_len > *place);
	}

	if (num_matches > 1 && (common_len != *place ||
		*lastwastab == FALSE))
	    beep();

	/* If there is more of a match to display on the statusbar, show
	 * it.  We reset lastwastab to FALSE: it requires hitting Tab
	 * twice in succession with no statusbar changes to see a match
	 * list. */
	if (common_len != *place) {
	    size_t buflen = strlen(buf);

	    *lastwastab = FALSE;
	    buf = charealloc(buf, common_len + buflen - *place + 1);
	    charmove(buf + common_len, buf + *place,
		buflen - *place + 1);
	    charcpy(buf, mzero, common_len);
	    *place = common_len;
	} else if (*lastwastab == FALSE || num_matches < 2)
	    *lastwastab = TRUE;
	else {
	    int longest_name = 0, editline = 0;
	    size_t columns;

	    /* Now we show a list of the available choices. */
	    assert(num_matches > 1);

	    /* Sort the list. */
	    qsort(matches, num_matches, sizeof(char *), diralphasort);

	    for (match = 0; match < num_matches; match++) {
		common_len = strnlenpt(matches[match], COLS - 1);

		if (common_len > COLS - 1) {
		    longest_name = COLS - 1;
		    break;
		}

		if (common_len > longest_name)
		    longest_name = common_len;
	    }

	    assert(longest_name <= COLS - 1);

	    /* Each column will be longest_name + 2 characters wide,
	     * i.e, two spaces between columns, except that there will
	     * be only one space after the last column. */
	    columns = (COLS + 1) / (longest_name + 2);

	    /* Blank the edit window, and print the matches out
	     * there. */
	    blank_edit();
	    wmove(edit, 0, 0);

	    /* Disable el cursor. */
	    curs_set(0);

	    for (match = 0; match < num_matches; match++) {
		char *disp;

		wmove(edit, editline, (longest_name + 2) *
			(match % columns));

		if (match % columns == 0 &&
			editline == editwinrows - 1 &&
			num_matches - match > columns) {
		    waddstr(edit, _("(more)"));
		    break;
		}

		disp = display_string(matches[match], 0, longest_name,
			FALSE);
		waddstr(edit, disp);
		free(disp);

		if ((match + 1) % columns == 0)
		    editline++;
	    }
	    wrefresh(edit);
	    *list = TRUE;
	}

	free(mzero);
    }

    free_charptrarray(matches, num_matches);

    /* Only refresh the edit window if we don't have a list of filename
     * matches on it. */
    if (*list == FALSE)
	edit_refresh();

    /* Enable el cursor. */
    curs_set(1);

    return buf;
}
#endif /* !DISABLE_TABCOMP */

/* Only print the last part of a path.  Isn't there a shell command for
 * this? */
const char *tail(const char *foo)
{
    const char *tmp = strrchr(foo, '/');

    if (tmp == NULL)
	tmp = foo;
    else if (*tmp == '/')
	tmp++;

    return tmp;
}

#ifndef DISABLE_BROWSER
/* Free our malloc()ed memory. */
void free_charptrarray(char **array, size_t len)
{
    for (; len > 0; len--)
	free(array[len - 1]);

    free(array);
}

/* Strip one directory from the end of path. */
void striponedir(char *path)
{
    char *tmp;

    assert(path != NULL);

    tmp = strrchr(path, '/');

    if (tmp != NULL)
 	*tmp = '\0';
}

/* Return a list of files contained in the directory path.  *longest is
 * the maximum display length of a file, up to COLS - 1 (but at least
 * 7).  *numents is the number of files.  We assume path exists and is a
 * directory.  If neither is true, we return NULL. */
char **browser_init(const char *path, int *longest, size_t *numents, DIR
	*dir)
{
    const struct dirent *nextdir;
    char **filelist;
    size_t i, path_len;

    assert(dir != NULL);

    *longest = 0;

    i = 0;

    while ((nextdir = readdir(dir)) != NULL) {
	size_t dlen;

	/* Don't show the . entry. */
	if (strcmp(nextdir->d_name, ".") == 0)
	   continue;
	i++;

	dlen = strlenpt(nextdir->d_name);
	if (dlen > *longest)
	    *longest = dlen;
    }

    *numents = i;
    rewinddir(dir);
    *longest += 10;

    filelist = (char **)nmalloc(*numents * sizeof(char *));

    path_len = strlen(path);

    i = 0;

    while ((nextdir = readdir(dir)) != NULL && i < *numents) {
	/* Don't show the "." entry. */
	if (strcmp(nextdir->d_name, ".") == 0)
	   continue;

	filelist[i] = charalloc(path_len + strlen(nextdir->d_name) + 1);
	sprintf(filelist[i], "%s%s", path, nextdir->d_name);
	i++;
    }

    /* Maybe the number of files in the directory changed between the
     * first time we scanned and the second.  i is the actual length of
     * filelist, so record it. */
    *numents = i;
    closedir(dir);

    if (*longest > COLS - 1)
	*longest = COLS - 1;
    if (*longest < 7)
	*longest = 7;

    return filelist;
}

/* Our browser function.  path is the path to start browsing from.
 * Assume path has already been tilde-expanded. */
char *do_browser(char *path, DIR *dir)
{
    int kbinput, longest, selected, width;
    bool meta_key, func_key, old_constupdate = ISSET(CONSTUPDATE);
    size_t numents;
    char **filelist, *retval = NULL;

    curs_set(0);
    blank_statusbar();
    bottombars(browser_list);
    wrefresh(bottomwin);

#if !defined(DISABLE_HELP) || !defined(DISABLE_MOUSE)
    /* Set currshortcut so the user can click in the shortcut area, and
     * so the browser help screen will come up. */
    currshortcut = browser_list;
#endif

    UNSET(CONSTUPDATE);

  change_browser_directory:
	/* We go here after the user selects a new directory. */

    kbinput = ERR;
    selected = 0;
    width = 0;

    path = mallocstrassn(path, get_full_path(path));

    /* Assume that path exists and ends with a slash. */
    assert(path != NULL && path[strlen(path) - 1] == '/');

    /* Get the list of files. */
    filelist = browser_init(path, &longest, &numents, dir);

    assert(filelist != NULL);

    /* Sort the list. */
    qsort(filelist, numents, sizeof(char *), diralphasort);

    titlebar(path);

    /* Loop invariant: Microsoft sucks. */
    do {
	bool abort = FALSE;
	int j, col = 0, editline = 0, fileline;
	int filecols = 0;
	    /* Used only if width == 0, to calculate the number of files
	     * per row below. */
	struct stat st;
	char *new_path;
	    /* Used by the Go To Directory prompt. */
#ifndef DISABLE_MOUSE
	MEVENT mevent;
#endif

	check_statusblank();

	/* Compute the line number we're on now, so that we don't divide
	 * by zero later. */
	fileline = selected;
	if (width != 0)
	    fileline /= width;

	switch (kbinput) {
#ifndef DISABLE_MOUSE
	    case KEY_MOUSE:
		if (getmouse(&mevent) == ERR)
		    break;

		/* If we clicked in the edit window, we probably clicked
		 * on a file. */
		if (wenclose(edit, mevent.y, mevent.x)) {
		    int selectedbackup = selected;

		    mevent.y -= 2;

		    /* longest is the width of each column.  There are
		     * two spaces between each column. */
		    selected = (fileline / editwinrows) * editwinrows *
			width + mevent.y * width + mevent.x /
			(longest + 2);

		    /* If they clicked beyond the end of a row, select
		     * the end of that row. */
		    if (mevent.x > width * (longest + 2))
			selected--;

		    /* If we're off the screen, reset to the last item.
		     * If we clicked the same place as last time, select
		     * this name! */
		    if (selected > numents - 1)
			selected = numents - 1;
		    else if (selectedbackup == selected)
			/* Put back the 'select' key. */
			unget_kbinput('s', FALSE, FALSE);
		} else {
		    /* We must have clicked a shortcut.  Put back the
		     * equivalent shortcut key. */
		    int mouse_x, mouse_y;
		    get_mouseinput(&mouse_x, &mouse_y, TRUE);
		}

		break;
#endif
	    case NANO_PREVLINE_KEY:
		if (selected >= width)
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
	    case '-': /* Pico compatibility. */
		if (selected >= (editwinrows + fileline % editwinrows) *
			width)
		    selected -= (editwinrows + fileline % editwinrows) *
			width;
		else
		    selected = 0;
		break;
	    case NANO_NEXTPAGE_KEY:
	    case NANO_NEXTPAGE_FKEY:
	    case ' ': /* Pico compatibility. */
		selected += (editwinrows - fileline % editwinrows) *
			width;
		if (selected >= numents)
		    selected = numents - 1;
		break;
	    case NANO_HELP_KEY:
	    case NANO_HELP_FKEY:
	    case '?': /* Pico compatibility. */
#ifndef DISABLE_HELP
		do_help();
		curs_set(0);
#else
		nano_disabled_msg();
#endif
		break;
	    case NANO_ENTER_KEY:
	    case 'S': /* Pico compatibility. */
	    case 's':
		/* You can't move up from "/". */
		if (strcmp(filelist[selected], "/..") == 0) {
		    statusbar(_("Can't move up a directory"));
		    beep();
		    break;
		}

#ifndef DISABLE_OPERATINGDIR
		/* Note: the selected file can be outside the operating
		 * directory if it's ".." or if it's a symlink to a
		 * directory outside the operating directory. */
		if (check_operating_dir(filelist[selected], FALSE)) {
		    statusbar(
			_("Can't go outside of %s in restricted mode"),
			operating_dir);
		    beep();
		    break;
		}
#endif

		if (stat(filelist[selected], &st) == -1) {
		    statusbar(_("Error reading %s: %s"),
			filelist[selected], strerror(errno));
		    beep();
		    break;
		}

		if (!S_ISDIR(st.st_mode)) {
		    retval = mallocstrcpy(retval, filelist[selected]);
		    abort = TRUE;
		    break;
		}

		dir = opendir(filelist[selected]);
		if (dir == NULL) {
		    /* We can't open this dir for some reason.
		     * Complain. */
		    statusbar(_("Error reading %s: %s"),
			filelist[selected], strerror(errno));
		    break;
		}

		path = mallocstrcpy(path, filelist[selected]);

		/* Start over again with the new path value. */
		free_charptrarray(filelist, numents);
		goto change_browser_directory;

	    /* Refresh the screen. */
	    case NANO_REFRESH_KEY:
		total_update();
		break;

	    /* Go to a specific directory. */
	    case NANO_GOTOLINE_KEY:
	    case NANO_GOTOLINE_FKEY:
	    case 'G': /* Pico compatibility. */
	    case 'g':
		curs_set(1);

		j = statusq(FALSE, gotodir_list, "",
#ifndef NANO_SMALL
			NULL,
#endif
			_("Go To Directory"));

		curs_set(0);
		bottombars(browser_list);

		if (j < 0) {
		    statusbar(_("Cancelled"));
		    break;
		}

		new_path = real_dir_from_tilde(answer);

		if (new_path[0] != '/') {
		    new_path = charealloc(new_path, strlen(path) +
			strlen(answer) + 1);
		    sprintf(new_path, "%s%s", path, answer);
		}

#ifndef DISABLE_OPERATINGDIR
		if (check_operating_dir(new_path, FALSE)) {
		    statusbar(
			_("Can't go outside of %s in restricted mode"),
			operating_dir);
		    free(new_path);
		    break;
		}
#endif

		dir = opendir(new_path);
		if (dir == NULL) {
		    /* We can't open this dir for some reason.
		     * Complain. */
		    statusbar(_("Error reading %s: %s"), answer,
			strerror(errno));
		    free(new_path);
		    break;
		}

		/* Start over again with the new path value. */
		free(path);
		path = new_path;
		free_charptrarray(filelist, numents);
		goto change_browser_directory;

	    /* Abort the browser. */
	    case NANO_EXIT_KEY:
	    case NANO_EXIT_FKEY:
	    case 'E': /* Pico compatibility. */
	    case 'e':
		abort = TRUE;
		break;
	}

	if (abort)
	    break;

	blank_edit();

	if (width != 0)
	    j = width * editwinrows *
		((selected / width) / editwinrows);
	else
	    j = 0;

	wmove(edit, 0, 0);

	{
	    int foo_len = mb_cur_max() * 7;
	    char *foo = charalloc(foo_len + 1);

	    for (; j < numents && editline <= editwinrows - 1; j++) {
		char *disp = display_string(tail(filelist[j]), 0,
			longest, FALSE);

		/* Highlight the currently selected file/dir. */
		if (j == selected)
		    wattron(edit, A_REVERSE);

		mvwaddnstr(edit, editline, col, hblank, longest);
		mvwaddstr(edit, editline, col, disp);
		free(disp);

		col += longest;
		filecols++;

		/* Show file info also.  We don't want to report file
		 * sizes for links, so we use lstat().  Also, stat() and
		 * lstat() return an error if, for example, the file is
		 * deleted while the file browser is open.  In that
		 * case, we report "--" as the file info. */
		if (lstat(filelist[j], &st) == -1 ||
			S_ISLNK(st.st_mode)) {
		    /* Aha!  It's a symlink!  Now, is it a dir?  If so,
		     * mark it as such. */
		    if (stat(filelist[j], &st) == 0 &&
			S_ISDIR(st.st_mode)) {
			charcpy(foo, _("(dir)"), foo_len);
			foo[foo_len] = '\0';
		    } else
			strcpy(foo, "--");
		} else if (S_ISDIR(st.st_mode)) {
		    charcpy(foo, _("(dir)"), foo_len);
		    foo[foo_len] = '\0';
		} else if (st.st_size < (1 << 10)) /* less than 1 k. */
		    sprintf(foo, "%4u  B", (unsigned int)st.st_size);
		else if (st.st_size < (1 << 20)) /* less than 1 meg. */
		    sprintf(foo, "%4u KB",
			(unsigned int)(st.st_size >> 10));
		else if (st.st_size < (1 << 30)) /* less than 1 gig. */
		    sprintf(foo, "%4u MB",
			(unsigned int)(st.st_size >> 20));
		else
		    sprintf(foo, "%4u GB",
			(unsigned int)(st.st_size >> 30));

		mvwaddnstr(edit, editline, col - strlen(foo), foo,
			foo_len);

		if (j == selected)
		    wattroff(edit, A_REVERSE);

		/* Add some space between the columns. */
		col += 2;

		/* If the next entry isn't going to fit on the line,
		 * move to the next line. */
		if (col > COLS - longest) {
		    editline++;
		    col = 0;
		    if (width == 0)
			width = filecols;
		}

		wmove(edit, editline, col);
	    }

	    free(foo);
	}

	wrefresh(edit);
    } while ((kbinput = get_kbinput(edit, &meta_key, &func_key)) !=
	NANO_EXIT_KEY && kbinput != NANO_EXIT_FKEY);

    blank_edit();
    titlebar(NULL);
    edit_refresh();
    curs_set(1);
    if (old_constupdate)
	SET(CONSTUPDATE);

    /* Clean up. */
    free_charptrarray(filelist, numents);
    free(path);

    return retval;
}

/* The file browser front end.  We check to see if inpath has a dir in
 * it.  If it does, we start do_browser() from there.  Otherwise, we
 * start do_browser() from the current directory. */
char *do_browse_from(const char *inpath)
{
    struct stat st;
    char *path;
	/* This holds the tilde-expanded version of inpath. */
    DIR *dir = NULL;

    assert(inpath != NULL);

    path = real_dir_from_tilde(inpath);

    /* Perhaps path is a directory.  If so, we'll pass it to
     * do_browser().  Or perhaps path is a directory / a file.  If so,
     * we'll try stripping off the last path element and passing it to
     * do_browser().  Or perhaps path doesn't have a directory portion
     * at all.  If so, we'll just pass the current directory to
     * do_browser(). */
    if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
	striponedir(path);
	if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
	    free(path);

	    path = charalloc(PATH_MAX + 1);
	    path = getcwd(path, PATH_MAX + 1);

	    if (path != NULL)
		align(&path);
	}
    }

#ifndef DISABLE_OPERATINGDIR
    /* If the resulting path isn't in the operating directory, use
     * the operating directory instead. */
    if (check_operating_dir(path, FALSE)) {
	if (path != NULL)
	    free(path);
	path = mallocstrcpy(NULL, operating_dir);
    }
#endif

    if (path != NULL)
	dir = opendir(path);

    if (dir == NULL) {
	beep();
	free(path);
	return NULL;
    }

    return do_browser(path, dir);
}
#endif /* !DISABLE_BROWSER */

#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
/* Return $HOME/.nano_history, or NULL if we can't find the homedir.
 * The string is dynamically allocated, and should be freed. */
char *histfilename(void)
{
    char *nanohist = NULL;

    if (homedir != NULL) {
	size_t homelen = strlen(homedir);

	nanohist = charalloc(homelen + 15);
	strcpy(nanohist, homedir);
	strcpy(nanohist + homelen, "/.nano_history");
    }
    return nanohist;
}

void load_history(void)
{
    char *nanohist = histfilename();

    /* Assume do_rcfile() has reported a missing home directory. */
    if (nanohist != NULL) {
	FILE *hist = fopen(nanohist, "r");

	if (hist == NULL) {
	    if (errno != ENOENT) {
		/* Don't save history when we quit. */
		UNSET(HISTORYLOG);
		rcfile_error(N_("Error reading %s: %s"), nanohist,
			strerror(errno));
		fprintf(stderr,
			_("\nPress Return to continue starting nano\n"));
		while (getchar() != '\n')
		    ;
	    }
	} else {
	    /* Load a history (first the search history, then the
	     * replace history) from oldest to newest.  Assume the last
	     * history entry is a blank line. */
	    filestruct **history = &search_history;
	    char *line = NULL;
	    size_t buflen = 0;
	    ssize_t read;

	    while ((read = getline(&line, &buflen, hist)) >= 0) {
		if (read > 0 && line[read - 1] == '\n') {
		    read--;
		    line[read] = '\0';
		}
		if (read > 0) {
		    unsunder(line, read);
		    update_history(history, line);
		} else
		    history = &replace_history;
	    }

	    fclose(hist);
	    free(line);
	}
	free(nanohist);
    }
}

bool writehist(FILE *hist, filestruct *h)
{
    filestruct *p;

    /* Write history from oldest to newest.  Assume the last history
     * entry is a blank line. */
    for (p = h; p != NULL; p = p->next) {
	size_t p_len = strlen(p->data);

	sunder(p->data);

	if (fwrite(p->data, sizeof(char), p_len, hist) < p_len ||
		putc('\n', hist) == EOF)
	    return FALSE;
    }

    return TRUE;
}

/* Save histories to ~/.nano_history. */
void save_history(void)
{
    char *nanohist;

    /* Don't save unchanged or empty histories. */
    if (!history_has_changed() || (searchbot->lineno == 1 &&
	replacebot->lineno == 1))
	return;

    nanohist = histfilename();

    if (nanohist != NULL) {
	FILE *hist = fopen(nanohist, "wb");

	if (hist == NULL)
	    rcfile_error(N_("Error writing %s: %s"), nanohist,
		strerror(errno));
	else {
	    /* Make sure no one else can read from or write to the
	     * history file. */
	    chmod(nanohist, S_IRUSR | S_IWUSR);

	    if (!writehist(hist, searchage) || !writehist(hist,
		replaceage))
		rcfile_error(N_("Error writing %s: %s"), nanohist,
			strerror(errno));

	    fclose(hist);
	}

	free(nanohist);
    }
}
#endif /* !NANO_SMALL && ENABLE_NANORC */
