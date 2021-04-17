//
// librx
//

#include "rx.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Reads a utf8 character from str and determines how many bytes it is. If the str
// doesn't contain a proper utf8 character, it returns 1. str needs to have at
// least one byte in it, but can end right after that, even if the byte sequence is
// invalid.
int rx_utf8_char_size (int str_size, char *str, int pos) {
    char c = str[pos];
    int size = 0;
    if ((c & 0x80) == 0x00) {
        size = 1;
    } else if ((c & 0xe0) == 0xc0) {
        size = 2;
    } else if ((c & 0xf0) == 0xe0) {
        size = 3;
    } else if ((c & 0xf8) == 0xf0) {
        size = 4;
    } else {
        // Invalid utf8 starting byte
        size = 1;
    }
    if (pos + size > str_size) {
        return 1;
    }
    for (int i = 1; i < size; i += 1) {
        c = str[pos + i];
        if ((c & 0xc0) != 0x80) {
            // Didn't find a proper utf8 continuation byte 10xx_xxxx
            return 1;
        }
    }
    return size;
}

int rx_int_to_utf8 (unsigned int value, char *str) {
    if (value < 0x80) {
        str[0] = value;
        return 1;
    } else if (value < 0x800) {
        str[0] = 0x80 | ((value & 0x7c) >> 6);
        str[1] = 0x80 | ((value & 0x3f) >> 0);
        return 2;
    } else if (value < 0x10000) {
        str[0] = 0xe0 | ((value & 0xf000) >> 12);
        str[1] = 0x80 | ((value & 0x0fc0) >> 6);
        str[2] = 0x80 | ((value & 0x003f) >> 0);
        return 3;
    } else if (value < 0x200000) {
        str[0] = 0xf0 | ((value & 0x1c0000) >> 18);
        str[1] = 0x80 | ((value & 0x03f000) >> 12);
        str[2] = 0x80 | ((value & 0x000fc0) >> 6);
        str[3] = 0x80 | ((value & 0x00003f) >> 0);
        return 4;
    } else {
        return 0;
    }
}

int rx_hex_to_int (char *str, int size, unsigned int *dest) {
    unsigned int value = 0;
    for (int i = 0; i < size; i += 1) {
        char c = str[i];
        unsigned char b = 0;
        if (c >= '0' && c <= '9') {
            b = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            b = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            b = c - 'A' + 10;
        } else {
            return 0;
        }
        value = (value << 4) | b;
    }
    *dest = value;
    return 1;
}

node_t *rx_node_create (rx_t *rx) {
    node_t *n = malloc(sizeof(node_t));
    n->type = EMPTY;
    n->next = NULL;
    if (rx->nodes_count >= rx->nodes_allocated) {
        rx->nodes_allocated *= 2;
        rx->nodes = realloc(rx->nodes, rx->nodes_allocated * sizeof(node_t *));
    }
    rx->nodes[rx->nodes_count] = n;
    rx->nodes_count += 1;
    return n;
}

int rx_node_index (rx_t *rx, node_t *n) {
    // This is slow.
    for (int i = 0; i < rx->nodes_count; i += 1) {
        if (rx->nodes[i] == n) {
            return i;
        }
    }
    return 0;
}

void rx_match_print (matcher_t *m) {
    if (m->success) {
        printf("matched\n");
    } else {
        printf("it didn't match\n");
        return;
    }

    for (int i = 0; i < m->cap_count; i += 1) {
        if (m->cap_defined[i]) {
            printf("%d: %.*s\n", i, m->cap_size[i], m->cap_str[i]);
        } else {
            printf("%d: ~\n", i);
        }
    }

    for (int i = 0; i < m->path_count; i += 1) {
        path_t *p = m->path + i;
        if (p->node->type == CAPTURE_START) {
            printf("capture %d start %d\n", p->node->value, p->pos);
        } else if (p->node->type == CAPTURE_END) {
            printf("capture %d end %d\n", p->node->value, p->pos);
        }
    }
}

