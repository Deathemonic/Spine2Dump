#ifndef SPINE2DUMP_GPU_BACKEND_H
#define SPINE2DUMP_GPU_BACKEND_H

#include "atlas_pages.h"
#include "render_canvas.h"
#include "software_rasterizer.h"

typedef struct GpuBackend GpuBackend;

GpuBackend* gpu_backend_init(int width, int height);
int gpu_backend_upload_atlas(GpuBackend* backend, const CpuAtlasPages* pages);
void gpu_backend_begin_frame(GpuBackend* backend);
void gpu_backend_draw_triangle(GpuBackend* backend,
                               int page_index,
                               const float* vertices,
                               const float* uvs,
                               RasterTriangle triangle,
                               RasterTransform transform,
                               RasterShade shade);
int gpu_backend_end_frame(GpuBackend* backend, RgbaImage* out);
void gpu_backend_shutdown(GpuBackend* backend);

#endif
