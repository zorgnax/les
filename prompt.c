#include "les.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <term.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

prompt_t *pr = NULL;
int prompt_done = 0;

void stage_prompt_line (tline_t *tline) {
    charinfo_t cinfo;
    int i;
    int width = 0;
    for (i = tline->pos; i < tline->end_pos;) {
        get_char_info(&cinfo, pr->buf, i);
        if (pr->buf[i] == '\r' || pr->buf[i] == '\n') {
            break;
        }
        else if (pr->buf[i] == '\t') {
            int j;
            for (j = 0; j < tab_width; j++) {
                stage_cat(" ");
            }
        }
        else {
            stage_ncat(pr->buf + i, cinfo.len);
        }
        width += cinfo.width;
        i += cinfo.len;
    }
    if (width < columns) {
        stage_cat(clr_eol);
    }
}

void draw_prompt () {
    stage_cat(tparm(cursor_address, lines - pr->nlines, 0));
    tlines2_len = 0;
    get_wrap_tlines(pr->buf, pr->len, 0, 0, &tlines2, &tlines2_len, &tlines2_size);
    if (tlines_len < pr->nlines2) {
        stage_cat(clr_eos);
    }
    pr->nlines2 = tlines2_len;
    if (tlines2_len > pr->nlines) {
        pr->nlines = tlines2_len;
    }
    int i;
    for (i = 0; i < tlines2_len; i++) {
        stage_prompt_line(tlines2 + i);
        if (i != tlines2_len - 1) {
            stage_cat("\n");
        }
    }
    int cursory = 0;
    int cursorx = 0;
    for (i = 0; i < tlines2_len; i++) {
        if (pr->cursor <= tlines2[i].end_pos) {
            cursory = i;
            cursorx = strnwidth(pr->buf + tlines2[i].pos, pr->cursor - tlines2[i].pos);
            if (cursorx >= columns) {
                cursory++;
                cursorx = 0;
            }
            break;
        }
    }
    if (cursory + 1 > pr->nlines) {
        pr->nlines++;
        stage_cat("\n");
    }
    stage_cat(tparm(cursor_address, lines - pr->nlines + cursory, cursorx));
    stage_write();
}

void prompt_left () {
    if (pr->cursor == pr->prompt_len) {
        return;
    }
    int i = pr->cursor;
    while (i--) {
        if ((pr->buf[i] & 0xc0) != 0x80) {
            break;
        }
    }
    pr->cursor = i;
}

void prompt_right () {
    if (pr->cursor == pr->len) {
        return;
    }
    pr->cursor += UTF8_LENGTH(pr->buf[pr->cursor]);
}

void prompt_left_word () {
    if (pr->cursor == pr->prompt_len) {
        return;
    }
    int i, j, whitespace1, whitespace2;
    for (i = pr->cursor - 1; (pr->buf[i] & 0xc0) == 0x80; i--)
        ;
    for (; i > pr->prompt_len;) {
        j = i - 1;
        while ((pr->buf[j] & 0xc0) == 0x80) {
            j--;
        }
        whitespace1 = pr->buf[i] == ' ' || pr->buf[i] == '\t';
        whitespace2 = pr->buf[j] == ' ' || pr->buf[j] == '\t';
        if (!whitespace1 && whitespace2) {
            break;
        }
        i = j;
    }
    pr->cursor = i;
}

void prompt_right_word () {
    if (pr->cursor == pr->len) {
        return;
    }
    int i, j, whitespace1, whitespace2;
    for (i = pr->cursor; i < pr->len;) {
        j = i + UTF8_LENGTH(pr->buf[i]);
        if (j == pr->len) {
            break;
        }
        whitespace1 = pr->buf[i] == ' ' || pr->buf[i] == '\t';
        whitespace2 = pr->buf[j] == ' ' || pr->buf[j] == '\t';
        if (!whitespace1 && whitespace2) {
            break;
        }
        i = j;
    }
    pr->cursor = j;
}

void prompt_backspace () {
    size_t cursor1 = pr->cursor;
    prompt_left();
    int len = cursor1 - pr->cursor;
    if (!len) {
        return;
    }
    int i;
    for (i = cursor1; i < pr->len; i++) {
        pr->buf[i - len] = pr->buf[i];
    }
    pr->len -= len;
}

void prompt_backspace_word () {
    size_t cursor1 = pr->cursor;
    prompt_left_word();
    int len = cursor1 - pr->cursor;
    if (!len) {
        return;
    }
    int i;
    for (i = cursor1; i < pr->len; i++) {
        pr->buf[i - len] = pr->buf[i];
    }
    pr->len -= len;
}

void prompt_delete () {
    size_t cursor1 = pr->cursor;
    prompt_right();
    int len = pr->cursor - cursor1;
    if (!len) {
        return;
    }
    int i;
    for (i = pr->cursor; i < pr->len; i++) {
        pr->buf[i - len] = pr->buf[i];
    }
    pr->len -= len;
    pr->cursor -= len;
}

void prompt_delete_word () {
    size_t cursor1 = pr->cursor;
    prompt_right_word();
    int len = pr->cursor - cursor1;
    if (!len) {
        return;
    }
    int i;
    for (i = pr->cursor; i < pr->len; i++) {
        pr->buf[i - len] = pr->buf[i];
    }
    pr->len -= len;
    pr->cursor -= len;
}

void prompt_kill_forward () {
    pr->len = pr->cursor;
}

void prompt_kill_backward () {
    int i;
    int len = pr->cursor - pr->prompt_len;
    for (i = pr->cursor; i < pr->len; i++) {
        pr->buf[i - len] = pr->buf[i];
    }
    pr->len -= len;
    pr->cursor -= len;
}

