/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_qoi.c — codec QOI (Quite OK Image): lossless, self-contained (decode + encode). */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned rd32be(const unsigned char* p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }
static void wr32be(unsigned char* p, unsigned v) { p[0] = (v >> 24) & 0xff; p[1] = (v >> 16) & 0xff; p[2] = (v >> 8) & 0xff; p[3] = v & 0xff; }
static int qoi_hash(int r, int g, int b, int a) { return (r * 3 + g * 5 + b * 7 + a * 11) % 64; }

int mm_qoi_read(const char* path, mm_image* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 14) { fclose(f); return -1; }
    unsigned char* D = (unsigned char*)malloc((size_t)sz);
    if (!D || fread(D, 1, (size_t)sz, f) != (size_t)sz) { free(D); fclose(f); return -1; }
    fclose(f);
    if (memcmp(D, "qoif", 4)) { free(D); return -1; }
    int W = (int)rd32be(D + 4), H = (int)rd32be(D + 8), ch = D[12];
    if (W < 1 || H < 1 || (ch != 3 && ch != 4)) { free(D); return -1; }
    out->width = W; out->height = H; out->channels = ch;
    out->data = (unsigned char*)malloc((size_t)W * H * ch);
    unsigned char idx[64][4]; memset(idx, 0, sizeof idx);
    unsigned char px[4] = { 0, 0, 0, 255 };
    size_t p = 14; long n = (long)W * H, i = 0;
    while (i < n && p < (size_t)sz) {
        int run = 1; int b1 = D[p++];
        if (b1 == 0xFE) { px[0] = D[p]; px[1] = D[p + 1]; px[2] = D[p + 2]; p += 3; }
        else if (b1 == 0xFF) { px[0] = D[p]; px[1] = D[p + 1]; px[2] = D[p + 2]; px[3] = D[p + 3]; p += 4; }
        else if ((b1 & 0xC0) == 0x00) { int x = b1 & 0x3F; px[0] = idx[x][0]; px[1] = idx[x][1]; px[2] = idx[x][2]; px[3] = idx[x][3]; }
        else if ((b1 & 0xC0) == 0x40) { px[0] += ((b1 >> 4) & 3) - 2; px[1] += ((b1 >> 2) & 3) - 2; px[2] += (b1 & 3) - 2; }
        else if ((b1 & 0xC0) == 0x80) { int b2 = D[p++]; int dg = (b1 & 0x3F) - 32; px[0] += dg + ((b2 >> 4) & 15) - 8; px[1] += dg; px[2] += dg + (b2 & 15) - 8; }
        else { run = (b1 & 0x3F) + 1; }
        idx[qoi_hash(px[0], px[1], px[2], px[3])][0] = px[0]; idx[qoi_hash(px[0], px[1], px[2], px[3])][1] = px[1]; idx[qoi_hash(px[0], px[1], px[2], px[3])][2] = px[2]; idx[qoi_hash(px[0], px[1], px[2], px[3])][3] = px[3];
        for (int k = 0; k < run && i < n; k++, i++) { memcpy(&out->data[i * ch], px, (size_t)ch); }
    }
    free(D);
    return 0;
}

int mm_qoi_write(const char* path, const mm_image* img) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1) return -1;
    int W = img->width, H = img->height, ch = img->channels;
    if (ch != 3 && ch != 4) return -1;
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    unsigned char hd[14]; memcpy(hd, "qoif", 4); wr32be(hd + 4, (unsigned)W); wr32be(hd + 8, (unsigned)H); hd[12] = (unsigned char)ch; hd[13] = 0;
    fwrite(hd, 1, 14, f);
    unsigned char idx[64][4]; memset(idx, 0, sizeof idx);
    unsigned char px[4] = { 0, 0, 0, 255 };
    unsigned char out[5]; int run = 0;
    for (long i = 0; i < (long)W * H; i++) {
        unsigned char c[4] = { 0, 0, 0, 255 };
        memcpy(c, &img->data[i * ch], (size_t)ch);
        int eq = (c[0] == px[0] && c[1] == px[1] && c[2] == px[2] && (!ch || c[3] == px[3]));
        if (eq) { run++; if (run == 62 || i == (long)W * H - 1) { out[0] = 0xC0 | (run - 1); fwrite(out, 1, 1, f); run = 0; } memcpy(px, c, 4); continue; }
        if (run > 0) { out[0] = 0xC0 | (run - 1); fwrite(out, 1, 1, f); run = 0; }
        int hi = qoi_hash(c[0], c[1], c[2], c[3]);
        if (idx[hi][0] == c[0] && idx[hi][1] == c[1] && idx[hi][2] == c[2] && idx[hi][3] == c[3]) { out[0] = 0x00 | hi; fwrite(out, 1, 1, f); }
        else if (ch == 4 && c[3] != px[3]) { out[0] = 0xFF; memcpy(out + 1, c, 4); fwrite(out, 1, 5, f); }
        else {
            int dr = c[0] - px[0], dg = c[1] - px[1], db = c[2] - px[2];
            if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) { out[0] = 0x40 | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2); fwrite(out, 1, 1, f); }
            else { int drdg = dr - dg, dbdg = db - dg; if (drdg >= -8 && drdg <= 7 && dg >= -32 && dg <= 31 && dbdg >= -8 && dbdg <= 7) { out[0] = 0x80 | (dg + 32); out[1] = ((drdg + 8) << 4) | (dbdg + 8); fwrite(out, 1, 2, f); } else { out[0] = 0xFE; out[1] = c[0]; out[2] = c[1]; out[3] = c[2]; fwrite(out, 1, 4, f); } }
        }
        memcpy(idx[hi], c, 4); memcpy(px, c, 4);
    }
    unsigned char end[8] = { 0, 0, 0, 0, 0, 0, 0, 1 }; fwrite(end, 1, 8, f);
    fclose(f);
    return 0;
}
