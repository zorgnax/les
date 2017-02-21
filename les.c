#include "les.h"
#include <stdio.h>
#include <stdlib.h>
#include <term.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <iconv.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/ioctl.h>

size_t tabs_len = 0;
size_t tabs_size = 0;
tab_t **tabs;
int current_tab = 0;
tab_t *tabb;
int line_wrap = 1;
char *input_encoding = NULL;
iconv_t cd = NULL;
char *status_buf;
size_t status_buf_size;
size_t status_buf_len;
int tty;
struct termios tcattr1, tcattr2;
char ttybuf[256];
size_t ttybuf_len = 0;
size_t ttybuf_width = 0;
int line1;
tline_t *tlines = NULL;
size_t tlines_len = 0;
size_t tlines_size = 0;
int tab_width = 4;

void bye () {
    tcsetattr(tty, TCSANOW, &tcattr1);
    printf("%s", tparm(change_scroll_region, 0, lines - 1));
    printf("%s", cursor_normal);
    printf("%s", exit_ca_mode);
}

void bye2 () {
    exit(1);
}

void interrupt () {
    if (pr) {
        prompt_done = 1;
    }
    else {
        exit(1);
    }
}

void set_tcattr () {
    tcattr2 = tcattr1;
    tcattr2.c_lflag &= ~(ICANON|ECHO);
    tcattr2.c_cc[VMIN] = 1;
    tcattr2.c_cc[VTIME] = 0;
    tcsetattr(tty, TCSAFLUSH, &tcattr2);
}

void cont () {
    set_tcattr();
    if (pr) {
        prompt_draw();
    }
    else {
        draw_tab();
    }
}

void add_tab (const char *name, int fd) {
    tabb = malloc(sizeof (tab_t));
    tabb->name = name;
    tabb->name_width = strwidth(name);
    tabb->name2 = strdup(name);
    tabb->fd = fd;
    tabb->pos = 0;
    tabb->loaded = 0;
    tabb->buf_size = 8192;
    tabb->buf = malloc(tabb->buf_size);
    tabb->buf_len = 0;
    tabb->stragglers = malloc(16);
    tabb->stragglers_len = 0;
    tabb->line = 1;
    tabb->nlines = 0;
    tabb->column = 0;
    if (tabs_size == 0) {
        tabs_size = 4;
        tabs = malloc(tabs_size * sizeof (tab_t *));
    }
    else if (tabs_len == tabs_size) {
        tabs_size *= 2;
        tabs = realloc(tabs, tabs_size * sizeof (tab_t *));
    }
    tabs[tabs_len] = tabb;
    tabs_len++;
}

int add_tab_file (const char *name) {
    int fd = open(name, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Can't open %s: %s\n", name, strerror(errno));
        return 0;
    }
    add_tab(name, fd);
    return 1;
}

