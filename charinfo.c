#include "les.h"
#include <string.h>

int get_char_width (unsigned int codepoint) {
    static width_range_t width_ranges[] = {
        {0x00,    0x07,    0},
        {0x08,    0x08,   -1},
        {0x09,    0x1f,    0},
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

    if (codepoint == 0x09) {
        return tab_width;
    }
    else if (0x20 <= codepoint && codepoint < 0x7f) {
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

int get_escape_len (const char *buf) {
    int i;
    for (i = 1;; i++) {
        if (buf[i] == '\0') {
            return 1;
        }
        if ((buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= 'A' && buf[i] <= 'Z')) {
            return i + 1;
        }
    }
    return i;
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
    if (c == 0x1b) {
        cinfo->len = get_escape_len(buf);
        cinfo->width = 0;
    }
    else {
        cinfo->width = get_char_width(codepoint);
    }
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

int strnwidth (const char *str, size_t len) {
    int i, width = 0;
    charinfo_t cinfo;
    for (i = 0; i < len;) {
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

