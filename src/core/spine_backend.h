#ifndef SPINE2DUMP_SPINE_BACKEND_H
#define SPINE2DUMP_SPINE_BACKEND_H

#include "render_options.h"

typedef struct SpineDumpOptions {
    const char* animation;
    double start_seconds;
    double end_seconds;
    double fps;
    RenderOptions render;
    RenderTrimMode trim_mode;
} SpineDumpOptions;

typedef struct SpineDumpExpressionsOptions {
    RenderOptions render;
} SpineDumpExpressionsOptions;

int spine_backend_inspect(const char* skel_path, const char* atlas_path);
int spine_backend_list_expressions(const char* skel_path, const char* atlas_path);
int spine_backend_dump_expressions(const char* skel_path,
                                   const char* atlas_path,
                                   const char* output_dir,
                                   const SpineDumpExpressionsOptions* options);
int spine_backend_dump_animations(const char* skel_path,
                                  const char* atlas_path,
                                  const char* output_dir,
                                  const SpineDumpOptions* options);

#endif
