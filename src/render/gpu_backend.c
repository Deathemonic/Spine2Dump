#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sokol_gfx.h>

#include "gpu_backend.h"
#include "gl_context.h"

enum {
    GPU_BLEND_MODE_NORMAL = 0,
    GPU_BLEND_MODE_ADDITIVE = 1,
    GPU_BLEND_MODE_MULTIPLY = 2,
    GPU_BLEND_MODE_SCREEN = 3,
    GPU_BLEND_MODE_COUNT = 4,
    GPU_INITIAL_VERTICES = 65536,
};

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

struct GpuBackend {
    GlContext* gl;
    int width;
    int height;
    sg_image color;
    sg_view color_view;
    sg_attachments attachments;
    sg_buffer vertex_buffer;
    int gpu_vertex_capacity;
    sg_shader shader;
    sg_pipeline pipelines[GPU_BLEND_MODE_COUNT];
    sg_sampler sampler;
    sg_image* images;
    sg_view* image_views;
    int image_count;
    GpuVertex* vertices;
    int vertex_count;
    int vertex_capacity;
    GpuBatch* batches;
    int batch_count;
    int batch_capacity;
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

static sg_blend_state blend_state(int blend_mode) {
    switch (blend_mode) {
        case GPU_BLEND_MODE_ADDITIVE:
            return (sg_blend_state){
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE,
                .op_rgb = SG_BLENDOP_ADD,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE,
                .op_alpha = SG_BLENDOP_ADD,
            };
        case GPU_BLEND_MODE_MULTIPLY:
            return (sg_blend_state){
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_DST_COLOR,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .op_rgb = SG_BLENDOP_ADD,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .op_alpha = SG_BLENDOP_ADD,
            };
        case GPU_BLEND_MODE_SCREEN:
            return (sg_blend_state){
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_ONE,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
                .op_rgb = SG_BLENDOP_ADD,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .op_alpha = SG_BLENDOP_ADD,
            };
        default:
            return (sg_blend_state){
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .op_rgb = SG_BLENDOP_ADD,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .op_alpha = SG_BLENDOP_ADD,
            };
    }
}

static sg_shader make_shader(void) {
    return sg_make_shader(&(sg_shader_desc){
        .vertex_func =
            {
                .source = "#version 410 core\n"
                          "in vec2 position;\n"
                          "in vec2 texcoord;\n"
                          "in vec4 color0;\n"
                          "out vec2 uv;\n"
                          "out vec4 color;\n"
                          "void main() {\n"
                          "  gl_Position = vec4(position.x * 2.0 - 1.0, 1.0 - position.y * 2.0, 0.0, 1.0);\n"
                          "  uv = texcoord;\n"
                          "  color = color0;\n"
                          "}\n",
            },
        .fragment_func =
            {
                .source = "#version 410 core\n"
                          "uniform sampler2D tex_sampler;\n"
                          "in vec2 uv;\n"
                          "in vec4 color;\n"
                          "out vec4 frag_color;\n"
                          "void main() {\n"
                          "  frag_color = texture(tex_sampler, uv) * color;\n"
                          "}\n",
            },
        .attrs =
            {
                [0] = {.glsl_name = "position", .base_type = SG_SHADERATTRBASETYPE_FLOAT},
                [1] = {.glsl_name = "texcoord", .base_type = SG_SHADERATTRBASETYPE_FLOAT},
                [2] = {.glsl_name = "color0", .base_type = SG_SHADERATTRBASETYPE_FLOAT},
            },
        .views[0].texture = {.stage = SG_SHADERSTAGE_FRAGMENT,
                             .image_type = SG_IMAGETYPE_2D,
                             .sample_type = SG_IMAGESAMPLETYPE_FLOAT},
        .samplers[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                        .sampler_type = SG_SAMPLERTYPE_FILTERING},
        .texture_sampler_pairs[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                                     .view_slot = 0,
                                     .sampler_slot = 0,
                                     .glsl_name = "tex_sampler"},
    });
}

static sg_pipeline make_pipeline(sg_shader shader, int blend_mode) {
    return sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shader,
        .layout =
            {
                .buffers[0].stride = sizeof(GpuVertex),
                .attrs =
                    {
                        [0] = {.offset = offsetof(GpuVertex, x), .format = SG_VERTEXFORMAT_FLOAT2},
                        [1] = {.offset = offsetof(GpuVertex, u), .format = SG_VERTEXFORMAT_FLOAT2},
                        [2] = {.offset = offsetof(GpuVertex, color),
                               .format = SG_VERTEXFORMAT_UBYTE4N},
                    },
            },
        .colors[0].blend = blend_state(blend_mode),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLES,
        .sample_count = 1,
    });
}

static int ensure_vertex_capacity(GpuBackend* backend, int needed) {
    if (needed <= backend->vertex_capacity) {
        return 0;
    }
    int capacity = backend->vertex_capacity == 0 ? GPU_INITIAL_VERTICES : backend->vertex_capacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    GpuVertex* vertices = realloc(backend->vertices, (size_t)capacity * sizeof(*vertices));
    if (vertices == NULL) {
        return -1;
    }
    backend->vertices = vertices;
    backend->vertex_capacity = capacity;
    return 0;
}

static int ensure_batch_capacity(GpuBackend* backend, int needed) {
    if (needed <= backend->batch_capacity) {
        return 0;
    }
    int capacity = backend->batch_capacity == 0 ? 256 : backend->batch_capacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    GpuBatch* batches = realloc(backend->batches, (size_t)capacity * sizeof(*batches));
    if (batches == NULL) {
        return -1;
    }
    backend->batches = batches;
    backend->batch_capacity = capacity;
    return 0;
}

