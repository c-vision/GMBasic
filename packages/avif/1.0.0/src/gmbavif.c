/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbavif.c — shim FFI del pacchetto gmbsuite `avif`: AVIF (decode/encode) via libavif
 * compilata dai SORGENTI e linkata staticamente dentro questo dylib (insieme a libaom +
 * libvmaf statiche) -> self-contained, nessuna lib di sistema. Il build lo fa il package
 * manager (build.sh: fetch libavif + link statico).
 *
 * Interfaccia BASIC (namespace AVIF, .inc): firma OMOGENEA (tutti LONG), stringhe AS STRING,
 * output scalare via BYREF LONG. Interscambio via PPM (P6 RGB8) self-contained. */
#include "gmbavif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <avif/avif.h>

int gmbavif_available(void) { return 1; }

unsigned char* gmbavif_decode_rgba(const char* path, long* w, long* h)
{
    if (!path) return NULL;
    avifDecoder* dec = avifDecoderCreate();
    if (!dec) return NULL;
    avifResult r = avifDecoderSetIOFile(dec, path);
    if (r == AVIF_RESULT_OK) r = avifDecoderParse(dec);
    if (r == AVIF_RESULT_OK) r = avifDecoderNextImage(dec);
    if (r != AVIF_RESULT_OK) { avifDecoderDestroy(dec); return NULL; }
    avifImage* img = dec->image;
    avifRGBImage rgb; avifRGBImageSetDefaults(&rgb, img);
    rgb.format = AVIF_RGB_FORMAT_RGBA; rgb.depth = 8;
    if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK) { avifDecoderDestroy(dec); return NULL; }
    if (avifImageYUVToRGB(img, &rgb) != AVIF_RESULT_OK) { avifRGBImageFreePixels(&rgb); avifDecoderDestroy(dec); return NULL; }
    long W = (long)img->width, H = (long)img->height;
    unsigned char* out = (unsigned char*)malloc((size_t)W * (size_t)H * 4);
    if (out) memcpy(out, rgb.pixels, (size_t)W * (size_t)H * 4);
    avifRGBImageFreePixels(&rgb);
    avifDecoderDestroy(dec);
    if (w) *w = W; if (h) *h = H;
    return out;
}

int gmbavif_encode_rgba(const unsigned char* rgba, long w, long h, long quality, const char* out_path)
{
    if (!rgba || !out_path || w < 1 || h < 1) return -1;
    if (quality < 1) quality = 1; if (quality > 100) quality = 100;
    avifImage* img = avifImageCreate((int)w, (int)h, 8, AVIF_PIXEL_FORMAT_YUV444);
    if (!img) return -1;
    avifRGBImage rgb; avifRGBImageSetDefaults(&rgb, img);
    rgb.format = AVIF_RGB_FORMAT_RGBA; rgb.depth = 8;
    if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK) { avifImageDestroy(img); return -1; }
    memcpy(rgb.pixels, rgba, (size_t)w * (size_t)h * 4);
    avifResult r = avifImageRGBToYUV(img, &rgb);
    avifRGBImageFreePixels(&rgb);
    if (r != AVIF_RESULT_OK) { avifImageDestroy(img); return -1; }
    avifEncoder* enc = avifEncoderCreate();
    if (!enc) { avifImageDestroy(img); return -1; }
    enc->quality = quality; enc->speed = 8;
    avifRWData out = AVIF_DATA_EMPTY;
    r = avifEncoderWrite(enc, img, &out);
    if (r != AVIF_RESULT_OK) { avifEncoderDestroy(enc); avifImageDestroy(img); return -1; }
    FILE* f = fopen(out_path, "wb");
    size_t wr = 0;
    if (f) { wr = fwrite(out.data, 1, out.size, f); fclose(f); }
    int ok = (wr == out.size);
    avifRWDataFree(&out);
    avifEncoderDestroy(enc); avifImageDestroy(img);
    return ok ? 0 : -1;
}

void gmbavif_free(void* p) { free(p); }

int gmbavif_info(const char* path, long* w, long* h)
{
    long W = 0, H = 0;
    unsigned char* p = gmbavif_decode_rgba(path, &W, &H);
    if (!p) return -1;
    gmbavif_free(p);
    if (w) *w = W; if (h) *h = H;
    return 0;
}

int gmbavif_decode_ppm(const char* avif_path, const char* ppm_path)
{
    long W = 0, H = 0;
    unsigned char* rgba = gmbavif_decode_rgba(avif_path, &W, &H);
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

int gmbavif_encode_ppm(const char* ppm_path, const char* avif_path, long quality)
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
    int rc = gmbavif_encode_rgba(rgba, w, h, quality, avif_path);
    free(rgba);
    return rc;
}

int gmbavif_convert(const char* in_path, const char* out_path, long quality)
{
    if (!in_path || !out_path) return -1;
    size_t l = strlen(out_path);
    int to_ppm = (l >= 4 && (strcasecmp(out_path + l - 4, ".ppm") == 0 || strcasecmp(out_path + l - 4, ".pgm") == 0));
    return to_ppm ? gmbavif_decode_ppm(in_path, out_path) : gmbavif_encode_ppm(in_path, out_path, quality);
}
