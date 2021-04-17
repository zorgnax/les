#include "les.h"
#include <stdlib.h>
#include <term.h>

tline_t *tlines = NULL;
size_t tlines_len = 0;
size_t tlines_size = 0;
tline_t *tlines2 = NULL;
size_t tlines2_len = 0;
size_t tlines2_size = 0;
tline_t *tlines3 = NULL;
size_t tlines3_len = 0;
size_t tlines3_size = 0;


void get_nowrap_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size) {
    int i;
    unsigned char c;

    *tlines_len = 1;
    (*tlines)[*tlines_len - 1].pos = pos;

    for (i = pos; i < len; i++) {
        c = buf[i];
        if (c == '\n' || i == len - 1) {
            (*tlines)[*tlines_len - 1].end_pos = i + 1;
            if (i == len - 1 || (max && *tlines_len == max)) {
                break;
            }
            if (*tlines_len + 1 > *tlines_size) {
                *tlines_size *= 2;
                *tlines = realloc(*tlines, *tlines_size * sizeof (tline_t));
            }
            (*tlines_len)++;
            (*tlines)[*tlines_len - 1].pos = i + 1;
        }
    }
}

void get_wrap_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size) {
    int i, j, k;
    unsigned char c1, c2;
    int whitespace1, whitespace2;
    charinfo_t cinfo1, cinfo2;
    int column = 0;
    int width;

    *tlines_len = 1;
    (*tlines)[*tlines_len - 1].pos = pos;

    for (i = pos; i < len;) {
        c1 = buf[i];

        if (c1 == '\n') {
            i++;
            (*tlines)[*tlines_len - 1].end_pos = i;
            if (i == len || (max && *tlines_len == max)) {
                goto end;
            }
            if (*tlines_len + 1 > *tlines_size) {
                *tlines_size *= 2;
                *tlines = realloc(*tlines, *tlines_size * sizeof (tline_t));
            }
            (*tlines_len)++;
            (*tlines)[*tlines_len - 1].pos = i;

            column = 0;
            continue;
        }

        // get the word starting at byte i to byte j
        get_char_info(&cinfo1, buf, i);
        whitespace1 = c1 == ' ' || c1 == '\t';
        width = cinfo1.width;
        for (j = i + cinfo1.len; j < len;) {
            unsigned char c2 = buf[j];
            if (c2 == '\n') {
                break;
            }
            int whitespace2 = c2 == ' ' || c2 == '\t';
            if (whitespace1 ^ whitespace2) {
                break;
            }
            get_char_info(&cinfo2, buf, j);
            width += cinfo2.width;
            j += cinfo2.len;
        }

        // width of the word fits on the line
        if (column + width <= columns) {
            column += width;
            i = j;
            continue;
        }

        // it doesn't fit on this line, but would fit on a line by itself
        if (width <= columns) {
            (*tlines)[*tlines_len - 1].end_pos = i;
            if (max && *tlines_len == max) {
                goto end;
            }
            if (*tlines_len + 1 > *tlines_size) {
                *tlines_size *= 2;
                *tlines = realloc(*tlines, *tlines_size * sizeof (tline_t));
            }
            (*tlines_len)++;
            (*tlines)[*tlines_len - 1].pos = i;
            column = width;
            i = j;
            continue;
        }

    // wouldn't fit on a line by itself, so display as much as
    // you can on this line, then more on the next line
        for (k = 0; k < j - i;) {
            get_char_info(&cinfo2, buf, i + k);
            if (column + cinfo2.width > columns) {
                (*tlines)[*tlines_len - 1].end_pos = i + k;
                if (max && *tlines_len == max) {
                    goto end;
                }
                if (*tlines_len + 1 > *tlines_size) {
                    *tlines_size *= 2;
                    *tlines = realloc(*tlines, *tlines_size * sizeof (tline_t));
                }
                (*tlines_len)++;
                (*tlines)[*tlines_len - 1].pos = i + k;
                column = 0;
            }
            column += cinfo2.width;
            k += cinfo2.len;
        }
        i = j;
    }
    (*tlines)[*tlines_len - 1].end_pos = i;

    end:
    return;
}

