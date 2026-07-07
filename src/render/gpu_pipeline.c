#include "gpu_pipeline.h"

#include <stddef.h>

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
                          "  gl_Position = vec4(position.x * 2.0 - 1.0, 1.0 - position.y * 2.0, "
                          "0.0, 1.0);\n"
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
        .samplers[0] = {.stage = SG_SHADERSTAGE_FRAGMENT, .sampler_type = SG_SAMPLERTYPE_FILTERING},
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
                .buffers[0].stride = 20,
                .attrs =
                    {
                        [0] = {.offset = 0, .format = SG_VERTEXFORMAT_FLOAT2},
                        [1] = {.offset = 8, .format = SG_VERTEXFORMAT_FLOAT2},
                        [2] = {.offset = 16, .format = SG_VERTEXFORMAT_UBYTE4N},
                    },
            },
        .colors[0].blend = blend_state(blend_mode),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLES,
        .sample_count = 1,
    });
}

int gpu_pipeline_init(GpuPipelines* pipelines) {
    if (pipelines == NULL) {
        return -1;
    }
    *pipelines = (GpuPipelines){};
    pipelines->shader = make_shader();
    pipelines->sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });
    for (int i = 0; i < GPU_BLEND_MODE_COUNT; i++) {
        pipelines->pipelines[i] = make_pipeline(pipelines->shader, i);
    }
    if (sg_query_shader_state(pipelines->shader) != SG_RESOURCESTATE_VALID ||
        sg_query_sampler_state(pipelines->sampler) != SG_RESOURCESTATE_VALID) {
        return -1;
    }
    for (int i = 0; i < GPU_BLEND_MODE_COUNT; i++) {
        if (sg_query_pipeline_state(pipelines->pipelines[i]) != SG_RESOURCESTATE_VALID) {
            return -1;
        }
    }
    return 0;
}

void gpu_pipeline_dispose(GpuPipelines* pipelines) {
    if (pipelines == NULL) {
        return;
    }
    for (int i = 0; i < GPU_BLEND_MODE_COUNT; i++) {
        sg_destroy_pipeline(pipelines->pipelines[i]);
    }
    sg_destroy_sampler(pipelines->sampler);
    sg_destroy_shader(pipelines->shader);
    *pipelines = (GpuPipelines){};
}
