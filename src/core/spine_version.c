#include "spine_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zf_log/zf_log.h>

#include "file.h"

typedef struct DataInput {
    const unsigned char* cursor;
    const unsigned char* end;
} DataInput;

static int read_byte(DataInput* input, unsigned char* out) {
    if (input->cursor >= input->end) {
        return -1;
    }
    *out = *input->cursor++;
    return 0;
}

static int read_varint(DataInput* input, int* value_out) {
    unsigned char b = 0;
    if (read_byte(input, &b) != 0) {
        return -1;
    }
    int value = b & 0x7F;
    if ((b & 0x80) != 0) {
        if (read_byte(input, &b) != 0) {
            return -1;
        }
        value |= (b & 0x7F) << 7;
        if ((b & 0x80) != 0) {
            if (read_byte(input, &b) != 0) {
                return -1;
            }
            value |= (b & 0x7F) << 14;
            if ((b & 0x80) != 0) {
                if (read_byte(input, &b) != 0) {
                    return -1;
                }
                value |= (b & 0x7F) << 21;
                if ((b & 0x80) != 0) {
                    if (read_byte(input, &b) != 0) {
                        return -1;
                    }
                    value |= (b & 0x7F) << 28;
                }
            }
        }
    }
    *value_out = value;
    return 0;
}

static int skip_string(DataInput* input) {
    int length = 0;
    if (read_varint(input, &length) != 0) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }
    int bytes = length - 1;
    if (bytes < 0 || input->cursor + bytes > input->end) {
        return -1;
    }
    input->cursor += bytes;
    return 0;
}

static int read_string(DataInput* input, char* buffer, size_t buffer_size) {
    int length = 0;
    if (read_varint(input, &length) != 0 || length <= 0) {
        return -1;
    }
    int bytes = length - 1;
    if (bytes < 0 || input->cursor + bytes > input->end || (size_t)bytes >= buffer_size) {
        return -1;
    }
    memcpy(buffer, input->cursor, (size_t)bytes);
    buffer[bytes] = '\0';
    input->cursor += bytes;
    return 0;
}

int spine_version_major_minor(const char* version, int* major, int* minor) {
    if (version == NULL || major == NULL || minor == NULL) {
        return -1;
    }

    char* end = NULL;
    long major_value = strtol(version, &end, 10);
    if (end == version || *end != '.') {
        return -1;
    }
    const char* minor_start = end + 1;
    long minor_value = strtol(minor_start, &end, 10);
    if (end == minor_start || (*end != '\0' && *end != '.')) {
        return -1;
    }

    *major = (int)major_value;
    *minor = (int)minor_value;
    return 0;
}

static int read_legacy_version(const unsigned char* data,
                               long file_size,
                               char* version,
                               size_t version_size) {
    DataInput input = {
        .cursor = data,
        .end = data + file_size,
    };
    return skip_string(&input) == 0 ? read_string(&input, version, version_size) : -1;
}

static int read_int_hash_version(const unsigned char* data,
                                 long file_size,
                                 char* version,
                                 size_t version_size) {
    if (file_size < 9) {
        return -1;
    }
    DataInput input = {
        .cursor = data + 8,
        .end = data + file_size,
    };
    return read_string(&input, version, version_size);
}

int spine_version_detect_file(const char* skel_path, char* version, size_t version_size) {
    FILE* file = file_open(skel_path, "rb");
    if (file == NULL) {
        ZF_LOGE("could not open skeleton: %s", skel_path);
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    long file_size = ftell(file);
    if (file_size <= 0) {
        fclose(file);
        return -1;
    }
    rewind(file);

    unsigned char* data = malloc((size_t)file_size);
    if (data == NULL) {
        fclose(file);
        return -1;
    }
    size_t read = fread(data, 1, (size_t)file_size, file);
    fclose(file);
    if (read != (size_t)file_size) {
        free(data);
        return -1;
    }

    int major = 0;
    int minor = 0;
    int result = read_legacy_version(data, file_size, version, version_size);
    if (result != 0 || spine_version_major_minor(version, &major, &minor) != 0) {
        result = read_int_hash_version(data, file_size, version, version_size);
    }
    free(data);
    return result;
}
