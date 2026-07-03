#ifndef SPINE2DUMP_CPU_RENDERER_H
#define SPINE2DUMP_CPU_RENDERER_H

#include <spine/Atlas.h>
#include <spine/Skeleton.h>

#include "render_canvas.h"
#include "render_options.h"

typedef struct CpuAtlasPages CpuAtlasPages;

CpuAtlasPages* cpu_atlas_pages_load(spAtlas* atlas, const char* atlas_dir);
void cpu_atlas_pages_free(CpuAtlasPages* pages);

typedef struct CpuRenderRequest {
    spSkeleton* skeleton;
    spAtlas* atlas;
    const char* atlas_dir;
    const RenderOptions* options;
    const CpuAtlasPages* pages;
} CpuRenderRequest;

typedef struct CpuRenderPngRequest {
    CpuRenderRequest render;
    const char* output_path;
    const RenderCropRect* forced_crop;
} CpuRenderPngRequest;

int cpu_renderer_render_image(const CpuRenderRequest* request, RgbaImage* out);
int cpu_renderer_render_png(const CpuRenderPngRequest* request);

#endif
