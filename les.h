// Copyright 2017 Jacob Gelbman <gelbman@gmail.com>
// This file is licensed under the LGPL

#ifndef __LES_H__
#define __LES_H__

#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include "defines.h"

typedef struct {
    int len;
    int mask;
    int error;
    unsigned int codepoint;
    int width;
} charinfo_t;

typedef struct {
    unsigned int from;
    unsigned int to;
    int width;
} width_range_t;

typedef struct {
    size_t start;
    size_t end;
} match_t;

typedef struct {
    const char *name;
    int name_width;
    char *name2;
    int fd;
    char *buf;
    size_t buf_size;
    size_t buf_len;
    char *stragglers;
    size_t stragglers_len;
    size_t pos;
    int state;
    int line;
    int nlines;
    int column;
    char *mesg;
    size_t mesg_size;
    size_t mesg_len;
    size_t mark;
    time_t opened;
    char *realpath;
    int last_line;
    int search_version;
    match_t *matches;
    size_t matches_len;
    size_t matches_size;
    size_t current_match;
} tab_t;

typedef struct {
    size_t pos;
    size_t end_pos;
} tline_t;

typedef struct {
    char *buf;
    size_t len;
    size_t size;
    const char *prompt;
    size_t prompt_len;
    int nlines;
    int nlines2;
    size_t cursor;
    char **history;
    size_t history_len;
    size_t history_size;
    size_t history_skip;
    size_t history_new;
    char *hcstr;
    size_t hcstr_len;
    size_t hcstr_size;
} prompt_t;

#define UTF8_LENGTH(c)        \
    ((c & 0x80) == 0x00 ? 1 : \
     (c & 0xc0) == 0x80 ? 1 : \
     (c & 0xe0) == 0xc0 ? 2 : \
     (c & 0xf0) == 0xe0 ? 3 : \
     (c & 0xf8) == 0xf0 ? 4 : \
     (c & 0xfc) == 0xf8 ? 5 : \
     (c & 0xfe) == 0xfc ? 6 : \
                          6)

#define READY       0
#define OPENED      1
#define LOADED      2
#define MARKED      4
#define RECENTS     8
#define HELP        16
#define ERROR       32
#define POSITIONED  64
#define FILEBACKED  128
#define LOADFOREVER 256
#define SPECIAL     512

extern int tty;
extern int line1;
extern int line_wrap;
extern int tab_width;
extern prompt_t *pr;
extern int interrupt;
extern size_t tabs_len;
extern tab_t **tabs;
extern int current_tab;
extern tab_t *tabb;
extern char *lespipe;
extern int active_search;
extern int search_version;
extern int man_page;

extern tline_t *tlines;
extern size_t tlines_len;
extern size_t tlines_size;
extern tline_t *tlines2;
extern size_t tlines2_len;
extern size_t tlines2_size;
extern tline_t *tlines3;
extern size_t tlines3_len;
extern size_t tlines3_size;

// main.c
int read_key (char *buf, int len);

// charinfo.c
void shorten (char *str, int n);
int strwidth (const char *str);
int strnwidth (const char *str, size_t len);
void get_char_info (charinfo_t *cinfo, const char *buf, int i);

// prompt.c
void draw_prompt ();
char *gets_prompt (prompt_t *ppr);
prompt_t *init_prompt (const char *prompt);
void alert (char *fmt, ...);
void add_history (prompt_t *pr, char *str, size_t len);

// linewrap.c
void get_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size);
void get_wrap_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size);
void get_nowrap_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size);
int prev_line (char *buf, size_t len, size_t pos, int n);
int next_line (char *buf, size_t len, size_t pos, int n);
void get_tlines_backward (char *buf, size_t len, size_t pos, int max);
int find_pos_backward (int max, size_t pos2, size_t *pos3);
int find_pos_forward (int max, size_t pos2, size_t *pos3);
size_t line_before_pos (size_t pos, int n);

// movement.c
void move_forward (int n);
void move_backward (int n);
void move_start ();
void move_end ();
void move_left (int n);
void move_right (int n);
void move_line_left ();
void move_line_right ();
void move_to_pos (size_t pos);
void move_to_line (int line);

// stage.c
void stage_init ();
void stage_cat (const char *str);
void stage_ncat (const char *str, size_t n);
void stage_vprintf (const char *fmt, va_list args);
void stage_printf (const char *fmt, ...);
void stage_write ();

// page.c
void draw_tab ();
void stage_tab2 (int n, tline_t *tlines, size_t tlines_len);
void draw_status ();
void init_page ();
void set_ttybuf (charinfo_t *cinfo, char *buf, int len);

// tabs.c
void stage_tabs ();
void close_tab ();
void next_tab ();
void prev_tab ();
void add_tab (const char *name, int fd, int state);
void change_tab ();
void init_line1 ();

// readfile.c
void read_file ();
void set_input_encoding (char *encoding);
void open_tab_file ();
int count_lines (char *buf, size_t len);
int count_lines_atob (char *buf, size_t a, size_t b);
void reload ();
void set_man_page_name ();

// recentfiles.c
void add_recent_tab (tab_t *tabb);
void save_recents_file ();
void add_recents_tab ();
void load_recents_file ();
void get_last_line ();

// search.c
void search ();
void next_match ();
void prev_match ();
void load_search_history ();
void save_search_history ();

#endif

