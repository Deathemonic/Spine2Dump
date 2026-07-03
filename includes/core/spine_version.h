#ifndef SPINE2DUMP_SPINE_VERSION_H
#define SPINE2DUMP_SPINE_VERSION_H

#include <stddef.h>

int spine_version_detect_file(const char* skel_path, char* version, size_t version_size);
int spine_version_major_minor(const char* version, int* major, int* minor);

#endif
