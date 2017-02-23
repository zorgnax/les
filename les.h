#ifndef __LES_H__
#define __LES_H__

#include <stdio.h>

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
    int loaded;
    int line;
    int nlines;
    int column;
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

extern int tty;
extern int line1;
extern tab_t *tabb;
extern int line_wrap;
extern int tab_width;
extern tline_t *tlines;
extern size_t tlines_len;
extern size_t tlines_size;
extern prompt_t *pr;
extern int prompt_done;

/* les.c */
void draw_tab ();
void stage_tab2 (int n, tline_t *tlines, size_t tlines_len);
void draw_status ();

/* charinfo.c */
void shorten (char *str, int n);
int strwidth (const char *str);
int strnwidth (const char *str, size_t len);
void get_char_info (charinfo_t *cinfo, const char *buf);

/* prompt.c */
void search ();
void draw_prompt ();

/* linewrap.c */
void get_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size);
int prev_line (char *buf, size_t len, size_t pos, int n);

/* movement.c */
void move_forward (int n);
void move_backward (int n);
void move_start ();
void move_end ();
void move_left (int n);
void move_right (int n);
void move_line_left ();
void move_line_right ();

/* stage.c */
void stage_init ();
void stage_cat (const char *str);
void stage_ncat (const char *str, size_t n);
void stage_write ();

#endif

