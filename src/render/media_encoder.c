#include "media_encoder.h"

#include <stdio.h>
#include <stdlib.h>

#include <zf_log/zf_log.h>

#if defined(HAVE_FFMPEG)
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libavutil/rational.h>
    #include <libswscale/swscale.h>
#endif

void media_encoder_set_verbose(int verbose) {
#if defined(HAVE_FFMPEG)
    av_log_set_level(verbose ? AV_LOG_INFO : AV_LOG_QUIET);
#else
    (void)verbose;
#endif
}

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
struct MediaEncoder {
    AVFormatContext* format;
    AVCodecContext* codec;
    AVStream* stream;
    SwsContext* scale;
    AVFrame* frame;
    AVPacket* packet;
    unsigned char* rgba;
};

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
    return output == RENDER_OUTPUT_GIF ? AV_PIX_FMT_RGB8 : AV_PIX_FMT_YUV420P;
}

static void encoder_free(MediaEncoder* state) {
    if (state == NULL) {
        return;
    }
    free(state->rgba);
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
    free(state);
}

static int write_packet(MediaEncoder* state) {
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

static int send_frame(MediaEncoder* state, AVFrame* frame) {
    int result = avcodec_send_frame(state->codec, frame);
    return result < 0 ? result : write_packet(state);
}

MediaEncoder* media_encoder_open(const MediaEncodeRequest* request, int width, int height) {
    if (request == NULL || request->output == RENDER_OUTPUT_IMAGE || request->fps <= 0.0 ||
        request->output_path == NULL || width <= 0 || height <= 0) {
        return NULL;
    }

    enum AVCodecID codec_id = codec_id_for(request);
    const AVCodec* codec = find_encoder_for(request);
    if (codec == NULL) {
        ZF_LOGE("FFmpeg encoder is unavailable for %s", media_output_extension(request->output));
        return NULL;
    }

    MediaEncoder* state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return NULL;
    }

    int result = avformat_alloc_output_context2(&state->format, NULL, NULL, request->output_path);
    if (result < 0 || state->format == NULL) {
        encoder_free(state);
        return NULL;
    }

    state->stream = avformat_new_stream(state->format, NULL);
    state->codec = avcodec_alloc_context3(codec);
    state->packet = av_packet_alloc();
    state->frame = av_frame_alloc();
    state->rgba = malloc((size_t)width * (size_t)height * 4);
    if (state->stream == NULL || state->codec == NULL || state->packet == NULL ||
        state->frame == NULL || state->rgba == NULL) {
        encoder_free(state);
        return NULL;
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
        encoder_free(state);
        return NULL;
    }

    state->frame->format = state->codec->pix_fmt;
    state->frame->width = width;
    state->frame->height = height;
    if (av_frame_get_buffer(state->frame, 32) < 0) {
        encoder_free(state);
        return NULL;
    }

    state->scale = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height,
                                  state->codec->pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
    if (state->scale == NULL) {
        encoder_free(state);
        return NULL;
    }

    if (!(state->format->oformat->flags & AVFMT_NOFILE) &&
        avio_open(&state->format->pb, request->output_path, AVIO_FLAG_WRITE) < 0) {
        encoder_free(state);
        return NULL;
    }

    if (avformat_write_header(state->format, NULL) < 0) {
        encoder_free(state);
        return NULL;
    }

    ZF_LOGI("encoding %s", request->output_path);
    return state;
}

int media_encoder_write(MediaEncoder* encoder, const RgbaImage* image, int frame_index) {
    if (encoder == NULL || image == NULL || image->pixels == NULL ||
        image->width != encoder->codec->width || image->height != encoder->codec->height) {
        return -1;
    }

    size_t pixel_count = (size_t)image->width * (size_t)image->height;
    for (size_t i = 0; i < pixel_count; i++) {
        const unsigned char* src = image->pixels + i * 4;
        unsigned char* dst = encoder->rgba + i * 4;
        unsigned alpha = src[3];
        dst[0] = (unsigned char)((src[0] * alpha + 127) / 255);
        dst[1] = (unsigned char)((src[1] * alpha + 127) / 255);
        dst[2] = (unsigned char)((src[2] * alpha + 127) / 255);
        dst[3] = 255;
    }

    int result = av_frame_make_writable(encoder->frame);
    if (result < 0) {
        return -1;
    }

    const uint8_t* src_data[1] = {encoder->rgba};
    int src_linesize[1] = {image->width * 4};
    sws_scale(encoder->scale, src_data, src_linesize, 0, image->height, encoder->frame->data,
              encoder->frame->linesize);
    encoder->frame->pts = frame_index;
    return send_frame(encoder, encoder->frame) < 0 ? -1 : 0;
}

int media_encoder_close(MediaEncoder* encoder) {
    if (encoder == NULL) {
        return -1;
    }

    int result = send_frame(encoder, NULL) == 0 && av_write_trailer(encoder->format) == 0 ? 0 : -1;
    if (result == 0) {
        ZF_LOGI("wrote %s", encoder->format->url);
    } else {
        ZF_LOGE("encode failed: %s", encoder->format->url);
    }
    encoder_free(encoder);
    return result;
}
#else
MediaEncoder* media_encoder_open(const MediaEncodeRequest* request, int width, int height) {
    (void)request;
    (void)width;
    (void)height;
    ZF_LOGE("FFmpeg support is disabled. Configure with ENABLE_FFMPEG=ON and FFMPEG_ROOT, or use "
            "FFMPEG_PROVIDER=external.");
    return NULL;
}

int media_encoder_write(MediaEncoder* encoder, const RgbaImage* image, int frame_index) {
    (void)encoder;
    (void)image;
    (void)frame_index;
    return -1;
}

int media_encoder_close(MediaEncoder* encoder) {
    (void)encoder;
    return -1;
}
#endif
