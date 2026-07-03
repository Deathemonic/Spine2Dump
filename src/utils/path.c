#include "path.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #include <direct.h>
    #define PATH_SEP '\\'
#else
    #include <sys/stat.h>
    #define PATH_SEP '/'
#endif

static int path_has_trailing_separator(const char* path) {
    size_t length = strlen(path);
    return length > 0 && (path[length - 1] == '/' || path[length - 1] == '\\');
}

static void make_dir_if_needed(const char* path) {
#if defined(_WIN32)
    _mkdir(path);
#else
    mkdir(path, 0777);
#endif
}

int path_join(const char* left, const char* right, char* buffer, size_t buffer_size) {
    int needs_sep = left[0] != '\0' && !path_has_trailing_separator(left);
    int written = snprintf(buffer, buffer_size, "%s%s%s", left,
                           needs_sep ? (char[]){PATH_SEP, '\0'} : "", right);
    return written >= 0 && (size_t)written < buffer_size ? 0 : -1;
}

void path_dirname(const char* path, char* buffer, size_t buffer_size) {
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* last = slash > backslash ? slash : backslash;
    if (last == NULL) {
        snprintf(buffer, buffer_size, ".");
        return;
    }

    size_t length = (size_t)(last - path);
    if (length >= buffer_size) {
        length = buffer_size - 1;
    }
    memcpy(buffer, path, length);
    buffer[length] = '\0';
}

const char* path_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* last = slash > backslash ? slash : backslash;
    return last == NULL ? path : last + 1;
}

void path_make_dirs(const char* path) {
    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", path);
    for (char* cursor = copy; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            char saved = *cursor;
            *cursor = '\0';
            if (copy[0] != '\0') {
                make_dir_if_needed(copy);
            }
            *cursor = saved;
        }
    }
    make_dir_if_needed(copy);
}