int isdir (const char *name) {
    struct stat statbuf;
    int retval = stat(name, &statbuf);
    if (retval != 0) {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

int add_tab_dir (const char *name) {
    add_tab(name, 0);
    static char buf[512];
    snprintf(buf, sizeof buf, "CLICOLOR_FORCE=1 ls -G -l -h %s", name);
    FILE *fh = popen(buf, "r");
    int c;
    while ((c = getc(fh)) != EOF) {
        if (tabb->buf_len == tabb->buf_size) {
            tabb->buf_size *= 2;
            tabb->buf = realloc(tabb->buf, tabb->buf_size);
        }
        tabb->buf_len++;
        tabb->buf[tabb->buf_len - 1] = c;
    }
    pclose(fh);
    tabb->loaded = 1;
    return 1;
}

void generate_tab_names () {
    int i;
    int width = (tabs_len - 1) * 2;
    for (i = 0; i < tabs_len; i++) {
        strcpy(tabs[i]->name2, tabs[i]->name);
        width += tabs[i]->name_width;
    }
    if (width <= columns) {
        return;
    }
    width = (tabs_len - 1) * 2;
    for (i = 0; i < tabs_len; i++) {
        char *name = basename(tabs[i]->name2);
        strcpy(tabs[i]->name2, name);
        width += strwidth(name);
    }
    if (width <= columns) {
        return;
    }
    width = 4 * tabs_len + 2 * (tabs_len - 1);
    int width2 = (columns - 2 * (tabs_len - 1)) / tabs_len;
    if (width2 < 6) {
        width2 = 10;
    }
    for (i = 0; i < tabs_len; i++) {
        char *name = tabs[i]->name2;
        shorten(name, width2);
    }
}

void draw_tabs () {
    if (tabs_len == 1) {
        return;
    }
    generate_tab_names();
    printf("%s", tparm(cursor_address, 0, 0));
    printf("%s", tparm(set_a_background, 232 + 2));
    int i;
    int width = 0;
    for (i = 0; i < tabs_len; i++) {
        char *name = tabs[i]->name2;
        int width2 = strwidth(name);
        if (width + width2 > columns) {
            break;
        }
        if (i == current_tab) {
            printf("%s", enter_bold_mode);
            printf("%s", name);
            printf("%s", exit_attribute_mode);
            printf("%s", tparm(set_a_background, 232 + 2));
        }
        else {
            printf("%s", name);
        }
        width += width2;
        if (width + 2 > columns) {
            break;
        }
        if (i < tabs_len - 1) {
            printf("  ");
        }
    }
    for (i = 0; i < columns - width; i++) {
        printf(" ");
    }
    printf("%s", exit_attribute_mode);
    printf("\n");
}

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

void draw_status () {
    static char right_buf[256];
    int retval;

    printf("%s", tparm(cursor_address, lines - 1, 0));
    int right_len = 0;
    int right_width = 0;
    if (ttybuf_len) {
        retval = snprintf(right_buf + right_len, sizeof right_buf - right_len,
            " %.*s", (int) ttybuf_len, ttybuf);
        right_len += retval;
        right_width += ttybuf_width + 1;
    }
    char *hrsize = human_readable(tabb->buf_len);
    retval = snprintf(right_buf + right_len, sizeof right_buf - right_len,
        " %lu -> %lu %d/%d %s", tabb->pos, tlines[tlines_len - 1].end_pos, tabb->line, tabb->nlines, hrsize);
    right_len += retval;
    right_width += retval;
    int right = columns - right_width;

    int width = 0;
    int i;
    charinfo_t cinfo;
    status_buf_len = 0;
    for (i = 0; i < status_buf_size;) {
        if (!tabb->name[i]) {
            break;
        }
        if (width >= right) {
            break;
        }
        get_char_info(&cinfo, tabb->name + i);
        memcpy(status_buf + i, tabb->name + i, cinfo.len);
        status_buf_len += cinfo.len;
        width += cinfo.width;
        i += cinfo.len;
    }

    for (; width < right && i < status_buf_size; i++) {
        status_buf[i] = ' ';
        status_buf_len++;
        width++;
    }

    int j;
    for (j = 0; j < right_len && i < status_buf_size; j++, i++) {
        status_buf[i] = right_buf[j];
        status_buf_len++;
    }

    printf("%s", tparm(set_a_background, 16 + 36*0 + 6*0 + 2));
    width = 0;
    j = 0;
    for (i = 0; i < status_buf_len;) {
        get_char_info(&cinfo, status_buf + i);
        if (j == 0 && (!tabb->buf_len || width >= columns * tabb->pos / tabb->buf_len)) {
            printf("%s", tparm(set_a_background, 16 + 36*0 + 6*1 + 5));
            j++;
        }
        else if (j == 1 && tabb->buf_len && width >= columns * tlines[tlines_len - 1].end_pos / tabb->buf_len) {
            printf("%s", exit_attribute_mode);
            j++;
        }
        printf("%.*s", cinfo.len, status_buf + i);
        width += cinfo.width;
        i += cinfo.len;
    }
    printf("%s", exit_attribute_mode);
}

void draw_line_wrap (tline_t *tline) {
    charinfo_t cinfo;
    int i;
    int width = 0;
    for (i = tline->pos; i < tline->end_pos;) {
        get_char_info(&cinfo, tabb->buf + i);
        if (tabb->buf[i] == '\r' || tabb->buf[i] == '\n') {
            break;
        }
        else if (tabb->buf[i] == '\t') {
            printf("%*s", tab_width, "");
        }
        else {
            printf("%.*s", cinfo.len, tabb->buf + i);
        }
        width += cinfo.width;
        i += cinfo.len;
    }
    if (width < columns) {
        printf("%s", clr_eol);
    }
}

void draw_line_nowrap (tline_t *tline) {
    int i;
    int e = 0;
    int width = 0;
    charinfo_t cinfo;
    for (i = tline->pos; i < tline->end_pos;) {
        get_char_info(&cinfo, tabb->buf + i);
        if (width >= tabb->column && (!tabb->column || cinfo.width)) {
            break;
        }
        if (tabb->buf[i] == 0x1b) {
            e++;
            printf("%.*s", cinfo.len, tabb->buf + i);
        }
        width += cinfo.width;
        i += cinfo.len;
    }
    if (i == tline->end_pos) {
        printf("%s", clr_eol);
        return;
    }
    for (; i < tline->end_pos;) {
        get_char_info(&cinfo, tabb->buf + i);
        if (width + cinfo.width > columns + tabb->column) {
            break;
        }
        if (tabb->buf[i] == 0x1b) {
            e++;
        }
        if (tabb->buf[i] == '\r' || tabb->buf[i] == '\n') {
            break;
        }
        if (tabb->buf[i] == '\t') {
            printf("%*s", tab_width, "");
        }
        else {
            printf("%.*s", cinfo.len, tabb->buf + i);
        }
        width += cinfo.width;
        i += cinfo.len;
    }
    if (width < columns + tabb->column) {
        printf("%s", clr_eol);
    }
    if (e) {
        printf("%s", exit_attribute_mode);
    }
}

void draw_tab2 (int n, tline_t *tlines, size_t tlines_len) {
    int i;
    for (i = 0; i < n; i++) {
        if (i < tlines_len) {
            if (line_wrap) {
                draw_line_wrap(tlines + i);
            }
            else {
                draw_line_nowrap(tlines + i);
            }
        }
        else {
            printf("%s", tparm(set_a_foreground, 4));
            printf("~");
            printf("%s", clr_eol);
            printf("%s", exit_attribute_mode);
        }
        if (i != n - 1) {
            printf("\n");
        }
    }
}

void draw_tab () {
    printf("%s", tparm(cursor_address, line1, 0));
    get_tlines(tabb->buf, tabb->buf_len, tabb->pos, lines - line1 - 1, &tlines, &tlines_len, &tlines_size);
    draw_tab2(lines - line1 - 1, tlines, tlines_len);
    draw_status();
}

void winch () {
    struct winsize w;
    ioctl(tty, TIOCGWINSZ, &w);
    lines = w.ws_row;
    columns = w.ws_col;
    if (pr) {
        prompt_draw();
    }
    else {
        draw_tab();
    }
}

void count_lines (char *buf, size_t len) {
    int i;
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            tabb->nlines++;
        }
    }
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
    count_lines(tabb->buf + tabb_buf_len_orig, tabb->buf_len - tabb_buf_len_orig);
}

