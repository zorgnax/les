#include <stdlib.h>
#include <unistd.h>

char *stg;
size_t stg_len = 0;
size_t stg_size = 0;

void stage_init () {
    stg_len = 0;
    stg_size = 1024;
    stg = malloc(stg_size);
}

void stage_cat (const char *str) {
    int i;
    for (i = 0; str[i]; i++) {
        if (stg_len >= stg_size) {
            stg_size *= 2;
            stg = realloc(stg, stg_size);
        }
        stg_len++;
        stg[stg_len - 1] = str[i];
    }
}

void stage_ncat (const char *str, size_t n) {
    while (stg_len + n >= stg_size) {
        stg_size *= 2;
        stg = realloc(stg, stg_size);
    }
    int i;
    for (i = 0; i < n; i++) {
        stg_len++;
        stg[stg_len - 1] = str[i];
    }
}

void stage_write () {
    write(1, stg, stg_len);
    stg_len = 0;
}

