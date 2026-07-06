#include <stdlib.h>
#include <string.h>

#include <sokol_gfx.h>

#include "gpu_backend.h"
#include "gl_context.h"
#include "gpu_frame.h"
#include "gpu_pipeline.h"

struct GpuBackend {
    GlContext* gl;
    int width;
    int height;
    sg_image color;
    sg_view color_view;
    sg_attachments attachments;
    sg_buffer vertex_buffer;
    int gpu_vertex_capacity;
    GpuPipelines pipelines;
    sg_image* images;
    sg_view* image_views;
    int image_count;
    GpuFrame frame;
};

static void flip_rows(unsigned char* pixels, int width, int height) {
    size_t stride = (size_t)width * 4;
    unsigned char* temp = malloc(stride);
    if (temp == NULL) {
        return;
    }
    for (int y = 0; y < height / 2; y++) {
        unsigned char* top = pixels + (size_t)y * stride;
        unsigned char* bottom = pixels + (size_t)(height - 1 - y) * stride;
        memcpy(temp, top, stride);
        memcpy(top, bottom, stride);
        memcpy(bottom, temp, stride);
    }
    free(temp);
}

GpuBackend* gpu_backend_init(int width, int height) {
    if (width <= 0 || height <= 0) {
        return NULL;
    }
    GlContext* gl = gl_context_create();
    if (gl == NULL) {
        return NULL;
    }

    sg_setup(&(sg_desc){
        .environment =
            {
                .defaults =
                    {
                        .color_format = SG_PIXELFORMAT_RGBA8,
                        .depth_format = SG_PIXELFORMAT_NONE,
                        .sample_count = 1,
                    },
            },
    });
    if (!sg_isvalid()) {
        gl_context_destroy(gl);
        return NULL;
    }

    GpuBackend* backend = calloc(1, sizeof(*backend));
    if (backend == NULL) {
        sg_shutdown();
        gl_context_destroy(gl);
        return NULL;
    }
    backend->gl = gl;
    backend->width = width;
    backend->height = height;
    backend->color = sg_make_image(&(sg_image_desc){
        .usage = {.color_attachment = true},
        .width = width,
        .height = height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
    });
    backend->color_view = sg_make_view(&(sg_view_desc){
        .color_attachment = {.image = backend->color},
    });
    backend->attachments = (sg_attachments){
        .colors[0] = backend->color_view,
    };
    if (sg_query_image_state(backend->color) != SG_RESOURCESTATE_VALID ||
        sg_query_view_state(backend->color_view) != SG_RESOURCESTATE_VALID ||
        gpu_pipeline_init(&backend->pipelines) != 0) {
        gpu_backend_shutdown(backend);
        return NULL;
    }
    return backend;
}

int gpu_backend_upload_atlas(GpuBackend* backend, const CpuAtlasPages* pages) {
    if (backend == NULL || pages == NULL) {
        return -1;
    }
    backend->images = calloc((size_t)pages->count, sizeof(*backend->images));
    backend->image_views = calloc((size_t)pages->count, sizeof(*backend->image_views));
    if ((backend->images == NULL || backend->image_views == NULL) && pages->count > 0) {
        return -1;
    }
    backend->image_count = pages->count;
    for (int i = 0; i < pages->count; i++) {
        const LoadedPage* page = &pages->items[i];
        backend->images[i] = sg_make_image(&(sg_image_desc){
            .width = page->width,
            .height = page->height,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .data = {.mip_levels[0] = {.ptr = page->pixels,
                                       .size = (size_t)page->width * (size_t)page->height *
                                               (size_t)4}},
        });
        backend->image_views[i] = sg_make_view(&(sg_view_desc){
            .texture = {.image = backend->images[i]},
        });
        if (sg_query_image_state(backend->images[i]) != SG_RESOURCESTATE_VALID ||
            sg_query_view_state(backend->image_views[i]) != SG_RESOURCESTATE_VALID) {
            return -1;
        }
    }
    return 0;
}

void gpu_backend_begin_frame(GpuBackend* backend) {
    if (backend == NULL) {
        return;
    }
    gpu_frame_reset(&backend->frame);
}

void gpu_backend_draw_triangle(GpuBackend* backend,
                               int page_index,
                               const float* vertices,
                               const float* uvs,
                               RasterTriangle triangle,
                               RasterTransform transform,
                               RasterShade shade) {
    if (backend == NULL) {
        return;
    }
    gpu_frame_push_triangle(&backend->frame, page_index, backend->image_count, backend->height,
                            backend->width, vertices, uvs, triangle, transform, shade);
}

int gpu_backend_end_frame(GpuBackend* backend, RgbaImage* out) {
    if (backend == NULL || out == NULL) {
        return -1;
    }

    sg_begin_pass(&(sg_pass){
        .attachments = {.colors[0] = backend->color_view},
        .action = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR,
                                 .clear_value = {0.0f, 0.0f, 0.0f, 0.0f}}},
    });
    int submit_result = gpu_frame_submit(&backend->frame, &backend->vertex_buffer,
                                         &backend->gpu_vertex_capacity, backend->image_views,
                                         &backend->pipelines);
    sg_end_pass();
    sg_commit();
    if (submit_result != 0) {
        return -1;
    }

    RgbaImage image = {
        .pixels = calloc((size_t)backend->width * (size_t)backend->height * 4, 1),
        .width = backend->width,
        .height = backend->height,
    };
    if (image.pixels == NULL) {
        return -1;
    }

    sg_gl_image_info info = sg_gl_query_image_info(backend->color);
    int result = gl_context_read_rgba(info.tex[info.active_slot], info.tex_target, backend->width,
                                      backend->height, image.pixels);
    sg_reset_state_cache();
    if (result != 0) {
        rgba_image_free(&image);
        return -1;
    }

    flip_rows(image.pixels, backend->width, backend->height);
    *out = image;
    return 0;
}

void gpu_backend_shutdown(GpuBackend* backend) {
    if (backend == NULL) {
        return;
    }
    for (int i = 0; i < backend->image_count; i++) {
        sg_destroy_view(backend->image_views[i]);
        sg_destroy_image(backend->images[i]);
    }
    free(backend->image_views);
    free(backend->images);
    gpu_frame_dispose(&backend->frame);
    gpu_pipeline_dispose(&backend->pipelines);
    sg_destroy_buffer(backend->vertex_buffer);
    sg_destroy_view(backend->color_view);
    sg_destroy_image(backend->color);
    sg_shutdown();
    gl_context_destroy(backend->gl);
    free(backend);
}
