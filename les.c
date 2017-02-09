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

typedef struct {
    const char *name;
    int name_width;
    char *name2;
    int fd;
    char *buf;
    size_t buf_size;
    size_t buf_len;
    char *stragglers;
    size_t stragglers_len;
    size_t pos;
    size_t end_pos;
    int loaded;
    int screen_filled;
    int line;
    int end_line;
    int nlines;
} tab_t;

typedef struct {
    int len;
    int mask;
    int error;
    unsigned int codepoint;
    int width;
} charinfo_t;

typedef struct {
    unsigned int from;
    unsigned int to;
    int width;
} width_range_t;

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

int get_char_width (unsigned int codepoint) {
    static width_range_t width_ranges[] = {
        {0x7f,    0x9f,    0},
        {0x300,   0x36f,   0},
        {0x483,   0x489,   0},
        {0x591,   0x5bd,   0},
        {0x5bf,   0x5bf,   0},
        {0x5c1,   0x5c2,   0},
        {0x5c4,   0x5c5,   0},
        {0x5c7,   0x5c7,   0},
        {0x610,   0x61a,   0},
        {0x64b,   0x65f,   0},
        {0x670,   0x670,   0},
        {0x6d6,   0x6dc,   0},
        {0x6df,   0x6e4,   0},
        {0x6e7,   0x6e8,   0},
        {0x6ea,   0x6ed,   0},
        {0x70f,   0x70f,   0},
        {0x711,   0x711,   0},
        {0x730,   0x74a,   0},
        {0x7a6,   0x7b0,   0},
        {0x7eb,   0x7f3,   0},
        {0x816,   0x819,   0},
        {0x81b,   0x823,   0},
        {0x825,   0x827,   0},
        {0x829,   0x82d,   0},
        {0x859,   0x85b,   0},
        {0x8e3,   0x903,   0},
        {0x93a,   0x93c,   0},
        {0x93e,   0x94f,   0},
        {0x951,   0x957,   0},
        {0x962,   0x963,   0},
        {0x981,   0x983,   0},
        {0x9bc,   0x9bc,   0},
        {0x9be,   0x9c4,   0},
        {0x9c7,   0x9c8,   0},
        {0x9cb,   0x9cd,   0},
        {0x9d7,   0x9d7,   0},
        {0x9e2,   0x9e3,   0},
        {0xa01,   0xa03,   0},
        {0xa3c,   0xa3c,   0},
        {0xa3e,   0xa42,   0},
        {0xa47,   0xa48,   0},
        {0xa4b,   0xa4d,   0},
        {0xa51,   0xa51,   0},
        {0xa70,   0xa71,   0},
        {0xa75,   0xa75,   0},
        {0xa81,   0xa83,   0},
        {0xabc,   0xabc,   0},
        {0xabe,   0xac5,   0},
        {0xac7,   0xac9,   0},
        {0xacb,   0xacd,   0},
        {0xae2,   0xae3,   0},
        {0xb01,   0xb03,   0},
        {0xb3c,   0xb3c,   0},
        {0xb3e,   0xb44,   0},
        {0xb47,   0xb48,   0},
        {0xb4b,   0xb4d,   0},
        {0xb56,   0xb57,   0},
        {0xb62,   0xb63,   0},
        {0xb82,   0xb82,   0},
        {0xbbe,   0xbc2,   0},
        {0xbc6,   0xbc8,   0},
        {0xbca,   0xbcd,   0},
        {0xbd7,   0xbd7,   0},
        {0xc00,   0xc03,   0},
        {0xc3e,   0xc44,   0},
        {0xc46,   0xc48,   0},
        {0xc4a,   0xc4d,   0},
        {0xc55,   0xc56,   0},
        {0xc62,   0xc63,   0},
        {0xc81,   0xc83,   0},
        {0xcbc,   0xcbc,   0},
        {0xcbe,   0xcc4,   0},
        {0xcc6,   0xcc8,   0},
        {0xcca,   0xccd,   0},
        {0xcd5,   0xcd6,   0},
        {0xce2,   0xce3,   0},
        {0xd01,   0xd03,   0},
        {0xd3e,   0xd44,   0},
        {0xd46,   0xd48,   0},
        {0xd4a,   0xd4d,   0},
        {0xd57,   0xd57,   0},
        {0xd62,   0xd63,   0},
        {0xd82,   0xd83,   0},
        {0xdca,   0xdca,   0},
        {0xdcf,   0xdd4,   0},
        {0xdd6,   0xdd6,   0},
        {0xdd8,   0xddf,   0},
        {0xdf2,   0xdf3,   0},
        {0xe31,   0xe31,   0},
        {0xe34,   0xe3a,   0},
        {0xe47,   0xe4e,   0},
        {0xeb1,   0xeb1,   0},
        {0xeb4,   0xeb9,   0},
        {0xebb,   0xebc,   0},
        {0xec8,   0xecd,   0},
        {0xf18,   0xf19,   0},
        {0xf35,   0xf35,   0},
        {0xf37,   0xf37,   0},
        {0xf39,   0xf39,   0},
        {0xf3e,   0xf3f,   0},
        {0xf71,   0xf84,   0},
        {0xf86,   0xf87,   0},
        {0xf8d,   0xf97,   0},
        {0xf99,   0xfbc,   0},
        {0xfc6,   0xfc6,   0},
        {0x102b,  0x103e,  0},
        {0x1056,  0x1059,  0},
        {0x105e,  0x1060,  0},
        {0x1062,  0x1064,  0},
        {0x1067,  0x106d,  0},
        {0x1071,  0x1074,  0},
        {0x1082,  0x108d,  0},
        {0x108f,  0x108f,  0},
        {0x109a,  0x109d,  0},
        {0x1100,  0x115f,  2},
        {0x135d,  0x135f,  0},
        {0x1712,  0x1714,  0},
        {0x1732,  0x1734,  0},
        {0x1752,  0x1753,  0},
        {0x1772,  0x1773,  0},
        {0x17b4,  0x17d3,  0},
        {0x17dd,  0x17dd,  0},
        {0x180b,  0x180d,  0},
        {0x180b,  0x180e,  0},
        {0x18a9,  0x18a9,  0},
        {0x1920,  0x192b,  0},
        {0x1930,  0x193b,  0},
        {0x1a17,  0x1a1b,  0},
        {0x1a55,  0x1a5e,  0},
        {0x1a60,  0x1a7c,  0},
        {0x1a7f,  0x1a7f,  0},
        {0x1ab0,  0x1abe,  0},
        {0x1b00,  0x1b04,  0},
        {0x1b34,  0x1b44,  0},
        {0x1b6b,  0x1b73,  0},
        {0x1b80,  0x1b82,  0},
        {0x1ba1,  0x1bad,  0},
        {0x1be6,  0x1bf3,  0},
        {0x1c24,  0x1c37,  0},
        {0x1cd0,  0x1cd2,  0},
        {0x1cd4,  0x1ce8,  0},
        {0x1ced,  0x1ced,  0},
        {0x1cf2,  0x1cf4,  0},
        {0x1cf8,  0x1cf9,  0},
        {0x1dc0,  0x1df5,  0},
        {0x1dfc,  0x1dff,  0},
        {0x200b,  0x200f,  0},
        {0x202a,  0x202e,  0},
        {0x206a,  0x206f,  0},
        {0x20d0,  0x20f0,  0},
        {0x2329,  0x232a,  2},
        {0x2cef,  0x2cf1,  0},
        {0x2d7f,  0x2d7f,  0},
        {0x2de0,  0x2dff,  0},
        {0x2e80,  0x2e99,  2},
        {0x2e9b,  0x2ef3,  2},
        {0x2f00,  0x2fd5,  2},
        {0x2ff0,  0x2ffb,  2},
        {0x3000,  0x303e,  2},
        {0x302a,  0x302f,  0},
        {0x3041,  0x3096,  2},
        {0x3099,  0x30ff,  2},
        {0x3099,  0x309a,  0},
        {0x3105,  0x312d,  2},
        {0x3131,  0x318e,  2},
        {0x3190,  0x31ba,  2},
        {0x31c0,  0x31e3,  2},
        {0x31f0,  0x321e,  2},
        {0x3220,  0x3247,  2},
        {0x3250,  0x32fe,  2},
        {0x3300,  0x4dbf,  2},
        {0x4e00,  0xa48c,  2},
        {0xa490,  0xa4c6,  2},
        {0xa66f,  0xa672,  0},
        {0xa674,  0xa67d,  0},
        {0xa69e,  0xa69f,  0},
        {0xa6f0,  0xa6f1,  0},
        {0xa802,  0xa802,  0},
        {0xa806,  0xa806,  0},
        {0xa80b,  0xa80b,  0},
        {0xa823,  0xa827,  0},
        {0xa880,  0xa881,  0},
        {0xa8b4,  0xa8c4,  0},
        {0xa8e0,  0xa8f1,  0},
        {0xa926,  0xa92d,  0},
        {0xa947,  0xa953,  0},
        {0xa960,  0xa97c,  2},
        {0xa980,  0xa983,  0},
        {0xa9b3,  0xa9c0,  0},
        {0xa9e5,  0xa9e5,  0},
        {0xaa29,  0xaa36,  0},
        {0xaa43,  0xaa43,  0},
        {0xaa4c,  0xaa4d,  0},
        {0xaa7b,  0xaa7d,  0},
        {0xaab0,  0xaab0,  0},
        {0xaab2,  0xaab4,  0},
        {0xaab7,  0xaab8,  0},
        {0xaabe,  0xaabf,  0},
        {0xaac1,  0xaac1,  0},
        {0xaaeb,  0xaaef,  0},
        {0xaaf5,  0xaaf6,  0},
        {0xabe3,  0xabea,  0},
        {0xabec,  0xabed,  0},
        {0xac00,  0xd7a3,  2},
        {0xd800,  0xdfff,  0},
        {0xf900,  0xfaff,  2},
        {0xfb1e,  0xfb1e,  0},
        {0xfe00,  0xfe0f,  0},
        {0xfe10,  0xfe19,  2},
        {0xfe20,  0xfe2f,  0},
        {0xfe30,  0xfe52,  2},
        {0xfe54,  0xfe66,  2},
        {0xfe68,  0xfe6b,  2},
        {0xfeff,  0xfeff,  0},
        {0xff01,  0xff60,  2},
        {0xffe0,  0xffe6,  2},
        {0xfff9,  0xfffb,  0},
        {0xfffe,  0xffff,  0},
        {0x101fd, 0x101fd, 0},
        {0x102e0, 0x102e0, 0},
        {0x10376, 0x1037a, 0},
        {0x10a01, 0x10a03, 0},
        {0x10a05, 0x10a06, 0},
        {0x10a0c, 0x10a0f, 0},
        {0x10a38, 0x10a3a, 0},
        {0x10a3f, 0x10a3f, 0},
        {0x10ae5, 0x10ae6, 0},
        {0x11000, 0x11002, 0},
        {0x11038, 0x11046, 0},
        {0x1107f, 0x11082, 0},
        {0x110b0, 0x110ba, 0},
        {0x11100, 0x11102, 0},
        {0x11127, 0x11134, 0},
        {0x11173, 0x11173, 0},
        {0x11180, 0x11182, 0},
        {0x111b3, 0x111c0, 0},
        {0x111ca, 0x111cc, 0},
        {0x1122c, 0x11237, 0},
        {0x112df, 0x112ea, 0},
        {0x11300, 0x11303, 0},
        {0x1133c, 0x1133c, 0},
        {0x1133e, 0x11344, 0},
        {0x11347, 0x11348, 0},
        {0x1134b, 0x1134d, 0},
        {0x11357, 0x11357, 0},
        {0x11362, 0x11363, 0},
        {0x11366, 0x1136c, 0},
        {0x11370, 0x11374, 0},
        {0x114b0, 0x114c3, 0},
        {0x115af, 0x115b5, 0},
        {0x115b8, 0x115c0, 0},
        {0x115dc, 0x115dd, 0},
        {0x11630, 0x11640, 0},
        {0x116ab, 0x116b7, 0},
        {0x1171d, 0x1172b, 0},
        {0x16af0, 0x16af4, 0},
        {0x16b30, 0x16b36, 0},
        {0x16f51, 0x16f7e, 0},
        {0x16f8f, 0x16f92, 0},
        {0x1b000, 0x1b001, 2},
        {0x1bc9d, 0x1bc9e, 0},
        {0x1d165, 0x1d169, 0},
        {0x1d16d, 0x1d172, 0},
        {0x1d17b, 0x1d182, 0},
        {0x1d185, 0x1d18b, 0},
        {0x1d1aa, 0x1d1ad, 0},
        {0x1d242, 0x1d244, 0},
        {0x1da00, 0x1da36, 0},
        {0x1da3b, 0x1da6c, 0},
        {0x1da75, 0x1da75, 0},
        {0x1da84, 0x1da84, 0},
        {0x1da9b, 0x1da9f, 0},
        {0x1daa1, 0x1daaf, 0},
        {0x1e8d0, 0x1e8d6, 0},
        {0x1f200, 0x1f202, 2},
        {0x1f210, 0x1f23a, 2},
        {0x1f240, 0x1f248, 2},
        {0x1f250, 0x1f251, 2},
        {0x20000, 0x2fffd, 2},
        {0x30000, 0x3fffd, 2},
        {0xe0100, 0xe01ef, 0},
    };

    if (codepoint < 0x20) {
        return 0;
    }
    else if (codepoint < 0x7f) {
        return 1;
    }

    int n = sizeof width_ranges / sizeof width_ranges[0];
    int i = 0, j = n - 1;
    while (i <= j) {
        int k = (i + j) / 2;
        if (codepoint < width_ranges[k].from) {
            j = k - 1;
        }
        else if (codepoint > width_ranges[k].to) {
            i = k + 1;
        }
        else {
            return width_ranges[k].width;
        }
    }

    return 1;
}

