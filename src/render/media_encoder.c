#include "media_encoder.h"

#include <stdio.h>
#include <stdlib.h>

#include <zf_log/zf_log.h>

#include "image_io.h"
#include "path.h"
#include "render_canvas.h"

#if defined(HAVE_FFMPEG)
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libavutil/rational.h>
    #include <libswscale/swscale.h>
#endif

const char* media_output_extension(RenderOutputKind output) {
    switch (output) {
        case RENDER_OUTPUT_VIDEO:
            return "mkv";
        case RENDER_OUTPUT_GIF:
            return "gif";
        case RENDER_OUTPUT_IMAGE:
        default:
            return "png";
    }
}

#if defined(HAVE_FFMPEG)
typedef struct EncoderState {
    AVFormatContext* format;
    AVCodecContext* codec;
    AVStream* stream;
    SwsContext* scale;
    AVFrame* frame;
    AVPacket* packet;
} EncoderState;

static enum AVCodecID codec_id_for(const MediaEncodeRequest* request) {
    if (request->output == RENDER_OUTPUT_GIF) {
        return AV_CODEC_ID_GIF;
    }
    if (request->codec == RENDER_VIDEO_CODEC_H264) {
        return AV_CODEC_ID_H264;
    }
    if (request->codec == RENDER_VIDEO_CODEC_FFV1) {
        return AV_CODEC_ID_FFV1;
    }
    return AV_CODEC_ID_MPEG4;
}

static const AVCodec* find_encoder_for(const MediaEncodeRequest* request) {
    if (request->output == RENDER_OUTPUT_VIDEO && request->codec == RENDER_VIDEO_CODEC_H264) {
        const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
        return codec == NULL ? avcodec_find_encoder(AV_CODEC_ID_H264) : codec;
    }
    return avcodec_find_encoder(codec_id_for(request));
}

static enum AVPixelFormat pixel_format_for(RenderOutputKind output) {
    return output == RENDER_OUTPUT_GIF ? AV_PIX_FMT_PAL8 : AV_PIX_FMT_YUV420P;
}

static void encoder_free(EncoderState* state) {
    if (state == NULL) {
        return;
    }
    sws_freeContext(state->scale);
    av_frame_free(&state->frame);
    av_packet_free(&state->packet);
    avcodec_free_context(&state->codec);
    if (state->format != NULL) {
        if (!(state->format->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&state->format->pb);
        }
        avformat_free_context(state->format);
    }
}

static int write_packet(EncoderState* state) {
    while (avcodec_receive_packet(state->codec, state->packet) == 0) {
        av_packet_rescale_ts(state->packet, state->codec->time_base, state->stream->time_base);
        state->packet->stream_index = state->stream->index;
        int result = av_interleaved_write_frame(state->format, state->packet);
        av_packet_unref(state->packet);
        if (result < 0) {
            return result;
        }
    }
    return 0;
}

static int send_frame(EncoderState* state, AVFrame* frame) {
    int result = avcodec_send_frame(state->codec, frame);
    return result < 0 ? result : write_packet(state);
}

