#ifndef SPINE2DUMP_GPU_PIPELINE_H
#define SPINE2DUMP_GPU_PIPELINE_H

#include <sokol_gfx.h>

enum {
    GPU_BLEND_MODE_NORMAL = 0,
    GPU_BLEND_MODE_ADDITIVE = 1,
    GPU_BLEND_MODE_MULTIPLY = 2,
    GPU_BLEND_MODE_SCREEN = 3,
    GPU_BLEND_MODE_COUNT = 4,
};

typedef struct GpuPipelines {
    sg_shader shader;
    sg_pipeline pipelines[GPU_BLEND_MODE_COUNT];
    sg_sampler sampler;
} GpuPipelines;

int gpu_pipeline_init(GpuPipelines* pipelines);
void gpu_pipeline_dispose(GpuPipelines* pipelines);

#endif
