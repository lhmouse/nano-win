/* $Id$ */
/**************************************************************************
 *   rcfile.c                                                             *
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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "config.h"
#include "proto.h"
#include "nano.h"

#ifdef ENABLE_NANORC

#ifndef NANO_SMALL
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#define NUM_RCOPTS 14
/* Static stuff for the nanorc file */
rcoption rcopts[NUM_RCOPTS] = 
{
{"regexp", USE_REGEXP},
{"const", CONSTUPDATE},
{"autoindent", AUTOINDENT},
{"cut", CUT_TO_END},
{"nofollow", FOLLOW_SYMLINKS},
{"mouse", USE_MOUSE},
{"pico", PICO_MODE},

#ifndef DISABLE_WRAPJUSTIFY
{"fill", 0},
#endif

{"speller", 0},
{"tempfile", TEMP_OPT},
{"view", VIEW_MODE},
{"nowrap", NO_WRAP}, 
{"nohelp", NO_HELP}, 
{"suspend", SUSPEND}};

/* We have an error in some part of the rcfile; put it on stderr and
  make the user hit return to continue starting up nano */
void rcfile_error(char *msg, ...)
{
    va_list ap;

    fprintf(stderr, "\n");
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, _("\nPress return to continue starting nano\n"));

    while (getchar() != '\n')
	;

}

/* Just print the error (one of many, perhaps) but don't abort, yet */
void rcfile_msg(int *errors, char *msg, ...)
{
    va_list ap;

    if (!*errors) {
	*errors = 1;
	fprintf(stderr, "\n");
    }
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");

}

/* Parse the next word from the string.  Returns NULL if we hit EOL */
char *parse_next_word(char *ptr)
{
    while (*ptr != ' ' && *ptr != '\n' && ptr != '\0')
	ptr++;

    if (*ptr == '\0')
	return NULL;

    /* Null terminate and advance ptr */
    *ptr++ = 0;

    return ptr;
}

/* Parse the RC file, once it has been opened successfully */
void parse_rcfile(FILE *rcstream, char *filename)
{
    char *buf, *ptr, *keyword, *option;
    int set = 0, lineno = 0, i;
    int errors = 0;

    buf = charalloc(1024);
    while (fgets(buf, 1023, rcstream) > 0) {
	lineno++;
	ptr = buf;
	while ((*ptr == ' ' || *ptr == '\t') && 
		(*ptr != '\n' && *ptr != '\0'))
	    ptr++;

	if (*ptr == '\n' || *ptr == '\0')
	    continue;

	if (*ptr == '#') {
#ifdef DEBUG
	    fprintf(stderr, _("parse_rcfile: Read a comment\n"));
#endif
	    continue;	/* Skip past commented lines */
	}

	/* Else skip to the next space */
	keyword = ptr;
	ptr = parse_next_word(ptr);
	if (!ptr)
	    continue;

	/* Else try to parse the keyword */
	if (!strcasecmp(keyword, "set"))
	    set = 1;
	else if (!strcasecmp(keyword, "unset"))
	    set = -1;
	else {
	    rcfile_msg(&errors, _("Error in %s on line %d: command %s not understood"),
		filename, lineno, keyword);
	    continue;
	}

	option = ptr;
	ptr = parse_next_word(ptr);
	/* We don't care if ptr == NULL, as it should if using proper syntax */

	if (set != 0) {
	    for (i = 0; i <= NUM_RCOPTS - 1; i++) {
	        if (!strcasecmp(option, rcopts[i].name)) {
#ifdef DEBUG
		    fprintf(stderr, _("parse_rcfile: Parsing option %s\n"), 
				rcopts[i].name);
#endif
		    if (set == 1 || rcopts[i].flag == FOLLOW_SYMLINKS) {
			if (
#ifndef DISABLE_WRAPJUSTIFY
			    !strcasecmp(rcopts[i].name, "fill") || 
#endif
#ifndef DISABLE_SPELLER
			    !strcasecmp(rcopts[i].name, "speller")
#else
				0
#endif
			   ) {

			    if (*ptr == '\n' || *ptr == '\0') {
	    			rcfile_msg(&errors, _("Error in %s on line %d: option %s requires an argument"),
						filename, lineno, rcopts[i].name);
				continue;
			    }
			    option = ptr;
			    ptr = parse_next_word(ptr);
			    if (!strcasecmp(rcopts[i].name, "fill")) {
#ifndef DISABLE_WRAPJUSTIFY

				if ((i = atoi(option)) < MIN_FILL_LENGTH) {
	    		 	    rcfile_msg(&errors, 
		_("Error in %s on line %d: requested fill size %d too small"),
						filename, lineno, option);
				}
				else
				     fill = i;
#endif
			    } 
			    else {
#ifndef DISABLE_SPELLER
				alt_speller = charalloc(strlen(option) + 1);
            			strcpy(alt_speller, option);
#endif
			    }
			} else 
			    SET(rcopts[i].flag);
#ifdef DEBUG
			fprintf(stderr, _("set flag %d!\n"), rcopts[i].flag);
#endif
		    } else {
			UNSET(rcopts[i].flag);
#ifdef DEBUG
			fprintf(stderr, _("unset flag %d!\n"), rcopts[i].flag);
#endif
		   }			
		}
	    }
	}

    }
    if (errors)
	rcfile_error(_("Errors found in .nanorc file"));

    return;
}

/* The main rc file function, tries to open the rc file */
void do_rcfile(void)
{
    char *nanorc;
    char *unable = _("Unable to open ~/.nanorc file, %s");
    struct stat fileinfo;
    FILE *rcstream;

    if (getenv("HOME") == NULL)
	return;

    nanorc = charalloc(strlen(getenv("HOME")) + 10);
    sprintf(nanorc, "%s/.nanorc", getenv("HOME"));

    if (stat(nanorc, &fileinfo) == -1) {

	/* Abort if the file doesn't exist and there's some other kind
	   of error stat()ing it */
	if (errno != ENOENT)
	    rcfile_error(unable, errno);
	return;
   }

    if ((rcstream = fopen(nanorc, "r")) == NULL) {
	rcfile_error(unable, strerror(errno));
	return;
    }

    parse_rcfile(rcstream, nanorc);
    fclose(rcstream);

}


#endif /* ENABLE_NANORC */

