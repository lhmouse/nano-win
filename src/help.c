/* $Id$ */
/**************************************************************************
 *   help.c                                                               *
 *                                                                        *
 *   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,  *
 *   2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.          *
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
#include <ctype.h>

#ifndef DISABLE_HELP

static char *help_text = NULL;
	/* The text displayed in the help window. */

/* Our main help browser function.  refresh_func is the function we will
 * call to refresh the edit window. */
void do_help(void (*refresh_func)(void))
{
    int kbinput = ERR;
    bool old_no_help = ISSET(NO_HELP);
    size_t line = 0;
	/* The line number in help_text of the first displayed help
	 * line.  This variable is zero-based. */
    size_t last_line = 0;
	/* The line number in help_text of the last help line.  This
	 * variable is zero-based. */
    int oldmenu = currmenu;
	/* The menu we were called from. */
    const char *ptr;
	/* The current line of the help text. */
    size_t old_line = (size_t)-1;
	/* The line we were on before the current line. */
    functionptrtype func;
	/* The function of the key the user typed in. */

    curs_set(0);
    blank_edit();
    blank_statusbar();

    /* Set help_text as the string to display. */
    help_init();

    assert(help_text != NULL);

    if (ISSET(NO_HELP)) {
	/* Make sure that the help screen's shortcut list will actually
	 * be displayed. */
	UNSET(NO_HELP);
	window_init();
    }

    bottombars(MHELP);
    wnoutrefresh(bottomwin);

    while (TRUE) {
	size_t i;

	/* Get the last line of the help text. */
	ptr = help_text;

	for (last_line = 0; *ptr != '\0'; last_line++) {
	    ptr += help_line_len(ptr);
	    if (*ptr == '\n')
		ptr++;
	}

	if (last_line > 0)
	    last_line--;

	/* Display the help text if we don't have a key, or if the help
	 * text has moved. */
	if (kbinput == ERR || line != old_line) {
	    blank_edit();

	    ptr = help_text;

	    /* Calculate where in the text we should be, based on the
	     * page. */
	    for (i = 0; i < line; i++) {
		ptr += help_line_len(ptr);
		if (*ptr == '\n')
		    ptr++;
	    }

	    for (i = 0; i < editwinrows && *ptr != '\0'; i++) {
		size_t j = help_line_len(ptr);

		mvwaddnstr(edit, i, 0, ptr, j);
		ptr += j;
		if (*ptr == '\n')
		    ptr++;
	    }
	}

	wnoutrefresh(edit);

	old_line = line;

	kbinput = get_kbinput(edit);

#ifndef NANO_TINY
	if (kbinput == KEY_WINCH) {
	    kbinput = ERR;
	    curs_set(0);
	    continue;
	}
#endif

#ifndef DISABLE_MOUSE
	if (kbinput == KEY_MOUSE) {
	    int mouse_x, mouse_y;
	    get_mouseinput(&mouse_x, &mouse_y, TRUE);
	    continue;    /* Redraw the screen. */
	}
#endif

	func = parse_help_input(&kbinput);

	if (func == total_refresh) {
	    total_redraw();
	} else if (func == do_page_up) {
	    if (line > editwinrows - 2)
		line -= editwinrows - 2;
	    else
		line = 0;
	} else if (func == do_page_down) {
	    if (line + (editwinrows - 1) < last_line)
		line += editwinrows - 2;
	} else if (func == do_up_void) {
	    if (line > 0)
		line--;
	} else if (func == do_down_void) {
	    if (line + (editwinrows - 1) < last_line)
		line++;
	} else if (func == do_first_line) {
	    line = 0;
	} else if (func == do_last_line) {
	    if (line + (editwinrows - 1) < last_line)
		line = last_line - (editwinrows - 1);
	} else if (func == do_exit) {
	    /* Exit from the help browser. */
	    break;
	}
    }

    if (old_no_help) {
	blank_bottombars();
	wnoutrefresh(bottomwin);
	currmenu = oldmenu;
	SET(NO_HELP);
	window_init();
    } else
	bottombars(oldmenu);

    curs_set(1);
    refresh_func();

    /* The help_init() at the beginning allocated help_text.  Since
     * help_text has now been written to the screen, we don't need it
     * anymore. */
    free(help_text);
    help_text = NULL;
}

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
    const subnfunc *f;
    const sc *s;
    int scsfound = 0;

