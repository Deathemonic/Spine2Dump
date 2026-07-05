#include "display.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fort.h>

struct DisplayTable {
    ft_table_t* table;
    char* title;
};

struct DisplayTreeNode {
    int depth;
    char* text;
};

struct DisplayTree {
    char* root;
    struct DisplayTreeNode* nodes;
    size_t count;
    size_t cap;
};

static char* dup_str(const char* s) {
    if (s == NULL) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

static DisplayTable* table_new(const char* title) {
    DisplayTable* wrapper = calloc(1, sizeof(*wrapper));
    if (wrapper == NULL) {
        return NULL;
    }
    wrapper->table = ft_create_table();
    if (wrapper->table == NULL) {
        free(wrapper);
        return NULL;
    }
    ft_set_border_style(wrapper->table, FT_SOLID_STYLE);
    wrapper->title = dup_str(title);
    return wrapper;
}

static void write_row_va(ft_table_t* table, va_list ap) {
    const char* cell;
    while ((cell = va_arg(ap, const char*)) != NULL) {
        ft_write(table, cell);
    }
    ft_ln(table);
}

DisplayTable* display_table_create(const char* title) {
    return table_new(title);
}

void display_table_header(DisplayTable* table, ...) {
    if (table == NULL) {
        return;
    }
    size_t row = ft_cur_row(table->table);
    va_list ap;
    va_start(ap, table);
    write_row_va(table->table, ap);
    va_end(ap);
    ft_set_cell_prop(table->table, row, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
}

void display_table_row(DisplayTable* table, ...) {
    if (table == NULL) {
        return;
    }
    va_list ap;
    va_start(ap, table);
    write_row_va(table->table, ap);
    va_end(ap);
}

void display_table_print(DisplayTable* table) {
    if (table == NULL) {
        return;
    }
    if (table->title != NULL) {
        printf("%s\n", table->title);
    }
    const char* rendered = ft_to_string(table->table);
    if (rendered != NULL) {
        printf("%s", rendered);
    }
    fflush(stdout);
    ft_destroy_table(table->table);
    free(table->title);
    free(table);
}

DisplayTable* display_kv_create(const char* title) {
    return table_new(title);
}

void display_kv_row(DisplayTable* kv, const char* key, const char* value) {
    if (kv == NULL) {
        return;
    }
    ft_write(kv->table, key == NULL ? "" : key);
    ft_write(kv->table, value == NULL ? "" : value);
    ft_ln(kv->table);
}

void display_kv_rowf(DisplayTable* kv, const char* key, const char* fmt, ...) {
    if (kv == NULL) {
        return;
    }
    char value[512];
    va_list ap;
    va_start(ap, fmt);
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
    vsnprintf(value, sizeof(value), fmt, ap);
#if defined(__clang__)
    #pragma clang diagnostic pop
#endif
    va_end(ap);
    display_kv_row(kv, key, value);
}

void display_kv_print(DisplayTable* kv) {
    display_table_print(kv);
}

DisplayTree* display_tree_create(const char* root) {
    DisplayTree* tree = calloc(1, sizeof(*tree));
    if (tree == NULL) {
        return NULL;
    }
    tree->root = dup_str(root);
    return tree;
}

void display_tree_add(DisplayTree* tree, int depth, const char* text) {
    if (tree == NULL || depth < 0) {
        return;
    }
    if (tree->count == tree->cap) {
        size_t new_cap = tree->cap == 0 ? 16 : tree->cap * 2;
        struct DisplayTreeNode* grown = realloc(tree->nodes, new_cap * sizeof(*grown));
        if (grown == NULL) {
            return;
        }
        tree->nodes = grown;
        tree->cap = new_cap;
    }
    tree->nodes[tree->count].depth = depth;
    tree->nodes[tree->count].text = dup_str(text);
    tree->count++;
}

static int tree_node_is_last(const DisplayTree* tree, size_t i) {
    int depth = tree->nodes[i].depth;
    for (size_t j = i + 1; j < tree->count; j++) {
        if (tree->nodes[j].depth <= depth) {
            return tree->nodes[j].depth < depth;
        }
    }
    return 1;
}

void display_tree_print(DisplayTree* tree) {
    if (tree == NULL) {
        return;
    }
    if (tree->root != NULL) {
        printf("%s\n", tree->root);
    }
    int ancestor_last[64] = {0};
    for (size_t i = 0; i < tree->count; i++) {
        int depth = tree->nodes[i].depth;
        int is_last = tree_node_is_last(tree, i);
        for (int d = 0; d < depth && d < 64; d++) {
            printf("%s", ancestor_last[d] ? "    " : "\xe2\x94\x82   ");
        }
        printf("%s%s\n", is_last ? "\xe2\x94\x94\xe2\x94\x80 " : "\xe2\x94\x9c\xe2\x94\x80 ",
               tree->nodes[i].text == NULL ? "" : tree->nodes[i].text);
        if (depth < 64) {
            ancestor_last[depth] = is_last;
        }
    }
    fflush(stdout);
    for (size_t i = 0; i < tree->count; i++) {
        free(tree->nodes[i].text);
    }
    free(tree->nodes);
    free(tree->root);
    free(tree);
}
