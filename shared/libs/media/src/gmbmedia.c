/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gmbmedia.c — WAV (RIFF) PCM16/FLOAT32 + DSP (gain, normalize, fade, reverse, mix, mono, resample). */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static unsigned rd32(const unsigned char* p) { return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }
static unsigned rd16(const unsigned char* p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static void wr32(unsigned char* p, unsigned v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff; }
static void wr16(unsigned char* p, unsigned v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }

/* legge header; ritorna af/ch/sr/bits + offset/size del data (in FILE* f posizionato a inizio dati). */
static int wav_scan(FILE* f, int* af, int* ch, int* sr, int* bits, long* doff, unsigned long* dsize) {
    unsigned char h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return -1;
    *ch = 0; *bits = 0; *dsize = 0; *af = 1;
    unsigned char ck[8];
    while (fread(ck, 1, 8, f) == 8) {
        unsigned sz = rd32(ck + 4);
        if (!memcmp(ck, "fmt ", 4)) {
            unsigned char fm[16];
            if (sz < 16 || fread(fm, 1, 16, f) != 16) return -1;
            *af = (int)rd16(fm + 0); *ch = (int)rd16(fm + 2); *sr = (int)rd32(fm + 4); *bits = (int)rd16(fm + 14);
            if (sz > 16) fseek(f, (long)(sz - 16), SEEK_CUR);
        } else if (!memcmp(ck, "data", 4)) { *doff = (long)ftell(f); *dsize = sz; fseek(f, (long)sz, SEEK_CUR); }
        else fseek(f, (long)(sz + (sz & 1)), SEEK_CUR);
        if (*ch && *dsize) break;
    }
    return (*ch && *bits) ? 0 : -1;
}

int mm_wav_info(const char* path, mm_wav_meta* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    int af, ch, sr, bits; long doff; unsigned long dsize;
    int rc = wav_scan(f, &af, &ch, &sr, &bits, &doff, &dsize);
    fclose(f);
    if (rc) return -1;
    out->channels = ch; out->sample_rate = sr; out->bits = bits;
    out->frames = (long)(dsize / (unsigned long)(ch * (bits / 8)));
    out->duration = sr ? (double)out->frames / sr : 0.0;
    return 0;
}

int mm_wav_read_pcm16(const char* path, short** pcm, long* frames, int* channels, int* sample_rate) {
    if (!path || !pcm || !frames) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    int af, ch, sr, bits; long doff; unsigned long dsize;
    if (wav_scan(f, &af, &ch, &sr, &bits, &doff, &dsize) || bits != 16) { fclose(f); return -1; }
    long n = (long)(dsize / 2);
    short* buf = (short*)malloc((size_t)n * sizeof(short));
    if (!buf) { fclose(f); return -1; }
    fseek(f, doff, SEEK_SET);
    size_t got = fread(buf, 2, (size_t)n, f);
    fclose(f);
    if ((long)got != n) { free(buf); return -1; }
    *pcm = buf; *frames = n / ch;
    if (channels) *channels = ch; if (sample_rate) *sample_rate = sr;
    return 0;
}

int mm_wav_write_pcm16(const char* path, const short* pcm, long frames, int channels, int sample_rate) {
    if (!path || !pcm || frames < 0 || channels < 1) return -1;
    unsigned long dsize = (unsigned long)frames * (unsigned long)channels * 2u;
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    unsigned char hdr[44];
    memcpy(hdr, "RIFF", 4); wr32(hdr + 4, (unsigned)(36 + dsize)); memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4); wr32(hdr + 16, 16); wr16(hdr + 20, 1); wr16(hdr + 22, (unsigned)channels);
    wr32(hdr + 24, (unsigned)sample_rate); wr32(hdr + 28, (unsigned)(sample_rate * channels * 2));
    wr16(hdr + 32, (unsigned)(channels * 2)); wr16(hdr + 34, 16);
    memcpy(hdr + 36, "data", 4); wr32(hdr + 40, (unsigned)dsize);
    size_t w = fwrite(hdr, 1, 44, f), w2 = fwrite(pcm, 2, (size_t)(frames * channels), f);
    fclose(f);
    return (w == 44 && w2 == (size_t)(frames * channels)) ? 0 : -1;
}

