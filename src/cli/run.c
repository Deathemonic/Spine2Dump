#include "run.h"

#include <stdio.h>
#include <string.h>

#include <zf_log/zf_log.h>

#include "asset_bundle.h"
#include "common.h"
#include "log.h"
#include "parser.h"
#include "spine_backend.h"

typedef int (*OneDirCommandHandler)(const char* input_dir);

typedef struct {
    const char* name;
    OneDirCommandHandler handler;
} OneDirCommand;

static int command_inspect(const char* input_dir) {
    AssetBundle bundle = {};

    int result = asset_bundle_scan(input_dir, &bundle);
    if (result != 0) {
        asset_bundle_free(&bundle);
        return result;
    }

    AtlasPageStats page_stats;
    int page_result = validate_atlas_pages(&bundle.atlas_files, &bundle.png_files, &page_stats);
    ZF_LOGI("Found %zu .skel, %zu .atlas, %zu .png files under %s", bundle.skel_files.count,
            bundle.atlas_files.count, bundle.png_files.count, input_dir);
    ZF_LOGI("Atlas pages: %zu referenced, %zu found, %zu missing", page_stats.referenced,
            page_stats.found, page_stats.missing);

    for (size_t i = 0; i < bundle.skel_files.count; i++) {
        ZF_LOGI("  skel  %s", bundle.skel_files.items[i]);
    }
    for (size_t i = 0; i < bundle.atlas_files.count; i++) {
        ZF_LOGI("  atlas %s", bundle.atlas_files.items[i]);
    }
    for (size_t i = 0; i < bundle.png_files.count; i++) {
        ZF_LOGI("  png   %s", bundle.png_files.items[i]);
    }

    if (bundle.skel_files.count > 0 && bundle.atlas_files.count > 0) {
        const char* skel = bundle.skel_files.items[0];
        const char* atlas = bundle.atlas_files.items[0];
        if (spine_backend_inspect(skel, atlas) != 0 ||
            spine_backend_list_expressions(skel, atlas) != 0) {
            result = -1;
        }
    } else {
        ZF_LOGE("Input must contain at least one .skel and one .atlas file.");
        result = -1;
    }

    if (page_result != 0) {
        result = -1;
    }

    asset_bundle_free(&bundle);
    return result;
}

static int command_dump(const DumpOptions* options) {
    AssetBundle bundle = {};
    AtlasPageStats page_stats = {};

    int result = asset_bundle_validate_dump_input(options->input_dir, &bundle, &page_stats);
    if (result != 0) {
        goto cleanup;
    }

    ZF_LOGI("Dump plan");
    ZF_LOGI("  input:     %s", options->input_dir);
    ZF_LOGI("  output:    %s", options->output_dir);
    ZF_LOGI("  skeletons: %zu", bundle.skel_files.count);
    ZF_LOGI("  atlases:   %zu", bundle.atlas_files.count);
    ZF_LOGI("  png pages: %zu", bundle.png_files.count);
    ZF_LOGI("  atlas refs:%zu referenced, %zu found", page_stats.referenced, page_stats.found);
    ZF_LOGI("  mode:      %s", options->stills ? "expression stills" : "animation frames");
    ZF_LOGI("  canvas:    %dx%d scale %.3f", options->render.width, options->render.height,
            options->render.scale);
    ZF_LOGI("  trim:      %s padding %d alpha >= %u", options->render.trim ? "enabled" : "disabled",
            options->render.trim_padding, (unsigned)options->render.alpha_threshold);

    if (options->stills) {
        SpineDumpExpressionsOptions backend_options = {
            .render = options->render,
        };
        result = spine_backend_dump_expressions(bundle.skel_files.items[0],
                                                bundle.atlas_files.items[0], options->output_dir,
                                                &backend_options);
    } else {
        ZF_LOGI("  animation: %s",
                options->animation == NULL ? "<all animations>" : options->animation);
        ZF_LOGI("  trim mode: %s",
                options->trim_mode == RENDER_TRIM_ANIMATION
                    ? "animation"
                    : (options->trim_mode == RENDER_TRIM_FRAME ? "frame" : "none"));
        ZF_LOGI("  time:      %.3fs to %s", options->start_seconds,
                options->end_seconds < 0.0 ? "<animation end>" : "custom end");
        if (options->end_seconds >= 0.0) {
            double duration = options->end_seconds - options->start_seconds;
            size_t frames = (size_t)(duration * options->fps) + 1;
            ZF_LOGI("  end:       %.3fs", options->end_seconds);
            ZF_LOGI("  frames:    %zu at %.3f fps", frames, options->fps);
        } else {
            ZF_LOGI("  frames:    based on animation duration at %.3f fps", options->fps);
        }

        SpineDumpOptions backend_options = {
            .animation = options->animation,
            .start_seconds = options->start_seconds,
            .end_seconds = options->end_seconds,
            .fps = options->fps,
            .render = options->render,
            .trim_mode = options->trim_mode,
        };
        result = spine_backend_dump_animations(bundle.skel_files.items[0],
                                               bundle.atlas_files.items[0], options->output_dir,
                                               &backend_options);
    }

cleanup:
    asset_bundle_free(&bundle);
    return result;
}

int cli_run(int argc, char** argv) {
    log_setup();

    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        cli_print_root_help();
        return argc < 2 ? 1 : 0;
    }

    const char* command = argv[1];
    int command_argc = argc - 1;
    char** command_argv = argv + 1;

    const OneDirCommand one_dir_commands[] = {
        {.name = "inspect", .handler = command_inspect},
    };
    for (size_t i = 0; i < ARRAY_LEN(one_dir_commands); i++) {
        if (strcmp(command, one_dir_commands[i].name) == 0) {
            const char* input_dir = NULL;
            CliParseResult parsed = cli_parse_one_dir_command(command_argc, command_argv,
                                                              &input_dir);
            int result = parsed == CLI_PARSE_OK ? one_dir_commands[i].handler(input_dir) : 0;
            return cli_exit_for_parse_result(parsed, result);
        }
    }

    if (strcmp(command, "dump") == 0) {
        DumpOptions options;
        CliParseResult parsed = cli_parse_dump_command(command_argc, command_argv, &options);
        return cli_exit_for_parse_result(parsed,
                                         parsed == CLI_PARSE_OK ? command_dump(&options) : 0);
    }

    ZF_LOGE("Unknown command: %s", command);
    cli_print_root_help();
    return 1;
}