void rx_print (rx_t *rx) {
    FILE *fp = fopen("/tmp/nfa.txt", "w");
    fprintf(fp, "graph g {\n");
    for (int i = 0; i < rx->nodes_count; i += 1) {
        node_t *n = rx->nodes[i];
        int i1 = rx_node_index(rx, n);
        int i2 = rx_node_index(rx, n->next);

        if (n->type == TAKE) {
            char label[6];
            if (n->value == '\x1b') {
                strcpy(label, "\u29f9e");
            } else if (n->value == '\r') {
                strcpy(label, "\u29f9r");
            } else if (n->value == '\n') {
                strcpy(label, "\u29f9n");
            } else if (n->value == '\t') {
                strcpy(label, "\u29f9t");
            } else {
                label[0] = n->value;
                label[1] = '\0';
            }
            fprintf(fp, "    %d -> %d [label=\"%s\",style=solid]\n", i1, i2, label);
        } else if (n->type == CAPTURE_START) {
            fprintf(fp, "    %d -> %d [label=\"(%d\",style=solid]\n", i1, i2, n->value);
        } else if (n->type == CAPTURE_END) {
            fprintf(fp, "    %d -> %d [label=\")%d\",style=solid]\n", i1, i2, n->value);
        } else if (n->type == GROUP_START) {
            fprintf(fp, "    %d -> %d [label=\"(?\",style=solid]\n", i1, i2);
        } else if (n->type == GROUP_END) {
            fprintf(fp, "    %d -> %d [label=\")?\",style=solid]\n", i1, i2);
        } else if (n->type == BRANCH) {
            int i3 = rx_node_index(rx, n->next2);
            fprintf(fp, "    %d [label=\"%dB\"]\n", i1, i1);
            fprintf(fp, "    %d -> %d [style=solid]\n", i1, i2);
            fprintf(fp, "    %d -> %d [style=dotted]\n", i1, i3);
        } else if (n->type == ASSERTION) {
            fprintf(fp, "    %d [label=\"%dA\"]\n", i1, i1);
            char *labels[] = {
                "^",         // ASSERT_SOS
                "^^",        // ASSERT_SOL
                "$",         // ASSERT_EOS
                "$$",        // ASSERT_EOL
                "\u29f9G",   // ASSERT_SOP
                "\\<",       // ASSERT_SOW
                "\\>",       // ASSERT_EOW
            };
            char *label = labels[n->value];
            fprintf(fp, "    %d -> %d [label=\"%s\"]\n", i1, i2, label);
        } else if (n->type == CHAR_CLASS) {
            char_class_t *ccval = n->ccval;
            fprintf(fp, "    %d [label=\"%dC\"]\n", i1, i1);
            fprintf(fp, "    %d -> %d [label=\"%.*s\"]\n", i1, i2, ccval->str_size, ccval->str);
        } else if (n->type == CHAR_SET) {
            char *labels[] = {
                ".",         // CS_ANY
                "\u29f9N",   // CS_NOTNL
                "\u29f9d",   // CS_DIGIT
                "\u29f9D",   // CS_NOTDIGIT
                "\u29f9w",   // CS_WORD
                "\u29f9W",   // CS_NOTWORD
                "\u29f9s",   // CS_SPACE
                "\u29f9S",   // CS_NOTSPACE
            };
            char *label = labels[n->value];
            fprintf(fp, "    %d [label=\"%dC\"]\n", i1, i1);
            fprintf(fp, "    %d -> %d [label=\"%s\"]\n", i1, i2, label);
        } else if (n->type == MATCH_END) {
            fprintf(fp, "    %d [label=\"%dE\"]\n", i1, i1);
        } else if (n->next) {
            fprintf(fp, "    %d -> %d [style=solid]\n", i1, i2);
        }
    }
    fprintf(fp, "}\n");
    fclose(fp);
    system("graph-easy -as=boxart /tmp/nfa.txt");
}

