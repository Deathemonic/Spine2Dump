#include "args.h"

#include "common.h"

RootArgs cli_root_args(void) {
    RootArgs args = {
        .help = arg_lit0("h", "help", "display this help and exit"),
        .command = arg_rem("<command> [options]", "command to run"),
        .inspect = arg_rem(NULL, "inspect <asset-dir>"),
        .dump = arg_rem(NULL, "dump <asset-dir> -o <output-dir> [options]"),
        .end = arg_end(1),
    };
    void* table[] = {args.help, args.command, args.inspect, args.dump, args.end};
    COPY_ARRAY(args.table, table);
    return args;
}

OneDirArgs cli_one_dir_args(void) {
    OneDirArgs args = {
        .help = arg_lit0("h", "help", "display this help and exit"),
        .verbose = arg_lit0("v", "verbose", "enable debug logs"),
        .input = arg_file1(NULL, NULL, "<asset-dir>", "asset directory"),
        .end = arg_end(8),
    };
    void* table[] = {args.help, args.verbose, args.input, args.end};
    COPY_ARRAY(args.table, table);
    return args;
}

DumpArgs cli_dump_args(void) {
    DumpArgs args = {
        .help = arg_lit0("h", "help", "display this help and exit"),
        .verbose = arg_lit0("v", "verbose", "enable debug logs"),
        .input = arg_file1(NULL, NULL, "<asset-dir>", "asset directory"),
        .output = arg_file1("o", "output", "<output-dir>", "output directory"),
        .stills = arg_lit0(
            NULL, "stills",
            "render one still per expression attachment instead of animation frames"),
        .animation = arg_str0(NULL, "animation", "<name>", "animation name"),
        .start = arg_dbl0(NULL, "start", "<seconds>", "start seconds"),
        .end_time = arg_dbl0(NULL, "end", "<seconds>", "end seconds"),
        .fps = arg_dbl0(NULL, "fps", "<value>", "frames per second"),
        .size = arg_int0(NULL, "size", "<px>", "square output canvas size"),
        .width = arg_int0(NULL, "width", "<px>", "output canvas width"),
        .height = arg_int0(NULL, "height", "<px>", "output canvas height"),
        .scale = arg_dbl0(NULL, "scale", "<value>", "render scale multiplier"),
        .trim = arg_lit0(NULL, "trim", "crop transparent borders"),
        .trim_padding = arg_int0(NULL, "trim-padding", "<px>", "padding around trimmed bounds"),
        .alpha_threshold = arg_int0(NULL, "alpha-threshold", "<0-255>",
                                    "minimum alpha counted as visible"),
        .trim_mode = arg_str0(NULL, "trim-mode", "<none|frame|animation>",
                              "animation crop behavior"),
        .compression = arg_str0(NULL, "compression", "<fast|balanced|small>",
                                "png compression preset (default balanced)"),
        .format = arg_str0(NULL, "format", "<image|gif|video>", "animation output format"),
        .codec = arg_str0(NULL, "codec", "<h264|mpeg4|ffv1>", "video codec"),
        .software = arg_lit0(NULL, "software", "force CPU renderer (disable GPU)"),
        .end = arg_end(18),
    };
    void* table[] = {args.help,      args.verbose,     args.input,        args.output,
                     args.stills,    args.animation,   args.start,        args.end_time,
                     args.fps,       args.size,        args.width,        args.height,
                     args.scale,     args.trim,        args.trim_padding, args.alpha_threshold,
                     args.trim_mode, args.compression, args.format,       args.codec,
                     args.software,  args.end};
    COPY_ARRAY(args.table, table);
    return args;
}
