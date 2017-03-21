#include "les.h"
#include <iconv.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <term.h>
#include <fcntl.h>

char readbuf[8192];
iconv_t cd = NULL;
char *input_encoding = NULL;
char *lespipe = PREFIX "/share/les/lespipe";

int count_lines (char *buf, size_t len) {
    int i;
    int nlines = 0;
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            nlines++;
        }
    }
    return nlines;
}

int count_lines_atob (char *buf, size_t a, size_t b) {
    int nlines = 0;
    if (a < b) {
        nlines = count_lines(buf + a, b - a);
    }
    else {
        nlines = -count_lines(buf + b, a - b);
    }
    return nlines;
}

void process_backspace_highlights () {
    if (!man_page) {
        return;
    }
    int i = tabb->highlights_processed;
    int p1 = i;
    if (p1 >= tabb->buf_len) {
        return;
    }
    int p2 = p1 + UTF8_LENGTH(tabb->buf[p1]);
    if (p2 >= tabb->buf_len) {
        return;
    }
    int p3 = p2 + UTF8_LENGTH(tabb->buf[p2]);
    while (p3 < tabb->buf_len) {
        int p4 = p3 + UTF8_LENGTH(tabb->buf[p3]);
        if (tabb->buf[p2] == '\b') {
            int type = 0;
            if (tabb->buf[p1] == '_') {
                type = UNDERLINED;
            }
            if ((p2 - p1 == p4 - p3) && strncmp(tabb->buf + p1, tabb->buf + p3, p2 - p1) == 0) {
                type = BOLD;
            }
            if (type) {
                highlight_t *h = NULL;
                if (tabb->highlights_len) {
                    highlight_t *h2 = tabb->highlights + tabb->highlights_len - 1;
                    if (h2->end == i && h2->type == type) {
                        h = h2;
                    }
                }
                if (!h) {
                    if (tabb->highlights_len == tabb->highlights_size) {
                        if (tabb->highlights_size == 0) {
                            tabb->highlights_size = 32;
                            tabb->highlights = malloc(tabb->highlights_size * sizeof (highlight_t));
                        }
                        else {
                            tabb->highlights_size *= 2;
                            tabb->highlights = realloc(tabb->highlights, tabb->highlights_size * sizeof (highlight_t));
                        }
                    }
                    tabb->highlights_len++;
                    h = tabb->highlights + tabb->highlights_len - 1;
                    h->start = i;
                    h->type = type;
                }
                h->end = i + p4 - p3;
            }
            memmove(tabb->buf + i, tabb->buf + p3, p4 - p3);
            i += p4 - p3;
            p1 = p4;
            if (p1 >= tabb->buf_len) {
                break;
            }
            p2 = p1 + UTF8_LENGTH(tabb->buf[p1]);
            if (p2 >= tabb->buf_len) {
                break;
            }
            p3 = p2 + UTF8_LENGTH(tabb->buf[p2]);
            continue;
        }
        memmove(tabb->buf + i, tabb->buf + p1, p2 - p1);
        i += p2 - p1;
        p1 = p2;
        p2 = p3;
        p3 = p4;
    }
    tabb->highlights_processed = i;
    int j;
    for (j = p1; j < tabb->buf_len; j++, i++) {
        tabb->buf[i] = tabb->buf[j];
    }
    tabb->buf_len = i;
}

void add_encoded_input (char *buf, size_t buf_len) {
    char *buf_ptr = buf;
    size_t buf_left = buf_len;

    size_t tabb_buf_left = tabb->buf_size - tabb->buf_len;
    char *tabb_buf_ptr = tabb->buf + tabb->buf_len;
    size_t tabb_buf_len_orig = tabb->buf_len;

    while (1) {
        size_t tabb_buf_left_orig = tabb_buf_left;
        int retval = iconv(cd, &buf_ptr, &buf_left, &tabb_buf_ptr, &tabb_buf_left);
        size_t n = tabb_buf_left_orig - tabb_buf_left;
        tabb->buf_len += n;
        if (retval == -1) {
            if (errno == E2BIG) {
                tabb_buf_left += tabb->buf_size;
                tabb->buf_size += tabb->buf_size;
                tabb->buf = realloc(tabb->buf, tabb->buf_size);
                tabb_buf_ptr = tabb->buf + tabb->buf_len;
                continue;
            }
            else if (errno == EINVAL) {
                memcpy(tabb->stragglers, buf_ptr, buf_left);
                tabb->stragglers_len = buf_left;
            }
            else if (errno == EILSEQ) {
                fprintf(stderr, "Cannot encode character.\n");
                exit(1);
            }
            else {
                fprintf(stderr, "iconv error.\n");
                exit(1);
            }
        }
        break;
    }
    tabb->nlines += count_lines(tabb->buf + tabb_buf_len_orig, tabb->buf_len - tabb_buf_len_orig);
    process_backspace_highlights();
}

// makes sure buffer only contains whole UTF-8 characters, if any
// are incomplete then they are stored in the stragglers array
void add_unencoded_input (char *buf, size_t buf_len) {
    if (tabb->buf_size - tabb->buf_len < buf_len) {
        tabb->buf_size *= 2;
        tabb->buf = realloc(tabb->buf, tabb->buf_size);
    }
    int i;
    for (i = 0; i < buf_len;) {
        unsigned char c = buf[i];
        int len = UTF8_LENGTH(c);
        if (i + len > buf_len) {
            memcpy(tabb->stragglers, buf + i, buf_len - i);
            tabb->stragglers_len = buf_len - i;
        }
        else {
            memcpy(tabb->buf + tabb->buf_len, buf + i, len);
            tabb->buf_len += len;
        }
        i += len;
    }
    tabb->nlines += count_lines(tabb->buf + tabb->buf_len - i, i);
    process_backspace_highlights();
}