int rx_error (rx_t *rx, char *fmt, ...) {
    rx->error = 1;
    if (!rx->errorstr) {
        rx->errorstr = malloc(64);
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(rx->errorstr, 64, fmt, args);
    va_end(args);
    return 0;
}

int rx_char_class_parse (rx_t *rx, int pos, int *pos2, int save, char_class_t *ccval) {
    int values_count = 0, ranges_count = 0, char_sets_count = 0;
    char *regexp = rx->regexp;
    int regexp_size = rx->regexp_size;
    char c1, c2;
    char char1[4], char2[4];
    int char1_size = 0, char2_size = 0;
    char seen_dash = 0;
    char seen_special = '\0';

    while (pos < regexp_size) {
        c1 = regexp[pos];
        if (c1 == ']') {
            break;
        } else if (c1 == '-' && !seen_dash) {
            seen_dash = 1;
            pos += 1;
            continue;
        } else if (c1 == '\\') {
            if (pos + 1 >= regexp_size) {
                return rx_error(rx, "Expected character after \\.");
            }
            c2 = regexp[pos + 1];
            if (c2 == 'd' || c2 == 'D' || c2 == 'w' || c2 == 'W' || c2 == 's' || c2 == 'S' || c2 == 'N') {
                // Something like \d or \D
                if (seen_dash) {
                    return rx_error(rx, "Can't have \\%c after -.", c2);
                }
                if (save) {
                    ccval->char_sets[char_sets_count]
                        = c2 == 'N' ? CS_NOTNL
                        : c2 == 'd' ? CS_DIGIT
                        : c2 == 'D' ? CS_NOTDIGIT
                        : c2 == 'w' ? CS_WORD
                        : c2 == 'W' ? CS_NOTWORD
                        : c2 == 's' ? CS_SPACE
                        : c2 == 'S' ? CS_NOTSPACE : 0;
                }
                char_sets_count += 1;
                seen_special = c2;
                pos += 2;
                continue;

            } else if (c2 == 'e' || c2 == 'r' || c2 == 'n' || c2 == 't') {
                // Something like \n or \t
                char2[0] = c2 == 'e' ? '\x1b'
                         : c2 == 'r' ? '\r'
                         : c2 == 'n' ? '\n'
                         : c2 == 't' ? '\t' : '\0';
                char2_size = 1;
                pos += 2;

            } else if (c2 == 'x') {
                // \x3f
                if (pos + 3 >= regexp_size) {
                    return rx_error(rx, "Expected 2 characters after \\x.");
                }
                unsigned int i;
                if (!rx_hex_to_int(regexp + pos + 2, 2, &i)) {
                    return rx_error(rx, "Expected 2 hex digits after \\x.");
                }
                char2_size = 1;
                char2[0] = i;
                pos += 4;

            } else if (c2 == 'u' || c2 == 'U') {
                // \u2603 or \U00002603
                int count = c2 == 'u' ? 4 : 8;
                if (pos + 1 + count >= regexp_size) {
                    return rx_error(rx, "Expected %d characters after \\%c.", count, c2);
                }
                unsigned int i;
                if (!rx_hex_to_int(regexp + pos + 2, count, &i)) {
                    return rx_error(rx, "Expected %d hex digits after \\%c.", count, c2);
                }
                char2_size = rx_int_to_utf8(i, char2);
                if (!char2_size) {
                    return rx_error(rx, "Invalid \\%c sequence.", c2);
                }
                pos += 2 + count;

            } else {
                // Any unrecognized backslash escape, for example \]
                pos += 1;
                char2_size = rx_utf8_char_size(regexp_size, regexp, pos);
                memcpy(char2, regexp + pos, char2_size);
                pos += char2_size;
            }

        } else {
            // An unspecial character, for example a or ☃
            char2_size = rx_utf8_char_size(regexp_size, regexp, pos);
            memcpy(char2, regexp + pos, char2_size);
            pos += char2_size;
        }

        // At this point we can assume we have a character and pos was incremented.
        if (char1_size && seen_dash) {
            // Range
            if (save) {
                memcpy(ccval->ranges + ranges_count, char1, char1_size);
                ranges_count += char1_size;
                memcpy(ccval->ranges + ranges_count, char2, char2_size);
                ranges_count += char2_size;
            }
            else {
                if (seen_special) {
                    return rx_error(rx, "Can't have - after \\%c.", seen_special);
                }
                if (char1_size > char2_size || strncmp(char1, char2, char1_size) >= 0) {
                    return rx_error(rx, "End of range must be higher than start.");
                }
                ranges_count += char1_size + char2_size;
            }
            seen_dash = 0;
            char1_size = 0;
        } else if (seen_dash) {
            return rx_error(rx, "Unexpected -.");
        } else {
            // Value
            if (char1_size) {
                if (save) {
                    memcpy(ccval->values + values_count, char1, char1_size);
                }
                values_count += char1_size;
            }
            memcpy(char1, char2, char2_size);
            char1_size = char2_size;
        }
        seen_special = '\0';
    }
    if (char1_size) {
        if (save) {
            memcpy(ccval->values + values_count, char1, char1_size);
        }
        values_count += char1_size;
    }
    if (seen_dash) {
        if (save) {
            ccval->values[values_count] = '-';
        }
        values_count += 1;
    }
    if (pos >= regexp_size || regexp[pos] != ']') {
        return rx_error(rx, "Expected ].");
    }
    if (save) {
        *pos2 = pos;
    }
    ccval->values_count = values_count;
    ccval->ranges_count = ranges_count;
    ccval->char_sets_count = char_sets_count;
    return 1;
}

void char_class_free (char_class_t *ccval) {
    free(ccval->values);
    free(ccval->ranges);
    free(ccval->char_sets);
    free(ccval);
}

// This construct is character oriented. If you write [☃], it will match the 3
// byte sequence \xe2\e98\x83, and not individual bytes in that sequnce. Similarly,
// if you specify a range like [Α-Ω], Greek alpha to omega, it will match only
// characters in that range, and not invalid bytes in that sequence. You can also
// enter invalid bytes, and they will only match 1 byte at a time.
int rx_char_class_init (rx_t *rx, int pos, int *pos2, char_class_t **ccval2) {
    char *regexp = rx->regexp;
    int regexp_size = rx->regexp_size;

    if (pos + 1 >= regexp_size) {
        rx_error(rx, "Expected a character after [.");
        return 0;
    }
    char_class_t *ccval = calloc(1, sizeof(char_class_t));
    ccval->str = regexp + pos;
    pos += 1;
    char c1 = regexp[pos];
    if (c1 == '^') {
        ccval->negated = 1;
        if (pos + 1 >= regexp_size) {
            rx_error(rx, "Expected a character in [.");
            char_class_free(ccval);
            return 0;
        }
        pos += 1;
    }

    // Count the number of values and ranges needed before allocating them.
    if(!rx_char_class_parse(rx, pos, &pos, 0, ccval)) {
        char_class_free(ccval);
        return 0;
    }

    // Allocate the arrays.
    if (ccval->values_count) {
        ccval->values = malloc(ccval->values_count + 1);
        ccval->values[ccval->values_count] = '\0';
    }
    if (ccval->ranges_count) {
        ccval->ranges = malloc(ccval->ranges_count + 1);
        ccval->ranges[ccval->ranges_count] = '\0';
    }
    if (ccval->char_sets_count) {
        ccval->char_sets = malloc(ccval->char_sets_count);
    }

    // Fill in the arrays.
    rx_char_class_parse(rx, pos, &pos, 1, ccval);
    ccval->str_size = regexp + pos - ccval->str + 1;
    if (rx->char_classes_count >= rx->char_classes_allocated) {
        rx->char_classes_allocated *= 2;
        rx->char_classes = realloc(rx->char_classes, rx->char_classes_allocated * sizeof(char_class_t *));
    }
    rx->char_classes[rx->char_classes_count] = ccval;
    rx->char_classes_count += 1;

    *pos2 = pos;
    *ccval2 = ccval;
    return 1;
}

int rx_quantifier_init (rx_t *rx, int pos, int *pos2, quantifier_t *qval) {
    int regexp_size = rx->regexp_size;
    char *regexp = rx->regexp;
    char c;
    int min = 0, seen_min = 0, max = 0, seen_max = 0, greedy = 1;
    pos += 1;
    for (; pos < regexp_size; pos += 1) {
        c = regexp[pos];
        if (c >= '0' && c <= '9') {
            min = 10 * min + (c - '0');
            seen_min = 1;
        } else if (c == ',') {
            if (!seen_min) {
                return rx_error(rx, "Expected a number before ,.");
            }
            pos += 1;
            break;
        } else if (c == '}') {
            if (!seen_min) {
                return rx_error(rx, "Expected a number before }.");
            }
            max = min;
            goto check_greedy;
        } else {
            return rx_error(rx, "Unexpected character in quantifier.");
        }
    }
    for (; pos < regexp_size; pos += 1) {
        c = regexp[pos];
        if (c >= '0' && c <= '9') {
            max = 10 * max + (c - '0');
            seen_max = 1;
        } else if (c == '}') {
            if (!seen_max) {
                max = -1; // -1 means infinite
            }
            goto check_greedy;
        } else {
            return rx_error(rx, "Unexpected character in quantifier.");
        }
    }
    return rx_error(rx, "Quantifier not closed.");

    check_greedy:
    c = (pos + 1 < regexp_size) ? regexp[pos + 1] : '\0';
    if (c == '?') {
        // non greedy
        pos += 1;
        greedy = 0;
    }
    else {
        // greedy
        greedy = 1;
    }
    qval->min = min;
    qval->max = max;
    qval->greedy = greedy;
    *pos2 = pos;
    return 1;
}

static void rx_partial_free (rx_t *rx) {
    int i;
    for (i = 0; i < rx->nodes_count; i += 1) {
        node_t *n = rx->nodes[i];
        free(n);
    }
    rx->nodes_count = 0;
    for (i = 0; i < rx->char_classes_count; i += 1) {
        char_class_t *c = rx->char_classes[i];
        char_class_free(c);
    }
    rx->char_classes_count = 0;
    rx->error = 0;
    rx->cap_count = 0;
    rx->ignorecase = 0;
}

void rx_free (rx_t *rx) {
    rx_partial_free(rx);
    hash_free(rx->dfs_map);
    free(rx->nodes);
    free(rx->cap_start);
    free(rx->or_end);
    free(rx->char_classes);
    free(rx->dfs_stack);
    free(rx->errorstr);
    free(rx);
}

rx_t *rx_alloc () {
    rx_t *rx = calloc(1, sizeof(rx_t));
    rx->nodes_allocated = 10;
    rx->nodes = malloc(rx->nodes_allocated * sizeof(node_t *));
    rx->char_classes_allocated = 10;
    rx->char_classes = malloc(rx->char_classes_allocated * sizeof(char_class_t *));
    rx->cap_allocated = 10;
    rx->cap_start = malloc(rx->cap_allocated * sizeof(node_t *));
    rx->or_end = malloc(rx->cap_allocated * sizeof(node_t *));
    rx->dfs_stack_allocated = 10;
    rx->dfs_stack = malloc(rx->dfs_stack_allocated * sizeof(node_t *));
    rx->dfs_map = hash_init(hash_direct_hash, hash_direct_equal);
    return rx;
}

matcher_t *rx_matcher_alloc () {
    matcher_t *m = calloc(1, sizeof(matcher_t));
    m->path_allocated = 10;
    m->path = malloc(m->path_allocated * sizeof(path_t));
    m->cap_allocated = 10;
    m->cap_start = realloc(m->cap_start, m->cap_allocated * sizeof(int));
    m->cap_end = realloc(m->cap_end, m->cap_allocated * sizeof(int));
    m->cap_defined = realloc(m->cap_defined, m->cap_allocated * sizeof(char));
    m->cap_str = realloc(m->cap_str, m->cap_allocated * sizeof(char *));
    m->cap_size = realloc(m->cap_size, m->cap_allocated * sizeof(int));
    return m;
}

// Copies the subgraph starting at sg_start and going no furthur than sg_end into
// the node starting at new_start. Returns the new_end node. Performs a depth first
// search iteratively.
node_t *copy_subgraph (rx_t *rx, node_t *sg_start, node_t *sg_end, node_t *new_start) {
    rx->dfs_stack_count = 0;
    hash_clear(rx->dfs_map);
    node_t *node, *new_node, *new_node2, *new_end;
    if (rx->dfs_stack_count >= rx->dfs_stack_allocated) {
        rx->dfs_stack_allocated *= 2;
        rx->dfs_stack = realloc(rx->dfs_stack, rx->dfs_stack_allocated * sizeof(node_t *));
    }
    rx->dfs_stack[rx->dfs_stack_count] = sg_start;
    rx->dfs_stack_count += 1;
    hash_insert(rx->dfs_map, sg_start, new_start);

    while (1) {
        if (rx->dfs_stack_count == 0) {
            break;
        }
        rx->dfs_stack_count -= 1;
        node = rx->dfs_stack[rx->dfs_stack_count];
        new_node = hash_lookup(rx->dfs_map, node);
        if (node == sg_end) {
            new_end = new_node;
            node->type |= 0x80; // visited
            continue;
        }
        *new_node = *node;
        node->type |= 0x80; // visited

        if (rx->dfs_stack_count + 1 >= rx->dfs_stack_allocated) {
            rx->dfs_stack_allocated *= 2;
            rx->dfs_stack = realloc(rx->dfs_stack, rx->dfs_stack_allocated * sizeof(node_t *));
        }

        if (node->next->type & 0x80) {
            new_node->next = hash_lookup(rx->dfs_map, node->next);
        } else {
            new_node2 = rx_node_create(rx);
            new_node->next = new_node2;
            hash_insert(rx->dfs_map, node->next, new_node2);
            rx->dfs_stack[rx->dfs_stack_count] = node->next;
            rx->dfs_stack_count += 1;
        }

        if (new_node->type == BRANCH) {
            if (node->next2->type & 0x80) {
                new_node->next2 = hash_lookup(rx->dfs_map, node->next2);
            } else {
                new_node2 = rx_node_create(rx);
                new_node->next2 = new_node2;
                hash_insert(rx->dfs_map, node->next2, new_node2);
                rx->dfs_stack[rx->dfs_stack_count] = node->next2;
                rx->dfs_stack_count += 1;
            }
        }
    }

    // Unset all the visited flags.
    for (int i = 0; i < rx->dfs_map->allocated; i += 1) {
        if (rx->dfs_map->defined[i]) {
            node = rx->dfs_map->keys[i];
            node->type &= ~0x80;
        }
    }

    return new_end;
}

// Returns 1 on success.
int rx_init (rx_t *rx, int regexp_size, char *regexp) {
    rx_partial_free(rx);
    rx->start = rx_node_create(rx);
    return rx_init_start(rx, regexp_size, regexp, rx->start, 0);
}

int rx_init_start (rx_t *rx, int regexp_size, char *regexp, node_t *start, int value) {
    rx->regexp_size = regexp_size;
    rx->regexp = regexp;

    node_t *node = start;
    node_t *atom_start = NULL;
    node_t *or_end = NULL;
    int cap_depth = 0;
    int cap_count = 0;

    for (int pos = 0; pos < regexp_size; pos += 1) {
        unsigned char c = regexp[pos];
        if (c == '(') {
            if (pos + 2 < regexp_size && regexp[pos + 1] == '?' && regexp[pos + 2] == ':') {
                pos += 2;
                node->type = GROUP_START;
            }
            else {
                cap_count += 1;
                node->value = cap_count;
                node->type = CAPTURE_START;
            }
            node_t *node2 = rx_node_create(rx);
            node->next = node2;
            if (cap_depth >= rx->cap_allocated) {
                rx->cap_allocated *= 2;
                rx->cap_start = realloc(rx->cap_start, rx->cap_allocated * sizeof(node_t *));
                rx->or_end = realloc(rx->or_end, rx->cap_allocated * sizeof(node_t *));
            }
            rx->cap_start[cap_depth] = node;
            rx->or_end[cap_depth] = or_end;
            or_end = NULL;
            cap_depth += 1;
            atom_start = NULL;
            node = node2;

        } else if (c == ')') {
            if (!cap_depth) {
                return rx_error(rx, ") was unexpected.");
            }
            if (or_end) {
                node->next = or_end;
                node = or_end;
            }
            cap_depth -= 1;
            or_end = rx->or_end[cap_depth];
            atom_start = rx->cap_start[cap_depth];
            node_t *node2 = rx_node_create(rx);
            if (atom_start->type == CAPTURE_START) {
                node->type = CAPTURE_END;
            } else {
                node->type = GROUP_END;
            }
            node->value = atom_start->value;
            node->next = node2;
            node = node2;

        } else if (c == '|') {
            node_t *node2 = rx_node_create(rx);
            node_t *node3 = rx_node_create(rx);
            node_t *or_start;
            if (cap_depth) {
                or_start = rx->cap_start[cap_depth - 1]->next;
            } else {
                or_start = start;
            }
            *node2 = *or_start;
            or_start->type = BRANCH;
            or_start->next = node2;
            or_start->next2 = node3;
            if (or_end) {
                node->next = or_end;
            } else {
                or_end = node;
            }
            node = node3;

        } else if (c == '*') {
            if (!atom_start) {
                return rx_error(rx, "Expected something to apply the *.");
            }
            node_t *node2 = rx_node_create(rx);
            node_t *node3 = rx_node_create(rx);
            *node2 = *atom_start;
            atom_start->type = BRANCH;
            node->type = BRANCH;
            char c2 = (pos + 1 < regexp_size) ? regexp[pos + 1] : '\0';
            if (c2 == '?') {
                // non greedy
                pos += 1;
                atom_start->next = node3;
                atom_start->next2 = node2;
                node->next = node3;
                node->next2 = node2;
            } else {
                // greedy
                atom_start->next = node2;
                atom_start->next2 = node3;
                node->next = node2;
                node->next2 = node3;
            }
            node = node3;

        } else if (c == '+') {
            if (!atom_start) {
                return rx_error(rx, "Expected something to apply the +.");
            }
            node_t *node2 = rx_node_create(rx);
            node->type = BRANCH;
            char c2 = (pos + 1 < regexp_size) ? regexp[pos + 1] : '\0';
            if (c2 == '?') {
                // non greedy
                pos += 1;
                node->next = node2;
                node->next2 = atom_start;
            } else {
                // greedy
                node->next = atom_start;
                node->next2 = node2;
            }
            node = node2;

        } else if (c == '?') {
            if (!atom_start) {
                return rx_error(rx, "Expected something to apply the ?.");
            }
            node_t *node2 = rx_node_create(rx);
            *node2 = *atom_start;
            atom_start->type = BRANCH;
            char c2 = (pos + 1 < regexp_size) ? regexp[pos + 1] : '\0';
            if (c2 == '?') {
                // non greedy
                pos += 1;
                atom_start->next = node;
                atom_start->next2 = node2;
            } else {
                // greedy
                atom_start->next = node2;
                atom_start->next2 = node;
            }

        } else if (c == '{') {
            if (!atom_start) {
                return rx_error(rx, "Expected something to apply the {.");
            }
            quantifier_t qval;
            if (!rx_quantifier_init(rx, pos, &pos, &qval)) {
                return 0;
            }
            node_t *sg_start = atom_start;
            node_t *sg_end = node;
            int i = 0;
            if (qval.min == 0) {
                sg_start = rx_node_create(rx);
                *sg_start = *atom_start;
                atom_start->type = EMPTY;
                atom_start->next = NULL;
                node = atom_start;
            }
            else {
                for (i = 1; i < qval.min; i += 1) {
                    node = copy_subgraph(rx, sg_start, sg_end, node);
                }
            }

            node_t *sg_start2, *sg_end2;
            if (qval.max == -1) {
                if (i == 0) {
                    sg_start2 = sg_start;
                    sg_end2 = sg_end;
                } else {
                    sg_start2 = rx_node_create(rx);
                    sg_end2 = copy_subgraph(rx, sg_start, sg_end, sg_start2);
                }

                node_t *node2 = rx_node_create(rx);
                node->type = BRANCH;
                sg_end2->type = BRANCH;
                if (qval.greedy) {
                    node->next = sg_start2;
                    node->next2 = node2;
                    sg_end2->next = sg_start2;
                    sg_end2->next2 = node2;
                } else {
                    node->next = node2;
                    node->next2 = sg_start2;
                    sg_end2->next = node2;
                    sg_end2->next2 = sg_start2;
                }
                node = node2;
                continue;
            }

            for (; i < qval.max; i += 1) {
                if (i == 0) {
                    sg_start2 = sg_start;
                    sg_end2 = sg_end;
                } else {
                    sg_start2 = rx_node_create(rx);
                    sg_end2 = copy_subgraph(rx, sg_start, sg_end, sg_start2);
                }
                node->type = BRANCH;
                if (qval.greedy) {
                    node->next = sg_start2;
                    node->next2 = sg_end2;
                } else {
                    node->next = sg_end2;
                    node->next2 = sg_start2;
                }
                node = sg_end2;
            }

        } else if (c == '\\') {
            if (pos + 1 == regexp_size) {
                return rx_error(rx, "Expected character after \\.");
            }
            pos += 1;
            char c2 = regexp[pos];
            if (c2 == 'G') {
                node_t *node2 = rx_node_create(rx);
                node->type = ASSERTION;
                node->value = ASSERT_SOP;
                node->next = node2;
                node = node2;
            } else if (c2 == '<') {
                node_t *node2 = rx_node_create(rx);
                node->type = ASSERTION;
                node->value = ASSERT_SOW;
                node->next = node2;
                node = node2;
            } else if (c2 == '>') {
                node_t *node2 = rx_node_create(rx);
                node->type = ASSERTION;
                node->value = ASSERT_EOW;
                node->next = node2;
                node = node2;
            } else if (c2 == 'c') {
                rx->ignorecase = 1;
            } else if (c2 == 'e' || c2 == 'r' || c2 == 'n' || c2 == 't') {
                node_t *node2 = rx_node_create(rx);
                node->type = TAKE;
                node->value = c2 == 'e' ? '\x1b'
                            : c2 == 'r' ? '\r'
                            : c2 == 'n' ? '\n'
                            : c2 == 't' ? '\t' : '\0';
                node->next = node2;
                atom_start = node;
                node = node2;
            } else if (c2 == 'N' || c2 == 'd' || c2 == 'D' || c2 == 'w' || c2 == 'W' || c2 == 's' || c2 == 'S') {
                node_t *node2 = rx_node_create(rx);
                node->type = CHAR_SET;
                node->value = c2 == 'N' ? CS_NOTNL
                            : c2 == 'd' ? CS_DIGIT
                            : c2 == 'D' ? CS_NOTDIGIT
                            : c2 == 'w' ? CS_WORD
                            : c2 == 'W' ? CS_NOTWORD
                            : c2 == 's' ? CS_SPACE
                            : c2 == 'S' ? CS_NOTSPACE : 0;
                node->next = node2;
                atom_start = node;
                node = node2;
            } else if (c2 == 'x') {
                if (pos + 2 >= regexp_size) {
                    return rx_error(rx, "Expected 2 characters after \\x.");
                }
                unsigned int i;
                if (!rx_hex_to_int(regexp + pos + 1, 2, &i)) {
                    return rx_error(rx, "Expected 2 hex digits after \\x.");
                }
                pos += 2;
                node_t *node2 = rx_node_create(rx);
                node->type = TAKE;
                node->value = i;
                node->next = node2;
                atom_start = node;
                node = node2;
            } else if (c2 == 'u' || c2 == 'U') {
                int count = c2 == 'u' ? 4 : 8;
                if (pos + count >= regexp_size) {
                    return rx_error(rx, "Expected %d characters after \\%c.", count, c2);
                }
                unsigned int i;
                if (!rx_hex_to_int(regexp + pos + 1, count, &i)) {
                    return rx_error(rx, "Expected %d hex digits after \\%c.", count, c2);
                }
                pos += count;
                char str[4];
                int str_size = rx_int_to_utf8(i, str);
                if (!str_size) {
                    return rx_error(rx, "Invalid \\%c sequence.", c2);
                }
                atom_start = node;
                for (i = 0; i < str_size; i += 1) {
                    node_t *node2 = rx_node_create(rx);
                    node->type = TAKE;
                    node->value = (unsigned char) str[i];
                    node->next = node2;
                    node = node2;
                }

            } else {
                // Unrecognized backslash escape will match itself, for example \\ or \*
                node_t *node2 = rx_node_create(rx);
                node->type = TAKE;
                node->value = c2;
                node->next = node2;
                atom_start = node;
                node = node2;
            }

        } else if (c == '^') {
            node_t *node2 = rx_node_create(rx);
            node->type = ASSERTION;
            node->next = node2;
            char c2 = (pos + 1 < regexp_size) ? regexp[pos + 1] : '\0';
            if (c2 == '^') {
                pos += 1;
                node->value = ASSERT_SOL;
            } else {
                node->value = ASSERT_SOS;
            }
            node = node2;

        } else if (c == '$') {
            node_t *node2 = rx_node_create(rx);
            node->type = ASSERTION;
            node->next = node2;
            char c2 = (pos + 1 < regexp_size) ? regexp[pos + 1] : '\0';
            if (c2 == '$') {
                pos += 1;
                node->value = ASSERT_EOL;
            } else {
                node->value = ASSERT_EOS;
            }
            node = node2;
        } else if (c == '[') {
            char_class_t *ccval;
            if (!rx_char_class_init(rx, pos, &pos, &ccval)) {
                return 0;
            }
            node_t *node2 = rx_node_create(rx);
            node->type = CHAR_CLASS;
            node->ccval = ccval;
            node->next = node2;
            atom_start = node;
            node = node2;

        } else if (c == '.') {
            node_t *node2 = rx_node_create(rx);
            node->type = CHAR_SET;
            node->value = CS_ANY;
            node->next = node2;
            atom_start = node;
            node = node2;

        } else {
            node_t *node2 = rx_node_create(rx);
            node->type = TAKE;
            node->next = node2;
            node->value = c;
            atom_start = node;
            node = node2;
        }

    }
    if (cap_depth) {
        return rx_error(rx, "Expected closing ).");
    }
    if (or_end) {
        node->next = or_end;
        node = or_end;
    }
    node->type = MATCH_END;
    node->value = value;
    if (cap_count > rx->cap_count) {
        rx->cap_count = cap_count;
    }
    return 1;
}

static int flip_case (unsigned char *c) {
    if (*c >= 'a' && *c <= 'z') {
        *c -= 'a' - 'A';
        return 1;
    } else if (*c >= 'A' && *c <= 'Z') {
        *c += 'a' - 'A';
        return 1;
    }
    else {
        return 0;
    }
}

static int rx_match_char_class (rx_t *rx, char_class_t *ccval, int test_size, char *test) {
    int matched = 0;

    // Check the individual values
    for (int i = 0; i < ccval->values_count;) {
        int char1_size = rx_utf8_char_size(ccval->values_count, ccval->values, i);
        char *char1 = ccval->values + i;
        if (test_size == char1_size && strncmp(test, char1, test_size) == 0) {
            matched = 1;
            goto out;
        }
        i += char1_size;
    }

    // Check the character ranges
    for (int i = 0; i < ccval->ranges_count;) {
        int char1_size = rx_utf8_char_size(ccval->ranges_count, ccval->ranges, i);
        char *char1 = ccval->ranges + i;
        i += char1_size;
        int char2_size = rx_utf8_char_size(ccval->ranges_count, ccval->ranges, i);
        char *char2 = ccval->ranges + i;
        i += char2_size;

        int ge = (test_size > char1_size) ||
                 (test_size == char1_size && strncmp(test, char1, test_size) >= 0);

        int le = (test_size < char2_size) ||
                 (test_size == char2_size && strncmp(test, char2, test_size) <= 0);

        if (ge && le) {
            matched = 1;
            goto out;
        }
    }

    // Check the character sets
    char c = test[0];
    for (int i = 0; i < ccval->char_sets_count; i += 1) {
        char cs = ccval->char_sets[i];
        if (cs == CS_NOTNL) {
            if (c != '\n') {
                matched = 1;
                goto out;
            }
        } else if (cs == CS_DIGIT) {
            if (c >= '0' && c <= '9') {
                matched = 1;
                goto out;
            }
        } else if (cs == CS_NOTDIGIT) {
            if (!(c >= '0' && c <= '9')) {
                matched = 1;
                goto out;
            }
        } else if (cs == CS_WORD) {
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_')) {
                matched = 1;
                goto out;
            }
        } else if (cs == CS_NOTWORD) {
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_'))) {
                matched = 1;
                goto out;
            }
        } else if (cs == CS_SPACE) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                matched = 1;
                goto out;
            }
        } else if (cs == CS_NOTSPACE) {
            if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
                matched = 1;
                goto out;
            }
        }
    }

    out:
    if (ccval->negated) {
        return !matched;
    } else {
        return matched;
    }
}