// makes sure buffer only contains whole utf-8 characters, if any
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
    count_lines(tabb->buf + tabb->buf_len - i, i);
}

void read_file () {
    static char buf[8192];
    if (tabb->stragglers_len) {
        memcpy(buf, tabb->stragglers, tabb->stragglers_len);
    }
    int nread = read(tabb->fd, buf + tabb->stragglers_len, sizeof buf - tabb->stragglers_len);
    if (errno == EAGAIN || errno == EINTR) {
        return;
    }
    if (nread < 0) {
        perror("read");
        exit(1);
    }
    if (nread == 0) {
        tabb->loaded = 1;
        if (tabb->fd) {
            close(tabb->fd);
        }
        if (tabb->buf_len && tabb->buf[tabb->buf_len - 1] != '\n') {
            tabb->nlines++;
        }
        draw_status();
        return;
    }
    nread += tabb->stragglers_len;
    tabb->stragglers_len = 0;
    if (input_encoding) {
        add_encoded_input(buf, nread);
    }
    else {
        add_unencoded_input(buf, nread);
    }
    if (tabb->buf_len == tabb->buf_size) {
        tabb->buf_size += tabb->buf_size;
        tabb->buf = realloc(tabb->buf, tabb->buf_size);
    }
    tabb->buf[tabb->buf_len] = '\0';
    if (tlines_len == lines - line1 - 1) {
         draw_status();
    }
    else {
        draw_tab();
    }
}

void set_ttybuf (charinfo_t *cinfo, char *buf, int len) {
    if (cinfo->width) {
        memcpy(ttybuf, buf, cinfo->len);
        ttybuf_len = cinfo->len;
        ttybuf_width = cinfo->width;
    }
    else {
        int i;
        ttybuf_len = 0;
        ttybuf_width = 0;
        for (i = 0; i < len; i++) {
            if (buf[i] < 0x20) {
                snprintf(ttybuf + ttybuf_len, sizeof ttybuf - ttybuf_len,
                    "^%c", 0x40 + buf[i]);
                ttybuf_len += 2;
                ttybuf_width += 2;
            }
            else if (buf[i] == 0x7f) {
                snprintf(ttybuf + ttybuf_len, sizeof ttybuf - ttybuf_len, "^?");
                ttybuf_len += 2;
                ttybuf_width += 2;
            }
            else {
                ttybuf[ttybuf_len] = buf[i];
                ttybuf_len++;
                ttybuf_width++;
            }
        }
    }
}

