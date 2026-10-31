/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_tga.c — codec TGA: truecolor 24/32 e grayscale 8, non compresso o RLE. */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mm_tga_read(const char* path, mm_image* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    unsigned char hd[18];
    if (fread(hd, 1, 18, f) != 18) { fclose(f); return -1; }
    int idlen = hd[0], cmap = hd[1], type = hd[2], bpp = hd[16], desc = hd[17];
    int w = hd[12] | (hd[13] << 8), h = hd[14] | (hd[15] << 8);
    if (cmap != 0 || w < 1 || h < 1) { fclose(f); return -1; }
    int gray = (type == 3 || type == 11);
    int rle = (type == 10 || type == 11);
    if (!(type == 2 || type == 3 || type == 10 || type == 11)) { fclose(f); return -1; }
    if (gray && bpp != 8) { fclose(f); return -1; }
    if (!gray && bpp != 24 && bpp != 32) { fclose(f); return -1; }
    if (idlen) fseek(f, idlen, SEEK_CUR);
    int ch = gray ? 1 : (bpp == 32 ? 4 : 3);
    int bpc = bpp / 8;
    unsigned char* data = (unsigned char*)malloc((size_t)w * h * ch);
    if (!data) { fclose(f); return -1; }
    int topdown = (desc & 0x20) != 0;
    long total = (long)w * h, got = 0;
    unsigned char px[4];
    while (got < total) {
        int run = 1, raw = 0;
        if (rle) {
            int hbyte = fgetc(f); if (hbyte < 0) { free(data); fclose(f); return -1; }
            run = (hbyte & 0x7f) + 1; raw = (hbyte & 0x80) != 0;
        }
        for (int k = 0; k < run && got < total; k++) {
            if (!raw || k == 0 || !rle) { if (fread(px, 1, (size_t)bpc, f) != (size_t)bpc) { free(data); fclose(f); return -1; } }
            long idx = got;
            int y = (int)(idx / w), x = (int)(idx % w);
            int ry = topdown ? y : (h - 1 - y);
            unsigned char* o = &data[((size_t)ry * w + x) * ch];
            if (gray) o[0] = px[0];
            else { o[0] = px[2]; o[1] = px[1]; o[2] = px[0]; if (ch == 4) o[3] = px[3]; }
            got++;
        }
    }
    fclose(f);
    out->width = w; out->height = h; out->channels = ch; out->data = data;
    return 0;
}

int mm_tga_write(const char* path, const mm_image* img) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1) return -1;
    int ch = img->channels;
    int gray = (ch == 1);
    int bpp = gray ? 8 : (ch == 4 ? 32 : 24);
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    unsigned char hd[18]; memset(hd, 0, 18);
    hd[2] = gray ? 3 : 2;                 /* uncompressed truecolor/gray */
    hd[12] = img->width & 0xff; hd[13] = (img->width >> 8) & 0xff;
    hd[14] = img->height & 0xff; hd[15] = (img->height >> 8) & 0xff;
    hd[16] = (unsigned char)bpp;
    hd[17] = 0x20 | (ch == 4 ? 8 : 0);    /* top-down; 8 alpha bits */
    fwrite(hd, 1, 18, f);
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            const unsigned char* p = &img->data[((size_t)y * img->width + x) * ch];
            unsigned char bgr[4];
            if (gray) bgr[0] = p[0];
            else { bgr[0] = p[2]; bgr[1] = p[1]; bgr[2] = p[0]; if (ch == 4) bgr[3] = p[3]; }
            fwrite(bgr, 1, (size_t)(bpp / 8), f);
        }
    }
    fclose(f);
    return 0;
}