void read_file2 (char *readbuf, int nread) {
    if (input_encoding) {
        add_encoded_input(readbuf, nread);
    }
    else {
        add_unencoded_input(readbuf, nread);
    }
    if (tabb->buf_len == tabb->buf_size) {
        tabb->buf_size += tabb->buf_size;
        tabb->buf = realloc(tabb->buf, tabb->buf_size);
    }
    tabb->buf[tabb->buf_len] = '\0';
    if (!(tabb->state & POSITIONED) && tabb->last_line > 1 && tabb->nlines >= tabb->last_line) {
        move_to_line(tabb->last_line);
        tabb->state |= POSITIONED;
    }
}

void read_file () {
    if (tabb->stragglers_len) {
        memcpy(readbuf, tabb->stragglers, tabb->stragglers_len);
    }
    int nread = read(tabb->fd, readbuf + tabb->stragglers_len, sizeof readbuf - tabb->stragglers_len);
    if (nread < 0 && (errno == EAGAIN || errno == EINTR)) {
        return;
    }
    if (nread < 0) {
        // For example trying to read a directory
        tabb->buf_len += snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "Cannot read %s: %s\n", tabb->name, strerror(errno));
        tabb->state |= ERROR;
        tabb->state |= LOADED;
        draw_tab();
        return;
    }
    if (nread == 0) {
        tabb->state |= LOADED;
        if (tabb->buf_len && tabb->buf[tabb->buf_len - 1] != '\n') {
            tabb->nlines++;
        }
        draw_status();
        return;
    }
    nread += tabb->stragglers_len;
    tabb->stragglers_len = 0;
    read_file2(readbuf, nread);
    if (tabb->state & LOADED && tabb->state & LOADFOREVER) {
        move_end();
    }
    else if (tlines_len == lines - line1 - 1) {
        draw_status();
    }
    else {
        draw_tab();
    }
}

int open_with_lespipe () {
    if (strcmp(lespipe, "") == 0 || strcmp(lespipe, "none") == 0) {
        return 0;
    }

    int pipefd[2];
    int retval = pipe(pipefd);
    if (retval < 0) {
        return 0;
    }
    int cpid = fork();
    if (cpid < 0) {
        return 0;
    }
    if (cpid == 0) {
        dup2(pipefd[1], 1);
        dup2(pipefd[1], 2);
        close(pipefd[0]);
        close(pipefd[1]);
        execl(lespipe, lespipe, tabb->name, NULL);
        _exit(1);
    }

    close(pipefd[1]);
    int nread = read(pipefd[0], readbuf, sizeof readbuf);
    if (nread <= 0) {
        close(pipefd[0]);
        return 0;
    }
    int i;
    for (i = 0; i < nread; i++) {
        if (readbuf[i] == '\n') {
            break;
        }
    }
    if (i == 0 || i == nread) {
        close(pipefd[0]);
        return 0;
    }
    read_file2(readbuf + i + 1, nread - i - 1);
    tabb->fd = pipefd[0];
    tabb->state |= OPENED;
    return 1;
}

void set_man_page_name () {
    int nread = read(tabb->fd, readbuf, sizeof readbuf);
    if (nread <= 0) {
        return;
    }
    if (nread < 3) {
        nread += read(tabb->fd, readbuf + nread, sizeof readbuf - nread);
    }
    int i;
    for (i = 0; i < nread; i++) {
        if (readbuf[i] != '\n' && readbuf[i] != ' ' && readbuf[i] != '\t') {
            break;
        }
    }
    int start = i;
    for (; i < nread; i++) {
        if (readbuf[i] == '\n' || readbuf[i] == ' ' || readbuf[i] == '\t') {
            break;
        }
    }
    if (i > start) {
        free(tabb->name2);
        tabb->name2 = strndup(readbuf + start, i - start);
        tabb->name = strdup(tabb->name2);
        tabb->name_width = strwidth(tabb->name);
    }
    read_file2(readbuf, nread);
}

void open_tab_file () {
    if (tabb->state & (OPENED|LOADED)) {
        return;
    }
    get_last_line();
    if (open_with_lespipe()) {
        return;
    }
    int fd = open(tabb->name, O_RDONLY);
    if (fd < 0) {
        tabb->buf_len += snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "Cannot open %s: %s\n", tabb->name, strerror(errno));
        tabb->state |= ERROR;
        tabb->state |= LOADED;
        return;
    }
    tabb->fd = fd;
    tabb->state |= OPENED;
}

void set_input_encoding (char *encoding) {
    input_encoding = encoding;
    cd = iconv_open("UTF-8", input_encoding);
    if (cd == (iconv_t) -1) {
        fprintf(stderr, "Invalid encoding \"%s\"\n", encoding);
        exit(1);
    }
}

void reload () {
    if (!(tabb->state & FILEBACKED)) {
        return;
    }
    if (tabb->state & OPENED && !(tabb->state & LOADED) && tabb->fd) {
        close(tabb->fd);
    }
    tabb->state &= ~(OPENED|LOADED|ERROR);
    tabb->nlines = 0;
    tabb->buf_len = 0;
    tabb->stragglers_len = 0;
    tabb->matches_len = 0;
    tabb->search_version = 0;
    open_tab_file();
    draw_tab();
}

