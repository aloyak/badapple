#include "video.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <stdlib.h>

struct Video {
    AVFormatContext* format_ctx;
    AVCodecContext* codec_ctx;
    struct SwsContext* sws_ctx;
    int stream_index;

    AVFrame* frame; // Directly decoded
    AVFrame* rgb_frame; // Converted to RGB24 stream
    AVPacket* packet;

    int width;
    int height;

    double start_time;
    double frame_show_time; // Presentation time of the frame
    bool started;
    bool finished;
};

// Pulls and decodes exactly one video frame, converts it to RGB24, and updates
// frame_pts_seconds. Returns false on EOF or decode failure
static bool decode_next_frame(Video* v) {
    AVStream* stream = v->format_ctx->streams[v->stream_index];

    while (true) {
        int recv = avcodec_receive_frame(v->codec_ctx, v->frame);

        if (recv == 0) {
            sws_scale(v->sws_ctx,
                (const uint8_t* const*)v->frame->data, v->frame->linesize,
                0, v->codec_ctx->height,
                v->rgb_frame->data, v->rgb_frame->linesize
            );


            int64_t pts = v->frame->best_effort_timestamp;
            if (pts == AV_NOPTS_VALUE) pts = v->frame->pts;
            v->frame_show_time = pts * av_q2d(stream->time_base);

            av_frame_unref(v->frame);
            return true;
        }

        if (recv != AVERROR(EAGAIN)) return false;

        int read = av_read_frame(v->format_ctx, v->packet);
        if (read < 0) {
            avcodec_send_packet(v->codec_ctx, NULL);
            continue;
        }

        if (v->packet->stream_index == v->stream_index) {
            avcodec_send_packet(v->codec_ctx, v->packet);
        }
        av_packet_unref(v->packet);
    }
}

Video* video_open(const char* path) {
    Video* v = calloc(1, sizeof(Video));
    if (!v) return NULL;

    if (avformat_open_input(&v->format_ctx, path, NULL, NULL) < 0) {
        printf("No such file or directory!");
        free(v);
        return NULL;
    }
    if (avformat_find_stream_info(v->format_ctx, NULL) < 0) {
        avformat_close_input(&v->format_ctx);
        free(v);
        return NULL;
    }

    const AVCodec* decoder = NULL;
    v->stream_index = av_find_best_stream(v->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (v->stream_index < 0) {
        avformat_close_input(&v->format_ctx);
        free(v);
        return NULL;
    }


    v->codec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(v->codec_ctx, v->format_ctx->streams[v->stream_index]->codecpar);
    if (avcodec_open2(v->codec_ctx, decoder, NULL) < 0) {
        avcodec_free_context(&v->codec_ctx);
        avformat_close_input(&v->format_ctx);
        free(v);
        return NULL;
    }

    v->width = v->codec_ctx->width;
    v->height = v->codec_ctx->height;

    v->sws_ctx = sws_getContext(
        v->width, v->height, v->codec_ctx->pix_fmt,
        v->width, v->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    v->frame = av_frame_alloc();
    v->rgb_frame = av_frame_alloc();
    v->packet = av_packet_alloc();


    int rgb_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, v->width, v->height, 1);
    uint8_t* rgb_buffer = av_malloc(rgb_bytes);
    av_image_fill_arrays(
        v->rgb_frame->data, v->rgb_frame->linesize, rgb_buffer,
        AV_PIX_FMT_RGB24, v->width, v->height, 1
    );
    
    v->started = false;
    v->finished = false;
    return v;
}

void video_close(Video* v) {
    if (!v) return;

    if (v->rgb_frame) {
        av_freep(&v->rgb_frame->data[0]);
        av_frame_free(&v->rgb_frame);
    }
    av_frame_free(&v->frame);
    av_packet_free(&v->packet);
    if (v->sws_ctx) sws_freeContext(v->sws_ctx);
    if (v->codec_ctx) avcodec_free_context(&v->codec_ctx);
    if (v->format_ctx) avformat_close_input(&v->format_ctx);

    free(v);
}

int video_width(Video* v) { return v ? v->width : 0; }
int video_height(Video* v) { return v ? v->height : 0; }
bool video_finished(Video* v) { return v ? v->finished : 0; }

unsigned char* video_get_frame(Video* v, double time, bool* out) {
    if (out) *out = false;
    if (!v) return NULL;
    if (v->finished) return v->rgb_frame->data[0];

    if (!v->started) {
        v->start_time = time;
        v->started = true;
        if (decode_next_frame(v)) {
            if (out) *out = true;
        } else {
            v->finished = true;
        }

        return v->rgb_frame->data[0];
    }

    double elapsed = time - v->start_time;
    bool updated = false;

    // If elapsed >= frame_pts_seconds, decode forward (possibly more than one
    // frame in a row) until we reach a frame that is not yet due to be shown
    while (elapsed >= v->frame_show_time) {
        if (!decode_next_frame(v)) {
            v->finished = true;
            break;
        }
        updated = true;
    }

    if (out) *out = updated;
    return v->rgb_frame->data[0];
}
