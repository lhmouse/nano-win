/**************************************************************************
 *   proto.h                                                              *
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

/* Externs */

#include <sys/stat.h>
#include <regex.h>
#include "nano.h"

extern int center_x, center_y, editwinrows;
extern int current_x, current_y, posible_max, totlines;
extern int placewewant;
extern int mark_beginx, samelinewrap;
extern int totsize, temp_opt;
extern int fill, flags;

extern WINDOW *edit, *topwin, *bottomwin;
extern char *filename, *answer, *last_search, *last_replace;
extern char *hblank, *help_text;
extern struct stat fileinfo;
extern filestruct *current, *fileage, *edittop, *editbot, *filebot; 
extern filestruct *cutbuffer, *mark_beginbuf;
extern shortcut *shortcut_list;
extern shortcut main_list[MAIN_LIST_LEN], whereis_list[WHEREIS_LIST_LEN];
extern shortcut replace_list[REPLACE_LIST_LEN], goto_list[GOTO_LIST_LEN];
extern shortcut writefile_list[WRITEFILE_LIST_LEN], help_list[HELP_LIST_LEN];
extern shortcut spell_list[SPELL_LIST_LEN];
extern int use_regexp, regexp_compiled;
extern regex_t search_regexp;
extern regmatch_t regmatches[10];  

/* Programs we want available */

char *strcasestr(char *haystack, char *needle);
char *strstrwrapper(char *haystack, char *needle);
int search_init(int replacing);
int renumber(filestruct * fileptr);
int free_filestruct(filestruct * src);
int xplustabs(void);
int do_yesno(int all, int leavecursor, char *msg, ...);
int actual_x(filestruct * fileptr, int xplus);
int strlenpt(char *buf);
int statusq(shortcut s[], int slen, char *def, char *msg, ...);
int write_file(char *name, int tmpfile);
int do_cut_text(void);
int do_uncut_text(void);
int no_help(void);
int renumber_all(void);
int open_file(char *filename, int insert, int quiet);
int do_writeout(int exiting);
int do_gotoline(long defline);
/* Now in move.c */
int do_up(void);
int do_down(void);
int do_left(void);
int do_right(void);


void shortcut_init(void);
void lowercase(char *src);
void blank_bottombars(void);
void check_wrap(filestruct * inptr, char ch);
void dump_buffer(filestruct * inptr);
void align(char **strp);
void edit_refresh(void);
void edit_update(filestruct * fileptr);
void edit_update_top(filestruct * fileptr);
void edit_update_bot(filestruct * fileptr);
void update_cursor(void);
void delete_node(filestruct * fileptr);
void set_modified(void);
void dump_buffer_reverse(filestruct * inptr);
void reset_cursor(void);
void check_statblank(void);
void update_line(filestruct * fileptr, int index);
void fix_editbot(void);
void statusbar(char *msg, ...);
void titlebar(void);
void previous_line(void);
void center_cursor(void);
void bottombars(shortcut s[], int slen);
void blank_statusbar_refresh(void);
void *nmalloc (size_t howmuch);
void wrap_reset(void);
void display_main_list(void);
void nano_small_msg(void);
void do_early_abort(void);
void *nmalloc(size_t howmuch);
void *nrealloc(void *ptr, size_t howmuch);
void die(char *msg, ...);
void new_file(void);
void new_magicline(void);
void splice_node(filestruct *begin, filestruct *new, filestruct *end);
void null_at(char *data, int index)

int do_writeout_void(void), do_exit(void), do_gotoline_void(void);
int do_insertfile(void), do_search(void), page_up(void), page_down(void);
int do_cursorpos(void), do_spell(void);
int do_up(void), do_down (void), do_right(void), do_left (void);
int do_home(void), do_end(void), total_refresh(void), do_mark(void);
int do_delete(void), do_backspace(void), do_tab(void), do_justify(void);
int do_first_line(void), do_last_line(void);
int do_replace(void), do_help(void), do_enter_void(void);

filestruct *copy_node(filestruct * src);
filestruct *copy_filestruct(filestruct * src);
filestruct *make_new_node(filestruct * prevnode);