void next_tab () {
    current_tab = (current_tab + 1) % tabs_len;
    tabb = tabs[current_tab];
    draw_tabs();
    draw_tab();
}

void prev_tab () {
    current_tab = current_tab > 0 ? (current_tab - 1) : (tabs_len - 1);
    tabb = tabs[current_tab];
    draw_tabs();
    draw_tab();
}

void close_tab () {
    if (tabs_len == 1) {
        exit(0);
        return;
    }
    tab_t *tabb2 = tabb;
    int i;
    for (i = current_tab; i < tabs_len - 1; i++) {
        tabs[i] = tabs[i + 1];
    }
    tabs_len--;
    if (current_tab == tabs_len) {
        current_tab--;
    }
    tabb = tabs[current_tab];

    free(tabb2->name2);
    free(tabb2->buf);
    free(tabb2->stragglers);
    if (!tabb2->loaded && tabb2->fd) {
        close(tabb2->fd);
    }
    free(tabb2);

    if (tabs_len == 1) {
        line1 = 0;
        printf("%s", tparm(change_scroll_region, line1, lines - 1));
    }

    draw_tabs();
    draw_tab();
}

void move_left (int n) {
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
    tabb->column += n;
    draw_tab();
}

int read_key (char *buf, int len) {
    charinfo_t cinfo;
    get_char_info(&cinfo, buf);
    set_ttybuf(&cinfo, buf, len);
    int extended = 0;
    switch (buf[0]) {
        case 'd':
            move_forward((lines - line1 - 1) / 2);
            break;
        case 'D':
            move_forward(lines - line1 - 2);
            break;
        case 'g':
            move_start();
            break;
        case 'G':
            move_end();
            break;
        case 'h':
            move_left(4);
            break;
        case 'H':
            move_left(columns / 2);
            break;
        case 'j':
            move_forward(1);
            break;
        case 'J':
            move_forward(2);
            break;
        case 'k':
            move_backward(1);
            break;
        case 'K':
            move_backward(2);
            break;
        case 'l':
            move_right(4);
            break;
        case 'L':
            move_right(columns / 2);
            break;
        case 'q':
            close_tab();
            break;
        case 'Q':
            exit(0);
            break;
        case 't':
            next_tab();
            break;
        case 'T':
            prev_tab();
            break;
        case 'u':
            move_backward((lines - line1 - 1) / 2);
            break;
        case 'U':
            move_backward(lines - line1 - 2);
            break;
        case 'w':
            line_wrap = !line_wrap;
            draw_tab();
            break;
        case '/':
            search();
            break;
        case -0x40 + 'D':
            move_forward(10000);
            break;
        case -0x40 + 'H':
            move_left(1);
            break;
        case -0x40 + 'L':
            move_right(1);
            break;
        case -0x40 + 'R':
            draw_tab();
            break;
        case -0x40 + 'U':
            move_backward(10000);
            break;
        default:
            extended = 1;
    }
    if (!extended) {
        return 1;
    }
    if (strncmp(buf, "\e[B", 3) == 0) {
        move_forward(1);
    }
    else if (strncmp(buf, "\e[A", 3) == 0) {
        move_backward(1);
    }
    else if (strncmp(buf, "\e[D", 3) == 0) {
        move_left(4);
    }
    else if (strncmp(buf, "\eb", 2) == 0) {
        move_left(columns / 2);
    }
    else if (strncmp(buf, "\e[C", 3) == 0) {
        move_right(4);
    }
    else if (strncmp(buf, "\ef", 2) == 0) {
        move_right(columns / 2);
    }
    else {
        draw_status();
    }
    if (cinfo.width) {
        return cinfo.len;
    }
    return len;
}

void read_terminal () {
    static char buf[256];
    int nread = read(tty, buf, sizeof buf);
    if (nread < 0 && (errno == EAGAIN || errno == EINTR)) {
        return;
    }
    if (nread < 0) {
        perror("read");
        exit(1);
    }
    if (nread == 0) {
        exit(1);
    }
    int i;
    for (i = 0; i < nread;) {
        int plen = read_key(buf + i, nread - i);
        i += plen;
    }
}

