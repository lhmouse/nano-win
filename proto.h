/* $Id$ */
/**************************************************************************
 *   proto.h                                                              *
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

/* Externs */

#include <sys/stat.h>

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#include "nano.h"

extern int wrap_at;
extern int editwinrows;
extern int current_x, current_y, totlines;
extern int placewewant;
extern int mark_beginx;
extern long totsize;
extern int temp_opt;
extern int wrap_at, flags, tabsize;
extern int search_last_line;
extern int currslen;

#ifndef DISABLE_JUSTIFY
extern char *quotestr;
#endif

extern WINDOW *edit, *topwin, *bottomwin;
extern char *filename;
extern struct stat originalfilestat;
extern char *answer;
extern char *hblank;
#ifndef DISABLE_HELP
extern char *help_text;
#endif
extern char *last_search;
extern char *last_replace;
#ifndef DISABLE_OPERATINGDIR
extern char *operating_dir;
extern char *full_operating_dir;
#endif
#ifndef DISABLE_SPELLER
extern  char *alt_speller;
#endif
#ifndef DISABLE_TABCOMP
char *real_dir_from_tilde(char *buf);
#endif
#ifndef DISABLE_BROWSER
char *do_browse_from(char *inpath);
#endif

extern struct stat fileinfo;
extern filestruct *current, *fileage, *edittop, *editbot, *filebot; 
extern filestruct *cutbuffer, *mark_beginbuf;

#ifdef ENABLE_MULTIBUFFER
extern openfilestruct *open_files;
#endif

#ifdef ENABLE_COLOR
extern colortype *colorstrings;
extern syntaxtype *syntaxes;
extern char *syntaxstr;
extern regex_t color_regexp;
extern regmatch_t colormatches[1];
#endif

extern shortcut *shortcut_list;
extern shortcut *main_list, *whereis_list;
extern shortcut *replace_list, *goto_list;
extern shortcut *writefile_list, *insertfile_list;
extern shortcut *spell_list, *replace_list_2;
#ifndef NANO_SMALL
extern shortcut *extcmd_list;
#endif
extern shortcut *help_list;
#ifndef DISABLE_BROWSER
extern shortcut *browser_list, *gotodir_list;
#endif
extern const shortcut *currshortcut;

#ifdef HAVE_REGEX_H
extern int use_regexp, regexp_compiled;
extern regex_t search_regexp;
extern regmatch_t regmatches[10];  
#ifdef ENABLE_COLOR
extern regex_t syntaxfile_regexp;
extern regmatch_t synfilematches[1];  
#endif /* ENABLE_COLOR */
#endif /* HAVE_REGEX_H */

#ifndef NANO_SMALL
extern toggle *toggles;
#endif

/* Functions we want available */

/* Public functions in color.c */
#ifdef ENABLE_COLOR
void set_colorpairs(void);
void do_colorinit(void);
void update_color(void);
#endif /* ENABLE_COLOR */

/* Public functions in cut.c */
int do_cut_text(void);
int do_uncut_text(void);
filestruct *get_cutbottom(void);
void add_to_cutbuffer(filestruct *inptr);
void cut_marked_segment(filestruct *top, size_t top_x, filestruct *bot,
                        size_t bot_x, int destructive);

/* Public functions in files.c */
int write_file(char *name, int tmpfile, int append, int nonamechange);
int open_file(const char *filename, int insert, int quiet);
int read_file(FILE *f, const char *filename, int quiet);
#ifdef ENABLE_MULTIBUFFER
openfilestruct *make_new_opennode(openfilestruct *prevnode);
void splice_opennode(openfilestruct *begin, openfilestruct *newnode, openfilestruct *end);
void unlink_opennode(const openfilestruct *fileptr);
void delete_opennode(openfilestruct *fileptr);
void free_openfilestruct(openfilestruct *src);
int add_open_file(int update);
int close_open_file(void);
int open_prevfile_void(void);
int open_nextfile_void(void);
#endif
#ifndef DISABLE_OPERATINGDIR
int check_operating_dir(char *currpath, int allow_tabcomp);
#endif
int do_writeout(char *path, int exiting, int append);
char *input_tab(char *buf, int place, int *lastwastab, int *newplace, int *list);
void new_file(void);
int do_writeout_void(void);
int do_insertfile_void(void);
char *get_next_filename(const char *name);
#ifndef DISABLE_SPELLER
char *safe_tempnam(const char *dirname, const char *filename_prefix);
#endif

