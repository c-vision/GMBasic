/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gmbmedia_basic.c — shim FFI del namespace BASIC `MEDIA`.
 * Avvolge l'API di gmbmedia (mm_image/mm_audio) con una superficie BASIC-friendly:
 * handle LONG, scalari/stringhe, nessuno struct/puntatore esposto. Direzione unica
 * C#→C / BASIC→C: la logica sta in gmbmedia, qui solo marshalling e registry degli handle.
 */
#include "gmbmedia.h"
#include "gmbmedia_basic.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#define MB_IMG_MAX 64
#define MB_AUD_MAX 32
#define MB_TMP 512

typedef struct { int used; mm_image img; } mb_img_slot;
typedef struct { int used; mm_audio aud; } mb_aud_slot;

static mb_img_slot g_img[MB_IMG_MAX];
static mb_aud_slot g_aud[MB_AUD_MAX];
static char g_tmp[MB_TMP];

int gmbmedia_available(void) { return 1; }

static mb_img_slot* islot(long h) { return (h >= 1 && h <= MB_IMG_MAX && g_img[h - 1].used) ? &g_img[h - 1] : NULL; }
static mb_aud_slot* aslot(long h) { return (h >= 1 && h <= MB_AUD_MAX && g_aud[h - 1].used) ? &g_aud[h - 1] : NULL; }

static long inew(mm_image* src)
{
    for (int i = 0; i < MB_IMG_MAX; i++)
        if (!g_img[i].used) { g_img[i].used = 1; g_img[i].img = *src; return i + 1; }
    mm_image_free(src);
    return -1;
}
static long anew(mm_audio* src)
{
    for (int i = 0; i < MB_AUD_MAX; i++)
        if (!g_aud[i].used) { g_aud[i].used = 1; g_aud[i].aud = *src; return i + 1; }
    mm_audio_free(src);
    return -1;
}

/* ── immagini ── */
long gmbmedia_img_read(const char* path)
{
    mm_image m; memset(&m, 0, sizeof m);
    if (mm_image_read(path ? path : "", &m) != 0) return -1;
    return inew(&m);
}

/* Lettura con shrink-on-load (DCT scaling) per JPEG: 1,2,4,8 (1/2..1/8 dimensione, più veloce). */
long gmbmedia_img_read_scaled(const char* path, long shrink)
{
    const char* p = path ? path : "";
    const char* dot = strrchr(p, '.');
    mm_image m; memset(&m, 0, sizeof m);
    int rc = (dot && (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")))
                 ? mm_jpeg_read_scaled(p, &m, (int)shrink)
                 : mm_image_read(p, &m);   /* non-JPEG: nessuno scaling */
    return rc == 0 ? inew(&m) : -1;
}

long gmbmedia_img_w(long h) { mb_img_slot* s = islot(h); return s ? s->img.width : 0; }
long gmbmedia_img_h(long h) { mb_img_slot* s = islot(h); return s ? s->img.height : 0; }
long gmbmedia_img_channels(long h) { mb_img_slot* s = islot(h); return s ? s->img.channels : 0; }

/* pixel: 0xRRGGBB; per 4 canali include l'alpha in 0xAARRGGBB via get_a. */
long gmbmedia_img_get(long h, long x, long y)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image* m = &s->img;
    if (x < 0 || y < 0 || x >= m->width || y >= m->height) return -1;
    unsigned char* p = m->data + ((size_t)y * m->width + x) * m->channels;
    if (m->channels == 1) { int v = p[0]; return (v << 16) | (v << 8) | v; }
    if (m->channels == 3) return ((int)p[0] << 16) | ((int)p[1] << 8) | p[2];
    return ((int)p[0] << 16) | ((int)p[1] << 8) | p[2]; /* RGBA -> RGB */
}
long gmbmedia_img_get_a(long h, long x, long y)
{
    mb_img_slot* s = islot(h); if (!s) return 255;
    mm_image* m = &s->img;
    if (m->channels != 4 || x < 0 || y < 0 || x >= m->width || y >= m->height) return 255;
    return m->data[((size_t)y * m->width + x) * 4 + 3];
}
long gmbmedia_img_set(long h, long x, long y, long rgb, long a)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image* m = &s->img;
    if (x < 0 || y < 0 || x >= m->width || y >= m->height) return -1;
    unsigned char* p = m->data + ((size_t)y * m->width + x) * m->channels;
    unsigned char r = (unsigned char)((rgb >> 16) & 0xFF), g = (unsigned char)((rgb >> 8) & 0xFF), b = (unsigned char)(rgb & 0xFF);
    if (m->channels == 1) p[0] = (unsigned char)((r * 299 + g * 587 + b * 114) / 1000);
    else if (m->channels == 3) { p[0] = r; p[1] = g; p[2] = b; }
    else { p[0] = r; p[1] = g; p[2] = b; p[3] = (unsigned char)(a & 0xFF); }
    return 0;
}

