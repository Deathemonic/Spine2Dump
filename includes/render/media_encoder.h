#ifndef SPINE2DUMP_MEDIA_ENCODER_H
#define SPINE2DUMP_MEDIA_ENCODER_H

#include "render_canvas.h"
#include "render_options.h"

typedef struct MediaEncoder MediaEncoder;

typedef struct MediaEncodeRequest {
    const char* output_path;
    double fps;
    RenderOutputKind output;
    RenderVideoCodec codec;
} MediaEncodeRequest;

const char* media_output_extension(RenderOutputKind output);
MediaEncoder* media_encoder_open(const MediaEncodeRequest* request, int width, int height);
int media_encoder_write(MediaEncoder* encoder, const RgbaImage* image, int frame_index);
int media_encoder_close(MediaEncoder* encoder);

#endif
