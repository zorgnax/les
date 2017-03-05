// Copyright 2017 Jacob Gelbman <gelbman@gmail.com>
// This file is licensed under the LGPL

#ifndef __LES_H__
#define __LES_H__

#include <stdio.h>
#include <time.h>
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

#define READY      0
#define OPENED     1
#define LOADED     2
#define MARKED     4
#define RECENTS    8
#define HELP       16
#define ERROR      32
#define POSITIONED 64

extern int tty;
extern int line1;
extern tab_t *tabb;
extern int line_wrap;
extern int tab_width;
extern tline_t *tlines;
extern size_t tlines_len;
extern size_t tlines_size;
extern prompt_t *pr;
extern int interrupt;
extern size_t tabs_len;
extern tab_t **tabs;
extern char *lespipe;

// main.c
int read_key (char *buf, int len);

// charinfo.c
void shorten (char *str, int n);
int strwidth (const char *str);
int strnwidth (const char *str, size_t len);
void get_char_info (charinfo_t *cinfo, const char *buf, int i);

// prompt.c
void search ();
void draw_prompt ();
char *gets_prompt (prompt_t *ppr);
prompt_t *init_prompt (const char *prompt);
void alert (char *mesg);

// linewrap.c
void get_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size);
void get_wrap_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size);
void get_nowrap_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size);
int prev_line (char *buf, size_t len, size_t pos, int n);
int next_line (char *buf, size_t len, size_t pos, int n);

// movement.c
void move_forward (int n);
void move_backward (int n);
void move_start ();
void move_end ();
void move_left (int n);
void move_right (int n);
void move_line_left ();
void move_line_right ();
void move_pos (size_t pos);
void move_to_line (int line);

// stage.c
void stage_init ();
void stage_cat (const char *str);
void stage_ncat (const char *str, size_t n);
void stage_write ();

// page.c
void draw_tab ();
void stage_tab2 (int n, tline_t *tlines, size_t tlines_len);
void draw_status ();
void init_status ();
void set_ttybuf (charinfo_t *cinfo, char *buf, int len);

// tabs.c
void stage_tabs ();
void close_tab ();
void next_tab ();
void prev_tab ();
void add_tab (const char *name, int fd, int state);
void select_tab (int t);
void init_line1 ();

// readfile.c
void read_file ();
void set_input_encoding (char *encoding);
void open_tab_file ();
int count_lines (char *buf, size_t len);

// recentfiles.c
void add_recent_tab (tab_t *tabb);
void save_recents_file ();
void add_recents_tab ();
void load_recents_file ();
void get_last_line ();

#endif

