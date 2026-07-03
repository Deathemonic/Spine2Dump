#ifndef SPINE2DUMP_PATH_H
#define SPINE2DUMP_PATH_H

#include <stddef.h>

int path_join(const char* left, const char* right, char* buffer, size_t buffer_size);
void path_dirname(const char* path, char* buffer, size_t buffer_size);
const char* path_basename(const char* path);
void path_make_dirs(const char* path);

#endif
