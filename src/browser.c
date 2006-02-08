/* $Id$ */
/**************************************************************************
 *   browser.c                                                            *
 *                                                                        *
 *   Copyright (C) 2001-2004 Chris Allegretta                             *
 *   Copyright (C) 2005-2006 David Lawrence Ramsey                        *
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

#include "proto.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifndef DISABLE_BROWSER

/* Our browser function.  path is the path to start browsing from.
 * Assume path has already been tilde-expanded. */
char *do_browser(char *path, DIR *dir)
{
    int kbinput, longest, selected, width;
    bool meta_key = FALSE, func_key = FALSE;
    bool old_const_update = ISSET(CONST_UPDATE);
    size_t numents;
    char **filelist, *retval = NULL;

    curs_set(0);
    blank_statusbar();

#if !defined(DISABLE_HELP) || !defined(DISABLE_MOUSE)
    currshortcut = browser_list;
#endif
    bottombars(browser_list);
    wnoutrefresh(bottomwin);

    UNSET(CONST_UPDATE);

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

    do {
	bool abort = FALSE;
	int j, col = 0, editline = 0, fileline;
	int filecols = 0;
	    /* Used only if width == 0, to calculate the number of files
	     * per row below. */
	struct stat st;
	char *new_path;
	    /* Used by the "Go To Directory" prompt. */
#ifndef DISABLE_MOUSE
	MEVENT mevent;
#endif

	check_statusblank();

	/* Compute the line number we're on now, so that we don't divide
	 * by zero later. */
	fileline = selected;
	if (width != 0)
	    fileline /= width;

	parse_browser_input(&kbinput, &meta_key, &func_key);

	switch (kbinput) {
#ifndef DISABLE_MOUSE
	    case KEY_MOUSE:
		if (getmouse(&mevent) == ERR)
		    break;

		/* If we clicked in the edit window, we probably clicked
		 * on a file. */
		if (wenclose(edit, mevent.y, mevent.x)) {
		    int old_selected = selected;

		    /* Subtract out the size of topwin. */
		    mevent.y -= 2 - no_more_space();

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
		    else if (old_selected == selected)
			/* Put back the 'select' key. */
			unget_kbinput(NANO_ENTER_KEY, FALSE, FALSE);
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
		if (selected >= (editwinrows + fileline % editwinrows) *
			width)
		    selected -= (editwinrows + fileline % editwinrows) *
			width;
		else
		    selected = 0;
		break;

	    case NANO_NEXTPAGE_KEY:
		selected += (editwinrows - fileline % editwinrows) *
			width;
		if (selected >= numents)
		    selected = numents - 1;
		break;

	    case NANO_HELP_KEY:
#ifndef DISABLE_HELP
		do_help();
		curs_set(0);
#else
		nano_disabled_msg();
#endif
		break;

	    case NANO_ENTER_KEY:
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
		    /* We can't open this file for some reason.
		     * Complain. */
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
		    beep();
		    break;
		}

		path = mallocstrcpy(path, filelist[selected]);

		/* Start over again with the new path value. */
		free_chararray(filelist, numents);
		goto change_browser_directory;

	    /* Redraw the screen. */
	    case NANO_REFRESH_KEY:
		total_redraw();
		break;

	    /* Go to a specific directory. */
	    case NANO_GOTOLINE_KEY:
		curs_set(1);

		j = do_prompt(TRUE,
#ifndef DISABLE_TABCOMP
			FALSE,
#endif
			gotodir_list, "",
#ifndef NANO_TINY
			NULL,
#endif
			_("Go To Directory"));

		curs_set(0);
#if !defined(DISABLE_HELP) || !defined(DISABLE_MOUSE)
		currshortcut = browser_list;
#endif
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
		    beep();
		    free(new_path);
		    break;
		}

		/* Start over again with the new path value. */
		free(path);
		path = new_path;
		free_chararray(filelist, numents);
		goto change_browser_directory;

	    /* Abort the browser. */
	    case NANO_EXIT_KEY:
		abort = TRUE;
		break;
	}

	if (abort)
	    break;

	blank_edit();

	j = (width != 0) ? width * editwinrows * ((selected / width) /
		editwinrows) : 0;

	wmove(edit, 0, 0);

	{
	    size_t foo_len = mb_cur_max() * 7;
	    char *foo = charalloc(foo_len + 1);

	    for (; j < numents && editline <= editwinrows - 1; j++) {
		char *disp = display_string(tail(filelist[j]), 0,
			longest, FALSE);

		/* Highlight the currently selected file/dir. */
		if (j == selected)
		    wattron(edit, A_REVERSE);

		blank_line(edit, editline, col, longest);
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
			strncpy(foo, _("(dir)"), foo_len);
			foo[foo_len] = '\0';
		    } else
			strcpy(foo, "--");
		} else if (S_ISDIR(st.st_mode)) {
		    strncpy(foo, _("(dir)"), foo_len);
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

	wnoutrefresh(edit);
    } while ((kbinput = get_kbinput(edit, &meta_key, &func_key)) !=
	NANO_EXIT_KEY && kbinput != NANO_EXIT_FKEY);

    blank_edit();
    titlebar(NULL);
    edit_refresh();
    curs_set(1);
    if (old_const_update)
	SET(CONST_UPDATE);

    /* Clean up. */
    free_chararray(filelist, numents);
    free(path);

    return retval;
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
    size_t i = 0, path_len;

    assert(dir != NULL);

    *longest = 0;

    while ((nextdir = readdir(dir)) != NULL) {
	size_t dlen;

	/* Don't show the "." entry. */
	if (strcmp(nextdir->d_name, ".") == 0)
	   continue;
	i++;

	dlen = strlenpt(nextdir->d_name);
	if (dlen > *longest)
	    *longest = (dlen > COLS - 1) ? COLS - 1 : dlen;
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

/* Determine the shortcut key corresponding to the values of kbinput
 * (the key itself), meta_key (whether the key is a meta sequence), and
 * func_key (whether the key is a function key), if any.  In the
 * process, convert certain non-shortcut keys used by Pico's file
 * browser into their corresponding shortcut keys. */
void parse_browser_input(int *kbinput, bool *meta_key, bool *func_key)
{
    get_shortcut(browser_list, kbinput, meta_key, func_key);

    /* Pico compatibility. */
    if (*meta_key == FALSE && *func_key == FALSE) {
	switch (*kbinput) {
	    case ' ':
		*kbinput = NANO_NEXTPAGE_KEY;
		break;
	    case '-':
		*kbinput = NANO_PREVPAGE_KEY;
		break;
	    case '?':
		*kbinput = NANO_HELP_KEY;
		break;
	    case 'E':
	    case 'e':
		*kbinput = NANO_EXIT_KEY;
		break;
	    case 'G':
	    case 'g':
		*kbinput = NANO_GOTOLINE_KEY;
		break;
	    case 'S':
	    case 's':
		*kbinput = NANO_ENTER_KEY;
		break;
	}
    }
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

#endif /* !DISABLE_BROWSER */
