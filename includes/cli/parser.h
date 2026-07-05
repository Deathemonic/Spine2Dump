#ifndef SPINE2DUMP_CLI_PARSER_H
#define SPINE2DUMP_CLI_PARSER_H

#include "render_options.h"

typedef struct {
    const char* input_dir;
    const char* output_dir;
    const char* animation;
    double start_seconds;
    double end_seconds;
    double fps;
    RenderOptions render;
    RenderTrimMode trim_mode;
    RenderOutputKind output;
    RenderVideoCodec codec;
    int stills;
} DumpOptions;

typedef enum {
    CLI_PARSE_ERROR = -1,
    CLI_PARSE_OK = 0,
    CLI_PARSE_HELP = 1
} CliParseResult;

void cli_print_root_help(void);
CliParseResult cli_parse_one_dir_command(int argc, char** argv, const char** input_dir);
CliParseResult cli_parse_dump_command(int argc, char** argv, DumpOptions* options);
int cli_exit_for_parse_result(CliParseResult parse_result, int command_result);

#endif
