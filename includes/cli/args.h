#ifndef SPINE2DUMP_CLI_ARGS_H
#define SPINE2DUMP_CLI_ARGS_H

#include <argtable3.h>

typedef struct {
    struct arg_lit* help;
    struct arg_rem* command;
    struct arg_rem* inspect;
    struct arg_rem* dump;
    struct arg_end* end;
    void* table[5];
} RootArgs;

typedef struct {
    struct arg_lit* help;
    struct arg_lit* verbose;
    struct arg_file* input;
    struct arg_end* end;
    void* table[4];
} OneDirArgs;

typedef struct {
    struct arg_lit* help;
    struct arg_lit* verbose;
    struct arg_file* input;
    struct arg_file* output;
    struct arg_lit* stills;
    struct arg_str* animation;
    struct arg_dbl* start;
    struct arg_dbl* end_time;
    struct arg_dbl* fps;
    struct arg_int* size;
    struct arg_int* width;
    struct arg_int* height;
    struct arg_dbl* scale;
    struct arg_lit* trim;
    struct arg_int* trim_padding;
    struct arg_int* alpha_threshold;
    struct arg_str* trim_mode;
    struct arg_str* compression;
    struct arg_str* format;
    struct arg_str* codec;
    struct arg_end* end;
    void* table[21];
} DumpArgs;

RootArgs cli_root_args(void);
OneDirArgs cli_one_dir_args(void);
DumpArgs cli_dump_args(void);

#endif
