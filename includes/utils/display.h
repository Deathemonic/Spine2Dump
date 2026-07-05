#ifndef SPINE2DUMP_DISPLAY_H
#define SPINE2DUMP_DISPLAY_H

typedef struct DisplayTable DisplayTable;
typedef struct DisplayTree DisplayTree;

DisplayTable* display_table_create(const char* title);
void display_table_header(DisplayTable* table, ...);
void display_table_row(DisplayTable* table, ...);
void display_table_print(DisplayTable* table);

DisplayTable* display_kv_create(const char* title);
void display_kv_row(DisplayTable* kv, const char* key, const char* value);
#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
void display_kv_rowf(DisplayTable* kv, const char* key, const char* fmt, ...);
void display_kv_print(DisplayTable* kv);

DisplayTree* display_tree_create(const char* root);
void display_tree_add(DisplayTree* tree, int depth, const char* text);
void display_tree_print(DisplayTree* tree);

#endif