static int wav_write_int(const char* path, const int* pcm, long frames, int channels, int sample_rate, int bps) {
    if (!path || !pcm || frames < 0 || channels < 1 || (bps != 2 && bps != 3 && bps != 4)) return -1;
    unsigned long dsize = (unsigned long)frames * (unsigned long)channels * (unsigned)bps;
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    unsigned char hdr[44];
    memcpy(hdr, "RIFF", 4); wr32(hdr + 4, (unsigned)(36 + dsize)); memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4); wr32(hdr + 16, 16); wr16(hdr + 20, 1); wr16(hdr + 22, (unsigned)channels);
    wr32(hdr + 24, (unsigned)sample_rate); wr32(hdr + 28, (unsigned)(sample_rate * channels * bps));
    wr16(hdr + 32, (unsigned)(channels * bps)); wr16(hdr + 34, (unsigned)(bps * 8));
    memcpy(hdr + 36, "data", 4); wr32(hdr + 40, (unsigned)dsize);
    if (fwrite(hdr, 1, 44, f) != 44) { fclose(f); return -1; }
    unsigned char b[4];
    for (long i = 0; i < frames * channels; i++) {
        int v = pcm[i];
        if (bps == 2) { if (v > 32767) v = 32767; if (v < -32768) v = -32768; b[0] = v & 0xff; b[1] = (v >> 8) & 0xff; }
        else if (bps == 3) { if (v > 8388607) v = 8388607; if (v < -8388608) v = -8388608; b[0] = v & 0xff; b[1] = (v >> 8) & 0xff; b[2] = (v >> 16) & 0xff; }
        else { b[0] = v & 0xff; b[1] = (v >> 8) & 0xff; b[2] = (v >> 16) & 0xff; b[3] = (v >> 24) & 0xff; }
        if (fwrite(b, 1, (size_t)bps, f) != (size_t)bps) { fclose(f); return -1; }
    }
    fclose(f); return 0;
}
int mm_wav_write_pcm24(const char* path, const int* pcm, long frames, int channels, int sample_rate) { return wav_write_int(path, pcm, frames, channels, sample_rate, 3); }
int mm_wav_write_pcm32(const char* path, const int* pcm, long frames, int channels, int sample_rate) { return wav_write_int(path, pcm, frames, channels, sample_rate, 4); }

int mm_gain_pcm16(short* pcm, long samples, double gain_db) {
    if (!pcm || samples < 0) return -1;
    double g = pow(10.0, gain_db / 20.0);
    for (long i = 0; i < samples; i++) {
        double v = (double)pcm[i] * g;
        if (v > 32767.0) v = 32767.0; if (v < -32768.0) v = -32768.0;
        pcm[i] = (short)(v >= 0 ? v + 0.5 : v - 0.5);
    }
    return 0;
}

