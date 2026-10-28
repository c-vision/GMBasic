/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbheif.c — shim FFI del pacchetto gmbsuite `heif`: HEIC/HEIF (decode/encode) via libheif
 * compilata DAI SORGENTI e linkata staticamente dentro questo dylib (insieme a libde265 +
 * x265 statiche) -> self-contained, nessuna lib di sistema. Il build lo fa il package manager.
 *
 * Interfaccia BASIC (namespace HEIF, .inc): firma OMOGENEA (tutti LONG), stringhe AS STRING,
 * output scalare via BYREF LONG. Interscambio via PPM (P6 RGB8) self-contained. */
#include "gmbheif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <libheif/heif.h>

int gmbheif_available(void) { return 1; }

/* libheif richiede heif_init() per registrare i codec (de265/x265). Idempotente. */
static void gmbheif_ensure_init(void) {
    static int done = 0;
    if (!done) { heif_init(NULL); done = 1; }
}

unsigned char* gmbheif_decode_rgba(const char* path, long* w, long* h)
{
    if (!path) return NULL;
    gmbheif_ensure_init();
    struct heif_context* ctx = heif_context_alloc();
    if (!ctx) return NULL;
    struct heif_error e = heif_context_read_from_file(ctx, path, NULL);
    if (e.code != heif_error_Ok) { heif_context_free(ctx); return NULL; }
    struct heif_image_handle* handle = NULL;
    e = heif_context_get_primary_image_handle(ctx, &handle);
    if (e.code != heif_error_Ok) { heif_context_free(ctx); return NULL; }
    struct heif_image* img = NULL;
    e = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, NULL);
    if (e.code != heif_error_Ok) { heif_image_handle_release(handle); heif_context_free(ctx); return NULL; }
    int W = heif_image_get_width(img, heif_channel_interleaved);
    int H = heif_image_get_height(img, heif_channel_interleaved);
    int stride = 0;
    const uint8_t* data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
    unsigned char* out = (unsigned char*)malloc((size_t)W * (size_t)H * 4);
    if (out && data) for (int y = 0; y < H; y++) memcpy(out + (size_t)y * W * 4, data + (size_t)y * stride, (size_t)W * 4);
    heif_image_release(img); heif_image_handle_release(handle); heif_context_free(ctx);
    if (w) *w = W; if (h) *h = H;
    return out;
}

int gmbheif_encode_rgba(const unsigned char* rgba, long w, long h, long quality, const char* out_path)
{
    if (!rgba || !out_path || w < 1 || h < 1) return -1;
    if (quality < 1) quality = 1; if (quality > 100) quality = 100;
    gmbheif_ensure_init();
    struct heif_context* ctx = heif_context_alloc();
    if (!ctx) return -1;
    struct heif_encoder* enc = NULL;
    struct heif_error e = heif_context_get_encoder_for_format(ctx, heif_compression_HEVC, &enc);
    if (e.code != heif_error_Ok) { heif_context_free(ctx); return -1; }
    heif_encoder_set_lossy_quality(enc, (int)quality);
    struct heif_image* img = NULL;
    e = heif_image_create((int)w, (int)h, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, &img);
    if (e.code != heif_error_Ok) { heif_encoder_release(enc); heif_context_free(ctx); return -1; }
    e = heif_image_add_plane(img, heif_channel_interleaved, (int)w, (int)h, 8);
    if (e.code != heif_error_Ok) { heif_image_release(img); heif_encoder_release(enc); heif_context_free(ctx); return -1; }
    int stride = 0;
    uint8_t* p = heif_image_get_plane(img, heif_channel_interleaved, &stride);
    for (long y = 0; y < h; y++) memcpy(p + (size_t)y * stride, rgba + (size_t)y * w * 4, (size_t)w * 4);
    struct heif_image_handle* handle = NULL;
    e = heif_context_encode_image(ctx, img, enc, NULL, &handle);
    int rc = -1;
    if (e.code == heif_error_Ok) {
        e = heif_context_write_to_file(ctx, out_path);
        if (e.code == heif_error_Ok) rc = 0;

    }
    if (handle) heif_image_handle_release(handle);
    heif_image_release(img); heif_encoder_release(enc); heif_context_free(ctx);
    return rc;
}

void gmbheif_free(void* p) { free(p); }

int gmbheif_info(const char* path, long* w, long* h)
{
    long W = 0, H = 0;
    unsigned char* p = gmbheif_decode_rgba(path, &W, &H);
    if (!p) return -1;
    gmbheif_free(p);
    if (w) *w = W; if (h) *h = H;
    return 0;
}

int gmbheif_decode_ppm(const char* heif_path, const char* ppm_path)
{
    long W = 0, H = 0;
    unsigned char* rgba = gmbheif_decode_rgba(heif_path, &W, &H);
    if (!rgba || W < 1 || H < 1) return -1;
    FILE* o = fopen(ppm_path, "wb");
    if (!o) { free(rgba); return -1; }
    fprintf(o, "P6\n%ld %ld\n255\n", W, H);
    unsigned char* row = (unsigned char*)malloc((size_t)W * 3);
    int rc = 0;
    if (!row) rc = -1;
    else for (long y = 0; y < H; y++) {
        for (long x = 0; x < W; x++) { const unsigned char* p = &rgba[((size_t)y * W + x) * 4]; row[x * 3] = p[0]; row[x * 3 + 1] = p[1]; row[x * 3 + 2] = p[2]; }
        if (fwrite(row, 1, (size_t)W * 3, o) != (size_t)W * 3) { rc = -1; break; }
    }
    free(row); fclose(o); free(rgba);
    return rc;
}

int gmbheif_encode_ppm(const char* ppm_path, const char* heif_path, long quality)
{
    FILE* f = fopen(ppm_path, "rb");
    if (!f) return -1;
    char magic[3] = {0}; long w = 0, h = 0, maxv = 0;
    if (fscanf(f, "%2s %ld %ld %ld", magic, &w, &h, &maxv) != 4 || strcmp(magic, "P6") != 0 || maxv != 255 || w < 1 || h < 1) { fclose(f); return -1; }
    fgetc(f);
    size_t n = (size_t)w * (size_t)h;
    unsigned char* rgb = (unsigned char*)malloc(n * 3);
    if (!rgb || fread(rgb, 1, n * 3, f) != n * 3) { free(rgb); fclose(f); return -1; }
    fclose(f);
    unsigned char* rgba = (unsigned char*)malloc(n * 4);
    if (!rgba) { free(rgb); return -1; }
    for (size_t i = 0; i < n; i++) { rgba[i * 4] = rgb[i * 3]; rgba[i * 4 + 1] = rgb[i * 3 + 1]; rgba[i * 4 + 2] = rgb[i * 3 + 2]; rgba[i * 4 + 3] = 255; }
    free(rgb);
    int rc = gmbheif_encode_rgba(rgba, w, h, quality, heif_path);
    free(rgba);
    return rc;
}

int gmbheif_convert(const char* in_path, const char* out_path, long quality)
{
    if (!in_path || !out_path) return -1;
    size_t l = strlen(out_path);
    int to_ppm = (l >= 4 && (strcasecmp(out_path + l - 4, ".ppm") == 0 || strcasecmp(out_path + l - 4, ".pgm") == 0));
    return to_ppm ? gmbheif_decode_ppm(in_path, out_path) : gmbheif_encode_ppm(in_path, out_path, quality);
}
