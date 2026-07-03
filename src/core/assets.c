#include "assets.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zf_log/zf_log.h>

#include "file.h"
#include "path.h"

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif

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

int scan_assets(const char* root,
                StringList* skel_files,
                StringList* atlas_files,
                StringList* png_files) {
#if defined(_WIN32)
    char pattern[MAX_PATH];
    if (path_join(root, "*", pattern, sizeof(pattern)) != 0) {
        ZF_LOGE("path is too long: %s", root);
        return -1;
    }

    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        ZF_LOGE("could not read directory: %s", root);
        return -1;
    }

    do {
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        char child[MAX_PATH];
        if (path_join(root, data.cFileName, child, sizeof(child)) != 0) {
            ZF_LOGE("path is too long under: %s", root);
            FindClose(handle);
            return -1;
        }

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (scan_assets(child, skel_files, atlas_files, png_files) != 0) {
                FindClose(handle);
                return -1;
            }
        } else if (ends_with_ignore_case(child, ".skel")) {
            if (string_list_push(skel_files, child) != 0) {
                FindClose(handle);
                return -1;
            }
        } else if (ends_with_ignore_case(child, ".atlas")) {
            if (string_list_push(atlas_files, child) != 0) {
                FindClose(handle);
                return -1;
            }
        } else if (ends_with_ignore_case(child, ".png")) {
            if (string_list_push(png_files, child) != 0) {
                FindClose(handle);
                return -1;
            }
        }
    } while (FindNextFileA(handle, &data) != 0);

    FindClose(handle);
    return 0;
#else
    DIR* dir = opendir(root);
    if (dir == NULL) {
        ZF_LOGE("could not read directory '%s': %s", root, strerror(errno));
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child[4096];
        if (path_join(root, entry->d_name, child, sizeof(child)) != 0) {
            ZF_LOGE("path is too long under: %s", root);
            closedir(dir);
            return -1;
        }

        struct stat stat_buffer;
        if (stat(child, &stat_buffer) != 0) {
            continue;
        }

        if (S_ISDIR(stat_buffer.st_mode)) {
            if (scan_assets(child, skel_files, atlas_files, png_files) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (ends_with_ignore_case(child, ".skel")) {
            if (string_list_push(skel_files, child) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (ends_with_ignore_case(child, ".atlas")) {
            if (string_list_push(atlas_files, child) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (ends_with_ignore_case(child, ".png")) {
            if (string_list_push(png_files, child) != 0) {
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
#endif
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
        FILE* file = file_open(atlas_files->items[i], "r");
        if (file == NULL) {
            ZF_LOGE("could not open atlas: %s", atlas_files->items[i]);
            return -1;
        }

        char line[4096];
        while (fgets(line, sizeof(line), file) != NULL) {
            char* trimmed = trim_line(line);
            if (!atlas_line_is_page_name(trimmed)) {
                continue;
            }

            stats->referenced++;
            if (png_list_contains_base_name(png_files, trimmed)) {
                stats->found++;
            } else {
                stats->missing++;
                ZF_LOGE("missing atlas page '%s' referenced by %s", trimmed, atlas_files->items[i]);
            }
        }

        fclose(file);
    }

    return stats->missing == 0 ? 0 : -1;
}
