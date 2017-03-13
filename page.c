#include "les.h"
#include <term.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *buf;
    size_t buf_size;
    size_t buf_len;
    char *left;
    size_t left_size;
    size_t left_len;
    int left_width;
    char *right;
    size_t right_size;
    size_t right_len;
    int right_width;
    char *tty;
    size_t tty_size;
    size_t tty_len;
    char *matches;
    size_t matches_size;
    size_t matches_len;
} status_t;

status_t *status = NULL;
int atmatch = 0;
char *escapes = NULL;
size_t escapes_len = 0;
size_t escapes_size = 0;

char *human_readable (double size) {
    static char buf[32];
    static char power[] = "BKMGTPEZY";
    int i;
    for (i = 0; i < sizeof power / sizeof power[0]; i++) {
        if (size < 1024)
            break;
        size /= 1024;
    }
    int hundredths = (int)(size * 100) % 100;
    if (hundredths) {
        snprintf(buf, sizeof buf, "%.2f%c", size, power[i]);
    }
    else {
        snprintf(buf, sizeof buf, "%.0f%c", size, power[i]);
    }
    return buf;
}

void stage_status () {
    status->matches_len = 0;
    if (active_search) {
        if (tabb->matches_len == 0) {
            status->matches_len = snprintf(status->matches, status->matches_size, " 0 matches");
        }
        else if (tabb->matches_len == 1) {
            status->matches_len = snprintf(status->matches, status->matches_size, " 1 match");
        }
        else {
            status->matches_len = snprintf(status->matches, status->matches_size, " %lu/%lu matches", tabb->current_match + 1, tabb->matches_len);
        }
    }
    char *hrsize = human_readable(tabb->buf_len);
    status->right_len = snprintf(
        status->right, status->right_size,
        "%.*s%.*s %d/%d %s",
	(int) status->tty_len, status->tty, (int) status->matches_len, status->matches,
        tabb->line, tabb->nlines, hrsize);
    status->right_width = strnwidth(status->right, status->right_len);

    status->left_len = snprintf(
        status->left, status->left_size,
        "%s%.*s",
        tabb->name, (int) tabb->mesg_len, tabb->mesg);

    status->buf_len = 0;
    while (status->buf_size < columns + status->right_len + status->left_len) {
        status->buf_size *= 2;
        status->buf = realloc(status->buf, status->buf_size);
    }

    int i;
    int width = 0;
    charinfo_t cinfo;
    int right = columns - status->right_width;

    for (i = 0; i < status->left_len;) {
        if (width >= right) {
            break;
        }
        get_char_info(&cinfo, status->left, i);
        memcpy(status->buf + status->buf_len, status->left + i, cinfo.len);
        status->buf_len += cinfo.len;
        width += cinfo.width;
        i += cinfo.len;
    }

    while (width < right) {
        status->buf[status->buf_len] = ' ';
        status->buf_len++;
        width++;
    }

    for (i = 0; i < status->right_len; i++) {
        status->buf[status->buf_len] = status->right[i];
        status->buf_len++;
    }

    stage_cat(tparm(cursor_address, lines - 1, 0));
    stage_cat(tparm(set_a_background, 16 + 36*0 + 6*0 + 2));
    width = 0;
    int j = 0;
    for (i = 0; i < status->buf_len;) {
        get_char_info(&cinfo, status->buf, i);
        if (j == 0 && (!tabb->buf_len || width >= columns * tabb->pos / tabb->buf_len)) {
            stage_cat(tparm(set_a_background, 16 + 36*0 + 6*1 + 5));
            j++;
        }
        else if (j == 1 && tabb->buf_len && width >= columns * tlines[tlines_len - 1].end_pos / tabb->buf_len) {
            stage_cat(exit_attribute_mode);
            j++;
        }
        stage_ncat(status->buf + i, cinfo.len);
        width += cinfo.width;
        i += cinfo.len;
    }
    stage_cat(exit_attribute_mode);
}

void draw_status () {
    stage_status();
    stage_write();
}

// In man page output, text is underlined by placing and underscore
// followed by a backspace followed by the character to be underlined.
// text is bolded by placing the character followed by a backspace
// then the same character again.
void stage_backspace (charinfo_t *cinfo, char *buf, int i) {
    if (i == 0) {
        cinfo->width = 2;
        cinfo->len = 1;
        stage_cat("^H");
    }
    else if (buf[i - 1] == '_') {
        get_char_info(cinfo, buf, i + 1);
        stage_cat("\b");
        stage_cat(enter_underline_mode);
        stage_ncat(buf + i + 1, cinfo->len);
        stage_cat(exit_underline_mode);
        cinfo->width = 0;
        cinfo->len += 1;
    }
    else if (buf[i - 1] == buf[i + 1]) {
        get_char_info(cinfo, buf, i + 1);
        stage_cat("\b");
        stage_cat(enter_bold_mode);
        stage_ncat(buf + i + 1, cinfo->len);
        stage_cat(exit_attribute_mode);
        cinfo->width = 0;
        cinfo->len += 1;
    }
    else {
        cinfo->width = 2;
        cinfo->len = 1;
        stage_cat("^H");
    }
}

