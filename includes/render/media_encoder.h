#ifndef SPINE2DUMP_MEDIA_ENCODER_H
#define SPINE2DUMP_MEDIA_ENCODER_H

#include "render_options.h"

typedef struct MediaEncodeRequest {
    const char* frame_dir;
    const char* output_path;
    double fps;
    int frame_count;
    RenderOutputKind output;
    RenderVideoCodec codec;
} MediaEncodeRequest;

const char* media_output_extension(RenderOutputKind output);
int media_encode_frames(const MediaEncodeRequest* request);

#endif