/* Public functions in global.c */
int length_of_list(const shortcut *s);
void shortcut_init(int unjustify);
#ifdef DEBUG
void thanks_for_all_the_fish(void);
#endif

/* Public functions in move.c */
int do_first_line(void);
int do_last_line(void);
size_t xplustabs(void);
size_t actual_x(const filestruct *fileptr, size_t xplus);
size_t strnlenpt(const char *buf, size_t size);
size_t strlenpt(const char *buf);
void reset_cursor(void);
void blank_bottombars(void);
void blank_edit(void);
void blank_statusbar(void);
void blank_statusbar_refresh(void);
void check_statblank(void);
void titlebar(const char *path);
void bottombars(const shortcut *s);
void set_modified(void);
int do_up(void);
int do_down(void);
int do_left(void);
int do_right(void);
void page_up(void);
int do_page_up(void);
int do_page_down(void);
int do_home(void);
int do_end(void);

/* Public functions in nano.c */
void renumber(filestruct *fileptr);
void free_filestruct(filestruct *src);
int no_help(void);
void renumber_all(void);
int open_pipe(const char *command);
int do_prev_word(void);
int do_next_word(void);
void delete_node(filestruct *fileptr);
void wrap_reset(void);
void do_early_abort(void);
void die(const char *msg, ...);
void splice_node(filestruct *begin, filestruct *newnode, filestruct *end);
void nano_disabled_msg(void);
void window_init(void);
void do_mouse(void);
void print_view_warning(void);
int do_exit(void);
int do_spell(void);
int do_mark(void);
int do_delete(void);
int do_backspace(void);
int do_tab(void);
int do_justify(void);
int do_enter(void);
int do_wrap(filestruct *inptr);
void signal_init(void);
void handle_sigwinch(int s);
void die_save_file(const char *die_filename);
size_t indent_length(const char *line);

filestruct *copy_node(const filestruct *src);
filestruct *copy_filestruct(const filestruct *src);
filestruct *make_new_node(filestruct *prevnode);
#ifndef DISABLE_HELP
void help_init(void);
#endif

/* Public functions in rcfile.c */
#ifdef ENABLE_NANORC
void do_rcfile(void);
#endif

/* Public functions in search.c */
int do_gotoline(int line, int save_pos);
int is_whole_word(int curr_pos, const char *datastr, const char *searchword);
int do_replace_loop(const char *prevanswer, const filestruct *begin,
			int *beginx, int wholewords, int *i);
int do_find_bracket(void);
#if defined (ENABLE_MULTIBUFFER) || !defined (DISABLE_SPELLER)
void do_gotopos(int line, int pos_x, int pos_y, int pos_placewewant);
#endif
void search_init_globals(void);
void replace_abort(void);
int do_gotoline_void(void);
int do_search(void);
int do_replace(void);
filestruct *findnextstr(int quiet, int bracket_mode, const filestruct *begin,
		int beginx, const char *needle);

/* Public functions in utils.c */
const char *stristr(const char *haystack, const char *needle);
const char *strstrwrapper(const char *haystack, const char *needle,
		const char *rev_start, int line_pos);
int is_cntrl_char(int c);
int num_of_digits(int n);
int check_wildcard_match(const char *text, const char *pattern);
void align(char **strp);
void null_at(char **data, size_t index);
void unsunder(char *str, size_t true_len);
void sunder(char *str);
void nperror(const char *s);
char *mallocstrcpy(char *dest, const char *src);
void *nmalloc(size_t howmuch);
void *nrealloc(void *ptr, size_t howmuch);
void new_magicline(void);
char *charalloc(size_t howmuch);

/* Public functions in winio.c */
int do_yesno(int all, int leavecursor, const char *msg, ...);
int statusq(int allowtabs, const shortcut *s, const char *def,
		const char *msg, ...);
void do_replace_highlight(int highlight_flag, const char *word);
void edit_refresh(void);
void edit_refresh_clearok(void);
void edit_update(filestruct *fileptr, topmidbotnone location);
void update_cursor(void);
#ifdef DEBUG
void dump_buffer(const filestruct *inptr);
void dump_buffer_reverse(void);
#endif
void update_line(filestruct *fileptr, int index);
void fix_editbot(void);
void statusbar(const char *msg, ...);
void center_cursor(void);
void display_main_list(void);
#ifdef NANO_EXTRA
void do_credits(void);
#endif
int do_cursorpos(int constant);
int do_cursorpos_void(void);
int total_refresh(void);
int do_help(void);
int keypad_on(WINDOW *win, int newval);
