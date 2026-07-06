#ifndef SPINE2DUMP_GPU_RENDERER_H
#define SPINE2DUMP_GPU_RENDERER_H

#include <spine/Atlas.h>
#include <spine/Skeleton.h>

#include "atlas_pages.h"
#include "gpu_backend.h"
#include "render_canvas.h"
#include "render_options.h"

typedef struct GpuRenderRequest {
    GpuBackend* backend;
    spSkeleton* skeleton;
    spAtlas* atlas;
    const CpuAtlasPages* pages;
    const RenderOptions* options;
} GpuRenderRequest;

int gpu_renderer_render_image(const GpuRenderRequest* request, RgbaImage* out);

#endif