static int open_encoder(const MediaEncodeRequest* request,
                        int width,
                        int height,
                        EncoderState* state) {
    enum AVCodecID codec_id = codec_id_for(request);
    const AVCodec* codec = find_encoder_for(request);
    if (codec == NULL) {
        ZF_LOGE("FFmpeg encoder is unavailable for %s", media_output_extension(request->output));
        return -1;
    }

    int result = avformat_alloc_output_context2(&state->format, NULL, NULL, request->output_path);
    if (result < 0 || state->format == NULL) {
        return -1;
    }

    state->stream = avformat_new_stream(state->format, NULL);
    state->codec = avcodec_alloc_context3(codec);
    state->packet = av_packet_alloc();
    state->frame = av_frame_alloc();
    if (state->stream == NULL || state->codec == NULL || state->packet == NULL ||
        state->frame == NULL) {
        return -1;
    }

    AVRational frame_rate = av_d2q(request->fps, 100000);
    state->stream->time_base = av_inv_q(frame_rate);
    state->codec->codec_id = codec_id;
    state->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    state->codec->width = width;
    state->codec->height = height;
    state->codec->time_base = state->stream->time_base;
    state->codec->framerate = frame_rate;
    state->codec->pix_fmt = pixel_format_for(request->output);
    state->codec->gop_size = 12;
    state->codec->max_b_frames = 0;

    if (state->format->oformat->flags & AVFMT_GLOBALHEADER) {
        state->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    result = avcodec_open2(state->codec, codec, NULL);
    if (result < 0 || avcodec_parameters_from_context(state->stream->codecpar, state->codec) < 0) {
        return -1;
    }

    state->frame->format = state->codec->pix_fmt;
    state->frame->width = width;
    state->frame->height = height;
    if (av_frame_get_buffer(state->frame, 32) < 0) {
        return -1;
    }

    state->scale = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height,
                                  state->codec->pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
    if (state->scale == NULL) {
        return -1;
    }

    if (!(state->format->oformat->flags & AVFMT_NOFILE) &&
        avio_open(&state->format->pb, request->output_path, AVIO_FLAG_WRITE) < 0) {
        return -1;
    }

    return avformat_write_header(state->format, NULL);
}

static int encode_png_frame(EncoderState* state, const char* path, int frame_index) {
    RgbaImage image = {};
    unsigned width = 0;
    unsigned height = 0;
    if (image_decode_png32_file(&image.pixels, &width, &height, path) != 0) {
        return -1;
    }
    image.width = (int)width;
    image.height = (int)height;

    int result = av_frame_make_writable(state->frame);
    if (result >= 0) {
        const uint8_t* src_data[1] = {image.pixels};
        int src_linesize[1] = {image.width * 4};
        sws_scale(state->scale, src_data, src_linesize, 0, image.height, state->frame->data,
                  state->frame->linesize);
        state->frame->pts = frame_index;
        result = send_frame(state, state->frame);
    }

    rgba_image_free(&image);
    return result < 0 ? -1 : 0;
}

int media_encode_frames(const MediaEncodeRequest* request) {
    if (request == NULL || request->output == RENDER_OUTPUT_IMAGE || request->fps <= 0.0 ||
        request->frame_count <= 0 || request->frame_dir == NULL || request->output_path == NULL) {
        return -1;
    }

    char first_frame[1024];
    if (path_join(request->frame_dir, "frame_00000.png", first_frame, sizeof(first_frame)) != 0) {
        return -1;
    }

    RgbaImage first = {};
    unsigned width = 0;
    unsigned height = 0;
    if (image_decode_png32_file(&first.pixels, &width, &height, first_frame) != 0) {
        return -1;
    }
    rgba_image_free(&first);

    EncoderState state = {};
    if (open_encoder(request, (int)width, (int)height, &state) != 0) {
        encoder_free(&state);
        ZF_LOGE("could not open FFmpeg encoder");
        return -1;
    }

    ZF_LOGI("encoding %s", request->output_path);
    int result = 0;
    for (int i = 0; i < request->frame_count; i++) {
        char file_name[64];
        char frame_path[1024];
        snprintf(file_name, sizeof(file_name), "frame_%05d.png", i);
        if (path_join(request->frame_dir, file_name, frame_path, sizeof(frame_path)) != 0 ||
            encode_png_frame(&state, frame_path, i) != 0) {
            result = -1;
            break;
        }
    }

    if (result == 0 && send_frame(&state, NULL) == 0 && av_write_trailer(state.format) == 0) {
        ZF_LOGI("wrote %s", request->output_path);
    } else {
        result = -1;
        ZF_LOGE("encode failed: %s", request->output_path);
    }

    encoder_free(&state);
    return result;
}
#else
int media_encode_frames(const MediaEncodeRequest* request) {
    (void)request;
    ZF_LOGE("FFmpeg support is disabled. Configure with ENABLE_FFMPEG=ON and FFMPEG_ROOT, or use "
            "FFMPEG_PROVIDER=external.");
    return -1;
}
#endif