void get_tlines (char *buf, size_t len, size_t pos, int max, tline_t **tlines, size_t *tlines_len, size_t *tlines_size) {
    *tlines_len = 0;
    if (!*tlines_size) {
        *tlines_size = lines;
        *tlines = malloc(*tlines_size * sizeof (tline_t));
    }
    if (pos >= len) {
        return;
    }
    if (line_wrap) {
        get_wrap_tlines(buf, len, pos, max, tlines, tlines_len, tlines_size);
    }
    else {
        get_nowrap_tlines(buf, len, pos, max, tlines, tlines_len, tlines_size);
    }
}

int prev_line (char *buf, size_t len, size_t pos, int n) {
    int i;
    int line = 0;
    if (pos == 0) {
        return 0;
    }
    for (i = pos - 1; i > 0; i--) {
        if (buf[i] == '\n' || i == len - 1) {
            line++;
            if (line == n + 1) {
                return i + 1;
            }
        }
    }
    line++;
    return 0;
}

int next_line (char *buf, size_t len, size_t pos, int n) {
    int i;
    int line = 0;
    for (i = pos; i < len; i++) {
        if (buf[i] == '\n' || i == len - 1) {
            line++;
            if (line == n) {
                return i + 1;
            }
        }
    }
    return len;
}

// uses tlines2 and puts answer into tlines3 for convenience
void get_tlines_backward (char *buf, size_t len, size_t pos, int max) {
    int i;
    if (tlines3_size < max) {
        tlines3_size = max;
        tlines3 = realloc(tlines3, tlines3_size * sizeof (tline_t));
    }
    tlines3_len = 0;
    while (1) {
        int prev = prev_line(buf, len, pos, 1);
        get_tlines(buf, pos, prev, 0, &tlines2, &tlines2_len, &tlines2_size);
        if (!tlines2_len) {
            break;
        }
        for (i = tlines2_len - 1; i >= 0; i--) {
            tlines3_len++;
            tlines3[tlines3_len - 1] = tlines2[i];
            if (tlines3_len == max) {
                return;
            }
        }
        pos = prev;
    }
}

// This is the position of the screen line before a position, if
// the position occurs in the middle of the line, it will go to the
// beginning of the screen line first.
size_t line_before_pos (size_t pos, int n) {
    // If pos occurs inside of a line, find the beginning of the
    // line that contains it
    if (pos != 0 && tabb->buf[pos - 1] != '\n') {
        n++;
    }
    if (n == 0 || pos == 0) {
        return pos;
    }
    get_tlines_backward(tabb->buf, tabb->buf_len, pos, n);
    size_t pos2 = tlines3[tlines3_len - 1].pos;
    return pos2;
}

// Find pos2 backwards in max number of lines, number of lines it
// actually took goes into nlines, and the position of the beginning
// of that line goes into pos3
int find_pos_backward (int max, size_t pos2, size_t *pos3) {
    size_t pos = tabb->pos;
    int i;
    if (tlines3_size < max) {
        tlines3_size = max;
        tlines3 = realloc(tlines3, tlines3_size * sizeof (tline_t));
    }
    tlines3_len = 0;
    while (1) {
        int prev = prev_line(tabb->buf, tabb->buf_len, pos, 1);
        get_tlines(tabb->buf, pos, prev, 0, &tlines2, &tlines2_len, &tlines2_size);
        if (!tlines2_len) {
            return 0;
        }
        for (i = tlines2_len - 1; i >= 0; i--) {
            tlines3_len++;
            tlines3[tlines3_len - 1] = tlines2[i];
            if (tlines3[tlines3_len - 1].pos <= pos2) {
                *pos3 = tlines3[tlines3_len - 1].pos;
                return 1;
            }
            if (tlines3_len == max) {
                return 0;
            }
        }
        pos = prev;
    }
    return 0;
}

// Finds the position of the top of the page to show pos at the bottom of it
int find_pos_forward (int max, size_t pos2, size_t *pos3) {
    int i;
    size_t pos = tlines[tlines_len - 1].end_pos;
    for (i = 0; i < max; i++) {
        get_tlines(tabb->buf, tabb->buf_len, pos, 1, &tlines2, &tlines2_len, &tlines2_size);
        if (!tlines2_len) {
            return 0;
        }
        if (tlines2[0].end_pos > pos2) {
            *pos3 = tlines[i + 1].pos;
            return 1;
        }
        pos = tlines2[0].end_pos;
    }
    return 0;
}

