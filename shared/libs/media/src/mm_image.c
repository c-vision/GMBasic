/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_image.c — raster immagine + codec BMP / PNM (self-contained) + dispatcher per estensione. */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

void mm_image_free(mm_image* img) { if (img) { free(img->data); img->data = NULL; img->width = img->height = img->channels = 0; } }

static void* xalloc(size_t n, size_t s) { if (s && n > (size_t)-1 / s) return NULL; return malloc(n * s); }

/* ── BMP ── */
int mm_bmp_write(const char* path, const mm_image* img) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1) return -1;
    int ch = img->channels; if (ch == 1) ch = 3;                 /* grigio -> RGB */
    int bpp = (ch == 4) ? 32 : 24;
    int rowsz = ((img->width * bpp / 8) + 3) & ~3;
    unsigned datasize = (unsigned)rowsz * (unsigned)img->height;
    unsigned filesize = 14 + 40 + datasize;
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    unsigned char hdr[54]; memset(hdr, 0, 54);
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = filesize & 0xff; hdr[3] = (filesize >> 8) & 0xff; hdr[4] = (filesize >> 16) & 0xff; hdr[5] = (filesize >> 24) & 0xff;
    unsigned off = 54; hdr[10] = off;
    hdr[14] = 40;
    hdr[18] = img->width & 0xff; hdr[19] = (img->width >> 8) & 0xff; hdr[20] = (img->width >> 16) & 0xff; hdr[21] = (img->width >> 24) & 0xff;
    hdr[22] = img->height & 0xff; hdr[23] = (img->height >> 8) & 0xff; hdr[24] = (img->height >> 16) & 0xff; hdr[25] = (img->height >> 24) & 0xff;
    hdr[26] = 1; hdr[28] = (unsigned char)bpp;
    fwrite(hdr, 1, 54, f);
    unsigned char* row = (unsigned char*)xalloc((size_t)rowsz, 1);
    if (!row) { fclose(f); return -1; }
    for (int y = img->height - 1; y >= 0; y--) {               /* bottom-up */
        memset(row, 0, (size_t)rowsz);
        for (int x = 0; x < img->width; x++) {
            const unsigned char* px = &img->data[((size_t)y * img->width + x) * img->channels];
            unsigned char r = (img->channels == 1) ? px[0] : px[0];
            unsigned char g = (img->channels == 1) ? px[0] : px[1];
            unsigned char b = (img->channels == 1) ? px[0] : px[2];
            unsigned char a = (img->channels == 4) ? px[3] : 255;
            int o = x * (bpp / 8);
            row[o] = b; row[o + 1] = g; row[o + 2] = r; if (bpp == 32) row[o + 3] = a;
        }
        fwrite(row, 1, (size_t)rowsz, f);
    }
    free(row); fclose(f); return 0;
}
int mm_bmp_read(const char* path, mm_image* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    unsigned char hdr[54];
    if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[1] != 'M') { fclose(f); return -1; }
    unsigned dataoff = hdr[10] | (hdr[11] << 8) | (hdr[12] << 16) | ((unsigned)hdr[13] << 24);
    int w = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);
    int h = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);
    int bpp = hdr[28] | (hdr[29] << 8);
    int comp = hdr[30] | (hdr[31] << 8);
    if (w < 1 || h == 0 || comp != 0 || (bpp != 24 && bpp != 32)) { fclose(f); return -1; }
    int top_down = h < 0; if (h < 0) h = -h;
    int ch = (bpp == 32) ? 4 : 3;
    unsigned char* data = (unsigned char*)xalloc((size_t)w * h * ch, 1);
    if (!data) { fclose(f); return -1; }
    int rowsz = ((w * bpp / 8) + 3) & ~3;
    unsigned char* row = (unsigned char*)xalloc((size_t)rowsz, 1);
    if (!row) { free(data); fclose(f); return -1; }
    fseek(f, (long)dataoff, SEEK_SET);
    for (int r = 0; r < h; r++) {
        if (fread(row, 1, (size_t)rowsz, f) != (size_t)rowsz) { free(row); free(data); fclose(f); return -1; }
        int y = top_down ? r : (h - 1 - r);
        for (int x = 0; x < w; x++) {
            unsigned char b = row[x * (bpp / 8)], g = row[x * (bpp / 8) + 1], rr = row[x * (bpp / 8) + 2];
            unsigned char a = (bpp == 32) ? row[x * (bpp / 8) + 3] : 255;
            unsigned char* px = &data[((size_t)y * w + x) * ch];
            px[0] = rr; px[1] = g; px[2] = b; if (ch == 4) px[3] = a;
        }
    }
    free(row); fclose(f);
    out->width = w; out->height = h; out->channels = ch; out->data = data;
    return 0;
}