/* ── read_float: 8/16/24/32 int e FLOAT32 ── */
static double rd_sample(const unsigned char* p, int af, int bits) {
    if (af == 3 && bits == 32) { float fv; memcpy(&fv, p, 4); return (double)fv; }
    switch (bits) {
        case 8:  return ((double)p[0] - 128.0) / 128.0;
        case 16: return (double)(short)(p[0] | (p[1] << 8)) / 32768.0;
        case 24: { int v = p[0] | (p[1] << 8) | (p[2] << 16); if (v & 0x800000) v -= 0x1000000; return (double)v / 8388608.0; }
        case 32: { int v = (int)rd32(p); return (double)v / 2147483648.0; }
    }
    return 0.0;
}
int mm_wav_read_float(const char* path, mm_audio* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    int af, ch, sr, bits; long doff; unsigned long dsize;
    if (wav_scan(f, &af, &ch, &sr, &bits, &doff, &dsize)) { fclose(f); return -1; }
    int bps = bits / 8;
    long nsamp = (long)(dsize / (unsigned)bps);
    float* buf = (float*)malloc((size_t)nsamp * sizeof(float));
    if (!buf) { fclose(f); return -1; }
    fseek(f, doff, SEEK_SET);
    unsigned char raw[8];
    for (long i = 0; i < nsamp; i++) { if (fread(raw, 1, (size_t)bps, f) != (size_t)bps) { free(buf); fclose(f); return -1; } buf[i] = (float)rd_sample(raw, af, bits); }
    fclose(f);
    out->data = buf; out->frames = nsamp / ch; out->channels = ch; out->sample_rate = sr;
    return 0;
}
int mm_wav_write_float(const char* path, const mm_audio* a) {
    if (!path || !a || !a->data || a->frames < 0 || a->channels < 1) return -1;
    unsigned long dsize = (unsigned long)a->frames * (unsigned long)a->channels * 4u;
    FILE* f = fopen(path, "wb"); if (!f) return -1;
    unsigned char hdr[44];
    memcpy(hdr, "RIFF", 4); wr32(hdr + 4, (unsigned)(36 + dsize)); memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4); wr32(hdr + 16, 16); wr16(hdr + 20, 3); wr16(hdr + 22, (unsigned)a->channels);
    wr32(hdr + 24, (unsigned)a->sample_rate); wr32(hdr + 28, (unsigned)(a->sample_rate * a->channels * 4));
    wr16(hdr + 32, (unsigned)(a->channels * 4)); wr16(hdr + 34, 32);
    memcpy(hdr + 36, "data", 4); wr32(hdr + 40, (unsigned)dsize);
    size_t w = fwrite(hdr, 1, 44, f);
    size_t w2 = fwrite(a->data, 4, (size_t)(a->frames * a->channels), f);
    fclose(f);
    return (w == 44 && w2 == (size_t)(a->frames * a->channels)) ? 0 : -1;
}
void mm_audio_free(mm_audio* a) { if (a) { free(a->data); a->data = NULL; a->frames = 0; } }

/* ── DSP ── */
int mm_gain_float(float* x, long n, double gain_db) {
    if (!x || n < 0) return -1;
    double g = pow(10.0, gain_db / 20.0);
    for (long i = 0; i < n; i++) x[i] = (float)(x[i] * g);
    return 0;
}
int mm_normalize_float(float* x, long n) {
    if (!x || n < 0) return -1;
    double pk = 0;
    for (long i = 0; i < n; i++) { double v = fabs(x[i]); if (v > pk) pk = v; }
    if (pk > 1e-12) { float g = (float)(1.0 / pk); for (long i = 0; i < n; i++) x[i] *= g; }
    return 0;
}
int mm_fade_float(float* x, long frames, int channels, long fin, long fout) {
    if (!x || frames < 0 || channels < 1) return -1;
    for (long fr = 0; fr < frames; fr++) {
        double g = 1.0;
        if (fin > 0 && fr < fin) g *= (double)fr / fin;
        if (fout > 0 && fr >= frames - fout) g *= (double)(frames - fr) / fout;
        for (int c = 0; c < channels; c++) x[fr * channels + c] = (float)(x[fr * channels + c] * g);
    }
    return 0;
}
int mm_reverse_float(float* x, long frames, int channels) {
    if (!x || frames < 0 || channels < 1) return -1;
    for (long i = 0, j = frames - 1; i < j; i++, j--)
        for (int c = 0; c < channels; c++) { float t = x[i * channels + c]; x[i * channels + c] = x[j * channels + c]; x[j * channels + c] = t; }
    return 0;
}
int mm_mix_float(float* dst, const float* a, const float* b, long n, double wa, double wb) {
    if (!dst || !a || !b || n < 0) return -1;
    for (long i = 0; i < n; i++) dst[i] = (float)(a[i] * wa + b[i] * wb);
    return 0;
}
int mm_to_mono_float(const float* x, long frames, int channels, float* out) {
    if (!x || !out || frames < 0 || channels < 1) return -1;
    for (long fr = 0; fr < frames; fr++) {
        double s = 0; for (int c = 0; c < channels; c++) s += x[fr * channels + c];
        out[fr] = (float)(s / channels);
    }
    return 0;
}
/* ── biquad (RBJ) ── */
void mm_biquad_lowpass(double fc, int sr, double q, double b[3], double a[3]) {
    if (fc < 1) fc = 1; if (fc > sr / 2.0 - 1) fc = sr / 2.0 - 1; if (q < 0.01) q = 0.01;
    double w0 = 2.0 * M_PI * fc / sr, cw = cos(w0), sw = sin(w0), alpha = sw / (2.0 * q);
    double a0 = 1.0 + alpha;
    b[0] = ((1.0 - cw) / 2.0) / a0; b[1] = (1.0 - cw) / a0; b[2] = ((1.0 - cw) / 2.0) / a0;
    a[0] = 1.0; a[1] = (-2.0 * cw) / a0; a[2] = (1.0 - alpha) / a0;
}
int mm_biquad_float(float* x, long frames, int channels, const double b[3], const double a[3]) {
    if (!x || !b || !a || frames < 0 || channels < 1) return -1;
    for (int c = 0; c < channels; c++) {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (long f = 0; f < frames; f++) {
            double xn = x[f * channels + c];
            double yn = b[0] * xn + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2;
            x2 = x1; x1 = xn; y2 = y1; y1 = yn;
            x[f * channels + c] = (float)yn;
        }
    }
    return 0;
}