#ifndef NANO_TINY
#ifndef DISABLE_NANORC
    bool old_whitespace = ISSET(WHITESPACE_DISPLAY);

    UNSET(WHITESPACE_DISPLAY);
#endif
#endif

    /* First, set up the initial help text for the current function. */
    if (currmenu == MWHEREIS || currmenu == MREPLACE || currmenu == MREPLACEWITH) {
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
    } else if (currmenu == MGOTOLINE) {
	htx[0] = N_("Go To Line Help Text\n\n "
		"Enter the line number that you wish to go to and hit "
		"Enter.  If there are fewer lines of text than the "
		"number you entered, you will be brought to the last "
		"line of the file.\n\n The following function keys are "
		"available in Go To Line mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    } else if (currmenu == MINSERTFILE) {
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
    } else if (currmenu == MWRITEFILE) {
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
    else if (currmenu == MBROWSER) {
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
    } else if (currmenu == MWHEREISFILE) {
	htx[0] = N_("Browser Search Command Help Text\n\n "
		"Enter the words or characters you would like to "
		"search for, and then press Enter.  If there is a "
		"match for the text you entered, the screen will be "
		"updated to the location of the nearest match for the "
		"search string.\n\n The previous search string will be "
		"shown in brackets after the search prompt.  Hitting "
		"Enter without entering any text will perform the "
		"previous search.\n\n");
	htx[1] = N_(" The following function keys are available in "
		"Browser Search mode:\n\n");
	htx[2] = NULL;
    } else if (currmenu == MGOTODIR) {
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
#endif /* !DISABLE_BROWSER */
#ifndef DISABLE_SPELLER
    else if (currmenu == MSPELL) {
	htx[0] = N_("Spell Check Help Text\n\n "
		"The spell checker checks the spelling of all text in "
		"the current file.  When an unknown word is "
		"encountered, it is highlighted and a replacement can "
		"be edited.  It will then prompt to replace every "
		"instance of the given misspelled word in the current "
		"file, or, if you have selected text with the mark, in "
		"the selected text.\n\n The following function keys "
		"are available in Spell Check mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#endif /* !DISABLE_SPELLER */
#ifndef NANO_TINY
    else if (currmenu == MEXTCMD) {
	htx[0] = N_("Execute Command Help Text\n\n "
		"This mode allows you to insert the output of a "
		"command run by the shell into the current buffer (or "
		"a new buffer in multiple file buffer mode).  If you "
		"need another blank buffer, do not enter any "
		"command.\n\n The following function keys are "
		"available in Execute Command mode:\n\n");
	htx[1] = NULL;
	htx[2] = NULL;
    }
#endif /* !NANO_TINY */
    else {
	/* Default to the main help list. */
	htx[0] = N_("Main nano help text\n\n "
		"The nano editor is designed to emulate the "
		"functionality and ease-of-use of the UW Pico text "
		"editor.  There are four main sections of the editor.  "
		"The top line shows the program version, the current "
		"filename being edited, and whether or not the file "
		"has been modified.  Next is the main editor window "
		"showing the file being edited.  The status line is "
		"the third line from the bottom and shows important "
		"messages.  ");
	htx[1] = N_("The bottom two lines show the most commonly used "
		"shortcuts in the editor.\n\n The notation for "
		"shortcuts is as follows: Control-key sequences are "
		"notated with a caret (^) symbol and can be entered "
		"either by using the Control (Ctrl) key or pressing "
		"the Escape (Esc) key twice.  Escape-key sequences are "
		"notated with the Meta (M-) symbol and can be entered "
		"using either the Esc, Alt, or Meta key depending on "
		"your keyboard setup.  ");
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

    /* Calculate the length of the shortcut help text.  Each entry has
     * one or two keys, which fill 16 columns, plus translated text,
     * plus one or two \n's. */
	for (f = allfuncs; f != NULL; f = f->next)
	    if (f->menus & currmenu)
		allocsize += (16 * mb_cur_max()) + strlen(f->help) + 2;

#ifndef NANO_TINY
    /* If we're on the main list, we also count the toggle help text.
     * Each entry has "M-%c\t\t", five chars which fill 16 columns,
     * plus a space, plus translated text, plus one or two '\n's. */
    if (currmenu == MMAIN) {
	size_t endis_len = strlen(_("enable/disable"));

	for (s = sclist; s != NULL; s = s->next)
	    if (s->scfunc == do_toggle_void)
		allocsize += strlen(_(flagtostr(s->toggle))) + endis_len + 8;
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

    /* Now add our shortcut info. */
    for (f = allfuncs; f != NULL; f = f->next) {

	if ((f->menus & currmenu) == 0)
	    continue;

	/* Let's simply show the first two shortcuts from the list. */
	for (s = sclist, scsfound = 0; s != NULL; s = s->next) {

	    if (s->type == RAWINPUT)
		continue;

	    if ((s->menus & currmenu) == 0)
		continue;

	    if (s->scfunc == f->scfunc) {
		scsfound++;
		/* Make the first column narrower (6) than the second (10),
		 * but allow it to spill into the second, for "M-Space". */
		if (scsfound == 1) {
		    sprintf(ptr, "%s              ", s->keystr);
		    ptr += 6;
		} else {
		    ptr += sprintf(ptr, "(%s)\t", s->keystr);
		    break;
		}
	    }
	}

	if (scsfound == 0)
	    ptr += sprintf(ptr, "\t\t");
	else if (scsfound == 1)
	    ptr += 10;

	/* The shortcut's help text. */
	ptr += sprintf(ptr, "%s\n", _(f->help));

	if (f->blank_after)
	    ptr += sprintf(ptr, "\n");
    }

#ifndef NANO_TINY
    /* And the toggles... */
    if (currmenu == MMAIN) {
	int maximum = 0, counter = 0;

	/* First see how many toggles there are. */
	for (s = sclist; s != NULL; s = s->next)
	   maximum = (s->toggle && s->ordinal > maximum) ? s->ordinal : maximum;

	/* Now show them in the original order. */
	while (counter < maximum) {
	    counter++;
	    for (s = sclist; s != NULL; s = s->next)
		if (s->toggle && s->ordinal == counter) {
		    ptr += sprintf(ptr, "%s\t\t%s %s\n", (s->menus == MMAIN ? s->keystr : ""),
				 _(flagtostr(s->toggle)), _("enable/disable"));
		    if (s->toggle == NO_COLOR_SYNTAX || s->toggle == TABS_TO_SPACES)
			ptr += sprintf(ptr, "\n");
		    break;
		}
	}
    }

#ifndef DISABLE_NANORC
    if (old_whitespace)
	SET(WHITESPACE_DISPLAY);
#endif
#endif /* !NANO_TINY */

    /* If all went well, we didn't overwrite the allocated space for
     * help_text. */
    assert(strlen(help_text) <= allocsize + 1);
}

/* Return the function that is bound to the given key, accepting certain
 * plain characters too, for consistency with the file browser. */
functionptrtype parse_help_input(int *kbinput)
{
    if (!meta_key) {
	switch (*kbinput) {
	    case ' ':
		return do_page_down;
	    case '-':
		return do_page_up;
	    case 'E':
	    case 'e':
		return do_exit;
	}
    }
    return func_from_key(kbinput);
}

/* Calculate the next line of help_text, starting at ptr. */
size_t help_line_len(const char *ptr)
{
    int help_cols = (COLS > 24) ? COLS - 1 : 24;

    /* Try to break the line at (COLS - 1) columns if we have more than
     * 24 columns, and at 24 columns otherwise. */
    ssize_t wrap_loc = break_line(ptr, help_cols, TRUE);
    size_t retval = (wrap_loc < 0) ? 0 : wrap_loc;
    size_t retval_save = retval;

    /* Get the length of the entire line up to a null or a newline. */
    while (*(ptr + retval) != '\0' && *(ptr + retval) != '\n')
	retval += move_mbright(ptr + retval, 0);

    /* If the entire line doesn't go more than one column beyond where
     * we tried to break it, we should display it as-is.  Otherwise, we
     * should display it only up to the break. */
    if (strnlenpt(ptr, retval) > help_cols + 1)
	retval = retval_save;

    return retval;
}

#endif /* !DISABLE_HELP */

/* Start the help browser. */
void do_help_void(void)
{
#ifndef DISABLE_HELP
    /* Start the help browser, with the correct refresher for afterwards. */
#ifndef DISABLE_BROWSER
    if (currmenu == MBROWSER || currmenu == MWHEREISFILE || currmenu == MGOTODIR)
	do_help(&browser_refresh);
    else
#endif
	do_help(&edit_refresh);
#else
    if (currmenu == MMAIN)
	say_there_is_no_help();
    else
	beep();
#endif /* !DISABLE_HELP */
}