void get_char_info (charinfo_t *cinfo, const char *buf) {
    char c = buf[0];
    cinfo->error = 0;
    if ((c & 0x80) == 0x00) {
        cinfo->len = 1;
        cinfo->mask = 0x7f;
    }
    else if ((c & 0xe0) == 0xc0) {
        cinfo->len = 2;
        cinfo->mask = 0x1f;
    }
    else if ((c & 0xf0) == 0xe0) {
        cinfo->len = 3;
        cinfo->mask = 0x0f;
    }
    else if ((c & 0xf8) == 0xf0) {
        cinfo->len = 4;
        cinfo->mask = 0x07;
    }
    else if ((c & 0xfc) == 0xf8) {
        cinfo->len = 5;
        cinfo->mask = 0x03;
    }
    else if ((c & 0xfe) == 0xfc) {
        cinfo->len = 6;
        cinfo->mask = 0x01;
    }
    else {
        cinfo->len = 1;
        cinfo->mask = 0x00;
        cinfo->error = 1;
        return;
    }

    unsigned int codepoint = buf[0] & cinfo->mask;
    int i;
    for (i = 1; i < cinfo->len; i++) {
        codepoint <<= 6;
        codepoint |= buf[i] & 0x3f;
    }
    cinfo->codepoint = codepoint;
    cinfo->width = get_char_width(codepoint);
}