int mm_resample_sinc(const float* in, long in_frames, int ch, long out_frames, float* out) {
    if (!in || !out || in_frames < 1 || out_frames < 0 || ch < 1) return -1;
    if (out_frames == 0) return 0;
    if (in_frames == 1) { for (long o = 0; o < out_frames; o++) for (int c = 0; c < ch; c++) out[o * ch + c] = in[c]; return 0; }
    double ratio = (double)(in_frames - 1) / (double)(out_frames > 1 ? out_frames - 1 : 1);
    double cutoff = ratio > 1.0 ? 1.0 / ratio : 1.0;       /* anti-alias in downsampling */
    const int taps = 16; double half = taps / 2.0;
    double* acc = (double*)malloc((size_t)ch * sizeof(double));
    if (!acc) return -1;
    for (long o = 0; o < out_frames; o++) {
        double center = (double)o * ratio;
        long i0 = (long)floor(center) - taps / 2 + 1;
        for (int c = 0; c < ch; c++) acc[c] = 0;
        double wsum = 0;
        for (int k = 0; k < taps; k++) {
            long idx = i0 + k;
            double x = center - (double)idx;
            double t = x / half;
            double win = (t >= -1.0 && t <= 1.0) ? 0.5 * (1.0 + cos(M_PI * t)) : 0.0;   /* Hann */
            double a = M_PI * x * cutoff;
            double s = (a == 0.0) ? 1.0 : sin(a) / a;
            double w = s * win;
            long ci = idx < 0 ? 0 : (idx >= in_frames ? in_frames - 1 : idx);
            for (int c = 0; c < ch; c++) acc[c] += w * in[ci * ch + c];
            wsum += w;
        }
        if (wsum != 0.0) for (int c = 0; c < ch; c++) out[o * ch + c] = (float)(acc[c] / wsum);
        else { long ci = center < 0 ? 0 : ((long)center >= in_frames ? in_frames - 1 : (long)center); for (int c = 0; c < ch; c++) out[o * ch + c] = in[ci * ch + c]; }
    }
    free(acc);
    return 0;
}
int mm_resample_linear(const float* in, long in_frames, int ch, long out_frames, float* out) {
    if (!in || !out || in_frames < 1 || out_frames < 0 || ch < 1) return -1;
    for (long i = 0; i < out_frames; i++) {
        double pos = (double)i * (double)(in_frames - 1) / (double)(out_frames > 1 ? out_frames - 1 : 1);
        long i0 = (long)pos; if (i0 >= in_frames - 1) i0 = in_frames - 2 < 0 ? 0 : in_frames - 2;
        double t = pos - i0;
        for (int c = 0; c < ch; c++) {
            double v0 = in[i0 * ch + c], v1 = in[(i0 + 1) * ch + c];
            out[i * ch + c] = (float)(v0 + (v1 - v0) * t);
        }
    }
    return 0;
}
