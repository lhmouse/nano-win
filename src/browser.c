/* $Id$ */
/**************************************************************************
 *   browser.c                                                            *
 *                                                                        *
 *   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,  *
 *   2010, 2011, 2013, 2014 Free Software Foundation, Inc.                *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 3, or (at your option)  *
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

static char **filelist = NULL;
	/* The list of files to display in the file browser. */
static size_t filelist_len = 0;
	/* The number of files in the list. */
static int width = 0;
	/* The number of files that we can display per line. */
static int longest = 0;
	/* The number of columns in the longest filename in the list. */
static size_t selected = 0;
	/* The currently selected filename in the list.  This variable
	 * is zero-based. */

/* Our main file browser function.  path is the tilde-expanded path we
 * start browsing from. */
char *do_browser(char *path, DIR *dir)
{
    char *retval = NULL;
    int kbinput;
    bool old_const_update = ISSET(CONST_UPDATE);
    char *prev_dir = NULL;
	/* The directory we were in before backing up to "..". */
    char *ans = NULL;
	/* The last answer the user typed at the statusbar prompt. */
    size_t old_selected;
	/* The selected file we had before the current selected file. */
    functionptrtype func;
	/* The function of the key the user typed in. */

    curs_set(0);
    blank_statusbar();
    currmenu = MBROWSER;
    bottombars(MBROWSER);
    wnoutrefresh(bottomwin);

    UNSET(CONST_UPDATE);

    ans = mallocstrcpy(NULL, "");

  change_browser_directory:
	/* We go here after we select a new directory. */

    /* Start with no key pressed. */
    kbinput = ERR;

    path = mallocstrassn(path, get_full_path(path));

    assert(path != NULL && path[strlen(path) - 1] == '/');

    /* Get the file list, and set longest and width in the process. */
    browser_init(path, dir);

    assert(filelist != NULL);

    /* Sort the file list. */
    qsort(filelist, filelist_len, sizeof(char *), diralphasort);

    /* If prev_dir isn't NULL, select the directory saved in it, and
     * then blow it away. */
    if (prev_dir != NULL) {
	browser_select_filename(prev_dir);

	free(prev_dir);
	prev_dir = NULL;
    /* Otherwise, select the first file or directory in the list. */
    } else
	selected = 0;

    old_selected = (size_t)-1;

    titlebar(path);

    while (TRUE) {
	struct stat st;
	int i;
	size_t fileline = selected / width;
		/* The line number the selected file is on. */
	char *new_path;
		/* The path we switch to at the "Go to Directory"
		 * prompt. */

	/* Display the file list if we don't have a key, or if the
	 * selected file has changed, and set width in the process. */
	if (kbinput == ERR || old_selected != selected)
	    browser_refresh();

	old_selected = selected;

	kbinput = get_kbinput(edit);

#ifndef DISABLE_MOUSE
	if (kbinput == KEY_MOUSE) {
	    int mouse_x, mouse_y;

	    /* We can click on the edit window to select a
	     * filename. */
	    if (get_mouseinput(&mouse_x, &mouse_y, TRUE) == 0 &&
		wmouse_trafo(edit, &mouse_y, &mouse_x, FALSE)) {
		/* longest is the width of each column.  There
		 * are two spaces between each column. */
		selected = (fileline / editwinrows) *
				(editwinrows * width) + (mouse_y *
				width) + (mouse_x / (longest + 2));

		/* If they clicked beyond the end of a row,
		 * select the last filename in that row. */
		if (mouse_x > width * (longest + 2))
		    selected--;

		/* If we're off the screen, select the last filename. */
		if (selected > filelist_len - 1)
		    selected = filelist_len - 1;

		/* If we selected the same filename as last time,
		 * put back the Enter key so that it's read in. */
		if (old_selected == selected)
		    unget_kbinput(sc_seq_or(do_enter_void, 0), FALSE, FALSE);
	    }
	}
#endif /* !DISABLE_MOUSE */

	func = parse_browser_input(&kbinput);

	if (func == total_refresh) {
	    total_redraw();
	} else if (func == do_help_void) {
#ifndef DISABLE_HELP
	    do_help_void();
	    curs_set(0);
#else
	    nano_disabled_msg();
#endif
	} else if (func == do_search) {
	    /* Search for a filename. */
	    curs_set(1);
	    do_filesearch();
	    curs_set(0);
	} else if (func == do_research) {
	    /* Search for another filename. */
	    do_fileresearch();
	} else if (func == do_page_up) {
	    if (selected >= (editwinrows + fileline % editwinrows) * width)
		selected -= (editwinrows + fileline % editwinrows) * width;
	    else
		selected = 0;
	} else if (func == do_page_down) {
	    selected += (editwinrows - fileline % editwinrows) * width;
	    if (selected > filelist_len - 1)
		selected = filelist_len - 1;
	} else if (func == do_first_file) {
	    selected = 0;
	} else if (func == do_last_file) {
	    selected = filelist_len - 1;
	} else if (func == goto_dir_void) {
	    /* Go to a specific directory. */
	    curs_set(1);
	    i = do_prompt(TRUE,
#ifndef DISABLE_TABCOMP
			FALSE,
#endif
			MGOTODIR, ans,
#ifndef DISABLE_HISTORIES
			NULL,
#endif
			/* TRANSLATORS: This is a prompt. */
			browser_refresh, _("Go To Directory"));

	    curs_set(0);
	    currmenu = MBROWSER;
	    bottombars(MBROWSER);

	    /* If the directory begins with a newline (i.e. an
	     * encoded null), treat it as though it's blank. */
	    if (i < 0 || *answer == '\n') {
		/* We canceled.  Indicate that on the statusbar, and
		* blank out ans, since we're done with it. */
		statusbar(_("Cancelled"));
		ans = mallocstrcpy(ans, "");
		continue;
	    } else if (i != 0) {
		/* Put back the "Go to Directory" key and save
		 * answer in ans, so that the file list is displayed
		 * again, the prompt is displayed again, and what we
		 * typed before at the prompt is displayed again. */
		unget_kbinput(sc_seq_or(do_gotolinecolumn_void, 0), FALSE, FALSE);
		ans = mallocstrcpy(ans, answer);
		continue;
	    }

	    /* We have a directory.  Blank out ans, since we're done
	     * with it. */
	    ans = mallocstrcpy(ans, "");

	    /* Convert newlines to nulls, just before we go to the
	     * directory. */
	    sunder(answer);
	    align(&answer);

	    new_path = real_dir_from_tilde(answer);

	    if (new_path[0] != '/') {
		new_path = charealloc(new_path, strlen(path) +
				strlen(answer) + 1);
		sprintf(new_path, "%s%s", path, answer);
	    }

#ifndef DISABLE_OPERATINGDIR
	    if (check_operating_dir(new_path, FALSE)) {
		statusbar(_("Can't go outside of %s in restricted mode"),
				operating_dir);
		free(new_path);
		continue;
	    }
#endif

	    dir = opendir(new_path);
	    if (dir == NULL) {
		/* We can't open this directory for some reason.
		* Complain. */
		statusbar(_("Error reading %s: %s"), answer,
				strerror(errno));
		beep();
		free(new_path);
		continue;
	    }

	    /* Start over again with the new path value. */
	    free(path);
	    path = new_path;
	    goto change_browser_directory;
	} else if (func == do_up_void) {
	    if (selected >= width)
		selected -= width;
	} else if (func == do_down_void) {
	    if (selected + width <= filelist_len - 1)
		selected += width;
	} else if (func == do_left) {
	    if (selected > 0)
		selected--;
	} else if (func == do_right) {
	    if (selected < filelist_len - 1)
		selected++;
	} else if (func == do_enter_void) {
	    /* We can't move up from "/". */
	    if (strcmp(filelist[selected], "/..") == 0) {
		statusbar(_("Can't move up a directory"));
		beep();
		continue;
	    }

#ifndef DISABLE_OPERATINGDIR
	    /* Note: The selected file can be outside the operating
	     * directory if it's ".." or if it's a symlink to a
	     * directory outside the operating directory. */
	    if (check_operating_dir(filelist[selected], FALSE)) {
		statusbar(_("Can't go outside of %s in restricted mode"),
				operating_dir);
		beep();
		continue;
	    }
#endif

	    if (stat(filelist[selected], &st) == -1) {
		/* We can't open this file for some reason.
		 * Complain. */
		 statusbar(_("Error reading %s: %s"),
				filelist[selected], strerror(errno));
		 beep();
		 continue;
	    }

	    if (!S_ISDIR(st.st_mode)) {
		/* We've successfully opened a file, we're done, so
		 * get out. */
		retval = mallocstrcpy(NULL, filelist[selected]);
		break;
	    } else if (strcmp(tail(filelist[selected]), "..") == 0)
		/* We've successfully opened the parent directory,
		 * save the current directory in prev_dir, so that
		 * we can easily return to it by hitting Enter. */
		prev_dir = mallocstrcpy(NULL, striponedir(filelist[selected]));

	    dir = opendir(filelist[selected]);
	    if (dir == NULL) {
		/* We can't open this directory for some reason.
		 * Complain. */
		statusbar(_("Error reading %s: %s"),
				filelist[selected], strerror(errno));
		beep();
		continue;
	    }

	    path = mallocstrcpy(path, filelist[selected]);

	    /* Start over again with the new path value. */
	    goto change_browser_directory;
	} else if (func == do_exit) {
	    /* Exit from the file browser. */
	    break;
	}
    }
    titlebar(NULL);
    edit_refresh();
    curs_set(1);
    if (old_const_update)
	SET(CONST_UPDATE);

    free(path);
    free(ans);

    free_chararray(filelist, filelist_len);
    filelist = NULL;
    filelist_len = 0;

    return retval;
}