int strwidth (const char *str) {
    int i, width = 0;
    charinfo_t cinfo;
    for (i = 0; str[i];) {
        get_char_info(&cinfo, str + i);
        width += cinfo.width;
        i += cinfo.len;
    }
    return width;
}

void shorten (char *str, int n) {
    int i, j;
    int width = 0;
    charinfo_t cinfo;
    for (i = 0; str[i];) {
        get_char_info(&cinfo, str + i);
        if (width + cinfo.width > n / 2) {
            break;
        }
        width += cinfo.width;
        i += cinfo.len;
    }
    int len = strlen(str);
    for (j = strlen(str) - 1; j > i; j--) {
        if ((str[j] & 0xc0) == 0x80) {
            j--;
        }
        get_char_info(&cinfo, str + j);
        width += cinfo.width;
        if (width >= n) {
            break;
        }
    }
    if (j > i) {
        memmove(str + i, str + j, len - j + 1);
    }
}

void bye () {
    tcsetattr(tty, TCSANOW, &tcattr1);
    printf("%s", tparm(change_scroll_region, 0, lines - 1));
    printf("%s", cursor_normal);
    printf("%s", exit_ca_mode);
    exit(0);
}

void set_tcattr () {
    tcattr2 = tcattr1;
    tcattr2.c_lflag &= ~(ICANON|ECHO);
    tcattr2.c_cc[VMIN] = 1;
    tcattr2.c_cc[VTIME] = 0;
    tcsetattr(tty, TCSAFLUSH, &tcattr2);
}

