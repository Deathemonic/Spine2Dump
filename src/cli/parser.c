#include "parser.h"

#include <stdio.h>
#include <string.h>

#include <argtable3.h>
#include <zf_log/zf_log.h>

#include "args.h"
#include "common.h"

static int read_render_options(struct arg_int* size,
                               struct arg_int* width,
                               struct arg_int* height,
                               struct arg_dbl* scale,
                               struct arg_lit* trim,
                               struct arg_int* trim_padding,
                               struct arg_int* alpha_threshold,
                               RenderOptions* options) {
    *options = render_options_default();
    if (size->count > 0) {
        options->width = size->ival[0];
        options->height = size->ival[0];
    }
    if (width->count > 0) {
        options->width = width->ival[0];
    }
    if (height->count > 0) {
        options->height = height->ival[0];
    }
    if (scale->count > 0) {
        options->scale = (float)scale->dval[0];
    }
    options->trim = trim->count > 0;
    if (trim_padding->count > 0) {
        options->trim_padding = trim_padding->ival[0];
    }
    if (alpha_threshold->count > 0) {
        options->alpha_threshold = (unsigned char)alpha_threshold->ival[0];
    }

    if (alpha_threshold->count > 0 &&
        (alpha_threshold->ival[0] < 0 || alpha_threshold->ival[0] > 255)) {
        ZF_LOGE("Invalid alpha threshold.");
        return -1;
    }
    if (options->width <= 0 || options->height <= 0 || options->scale <= 0.0f ||
        options->trim_padding < 0) {
        ZF_LOGE("Invalid render size/scale/trim options.");
        return -1;
    }
    return 0;
}

static int parse_trim_mode(const char* value, RenderTrimMode* trim_mode) {
    if (value == NULL || strcmp(value, "none") == 0) {
        *trim_mode = RENDER_TRIM_NONE;
        return 0;
    }
    if (strcmp(value, "frame") == 0) {
        *trim_mode = RENDER_TRIM_FRAME;
        return 0;
    }
    if (strcmp(value, "animation") == 0) {
        *trim_mode = RENDER_TRIM_ANIMATION;
        return 0;
    }
    ZF_LOGE("Invalid trim mode: %s", value);
    return -1;
}

static int parse_compression(const char* value, PngCompressionPreset* preset) {
    if (value == NULL || strcmp(value, "balanced") == 0) {
        *preset = PNG_COMPRESSION_BALANCED;
        return 0;
    }
    if (strcmp(value, "fast") == 0) {
        *preset = PNG_COMPRESSION_FAST;
        return 0;
    }
    if (strcmp(value, "small") == 0) {
        *preset = PNG_COMPRESSION_SMALL;
        return 0;
    }
    ZF_LOGE("Invalid compression preset: %s", value);
    return -1;
}

static void print_argtable_help(const char* progname, void** argtable) {
    printf("Usage: %s", progname);
    arg_print_syntax(stdout, argtable, "\n\n");
    arg_print_glossary(stdout, argtable, "  %-28s %s\n");
}

static CliParseResult parse_result_from_errors(int errors) {
    return errors == CLI_PARSE_HELP ? CLI_PARSE_HELP
                                    : (errors == 0 ? CLI_PARSE_OK : CLI_PARSE_ERROR);
}

static int parse_or_print_help(int argc,
                               char** argv,
                               void** argtable,
                               struct arg_lit* help,
                               struct arg_end* end) {
    int errors = arg_parse(argc, argv, argtable);
    if (help->count > 0) {
        print_argtable_help(argv[0], argtable);
        return CLI_PARSE_HELP;
    }
    if (errors != 0) {
        arg_print_errors(stderr, end, "spine2dump");
    }
    return errors;
}

void cli_print_root_help(void) {
    RootArgs args = cli_root_args();
    print_argtable_help("spine2dump", args.table);
    arg_freetable(args.table, ARRAY_LEN(args.table));
}

CliParseResult cli_parse_one_dir_command(int argc, char** argv, const char** input_dir) {
    OneDirArgs args = cli_one_dir_args();
    int errors = parse_or_print_help(argc, argv, args.table, args.help, args.end);
    if (errors == 0) {
        *input_dir = args.input->filename[0];
    }
    arg_freetable(args.table, ARRAY_LEN(args.table));
    return parse_result_from_errors(errors);
}

CliParseResult cli_parse_dump_command(int argc, char** argv, DumpOptions* options) {
    DumpArgs args = cli_dump_args();
    int errors = parse_or_print_help(argc, argv, args.table, args.help, args.end);
    if (errors == 0) {
        *options = (DumpOptions){
            .input_dir = args.input->filename[0],
            .output_dir = args.output->filename[0],
            .animation = args.animation->count > 0 ? args.animation->sval[0] : NULL,
            .start_seconds = args.start->count > 0 ? args.start->dval[0] : 0.0,
            .end_seconds = args.end_time->count > 0 ? args.end_time->dval[0] : -1.0,
            .fps = args.fps->count > 0 ? args.fps->dval[0] : 30.0,
            .trim_mode = RENDER_TRIM_NONE,
            .stills = args.stills->count > 0,
        };
        if (read_render_options(args.size, args.width, args.height, args.scale, args.trim,
                                args.trim_padding, args.alpha_threshold, &options->render) != 0 ||
            parse_trim_mode(args.trim_mode->count > 0 ? args.trim_mode->sval[0] : NULL,
                            &options->trim_mode) != 0 ||
            parse_compression(args.compression->count > 0 ? args.compression->sval[0] : NULL,
                              &options->render.png_compression) != 0) {
            errors = 1;
        }
        if (options->trim_mode != RENDER_TRIM_NONE) {
            options->render.trim = 1;
        }
        if (options->start_seconds < 0.0 || options->fps <= 0.0 ||
            (options->end_seconds >= 0.0 && options->end_seconds < options->start_seconds)) {
            ZF_LOGE("Invalid time/fps options.");
            errors = 1;
        }
    }

    arg_freetable(args.table, ARRAY_LEN(args.table));
    return parse_result_from_errors(errors);
}

int cli_exit_for_parse_result(CliParseResult parse_result, int command_result) {
    if (parse_result == CLI_PARSE_HELP) {
        return 0;
    }
    if (parse_result != CLI_PARSE_OK) {
        return 1;
    }
    return command_result == 0 ? 0 : 1;
}
