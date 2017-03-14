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
    int deleted;
} recent_t;

recent_t *recents;
size_t recents_len = 0;
size_t recents_size = 0;
int recents_loaded = 0;
char *home;

recent_t *add_recent () {
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
    r->deleted = 0;
    return r;
}

int recentscmp (const void *a, const void *b) {
    const recent_t *a2 = a;
    const recent_t *b2 = b;
    return strcmp(a2->name, b2->name);
}

void delete_prior_entry (recent_t *r) {
    int i;
    for (i = recents_len - 2; i >= 0; i--) {
        recent_t *r2 = recents + i;
        if (strcmp(r2->name, r->name) == 0) {
            r2->deleted = 1;
            break;
        }
    }
}

void add_recent_tab (tab_t *tabb) {
    if (!tabb->fd || !tabb->realpath) {
        return;
    }
    recent_t *r = add_recent();
    r->opened = tabb->opened;
    r->closed = time(NULL);
    r->line = tabb->line;
    r->name = strdup(tabb->realpath);
    r->new = 1;
    delete_prior_entry(r);
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

void save_recents_file () {
    load_recents_file();
    int i;
    for (i = 0; i < tabs_len; i++) {
        add_recent_tab(tabs[i]);
    }
    char *file = recents_file();
    if (!file) {
        return;
    }
    FILE *fp = fopen(file, "w");
    if (!fp) {
        fprintf(stderr, "Couldn't open %s: %s\n", file, strerror(errno));
        exit(1);
    }
    for (i = 0; i < recents_len; i++) {
        recent_t *r = recents + i;
        if (r->deleted) {
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

    recent_t *r = add_recent();
    opened[opened_len] = '\0';
    r->opened = atoll(opened);
    closed[closed_len] = '\0';
    r->closed = atoll(closed);
    line[line_len]  ='\0';
    r->line = atoi(line);
    name[name_len] = '\0';
    r->name = strdup(name);
}

// Returns the most recent line from the last time you opened this file
void get_last_line () {
    if (!tabb->realpath) {
        tabb->realpath = realpath(tabb->name, NULL);
    }
    if (!tabb->realpath) {
        return;
    }
    if (!recents_loaded) {
        load_recents_file();
    }
    int i;
    for (i = recents_len - 1; i >= 0; i--) {
        recent_t *r = recents + i;
        if (strcmp(r->name, tabb->realpath) == 0) {
            tabb->last_line = r->line;
            return;
        }
    }
}

void load_recents_file () {
    recents_loaded = 1;

    recent_t *recents2 = recents;
    size_t recents2_len = recents_len;
    size_t recents2_size = recents_size;
    recents = NULL;
    recents_len = 0;
    recents_size = 0;

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

    int i;
    for (i = 0; i < recents2_len; i++) {
        recent_t *r2 = recents2 + i;
        if (!r2->new) {
            continue;
        }
        recent_t *r = add_recent();
        r->opened = r2->opened;
        r->closed = r2->closed;
        r->line = r2->line;
        r->name = r2->name;
        r->new = r2->new;
        delete_prior_entry(r);
    }
    free(recents2);
}

void add_recents_tab_line (recent_t *r) {
    int len = 0;
    static char str[32];
    if (r->new) {
        len = snprintf(str, sizeof str, "* ");
    }
    struct tm *opened = localtime(&(r->opened));
    len += strftime(str + len, sizeof str - len, "%Y-%m-%d %I:%M%p", opened);
    len = snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "%-18s ", str);
    tabb->buf_len += len;
    int dur = r->closed - r->opened;
    if (dur < 60) {
        len = snprintf(str, sizeof str, "%ds", dur);
    }
    else if (dur < 60 * 60) {
        len = snprintf(str, sizeof str, "%dm", dur / 60);
    }
    else {
        dur /= (60 * 60);
        len = snprintf(str, sizeof str, "%dh", dur / (60 * 60));
    }
    const char *name = r->name;
    char *hdir = "";
    if (home) {
        size_t home_len = strlen(home);
        size_t name_len = strlen(name);
        if (name_len > home_len + 1 && strncmp(name, home, home_len) == 0 && name[home_len] == '/') {
            name += home_len + 1;
            hdir = "~/";
        }
    }
    len = snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "%-3s line %-5d %s%s\n", str, r->line, hdir, name);
    tabb->buf_len += len;

    tabb->nlines++;
}

void add_recents_tab () {
    int i;
    if (tabb->state & RECENTS) {
        close_tab();
        return;
    }
    for (i = 0; i < tabs_len; i++) {
        if (tabs[i]->state & RECENTS) {
            current_tab = i;
            tabb = tabs[current_tab];
            change_tab();
            draw_tab();
            return;
        }
    }

    static char str[32];
    time_t today = time(NULL);
    struct tm *todaytm = malloc(sizeof (struct tm));
    localtime_r(&today, todaytm);
    todaytm->tm_sec = 0;
    todaytm->tm_min = 0;
    todaytm->tm_hour = 0;
    todaytm->tm_isdst = -1;
    today = mktime(todaytm);
    struct tm *daytm = malloc(sizeof (struct tm));
    home = getenv("HOME");

    add_tab("[Recent Files]", 0, LOADED|RECENTS);
    current_tab = tabs_len - 1;
    tabb = tabs[current_tab];
    if (!recents_loaded) {
        load_recents_file();
    }

    int day_line = 8;
    for (i = 0; i < recents_len; i++) {
        recent_t *r = recents + i;
        if (r->deleted) {
            continue;
        }
        if (tabb->buf_len + 512 > tabb->buf_size) {
            tabb->buf_size *= 2;
            tabb->buf = realloc(tabb->buf, tabb->buf_size);
        }
        if (day_line) {
            memcpy(daytm, todaytm, sizeof (struct tm));
            daytm->tm_mday -= day_line - 1;
            daytm->tm_isdst = -1;
            time_t day = mktime(daytm);
            if (r->opened >= day) {
                day_line--;
                while (1) {
                    daytm->tm_mday++;
                    daytm->tm_isdst = -1;
                    day = mktime(daytm);
                    if (r->opened < day) {
                        break;
                    }
                    day_line--;
                }
                memcpy(daytm, todaytm, sizeof (struct tm));
                daytm->tm_mday -= day_line;
                daytm->tm_isdst = -1;
                day = mktime(daytm);
                strftime(str, sizeof str, "%A", daytm);
                char *day_nickname = "";
                if (day_line == 0) {
                    day_nickname = " (Today)";
                }
                else if (day_line == 1) {
                    day_nickname = " (Yesterday)";
                }

                tabb->buf_len += snprintf(tabb->buf + tabb->buf_len, tabb->buf_size - tabb->buf_len, "--------- %s%s ---------\n", str, day_nickname);
                tabb->nlines++;
            }
        }
        add_recents_tab_line(r);
    }

    free(daytm);
    free(todaytm);
    init_line1();
    change_tab();
    move_end();
}