long gmbmedia_img_write(long h, const char* path)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    return mm_image_write(path ? path : "", &s->img);
}
long gmbmedia_img_write_q(long h, const char* path, long quality)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    const char* p = path ? path : "";
    const char* dot = strrchr(p, '.');
    if (dot && (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg"))) return mm_jpeg_write_q(p, &s->img, (int)quality);
    if (dot && !strcasecmp(dot, ".webp")) return mm_webp_write(p, &s->img, (int)quality);
    if (dot && !strcasecmp(dot, ".gif")) return mm_gif_write_mc(p, &s->img, (int)quality);
    return mm_image_write(p, &s->img);
}
long gmbmedia_img_convert(const char* src, const char* dst)
{
    return mm_image_convert(src ? src : "", dst ? dst : "");
}

/* Compressione adattiva (Luban-style): max_short (0=1440), quality (0=auto binary-search). */
long gmbmedia_img_compress(const char* src, const char* dst, long max_short, long quality)
{
    return mm_image_compress(src ? src : "", dst ? dst : "", (int)max_short, (int)quality) == 0 ? 0 : -1;
}
const char* gmbmedia_img_formats(void)
{
    g_tmp[0] = 0;
    mm_image_formats(g_tmp, MB_TMP);
    return g_tmp;
}
long gmbmedia_img_free(long h)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image_free(&s->img);
    memset(s, 0, sizeof *s);
    return 0;
}

/* operazioni (ritornano un NUOVO handle) */
long gmbmedia_img_resize(long h, long w, long hh, long mode)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    int rc = (mode == 2) ? mm_img_resize_lanczos(&s->img, (int)w, (int)hh, &out)
                         : (mode == 1) ? mm_img_resize_bilinear(&s->img, (int)w, (int)hh, &out)
                         : mm_img_resize_nn(&s->img, (int)w, (int)hh, &out);
    return rc == 0 ? inew(&out) : -1;
}
long gmbmedia_img_crop(long h, long x, long y, long w, long hh)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_crop(&s->img, (int)x, (int)y, (int)w, (int)hh, &out) == 0 ? inew(&out) : -1;
}
long gmbmedia_img_rotate(long h, long turns)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_rotate90(&s->img, (int)turns, &out) == 0 ? inew(&out) : -1;
}