void escapes_record (charinfo_t *cinfo, char *buf, int i) {
    if (buf[i] == '\e' && cinfo->len > 1) {
        if (strncmp(buf + i, "\e[0m", cinfo->len) == 0 ||
            strncmp(buf + i, "\e[m", cinfo->len) == 0) {
            escapes_len = 0;
        }
        else {
            if (escapes_len + cinfo->len <= escapes_size) {
                strncpy(escapes + escapes_len, buf + i, cinfo->len);
                escapes_len += cinfo->len;
            }
        }
    }
}

void escapes_start () {
    if (escapes_len) {
        stage_ncat(escapes, escapes_len);
    }
}

void escapes_end () {
    if (escapes_len) {
        stage_cat(exit_attribute_mode);
    }
}

void highlight_match3 () {
    if (escapes_len) {
        stage_cat(exit_attribute_mode);
    }
    if (atmatch == tabb->current_match) {
        stage_cat(tparm(set_a_background, 16 + 36*0 + 6*2 + 0));
    }
    else {
        stage_cat(tparm(set_a_background, 16 + 36*0 + 6*0 + 2));
    }
}

void highlight_match_start (char *buf, int i) {
    if (!tabb->matches_len || atmatch >= tabb->matches_len) {
        return;
    }
    if (i > tabb->matches[atmatch].start && i < tabb->matches[atmatch].end) {
        highlight_match3();
    }
}

void highlight_match_end (char *buf, int i) {
    if (!tabb->matches_len || atmatch >= tabb->matches_len) {
        return;
    }
    if (i > tabb->matches[atmatch].start && i <= tabb->matches[atmatch].end) {
        stage_cat(exit_attribute_mode);
    }
}

void highlight_match (char *buf, int i) {
    if (!tabb->matches_len) {
        return;
    }
    if (atmatch < tabb->matches_len && tabb->matches[atmatch].start != tabb->matches[atmatch].end && i == tabb->matches[atmatch].end) {
        stage_cat(exit_attribute_mode);
        escapes_start();
        atmatch++;
    }
    if (atmatch < tabb->matches_len && i == tabb->matches[atmatch].start) {
        highlight_match3();
    }
}

void highlight_match2 (char *buf, int i) {
    if (!tabb->matches_len || atmatch >= tabb->matches_len) {
        return;
    }
    if (i == tabb->matches[atmatch].end) {
        stage_cat(exit_attribute_mode);
        escapes_start();
        atmatch++;
    }
}

void stage_character (charinfo_t *cinfo, char *buf, int i) {
    static char str[16];
    int j;
    unsigned char c = buf[i];
    if (cinfo->error) {
        str[0] = '[';
        for (j = 0; j < cinfo->len; j++) {
            unsigned char c2 = buf[i + j];
            sprintf(str + 1 + j * 2, "%02X", c2);
        }
        str[j * 2 + 1] = ']';
        str[j * 2 + 2] = '\0';
        stage_cat(str);
    }
    else if (c == '\t') {
        for (j = 0; j < tab_width; j++) {
            stage_cat(" ");
        }
    }
    else if (c == '\b') {
        stage_backspace(cinfo, buf, i);
    }
    else if (buf[i] == '\e' && cinfo->len > 1) {
        stage_ncat(buf + i, cinfo->len);
    }
    else if (c == 0x00) {
        stage_cat("·");
    }
    else if (c < 0x20) {
        sprintf(str, "^%c", 0x40 + c);
        stage_cat(str);
    }
    else if (c == 0x7f) {
        sprintf(str, "^%c", -0x40 + c);
        stage_cat(str);
    }
    else {
        stage_ncat(buf + i, cinfo->len);
    }
}

void stage_line_wrap (tline_t *tline) {
    charinfo_t cinfo;
    int width = 0;
    int i = tline->pos;
    escapes_start();
    highlight_match_start(tabb->buf, i);
    while (i < tline->end_pos) {
        get_char_info(&cinfo, tabb->buf, i);
        highlight_match(tabb->buf, i);
        if ((tabb->buf[i] == '\r' && tabb->buf[i + 1] == '\n') || tabb->buf[i] == '\n') {
            highlight_match2(tabb->buf, i);
            break;
        }
        escapes_record(&cinfo, tabb->buf, i);
        stage_character(&cinfo, tabb->buf, i);
        highlight_match2(tabb->buf, i);
        width += cinfo.width;
        i += cinfo.len;
    }
    highlight_match_end(tabb->buf, i);
    escapes_end();
    if (width < columns) {
        stage_cat(clr_eol);
    }
}

