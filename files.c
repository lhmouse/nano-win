/* $Id$ */
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
#include <ctype.h>
#include <dirent.h>

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
    long size, num_lines = 0, linetemp = 0;
    char input[2];		/* buffer */
    char *buf;
    long i = 0, bufx = 128;
    filestruct *fileptr = current, *tmp = NULL;
    int line1ins = 0;

    buf = nmalloc(bufx);
    buf[0] = '\0';

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
	    num_lines++;
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
	num_lines++;
	buf[0] = 0;
    }
    /* Did we even GET a file? */
    if (totsize == 0 || fileptr == NULL) {
	new_file();
	statusbar(_("Read %d lines"), num_lines);
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
	load_file();
    }
    statusbar(_("Read %d lines"), num_lines);
    totlines += num_lines;

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
	read_file(fd, filename);
    }

    return 1;
}

int do_insertfile(void)
{
    int i;
    char *realname = NULL;

    wrap_reset();
    i = statusq(1, writefile_list, WRITEFILE_LIST_LEN, "",
		_("File to insert [from ./] "));
    if (i != -1) {

#ifdef DEBUG
	fprintf(stderr, "filename is %s", answer);
#endif

#ifndef DISABLE_TABCOMP
	realname = real_dir_from_tilde(answer);
#else
	realname = mallocstrcpy(realname, answer);
#endif

#ifdef ENABLE_BROWSER
	if (i == NANO_TOFILES_KEY) {
	    char *tmp = do_browser(getcwd(NULL, 0));

	    if 	(tmp != NULL) {
		free(realname);
		realname = tmp;
	    }
	    else {
		free(realname);
		return do_insertfile();
	    }
	}
#endif

	i = open_file(realname, 1, 0);
	free(realname);

	dump_buffer(fileage);
	set_modified();

	/* Here we want to rebuild the edit window */
	fix_editbot();

	/* If we've gone off the bottom, recenter, otherwise just redraw */
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

/*
 * Write a file out.  If tmp is nonzero, we set the umask to 0600,
 * we don't set the global variable filename to it's name, and don't
 * print out how many lines we wrote on the statusbar.
 * 
 * tmp means we are writing a tmp file in a secute fashion.  We use
 * it when spell checking or dumping the file on an error.
 */
int write_file(char *name, int tmp)
{
    long size, lineswritten = 0;
    static char *buf = NULL;
    filestruct *fileptr;
    int fd, mask = 0, realexists, anyexists;
    struct stat st, lst;
    static char *realname = NULL;

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
	if (tmp)
	    fd = open(realname, O_WRONLY | O_CREAT | O_EXCL, (S_IRUSR|S_IWUSR));
	else
	    fd = open(realname, O_WRONLY | O_CREAT | O_TRUNC, (S_IRUSR|S_IWUSR));

	/* First, just give up if we couldn't even open the file */
	if (fd == -1) {
	    if (!tmp && ISSET(TEMP_OPT)) {
		UNSET(TEMP_OPT);
		return do_writeout(1);
	    }
	    statusbar(_("Could not open file for writing: %s"),
		      strerror(errno));
	    free(realname);
	    return -1;
	}

    }
    /* Don't follow symlink.  Create new file. */
    else {
	buf = nmalloc(strlen(realname) + 8);
	strncpy(buf, realname, strlen(realname)+1);
	strcat(buf, ".XXXXXX");
	if ((fd = mkstemp(buf)) == -1) {
	    if (ISSET(TEMP_OPT)) {
		UNSET(TEMP_OPT);
		return do_writeout(1);
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
	statusbar(_("Could not close %s: %s"), realname, strerror(errno));
	unlink(buf);
	return -1;
    }

    if (realexists == -1 || tmp ||
	(!ISSET(FOLLOW_SYMLINKS) && S_ISLNK(lst.st_mode))) {

	/* Use default umask as file permisions if file is a new file. */
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
	filename = mallocstrcpy(filename, realname);
	statusbar(_("Wrote %d lines"), lineswritten);
	UNSET(MODIFIED);
	titlebar(NULL);
    }
    return 1;
}

int do_writeout(int exiting)
{
    int i = 0;

#ifdef NANO_EXTRA
    static int did_cred = 0;
#endif

    answer = mallocstrcpy(answer, filename);

    if ((exiting) && (ISSET(TEMP_OPT))) {
	if (filename[0]) {
	    i = write_file(answer, 0);
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
	i = statusq(1, writefile_list, WRITEFILE_LIST_LEN, answer,
		    _("File Name to write"));

	if (i != -1) {

#ifdef ENABLE_BROWSER
	if (i == NANO_TOFILES_KEY) {
	    char *tmp = do_browser(getcwd(NULL, 0));

	    if (tmp != NULL) {
		free(answer);
		answer = tmp;
	    }
	    else
		return do_writeout(exiting);
	}
#endif

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
	    if (strcmp(answer, filename)) {
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

#ifndef DISABLE_TABCOMP
static char **homedirs;

/* Return a malloc()ed string containing the actual directory, used
 * to convert ~user and ~/ notation...
 */
char *real_dir_from_tilde(char *buf)
{
    char *dirtmp = NULL, *line = NULL, byte[1], *lineptr;
    int fd, i, status, searchctr = 1;

    if (buf[0] == '~') {
	if (buf[1] == '~')
	    goto abort;		/* Handle ~~ without segfaulting =) */
	else if (buf[1] == '/') {
	    if (getenv("HOME") != NULL) {
		dirtmp = nmalloc(strlen(buf) + 2 + strlen(getenv("HOME")));

		sprintf(dirtmp, "%s/%s", getenv("HOME"), &buf[2]);
	    }
	} else if (buf[1] != 0) {

	    if ((fd = open("/etc/passwd", O_RDONLY)) == -1)
		goto abort;

	    /* Figure how how much of of the str we need to compare */
	    for (searchctr = 1; buf[searchctr] != '/' &&
		 buf[searchctr] != 0; searchctr++);

	    do {
		i = 0;
		line = nmalloc(1);
		while ((status = read(fd, byte, 1)) != 0
		       && byte[0] != '\n') {

		    line[i] = byte[0];
		    i++;
		    line = nrealloc(line, i + 1);
		}
		line[i] = 0;

		if (i == 0)
		    goto abort;

		line[i] = 0;
		lineptr = strtok(line, ":");

		if (!strncmp(lineptr, &buf[1], searchctr - 1)) {

		    /* Okay, skip to the password portion now */
		    for (i = 0; i <= 4 && lineptr != NULL; i++)
			lineptr = strtok(NULL, ":");

		    if (lineptr == NULL)
			goto abort;

		    /* Else copy the new string into the new buf */
		    dirtmp = nmalloc(strlen(buf) + 2 + strlen(lineptr));

		    sprintf(dirtmp, "%s%s", lineptr, &buf[searchctr]);
		    free(line);
		    break;
		}

		free(line);

	    } while (status != 0);
	}
    } else
	dirtmp = mallocstrcpy(dirtmp, buf);

    return dirtmp;

  abort:
    dirtmp = mallocstrcpy(dirtmp, buf);
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
    char **matches = (char **) NULL, *line = NULL, *lineptr;
    char *matchline = NULL, *matchdir = NULL;

    int fd, i = 0, status = 1;
    char byte[1];

    if ((fd = open("/etc/passwd", O_RDONLY)) == -1) {
	return NULL;
    }

    if (homedirs != NULL) {
	for (i = 0; i < *num_matches; i++)
	    free(homedirs[i]);
	free(homedirs);
	homedirs = (char **) NULL;
	*num_matches = 0;
    }
    matches = nmalloc(BUFSIZ * sizeof(char *));
    homedirs = nmalloc(BUFSIZ * sizeof(char *));
    strcat(buf, "*");
    do {
	i = 0;
	line = nmalloc(1);
	while ((status = read(fd, byte, 1)) != 0 && byte[0] != '\n') {

	    line[i] = byte[0];
	    i++;
	    line = nrealloc(line, i + 1);
	}

	if (i == 0)
	    break;

	line[i] = 0;
	lineptr = strtok(line, ":");

	if (check_wildcard_match(line, &buf[1]) == TRUE) {

	    if (*num_matches == BUFSIZ)
		break;

	    /* Cool, found a match.  Add it to the list
	     * This makes a lot more sense to me (Chris) this way...
	     */
	    matchline = nmalloc(strlen(line) + 2);
	    sprintf(matchline, "~%s", line);

	    for (i = 0; i <= 4 && lineptr != NULL; i++)
		lineptr = strtok(NULL, ":");

	    if (lineptr == NULL)
		break;

	    matchdir = mallocstrcpy(matchdir, lineptr);
	    homedirs[*num_matches] = matchdir;
	    matches[*num_matches] = matchline;

	    ++*num_matches;

	    /* If there's no more room, bail out */
	    if (*num_matches == BUFSIZ)
		break;
	}

	free(line);

    } while (status != 0);

    close(fd);
    return matches;
#ifdef DEBUG
    fprintf(stderr, "\nin username_tab_completion\n");
#endif
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
	dirName = malloc(strlen(buf) + 1);
	tmp = buf + strlen(buf);
	while (*tmp != '/' && tmp != buf)
	    tmp--;

	tmp++;

	strncpy(dirName, buf, tmp - buf + 1);
	dirName[tmp - buf] = 0;

    } else {
	if ((dirName = getcwd(NULL, 0)) == NULL)
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
	    tmp2 = NULL;
	    tmp2 = nmalloc(strlen(next->d_name) + 1);
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
char *input_tab(char *buf, int place, int *lastWasTab, int *newplace)
{
    /* Do TAB completion */
    static int num_matches = 0, match_matches = 0;
    static char **matches = (char **) NULL;
    int pos = place, i = 0, col = 0, editline = 0;
    int longestname = 0, is_dir = 0;
    char *foo;

    if (*lastWasTab == FALSE) {
	char *tmp, *copyto, *matchBuf;

	*lastWasTab = 1;

	/* Make a local copy of the string -- up to the position of the
	   cursor */
	matchBuf = (char *) calloc(strlen(buf) + 2, sizeof(char));

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

	    /* Is it a directory? */
	    append_slash_if_dir(buf, lastWasTab, newplace);

	    break;
	default:
	    /* Check to see if all matches share a beginning, and if so
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
	if (matches && num_matches > 0) {

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

	    foo = nmalloc(longestname + 5);

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
	} else
	    beep();

    }

    edit_refresh();
    curs_set(1);
    return buf;
}
#endif

#ifdef ENABLE_BROWSER

/* Return the stat of the file pointed to by path */
struct stat filestat(const char *path) {
    struct stat st;

    lstat(path, &st);
    return st;
}

/* Our sort routine for file listings - sort directories before
 * files, and then alphabetically
 */ 
int diralphasort(const void *va, const void *vb) {
    struct stat file1info, file2info;
    char *a = *(char **)va, *b = *(char **)vb;
    int answer = 0;

    if (stat(a, &file1info) == -1)
	answer = 1;
    else if (stat(b, &file2info) == -1)
	answer = 1;
    else {
	/* If is a is a dir and b isn't, return -1.
	   Else if b is a dir and a isn't, return 0.
	   Else return a < b */

	if (S_ISDIR(file1info.st_mode) && !S_ISDIR(file2info.st_mode))
	    return -1;
	else if (!S_ISDIR(file1info.st_mode) && S_ISDIR(file2info.st_mode))
	    return 1;
	else
	    answer = strcmp(a, b);
    }

    return(answer);
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
	filelist[i] = nmalloc(strlen(next->d_name) + strlen(path) + 2);

	if (!strcmp(path, "/"))
	    snprintf(filelist[i], strlen(next->d_name) + strlen(path) + 1, 
			"%s%s", path, next->d_name);
	else
	    snprintf(filelist[i], strlen(next->d_name) + strlen(path) + 2, 
			"%s/%s", path, next->d_name);
	i++;
    }

    longest -= strlen(path);

    if (*longest > COLS - 1)
	*longest = COLS - 1;

    return filelist;
}

/* Free our malloced memory */
void free_charptrarray(char **array, int len)
{
    int i;

    for (i = 0; i < len - 1; i++)
	free(array[i]);
    free(array);
}

/* only print the last part of a path, isn't there a shell 
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
	*(tmp+1) = 0;

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
    int lineno = 0;
    char **filelist = (char **) NULL;

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
    foo = nmalloc(longest + 8);

    /* Sort the list by directory first then alphabetically */
    qsort(filelist, numents, sizeof(char *), diralphasort);

    titlebar(path);
    bottombars(browser_list, BROWSER_LIST_LEN);
    curs_set(0);
    wmove(edit, 0, 0);
    i = 0;
    width = 0;
    filecols = 0;

    /* Loop invariant: Microsoft sucks. */
    do {
	blank_edit();
	blank_statusbar();
 	editline = 0;
	col = 0;
	    
	/* Compute line number we're on now so we don't divide by zero later */
	if (width == 0)
	    lineno = selected;
	else
	    lineno = selected / width;

	switch (kbinput) {
	case KEY_UP:
	case 'u':
	    if (selected - width >= 0)
		selected -= width;
	    break;
	case KEY_LEFT:
	case 'l':
	    if (selected > 0)
		selected--;
	    break;
	case KEY_DOWN:
	case 'd':
	    if (selected + width <= numents - 1)
		selected += width;
	    break;
	case KEY_RIGHT:
	case 'r':
	    if (selected < numents - 1)
		selected++;
	    break;
	case NANO_PREVPAGE_KEY:
	case NANO_PREVPAGE_FKEY:
	case KEY_PPAGE:

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
	case KEY_ENTER:
	case NANO_CONTROL_M:
	case 's': /* More Pico compatibility */
	case 'S':

	    /* You can't cd up from / */
	    if (!strcmp(filelist[selected], "/..") && !strcmp(path, "/"))
		statusbar(_("Can't move up a directory"));
	    else
		path = mallocstrcpy(path, filelist[selected]);

	    st = filestat(path);
	    if (S_ISDIR(st.st_mode)) {
		if (opendir(path) == NULL) {
		    /* We can't open this dir for some reason.  Complain */
		    statusbar(_("Can't open \"%s\": %s"), path, strerror(errno));
		    striponedir(path);		    
		    align(&path);
		    break;
		} 

		if (!strcmp("..", tail(path))) {
		    /* They want to go up a level, so strip off .. and the
			current dir */
		    striponedir(path);
		    striponedir(path);
		    align(&path);
		}

		/* Start over again with the new path value */
		return do_browser(path);
	    } else {

		/* Work around for duplicating code */
		ungetch(NANO_EXIT_KEY);
		retval = path;
		abort = 1;
	    }
	    break;
	/* Stuff we want to abort the browser */
	case 'q':
	case 'Q':
	case 'e':	/* Pico compatibility, yeech */
	case 'E':
	case NANO_EXIT_FKEY:
		abort = 1;
		break;
	}
	if (abort)
	    break;

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
	    st = filestat(filelist[j]);
	    if (S_ISDIR(st.st_mode))
		strcpy(foo + longest - 5, "(dir)");
	    else {
		if (S_ISLNK(st.st_mode))
		    strcpy(foo + longest - 2, "--");
		else if (st.st_size < 1024) /* less than 1 K */
		    sprintf(foo + longest - 7, "%4d  B", (int) st.st_size);
		else if (st.st_size > 1073741824) /* at least 1 gig */
		    sprintf(foo + longest - 7, "%4d GB", (int) st.st_size / 1073741824);
		else if (st.st_size > 1048576) /* at least 1 meg */
		    sprintf(foo + longest - 7, "%4d MB", (int) st.st_size / 1048576);
		else /* Its more than 1 k and less than a meg */
		    sprintf(foo + longest - 7, "%4d KB", (int) st.st_size / 1024);
	    }

	    /* Hilight the currently selected file/dir */
	    if (j == selected)
		wattron(edit, A_REVERSE);
	    waddnstr(edit, foo, strlen(foo));
	    if (j == selected)
		wattroff(edit, A_REVERSE);

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

    /* cleanup */
    free_charptrarray(filelist, numents);
    free(foo);
    return retval;
}
#endif