static int rx_match_assertion (int type, int start_pos, int str_size, char *str, int pos) {
    if (type == ASSERT_SOS) {
        if (pos == 0) {
            return 1;
        }
    } else if (type == ASSERT_SOL) {
        if (pos == 0 || str[pos - 1] == '\n') {
            return 1;
        }
    } else if (type == ASSERT_EOS) {
        if (pos == str_size) {
            return 1;
        }
    } else if (type == ASSERT_EOL) {
        if (pos == str_size || str[pos] == '\n' || str[pos] == '\r') {
            return 1;
        }
    } else if (type == ASSERT_SOP) {
        if (pos == start_pos) {
            return 1;
        }
    } else if (type == ASSERT_SOW || type == ASSERT_EOW) {
        char w1, w2;
        if (pos == 0) {
            w1 = 0;
        } else {
            char c1 = str[pos - 1];
            w1 = (c1 >= '0' && c1 <= '9') || (c1 >= 'A' && c1 <= 'Z') || (c1 >= 'a' && c1 <= 'z') || (c1 == '_');
        }

        if (pos >= str_size) {
            w2 = 0;
        } else {
            char c2 = str[pos];
            w2 = (c2 >= '0' && c2 <= '9') || (c2 >= 'A' && c2 <= 'Z') || (c2 >= 'a' && c2 <= 'z') || (c2 == '_');
        }

        if (type == ASSERT_SOW) {
            return !w1 && w2;
        } else if (type == ASSERT_EOW) {
            return w1 && !w2;
        }
    }
    return 0;
}

