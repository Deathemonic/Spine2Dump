#include <stdlib.h>
#include <string.h>

#define SOKOL_IMPL
#define SOKOL_GLCORE
#include <sokol_gfx.h>

#define SOKOL_GP_IMPL
#include <sokol_gp.h>

#include "gpu_backend.h"
#include "gl_context.h"

enum {
    GPU_BLEND_MODE_NORMAL = 0,
    GPU_BLEND_MODE_ADDITIVE = 1,
    GPU_BLEND_MODE_MULTIPLY = 2,
    GPU_BLEND_MODE_SCREEN = 3,
};

struct GpuBackend {
    GlContext* gl;
    int width;
    int height;
    sg_image color;
    sg_attachments attachments;
    sg_image* images;
    int image_count;
};

static sgp_blend_mode blend_from(int blend_mode) {
    switch (blend_mode) {
        case GPU_BLEND_MODE_ADDITIVE:
            return SGP_BLENDMODE_ADD;
        case GPU_BLEND_MODE_MULTIPLY:
            return SGP_BLENDMODE_MUL;
        case GPU_BLEND_MODE_SCREEN:
            return SGP_BLENDMODE_MOD;
        default:
            return SGP_BLENDMODE_BLEND;
    }
}

static unsigned char to_byte(float value) {
    float scaled = value * 255.0f;
    if (scaled < 0.0f) {
        scaled = 0.0f;
    }
    if (scaled > 255.0f) {
        scaled = 255.0f;
    }
    return (unsigned char)scaled;
}

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

    sgp_setup(&(sgp_desc){
        .color_format = SG_PIXELFORMAT_RGBA8,
        .depth_format = SG_PIXELFORMAT_NONE,
        .sample_count = 1,
    });
    if (!sgp_is_valid()) {
        sg_shutdown();
        gl_context_destroy(gl);
        return NULL;
    }

    sg_image color = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = width,
        .height = height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
    });
    sg_attachments attachments = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = color,
    });
    if (sg_query_image_state(color) != SG_RESOURCESTATE_VALID ||
        sg_query_attachments_state(attachments) != SG_RESOURCESTATE_VALID) {
        sgp_shutdown();
        sg_shutdown();
        gl_context_destroy(gl);
        return NULL;
    }

    GpuBackend* backend = calloc(1, sizeof(*backend));
    if (backend == NULL) {
        sgp_shutdown();
        sg_shutdown();
        gl_context_destroy(gl);
        return NULL;
    }
    backend->gl = gl;
    backend->width = width;
    backend->height = height;
    backend->color = color;
    backend->attachments = attachments;
    return backend;
}

int gpu_backend_upload_atlas(GpuBackend* backend, const CpuAtlasPages* pages) {
    if (backend == NULL || pages == NULL) {
        return -1;
    }
    backend->images = calloc((size_t)pages->count, sizeof(*backend->images));
    if (backend->images == NULL && pages->count > 0) {
        return -1;
    }
    backend->image_count = pages->count;
    for (int i = 0; i < pages->count; i++) {
        const LoadedPage* page = &pages->items[i];
        backend->images[i] = sg_make_image(&(sg_image_desc){
            .width = page->width,
            .height = page->height,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .data = {.subimage[0][0] = {.ptr = page->pixels,
                                        .size = (size_t)page->width * (size_t)page->height *
                                                (size_t)4}},
        });
        if (sg_query_image_state(backend->images[i]) != SG_RESOURCESTATE_VALID) {
            return -1;
        }
    }
    return 0;
}

void gpu_backend_begin_frame(GpuBackend* backend) {
    if (backend == NULL) {
        return;
    }
    sg_begin_pass(&(sg_pass){
        .attachments = backend->attachments,
        .action = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR,
                                 .clear_value = {0.0f, 0.0f, 0.0f, 0.0f}}},
    });
    sgp_begin(backend->width, backend->height);
    sgp_viewport(0, 0, backend->width, backend->height);
    sgp_project(0.0f, (float)backend->width, 0.0f, (float)backend->height);
}

void gpu_backend_draw_triangle(GpuBackend* backend,
                               int page_index,
                               const float* vertices,
                               const float* uvs,
                               RasterTriangle triangle,
                               RasterTransform transform,
                               RasterShade shade) {
    if (backend == NULL || page_index < 0 || page_index >= backend->image_count) {
        return;
    }
    int indices[3] = {triangle.a, triangle.b, triangle.c};
    sgp_vertex out[3];
    for (int i = 0; i < 3; i++) {
        int index = indices[i];
        float world_x = vertices[index * 2];
        float world_y = vertices[index * 2 + 1];
        float px = (world_x - transform.min_x) * transform.scale;
        float py = (float)(backend->height - 1) - (world_y - transform.min_y) * transform.scale;
        out[i].position.x = px;
        out[i].position.y = py;
        out[i].texcoord.x = uvs[index * 2];
        out[i].texcoord.y = uvs[index * 2 + 1];
        out[i].color.r = to_byte(shade.color[0]);
        out[i].color.g = to_byte(shade.color[1]);
        out[i].color.b = to_byte(shade.color[2]);
        out[i].color.a = to_byte(shade.color[3]);
    }
    sgp_set_image(0, backend->images[page_index]);
    sgp_set_blend_mode(blend_from(shade.blend_mode));
    sgp_draw(SG_PRIMITIVETYPE_TRIANGLES, out, 3);
    sgp_reset_image(0);
}

int gpu_backend_end_frame(GpuBackend* backend, RgbaImage* out) {
    if (backend == NULL || out == NULL) {
        return -1;
    }
    sgp_flush();
    sgp_end();
    sg_end_pass();
    sg_commit();

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
        sg_destroy_image(backend->images[i]);
    }
    free(backend->images);
    sg_destroy_attachments(backend->attachments);
    sg_destroy_image(backend->color);
    sgp_shutdown();
    sg_shutdown();
    gl_context_destroy(backend->gl);
    free(backend);
}
