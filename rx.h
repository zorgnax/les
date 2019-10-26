#ifndef __RX_H__
#define __RX_H__

enum {
    EMPTY,
    TAKE,
    BRANCH,
    CAPTURE_START,
    CAPTURE_END,
    MATCH_END,
    ASSERTION,
    CHAR_CLASS,
    CHAR_SET,
    GROUP_START,
    GROUP_END,
};

enum {
    ASSERT_SOS, // start of string
    ASSERT_SOL, // start of line
    ASSERT_EOS, // end of string
    ASSERT_EOL, // end of line
    ASSERT_SOP, // start of position
    ASSERT_SOW, // start of word
    ASSERT_EOW, // end of word
};

enum {
    CS_ANY,
    CS_NOTNL,
    CS_DIGIT,
    CS_NOTDIGIT,
    CS_WORD,
    CS_NOTWORD,
    CS_SPACE,
    CS_NOTSPACE,
};

typedef unsigned int (hash_func_t) (void *key);
typedef int (equal_func_t) (void *key1, void *key2);

typedef struct {
    int allocated;
    int count;
    int *defined;
    unsigned int *hashes;
    void **keys;
    void **values;
    hash_func_t *hash_func;
    equal_func_t *equal_func;
} hash_t;

typedef struct node_t node_t;

typedef struct {
    int min;
    int max;
    int greedy;
} quantifier_t;

typedef struct {
    char negated;
    int values_count;
    char *values;
    int ranges_count;
    char *ranges;
    int char_sets_count;
    char *char_sets;
    int str_size;
    char *str;
} char_class_t;

struct node_t {
    char type;
    node_t *next;
    union {
        int value;
        node_t *next2;
        char_class_t *ccval;
    };
};

typedef struct {
    node_t *start;
    int regexp_size;
    char *regexp;
    node_t **nodes;
    int nodes_count;
    int nodes_allocated;
    int cap_count;
    int cap_allocated;
    node_t **cap_start;
    node_t **or_end;
    int error;
    char *errorstr;
    int ignorecase;
    int char_classes_count;
    int char_classes_allocated;
    char_class_t **char_classes;
    int dfs_stack_count;
    int dfs_stack_allocated;
    node_t **dfs_stack;
    hash_t *dfs_map;
} rx_t;

typedef struct {
    node_t *node;
    int pos;
} path_t;

// The matcher maintains a list of positions that are important for backtracking
// and for remembering captures.
typedef struct {
    int path_count;
    int path_allocated;
    path_t *path;
    int cap_count;
    int cap_allocated;
    int *cap_start;
    int *cap_end;
    char *cap_defined;
    char **cap_str;
    int *cap_size;
    int success;
    int value;
} matcher_t;

rx_t *rx_alloc ();
matcher_t *rx_matcher_alloc ();
int rx_init (rx_t *rx, int regexp_size, char *regexp);
int rx_init_start (rx_t *rx, int regexp_size, char *regexp, node_t *start, int value);
node_t *rx_node_create (rx_t *rx);
void rx_print (rx_t *rx);
void rx_match_print (matcher_t *m);
int rx_match (rx_t *rx, matcher_t *m, int str_size, char *str, int start_pos);
int rx_hex_to_int (char *str, int size, unsigned int *dest);
int rx_int_to_utf8 (unsigned int value, char *str);
int rx_utf8_char_size (int str_size, char *str, int pos);
void rx_matcher_free (matcher_t *m);
void rx_free (rx_t *rx);

#endif