void read_loop () {
    fd_set fds;
    int i, nfds, retval;
    for (i = 0;; i++) {
        FD_ZERO(&fds);
        FD_SET(tty, &fds);
        if (tabb->loaded) {
            nfds = tty + 1;
        }
        else {
            FD_SET(tabb->fd, &fds);
            nfds = tty > tabb->fd ? (tty + 1) : (tabb->fd + 1);
        }
        retval = select(nfds, &fds, NULL, NULL, NULL);
        if (retval < 0 && (errno == EAGAIN || errno == EINTR)) {
            continue;
        }
        if (retval < 0) {
            perror("select");
            exit(1);
        }
        if (FD_ISSET(tabb->fd, &fds)) {
            read_file();
        }
        if (FD_ISSET(tty, &fds)) {
            read_terminal();
        }
    }
}

void usage () {
    printf(
        "Usage: les [-hw] [-e=encoding] [-t=width] file...\n"
        "\n"
        "Options:\n"
        "    -e=encoding   input file encoding\n"
        "    -h            help\n"
        "    -t=width      tab width (default 4)\n"
        "    -w            disable line wrap\n"
        "\n"
        "Key Binds:\n"
        "    d             go down half a screen\n"
        "    D             go down a screen\n"
        "    g             go to the top of the file\n"
        "    G             go to the bottom of the file\n"
        "    h,←           go left 4 spaced\n"
        "    H             go left half a screen\n"
        "    j,↓           go down one line\n"
        "    k,↑           go up one line\n"
        "    l,→           go right 4 spaces\n"
        "    L             go right half a screen\n"
        "    q             quit\n"
        "    t             go to next tab\n"
        "    T             go to previous tab\n"
        "    u             go up half a screen\n"
        "    U             go up a screen\n"
        "    w             toggle line wrap\n"
    );
}

void set_input_encoding (char *encoding) {
    input_encoding = encoding;
    cd = iconv_open("UTF-8", input_encoding);
    if (cd == (iconv_t) -1) {
        fprintf(stderr, "Invalid encoding \"%s\"\n", encoding);
        exit(1);
    }
}

void parse_args (int argc, char **argv) {
    struct option longopts[] = {
        {"e", required_argument, NULL, 'e'},
        {"h", no_argument, NULL, 'h'},
        {"t", required_argument, NULL, 't'},
        {"w", no_argument, NULL, 'w'},
        {NULL, 0, NULL, 0}
    };

    int c;
    while ((c = getopt_long_only(argc, argv, "", longopts, NULL)) != -1) {
        switch (c) {
        case 'e':
            set_input_encoding(optarg);
            break;
        case 'h':
            usage();
            exit(0);
        case 't':
            tab_width = atoi(optarg);
            break;
        case 'w':
            line_wrap = 0;
            break;
        default:
            usage();
            exit(1);
        }
    }

    int error = 0;
    int i;
    for (i = optind; i < argc; i++) {
        int retval;
        if (isdir(argv[i])) {
            retval = add_tab_dir(argv[i]);
        }
        else {
            retval = add_tab_file(argv[i]);
        }
        if (!retval) {
            error++;
        }
    }
    if (error) {
        exit(1);
    }
    if (!tabs_len) {
        fprintf(stderr, "No files\n");
        exit(1);
    }
}

void signal2 (int sig, void (*func)(int)) {
    struct sigaction *sa = calloc(1, sizeof (struct sigaction));
    sa->sa_handler = func;
    sigaction(sig, sa, 0);
}

int main (int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (!isatty(0)) {
        add_tab("stdin", 0);
    }

    parse_args(argc, argv);
    tabb = tabs[current_tab];

    int retval = 0;
    char *term = getenv("TERM");
    if (setupterm(term, 1, &retval) < 0) {
        fprintf(stderr, "Can't find \"%s\" in the terminfo database.\n", term);
        exit(1);
    }

    atexit(bye);
    signal2(SIGINT, interrupt);
    signal2(SIGQUIT, bye2);
    signal2(SIGCONT, cont);
    signal2(SIGWINCH, winch);

    printf("%s", enter_ca_mode);
    printf("%s", cursor_invisible);

    tty = open("/dev/tty", O_RDONLY);
    tcgetattr(tty, &tcattr1);
    set_tcattr();

    status_buf_size = columns * 6;
    status_buf_len = 0;
    status_buf = malloc(status_buf_size);

    line1 = tabs_len == 1 ? 0 : 1;
    printf("%s", tparm(change_scroll_region, line1, lines - 2));

    draw_tabs();
    draw_tab();

    read_loop();
    return 0;
}