// rx_match() will match a regexp against a given string. The strings it finds
// will be stored in the matcher argument. The start position can be given, but usually
// it would be 0 for the start of the string. Returns 1 on success and 0 on failure.
//
// The same matcher object can be used multiple times which will reuse the
// memory allocated for previous matches. All the captures are references into the
// original string.
int rx_match (rx_t *rx, matcher_t *m, int str_size, char *str, int start_pos) {
    m->success = 0;
    m->path_count = 0;
    node_t *node = rx->start;
    int pos = start_pos;
    unsigned char c;

    while (1) {
        retry:

        switch (node->type) {
        case TAKE:
            if (pos >= str_size) {
                goto try_alternative;
            }
            c = str[pos];
            if (c == node->value) {
                node = node->next;
                pos += 1;
                continue;
            } else if (rx->ignorecase && flip_case(&c) && c == node->value) {
                node = node->next;
                pos += 1;
                continue;
            }
            break;

        case MATCH_END:
            // End node found!
            // Match cap count is one more than rx cap count since it counts the
            // entire match as the 0 capture.
            m->cap_count = rx->cap_count + 1;
            if (m->cap_count > m->cap_allocated) {
                m->cap_allocated = m->cap_count;
                m->cap_start = realloc(m->cap_start, m->cap_allocated * sizeof(int));
                m->cap_end = realloc(m->cap_end, m->cap_allocated * sizeof(int));
                m->cap_defined = realloc(m->cap_defined, m->cap_allocated * sizeof(char));
                m->cap_str = realloc(m->cap_str, m->cap_allocated * sizeof(char *));
                m->cap_size = realloc(m->cap_size, m->cap_allocated * sizeof(int));
            }
            m->cap_defined[0] = 1;
            m->cap_start[0] = start_pos;
            m->cap_end[0] = pos;
            m->cap_str[0] = str + start_pos;
            m->cap_size[0] = pos - start_pos;
            for (int i = 1; i < m->cap_count; i++) {
                m->cap_defined[i] = 0;
                m->cap_start[i] = 0;
                m->cap_end[i] = 0;
                m->cap_str[i] = NULL;
                m->cap_size[i] = 0;
            }
            for (int i = 0; i < m->path_count; i += 1) {
                path_t *p = m->path + i;
                if (p->node->type == CAPTURE_START) {
                    int j = p->node->value;
                    m->cap_defined[j] = 1;
                    m->cap_start[j] = p->pos;
                    m->cap_str[j] = str + p->pos;
                } else if (p->node->type == CAPTURE_END) {
                    int j = p->node->value;
                    m->cap_end[j] = p->pos;
                    m->cap_size[j] = p->pos - m->cap_start[j];
                }
            }
            m->success = 1;
            m->value = node->value;
            return 1;
            break;

        case BRANCH:
        case CAPTURE_START:
        case CAPTURE_END:
            {
                if (m->path_count == m->path_allocated) {
                    m->path_allocated *= 2;
                    m->path = realloc(m->path, m->path_allocated * sizeof(path_t));
                }
                path_t *p = m->path + m->path_count;
                p->pos = pos;
                p->node = node;
                m->path_count += 1;
                node = node->next;
                continue;
            }
            break;

        case GROUP_START:
        case GROUP_END:
            node = node->next;
            continue;
            break;

        case ASSERTION:
            if (rx_match_assertion(node->value, start_pos, str_size, str, pos)) {
                node = node->next;
                continue;
            }
            // Do not try an alternative if it was a SOS or SOP assertion that failed
            if (node->value == ASSERT_SOS || node->value == ASSERT_SOP) {
                goto out;
            }
            break;

        case CHAR_CLASS:
            if (pos >= str_size) {
                goto try_alternative;
            }
            int test_size = rx_utf8_char_size(str_size, str, pos);
            char *test = str + pos;
            char_class_t *ccval = node->ccval;

            if (rx_match_char_class(rx, ccval, test_size, test)) {
                pos += test_size;
                node = node->next;
                continue;
            } else if (rx->ignorecase) {
                // If retrying because of ignorecase, copy the char to a buffer to
                // change the case of what's being tested.
                unsigned char retry_buf[4];
                memcpy(retry_buf, test, test_size);
                if (flip_case(retry_buf)) {
                    if (rx_match_char_class(rx, ccval, test_size, (char *) retry_buf)) {
                        pos += test_size;
                        node = node->next;
                        continue;
                    }
                }
            }
            break;

        case CHAR_SET:
            if (pos >= str_size) {
                goto try_alternative;
            }
            c = str[pos];
            if (node->value == CS_ANY) {
                pos += 1;
                node = node->next;
                continue;
            } else if (node->value == CS_NOTNL) {
                if (c != '\n') {
                    pos += 1;
                    node = node->next;
                    continue;
                }
            } else if (node->value == CS_DIGIT) {
                if (c >= '0' && c <= '9') {
                    pos += 1;
                    node = node->next;
                    continue;
                }
            } else if (node->value == CS_NOTDIGIT) {
                if (!(c >= '0' && c <= '9')) {
                    pos += 1;
                    node = node->next;
                    continue;
                }
            } else if (node->value == CS_WORD) {
                if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_')) {
                    pos += 1;
                    node = node->next;
                    continue;
                }
            } else if (node->value == CS_NOTWORD) {
                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_'))) {
                    pos += 1;
                    node = node->next;
                    continue;
                }
            } else if (node->value == CS_SPACE) {
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    pos += 1;
                    node = node->next;
                    continue;
                }
            } else if (node->value == CS_NOTSPACE) {
                if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
                    pos += 1;
                    node = node->next;
                    continue;
                }
            }
            break;

        case EMPTY:
            node = node->next;
            continue;
            break;
        }

        try_alternative:

        for (int i = m->path_count - 1; i >= 0; i--) {
            path_t *p = m->path + i;
            if (p->node->type == BRANCH) {
                node = p->node->next2;
                pos = p->pos;
                m->path_count = i;
                goto retry;
            }
        }

        // Try another start position.
        if (start_pos == str_size) {
            break;
        }
        m->path_count = 0;
        start_pos += 1;
        pos = start_pos;
        node = rx->start;
    }
    out:
    return 0;
}

void rx_matcher_free (matcher_t *m) {
    free(m->path);
    free(m->cap_start);
    free(m->cap_end);
    free(m->cap_defined);
    free(m->cap_str);
    free(m->cap_size);
    free(m);
}