void addc_prompt (char *buf, int len) {
    int i;
    if (pr->len + len >= pr->size) {
        pr->size *= 2;
        pr->buf = realloc(pr->buf, pr->size);
    }
    for (i = pr->len - 1; i >= pr->cursor; i--) {
        pr->buf[i + len] = pr->buf[i];
    }
    for (i = 0; i < len; i++) {
        pr->buf[pr->cursor + i] = buf[i];
    }
    pr->len += len;
    pr->cursor += len;
}

int getc_prompt (char *buf, int len) {
    unsigned char c = buf[0];
    if ((c > 0x1f && c < 0x7f) || (c > 0x7f)) {
        int len2 = UTF8_LENGTH(c);
        addc_prompt(buf, len2);
        return len2;
    }
    if (buf[0] == '\n') {
        prompt_done = 1;
    }
    else if (buf[0] == '\e' && len == 1) {
        interrupt = 1;
    }
    else if (strncmp(buf, "\eOD", 3) == 0) { // left
        prompt_left();
    }
    else if (strncmp(buf, "\eb", 2) == 0) { // alt-left
        prompt_left_word();
    }
    else if (strncmp(buf, "\eOC", 3) == 0) { // right
        prompt_right();
    }
    else if (strncmp(buf, "\ef", 2) == 0) { // alt-right
        prompt_right_word();
    }
    else if (strncmp(buf, "\x7f", 1) == 0) { // backspace
        prompt_backspace();
    }
    else if (strncmp(buf, "\e\x7f", 2) == 0) { // alt-backspace
        prompt_backspace_word();
    }
    else if (strncmp(buf, "\e[3~", 4) == 0) { // delete
        prompt_delete();
    }
    else if (strncmp(buf, "\e(", 2) == 0) { // alt-delete
        prompt_delete_word();
    }
    else if (buf[0] == -0x40 + 'K') {
        prompt_kill_forward();
    }
    else if (buf[0] == -0x40 + 'U') {
        prompt_kill_backward();
    }
    else if (buf[0] == -0x40 + 'A') {
        pr->cursor = pr->prompt_len;
    }
    else if (buf[0] == -0x40 + 'E') {
        pr->cursor = pr->len;
    }
    else if (strncmp(buf, "\eOH", 3) == 0) { // home
        pr->cursor = pr->prompt_len;
    }
    else if (strncmp(buf, "\eOF", 3) == 0) { // end
        pr->cursor = pr->len;
    }
    return len;
}

void alert (char *fmt, ...) {
    va_list args;
    interrupt = 0;
    stage_cat(tparm(change_scroll_region, 0, lines - 1));
    stage_cat(tparm(cursor_address, lines - 1, 0));
    stage_cat("\n");
    va_start(args, fmt);
    stage_vprintf(fmt, args);
    va_end(args);
    stage_write();

    char buf[16];
    int nread = read(tty, buf, sizeof buf);
    stage_cat(tparm(change_scroll_region, line1, lines - 2));
    stage_tabs();
    draw_tab();

    if (nread > 0) {
        read_key(buf, nread);
    }
}

void gets1_prompt () {
    prompt_done = 0;
    interrupt = 0;
    pr->len = pr->prompt_len;
    pr->cursor = pr->len;
    pr->nlines = 1;
    if (!tlines2_size) {
        tlines2_size = lines;
        tlines2 = malloc(tlines_size * sizeof (tline_t));
    }
    stage_cat(tparm(change_scroll_region, 0, lines - 1));
    stage_cat(cursor_normal);
    stage_cat(tparm(cursor_address, lines - 1, 0));
    stage_cat("\r");
    stage_cat(clr_eol);
    stage_cat(pr->prompt);
    stage_write();
}

void gets2_prompt () {
    if (pr->len == pr->size) {
        pr->size *= 2;
        pr->buf = realloc(pr->buf, pr->size);
    }
    pr->buf[pr->len] = '\0';
    stage_cat(cursor_invisible);
    stage_cat(tparm(change_scroll_region, line1, lines - 2));
    stage_tabs();
}

char *gets_prompt (prompt_t *ppr) {
    pr = ppr;
    gets1_prompt();
    static char buf[256];
    int len = 0;
    int i, nread, clen, processed;
    while (1) {
        nread = read(tty, buf + len, sizeof buf - len);
        if (interrupt) {
            break;
        }
        if (nread < 0 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        }
        if (nread < 0) {
            perror("read");
            exit(1);
        }
        if (nread == 0) {
            exit(1);
        }
        for (i = 0; i < nread;) {
            clen = UTF8_LENGTH(buf[i]);
            if (clen > nread - i) {
                memmove(buf, buf + i, nread - i);
                len = nread - i;
                break;
            }
            processed = getc_prompt(buf + i, nread - i);
            i += processed;
            if (prompt_done || interrupt) {
                goto end;
            }
        }
        draw_prompt();
    }
    end:
    gets2_prompt();
    char *str = pr->buf + pr->prompt_len;
    pr = NULL;
    return str;
}

prompt_t *init_prompt (const char *prompt) {
    prompt_t *pr = malloc(sizeof (prompt_t));
    pr->prompt = prompt;
    pr->prompt_len = strlen(prompt);
    pr->size = 1024;
    pr->buf = malloc(pr->size);
    strcpy(pr->buf, pr->prompt);
    pr->len = pr->prompt_len;
    pr->cursor = pr->len;
    pr->nlines = 1;
    return pr;
}

