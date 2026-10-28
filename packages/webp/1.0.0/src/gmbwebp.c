/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbwebp.c — shim FFI del pacchetto gmbsuite `webp`: WebP (decode/encode) via libwebp
 * linkata STATICAMENTE dentro questo dylib (self-contained, come il pacchetto `duckdb`).
 * Nessuna lib esterna a runtime: la libwebp e' INSTALLATA col pacchetto.
 *
 * Interfaccia BASIC (namespace WEBP, .inc): firma OMOGENEA (tutti LONG), stringhe AS STRING,
 * output scalare via BYREF LONG. Interscambio con PPM (P6) self-contained (nessuna dipendenza). */
#include "gmbwebp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <webp/decode.h>
#include <webp/encode.h>

int gmbwebp_available(void) { return 1; }

int gmbwebp_info(const char* path, long* w, long* h)
{
    if (!path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 1) { fclose(f); return -1; }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);
    int iw = 0, ih = 0;
    int ok = WebPGetInfo(buf, (size_t)sz, &iw, &ih);
    free(buf);
    if (!ok) return -1;
    if (w) *w = iw;
    if (h) *h = ih;
    return 0;
}

/* Decodifica un WebP in un file PPM (P6, RGB 8-bit). 0 ok, -1 errore. */
int gmbwebp_decode_ppm(const char* webp_path, const char* ppm_path)
{
    if (!webp_path || !ppm_path) return -1;
    FILE* f = fopen(webp_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 1) { fclose(f); return -1; }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);
    int w = 0, h = 0;
    unsigned char* rgba = WebPDecodeRGBA(buf, (size_t)sz, &w, &h);
    free(buf);
    if (!rgba || w < 1 || h < 1) return -1;
    FILE* o = fopen(ppm_path, "wb");
    if (!o) { WebPFree(rgba); return -1; }
    fprintf(o, "P6\n%d %d\n255\n", w, h);
    unsigned char* row = (unsigned char*)malloc((size_t)w * 3);
    int rc = 0;
    if (!row) rc = -1;
    else for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) { const unsigned char* p = &rgba[((size_t)y * w + x) * 4]; row[x * 3] = p[0]; row[x * 3 + 1] = p[1]; row[x * 3 + 2] = p[2]; }
        if (fwrite(row, 1, (size_t)w * 3, o) != (size_t)w * 3) { rc = -1; break; }
    }
    free(row); fclose(o); WebPFree(rgba);
    return rc;
}

/* Codifica un PPM (P6, RGB 8-bit) in WebP (lossy, quality 1..100). 0 ok, -1 errore. */
int gmbwebp_encode_ppm(const char* ppm_path, const char* webp_path, long quality)
{
    if (!ppm_path || !webp_path) return -1;
    if (quality < 1) quality = 1; if (quality > 100) quality = 100;
    FILE* f = fopen(ppm_path, "rb");
    if (!f) return -1;
    char magic[3] = {0}; int w = 0, h = 0, maxv = 0;
    if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &maxv) != 4 || strcmp(magic, "P6") != 0 || maxv != 255 || w < 1 || h < 1) { fclose(f); return -1; }
    fgetc(f);                                   /* singolo whitespace dopo l'header */
    size_t n = (size_t)w * h;
    unsigned char* rgb = (unsigned char*)malloc(n * 3);
    if (!rgb || fread(rgb, 1, n * 3, f) != n * 3) { free(rgb); fclose(f); return -1; }
    fclose(f);
    unsigned char* rgba = (unsigned char*)malloc(n * 4);
    if (!rgba) { free(rgb); return -1; }
    for (size_t i = 0; i < n; i++) { rgba[i * 4] = rgb[i * 3]; rgba[i * 4 + 1] = rgb[i * 3 + 1]; rgba[i * 4 + 2] = rgb[i * 3 + 2]; rgba[i * 4 + 3] = 255; }
    free(rgb);
    uint8_t* out = NULL;
    size_t outn = WebPEncodeRGBA(rgba, w, h, w * 4, (float)quality, &out);
    free(rgba);
    if (outn == 0 || !out) return -1;
    FILE* o = fopen(webp_path, "wb");
    if (!o) { WebPFree(out); return -1; }
    size_t wr = fwrite(out, 1, outn, o);
    fclose(o); WebPFree(out);
    return wr == outn ? 0 : -1;
}

/* Decodifica WebP -> buffer RGBA (malloc; liberare con gmbwebp_free). w/h via out. NULL err.
 * Usata dall'include nativo gmbmedia (mm_webp) che carica questo shim dal pacchetto. */
unsigned char* gmbwebp_decode_rgba(const char* path, long* w, long* h)
{
    if (!path) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 1) { fclose(f); return NULL; }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    int iw = 0, ih = 0;
    unsigned char* rgba = WebPDecodeRGBA(buf, (size_t)sz, &iw, &ih);
    free(buf);
    if (!rgba) return NULL;
    if (w) *w = iw; if (h) *h = ih;
    return rgba;
}

/* Codifica buffer RGBA -> WebP. 0 ok, -1 err. */
int gmbwebp_encode_rgba(const unsigned char* rgba, long w, long h, long quality, const char* out_path)
{
    if (!rgba || !out_path || w < 1 || h < 1) return -1;
    if (quality < 1) quality = 1; if (quality > 100) quality = 100;
    uint8_t* out = NULL;
    size_t outn = WebPEncodeRGBA(rgba, (int)w, (int)h, (int)(w * 4), (float)quality, &out);
    if (outn == 0 || !out) return -1;
    FILE* o = fopen(out_path, "wb");
    if (!o) { WebPFree(out); return -1; }
    size_t wr = fwrite(out, 1, outn, o);
    fclose(o); WebPFree(out);
    return wr == outn ? 0 : -1;
}

void gmbwebp_free(void* p) { if (p) WebPFree(p); }

/* Converte WebP -> PPM o PPM -> WebP in base all'estensione di out (".ppm"/".pgm" vs altro). */
int gmbwebp_convert(const char* in_path, const char* out_path, long quality)
{
    if (!in_path || !out_path) return -1;
    size_t l = strlen(out_path);
    int to_ppm = (l >= 4 && (strcasecmp(out_path + l - 4, ".ppm") == 0 || strcasecmp(out_path + l - 4, ".pgm") == 0));
    return to_ppm ? gmbwebp_decode_ppm(in_path, out_path) : gmbwebp_encode_ppm(in_path, out_path, quality);
}
