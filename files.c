/**************************************************************************
 *   files.c                                                              *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "proto.h"
#include "nano.h"

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/* Load file into edit buffer - takes data from file struct */
void load_file(void)
{
    current = fileage;
    wmove(edit, current_y, current_x);
}

/* What happens when there is no file to open? aiee! */
void new_file(void)
{
    fileage = nmalloc(sizeof(filestruct));
    fileage->data = nmalloc(1);
    strcpy(fileage->data, "");
    fileage->prev = NULL;
    fileage->next = NULL;
    fileage->lineno = 1;
    filebot = fileage;
    edittop = fileage;
    editbot = fileage;
    current = fileage;
    totlines = 1;
    UNSET(VIEW_MODE);
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
    fileptr->data = nmalloc(strlen(buf) + 2);
    strcpy(fileptr->data, buf);

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


int read_file(int fd, char *filename)
{
    long size, lines = 0, linetemp = 0;
    char input[2];		/* buffer */
    char *buf;
    long i = 0, bufx = 128;
    filestruct *fileptr = current, *tmp = NULL;
    int line1ins = 0;

    buf = nmalloc(bufx);

    if (fileptr != NULL && fileptr->prev != NULL) {
	fileptr = fileptr->prev;
	tmp = fileptr;
    } else if (fileptr != NULL && fileptr->prev == NULL) {
	tmp = fileage;
	current = fileage;
	line1ins = 1;
    }
    input[1] = 0;
    /* Read the entire file into file struct */
    while ((size = read_byte(fd, filename, input)) > 0) {
	linetemp = 0;
	if (input[0] == '\n') {
	    fileptr = read_line(buf, fileptr, &line1ins);
	    lines++;
	    buf[0] = 0;
	    i = 0;
	} else {
	    /* Now we allocate a bigger buffer 128 characters at a time.
	       If we allocate a lot of space for one line, we may indeed 
	       have to use a buffer this big later on, so we don't
	       decrease it at all.  We do free it at the end though. */

	    if (i >= bufx - 1) {
		buf = nrealloc(buf, bufx + 128);
		bufx += 128;
	    }
	    buf[i] = input[0];
	    buf[i + 1] = 0;
	    i++;
	}
	totsize += size;
    }

    /* Did we not get a newline but still have stuff to do? */
    if (buf[0]) {
	fileptr = read_line(buf, fileptr, &line1ins);
	lines++;
	buf[0] = 0;
    }
    /* Did we even GET a file? */
    if (totsize == 0) {
	new_file();
	statusbar(_("Read %d lines"), lines);
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
	load_file();
    }
    statusbar(_("Read %d lines"), lines);
    totlines += lines;

    free(buf);
    close(fd);

    return 1;
}

/* Open the file (and decide if it exists) */
int open_file(char *filename, int insert, int quiet)
{
    int fd;
    struct stat fileinfo;

    if (!strcmp(filename, "") || stat(filename, &fileinfo) == -1) {
	if (insert) {
	    if (!quiet)
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
	return -1;
    } else {			/* File is A-OK */
	if (S_ISDIR(fileinfo.st_mode)) {
	    statusbar(_("File \"%s\" is a directory"), filename);
	    new_file();
	    return -1;
	}
	if (!quiet)
	    statusbar(_("Reading File"));
	read_file(fd, filename);
    }

    return 1;
}

int do_insertfile(void)
{
    int i;

    wrap_reset();
    i = statusq(writefile_list, WRITEFILE_LIST_LEN, "",
		_("File to insert [from ./] "));
    if (i != -1) {

#ifdef DEBUG
	fprintf(stderr, "filename is %s", answer);
#endif

	i = open_file(answer, 1, 0);

	dump_buffer(fileage);
	set_modified();

	/* Here we want to rebuild the edit window */
	for (i = 0, editbot = edittop;
	     i <= editwinrows - 1
	     && i <= totlines
	     && editbot->next != NULL; editbot = editbot->next, i++);

	/* If we've gone off the bottom, recenter, otherwise just redraw */
	if (current->lineno > editbot->lineno)
	    edit_update(current);
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

/*
 * Write a file out.  If tmp is nonzero, we set the umask to 0600,
 * we don't set the global variable filename to it's name, and don't
 * print out how many lines we wrote on the statusbar.
 * 
 * Note that tmp is only set to 1 for storing temporary files internal
 * to the editor, and is completely different from temp_opt.
 */
int write_file(char *name, int tmp)
{
    long size, lineswritten = 0;
    char buf[PATH_MAX + 1];
    filestruct *fileptr;
    int fd, mask = 0;
    struct stat st;

    if (!strcmp(name, "")) {
	statusbar(_("Cancelled"));
	return -1;
    }
    titlebar();
    fileptr = fileage;


    /* Open the file and truncate it.  Trust the symlink. */
    if (ISSET(FOLLOW_SYMLINKS) && !tmp) {
	if ((fd = open(name, O_WRONLY | O_CREAT | O_TRUNC,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH |
		       S_IWOTH)) == -1) {
	    statusbar(_("Could not open file for writing: %s"),
		      strerror(errno));
	    return -1;
	}
    }
    /* Don't follow symlink.  Create new file. */
    else {
	if (strlen(name) > (PATH_MAX - 7)) {
	    statusbar(_("Could not open file: Path length exceeded."));
	    return -1;
	}

	memset(buf, 0x00, PATH_MAX + 1);
	strcat(buf, name);
	strcat(buf, ".XXXXXX");
	if ((fd = mkstemp(buf)) == -1) {
	    statusbar(_("Could not open file for writing: %s"),
		      strerror(errno));
	    return -1;
	}
    }



    dump_buffer(fileage);
    while (fileptr != NULL && fileptr->next != NULL) {
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
	    size = write(fd, "\n", 1);
	    if (size == -1) {
		statusbar(_("Could not open file for writing: %s"),
			  strerror(errno));
		return -1;
	    }
	}
    }


    if (close(fd) == -1) {
	statusbar(_("Could not close %s: %s"), name, strerror(errno));
	unlink(buf);
	return -1;
    }

    if (!ISSET(FOLLOW_SYMLINKS) || tmp) {
	if (stat(name, &st) == -1) {
	    /* Use default umask as file permisions if file is a new file. */
	    mask = umask(0);
	    umask(mask);

	    if (tmp)		/* We don't want anyone reading our temporary file! */
		mask = 0600;
	    else
		mask = 0666 & ~mask;

	} else {
	    /* Use permissions from file we are overwriting. */
	    mask = st.st_mode;
	    if (unlink(name) == -1) {
		if (errno != ENOENT) {
		    statusbar(_("Could not open %s for writing: %s"),
			      name, strerror(errno));
		    unlink(buf);
		    return -1;
		}
	    }
	}

	if (link(buf, name) != -1)
	    unlink(buf);
	else if (errno != EPERM) {
	    statusbar(_("Could not open %s for writing: %s"),
		      name, strerror(errno));
	    unlink(buf);
	    return -1;
	} else if (rename(buf, name) == -1) {	/* Try a rename?? */
	    statusbar(_("Could not open %s for writing: %s"),
		      name, strerror(errno));
	    unlink(buf);
	    return -1;
	}
	if (chmod(name, mask) == -1) {
	    statusbar(_("Could not set permissions %o on %s: %s"),
		      mask, name, strerror(errno));
	}

    }
    if (!tmp) {
	strncpy(filename, name, 132);
	statusbar(_("Wrote %d lines"), lineswritten);
    }
    UNSET(MODIFIED);
    titlebar();
    return 1;
}

int do_writeout(int exiting)
{
    int i = 0;

    strncpy(answer, filename, 132);

    if ((exiting) && (temp_opt) && (filename)) {
	i = write_file(answer, 0);
	display_main_list();
	return i;
    }

    while (1) {
	i = statusq(writefile_list, WRITEFILE_LIST_LEN, answer,
		    _("File Name to write"));

	if (i != -1) {

#ifdef DEBUG
	    fprintf(stderr, _("filename is %s"), answer);
#endif
	    if (strncmp(answer, filename, 132)) {
		struct stat st;
		if (!stat(answer, &st)) {
		    i = do_yesno(0, 0, _("File exists, OVERWRITE ?"));

		    if (!i || (i == -1))
			continue;
		}
	    }
	    i = write_file(answer, 0);

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
    return do_writeout(0);
}
