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

static char **filelist = NULL;
	/* The list of files to display in the browser. */
static size_t filelist_len = 0;
	/* The number of files in the list. */
static int width = 0;
	/* The number of columns to display the list in. */
static int longest = 0;
	/* The number of columns in the longest filename in the list. */
static size_t selected = 0;
	/* The currently selected filename in the list.  This variable
	 * is zero-based. */
static bool search_last_file = FALSE;
	/* Have we gone past the last file while searching? */

/* Our browser function.  path is the tilde-expanded path to start
 * browsing from. */
char *do_browser(char *path, DIR *dir)
{
    int kbinput;
    bool meta_key, func_key, old_const_update = ISSET(CONST_UPDATE);
    char *prev_dir = NULL;
	/* The directory we were in, if any, before backing up via
	 * entering "..". */
    char *ans = mallocstrcpy(NULL, "");
	/* The last answer the user typed on the statusbar. */
    char *retval = NULL;

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
    meta_key = FALSE;
    func_key = FALSE;
    width = 0;
    selected = 0;

    path = mallocstrassn(path, get_full_path(path));

    /* Assume that path exists and ends with a slash. */
    assert(path != NULL && path[strlen(path) - 1] == '/');

    /* Get the file list. */
    browser_init(path, dir);

    assert(filelist != NULL);

    /* Sort the list. */
    qsort(filelist, filelist_len, sizeof(char *), diralphasort);

    titlebar(path);

    /* If prev_dir isn't NULL, select the directory saved in it, and
     * then blow it away. */
    if (prev_dir != NULL) {
	browser_select_filename(prev_dir);

	free(prev_dir);
	prev_dir = NULL;
    }

    do {
	size_t fileline;
		/* The line number the selected file is on. */
	size_t old_selected = selected;
		/* We display the file list only if the selected file
		 * changed. */
	struct stat st;
	int i;
	char *new_path;

	/* Compute the line number we're on now, so that we don't divide
	 * by zero. */
	fileline = selected;
	if (width != 0)
	    fileline /= width;

	switch (kbinput) {
#ifndef DISABLE_MOUSE
	    case KEY_MOUSE:
		{
		    int mouse_x, mouse_y;

		    if (!get_mouseinput(&mouse_x, &mouse_y, TRUE)) {
			/* We can click in the edit window to select a
			 * file. */
			if (wenclose(edit, mouse_y, mouse_x)) {
			    /* Subtract out the size of topwin. */
			    mouse_y -= 2 - no_more_space();

			    /* longest is the width of each column.
			     * There are two spaces between each
			     * column. */
			    selected = (fileline / editwinrows) *
				(editwinrows * width) + (mouse_y *
				width) + (mouse_x / (longest + 2));

			    /* If they clicked beyond the end of a row,
			     * select the file at the end of that
			     * row. */
			    if (mouse_x > width * (longest + 2))
				selected--;

			    /* If we're off the screen, select the last
			     * file.  If we clicked the same place as
			     * last time, read in the file there. */
			    if (selected > filelist_len - 1)
				selected = filelist_len - 1;
			    else if (old_selected == selected)
				/* Put back the Enter key, so that the
				 * file is read in. */
				unget_kbinput(NANO_ENTER_KEY, FALSE,
					FALSE);
			}
		    }
		}
		break;
#endif /* !DISABLE_MOUSE */
	    /* Redraw the screen. */
	    case NANO_REFRESH_KEY:
		total_redraw();
		kbinput = ERR;
		break;
	    case NANO_HELP_KEY:
#ifndef DISABLE_HELP
		do_browser_help();
		curs_set(0);
#else
		nano_disabled_msg();
#endif
		break;
	    /* Search for a filename. */
	    case NANO_WHEREIS_KEY:
		curs_set(1);
		do_filesearch();
		curs_set(0);
		break;
	    /* Search for another filename. */
	    case NANO_WHEREIS_NEXT_KEY:
		do_fileresearch();
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
		if (selected >= filelist_len)
		    selected = filelist_len - 1;
		break;
	    case NANO_FIRSTFILE_ALTKEY:
		if (meta_key)
		    selected = 0;
		break;
	    case NANO_LASTFILE_ALTKEY:
		if (meta_key)
		    selected = filelist_len - 1;
		break;
	    /* Go to a specific directory. */
	    case NANO_GOTODIR_KEY:
		curs_set(1);

		i = do_prompt(TRUE,
#ifndef DISABLE_TABCOMP
			FALSE,
#endif
			gotodir_list, ans,
#ifndef NANO_TINY
			NULL,
#endif
			browser_refresh, _("Go To Directory"));

		curs_set(0);
#if !defined(DISABLE_HELP) || !defined(DISABLE_MOUSE)
		currshortcut = browser_list;
#endif
		bottombars(browser_list);

		if (i < 0) {
		    /* We canceled.  Indicate that on the statusbar, and
		     * blank out ans, since we're done with it. */
		    statusbar(_("Cancelled"));
		    ans = mallocstrcpy(ans, "");
		    break;
		} else if (i != 0) {
		    /* Put back the "Go to Directory" key and save
		     * answer in ans, so that the file list is displayed
		     * again, the prompt is displayed again, and what we
		     * typed before at the prompt is displayed again. */
		    unget_kbinput(NANO_GOTOLINE_KEY, FALSE, FALSE);
		    ans = mallocstrcpy(ans, answer);
		    break;
		}

		/* We have a directory.  Blank out ans, since we're done
		 * with it. */
		ans = mallocstrcpy(ans, "");

		new_path = real_dir_from_tilde(answer);

		if (new_path[0] != '/') {
		    new_path = charealloc(new_path, strlen(new_path) +
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
		    /* We can't open this directory for some reason.
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
		free_chararray(filelist, filelist_len);
		goto change_browser_directory;
	    case NANO_PREVLINE_KEY:
		if (selected >= width)
		    selected -= width;
		break;
	    case NANO_BACK_KEY:
		if (selected > 0)
		    selected--;
		break;
	    case NANO_NEXTLINE_KEY:
		if (selected + width <= filelist_len - 1)
		    selected += width;
		break;
	    case NANO_FORWARD_KEY:
		if (selected < filelist_len - 1)
		    selected++;
		break;
	    case NANO_ENTER_KEY:
		/* We can't move up from "/". */
		if (strcmp(filelist[selected], "/..") == 0) {
		    statusbar(_("Can't move up a directory"));
		    beep();
		    break;
		}

#ifndef DISABLE_OPERATINGDIR
		/* Note: The selected file can be outside the operating
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

		/* If we've successfully opened a file, we're done, so
		 * get out. */
		if (!S_ISDIR(st.st_mode)) {
		    retval = mallocstrcpy(NULL, filelist[selected]);
		    kbinput = NANO_EXIT_KEY;
		    break;
		/* If we've successfully opened a directory, and it's
		 * "..", save the current directory in prev_dir, so that
		 * we can select it later. */
		} else if (strcmp(tail(filelist[selected]), "..") == 0)
		    prev_dir = mallocstrcpy(NULL,
			striponedir(filelist[selected]));

		dir = opendir(filelist[selected]);
		if (dir == NULL) {
		    /* We can't open this directory for some reason.
		     * Complain. */
		    statusbar(_("Error reading %s: %s"),
			filelist[selected], strerror(errno));
		    beep();
		    break;
		}

		path = mallocstrcpy(path, filelist[selected]);

		/* Start over again with the new path value. */
		free_chararray(filelist, filelist_len);
		goto change_browser_directory;
	}

	/* Display the file list if we don't have a key, or if we do
	 * have a key and the selected file has changed. */
	if (kbinput == ERR || old_selected == selected)
	    browser_refresh();

	kbinput = get_kbinput(edit, &meta_key, &func_key);
	parse_browser_input(&kbinput, &meta_key, &func_key);
    } while (kbinput != NANO_EXIT_KEY);

    titlebar(NULL);
    edit_refresh();
    curs_set(1);
    if (old_const_update)
	SET(CONST_UPDATE);

    /* Clean up. */
    free(path);
    free(ans);
    free_chararray(filelist, filelist_len);

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

/* Set filelist to the list of files contained in the directory path,
 * set filelist_len to the number of files in that list, and set longest
 * to the width in columns of the longest filename in that list, up to
 * COLS - 1 (but at least 15).  Assume path exists and is a
 * directory. */
void browser_init(const char *path, DIR *dir)
{
    const struct dirent *nextdir;
    size_t i = 0, path_len;

    assert(dir != NULL);

    longest = 0;

    while ((nextdir = readdir(dir)) != NULL) {
	size_t d_len;

	/* Don't show the "." entry. */
	if (strcmp(nextdir->d_name, ".") == 0)
	   continue;

	i++;

	d_len = strlenpt(nextdir->d_name);
	if (d_len > longest)
	    longest = (d_len > COLS - 1) ? COLS - 1 : d_len;
    }

    filelist_len = i;
    rewinddir(dir);
    longest += 10;

    filelist = (char **)nmalloc(filelist_len * sizeof(char *));

    path_len = strlen(path);

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

    if (longest > COLS - 1)
	longest = COLS - 1;
    if (longest < 15)
	longest = 15;
}

/* Determine the shortcut key corresponding to the values of kbinput
 * (the key itself), meta_key (whether the key is a meta sequence), and
 * func_key (whether the key is a function key), if any.  In the
 * process, convert certain non-shortcut keys into their corresponding
 * shortcut keys. */
void parse_browser_input(int *kbinput, bool *meta_key, bool *func_key)
{
    get_shortcut(browser_list, kbinput, meta_key, func_key);

    /* Pico compatibility. */
    if (*meta_key == FALSE) {
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
	    /* Cancel is equivalent to Exit here. */
	    case NANO_CANCEL_KEY:
	    case 'E':
	    case 'e':
		*kbinput = NANO_EXIT_KEY;
		break;
	    case 'G':
	    case 'g':
		*kbinput = NANO_GOTODIR_KEY;
		break;
	    case 'S':
	    case 's':
		*kbinput = NANO_ENTER_KEY;
		break;
	    case 'W':
	    case 'w':
		*kbinput = NANO_WHEREIS_KEY;
		break;
	}
    }
}

/* Calculate the number of columns needed to display the list of files
 * in the array filelist, if necessary, and then display the list of
 * files. */
void browser_refresh(void)
{
    static int uimax_digits = -1;
    size_t i;
    int col = 0, line = 0, filecols = 0;
    char *foo;

    if (uimax_digits == -1)
	uimax_digits = digits(UINT_MAX);

    blank_edit();

    wmove(edit, 0, 0);

    i = (width != 0) ? width * editwinrows * ((selected / width) /
	editwinrows) : 0;

    for (; i < filelist_len && line < editwinrows; i++) {
	struct stat st;
	const char *filetail = tail(filelist[i]);
	char *disp = display_string(filetail, 0, longest, FALSE);
	size_t foo_col;

	/* Highlight the currently selected file or directory. */
	if (i == selected)
	    wattron(edit, reverse_attr);

	blank_line(edit, line, col, longest);
	mvwaddstr(edit, line, col, disp);
	free(disp);

	col += longest;
	filecols++;

	/* Show information about the file.  We don't want to report
	 * file sizes for links, so we use lstat(). */
	if (lstat(filelist[i], &st) == -1 || S_ISLNK(st.st_mode)) {
	    /* If the file doesn't exist (i.e, it's been deleted while
	     * the file browser is open), or it's a symlink that doesn't
	     * point to a directory, display "--". */
	    if (stat(filelist[i], &st) == -1 || !S_ISDIR(st.st_mode))
		foo = mallocstrcpy(NULL, "--");
	    /* If the file is a symlink that points to a directory,
	     * display it as a directory. */
	    else
		foo = mallocstrcpy(NULL, _("(dir)"));
	} else if (S_ISDIR(st.st_mode))
	    /* If the file is a directory, display it as such. */
	    foo = mallocstrcpy(NULL, (strcmp(filetail, "..") == 0) ?
		_("(parent dir)") : _("(dir)"));
	else {
	    foo = charalloc(uimax_digits + 4);

	    /* Bytes. */
	    if (st.st_size < (1 << 10)) 
		sprintf(foo, "%4u  B", (unsigned int)st.st_size);
	    /* Kilobytes. */
	    else if (st.st_size < (1 << 20))
		sprintf(foo, "%4u KB",
			(unsigned int)(st.st_size >> 10));
	    /* Megabytes. */
	    else if (st.st_size < (1 << 30))
		sprintf(foo, "%4u MB",
			(unsigned int)(st.st_size >> 20));
	    /* Gigabytes. */
	    else
		sprintf(foo, "%4u GB",
			(unsigned int)(st.st_size >> 30));
	}

	foo_col = col - strlenpt(foo);

	mvwaddnstr(edit, line, foo_col, foo, actual_x(foo, longest -
		foo_col));

	if (i == selected)
	    wattroff(edit, reverse_attr);

	free(foo);

	/* Add some space between the columns. */
	col += 2;

	/* If the next entry isn't going to fit on the current line,
	 * move to the next line. */
	if (col > COLS - longest) {
	    line++;
	    col = 0;

	    /* Set the number of columns to display the list in, if
	     * necessary, so that we don't divide by zero. */
	    if (width == 0)
		width = filecols;
	}

	wmove(edit, line, col);
    }

    wnoutrefresh(edit);
}

/* Look for needle.  If we find it, set selected to its location, and
 * update the screen.  Note that needle must be an exact match for a
 * file in the list. */
void browser_select_filename(const char *needle)
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

    if (found) {
	selected = currselected;
	browser_refresh();
    }
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

    /* We display the search prompt below.  If the user types a partial
     * search string and then Replace or a toggle, we will return to
     * do_search() or do_replace() and be called again.  In that case,
     * we should put the same search string back up. */

    search_init_globals();

    if (last_search[0] != '\0') {
	char *disp = display_string(last_search, 0, COLS / 3, FALSE);

	buf = charalloc(strlen(disp) + 7);
	/* We use (COLS / 3) here because we need to see more on the
	 * line. */
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
	whereis_file_list, backupstring,
#ifndef NANO_TINY
	&search_history,
#endif
	browser_refresh, "%s%s%s%s%s%s", _("Search"),
#ifndef NANO_TINY
	/* This string is just a modifier for the search prompt; no
	 * grammar is implied. */
	ISSET(CASE_SENSITIVE) ? _(" [Case Sensitive]") :
#endif
	"",
#ifdef HAVE_REGEX_H
	/* This string is just a modifier for the search prompt; no
	 * grammar is implied. */
	ISSET(USE_REGEXP) ? _(" [Regexp]") :
#endif
	"",
#ifndef NANO_TINY
	/* This string is just a modifier for the search prompt; no
	 * grammar is implied. */
	ISSET(BACKWARDS_SEARCH) ? _(" [Backwards]") :
#endif
	"", "", buf);

    /* Release buf now that we don't need it anymore. */
    free(buf);

    free(backupstring);
    backupstring = NULL;

    /* Cancel any search, or just return with no previous search. */
    if (i == -1 || (i < 0 && last_search[0] == '\0') ||
	    (i == 0 && answer[0] == '\0')) {
	statusbar(_("Cancelled"));
	return -1;
    } else {
	switch (i) {
	    case -2:		/* It's an empty string. */
	    case 0:		/* It's a new string. */
#ifdef HAVE_REGEX_H
		/* Use last_search if answer is an empty string, or
		 * answer if it isn't. */
		if (ISSET(USE_REGEXP) &&
			regexp_init((i == -2) ? last_search :
			answer) == 0)
		    return -1;
#endif
		break;
#ifndef NANO_TINY
	    case TOGGLE_CASE_KEY:
		TOGGLE(CASE_SENSITIVE);
		backupstring = mallocstrcpy(backupstring, answer);
		return 1;
	    case TOGGLE_BACKWARDS_KEY:
		TOGGLE(BACKWARDS_SEARCH);
		backupstring = mallocstrcpy(backupstring, answer);
		return 1;
#endif
#ifdef HAVE_REGEX_H
	    case NANO_REGEXP_KEY:
		TOGGLE(USE_REGEXP);
		backupstring = mallocstrcpy(backupstring, answer);
		return 1;
#endif
	    default:
		return -1;
	}
    }

    return 0;
}

/* Look for needle.  If no_sameline is TRUE, skip over selected when
 * looking for needle.  begin is the location of the filename where we
 * first started searching.  The return value specifies whether we found
 * anything. */
bool findnextfile(bool no_sameline, size_t begin, const char *needle)
{
    size_t currselected = selected;
	/* The location in the current file list of the match we
	 * find. */
    const char *filetail = tail(filelist[currselected]);
    const char *rev_start = filetail, *found = NULL;

#ifndef NANO_TINY
    if (ISSET(BACKWARDS_SEARCH))
	rev_start += strlen(rev_start);
#endif

    /* Look for needle in the current filename we're searching. */
    while (TRUE) {
	found = strstrwrapper(filetail, needle, rev_start);

	/* We've found a potential match.  If we're not allowed to find
	 * a match on the same filename we started on and this potential
	 * match is on that line, continue searching. */
	if (found != NULL && (!no_sameline || currselected != begin))
	    break;

	/* We've finished processing the filenames, so get out. */
	if (search_last_file) {
	    not_found_msg(needle);
	    return FALSE;
	}

	/* Move to the previous or next filename in the list.  If we've
	 * reached the start or end of the list, wrap around. */
#ifndef NANO_TINY
	if (ISSET(BACKWARDS_SEARCH)) {
	    if (currselected > 0)
		currselected--;
	    else {
		currselected = filelist_len - 1;
		statusbar(_("Search Wrapped"));
	    }
	} else {
#endif
	    if (currselected < filelist_len - 1)
		currselected++;
	    else {
		currselected = 0;
		statusbar(_("Search Wrapped"));
	    }
#ifndef NANO_TINY
	}
#endif

	/* We've reached the original starting file. */
	if (currselected == begin)
	    search_last_file = TRUE;

	filetail = tail(filelist[currselected]);

	rev_start = filetail;
#ifndef NANO_TINY
	if (ISSET(BACKWARDS_SEARCH))
	    rev_start += strlen(rev_start);
#endif
    }

    /* We've definitely found something. */
    selected = currselected;

    return TRUE;
}

/* Clear the flag indicating that a search reached the last file in the
 * list.  We need to do this just before a new search. */
void findnextfile_wrap_reset(void)
{
    search_last_file = FALSE;
}

/* Abort the current filename search.  Clean up by setting the current
 * shortcut list to the browser shortcut list, displaying it, and
 * decompiling the compiled regular expression we used in the last
 * search, if any. */
void filesearch_abort(void)
{
    currshortcut = browser_list;
    bottombars(browser_list);
#ifdef HAVE_REGEX_H
    regexp_cleanup();
#endif
}

/* Search for a filename. */
void do_filesearch(void)
{
    size_t begin = selected;
    int i;
    bool didfind;

    i = filesearch_init();
    if (i == -1)	/* Cancel, blank search string, or regcomp()
			 * failed. */
	filesearch_abort();
#if !defined(NANO_TINY) || defined(HAVE_REGEX_H)
    else if (i == 1)	/* Case Sensitive, Backwards, or Regexp search
			 * toggle. */
	do_filesearch();
#endif

    if (i != 0)
	return;

    /* If answer is now "", copy last_search into answer. */
    if (answer[0] == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

#ifndef NANO_TINY
    /* If answer is not "", add this search string to the search history
     * list. */
    if (answer[0] != '\0')
	update_history(&search_history, answer);
#endif

    findnextfile_wrap_reset();
    didfind = findnextfile(FALSE, begin, answer);

    /* Check to see if there's only one occurrence of the string and
     * we're on it now. */
    if (selected == begin && didfind) {
	/* Do the search again, skipping over the current line.  We
	 * should only end up back at the same position if the string
	 * isn't found again, in which case it's the only occurrence. */
	didfind = findnextfile(TRUE, begin, answer);
	if (selected == begin && !didfind)
	    statusbar(_("This is the only occurrence"));
    }

    filesearch_abort();
}

/* Search for the last filename without prompting. */
void do_fileresearch(void)
{
    size_t begin = selected;
    bool didfind;

    search_init_globals();

    if (last_search[0] != '\0') {
#ifdef HAVE_REGEX_H
	/* Since answer is "", use last_search! */
	if (ISSET(USE_REGEXP) && regexp_init(last_search) == 0)
	    return;
#endif

	findnextfile_wrap_reset();
	didfind = findnextfile(FALSE, begin, answer);

	/* Check to see if there's only one occurrence of the string and
	 * we're on it now. */
	if (selected == begin && didfind) {
	    /* Do the search again, skipping over the current line.  We
	     * should only end up back at the same position if the
	     * string isn't found again, in which case it's the only
	     * occurrence. */
	    didfind = findnextfile(TRUE, begin, answer);
	    if (selected == begin && !didfind)
		statusbar(_("This is the only occurrence"));
	}
    } else
        statusbar(_("No current search pattern"));

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
