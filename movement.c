#include "les.h"
#include <term.h>
#include <stdlib.h>
#include <unistd.h>

tline_t *tlines2 = NULL;
size_t tlines2_len = 0;
size_t tlines2_size = 0;
tline_t *tlines3 = NULL;
size_t tlines3_len = 0;
size_t tlines3_size = 0;

void move_forward_long (int n) {
    size_t pos = tlines[tlines_len - 1].end_pos;
    get_tlines(tabb->buf, tabb->buf_len, pos, n - tlines_len + 1, &tlines2, &tlines2_len, &tlines2_size);
    tabb->pos = tlines2[tlines2_len - 1].pos;
    int i;
    for (i = 0; i < tlines_len; i++) {
        if (tabb->buf[tlines[i].end_pos - 1] == '\n') {
            tabb->line++;
        }
    }
    for (i = 0; i < tlines2_len - 1; i++) {
        if (tabb->buf[tlines2[i].end_pos - 1] == '\n') {
            tabb->line++;
        }
    }
    draw_tab();
}

void move_forward_short (int n) {
    int m = n > (tlines_len - 1) ? (tlines_len - 1) : n;
    int i;
    for (i = 0; i < m; i++) {
        if (tabb->buf[tlines[i].end_pos - 1] == '\n') {
            tabb->line++;
        }
    }
    for (i = 0; i < tlines_len - m; i++) {
        tlines[i] = tlines[i + m];
    }
    tlines_len = tlines_len - m;
    size_t pos = tlines[tlines_len - 1].end_pos;
    get_tlines(tabb->buf, tabb->buf_len, pos, m, &tlines2, &tlines2_len, &tlines2_size);
    for (i = 0; i < tlines2_len; i++) {
        tlines_len++;
        tlines[tlines_len - 1] = tlines2[i];
    }
    tabb->pos = tlines[0].pos;
    stage_cat(cursor_up);
    stage_cat(tparm(parm_index, m));
    stage_cat(tparm(cursor_address, lines - 1 - m, 0));
    stage_tab2(m, tlines2, tlines2_len);
    draw_status();
}

void move_forward (int n) {
    if (tlines_len <= 1) {
        return;
    }
    if (n < lines - line1 - 1 || tlines[tlines_len - 1].end_pos == tabb->buf_len) {
        move_forward_short(n);
    }
    else {
        move_forward_long(n);
    }
}

void move_backward (int n) {
    if (tabb->pos == 0) {
        return;
    }
    int i, t, m, n2;
    size_t pos = tabb->pos;
    if (tlines3_size < n) {
        tlines3_size = n;
        tlines3 = realloc(tlines3, tlines3_size * sizeof (tline_t));
    }
    tlines3_len = 0;
    while (1) {
        int prev = prev_line(tabb->buf, tabb->buf_len, pos, 1);
        get_tlines(tabb->buf, pos, prev, 0, &tlines2, &tlines2_len, &tlines2_size);
        if (!tlines2_len) {
            break;
        }
        for (i = tlines2_len - 1; i >= 0; i--) {
            tlines3_len++;
            tlines3[tlines3_len - 1] = tlines2[i];
            if (tlines3_len == n) {
                goto end;
            }
        }
        pos = prev;
    }

    end:
    m = tlines3_len;
    tabb->pos = tlines3[tlines3_len - 1].pos;
    for (i = 0; i < tlines3_len; i++) {
        if (tabb->buf[tlines3[i].end_pos - 1] == '\n') {
            tabb->line--;
        }
    }
    if (m >= lines - line1 - 1) {
        draw_tab();
        return;
    }
    for (i = 0; i < m; i++) {
        tlines2[m - i - 1] = tlines3[i];
    }
    tlines2_len = m;

    n2 = (lines - line1 - 1 - m) > tlines_len ? (tlines_len) : (lines - line1 - 1 - m);
    for (i = n2 - 1; i >= 0; i--) {
        tlines[i + m] = tlines[i];
    }
    tlines_len = n2 + m;
    for (i = 0; i < m; i++) {
        tlines[i] = tlines2[i];
    }

    stage_cat(tparm(cursor_address, line1, 0));
    stage_cat(tparm(parm_rindex, m));
    stage_tab2(m, tlines2, tlines2_len);
    draw_status();
}

void move_start () {
    tabb->pos = 0;
    tabb->line = 1;
    tabb->column = 0;
    draw_tab();
}

void move_end () {
    if (tabb->nlines < lines) {
        tabb->pos = 0;
        tabb->line = 1;
        draw_tab();
        return;
    }
    tabb->pos = tabb->buf_len;
    tabb->line = tabb->nlines + 1;
    move_backward(lines - line1 - 1);
}

void move_pos (size_t pos) {
    if (pos == tabb->pos) {
        return;
    }

    // If pos occurs inside of a line, find the beginning of the line that contains it
    if (pos != 0 && tabb->buf[pos - 1] != '\n') {
        int prev = prev_line(tabb->buf, tabb->buf_len, pos, 0);
        int next = next_line(tabb->buf, tabb->buf_len, pos, 1);
        get_tlines(tabb->buf, next, prev, 0, &tlines2, &tlines2_len, &tlines2_size);
        int i;
        for (i = 0; i < tlines2_len; i++) {
            if (pos < tlines2[i].end_pos) {
                pos = tlines2[i].pos;
                break;
            }
        }
    }

    int nlines = 0;
    if (pos > tabb->pos) {
        nlines = count_lines(tabb->buf + tabb->pos, pos - tabb->pos);
        tabb->line += nlines;
    }
    else {
        nlines = count_lines(tabb->buf + pos, tabb->pos - pos);
        tabb->line -= nlines;
    }
    tabb->pos = pos;
    draw_tab();
}

void move_to_line (int line) {
    if (line == 1) {
        tabb->pos = 0;
        tabb->line = 1;
        tlines_len = 0;
        return;
    }
    int line2 = 1;
    int pos = 0;
    int i;
    for (i = 0; i < tabb->buf_len; i++) {
        if (tabb->buf[i] == '\n') {
            pos = i + 1;
            line2++;
            if (line == line2) {
                tabb->pos = pos;
                tabb->line = line2;
                tlines_len = 0;
                return;
            }
        }
    }
    tabb->pos = pos;
    tabb->line = line2;
    tlines_len = 0;
}

void move_left (int n) {
    if (line_wrap) {
        return;
    }
    if (tabb->column == 0) {
        return;
    }
    tabb->column -= n;
    if (tabb->column < 0) {
        tabb->column = 0;
    }
    draw_tab();
}

void move_right (int n) {
    if (line_wrap) {
        return;
    }
    tabb->column += n;
    draw_tab();
}

void move_line_left () {
    if (line_wrap) {
        return;
    }
    if (tabb->column == 0) {
        return;
    }
    tabb->column = 0;
    draw_tab();
}

void move_line_right () {
    if (line_wrap) {
        return;
    }
    int i;
    int width = 0;
    for (i = 0; i < tlines_len; i++) {
        int width2 = strnwidth(tabb->buf + tlines[i].pos, tlines[i].end_pos - tlines[i].pos);
        if (width2 > width) {
            width = width2;
        }
    }
    int column = width - columns;
    if (column < 0) {
        column = 0;
    }
    tabb->column = column;
    draw_tab();
}

