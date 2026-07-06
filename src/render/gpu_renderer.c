#include "gpu_renderer.h"

#include <stddef.h>

#include "spine_slot_walk.h"

typedef struct {
    GpuBackend* backend;
} GpuSinkContext;

static void gpu_sink(const SlotWalkTriangle* triangle, void* user) {
    GpuSinkContext* context = user;
    gpu_backend_draw_triangle(context->backend, triangle->page_index, triangle->vertices,
                              triangle->uvs, triangle->triangle, triangle->transform,
                              triangle->shade);
}

int gpu_renderer_render_image(const GpuRenderRequest* request, RgbaImage* out) {
    if (request == NULL || request->backend == NULL || request->skeleton == NULL ||
        request->atlas == NULL || request->pages == NULL || request->options == NULL ||
        out == NULL) {
        return -1;
    }

    gpu_backend_begin_frame(request->backend);
    GpuSinkContext context = {
        .backend = request->backend,
    };
    SlotWalkRequest walk = {
        .skeleton = request->skeleton,
        .atlas = request->atlas,
        .pages = request->pages,
        .options = request->options,
    };
    if (spine_slot_walk(&walk, gpu_sink, &context) != 0) {
        return -1;
    }
    return gpu_backend_end_frame(request->backend, out);
}
