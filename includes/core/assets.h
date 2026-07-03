#ifndef SPINE2DUMP_ASSETS_H
#define SPINE2DUMP_ASSETS_H

#include <stddef.h>

typedef struct {
    char** items;
    size_t count;
    size_t capacity;
} StringList;

typedef struct {
    size_t referenced;
    size_t found;
    size_t missing;
} AtlasPageStats;

int scan_assets(const char* root,
                StringList* skel_files,
                StringList* atlas_files,
                StringList* png_files);
int validate_atlas_pages(const StringList* atlas_files,
                         const StringList* png_files,
                         AtlasPageStats* stats);
void string_list_free(StringList* list);

#endif
