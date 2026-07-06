#ifndef SPINE2DUMP_GPU_FRAME_H
#define SPINE2DUMP_GPU_FRAME_H

#include <stdint.h>

#include <sokol_gfx.h>

#include "gpu_pipeline.h"
#include "software_rasterizer.h"

typedef struct GpuVertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
} GpuVertex;

typedef struct GpuBatch {
    int page_index;
    int blend_mode;
    int base_vertex;
    int vertex_count;
} GpuBatch;

typedef struct GpuFrame {
    GpuVertex* vertices;
    int vertex_count;
    int vertex_capacity;
    GpuBatch* batches;
    int batch_count;
    int batch_capacity;
} GpuFrame;

void gpu_frame_reset(GpuFrame* frame);
void gpu_frame_dispose(GpuFrame* frame);
int gpu_frame_push_triangle(GpuFrame* frame,
                            int page_index,
                            int image_count,
                            int height,
                            int width,
                            const float* vertices,
                            const float* uvs,
                            RasterTriangle triangle,
                            RasterTransform transform,
                            RasterShade shade);
int gpu_frame_submit(GpuFrame* frame,
                     sg_buffer* vertex_buffer,
                     int* gpu_vertex_capacity,
                     sg_view* image_views,
                     const GpuPipelines* pipelines);

#endif
