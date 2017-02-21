#include "les.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <term.h>
#include <errno.h>
#include <string.h>

prompt_t *pr = NULL;
prompt_t *spr = NULL;
int prompt_done = 0;

void prompt_draw_line (tline_t *tline) {
    charinfo_t cinfo;
    int i;
    int width = 0;
    for (i = tline->pos; i < tline->end_pos;) {
        get_char_info(&cinfo, pr->buf + i);
        if (pr->buf[i] == '\r' || pr->buf[i] == '\n') {
            break;
        }
        else if (pr->buf[i] == '\t') {
            printf("%*s", tab_width, "");
        }
        else {
            printf("%.*s", cinfo.len, pr->buf + i);
        }
        width += cinfo.width;
        i += cinfo.len;
    }
    if (width < columns) {
        printf("%s", clr_eol);
    }
}

void prompt_draw () {
    printf("%s", tparm(cursor_address, lines - pr->nlines, 0));
    get_tlines(pr->buf, pr->len, 0, 0, &tlines, &tlines_len, &tlines_size);
    if (tlines_len < pr->nlines2) {
        printf("%s", clr_eos);
    }
    pr->nlines2 = tlines_len;
    if (tlines_len > pr->nlines) {
        pr->nlines = tlines_len;
    }
    int i;
    for (i = 0; i < tlines_len; i++) {
        prompt_draw_line(tlines + i);
        if (i != tlines_len - 1) {
            printf("\n");
        }
    }
    int cursory = 0;
    int cursorx = 0;
    for (i = 0; i < tlines_len; i++) {
        if (pr->cursor <= tlines[i].end_pos) {
            cursory = i;
            cursorx = strnwidth(pr->buf + tlines[i].pos, pr->cursor - tlines[i].pos);
            if (cursorx >= columns) {
                cursory++;
                cursorx = 0;
            }
            break;
        }
    }
    if (cursory + 1 > pr->nlines) {
        pr->nlines++;
        printf("\n");
    }
    printf("%s", tparm(cursor_address, lines - pr->nlines + cursory, cursorx));
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
    for (; i >= pr->prompt_len;) {
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

void prompt_addc (char *buf, int len) {
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

int prompt_getc (char *buf, int len) {
    unsigned char c = buf[0];
    if ((c > 0x1f && c < 0x7f) || (c > 0x7f)) {
        int len2 = UTF8_LENGTH(c);
        prompt_addc(buf, len2);
        return len2;
    }
    if (strncmp(buf, "\e[D", 3) == 0) {
        prompt_left();
    }
    else if (strncmp(buf, "\eb", 2) == 0) {
        prompt_left_word();
    }
    else if (strncmp(buf, "\e[C", 3) == 0) {
        prompt_right();
    }
    else if (strncmp(buf, "\ef", 2) == 0) {
        prompt_right_word();
    }
    else if (strncmp(buf, "\x7f", 1) == 0) {
        prompt_backspace();
    }
    else if (strncmp(buf, "\e\x7f", 2) == 0) {
        prompt_backspace_word();
    }
    else if (strncmp(buf, "\e[3~", 4) == 0) {
        prompt_delete();
    }
    else if (strncmp(buf, "\e(", 2) == 0) {
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
    else if (strncmp(buf, "\e[H", 3) == 0) {
        pr->cursor = pr->prompt_len;
    }
    else if (strncmp(buf, "\e[F", 3) == 0) {
        pr->cursor = pr->len;
    }
    return len;
}

void prompt_gets1 () {
    pr->len = pr->prompt_len;
    pr->cursor = pr->len;
    pr->nlines = 1;
    printf("%s", tparm(change_scroll_region, 0, lines - 1));
    printf("%s", cursor_normal);
    printf("%s", tparm(cursor_address, lines - 1, 0));
    printf("\n%s", pr->prompt);
}

void prompt_gets2 () {
    prompt_done = 0;
    printf("%s", cursor_invisible);
    printf("%s", tparm(change_scroll_region, line1, lines - 2));
    draw_tab();
}

void prompt_gets () {
    prompt_gets1();
    static char buf[256];
    int len = 0;
    int i, nread, clen, processed;
    while (1) {
        nread = read(tty, buf + len, sizeof buf - len);
        if (prompt_done) {
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
            if (buf[i] == '\n') {
                goto end;
            }
            clen = UTF8_LENGTH(buf[i]);
            if (clen > nread - i) {
                memmove(buf, buf + i, nread - i);
                len = nread - i;
                break;
            }
            processed = prompt_getc(buf + i, nread - i);
            i += processed;
        }
        prompt_draw();
    }
    end:
    prompt_gets2();
}

prompt_t *prompt_init (const char *prompt) {
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

void search () {
    if (!spr) {
        spr = prompt_init("foo: ");
    }
    pr = spr;
    prompt_gets();
    pr = NULL;
}