static int ensure_gpu_capacity(GpuBackend* backend, int needed) {
    if (needed <= backend->gpu_vertex_capacity && backend->vertex_buffer.id != SG_INVALID_ID) {
        return 0;
    }
    int capacity = backend->gpu_vertex_capacity == 0 ? GPU_INITIAL_VERTICES : backend->gpu_vertex_capacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    if (backend->vertex_buffer.id != SG_INVALID_ID) {
        sg_destroy_buffer(backend->vertex_buffer);
    }
    backend->vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
        .size = (size_t)capacity * sizeof(GpuVertex),
        .usage = {.dynamic_update = true},
    });
    if (sg_query_buffer_state(backend->vertex_buffer) != SG_RESOURCESTATE_VALID) {
        return -1;
    }
    backend->gpu_vertex_capacity = capacity;
    return 0;
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
    backend->shader = make_shader();
    backend->sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });
    for (int i = 0; i < GPU_BLEND_MODE_COUNT; i++) {
        backend->pipelines[i] = make_pipeline(backend->shader, i);
    }
    if (sg_query_image_state(backend->color) != SG_RESOURCESTATE_VALID ||
        sg_query_view_state(backend->color_view) != SG_RESOURCESTATE_VALID ||
        sg_query_shader_state(backend->shader) != SG_RESOURCESTATE_VALID ||
        sg_query_sampler_state(backend->sampler) != SG_RESOURCESTATE_VALID) {
        gpu_backend_shutdown(backend);
        return NULL;
    }
    for (int i = 0; i < GPU_BLEND_MODE_COUNT; i++) {
        if (sg_query_pipeline_state(backend->pipelines[i]) != SG_RESOURCESTATE_VALID) {
            gpu_backend_shutdown(backend);
            return NULL;
        }
    }
    if (ensure_gpu_capacity(backend, GPU_INITIAL_VERTICES) != 0) {
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
    backend->vertex_count = 0;
    backend->batch_count = 0;
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
    if (ensure_vertex_capacity(backend, backend->vertex_count + 3) != 0) {
        return;
    }
    int blend_mode = shade.blend_mode;
    if (blend_mode < 0 || blend_mode >= GPU_BLEND_MODE_COUNT) {
        blend_mode = GPU_BLEND_MODE_NORMAL;
    }
    if (backend->batch_count == 0 || backend->batches[backend->batch_count - 1].page_index != page_index ||
        backend->batches[backend->batch_count - 1].blend_mode != blend_mode) {
        if (ensure_batch_capacity(backend, backend->batch_count + 1) != 0) {
            return;
        }
        backend->batches[backend->batch_count] = (GpuBatch){
            .page_index = page_index,
            .blend_mode = blend_mode,
            .base_vertex = backend->vertex_count,
            .vertex_count = 0,
        };
        backend->batch_count++;
    }

    int indices[3] = {triangle.a, triangle.b, triangle.c};
    uint32_t color = color_pack(shade.color[0], shade.color[1], shade.color[2], shade.color[3]);
    for (int i = 0; i < 3; i++) {
        int index = indices[i];
        float world_x = vertices[index * 2];
        float world_y = vertices[index * 2 + 1];
        GpuVertex* out = &backend->vertices[backend->vertex_count++];
        out->x = (world_x - transform.min_x) * transform.scale / (float)backend->width;
        out->y = ((float)(backend->height - 1) - (world_y - transform.min_y) * transform.scale) /
                 (float)backend->height;
        out->u = uvs[index * 2];
        out->v = uvs[index * 2 + 1];
        out->color = color;
    }
    backend->batches[backend->batch_count - 1].vertex_count += 3;
}

int gpu_backend_end_frame(GpuBackend* backend, RgbaImage* out) {
    if (backend == NULL || out == NULL) {
        return -1;
    }
    if (backend->vertex_count > 0 && ensure_gpu_capacity(backend, backend->vertex_count) != 0) {
        return -1;
    }
    if (backend->vertex_count > 0) {
        sg_update_buffer(backend->vertex_buffer, &(sg_range){
                                                    .ptr = backend->vertices,
                                                    .size = (size_t)backend->vertex_count *
                                                            sizeof(*backend->vertices),
                                                });
    }

    sg_begin_pass(&(sg_pass){
        .attachments = {.colors[0] = backend->color_view},
        .action = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR,
                                 .clear_value = {0.0f, 0.0f, 0.0f, 0.0f}}},
    });
    for (int i = 0; i < backend->batch_count; i++) {
        GpuBatch* batch = &backend->batches[i];
        sg_apply_pipeline(backend->pipelines[batch->blend_mode]);
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = backend->vertex_buffer,
            .views[0] = backend->image_views[batch->page_index],
            .samplers[0] = backend->sampler,
        });
        sg_draw(batch->base_vertex, batch->vertex_count, 1);
    }
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
        sg_destroy_view(backend->image_views[i]);
        sg_destroy_image(backend->images[i]);
    }
    free(backend->image_views);
    free(backend->images);
    free(backend->batches);
    free(backend->vertices);
    for (int i = 0; i < GPU_BLEND_MODE_COUNT; i++) {
        sg_destroy_pipeline(backend->pipelines[i]);
    }
    sg_destroy_sampler(backend->sampler);
    sg_destroy_shader(backend->shader);
    sg_destroy_buffer(backend->vertex_buffer);
    sg_destroy_view(backend->color_view);
    sg_destroy_image(backend->color);
    sg_shutdown();
    gl_context_destroy(backend->gl);
    free(backend);
}
