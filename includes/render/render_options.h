#ifndef SPINE2DUMP_RENDER_OPTIONS_H
#define SPINE2DUMP_RENDER_OPTIONS_H

#include "image_io.h"

#define RENDER_HIDE_MAX 64

typedef enum RenderTrimMode {
    RENDER_TRIM_NONE = 0,
    RENDER_TRIM_FRAME = 1,
    RENDER_TRIM_ANIMATION = 2
} RenderTrimMode;

typedef enum RenderOutputKind {
    RENDER_OUTPUT_IMAGE = 0,
    RENDER_OUTPUT_GIF,
    RENDER_OUTPUT_VIDEO
} RenderOutputKind;

typedef enum RenderVideoCodec {
    RENDER_VIDEO_CODEC_H264 = 0,
    RENDER_VIDEO_CODEC_MPEG4,
    RENDER_VIDEO_CODEC_FFV1
} RenderVideoCodec;

typedef struct RenderCropRect {
    int x;
    int y;
    int width;
    int height;
    int valid;
} RenderCropRect;

typedef struct RenderOptions {
    int width;
    int height;
    float scale;
    int trim;
    int trim_padding;
    RenderCropRect crop;
    unsigned char alpha_threshold;
    PngCompressionPreset png_compression;
    int software;
    const char* hide_patterns[RENDER_HIDE_MAX];
    int hide_count;
} RenderOptions;

static inline RenderOptions render_options_default(void) {
    RenderOptions options = {
        .width = 1024,
        .height = 1024,
        .scale = 1.0f,
        .trim = 0,
        .trim_padding = 0,
        .alpha_threshold = 1,
        .png_compression = PNG_COMPRESSION_BALANCED,
        .software = 0,
        .hide_count = 0,
    };
    return options;
}

#endif
