#include "les.h"
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct {
    time_t opened;
    time_t closed;
    int line;
    const char *name;
    int new;
} recent_t;

recent_t *recents;
size_t recents_len = 0;
size_t recents_size = 0;
int recents_loaded = 0;
struct tm *now = NULL;

recent_t *add_recent_file2 () {
    if (recents_len == recents_size) {
        if (recents_size == 0) {
            recents_size = 16;
            recents = malloc(recents_size * sizeof (recent_t));
        }
        else {
            recents_size *= 2;
            recents = realloc(recents, recents_size * sizeof (recent_t));
        }
    }
    recents_len++;
    recent_t *r = recents + recents_len - 1;
    r->new = 0;
    return r;
}

void add_recent_file (tab_t *tabb) {
    if (!tabb->fd || !tabb->realpath) {
        return;
    }
    load_recent_files();
    recent_t *r = add_recent_file2();
    r->opened = tabb->opened;
    r->closed = time(NULL);
    r->line = tabb->line;
    r->name = strdup(tabb->realpath);
    r->new = 1;
}

char *recents_file () {
    char *home = getenv("HOME");
    if (!home) {
        return NULL;
    }
    static char file[256];
    snprintf(file, sizeof file, "%s/.les_recents", home);
    return file;
}

void save_recent_files () {
    int i;
    for (i = 0; i < tabs_len; i++) {
        add_recent_file(tabs[i]);
    }
    char *file = recents_file();
    if (!file) {
        return;
    }
    FILE *fp = fopen(file, "a");
    if (!fp) {
        fprintf(stderr, "Couldn't open %s: %s\n", file, strerror(errno));
        exit(1);
    }
    for (i = 0; i < recents_len; i++) {
        recent_t *r = recents + i;
        if (!r->new) {
            continue;
        }
        fprintf(fp, "%ld %ld %d %s\n", r->opened, r->closed, r->line, r->name);
    }
    fclose(fp);
}

int ws (char *str) {
    int i;
    for (i = 0; str[i]; i++) {
        if (str[i] != ' ' && str[i] != '\t') {
            break;
        }
    }
    return i;
}

int digits (char *str) {
    int i;
    for (i = 0; str[i]; i++) {
        if (str[i] < '0' || str[i] > '9') {
            break;
        }
    }
    return i;
}

int therest (char *str) {
    int i;
    for (i = strlen(str) - 1; i >= 0; i--) {
        if (!(str[i] == ' ' || str[i] == '\t' || str[i] == '\n')) {
            break;
        }
    }
    return i + 1;
}

void parse_recent_files_line (char *str) {
    int i = 0;
    int len;

    // optional leading whitespace
    len = ws(str + i);
    i += len;

    // opened timestamp
    char *opened = str + i;
    int opened_len = digits(str + i);
    if (!opened_len) {
        return;
    }
    i += opened_len;

    // whitespace
    len = ws(str + i);
    if (!len) {
        return;
    }
    i += len;

    // closed timestamp
    char *closed = str + i;
    int closed_len = digits(str + i);
    if (!closed_len) {
        return;
    }
    i += closed_len;

    // whitespace
    len = ws(str + i);
    if (!len) {
        return;
    }
    i += len;

    // last line you were on
    char *line = str + i;
    int line_len = digits(str + i);
    if (!line_len) {
        return;
    }
    i += line_len;

    // whitespace
    len = ws(str + i);
    if (!len) {
        return;
    }
    i += len;

    // file name
    char *name = str + i;
    int name_len = therest(str + i);
    if (!name_len) {
        return;
    }

    recent_t *r = add_recent_file2();
    opened[opened_len] = '\0';
    r->opened = atoll(opened);
    closed[closed_len] = '\0';
    r->closed = atoll(closed);
    line[line_len]  ='\0';
    r->line = atoi(line);
    name[name_len] = '\0';
    r->name = strdup(name);
}

void load_recent_files () {
    if (recents_loaded) {
        return;
    }
    recents_loaded = 1;
    char *file = recents_file();
    if (!file) {
        return;
    }
    FILE *fp = fopen(file, "r");
    if (!fp) {
        return;
    }

    char str[512];
    while (fgets(str, sizeof str, fp)) {
        parse_recent_files_line(str);
    }

    fclose(fp);
}

void add_recents_tab_line (int i) {
    recent_t *r = recents + i;
    int len = 0;
    int line_len = 0;
    if (r->new) {
        len = snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "* ");
        line_len += len;
        tabb->buf_len += len;
    }
    struct tm *opened = localtime(&(r->opened));
    if (opened->tm_year == now->tm_year && opened->tm_mon == now->tm_mon && opened->tm_mday == now->tm_mday) {
        len = strftime(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "%I:%M%p", opened);
    }
    else {
        len = strftime(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "%Y-%m-%d %I:%M%p", opened);
    }
    line_len += len;
    tabb->buf_len += len;
    len = snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, " ");
    line_len += len;
    tabb->buf_len += len;
    int dur = r->closed - r->opened;
    if (dur < 60) {
        len = snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "%ds", dur);
    }
    else if (dur < 60 * 60) {
        dur /= 60;
        len = snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "%dm", dur);
    }
    else {
        dur /= (60 * 60);
        len = snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "%dh", dur);
    }
    line_len += len;
    tabb->buf_len += len;

    tabb->buf_len += snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, " line %d %s\n", r->line, r->name);
    tabb->nlines++;
}

void add_recents_tab () {
    int i;
    for (i = 0; i < tabs_len; i++) {
        if (tabs[i]->state & RECENTS) {
            select_tab(i);
            stage_tabs();
            draw_tab();
            return;
        }
    }

    time_t t = time(NULL);
    now = localtime(&t);

    add_tab("[Recent Files]", 0, LOADED|RECENTS);
    select_tab(tabs_len - 1);
    load_recent_files();

    for (i = 0; i < recents_len; i++) {
        if (tabb->buf_len + 512 > tabb->buf_size) {
            tabb->buf_size *= 2;
            tabb->buf = realloc(tabb->buf, tabb->buf_size);
        }
        add_recents_tab_line(i);
    }

    init_line1();
    stage_tabs();
    move_end();
}