/* Rotazione ad angolo arbitrario (centesimi di grado, bilineare). */
long gmbmedia_img_rotate_angle(long h, long angle_cdeg)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_rotate_angle(&s->img, (double)angle_cdeg / 100.0, &out) == 0 ? inew(&out) : -1;
}
long gmbmedia_img_flip(long h, long axis)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    int rc = axis ? mm_img_flip_v(&s->img, &out) : mm_img_flip_h(&s->img, &out);
    return rc == 0 ? inew(&out) : -1;
}
long gmbmedia_img_gray(long h)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_to_gray(&s->img, &out) == 0 ? inew(&out) : -1;
}
long gmbmedia_img_to_channels(long h, long ch)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_to_channels(&s->img, (int)ch, &out) == 0 ? inew(&out) : -1;
}
long gmbmedia_img_blur(long h, long radius, long kind)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    int rc = kind ? mm_img_blur_gaussian(&s->img, (int)radius, &out) : mm_img_blur_box(&s->img, (int)radius, &out);
    return rc == 0 ? inew(&out) : -1;
}
long gmbmedia_img_morph(long h, long radius, long mode)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_morph(&s->img, (int)radius, (int)mode, &out) == 0 ? inew(&out) : -1;
}
long gmbmedia_img_dither(long h, long levels)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_dither(&s->img, (int)levels, &out) == 0 ? inew(&out) : -1;
}
long gmbmedia_img_sharpen(long h)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_sharpen(&s->img, &out) == 0 ? inew(&out) : -1;
}
long gmbmedia_img_sobel(long h)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_sobel(&s->img, &out) == 0 ? inew(&out) : -1;
}
long gmbmedia_img_blend(long base, long over, long alpha_pct)
{
    mb_img_slot* a = islot(base), *b = islot(over); if (!a || !b) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    double al = (double)alpha_pct / 100.0;
    return mm_img_blend(&a->img, &b->img, al, &out) == 0 ? inew(&out) : -1;
}
/* warp prospettico: m = 9 coefficienti passati come CSV "a,b,...,i" (homografia out->in) */
long gmbmedia_img_warp(long h, const char* m_csv, long ow, long oh)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    double m[9]; int k = 0; const char* p = m_csv ? m_csv : "";
    while (k < 9 && *p) { char* e; m[k++] = strtod(p, &e); if (e == p) break; p = e; while (*p == ',' || *p == ' ') p++; }
    if (k < 9) return -1;
    mm_image out; memset(&out, 0, sizeof out);
    return mm_img_warp_perspective(&s->img, m, (int)ow, (int)oh, &out) == 0 ? inew(&out) : -1;
}

/* operazioni in-place */
long gmbmedia_img_invert(long h) { mb_img_slot* s = islot(h); return s ? mm_img_invert(&s->img) : -1; }
long gmbmedia_img_brightness(long h, long delta) { mb_img_slot* s = islot(h); return s ? mm_img_brightness(&s->img, (int)delta) : -1; }
long gmbmedia_img_contrast(long h, long factor_pct) { mb_img_slot* s = islot(h); return s ? mm_img_contrast(&s->img, (double)factor_pct / 100.0) : -1; }
long gmbmedia_img_threshold(long h, long t) { mb_img_slot* s = islot(h); return s ? mm_img_threshold(&s->img, (int)t) : -1; }
long gmbmedia_img_fill(long h, long rgb, long a)
{
    mb_img_slot* s = islot(h); if (!s) return -1;
    return mm_img_fill(&s->img, (int)((rgb >> 16) & 0xFF), (int)((rgb >> 8) & 0xFF), (int)(rgb & 0xFF), (int)a);
}
long gmbmedia_img_paste(long dst, long src, long x, long y)
{
    mb_img_slot* d = islot(dst), *s = islot(src); if (!d || !s) return -1;
    return mm_img_paste(&d->img, &s->img, (int)x, (int)y);
}

