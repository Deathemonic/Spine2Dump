#include "assets.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <uv.h>
#include <zf_log/zf_log.h>

#include "file.h"
#include "path.h"

static char* copy_string(const char* source) {
    size_t length = strlen(source);
    char* copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, source, length + 1);
    return copy;
}

static int string_list_push(StringList* list, const char* value) {
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        char** new_items = realloc(list->items, new_capacity * sizeof(*new_items));
        if (new_items == NULL) {
            return -1;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count] = copy_string(value);
    if (list->items[list->count] == NULL) {
        return -1;
    }
    list->count++;
    return 0;
}

void string_list_free(StringList* list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int ends_with_ignore_case(const char* path, const char* suffix) {
    size_t path_length = strlen(path);
    size_t suffix_length = strlen(suffix);
    if (suffix_length > path_length) {
        return 0;
    }

    const char* start = path + path_length - suffix_length;
    for (size_t i = 0; i < suffix_length; i++) {
        if (tolower((unsigned char)start[i]) != tolower((unsigned char)suffix[i])) {
            return 0;
        }
    }
    return 1;
}

static int scan_entry(const char* child,
                      int is_dir,
                      StringList* skel_files,
                      StringList* atlas_files,
                      StringList* png_files) {
    if (is_dir) {
        return scan_assets(child, skel_files, atlas_files, png_files);
    }
    if (ends_with_ignore_case(child, ".skel")) {
        return string_list_push(skel_files, child);
    }
    if (ends_with_ignore_case(child, ".atlas")) {
        return string_list_push(atlas_files, child);
    }
    if (ends_with_ignore_case(child, ".png")) {
        return string_list_push(png_files, child);
    }
    return 0;
}

int scan_assets(const char* root,
                StringList* skel_files,
                StringList* atlas_files,
                StringList* png_files) {
    uv_fs_t req;
    if (uv_fs_scandir(NULL, &req, root, 0, NULL) < 0) {
        ZF_LOGE("Could not read directory: %s", root);
        uv_fs_req_cleanup(&req);
        return -1;
    }

    int result = 0;
    uv_dirent_t entry;
    while (result == 0 && uv_fs_scandir_next(&req, &entry) != UV_EOF) {
        char child[4096];
        if (path_join(root, entry.name, child, sizeof(child)) != 0) {
            ZF_LOGE("Path is too long under: %s", root);
            result = -1;
            break;
        }

        int is_dir;
        if (entry.type == UV_DIRENT_DIR) {
            is_dir = 1;
        } else if (entry.type == UV_DIRENT_FILE) {
            is_dir = 0;
        } else {
            uv_fs_t stat_req;
            if (uv_fs_stat(NULL, &stat_req, child, NULL) != 0) {
                uv_fs_req_cleanup(&stat_req);
                continue;
            }
            is_dir = (stat_req.statbuf.st_mode & S_IFMT) == S_IFDIR;
            uv_fs_req_cleanup(&stat_req);
        }

        result = scan_entry(child, is_dir, skel_files, atlas_files, png_files);
    }

    uv_fs_req_cleanup(&req);
    return result;
}

static char* trim_line(char* line) {
    while (*line != '\0' && isspace((unsigned char)*line)) {
        line++;
    }

    char* end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return line;
}

static int png_list_contains_base_name(const StringList* png_files, const char* page_name) {
    for (size_t i = 0; i < png_files->count; i++) {
        const char* name = path_basename(png_files->items[i]);
        if (strcmp(name, page_name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int atlas_line_is_page_name(const char* line) {
    if (*line == '\0' || strchr(line, ':') != NULL) {
        return 0;
    }
    return ends_with_ignore_case(line, ".png");
}

int validate_atlas_pages(const StringList* atlas_files,
                         const StringList* png_files,
                         AtlasPageStats* stats) {
    *stats = (AtlasPageStats){0};

    for (size_t i = 0; i < atlas_files->count; i++) {
        void* contents = NULL;
        size_t size = 0;
        if (file_read_all(atlas_files->items[i], &contents, &size) != 0) {
            ZF_LOGE("Could not open atlas: %s", atlas_files->items[i]);
            return -1;
        }

        char* text = realloc(contents, size + 1);
        if (text == NULL) {
            free(contents);
            return -1;
        }
        text[size] = '\0';

        char* cursor = text;
        while (cursor < text + size) {
            char* line = cursor;
            char* newline = strchr(cursor, '\n');
            if (newline != NULL) {
                *newline = '\0';
                cursor = newline + 1;
            } else {
                cursor = text + size;
            }

            char* trimmed = trim_line(line);
            if (!atlas_line_is_page_name(trimmed)) {
                continue;
            }

            stats->referenced++;
            if (png_list_contains_base_name(png_files, trimmed)) {
                stats->found++;
            } else {
                stats->missing++;
                ZF_LOGE("Missing atlas page '%s' referenced by %s", trimmed, atlas_files->items[i]);
            }
        }

        free(text);
    }

    return stats->missing == 0 ? 0 : -1;
}
