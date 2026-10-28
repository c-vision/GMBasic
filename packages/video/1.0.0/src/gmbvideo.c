/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbvideo.c — shim FFI del pacchetto gmbsuite `video`: lettura video (probe + estrazione
 * frame) via ffmpeg COMPILATO DAI SORGENTI e linkato staticamente dentro questo dylib
 * -> self-contained, nessuna lib di sistema. Il build lo fa il package manager.
 *
 * Interfaccia BASIC (namespace VIDEO, .inc): firma OMOGENEA (tutti LONG), stringhe AS STRING,
 * output scalare via BYREF LONG. Output frame come PPM (P6 RGB8) self-contained. */
#include "gmbvideo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

int gmbvideo_available(void) { return 1; }

int gmbvideo_info(const char* path, long* w, long* h, long* dur_ms)
{
    if (!path) return -1;
    AVFormatContext* fmt = NULL;
    if (avformat_open_input(&fmt, path, NULL, NULL) < 0) return -1;
    if (avformat_find_stream_info(fmt, NULL) < 0) { avformat_close_input(&fmt); return -1; }
    int vs = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (vs < 0) { avformat_close_input(&fmt); return -1; }
    AVStream* st = fmt->streams[vs];
    if (w) *w = st->codecpar->width;
    if (h) *h = st->codecpar->height;
    if (dur_ms) *dur_ms = (long)((fmt->duration != AV_NOPTS_VALUE) ? fmt->duration / (AV_TIME_BASE / 1000) : 0);
    avformat_close_input(&fmt);
    return 0;
}

/* Estrae un frame al tempo t_ms e lo scrive come PPM (P6 RGB8). 0 ok, -1 err. */
int gmbvideo_frame(const char* path, long t_ms, const char* ppm_out)
{
    if (!path || !ppm_out) return -1;
    AVFormatContext* fmt = NULL;
    if (avformat_open_input(&fmt, path, NULL, NULL) < 0) return -1;
    if (avformat_find_stream_info(fmt, NULL) < 0) { avformat_close_input(&fmt); return -1; }
    int vs = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (vs < 0) { avformat_close_input(&fmt); return -1; }
    AVStream* st = fmt->streams[vs];
    const AVCodec* dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) { avformat_close_input(&fmt); return -1; }
    AVCodecContext* cc = avcodec_alloc_context3(dec);
    if (!cc) { avformat_close_input(&fmt); return -1; }
    if (avcodec_parameters_to_context(cc, st->codecpar) < 0 || avcodec_open2(cc, dec, NULL) < 0) { avcodec_free_context(&cc); avformat_close_input(&fmt); return -1; }

    if (t_ms > 0) {
        int64_t ts = av_rescale(t_ms, st->time_base.den, st->time_base.num * 1000);
        av_seek_frame(fmt, vs, ts, AVSEEK_FLAG_BACKWARD);
    }
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame) { av_packet_free(&pkt); av_frame_free(&frame); avcodec_free_context(&cc); avformat_close_input(&fmt); return -1; }

    int got = 0;
    while (av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index != vs) { av_packet_unref(pkt); continue; }
        if (avcodec_send_packet(cc, pkt) < 0) { av_packet_unref(pkt); continue; }
        if (avcodec_receive_frame(cc, frame) == 0) { got = 1; av_packet_unref(pkt); break; }
        av_packet_unref(pkt);
    }
    if (!got) { av_packet_free(&pkt); av_frame_free(&frame); avcodec_free_context(&cc); avformat_close_input(&fmt); return -1; }

    int W = frame->width, H = frame->height;
    struct SwsContext* sws = sws_getContext(W, H, frame->format, W, H, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
    int rc = -1;
    if (sws) {
        unsigned char* rgb = (unsigned char*)malloc((size_t)W * H * 3);
        uint8_t* dst[4] = { rgb, NULL, NULL, NULL };
        int dststride[4] = { W * 3, 0, 0, 0 };
        sws_scale(sws, (const uint8_t* const*)frame->data, frame->linesize, 0, H, dst, dststride);
        FILE* o = fopen(ppm_out, "wb");
        if (o) {
            fprintf(o, "P6\n%d %d\n255\n", W, H);
            size_t wr = fwrite(rgb, 1, (size_t)W * H * 3, o);
            fclose(o);
            if (wr == (size_t)W * H * 3) rc = 0;
        }
        free(rgb);
        sws_freeContext(sws);
    }
    av_packet_free(&pkt); av_frame_free(&frame); avcodec_free_context(&cc); avformat_close_input(&fmt);
    return rc;
}

int gmbvideo_convert(const char* in_path, const char* ppm_out)
{
    return gmbvideo_frame(in_path, 0, ppm_out);
}