/* ── PNM (P5 grigio / P6 RGB, binari) ── */
static int pnm_token(FILE* f, char* buf, int cap) {   /* salta commenti e whitespace, legge un token */
    int c, i = 0;
    for (;;) { c = fgetc(f); if (c == '#') { while (c != '\n' && c != EOF) c = fgetc(f); } else if (c != EOF && !isspace(c)) break; if (c == EOF) return -1; }
    while (c != EOF && !isspace(c) && i < cap - 1) { buf[i++] = (char)c; c = fgetc(f); }
    buf[i] = 0; return i > 0 ? 0 : -1;
}
int mm_pnm_read(const char* path, mm_image* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    char t[32];
    if (pnm_token(f, t, 32) || t[0] != 'P' || (t[1] != '5' && t[1] != '6')) { fclose(f); return -1; }
    int ch = (t[1] == '6') ? 3 : 1;
    char s[32];
    if (pnm_token(f, s, 32)) { fclose(f); return -1; } int w = atoi(s);
    if (pnm_token(f, s, 32)) { fclose(f); return -1; } int h = atoi(s);
    if (pnm_token(f, s, 32)) { fclose(f); return -1; } int maxv = atoi(s);
    if (w < 1 || h < 1 || maxv != 255) { fclose(f); return -1; }
    size_t n = (size_t)w * h * ch;
    unsigned char* data = (unsigned char*)xalloc(n, 1);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, n, f) != n) { free(data); fclose(f); return -1; }
    fclose(f);
    out->width = w; out->height = h; out->channels = ch; out->data = data;
    return 0;
}
int mm_pnm_write(const char* path, const mm_image* img) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1) return -1;
    int ch = (img->channels == 1) ? 1 : 3;   /* alpha scartato */
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    fprintf(f, "%s\n%d %d\n255\n", ch == 1 ? "P5" : "P6", img->width, img->height);
    unsigned char* row = (unsigned char*)xalloc((size_t)img->width * ch, 1);
    if (!row) { fclose(f); return -1; }
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            const unsigned char* px = &img->data[((size_t)y * img->width + x) * img->channels];
            if (ch == 1) row[x] = px[0];
            else { row[x * 3] = px[0]; row[x * 3 + 1] = px[1]; row[x * 3 + 2] = px[2]; }
        }
        fwrite(row, 1, (size_t)img->width * ch, f);
    }
    free(row); fclose(f); return 0;
}

/* ── dispatcher ── */
static int has_ext(const char* p, const char* e) {
    size_t lp = strlen(p), le = strlen(e);
    if (lp < le) return 0;
    for (size_t i = 0; i < le; i++) if (tolower((unsigned char)p[lp - le + i]) != e[i]) return 0;
    return 1;
}
int mm_image_read(const char* path, mm_image* out) {
    if (has_ext(path, ".png")) return mm_png_read(path, out);
    if (has_ext(path, ".gif")) return mm_gif_read(path, out);
    if (has_ext(path, ".jpg") || has_ext(path, ".jpeg")) return mm_jpeg_read(path, out);
    if (has_ext(path, ".qoi")) return mm_qoi_read(path, out);
    if (has_ext(path, ".bmp")) return mm_bmp_read(path, out);
    if (has_ext(path, ".tga")) return mm_tga_read(path, out);
    if (has_ext(path, ".tif") || has_ext(path, ".tiff")) return mm_tiff_read(path, out);
    if (has_ext(path, ".webp")) return mm_webp_read(path, out);
    if (has_ext(path, ".ppm") || has_ext(path, ".pgm") || has_ext(path, ".pnm")) return mm_pnm_read(path, out);
    return -1;
}
int mm_image_write(const char* path, const mm_image* img) {
    if (has_ext(path, ".png")) return mm_png_write(path, img);
    if (has_ext(path, ".gif")) return mm_gif_write(path, img);
    if (has_ext(path, ".jpg") || has_ext(path, ".jpeg")) return mm_jpeg_write(path, img);
    if (has_ext(path, ".qoi")) return mm_qoi_write(path, img);
    if (has_ext(path, ".bmp")) return mm_bmp_write(path, img);
    if (has_ext(path, ".tga")) return mm_tga_write(path, img);
    if (has_ext(path, ".tif") || has_ext(path, ".tiff")) return mm_tiff_write(path, img);
    if (has_ext(path, ".webp")) return mm_webp_write(path, img, 90);
    if (has_ext(path, ".ppm") || has_ext(path, ".pgm") || has_ext(path, ".pnm")) return mm_pnm_write(path, img);
    return -1;
}
int mm_image_convert(const char* src, const char* dst) {
    mm_image im;
    if (mm_image_read(src, &im)) return -1;
    int rc = mm_image_write(dst, &im);
    mm_image_free(&im);
    return rc;
}
void mm_image_formats(char* out, int cap) { if (out && cap > 0) snprintf(out, (size_t)cap, "png,gif,jpg,jpeg,bmp,tga,tif,tiff,webp,ppm,pgm,pnm,qoi"); }

