#include "gpu_frame.h"

#include <stdlib.h>

#include <sokol_gfx.h>

enum {
    GPU_INITIAL_VERTICES = 65536,
};

static uint32_t color_pack(float r, float g, float b, float a) {
    float values[4] = {r, g, b, a};
    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
        float scaled = values[i] * 255.0f;
        if (scaled < 0.0f) {
            scaled = 0.0f;
        }
        if (scaled > 255.0f) {
            scaled = 255.0f;
        }
        result |= (uint32_t)(unsigned char)scaled << (i * 8);
    }
    return result;
}

static int ensure_vertex_capacity(GpuFrame* frame, int needed) {
    if (needed <= frame->vertex_capacity) {
        return 0;
    }
    int capacity = frame->vertex_capacity == 0 ? GPU_INITIAL_VERTICES : frame->vertex_capacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    GpuVertex* vertices = realloc(frame->vertices, (size_t)capacity * sizeof(*vertices));
    if (vertices == NULL) {
        return -1;
    }
    frame->vertices = vertices;
    frame->vertex_capacity = capacity;
    return 0;
}

static int ensure_batch_capacity(GpuFrame* frame, int needed) {
    if (needed <= frame->batch_capacity) {
        return 0;
    }
    int capacity = frame->batch_capacity == 0 ? 256 : frame->batch_capacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    GpuBatch* batches = realloc(frame->batches, (size_t)capacity * sizeof(*batches));
    if (batches == NULL) {
        return -1;
    }
    frame->batches = batches;
    frame->batch_capacity = capacity;
    return 0;
}

static int ensure_gpu_capacity(sg_buffer* vertex_buffer, int* gpu_vertex_capacity, int needed) {
    if (needed <= *gpu_vertex_capacity && vertex_buffer->id != SG_INVALID_ID) {
        return 0;
    }
    int capacity = *gpu_vertex_capacity == 0 ? GPU_INITIAL_VERTICES : *gpu_vertex_capacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    if (vertex_buffer->id != SG_INVALID_ID) {
        sg_destroy_buffer(*vertex_buffer);
    }
    *vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
        .size = (size_t)capacity * sizeof(GpuVertex),
        .usage = {.dynamic_update = true},
    });
    if (sg_query_buffer_state(*vertex_buffer) != SG_RESOURCESTATE_VALID) {
        return -1;
    }
    *gpu_vertex_capacity = capacity;
    return 0;
}

void gpu_frame_reset(GpuFrame* frame) {
    if (frame == NULL) {
        return;
    }
    frame->vertex_count = 0;
    frame->batch_count = 0;
}

void gpu_frame_dispose(GpuFrame* frame) {
    if (frame == NULL) {
        return;
    }
    free(frame->batches);
    free(frame->vertices);
    *frame = (GpuFrame){};
}

int gpu_frame_push_triangle(GpuFrame* frame,
                            int page_index,
                            int image_count,
                            int height,
                            int width,
                            const float* vertices,
                            const float* uvs,
                            RasterTriangle triangle,
                            RasterTransform transform,
                            RasterShade shade) {
    if (frame == NULL || page_index < 0 || page_index >= image_count) {
        return -1;
    }
    if (ensure_vertex_capacity(frame, frame->vertex_count + 3) != 0) {
        return -1;
    }
    int blend_mode = shade.blend_mode;
    if (blend_mode < 0 || blend_mode >= GPU_BLEND_MODE_COUNT) {
        blend_mode = GPU_BLEND_MODE_NORMAL;
    }
    if (frame->batch_count == 0 || frame->batches[frame->batch_count - 1].page_index != page_index ||
        frame->batches[frame->batch_count - 1].blend_mode != blend_mode) {
        if (ensure_batch_capacity(frame, frame->batch_count + 1) != 0) {
            return -1;
        }
        frame->batches[frame->batch_count] = (GpuBatch){
            .page_index = page_index,
            .blend_mode = blend_mode,
            .base_vertex = frame->vertex_count,
            .vertex_count = 0,
        };
        frame->batch_count++;
    }

    int indices[3] = {triangle.a, triangle.b, triangle.c};
    uint32_t color = color_pack(shade.color[0], shade.color[1], shade.color[2], shade.color[3]);
    for (int i = 0; i < 3; i++) {
        int index = indices[i];
        float world_x = vertices[index * 2];
        float world_y = vertices[index * 2 + 1];
        GpuVertex* out = &frame->vertices[frame->vertex_count++];
        out->x = (world_x - transform.min_x) * transform.scale / (float)width;
        out->y = ((float)(height - 1) - (world_y - transform.min_y) * transform.scale) /
                 (float)height;
        out->u = uvs[index * 2];
        out->v = uvs[index * 2 + 1];
        out->color = color;
    }
    frame->batches[frame->batch_count - 1].vertex_count += 3;
    return 0;
}

int gpu_frame_submit(GpuFrame* frame,
                     sg_buffer* vertex_buffer,
                     int* gpu_vertex_capacity,
                     sg_view* image_views,
                     const GpuPipelines* pipelines) {
    if (frame == NULL || vertex_buffer == NULL || gpu_vertex_capacity == NULL || image_views == NULL ||
        pipelines == NULL) {
        return -1;
    }
    if (frame->vertex_count > 0 &&
        ensure_gpu_capacity(vertex_buffer, gpu_vertex_capacity, frame->vertex_count) != 0) {
        return -1;
    }
    if (frame->vertex_count > 0) {
        sg_update_buffer(*vertex_buffer, &(sg_range){
                                           .ptr = frame->vertices,
                                           .size = (size_t)frame->vertex_count *
                                                   sizeof(*frame->vertices),
                                       });
    }
    for (int i = 0; i < frame->batch_count; i++) {
        GpuBatch* batch = &frame->batches[i];
        sg_apply_pipeline(pipelines->pipelines[batch->blend_mode]);
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = *vertex_buffer,
            .views[0] = image_views[batch->page_index],
            .samplers[0] = pipelines->sampler,
        });
        sg_draw(batch->base_vertex, batch->vertex_count, 1);
    }
    return 0;
}
