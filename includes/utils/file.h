#ifndef SPINE2DUMP_UTILS_FILE_H
#define SPINE2DUMP_UTILS_FILE_H

#include <stddef.h>

int file_read_all(const char* path, void** data, size_t* size);
int file_write_all(const char* path, const void* data, size_t size);

#endif