/* The file browser front end.  We check to see if inpath has a
 * directory in it.  If it does, we start do_browser() from there.
 * Otherwise, we start do_browser() from the current directory. */
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
	path = mallocstrassn(path, striponedir(path));

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
    if (check_operating_dir(path, FALSE))
	path = mallocstrcpy(path, operating_dir);
#endif

    if (path != NULL)
	dir = opendir(path);

    /* If we can't open the path, get out. */
    if (dir == NULL) {
	if (path != NULL)
	    free(path);
	beep();
	return NULL;
    }

    return do_browser(path, dir);
}

/* Set filelist to the list of files contained in the directory path,
 * set filelist_len to the number of files in that list, set longest to
 * the width in columns of the longest filename in that list (between 15
 * and COLS), and set width to the number of files that we can display
 * per line.  longest needs to be at least 15 columns in order to
 * display ".. (parent dir)", as Pico does.  Assume path exists and is a
 * directory. */
void browser_init(const char *path, DIR *dir)
{
    const struct dirent *nextdir;
    size_t i = 0, path_len = strlen(path);
    int col = 0;
	/* The maximum number of columns that the filenames will take
	 * up. */
    int line = 0;
	/* The maximum number of lines that the filenames will take
	 * up. */
    int filesperline = 0;
	/* The number of files that we can display per line. */

    assert(path != NULL && path[strlen(path) - 1] == '/' && dir != NULL);

    /* Set longest to zero, just before we initialize it. */
    longest = 0;

    while ((nextdir = readdir(dir)) != NULL) {
	size_t d_len;

	/* Don't show the "." entry. */
	if (strcmp(nextdir->d_name, ".") == 0)
	    continue;

	d_len = strlenpt(nextdir->d_name);
	if (d_len > longest)
	    longest = (d_len > COLS) ? COLS : d_len;

	i++;
    }

    rewinddir(dir);

    /* Put 10 columns' worth of blank space between columns of filenames
     * in the list whenever possible, as Pico does. */
    longest += 10;

    if (filelist != NULL)
	free_chararray(filelist, filelist_len);

    filelist_len = i;

    filelist = (char **)nmalloc(filelist_len * sizeof(char *));

    i = 0;

    while ((nextdir = readdir(dir)) != NULL && i < filelist_len) {
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
    filelist_len = i;

    closedir(dir);

    /* Make sure longest is between 15 and COLS. */
    if (longest < 15)
	longest = 15;
    if (longest > COLS)
	longest = COLS;

    /* Set width to zero, just before we initialize it. */
    width = 0;

    for (i = 0; i < filelist_len && line < editwinrows; i++) {
	/* Calculate the number of columns one filename will take up. */
	col += longest;
	filesperline++;

	/* Add some space between the columns. */
	col += 2;

	/* If the next entry isn't going to fit on the current line,
	 * move to the next line. */
	if (col > COLS - longest) {
	    line++;
	    col = 0;

	    /* If width isn't initialized yet, and we've taken up more
	     * than one line, it means that width is equal to
	     * filesperline. */
	    if (width == 0)
		width = filesperline;
	}
    }

    /* If width isn't initialized yet, and we've taken up only one line,
     * it means that width is equal to longest. */
    if (width == 0)
	width = longest;
}

/* Return the function that is bound to the given key, accepting certain
 * plain characters too, for compatibility with Pico. */
functionptrtype parse_browser_input(int *kbinput)
{
    if (!meta_key) {
	switch (*kbinput) {
	    case ' ':
		return do_page_down;
	    case '-':
		return do_page_up;
	    case '?':
		return do_help_void;
	    case 'E':
	    case 'e':
		return do_exit;
	    case 'G':
	    case 'g':
		return goto_dir_void;
	    case 'S':
	    case 's':
		return do_enter_void;
	    case 'W':
	    case 'w':
		return do_search;
	}
    }
    return func_from_key(kbinput);
}

/* Set width to the number of files that we can display per line, if
 * necessary, and display the list of files. */
void browser_refresh(void)
{
    static int uimax_digits = -1;
    size_t i;
    int col = 0;
	/* The maximum number of columns that the filenames will take
	 * up. */
    int line = 0;
	/* The maximum number of lines that the filenames will take
	 * up. */
    char *foo;
	/* The file information that we'll display. */

    if (uimax_digits == -1)
	uimax_digits = digits(UINT_MAX);

    blank_edit();

    wmove(edit, 0, 0);

    i = width * editwinrows * ((selected / width) / editwinrows);

    for (; i < filelist_len && line < editwinrows; i++) {
	struct stat st;
	const char *filetail = tail(filelist[i]);
		/* The filename we display, minus the path. */
	size_t filetaillen = strlenpt(filetail);
		/* The length of the filename in columns. */
	size_t foolen;
		/* The length of the file information in columns. */
	int foomaxlen = 7;
		/* The maximum length of the file information in
		 * columns: seven for "--", "(dir)", or the file size,
		 * and 12 for "(parent dir)". */
	bool dots = (COLS >= 15 && filetaillen >= longest - foomaxlen - 1);
		/* Do we put an ellipsis before the filename?  Don't set
		 * this to TRUE if we have fewer than 15 columns (i.e.
		 * one column for padding, plus seven columns for a
		 * filename other than ".."). */
	char *disp = display_string(filetail, dots ? filetaillen -
		longest + foomaxlen + 4 : 0, longest, FALSE);
		/* If we put an ellipsis before the filename, reserve
		 * one column for padding, plus seven columns for "--",
		 * "(dir)", or the file size, plus three columns for the
		 * ellipsis. */

	/* Start highlighting the currently selected file or
	 * directory. */
	if (i == selected)
	    wattron(edit, hilite_attribute);

	blank_line(edit, line, col, longest);

	/* If dots is TRUE, we will display something like
	 * "...ename". */
	if (dots)
	    mvwaddstr(edit, line, col, "...");
	mvwaddstr(edit, line, dots ? col + 3 : col, disp);

	free(disp);

	col += longest;

	/* Show information about the file.  We don't want to report
	 * file sizes for links, so we use lstat(). */
	if (lstat(filelist[i], &st) == -1 || S_ISLNK(st.st_mode)) {
	    /* If the file doesn't exist (i.e. it's been deleted while
	     * the file browser is open), or it's a symlink that doesn't
	     * point to a directory, display "--". */
	    if (stat(filelist[i], &st) == -1 || !S_ISDIR(st.st_mode))
		foo = mallocstrcpy(NULL, "--");
	    /* If the file is a symlink that points to a directory,
	     * display it as a directory. */
	    else
		/* TRANSLATORS: Try to keep this at most 7
		 * characters. */
		foo = mallocstrcpy(NULL, _("(dir)"));
	} else if (S_ISDIR(st.st_mode)) {
	    /* If the file is a directory, display it as such. */
	    if (strcmp(filetail, "..") == 0) {
		/* TRANSLATORS: Try to keep this at most 12
		 * characters. */
		foo = mallocstrcpy(NULL, _("(parent dir)"));
		foomaxlen = 12;
	    } else
		foo = mallocstrcpy(NULL, _("(dir)"));
	} else {
	    unsigned long result = st.st_size;
	    char modifier;

	    foo = charalloc(uimax_digits + 4);

	    /* Bytes. */
	    if (st.st_size < (1 << 10))
		modifier = ' ';
	    /* Kilobytes. */
	    else if (st.st_size < (1 << 20)) {
		result >>= 10;
		modifier = 'K';
	    /* Megabytes. */
	    } else if (st.st_size < (1 << 30)) {
		result >>= 20;
		modifier = 'M';
	    /* Gigabytes. */
	    } else {
		result >>= 30;
		modifier = 'G';
	    }

	    sprintf(foo, "%4lu %cB", result, modifier);
	}

	/* Make sure foo takes up no more than foomaxlen columns. */
	foolen = strlenpt(foo);
	if (foolen > foomaxlen) {
	    null_at(&foo, actual_x(foo, foomaxlen));
	    foolen = foomaxlen;
	}

	mvwaddstr(edit, line, col - foolen, foo);

	/* Finish highlighting the currently selected file or
	 * directory. */
	if (i == selected)
	    wattroff(edit, hilite_attribute);

	free(foo);

	/* Add some space between the columns. */
	col += 2;

	/* If the next entry isn't going to fit on the current line,
	 * move to the next line. */
	if (col > COLS - longest) {
	    line++;
	    col = 0;
	}

	wmove(edit, line, col);
    }

    wnoutrefresh(edit);
}

/* Look for needle.  If we find it, set selected to its location.  Note
 * that needle must be an exact match for a file in the list.  The
 * return value specifies whether we found anything. */
bool browser_select_filename(const char *needle)
{
    size_t currselected;
    bool found = FALSE;

    for (currselected = 0; currselected < filelist_len;
	currselected++) {
	if (strcmp(filelist[currselected], needle) == 0) {
	    found = TRUE;
	    break;
	}
    }

    if (found)
	selected = currselected;

    return found;
}

/* Set up the system variables for a filename search.  Return -1 if the
 * search should be canceled (due to Cancel, a blank search string, or a
 * failed regcomp()), return 0 on success, and return 1 on rerun calling
 * program. */
int filesearch_init(void)
{
    int i = 0;
    char *buf;
    static char *backupstring = NULL;
	/* The search string we'll be using. */

    /* If backupstring doesn't exist, initialize it to "". */
    if (backupstring == NULL)
	backupstring = mallocstrcpy(NULL, "");

    if (last_search == NULL)
	last_search = mallocstrcpy(NULL, "");

    if (last_search[0] != '\0') {
	char *disp = display_string(last_search, 0, COLS / 3, FALSE);

	buf = charalloc(strlen(disp) + 7);
	/* We use (COLS / 3) here because we need to see more on the line. */
	sprintf(buf, " [%s%s]", disp,
		(strlenpt(last_search) > COLS / 3) ? "..." : "");
	free(disp);
    } else
	buf = mallocstrcpy(NULL, "");

    /* This is now one simple call.  It just does a lot. */
    i = do_prompt(FALSE,
#ifndef DISABLE_TABCOMP
	TRUE,
#endif
	MWHEREISFILE, backupstring,
#ifndef DISABLE_HISTORIES
	&search_history,
#endif
	browser_refresh, "%s%s", _("Search"), buf);

    /* Release buf now that we don't need it anymore. */
    free(buf);

    free(backupstring);
    backupstring = NULL;

    /* Cancel any search, or just return with no previous search. */
    if (i == -1 || (i < 0 && *last_search == '\0') || (i == 0 &&
	*answer == '\0')) {
	statusbar(_("Cancelled"));
	return -1;
    }

    return 0;
}

/* Look for the given needle in the list of files. */
void findnextfile(const char *needle)
{
    size_t currselected = selected;
	/* The location in the file list of the filename we're looking at. */
    bool came_full_circle = FALSE;
	/* Have we reached the starting file again? */
    const char *filetail = tail(filelist[currselected]);
	/* The filename we display, minus the path. */
    const char *rev_start = filetail, *found = NULL;

    /* Step through each filename in the list until a match is found or
     * we've come back to the point where we started. */
    while (TRUE) {
	found = strstrwrapper(filetail, needle, rev_start);

	/* If we've found a match and it's not the same filename where
	 * we started, then we're done. */
	if (found != NULL && currselected != selected)
	    break;

	/* If we've found a match and we're back at the beginning, then
	 * it's the only occurrence. */
	if (found != NULL && came_full_circle) {
	    statusbar(_("This is the only occurrence"));
	    break;
	}

	if (came_full_circle) {
	    /* We're back at the beginning and didn't find anything. */
	    not_found_msg(needle);
	    return;
	}

	/* Move to the next filename in the list.  If we've reached the
	 * end of the list, wrap around. */
	if (currselected < filelist_len - 1)
	    currselected++;
	else {
	    currselected = 0;
	    statusbar(_("Search Wrapped"));
	}

	if (currselected == selected)
	    /* We've reached the original starting file. */
	    came_full_circle = TRUE;

	filetail = tail(filelist[currselected]);

	rev_start = filetail;
    }

    /* Select the one we've found. */
    selected = currselected;
}

/* Abort the current filename search.  Clean up by setting the current
 * shortcut list to the browser shortcut list, and displaying it. */
void filesearch_abort(void)
{
    currmenu = MBROWSER;
    bottombars(MBROWSER);
}

/* Search for a filename. */
void do_filesearch(void)
{
    UNSET(CASE_SENSITIVE);
    UNSET(USE_REGEXP);
    UNSET(BACKWARDS_SEARCH);

    if (filesearch_init() != 0) {
	/* Cancelled or a blank search string. */
	filesearch_abort();
	return;
    }

    /* If answer is now "", copy last_search into answer. */
    if (*answer == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

#ifndef DISABLE_HISTORIES
    /* If answer is not "", add this search string to the search history
     * list. */
    if (answer[0] != '\0')
	update_history(&search_history, answer);
#endif

    findnextfile(answer);

    filesearch_abort();
}

/* Search for the last given filename again without prompting. */
void do_fileresearch(void)
{
    if (last_search == NULL)
	last_search = mallocstrcpy(NULL, "");

    if (last_search[0] == '\0')
	statusbar(_("No current search pattern"));
    else
	findnextfile(last_search);

    filesearch_abort();
}

/* Select the first file in the list. */
void do_first_file(void)
{
    selected = 0;
}

/* Select the last file in the list. */
void do_last_file(void)
{
    selected = filelist_len - 1;
}

/* Strip one directory from the end of path, and return the stripped
 * path.  The returned string is dynamically allocated, and should be
 * freed. */
char *striponedir(const char *path)
{
    char *retval, *tmp;

    assert(path != NULL);

    retval = mallocstrcpy(NULL, path);

    tmp = strrchr(retval, '/');

    if (tmp != NULL)
	null_at(&retval, tmp - retval);

    return retval;
}

#endif /* !DISABLE_BROWSER */
