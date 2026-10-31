/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_imgops.c — operazioni immagine: resize (nn/bilinear), crop, rotate90, flip, gray, canali,
 * invert/brightness/contrast/threshold/lut, blur/sharpen/sobel, blend, fill, paste. */
#include "gmbmedia.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int alloc_img(mm_image* o, int w, int h, int ch) { if (w < 0 || h < 0) return -1; o->width = w; o->height = h; o->channels = ch; o->data = (unsigned char*)malloc((size_t)(w * h * ch) ? (size_t)w * h * ch : 1); return o->data ? 0 : -1; }
static int clampi(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

int mm_img_resize_nn(const mm_image* in, int w, int h, mm_image* out) {
    if (!in || !out || w < 1 || h < 1) return -1;
    if (alloc_img(out, w, h, in->channels)) return -1;
    for (int y = 0; y < h; y++) { int sy = (int)((long)y * in->height / h);
        for (int x = 0; x < w; x++) { int sx = (int)((long)x * in->width / w);
            memcpy(&out->data[((size_t)y * w + x) * in->channels], &in->data[((size_t)sy * in->width + sx) * in->channels], (size_t)in->channels); } }
    return 0;
}
int mm_img_resize_bilinear(const mm_image* in, int w, int h, mm_image* out) {
    if (!in || !out || w < 1 || h < 1) return -1;
    int ch = in->channels;
    if (alloc_img(out, w, h, ch)) return -1;
    for (int y = 0; y < h; y++) {
        double fy = (h > 1) ? (double)y * (in->height - 1) / (h - 1) : 0;
        int y0 = (int)fy; int y1 = y0 + 1 < in->height ? y0 + 1 : y0; double ty = fy - y0;
        for (int x = 0; x < w; x++) {
            double fx = (w > 1) ? (double)x * (in->width - 1) / (w - 1) : 0;
            int x0 = (int)fx; int x1 = x0 + 1 < in->width ? x0 + 1 : x0; double tx = fx - x0;
            for (int c = 0; c < ch; c++) {
                double p00 = in->data[((size_t)y0 * in->width + x0) * ch + c], p01 = in->data[((size_t)y0 * in->width + x1) * ch + c];
                double p10 = in->data[((size_t)y1 * in->width + x0) * ch + c], p11 = in->data[((size_t)y1 * in->width + x1) * ch + c];
                double v = p00 * (1 - tx) * (1 - ty) + p01 * tx * (1 - ty) + p10 * (1 - tx) * ty + p11 * tx * ty;
                out->data[((size_t)y * w + x) * ch + c] = (unsigned char)clampi((int)(v + 0.5));
            }
        }
    }
    return 0;
}
/* Resize Lanczos3 (separabile): filtro sinc finestrato, supporto allargato in downscale (anti-alias). */
static double lanczos3(double x) {
    if (x == 0.0) return 1.0;
    x = fabs(x);
    if (x >= 3.0) return 0.0;
    double px = M_PI * x;
    return 3.0 * sin(px) * sin(px / 3.0) / (px * px);
}
int mm_img_resize_lanczos(const mm_image* in, int nw, int nh, mm_image* out) {
    if (!in || !out || nw < 1 || nh < 1) return -1;
    int ch = in->channels, W = in->width, H = in->height;
    unsigned char* tmp = (unsigned char*)malloc((size_t)nw * H * ch);
    if (!tmp) return -1;
    for (int x = 0; x < nw; x++) {                       /* passata orizzontale */
        double center = (x + 0.5) * W / nw, scale = (double)W / nw, fs = scale < 1.0 ? 1.0 : scale;
        int lo = (int)floor(center - 3.0 * fs), hi = (int)ceil(center + 3.0 * fs);
        double sum = 0.0;
        for (int i = lo; i <= hi; i++) sum += lanczos3((center - (i + 0.5)) / fs);
        for (int y = 0; y < H; y++) for (int c = 0; c < ch; c++) {
            double acc = 0.0;
            for (int i = lo; i <= hi; i++) { int ii = i < 0 ? 0 : (i >= W ? W - 1 : i); acc += in->data[((size_t)y * W + ii) * ch + c] * lanczos3((center - (i + 0.5)) / fs); }
            tmp[((size_t)y * nw + x) * ch + c] = (unsigned char)clampi((int)(acc / sum + 0.5));
        }
    }
    if (alloc_img(out, nw, nh, ch)) { free(tmp); return -1; }
    for (int y = 0; y < nh; y++) {                       /* passata verticale */
        double center = (y + 0.5) * H / nh, scale = (double)H / nh, fs = scale < 1.0 ? 1.0 : scale;
        int lo = (int)floor(center - 3.0 * fs), hi = (int)ceil(center + 3.0 * fs);
        double sum = 0.0;
        for (int i = lo; i <= hi; i++) sum += lanczos3((center - (i + 0.5)) / fs);
        for (int x = 0; x < nw; x++) for (int c = 0; c < ch; c++) {
            double acc = 0.0;
            for (int i = lo; i <= hi; i++) { int ii = i < 0 ? 0 : (i >= H ? H - 1 : i); acc += tmp[((size_t)ii * nw + x) * ch + c] * lanczos3((center - (i + 0.5)) / fs); }
            out->data[((size_t)y * nw + x) * ch + c] = (unsigned char)clampi((int)(acc / sum + 0.5));
        }
    }
    free(tmp);
    return 0;
}
/* Rotazione ad angolo arbitrario (gradi) con campionamento bilineare; ridimensiona al bounding box. */
int mm_img_rotate_angle(const mm_image* in, double angle_deg, mm_image* out) {
    if (!in || !out) return -1;
    int W = in->width, H = in->height, ch = in->channels;
    double a = angle_deg * M_PI / 180.0, cs = cos(a), sn = sin(a);
    double cx = W / 2.0, cy = H / 2.0;
    double minx = 1e18, miny = 1e18, maxx = -1e18, maxy = -1e18;
    double pts[4][2] = { {0,0},{W,0},{W,H},{0,H} };
    for (int i = 0; i < 4; i++) {
        double x = pts[i][0] - cx, y = pts[i][1] - cy;
        double rx = x * cs - y * sn, ry = x * sn + y * cs;
        if (rx < minx) minx = rx; if (rx > maxx) maxx = rx;
        if (ry < miny) miny = ry; if (ry > maxy) maxy = ry;
    }
    int nw = (int)ceil(maxx - minx), nh = (int)ceil(maxy - miny);
    if (nw < 1) nw = 1; if (nh < 1) nh = 1;
    if (alloc_img(out, nw, nh, ch)) return -1;
    double ncx = nw / 2.0, ncy = nh / 2.0;
    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) {
        double dx = x + 0.5 - ncx, dy = y + 0.5 - ncy;
        double sx = dx * cs + dy * sn + cx, sy = -dx * sn + dy * cs + cy;   /* rotazione inversa */
        sx -= 0.5; sy -= 0.5;
        int x0 = (int)floor(sx), y0 = (int)floor(sy);
        double tx = sx - x0, ty = sy - y0;
        int x1 = x0 + 1 < W ? x0 + 1 : W - 1, y1 = y0 + 1 < H ? y0 + 1 : H - 1;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 < 0) x1 = 0; if (y1 < 0) y1 = 0;
        if (x0 > W - 1) x0 = W - 1; if (y0 > H - 1) y0 = H - 1; if (x1 > W - 1) x1 = W - 1; if (y1 > H - 1) y1 = H - 1;
        for (int c = 0; c < ch; c++) {
            double p00 = in->data[((size_t)y0 * W + x0) * ch + c], p01 = in->data[((size_t)y0 * W + x1) * ch + c];
            double p10 = in->data[((size_t)y1 * W + x0) * ch + c], p11 = in->data[((size_t)y1 * W + x1) * ch + c];
            double v = p00 * (1 - tx) * (1 - ty) + p01 * tx * (1 - ty) + p10 * (1 - tx) * ty + p11 * tx * ty;
            out->data[((size_t)y * nw + x) * ch + c] = (unsigned char)clampi((int)(v + 0.5));
        }
    }
    return 0;
}
int mm_img_crop(const mm_image* in, int x, int y, int w, int h, mm_image* out) {
    if (!in || !out || w < 1 || h < 1 || x < 0 || y < 0 || x + w > in->width || y + h > in->height) return -1;
    int ch = in->channels;
    if (alloc_img(out, w, h, ch)) return -1;
    for (int r = 0; r < h; r++) memcpy(&out->data[(size_t)r * w * ch], &in->data[((size_t)(y + r) * in->width + x) * ch], (size_t)w * ch);
    return 0;
}
int mm_img_rotate90(const mm_image* in, int turns, mm_image* out) {
    if (!in || !out || turns < 1) return -1;
    turns %= 4; if (turns == 0) turns = 1;
    int ch = in->channels, w = in->width, h = in->height;
    int ow = (turns % 2) ? h : w, oh = (turns % 2) ? w : h;
    if (alloc_img(out, ow, oh, ch)) return -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int nx, ny;
        if (turns == 1) { nx = h - 1 - y; ny = x; }            /* 90 CW */
        else if (turns == 2) { nx = w - 1 - x; ny = h - 1 - y; }/* 180 */
        else { nx = y; ny = w - 1 - x; }                        /* 270 CW = 90 CCW */
        memcpy(&out->data[((size_t)ny * ow + nx) * ch], &in->data[((size_t)y * w + x) * ch], (size_t)ch);
    }
    return 0;
}
int mm_img_flip_h(const mm_image* in, mm_image* out) {
    if (!in || !out) return -1; int ch = in->channels;
    if (alloc_img(out, in->width, in->height, ch)) return -1;
    for (int y = 0; y < in->height; y++) for (int x = 0; x < in->width; x++)
        memcpy(&out->data[((size_t)y * in->width + (in->width - 1 - x)) * ch], &in->data[((size_t)y * in->width + x) * ch], (size_t)ch);
    return 0;
}
int mm_img_flip_v(const mm_image* in, mm_image* out) {
    if (!in || !out) return -1; int ch = in->channels;
    if (alloc_img(out, in->width, in->height, ch)) return -1;
    for (int y = 0; y < in->height; y++) memcpy(&out->data[(size_t)y * in->width * ch], &in->data[(size_t)(in->height - 1 - y) * in->width * ch], (size_t)in->width * ch);
    return 0;
}
int mm_img_to_gray(const mm_image* in, mm_image* out) {
    if (!in || !out) return -1; int ch = in->channels;
    if (alloc_img(out, in->width, in->height, 1)) return -1;
    for (int i = 0; i < in->width * in->height; i++) {
        int r = in->data[(size_t)i * ch], g = (ch == 1) ? r : in->data[(size_t)i * ch + 1], b = (ch == 1) ? r : in->data[(size_t)i * ch + 2];
        out->data[i] = (unsigned char)((299 * r + 587 * g + 114 * b) / 1000);
    }
    return 0;
}
int mm_img_to_channels(const mm_image* in, int channels, mm_image* out) {
    if (!in || !out || (channels != 1 && channels != 3 && channels != 4)) return -1;
    if (channels == 1) return mm_img_to_gray(in, out);
    int ch = in->channels;
    if (alloc_img(out, in->width, in->height, channels)) return -1;
    for (int i = 0; i < in->width * in->height; i++) {
        int r, g, b;
        if (ch == 1) { r = g = b = in->data[i]; } else { r = in->data[(size_t)i * ch]; g = in->data[(size_t)i * ch + 1]; b = in->data[(size_t)i * ch + 2]; }
        out->data[(size_t)i * channels] = (unsigned char)r; out->data[(size_t)i * channels + 1] = (unsigned char)g; out->data[(size_t)i * channels + 2] = (unsigned char)b;
        if (channels == 4) out->data[(size_t)i * channels + 3] = (ch == 4) ? in->data[(size_t)i * ch + 3] : 255;
    }
    return 0;
}
int mm_img_invert(mm_image* img) {
    if (!img) return -1; int ch = img->channels; int cc = (ch == 4) ? 3 : ch;
    for (int i = 0; i < img->width * img->height; i++) for (int c = 0; c < cc; c++) img->data[(size_t)i * ch + c] = (unsigned char)(255 - img->data[(size_t)i * ch + c]);
    return 0;
}
int mm_img_brightness(mm_image* img, int delta) {
    if (!img) return -1; int ch = img->channels; int cc = (ch == 4) ? 3 : ch;
    for (int i = 0; i < img->width * img->height; i++) for (int c = 0; c < cc; c++) img->data[(size_t)i * ch + c] = (unsigned char)clampi(img->data[(size_t)i * ch + c] + delta);
    return 0;
}
int mm_img_contrast(mm_image* img, double factor) {
    if (!img) return -1; int ch = img->channels; int cc = (ch == 4) ? 3 : ch;
    for (int i = 0; i < img->width * img->height; i++) for (int c = 0; c < cc; c++) img->data[(size_t)i * ch + c] = (unsigned char)clampi((int)((img->data[(size_t)i * ch + c] - 128) * factor + 128 + 0.5));
    return 0;
}
int mm_img_apply_lut(mm_image* img, const unsigned char lut[256]) {
    if (!img || !lut) return -1; int ch = img->channels; int cc = (ch == 4) ? 3 : ch;
    for (int i = 0; i < img->width * img->height; i++) for (int c = 0; c < cc; c++) img->data[(size_t)i * ch + c] = lut[img->data[(size_t)i * ch + c]];
    return 0;
}
int mm_img_threshold(mm_image* img, int t) {
    if (!img) return -1; int ch = img->channels;
    for (int i = 0; i < img->width * img->height; i++) {
        int r = img->data[(size_t)i * ch], g = (ch == 1) ? r : img->data[(size_t)i * ch + 1], b = (ch == 1) ? r : img->data[(size_t)i * ch + 2];
        int gray = (299 * r + 587 * g + 114 * b) / 1000;
        unsigned char v = gray >= t ? 255 : 0;
        if (ch == 1) img->data[i] = v;
        else { img->data[(size_t)i * ch] = v; img->data[(size_t)i * ch + 1] = v; img->data[(size_t)i * ch + 2] = v; }
    }
    return 0;
}
int mm_img_fill(mm_image* img, int r, int g, int b, int a) {
    if (!img) return -1; int ch = img->channels;
    for (int i = 0; i < img->width * img->height; i++) {
        img->data[(size_t)i * ch] = (unsigned char)clampi(r);
        if (ch >= 3) { img->data[(size_t)i * ch + 1] = (unsigned char)clampi(g); img->data[(size_t)i * ch + 2] = (unsigned char)clampi(b); }
        if (ch == 4) img->data[(size_t)i * ch + 3] = (unsigned char)clampi(a);
    }
    return 0;
}
int mm_img_paste(mm_image* dst, const mm_image* src, int ox, int oy) {
    if (!dst || !src) return -1;
    for (int y = 0; y < src->height; y++) for (int x = 0; x < src->width; x++) {
        int dx = ox + x, dy = oy + y; if (dx < 0 || dy < 0 || dx >= dst->width || dy >= dst->height) continue;
        const unsigned char* s = &src->data[((size_t)y * src->width + x) * src->channels];
        unsigned char* d = &dst->data[((size_t)dy * dst->width + dx) * dst->channels];
        int n = src->channels < dst->channels ? src->channels : dst->channels;
        for (int c = 0; c < n; c++) d[c] = s[c];
    }
    return 0;
}
int mm_img_blend(const mm_image* base, const mm_image* over, double alpha, mm_image* out) {
    if (!base || !over || !out || base->width != over->width || base->height != over->height) return -1;
    int ch = base->channels; if (over->channels != ch) return -1;
    if (alloc_img(out, base->width, base->height, ch)) return -1;
    for (int i = 0; i < base->width * base->height * ch; i++) out->data[i] = (unsigned char)clampi((int)(base->data[i] * (1 - alpha) + over->data[i] * alpha + 0.5));
    return 0;
}
int mm_img_blur_box(const mm_image* in, int radius, mm_image* out) {
    if (!in || !out || radius < 1) return -1;
    int ch = in->channels, w = in->width, h = in->height;
    if (alloc_img(out, w, h, ch)) return -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        for (int c = 0; c < ch; c++) {
            int s = 0, n = 0;
            for (int dy = -radius; dy <= radius; dy++) for (int dx = -radius; dx <= radius; dx++) {
                int yy = y + dy, xx = x + dx; if (yy < 0 || xx < 0 || yy >= h || xx >= w) continue;
                s += in->data[((size_t)yy * w + xx) * ch + c]; n++;
            }
            out->data[((size_t)y * w + x) * ch + c] = (unsigned char)(s / n);
        }
    }
    return 0;
}
int mm_img_blur_gaussian(const mm_image* in, int radius, mm_image* out) {
    if (!in || !out || radius < 1) return -1;
    int ch = in->channels, w = in->width, h = in->height;
    if (alloc_img(out, w, h, ch)) return -1;
    int k = 2 * radius + 1;
    double sigma = (double)radius / 3.0; if (sigma < 0.5) sigma = 0.5;
    double* ker = (double*)malloc((size_t)k * sizeof(double));
    unsigned char* tmp = (unsigned char*)malloc((size_t)w * h * ch ? (size_t)w * h * ch : 1);
    if (!ker || !tmp) { free(ker); free(tmp); mm_image_free(out); return -1; }
    double sum = 0;
    for (int i = 0; i < k; i++) { int d = i - radius; ker[i] = exp(-(double)d * d / (2 * sigma * sigma)); sum += ker[i]; }
    for (int i = 0; i < k; i++) ker[i] /= sum;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) for (int c = 0; c < ch; c++) {   /* passata orizzontale */
        double acc = 0;
        for (int i = 0; i < k; i++) { int xx = x + i - radius; if (xx < 0) xx = 0; else if (xx >= w) xx = w - 1; acc += ker[i] * in->data[((size_t)y * w + xx) * ch + c]; }
        tmp[((size_t)y * w + x) * ch + c] = (unsigned char)clampi((int)(acc + 0.5));
    }
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) for (int c = 0; c < ch; c++) {   /* passata verticale */
        double acc = 0;
        for (int i = 0; i < k; i++) { int yy = y + i - radius; if (yy < 0) yy = 0; else if (yy >= h) yy = h - 1; acc += ker[i] * tmp[((size_t)yy * w + x) * ch + c]; }
        out->data[((size_t)y * w + x) * ch + c] = (unsigned char)clampi((int)(acc + 0.5));
    }
    free(tmp); free(ker);
    return 0;
}
int mm_img_morph(const mm_image* in, int radius, int mode, mm_image* out) {
    if (!in || !out || radius < 1) return -1;
    int ch = in->channels, w = in->width, h = in->height;
    if (alloc_img(out, w, h, ch)) return -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) for (int c = 0; c < ch; c++) {
        int best = in->data[((size_t)y * w + x) * ch + c];
        for (int dy = -radius; dy <= radius; dy++) for (int dx = -radius; dx <= radius; dx++) {
            int yy = y + dy, xx = x + dx; if (yy < 0 || xx < 0 || yy >= h || xx >= w) continue;
            int v = in->data[((size_t)yy * w + xx) * ch + c];
            if (mode == 1) { if (v > best) best = v; }   /* dilate */
            else { if (v < best) best = v; }             /* erode */
        }
        out->data[((size_t)y * w + x) * ch + c] = (unsigned char)best;
    }
    return 0;
}
int mm_img_dither(const mm_image* in, int levels, mm_image* out) {
    if (!in || !out || levels < 2) return -1;
    int ch = in->channels, w = in->width, h = in->height;
    if (alloc_img(out, w, h, ch)) return -1;
    int cc = (ch == 4) ? 3 : ch;                            /* dither sui canali colore, alpha copiato */
    double* buf = (double*)malloc((size_t)w * h * ch * sizeof(double));
    if (!buf) { mm_image_free(out); return -1; }
    for (int i = 0; i < w * h * ch; i++) buf[i] = in->data[i];
    double step = 255.0 / (levels - 1);
    long row = (long)w * ch;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        long idx = ((long)y * w + x) * ch;
        for (int c = 0; c < cc; c++) {
            double v = buf[idx + c];
            double q = ((int)(v / step + 0.5)) * step; if (q < 0) q = 0; if (q > 255) q = 255;
            double e = v - q;
            out->data[idx + c] = (unsigned char)(int)q;
            if (x + 1 < w) buf[idx + c + ch] += e * 7.0 / 16.0;
            if (y + 1 < h) {
                if (x > 0) buf[idx + c + row - ch] += e * 3.0 / 16.0;
                buf[idx + c + row] += e * 5.0 / 16.0;
                if (x + 1 < w) buf[idx + c + row + ch] += e * 1.0 / 16.0;
            }
        }
        if (ch == 4) out->data[idx + 3] = in->data[idx + 3];
    }
    free(buf);
    return 0;
}
int mm_img_warp_perspective(const mm_image* in, const double m[9], int ow, int oh, mm_image* out) {
    if (!in || !m || !out || ow < 1 || oh < 1) return -1;
    int ch = in->channels;
    if (alloc_img(out, ow, oh, ch)) return -1;
    for (int y = 0; y < oh; y++) for (int x = 0; x < ow; x++) {
        double den = m[6] * x + m[7] * y + m[8];
        double sx = den != 0.0 ? (m[0] * x + m[1] * y + m[2]) / den : -1e9;
        double sy = den != 0.0 ? (m[3] * x + m[4] * y + m[5]) / den : -1e9;
        unsigned char* o = &out->data[((size_t)y * ow + x) * ch];
        if (sx < 0 || sy < 0 || sx > in->width - 1 || sy > in->height - 1) { memset(o, 0, (size_t)ch); continue; }
        int x0 = (int)sx, y0 = (int)sy;
        int x1 = (x0 + 1 < in->width) ? x0 + 1 : x0, y1 = (y0 + 1 < in->height) ? y0 + 1 : y0;
        double tx = sx - x0, ty = sy - y0;
        for (int c = 0; c < ch; c++) {
            double p00 = in->data[((size_t)y0 * in->width + x0) * ch + c];
            double p01 = in->data[((size_t)y0 * in->width + x1) * ch + c];
            double p10 = in->data[((size_t)y1 * in->width + x0) * ch + c];
            double p11 = in->data[((size_t)y1 * in->width + x1) * ch + c];
            double v = p00 * (1 - tx) * (1 - ty) + p01 * tx * (1 - ty) + p10 * (1 - tx) * ty + p11 * tx * ty;
            o[c] = (unsigned char)clampi((int)(v + 0.5));
        }
    }
    return 0;
}
int mm_img_sharpen(const mm_image* in, mm_image* out) {
    if (!in || !out) return -1;
    int ch = in->channels, w = in->width, h = in->height;
    mm_image blur; if (mm_img_blur_box(in, 1, &blur)) return -1;
    if (alloc_img(out, w, h, ch)) { mm_image_free(&blur); return -1; }
    const int K[9] = { 0, -1, 0, -1, 5, -1, 0, -1, 0 };
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) for (int c = 0; c < ch; c++) {
        int s = 0, ki = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++, ki++) {
            int yy = y + dy, xx = x + dx; if (yy < 0 || xx < 0 || yy >= h || xx >= w) continue;
            s += K[ki] * in->data[((size_t)yy * w + xx) * ch + c];
        }
        out->data[((size_t)y * w + x) * ch + c] = (unsigned char)clampi(s);
    }
    mm_image_free(&blur);
    return 0;
}
int mm_img_sobel(const mm_image* in, mm_image* out) {
    if (!in || !out) return -1;
    int ch = in->channels, w = in->width, h = in->height;
    if (alloc_img(out, w, h, 1)) return -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int gx = 0, gy = 0;
        const int Gx[9] = { -1, 0, 1, -2, 0, 2, -1, 0, 1 }, Gy[9] = { -1, -2, -1, 0, 0, 0, 1, 2, 1 };
        int ki = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++, ki++) {
            int yy = y + dy, xx = x + dx; if (yy < 0 || xx < 0 || yy >= h || xx >= w) continue;
            unsigned char* p = &in->data[((size_t)yy * w + xx) * ch];
            int r = p[0], g = (ch == 1) ? r : p[1], b = (ch == 1) ? r : p[2];
            int lum = (299 * r + 587 * g + 114 * b) / 1000;
            gx += Gx[ki] * lum; gy += Gy[ki] * lum;
        }
        int mag = (int)sqrt((double)gx * gx + (double)gy * gy);
        out->data[(size_t)y * w + x] = (unsigned char)clampi(mag);
    }
    return 0;
}
