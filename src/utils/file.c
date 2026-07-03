#include "file.h"

FILE* file_open(const char* path, const char* mode) {
#if defined(_MSC_VER)
    FILE* file = NULL;
    if (fopen_s(&file, path, mode) != 0) {
        return NULL;
    }
    return file;
#else
    return fopen(path, mode);
#endif
}