/* Compressione adattiva (algoritmo Luban): risoluzione target (short-side maxShort, strategie
   panorama/long/huge-pixel) + qualità adattiva (binary-search sul size target). quality>0 =
   qualità fissa. Fallback all'originale se il risultato è più grande (mai peggiorare). */
int mm_image_compress(const char* src, const char* dst, int max_short, int quality) {
    if (max_short <= 0) max_short = 1440;
    mm_image img;
    if (mm_image_read(src, &img) != 0) return -1;
    int W = img.width, H = img.height;
    if (W < 1 || H < 1) { mm_image_free(&img); return -1; }

    int ss = W < H ? W : H, ls = W < H ? H : W;
    double ratio = (double)ss / (double)ls;
    long pixels = (long)W * H;
    int ts = max_short, tl = (int)(ts / ratio);
    if (ls >= 10800 && ratio > 0.4) { tl = max_short; ts = (int)(tl * ratio); }
    if (pixels > 40960000L) { int tr = (int)(ss * 0.25); if (tr < ts) { ts = tr; tl = (int)(ts / ratio); } }
    if (ts > ss) { ts = ss; tl = ls; }
    long cur = (long)ts * tl;
    if (cur > 10240000L) { double sc = floor(sqrt((double)10240000L / (double)cur) * 1000.0) / 1000.0; ts = (int)(ts * sc); tl = (int)(tl * sc); }
    ts = ts / 2 * 2; tl = tl / 2 * 2; if (ts < 2) ts = 2; if (tl < 2) tl = 2;
    int fw = (W < H) ? ts : tl, fh = (W < H) ? tl : ts;

    mm_image res; int owned = 0;
    if (fw != W || fh != H) { if (mm_img_resize_bilinear(&img, fw, fh, &res) != 0) { mm_image_free(&img); return -1; } owned = 1; }
    else res = img;

    long fp = (long)res.width * res.height;
    double factor = fp < 500000L ? 0.0005 : fp < 1000000L ? 0.00015 : fp < 3000000L ? 0.00011 : 0.000025;
    int estKb = (int)(fp * factor); if (estKb < 20) estKb = 20;
    if (ratio < 0.2 && estKb < 400) estKb = estKb < 250 ? 250 : estKb;

    unsigned char* best = NULL; size_t bestLen = 0;
    if (quality > 0) {
        if (mm_jpeg_encode_mem(&res, quality, &best, &bestLen) != 0) { if (owned) mm_image_free(&res); mm_image_free(&img); return -1; }
    } else {
        int lo = 5, hi = 95;
        while (lo <= hi) {
            int mid = (lo + hi) / 2; unsigned char* buf = NULL; size_t ln = 0;
            if (mm_jpeg_encode_mem(&res, mid, &buf, &ln) != 0) break;
            if ((long)(ln / 1024) <= estKb) { free(best); best = buf; bestLen = ln; lo = mid + 1; }
            else { free(buf); hi = mid - 1; }
        }
        if (!best && mm_jpeg_encode_mem(&res, 5, &best, &bestLen) != 0) { if (owned) mm_image_free(&res); mm_image_free(&img); return -1; }
    }

    long origSize = 0;
    { FILE* fs = fopen(src, "rb"); if (fs) { fseek(fs, 0, SEEK_END); origSize = ftell(fs); fclose(fs); } }
    int rc = 0;
    if (origSize > 0 && (long)bestLen >= origSize) {          /* fallback: copia l'originale */
        FILE* fi = fopen(src, "rb"); FILE* fo = fopen(dst, "wb");
        rc = (fi && fo) ? 0 : -1;
        if (fi && fo) { char b[8192]; size_t n; while ((n = fread(b, 1, sizeof b, fi)) > 0) fwrite(b, 1, n, fo); }
        if (fi) fclose(fi); if (fo) fclose(fo);
    } else {
        FILE* fo = fopen(dst, "wb");
        if (fo) { if (fwrite(best, 1, bestLen, fo) != bestLen) rc = -1; fclose(fo); } else rc = -1;
    }
    free(best);
    if (owned) mm_image_free(&res);
    mm_image_free(&img);
    return rc;
}
