#include "les.h"
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <libgen.h>
#include <unistd.h>
#include <time.h>

size_t tabs_len = 0;
size_t tabs_size = 0;
tab_t **tabs;
int current_tab = 0;
tab_t *tabb;

void add_tab (const char *name, int fd, int state) {
    tab_t *tabb = malloc(sizeof (tab_t));
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
    tabb->mesg_size = 16;
    tabb->mesg_len = 0;
    tabb->mesg = malloc(tabb->mesg_size);
    tabb->mark = 0;
    tabb->opened = time(NULL);
    tabb->realpath = NULL;
    tabb->search_version = 0;
    tabb->matches = NULL;
    tabb->matches_len = 0;
    tabb->matches_size = 0;
    tabb->current_match = 0;
    tabb->highlights = NULL;
    tabb->highlights_len = 0;
    tabb->highlights_size = 0;
    tabb->highlights_processed = 0;
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

void init_line1 () {
    if (tabs_len > 1) {
        line1 = 1;
    }
    else {
        line1 = 0;
    }
    stage_cat(tparm(change_scroll_region, line1, lines - 2));
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

void next_tab () {
    current_tab = (current_tab + 1) % tabs_len;
    tabb = tabs[current_tab];
    change_tab();
    draw_tab();
}

void prev_tab () {
    current_tab = current_tab > 0 ? (current_tab - 1) : (tabs_len - 1);
    tabb = tabs[current_tab];
    change_tab();
    draw_tab();
}

void change_tab () {
    open_tab_file();
    if (tabb->search_version != search_version) {
        tabb->matches_len = 0;
    // Can't search here since the buffer might not be loaded
    // yet, best wait till user presses n.
    }
    stage_tabs();
}

void close_tab () {
    if (tabs_len == 1) {
        exit(0);
        return;
    }
    tab_t *tabb2 = tabb;
    add_recent_tab(tabb2);
    int i;
    for (i = current_tab; i < tabs_len - 1; i++) {
        tabs[i] = tabs[i + 1];
    }
    tabs_len--;
    if (current_tab == tabs_len) {
        current_tab--;
    }
    tabb = tabs[current_tab];

    free(tabb2->realpath);
    free(tabb2->name2);
    free(tabb2->buf);
    free(tabb2->stragglers);
    free(tabb2->mesg);
    free(tabb2->matches);
    free(tabb2->highlights);
    if (tabb2->state & OPENED && tabb2->fd) {
        close(tabb2->fd);
    }
    free(tabb2);

    init_line1();
    change_tab();
    draw_tab();
}