void cont () {
    signal(SIGCONT, cont);
    set_tcattr();
}

void add_tab (const char *name, int fd) {
    tab_t *tabb = malloc(sizeof (tab_t));
    tabb->name = name;
    tabb->name_width = strwidth(name);
    tabb->name2 = strdup(name);
    tabb->fd = fd;
    tabb->pos = 0;
    tabb->end_pos = 0;
    tabb->loaded = 0;
    tabb->buf = NULL;
    tabb->buf_len = 0;
    tabb->buf_size = 0;
    tabb->screen_filled = 0;
    tabb->stragglers = malloc(16);
    tabb->stragglers_len = 0;
    tabb->line = 1;
    tabb->end_line = 1;
    tabb->nlines = 1;
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

void draw_tabs () {
    if (tabs_len == 1) {
        return;
    }
    generate_tab_names();
    char *str = tparm(cursor_address, 0, 0);
    printf("%s", str);
    str = tparm(set_a_background, 232 + 2);
    printf("%s", str);
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
            str = tparm(set_a_background, 232 + 2);
            printf("%s", str);
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

void draw_tab_nowrap (int n, size_t pos) {
    int i;
    int line = 0;
    int column = 0;
    charinfo_t cinfo;
    for (i = pos; i < tabb->buf_len;) {
        unsigned char c = tabb->buf[i];
        get_char_info(&cinfo, tabb->buf + i);
        if (column + cinfo.width <= columns) {
            if (cinfo.error) {
                printf("?");
            }
            else if (c != '\n') {
                printf("%.*s", cinfo.len, tabb->buf + i);
            }
        }
        i += cinfo.len;
        if (c == '\n' || i + cinfo.len == tabb->buf_len) {
            if (line == n - 1) {
                break;
            }
            if (column < columns) {
                printf("%s", clr_eol);
            }
            printf("\n");
            column = 0;
            line++;
        }
        else {
            column += cinfo.width;
        }
    }

    tabb->end_pos = i;
    tabb->end_line = line;
    if (line == n - 1) {
        tabb->screen_filled = 1;
    }
    if (column >= columns) {
        printf("\n");
        line++;
    }
    for (i = line; i < n; i++) {
        printf("%s\n", clr_eol);
    }
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

// after displaying the tab you will be in the correct position to
// write the status, otherwise you will have to call this before
// draw_status.
void position_status () {
    printf("%s", tparm(cursor_address, lines - 1, 0));
}

void draw_status () {
    static char right_buf[256];
    int retval;

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
        " %lu -> %lu %d/%d %s", tabb->pos, tabb->end_pos, tabb->line, tabb->nlines, hrsize);
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

    printf("%.*s", (int) status_buf_len, status_buf);
}

void get_wrap_lines (char *buf, size_t buf_len, size_t pos, int max,
                     size_t **wlines_ret, size_t *wlines_len_ret,
                     size_t *lines_len_ret) {
    static size_t *wlines = NULL;
    static size_t wlines_len = 0;
    static size_t wlines_size = 0;
    size_t lines_len = 0;
    int i, j, k;
    unsigned char c1, c2;
    int whitespace1, whitespace2;
    charinfo_t cinfo1, cinfo2;
    int column = 0;
    int width;

    if (!wlines_size) {
        wlines_size = 64;
        wlines = malloc(wlines_size * sizeof (size_t));
    }
    wlines[0] = pos;
    wlines_len = 1;
    if (pos == buf_len) {
        *wlines_ret = wlines;
        *wlines_len_ret = wlines_len;
        return;
    }
    for (i = pos; i < buf_len;) {
        c1 = buf[i];

        if (c1 == '\n') {
            i++;
            if (wlines_size < wlines_len + 1) {
                wlines_size *= 2;
                wlines = realloc(wlines, wlines_size * sizeof (size_t));
            }
            wlines[wlines_len] = i;
            wlines_len++;
            lines_len++;
            column = 0;
            if (max && wlines_len > max) {
                break;
            }
            continue;
        }

        // get the word starting at byte i to byte j
        get_char_info(&cinfo1, buf + i);
        whitespace1 = c1 == ' ' || c1 == '\t';
        width = cinfo1.width;
        for (j = i + cinfo1.len; j < buf_len;) {
            unsigned char c2 = buf[j];
            if (c2 == '\n') {
                break;
            }
            int whitespace2 = c2 == ' ' || c2 == '\t';
            if (whitespace1 ^ whitespace2) {
                break;
            }
            get_char_info(&cinfo2, buf + j);
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
            if (wlines_size < wlines_len + 1) {
                wlines_size *= 2;
                wlines = realloc(wlines, wlines_size * sizeof (size_t));
            }
            wlines[wlines_len] = i;
            wlines_len++;
            if (max && wlines_len > max) {
                break;
            }
            column = width;
            i = j;
            continue;
        }

	// wouldn't fit on a line by itself, so display as much as
	// you can on this line, then more on the next line
        for (k = 0; k < j - i;) {
            get_char_info(&cinfo2, buf + i + k);
            if (column + cinfo2.width > columns) {
                if (wlines_size < wlines_len + 1) {
                    wlines_size *= 2;
                    wlines = realloc(wlines, wlines_size * sizeof (size_t));
                }
                wlines[wlines_len] = i + k;
                wlines_len++;
                if (max && wlines_len > max) {
                    goto end;
                }
                column = 0;
            }
            column += cinfo2.width;
            k += cinfo2.len;
        }
        i = j;
    }

    end:
    if ((!max || wlines_len <= max) && wlines[wlines_len - 1] != buf_len) {
        if (wlines_size < wlines_len + 1) {
            wlines_size *= 2;
            wlines = realloc(wlines, wlines_size * sizeof (size_t));
        }
        wlines[wlines_len] = buf_len;
        wlines_len++;
    }
    *wlines_ret = wlines;
    *wlines_len_ret = wlines_len;
    *lines_len_ret = lines_len;
}

void draw_tab_wrap (int n, size_t pos) {
    int i;
    int len;
    size_t *wlines;
    size_t wlines_len;
    size_t lines_len;

    get_wrap_lines(tabb->buf, tabb->buf_len, pos, n, &wlines, &wlines_len, &lines_len);
    for (i = 0; i < n; i++) {
        printf("%s", clr_eol);
        if (i < wlines_len - 1) {
            len = wlines[i + 1] - wlines[i];
            printf("%.*s", len, tabb->buf + wlines[i]);
            if (!len || tabb->buf[wlines[i] + len - 1] != '\n') {
                printf("\n");
            }
        }
        else {
            printf("\n");
        }
    }
    tabb->end_pos = wlines[wlines_len - 1];
    tabb->end_line = wlines_len - 1;
    if (wlines_len - 1 == n) {
        tabb->screen_filled = 1;
    }
}

void draw_tab2 (int n, size_t pos) {
    if (line_wrap) {
        draw_tab_wrap(n, pos);
    }
    else {
        draw_tab_nowrap(n, pos);
    }
}

void draw_tab () {
    printf("%s", tparm(cursor_address, line1, 0));
    draw_tab2(lines - line1 - 1, tabb->pos);
    draw_status();
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

    if (tabb->buf_size == 0) {
        tabb->buf_size = 1024;
        tabb->buf = malloc(tabb->buf_size);
    }
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

#define UTF8_LENGTH(c)       \
    (c & 0x80) == 0x00 ? 1 : \
    (c & 0xc0) == 0x80 ? 1 : \
    (c & 0xe0) == 0xc0 ? 2 : \
    (c & 0xf0) == 0xe0 ? 3 : \
    (c & 0xf8) == 0xf0 ? 4 : \
    (c & 0xfc) == 0xf8 ? 5 : \
    (c & 0xfe) == 0xfc ? 6 : \
                         6

// makes sure buffer only contains whole utf-8 characters, if any
// are incomplete then they are stored in the stragglers array
void add_unencoded_input (char *buf, size_t buf_len) {
    if (tabb->buf_size - tabb->buf_len < buf_len) {
        if (tabb->buf_size == 0) {
            tabb->buf_size = 1024;
        }
        else {
            tabb->buf_size *= 2;
        }
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
    char buf[1024];
    if (tabb->stragglers_len) {
        memcpy(buf, tabb->stragglers, tabb->stragglers_len);
    }
    int nread = read(tabb->fd, buf + tabb->stragglers_len, sizeof buf - tabb->stragglers_len);
    if (nread < 0 && errno != EAGAIN) {
        perror("read");
        exit(1);
    }
    if (nread == 0) {
        tabb->loaded = 1;
        if (tabb->fd) {
            close(tabb->fd);
        }
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
    if (tabb->screen_filled) {
        position_status();
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
            else {
                ttybuf[ttybuf_len] = buf[i];
                ttybuf_len++;
                ttybuf_width++;
            }
        }
    }
}

int next_line (char *buf, size_t len, size_t pos, int n, int *m) {
    int i;
    int line = 0;
    *m = line;
    size_t pos2 = pos;
    for (i = pos; i < len - 1; i++) {
        if (buf[i] == '\n') {
            line++;
            *m = line;
            pos2 = i + 1;
            if (line == n) {
                return pos2;
            }
        }
    }
    return pos2;
}

// you ask for the 5th previous line but it only has 3 lines, it
// will return the location of the 3rd previous line and put 3 into
// the m pointer.
int prev_line (char *buf, size_t len, size_t pos, int n, int *m) {
    int i;
    int line = 0;
    *m = 0;
    size_t pos2 = pos;
    if (pos == 0) {
        return 0;
    }
    for (i = pos - 1; i > 0; i--) {
        if (buf[i] == '\n' || i == len - 1) {
            line++;
            pos2 = i + 1;
            if (line == n + 1) {
                *m = line - 1;
                return pos2;
            }
        }
    }
    line++;
    *m = line - 1;
    return 0;
}

void move_forward (int n) {
    int m = 0;
    if (line_wrap) {
        size_t *wlines;
        size_t wlines_len;
        size_t lines_len;
        get_wrap_lines(tabb->buf, tabb->buf_len, tabb->pos, n,
                   &wlines, &wlines_len, &lines_len);
        if (wlines[wlines_len - 1] == tabb->buf_len) {
            if (wlines_len <= 2) {
                return;
            }
            m = wlines_len - 2;
        }
        else {
            m = wlines_len - 1;
        }
        tabb->pos = wlines[m];
        tabb->line += lines_len;
    }
    else {
        int next = next_line(tabb->buf, tabb->buf_len, tabb->pos, n, &m);
        tabb->pos = next;
        tabb->line += m;
    }
    if (!m) {
        return;
    }
    if (m < lines - line1 - 1) {
        printf("%s", cursor_up);
        printf("%s", tparm(parm_index, m));
        printf("%s", tparm(cursor_address, lines - 1 - m, 0));
        int end_line = tabb->end_line;
        draw_tab2(m, tabb->end_pos);
        tabb->end_line = end_line - m + tabb->end_line;
        draw_status();
    }
    else {
        draw_tab();
    }
}

// returns the nth previous line. if word wrap is active, then those
// lines are word wrapped lines. if there aren't n lines available it
// does as much as possible. the actual number of wrapped lines
// previous is returned in m. the actual number of lines previous
// is returned in m2.
int prev_line2 (char *buf, size_t len, size_t pos, int n, int *m, int *m2) {
    int i, t;
    size_t pos2;
    *m = 0;
    *m2 = 0;
    if (pos == 0) {
        return 0;
    }
    if (line_wrap) {
        size_t *wlines;
        size_t wlines_len;
        size_t lines_len;
        if (buf[pos - 1] == '\n') {
            (*m2)++;
        }
        for (i = 0; i < n;) {
            get_wrap_lines(buf, pos, prev_line(buf, len, pos, 1, &t), 0,
                           &wlines, &wlines_len, &lines_len);
            if (wlines_len == 1) {
                break;
            }
            if (n - i <= wlines_len - 1) {
                pos = wlines[wlines_len - 1 - (n - i)];
                i += n - i;
                break;
            }
            else {
                pos = wlines[0];
                i += wlines_len - 1;
                (*m2)++;
            }
        }
        pos2 = pos;
        *m = i;
    }
    else {
        pos2 = prev_line(buf, len, pos, n, m);
        *m2 = *m;
    }
    return pos2;
}

void move_backward (int n) {
    int m, m2, m3, t, t2;
    tabb->pos = prev_line2(tabb->buf, tabb->buf_len, tabb->pos, n, &m, &m2);
    tabb->line -= m2;
    if (!m) {
        return;
    }
    if (m < lines - line1 - 1) {
        printf("%s", tparm(cursor_address, line1, 0));
        printf("%s", tparm(parm_rindex, m));
        int end_line = tabb->end_line;
        size_t end_pos = tabb->end_pos;
        draw_tab2(m, tabb->pos);
        tabb->end_line = end_line + m;
        if (tabb->end_line > lines - line1 - 1) {
            tabb->end_line = lines - line1 - 1;
        }
        tabb->end_pos = end_pos;
        m3 = m - (lines - line1 - 1 - end_line);
        if (m3 > 0) {
            tabb->end_pos = prev_line2(tabb->buf, tabb->buf_len, end_pos, m3, &t, &t2);
        }
        position_status();
        draw_status();
    }
    else {
        draw_tab();
    }
}

void move_end () {
    if (tabb->nlines < lines) {
        tabb->pos = 0;
        tabb->line = 1;
        draw_tab();
        return;
    }
    tabb->pos = tabb->buf_len;
    tabb->line = tabb->nlines;
    move_backward(lines - line1 - 1);
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
        bye();
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

int read_key (char *buf, int len) {
    charinfo_t cinfo;
    get_char_info(&cinfo, buf);
    set_ttybuf(&cinfo, buf, len);
    int extended = 0;
    switch (buf[0]) {
        case 'b':
            move_backward(lines - line1 - 2);
            break;
        case 'd':
            move_forward((lines - line1 - 1) / 2);
            break;
        case 'f':
            move_forward(lines - line1 - 2);
            break;
        case 'g':
            tabb->pos = 0;
            tabb->line = 1;
            draw_tab();
            break;
        case 'G':
            move_end();
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
        case 'q':
            close_tab();
            break;
        case 'Q':
            bye();
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
        case 'w':
            line_wrap = !line_wrap;
            draw_tab();
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
    else {
        position_status();
        draw_status();
    }
    if (cinfo.width) {
        return cinfo.len;
    }
    return len;
}

void read_terminal () {
    char buf[256];
    int nread = read(tty, buf, sizeof buf);
    if (nread < 0 && errno != EAGAIN) {
        perror("read");
        exit(1);
    }
    if (nread == 0) {
        bye();
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
    int nfds;
    int i;
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
        if (select(nfds, &fds, NULL, NULL, NULL) < 0) {
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
        "Usage: les [-hw] [-e=encoding] file...\n"
        "\n"
        "Options:\n"
        "    -e=encoding   input file encoding (affects all inputs)\n"
        "    -h            help text\n"
        "    -w            disable line wrap\n"
        "\n"
        "Key Binds:\n"
        "    b             go up a screen\n"
        "    d             go down half a screen\n"
        "    f             go down a screen\n"
        "    g             go to the top of the buffer\n"
        "    G             go to the bottom of the buffer\n"
        "    j,↓           go down one line\n"
        "    k,↑           go up one line\n"
        "    q             quit\n"
        "    u             go up half a screen\n"
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
    signal(SIGINT, bye);
    signal(SIGQUIT, bye);
    signal(SIGCONT, cont);

    printf("%s", enter_ca_mode);
    printf("%s", cursor_invisible);

    tty = open("/dev/tty", O_RDONLY);
    tcgetattr(tty, &tcattr1);
    set_tcattr();

    status_buf_size = columns * 6;
    status_buf_len = 0;
    status_buf = malloc(status_buf_size);

    line1 = tabs_len == 1 ? 0 : 1;
    printf("%s", tparm(change_scroll_region, line1, lines - 1));

    draw_tabs();
    draw_tab();

    read_loop();
    return 0;
}

