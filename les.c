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
#include <time.h>

typedef struct {
    const char *name;
    int fd;
    char *buf;
    size_t buf_size;
    size_t buf_len;
    size_t pos;
    int loaded;
    int screen_filled;
} tab_t;

size_t tabs_len;
size_t tabs_size;
tab_t **tabs;
int current_tab = 0;
int tty;
int line_wrap = 1;

void bye () {
    printf("%s", cursor_normal);
    printf("%s", exit_ca_mode);
    exit(0);
}

void add_tab (const char *name, int fd) {
    tab_t *tabb = malloc(sizeof (tab_t));
    tabb->name = name;
    tabb->fd = fd;
    tabb->pos = 0;
    tabb->loaded = 0;
    tabb->buf = NULL;
    tabb->buf_len = 0;
    tabb->buf_size = 0;
    tabb->screen_filled = 0;
    if (tabs_len == tabs_size) {
        tabs_size *= 2;
        tabs = realloc(tabs, tabs_size * sizeof (tab_t *));
    }
    tabs[tabs_len] = tabb;
    tabs_len++;
}

void print_tabs () {
    if (tabs_len == 1) {
        return;
    }
    char *str = tparm(cursor_address, 0, 0);
    printf("%s", str);
    str = tparm(set_a_background, 232 + 2);
    printf("%s", str);
    int i;
    for (i = 0; i < tabs_len; i++) {
        if (i == current_tab) {
            printf("%s", enter_bold_mode);
            printf("%s", tabs[i]->name);
            printf("%s", exit_attribute_mode);
            str = tparm(set_a_background, 232 + 2);
            printf("%s", str);
        }
        else {
            printf("%s", tabs[i]->name);
        }
        if (i < tabs_len - 1) {
            printf("  ");
        }
    }
    printf("%s", exit_attribute_mode);
    printf("\n");
}

void print_tab () {
    int line1 = 1;
    if (tabs_len == 1) {
        line1 = 0;
    }
    char *str = tparm(cursor_address, line1, 0);
    printf("%s", str);
    tab_t *tabb = tabs[current_tab];
    int i;
    int line = 0;
    int column = 0;
    for (i = tabb->pos; i < tabb->buf_len; i++) {
        char c = tabb->buf[i];
        if (c != '\n' && column < columns) {
            printf("%c", c);
        }
        if (c == '\n' || i == tabb->buf_len - 1) {
            printf("\n");
            column = 0;
            line++;
            if (line == lines - line1 - 1) {
                if (c == '\n' || column >= columns) {
                    tabb->screen_filled = 1;
                    break;
                }
            }
            continue;
        }
        column++;
    }
    for (i = line; i < lines - line1 - 1; i++) {
        printf("%s\n", clr_eol);
    }
    printf("Hello %ld", time(NULL));
}

// Read from the file into the current tab's buffer. Encodes content
// as utf-8 if needed.
void read_file () {
    tab_t *tabb = tabs[current_tab];
    if (tabb->buf_size - tabb->buf_len < 1024) {
        if (tabb->buf_size == 0) {
            tabb->buf_size = 1024;
        }
        else {
            tabb->buf_size *= 2;
        }
        tabb->buf = realloc(tabb->buf, tabb->buf_size);
    }
    int retval = read(tabb->fd, tabb->buf + tabb->buf_len, 1024);
    if (retval < 0 && errno != EAGAIN) {
        perror("read");
        exit(1);
    }
    if (retval == 0) {
        tabb->loaded = 1;
        if (tabb->fd) {
            close(tabb->fd);
        }
    }
    else {
        tabb->buf_len += retval;
        if (!tabb->screen_filled) {
            print_tab();
        }
    }
}

void read_loop () {
    fd_set fds;
    tab_t *tabb;
    int nfds;
    int i;
    char buf[256];
    for (i = 0;; i++) {
        tabb = tabs[current_tab];
        FD_ZERO(&fds);
        FD_SET(tty, &fds);
        if (tabb->loaded) {
            nfds = tty + 1;
        }
        else {
            FD_SET(tabb->fd, &fds);
            nfds = tty > tabb->fd ? (tty + 1) : (tabb->fd + 1);
        }
        if (select(nfds, &fds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }
        if (FD_ISSET(tabb->fd, &fds)) {
            read_file();
        }
        if (FD_ISSET(tty, &fds)) {
            int retval = read(tty, buf, sizeof buf);
            if (retval < 0 && errno != EAGAIN) {
                perror("read");
                exit(1);
            }
            if (retval == 0) {
                bye();
            }
            else {
                bye();
                printf("Keyboard input is: %.*s\n", retval, buf);
            }
        }
    }
}

int main (int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    tabs_len = 0;
    tabs_size = 4;
    tabs = calloc(tabs_size, sizeof (tab_t *));

    int error = 0;
    if (!isatty(0)) {
        add_tab("stdin", 0);
    }
    int i;
    for (i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Can't open %s: %s\n", argv[i], strerror(errno));
            error = 1;
            continue;
        }
        add_tab(argv[i], fd);
    }

    if (error) {
        exit(1);
    }
    if (!tabs_len) {
        fprintf(stderr, "No files\n");
        exit(1);
    }

    int retval = 0;
    char *term = getenv("TERM");
    if (setupterm(term, 1, &retval) < 0) {
        fprintf(stderr, "Can't find \"%s\" in the terminfo database.\n", term);
        exit(1);
    }

    atexit(bye);
    signal(SIGINT, bye);
    printf("%s", enter_ca_mode);
    // printf("%s", cursor_invisible);

    print_tabs();
    print_tab();

    tty = open("/dev/tty", O_RDONLY);

    read_loop();
    return 0;
}