void stage_line_nowrap (tline_t *tline) {
    int width = 0;
    charinfo_t cinfo;
    int i = tline->pos;
    escapes_start();
    if (atmatch < tabb->matches_len && tabb->matches[atmatch].end < i) {
        atmatch++;
    }
    highlight_match_start(tabb->buf, i);
    while (i < tline->end_pos) {
        get_char_info(&cinfo, tabb->buf, i);
        if (width >= tabb->column && (!tabb->column || cinfo.width)) {
            break;
        }
        if (tabb->buf[i] == 0x1b && cinfo.len > 1) {
            escapes_record(&cinfo, tabb->buf, i);
            stage_ncat(tabb->buf + i, cinfo.len);
        }
        highlight_match(tabb->buf, i);
        highlight_match2(tabb->buf, i);
        width += cinfo.width;
        i += cinfo.len;
    }
    if (i == tline->end_pos) {
        highlight_match_end(tabb->buf, i);
        escapes_end();
        stage_cat(clr_eol);
        return;
    }
    while (i < tline->end_pos) {
        get_char_info(&cinfo, tabb->buf, i);
        if (width + cinfo.width > columns + tabb->column) {
            break;
        }
        if (tabb->buf[i] == 0x1b && cinfo.len > 1) {
            escapes_record(&cinfo, tabb->buf, i);
        }
        highlight_match(tabb->buf, i);
        if ((tabb->buf[i] == '\r' && tabb->buf[i + 1] == '\n') || tabb->buf[i] == '\n') {
            highlight_match2(tabb->buf, i);
            break;
        }
        stage_character(&cinfo, tabb->buf, i);
        highlight_match2(tabb->buf, i);
        width += cinfo.width;
        i += cinfo.len;
    }
    highlight_match_end(tabb->buf, i);
    escapes_end();
    if (width < columns + tabb->column) {
        stage_cat(clr_eol);
    }
}

void stage_tab2 (int n, tline_t *tlines, size_t tlines_len) {
    int i;
    if (tabb->matches_len) {
        atmatch = 0;
        for (i = 0; i < tabb->matches_len; i++) {
            if (tabb->matches[i].end >= tlines[0].pos) {
                atmatch = i;
                break;
            }
        }
    }
    escapes[0] = '\0';
    for (i = 0; i < n; i++) {
        if (i < tlines_len) {
            if (line_wrap) {
                stage_line_wrap(tlines + i);
            }
            else {
                stage_line_nowrap(tlines + i);
            }
        }
        else {
            stage_cat(tparm(set_a_foreground, 4));
            stage_cat("~");
            stage_cat(clr_eol);
            stage_cat(exit_attribute_mode);
        }
        if (i != n - 1) {
            stage_cat("\n");
        }
    }
}

void stage_tab () {
    stage_cat(tparm(cursor_address, line1, 0));
    get_tlines(tabb->buf, tabb->buf_len, tabb->pos, lines - line1 - 1, &tlines, &tlines_len, &tlines_size);
    stage_tab2(lines - line1 - 1, tlines, tlines_len);
    stage_status();
}

void draw_tab () {
    stage_tab();
    stage_write();
}

void init_page () {
    status = malloc(sizeof (status_t));
    status->buf_size = 1024;
    status->buf_len = 0;
    status->buf = malloc(status->buf_size);
    status->left_size = 1024;
    status->left_len = 0;
    status->left = malloc(status->left_size);
    status->right_size = 1024;
    status->right_len = 0;
    status->right = malloc(status->right_size);
    status->tty_size = 64;
    status->tty_len = 0;
    status->tty = malloc(status->tty_size);
    status->matches_size = 64;
    status->matches_len = 0;
    status->matches = malloc(status->matches_size);
    escapes_size = 256;
    escapes_len = 0;
    escapes = malloc(escapes_size);
}

void set_ttybuf (charinfo_t *cinfo, char *buf, int len) {
    int i;
    status->tty[0] = ' ';
    status->tty_len = 1;
    int retval;
    for (i = 0; i < len; i++) {
        if (status->tty_len + 3 > status->tty_size) {
            break;
        }
        unsigned char c = buf[i];
        if (c < 0x20) {
            retval = sprintf(status->tty + status->tty_len, "^%c", 0x40 + c);
            status->tty_len += retval;
        }
        else if (c == 0x7f) {
            retval = sprintf(status->tty + status->tty_len, "^?");
            status->tty_len += retval;
        }
        else if (c == 0x20) {
            retval = sprintf(status->tty + status->tty_len, "␣");
            status->tty_len += retval;
        }
        else {
            status->tty[status->tty_len] = c;
            status->tty_len++;
        }
    }
}

