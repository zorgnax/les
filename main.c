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
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

int line_wrap = 1;
int tty;
struct termios tcattr1, tcattr2;
int line1;
int tab_width = 4;
int interrupt = 0;

void reset () {
    tcsetattr(tty, TCSANOW, &tcattr1);
    stage_cat(tparm(change_scroll_region, 0, lines - 1));
    stage_cat(cursor_normal);
    stage_cat(keypad_local);
    stage_cat(exit_ca_mode);
    stage_write();
}

void bye () {
    reset();
    save_recents_file();
}

void bye2 () {
    exit(1);
}

void sigint () {
    if (pr) {
        interrupt = 1;
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

void sigchld () {
    int status;
    int cpid = wait3(&status, WNOHANG, NULL);
}

void sigtstp () {
    reset();
    kill(0, SIGSTOP);
}

void sigcont () {
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

void sigwinch () {
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

void save_mark () {
    tabb->state |= MARKED;
    tabb->mark = tabb->pos;
}

void restore_mark () {
    if (!(tabb->state & MARKED)) {
        return;
    }
    move_pos(tabb->mark);
}

char *usage_text () {
    static char *str =
        "Usage: les [-hw] [-e=encoding] [-p=script] [-t=width] file...\n"
        "\n"
        "Options:\n"
        "    -e=encoding   input file encoding\n"
        "    -h            help\n"
        "    -p=script     lespipe script\n"
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
        "    m             mark position\n"
        "    M             restore marked position\n"
        "    q             close file\n"
        "    Q             close all files\n"
        "    t             go to next tab\n"
        "    T             go to previous tab\n"
        "    u             go up half a screen\n"
        "    U,⇞           go up a screen\n"
        "    w             toggle line wrap\n"
        "    F1            view help\n"
        "    F2            view recently opened files\n";
    return str;
}

void usage () {
    stage_cat(usage_text());
    stage_write();
}

void add_help_tab () {
    int i;
    if (tabb->state & HELP) {
        close_tab();
        return;
    }
    for (i = 0; i < tabs_len; i++) {
        if (tabs[i]->state & HELP) {
            select_tab(i);
            stage_tabs();
            draw_tab();
            return;
        }
    }

    add_tab("[Help]", 0, LOADED|HELP);
    select_tab(tabs_len - 1);

    char *str = usage_text();
    int len = strlen(str);
    if (tabb->buf_size < len + 1) {
        tabb->buf_size = len + 1;
        tabb->buf = realloc(tabb->buf, tabb->buf_size);
    }
    strcpy(tabb->buf, str);
    tabb->buf_len = len;

    init_line1();
    stage_tabs();
    move_end();
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
        case 'm':
            save_mark();
            break;
        case 'M':
            restore_mark();
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
        case '%':
            printf("%.2f%% \n", (double) tabb->pos / tabb->buf_len * 100);
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
    if (len == 1) {
        draw_status();
        return 1;
    }
    if (strncmp(buf, "\eOB", 3) == 0) { // down
        move_forward(1);
    }
    else if (strncmp(buf, "\eOA", 3) == 0) { // up
        move_backward(1);
    }
    else if (strncmp(buf, "\eOD", 3) == 0) { // left
        move_left(columns / 3);
    }
    else if (strncmp(buf, "\eb", 2) == 0) { // alt-left
        move_line_left();
    }
    else if (strncmp(buf, "\eOC", 3) == 0) { // right
        move_right(columns / 3);
    }
    else if (strncmp(buf, "\ef", 2) == 0) { // alt-right
        move_line_right();
    }
    else if (strncmp(buf, "\eOH", 3) == 0) { // home
        move_line_left();
    }
    else if (strncmp(buf, "\eOF", 3) == 0) { // end
        move_line_right();
    }
    else if (strncmp(buf, "\e[5~", 4) == 0) { // pgup
        move_backward(lines - line1 - 2);
    }
    else if (strncmp(buf, "\e[6~", 4) == 0) { // pgdn
        move_forward(lines - line1 - 2);
    }
    else if (strncmp(buf, "\eOP", 3) == 0) { // F1
        add_help_tab();
    }
    else if (strncmp(buf, "\eOQ", 3) == 0) { // F2
        add_recents_tab();
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
        if ((tabb->state & (OPENED|LOADED)) == OPENED) {
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

void parse_args (int argc, char **argv) {
    struct option longopts[] = {
        {"e", required_argument, NULL, 'e'},
        {"h", no_argument, NULL, 'h'},
        {"p", required_argument, NULL, 'p'},
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
        case 'p':
            lespipe = optarg;
            break;
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

int main (int argc, char **argv) {
    stage_init();
    int retval = 0;
    char *term = getenv("TERM");
    if (setupterm(term, 1, &retval) < 0) {
        fprintf(stderr, "Can't find \"%s\" in the terminfo database.\n", term);
        exit(1);
    }

    if (!isatty(0)) {
        add_tab("stdin", 0, OPENED);
    }

    parse_args(argc, argv);
    tabb = tabs[0];
    open_tab_file();
    if (tabb->state & ERROR) {
        write(1, tabb->buf, tabb->buf_len);
        exit(1);
    }

    atexit(bye);
    signal2(SIGINT, sigint);
    signal2(SIGTERM, bye2);
    signal2(SIGQUIT, bye2);
    signal2(SIGCONT, sigcont);
    signal2(SIGTSTP, sigtstp);
    signal2(SIGCHLD, sigchld);
    signal2(SIGWINCH, sigwinch);

    tty = open("/dev/tty", O_RDONLY);
    tcgetattr(tty, &tcattr1);
    set_tcattr();

    stage_cat(enter_ca_mode);
    stage_cat(keypad_xmit);
    stage_cat(cursor_invisible);
    init_line1();
    stage_write();

    init_status();
    stage_tabs();
    draw_tab();

    read_loop();
    return 0;
}

