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
int tty;
struct termios tcattr1, tcattr2;
int line1;
tline_t *tlines = NULL;
size_t tlines_len = 0;
size_t tlines_size = 0;
int tab_width = 4;
char readbuf[8192];

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
} status_t;

status_t *status = NULL;

#define READY  0
#define OPENED 1
#define LOADED 2

void bye () {
    tcsetattr(tty, TCSANOW, &tcattr1);
    stage_cat(tparm(change_scroll_region, 0, lines - 1));
    stage_cat(cursor_normal);
    stage_cat(exit_ca_mode);
    stage_write();
}

void bye2 () {
    exit(1);
}

void interrupt () {
    if (pr) {
        prompt_cancel = 1;
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

void tstp () {
    bye();
    kill(0, SIGSTOP);
}

void cont () {
    set_tcattr();
    stage_cat(enter_ca_mode);
    stage_cat(cursor_invisible);
    stage_cat(tparm(change_scroll_region, line1, lines - 2));
    if (pr) {
        draw_prompt();
    }
    else {
        draw_tab();
    }
}

void add_tab (const char *name, int fd, int state) {
    tabb = malloc(sizeof (tab_t));
    tabb->name = name;
    tabb->name_width = strwidth(name);
    tabb->name2 = strdup(name);
    tabb->fd = fd;
    tabb->pos = 0;
    tabb->state = state;
    tabb->buf_size = 8192;
    tabb->buf = malloc(tabb->buf_size);
    tabb->buf_len = 0;
    tabb->stragglers = malloc(16);
    tabb->stragglers_len = 0;
    tabb->line = 1;
    tabb->nlines = 0;
    tabb->column = 0;
    tabb->mesg = NULL;
    tabb->mesg_len = 0;
    tabb->mesg_size = 0;
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

void stage_tabs () {
    if (tabs_len == 1) {
        return;
    }
    generate_tab_names();
    stage_cat(tparm(cursor_address, 0, 0));
    stage_cat(tparm(set_a_background, 232 + 2));
    int i;
    int width = 0;
    for (i = 0; i < tabs_len; i++) {
        char *name = tabs[i]->name2;
        int width2 = strwidth(name);
        if (width + width2 > columns) {
            break;
        }
        if (i == current_tab) {
            stage_cat(enter_bold_mode);
            stage_cat(name);
            stage_cat(exit_attribute_mode);
            stage_cat(tparm(set_a_background, 232 + 2));
        }
        else {
            stage_cat(name);
        }
        width += width2;
        if (width + 2 > columns) {
            break;
        }
        if (i < tabs_len - 1) {
            stage_cat("  ");
        }
    }
    for (i = 0; i < columns - width; i++) {
        stage_cat(" ");
    }
    stage_cat(exit_attribute_mode);
    stage_cat("\n");
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

void stage_status () {
    char *hrsize = human_readable(tabb->buf_len);
    status->right_len = snprintf(
        status->right, status->right_size,
        "%.*s %lu -> %lu %d/%d %s",
	(int) status->tty_len, status->tty, tabb->pos, tlines[tlines_len - 1].end_pos,
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
        cinfo->width = 4;
        cinfo->len = 1;
        stage_cat("<08>");
    }
    else if (buf[i - 1] == '_') {
        get_char_info(cinfo, buf, i + 1);
        stage_cat("\b");
        stage_cat(enter_underline_mode);
        stage_ncat(buf + i + 1, cinfo->len);
        stage_cat(exit_underline_mode);
        cinfo->len += 1;
    }
    else if (buf[i - 1] == buf[i + 1]) {
        get_char_info(cinfo, buf, i + 1);
        stage_cat("\b");
        stage_cat(enter_bold_mode);
        stage_ncat(buf + i + 1, cinfo->len);
        stage_cat(exit_attribute_mode);
        cinfo->len += 1;
    }
    else {
        cinfo->width = 4;
        cinfo->len = 1;
        stage_cat("<08>");
    }
}

void stage_character (charinfo_t *cinfo, char *buf, int i) {
    static char str[16];
    int j;
    unsigned char c = buf[i];
    if (cinfo->error) {
        str[0] = '<';
        for (j = 0; j < cinfo->len; j++) {
            unsigned char c2 = buf[i + j];
            sprintf(str + 1 + j * 2, "%02x", c2);
        }
        str[j * 2 + 1] = '>';
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
        sprintf(str, "<%02x>", c);
        stage_cat(str);
    }
    else {
        stage_ncat(buf + i, cinfo->len);
    }
}

void stage_line_wrap (tline_t *tline) {
    charinfo_t cinfo;
    int i;
    int width = 0;
    for (i = tline->pos; i < tline->end_pos;) {
        get_char_info(&cinfo, tabb->buf, i);
        if (tabb->buf[i] == '\r' || tabb->buf[i] == '\n') {
            break;
        }
        stage_character(&cinfo, tabb->buf, i);
        width += cinfo.width;
        i += cinfo.len;
    }
    if (width < columns) {
        stage_cat(clr_eol);
    }
}

void stage_line_nowrap (tline_t *tline) {
    int i;
    int e = 0;
    int width = 0;
    charinfo_t cinfo;
    for (i = tline->pos; i < tline->end_pos;) {
        get_char_info(&cinfo, tabb->buf, i);
        if (width >= tabb->column && (!tabb->column || cinfo.width)) {
            break;
        }
        if (tabb->buf[i] == 0x1b && cinfo.len > 1) {
            e++;
            stage_ncat(tabb->buf + i, cinfo.len);
        }
        width += cinfo.width;
        i += cinfo.len;
    }
    if (i == tline->end_pos) {
        stage_cat(clr_eol);
        return;
    }
    for (; i < tline->end_pos;) {
        get_char_info(&cinfo, tabb->buf, i);
        if (width + cinfo.width > columns + tabb->column) {
            break;
        }
        if (tabb->buf[i] == 0x1b) {
            e++;
        }
        if (tabb->buf[i] == '\r' || tabb->buf[i] == '\n') {
            break;
        }
        stage_character(&cinfo, tabb->buf, i);
        width += cinfo.width;
        i += cinfo.len;
    }
    if (width < columns + tabb->column) {
        stage_cat(clr_eol);
    }
    if (e) {
        stage_cat(exit_attribute_mode);
    }
}

void stage_tab2 (int n, tline_t *tlines, size_t tlines_len) {
    int i;
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

void winch () {
    struct winsize w;
    ioctl(tty, TIOCGWINSZ, &w);
    lines = w.ws_row;
    columns = w.ws_col;
    if (pr) {
        draw_prompt();
    }
    else {
        stage_tabs();
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
    count_lines(tabb->buf + tabb->buf_len - i, i);
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
        int errno2 = errno;
        tabb->mesg_size = 256;
        tabb->mesg = malloc(tabb->mesg_size);
        tabb->mesg_len = snprintf(tabb->mesg, tabb->mesg_size, " [%s]", strerror(errno2));
        tabb->state = LOADED;
        draw_status();
        return;
    }
    if (nread == 0) {
        tabb->state = LOADED;
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
    read_file2(readbuf, nread);
    if (tlines_len == lines - line1 - 1) {
         draw_status();
    }
    else {
        draw_tab();
    }
}

int open_with_lesopen () {
    char *lesopen = getenv("LESOPEN");
    if (!lesopen) {
        lesopen = PREFIX "/share/les/lesopen";
    }
    if (strcmp(lesopen, "") == 0) {
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
        execlp(lesopen, lesopen, tabb->name, NULL);
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
    tabb->state = OPENED;
    return 1;
}

void open_tab_file () {
    if (tabb->state != READY) {
        return;
    }
    if (open_with_lesopen()) {
        return;
    }

    int fd = open(tabb->name, O_RDONLY);
    if (fd < 0) {
        int errno2 = errno;
        tabb->mesg_size = 256;
        tabb->mesg = malloc(tabb->mesg_size);
        tabb->mesg_len = snprintf(tabb->mesg, tabb->mesg_size, " [%s]", strerror(errno2));
        tabb->state = LOADED;
        return;
    }
    tabb->fd = fd;
    tabb->state = OPENED;
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

void next_tab () {
    current_tab = (current_tab + 1) % tabs_len;
    tabb = tabs[current_tab];
    open_tab_file();
    stage_tabs();
    draw_tab();
}

void prev_tab () {
    current_tab = current_tab > 0 ? (current_tab - 1) : (tabs_len - 1);
    tabb = tabs[current_tab];
    open_tab_file();
    stage_tabs();
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
    if (tabb2->state == OPENED && tabb2->fd) {
        close(tabb2->fd);
    }
    free(tabb2);

    if (tabs_len == 1) {
        line1 = 0;
        stage_cat(tparm(change_scroll_region, line1, lines - 1));
    }

    open_tab_file();
    stage_tabs();
    draw_tab();
}

int read_key (char *buf, int len) {
    charinfo_t cinfo;
    get_char_info(&cinfo, buf, 0);
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
            move_left(columns / 3);
            break;
        case 'H':
            move_line_left();
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
            move_right(columns / 3);
            break;
        case 'L':
            move_line_right();
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
            stage_tabs();
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
        move_left(columns / 3);
    }
    else if (strncmp(buf, "\eb", 2) == 0) {
        move_line_left();
    }
    else if (strncmp(buf, "\e[C", 3) == 0) {
        move_right(columns / 3);
    }
    else if (strncmp(buf, "\ef", 2) == 0) {
        move_line_right();
    }
    else if (strncmp(buf, "\e[H", 3) == 0) {
        move_line_left();
    }
    else if (strncmp(buf, "\e[F", 3) == 0) {
        move_line_right();
    }
    else if (strncmp(buf, "\e[5~", 4) == 0) {
        move_backward(lines - line1 - 2);
    }
    else if (strncmp(buf, "\e[6~", 4) == 0) {
        move_forward(lines - line1 - 2);
    }
    else {
        draw_status();
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
        if (tabb->state == OPENED) {
            FD_SET(tabb->fd, &fds);
            nfds = tty > tabb->fd ? (tty + 1) : (tabb->fd + 1);
        }
        else {
            nfds = tty + 1;
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
    stage_cat(
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
        "    D,⇟           go down a screen\n"
        "    g             go to the top of the file\n"
        "    G             go to the bottom of the file\n"
        "    h,←           go left one third a screen\n"
        "    H,⇤           go left all the way\n"
        "    j,↓           go down one line\n"
        "    k,↑           go up one line\n"
        "    l,→           go right one third a screen\n"
        "    L,⇥           go right all the way\n"
        "    q             quit\n"
        "    t             go to next tab\n"
        "    T             go to previous tab\n"
        "    u             go up half a screen\n"
        "    U,⇞           go up a screen\n"
        "    w             toggle line wrap\n"
    );
    stage_write();
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

    int i;
    for (i = optind; i < argc; i++) {
        add_tab(argv[i], 0, READY);
    }
    if (!tabs_len) {
        fprintf(stderr, "No files\n");
        exit(1);
    }
}

// sigaction is used because I want there to be a read() call
// blocking till it has input, I hit ^C, the read call should then
// return EINTR, check values, then resume. The standard signal()
// function will return from the interrupt handler to the same read
// call, with no chance to affect it.
void signal2 (int sig, void (*func)(int)) {
    struct sigaction *sa = calloc(1, sizeof (struct sigaction));
    sa->sa_handler = func;
    sigaction(sig, sa, 0);
}

void init_status () {
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
    status->tty_size = 1024;
    status->tty_len = 0;
    status->tty = malloc(status->tty_size);
}

int main (int argc, char **argv) {
    if (!isatty(0)) {
        add_tab("stdin", 0, OPENED);
    }

    stage_init();
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
    signal2(SIGTERM, bye2);
    signal2(SIGQUIT, bye2);
    signal2(SIGCONT, cont);
    signal2(SIGTSTP, tstp);
    signal2(SIGWINCH, winch);

    tty = open("/dev/tty", O_RDONLY);
    tcgetattr(tty, &tcattr1);
    set_tcattr();

    line1 = tabs_len == 1 ? 0 : 1;

    stage_cat(enter_ca_mode);
    stage_cat(cursor_invisible);
    stage_cat(tparm(change_scroll_region, line1, lines - 2));
    stage_write();

    init_status();
    open_tab_file();
    stage_tabs();
    draw_tab();

    read_loop();
    return 0;
}

