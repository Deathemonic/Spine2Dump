#ifndef SPINE2DUMP_COMMON_H
#define SPINE2DUMP_COMMON_H

#include <string.h>

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))
#define COPY_ARRAY(destination, source) memcpy((destination), (source), sizeof(source))

#endif