/* ── audio (float interleaved) ── */
long gmbmedia_audio_read(const char* path)
{
    mm_audio a; memset(&a, 0, sizeof a);
    if (mm_wav_read_float(path ? path : "", &a) != 0) return -1;
    return anew(&a);
}
long gmbmedia_audio_frames(long h) { mb_aud_slot* s = aslot(h); return s ? s->aud.frames : 0; }
long gmbmedia_audio_channels(long h) { mb_aud_slot* s = aslot(h); return s ? s->aud.channels : 0; }
long gmbmedia_audio_rate(long h) { mb_aud_slot* s = aslot(h); return s ? s->aud.sample_rate : 0; }
/* campione scalato a intero 16-bit con segno (per fedeltà su LONG) */
long gmbmedia_audio_sample(long h, long frame, long ch)
{
    mb_aud_slot* s = aslot(h); if (!s) return 0;
    mm_audio* a = &s->aud;
    if (frame < 0 || frame >= a->frames || ch < 0 || ch >= a->channels) return 0;
    double v = a->data[(size_t)frame * a->channels + ch];
    if (v > 1.0) v = 1.0; if (v < -1.0) v = -1.0;
    return (long)(v * 32767.0);
}
long gmbmedia_audio_set(long h, long frame, long ch, long val16)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    mm_audio* a = &s->aud;
    if (frame < 0 || frame >= a->frames || ch < 0 || ch >= a->channels) return -1;
    a->data[(size_t)frame * a->channels + ch] = (float)((double)val16 / 32767.0);
    return 0;
}
long gmbmedia_audio_write(long h, const char* path)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    return mm_wav_write_float(path ? path : "", &s->aud);
}
long gmbmedia_audio_free(long h)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    mm_audio_free(&s->aud);
    memset(s, 0, sizeof *s);
    return 0;
}
/* gain in centesimi di dB (es. 600 = +6.00 dB) */
long gmbmedia_audio_gain(long h, long gain_cdb)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    return mm_gain_float(s->aud.data, s->aud.frames * s->aud.channels, (double)gain_cdb / 100.0);
}
long gmbmedia_audio_normalize(long h)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    return mm_normalize_float(s->aud.data, s->aud.frames * s->aud.channels);
}
long gmbmedia_audio_fade(long h, long fade_in, long fade_out)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    return mm_fade_float(s->aud.data, s->aud.frames, s->aud.channels, fade_in, fade_out);
}
long gmbmedia_audio_reverse(long h)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    return mm_reverse_float(s->aud.data, s->aud.frames, s->aud.channels);
}
long gmbmedia_audio_to_mono(long h)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    long n = s->aud.frames;
    float* out = (float*)malloc(sizeof(float) * (size_t)(n > 0 ? n : 1));
    if (!out) return -1;
    if (mm_to_mono_float(s->aud.data, s->aud.frames, s->aud.channels, out) != 0) { free(out); return -1; }
    mm_audio a; a.data = out; a.frames = n; a.channels = 1; a.sample_rate = s->aud.sample_rate;
    return anew(&a);
}
long gmbmedia_audio_resample(long h, long out_frames, long kind)
{
    mb_aud_slot* s = aslot(h); if (!s) return -1;
    long n = out_frames > 0 ? out_frames : 1;
    float* out = (float*)malloc(sizeof(float) * (size_t)n * (size_t)(s->aud.channels > 0 ? s->aud.channels : 1));
    if (!out) return -1;
    int rc = kind ? mm_resample_sinc(s->aud.data, s->aud.frames, s->aud.channels, n, out)
                  : mm_resample_linear(s->aud.data, s->aud.frames, s->aud.channels, n, out);
    if (rc != 0) { free(out); return -1; }
    mm_audio a; a.data = out; a.frames = n; a.channels = s->aud.channels; a.sample_rate = s->aud.sample_rate;
    return anew(&a);
}
long gmbmedia_audio_mix(long a_h, long b_h, long wa_pct, long wb_pct)
{
    mb_aud_slot* a = aslot(a_h), *b = aslot(b_h); if (!a || !b) return -1;
    long n = a->aud.frames * a->aud.channels;
    if (b->aud.frames * b->aud.channels < n) n = b->aud.frames * b->aud.channels;
    float* out = (float*)malloc(sizeof(float) * (size_t)(n > 0 ? n : 1));
    if (!out) return -1;
    if (mm_mix_float(out, a->aud.data, b->aud.data, n, (double)wa_pct / 100.0, (double)wb_pct / 100.0) != 0) { free(out); return -1; }
    mm_audio m; m.data = out; m.frames = a->aud.frames; m.channels = a->aud.channels; m.sample_rate = a->aud.sample_rate;
    return anew(&m);
}

/* info WAV da path (senza tenere l'handle) */
long gmbmedia_wav_channels(const char* path) { mm_wav_meta m; return mm_wav_info(path ? path : "", &m) == 0 ? m.channels : 0; }
long gmbmedia_wav_rate(const char* path) { mm_wav_meta m; return mm_wav_info(path ? path : "", &m) == 0 ? m.sample_rate : 0; }
long gmbmedia_wav_bits(const char* path) { mm_wav_meta m; return mm_wav_info(path ? path : "", &m) == 0 ? m.bits : 0; }
long gmbmedia_wav_frames(const char* path) { mm_wav_meta m; return mm_wav_info(path ? path : "", &m) == 0 ? m.frames : 0; }
