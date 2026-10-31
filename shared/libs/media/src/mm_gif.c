/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_gif.c — codec GIF: LZW (bit LSB-first, standard GIF). Decode (primo frame, palette
 * globale/locale, interlace) + encode (quantizzazione 3-3-2). */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_subblocks(const unsigned char* p, size_t len, size_t* pos, unsigned char** out, size_t* outn) {
    size_t cap = 256, n = 0; unsigned char* buf = (unsigned char*)malloc(cap);
    if (!buf) return -1;
    while (*pos < len) {
        int sz = p[(*pos)++]; if (sz == 0) break;
        if (n + (size_t)sz > cap) { cap = (n + (size_t)sz) * 2; unsigned char* nb = (unsigned char*)realloc(buf, cap); if (!nb) { free(buf); return -1; } buf = nb; }
        if (*pos + (size_t)sz > len) { free(buf); return -1; }
        memcpy(buf + n, p + *pos, (size_t)sz); n += (size_t)sz; *pos += (size_t)sz;
    }
    *out = buf; *outn = n; return 0;
}

/* ── LZW decode (LSB-first) ── */
typedef struct { const unsigned char* p; size_t len, pos; unsigned cur; int bit; } lzw_t;
static int lzw_bit(lzw_t* b) { if (b->bit == 0) { if (b->pos >= b->len) return -1; b->cur = b->p[b->pos++]; b->bit = 8; } int v = b->cur & 1; b->cur >>= 1; b->bit--; return v; }
static int first_of(const short* prefix, int clear, int code) { while (code >= clear) code = prefix[code]; return code; }

static int lzw_decode(const unsigned char* data, size_t n, int mincode, unsigned char* out, size_t outcap, size_t* outn) {
    if (n == 0 || mincode < 2 || mincode > 8) return -1;
    int clear = 1 << mincode, eoi = clear + 1, codesize = mincode + 1, next = eoi + 1, prev = -1;
    short* prefix = (short*)malloc(sizeof(short) * 4096);
    unsigned char *suffix = (unsigned char*)malloc(4096), *rev = (unsigned char*)malloc(4096);
    if (!prefix || !suffix || !rev) { free(prefix); free(suffix); free(rev); return -1; }
    for (int i = 0; i < clear; i++) { prefix[i] = -1; suffix[i] = (unsigned char)i; }
    lzw_t b = { data, n, 0, 0, 0 };
    size_t o = 0;
    for (;;) {
        int code = 0;
        for (int i = 0; i < codesize; i++) { int bit = lzw_bit(&b); if (bit < 0) { code = -1; break; } code |= bit << i; }
        if (code < 0) break;
        if (code == clear) { codesize = mincode + 1; next = eoi + 1; prev = -1; continue; }
        if (code == eoi) break;
        int c = (code < next) ? code : prev;
        if (c < 0) { free(prefix); free(suffix); free(rev); return -1; }
        int rl = 0, x = c;
        while (x >= clear) { rev[rl++] = suffix[x]; x = prefix[x]; }
        rev[rl++] = (unsigned char)x;
        for (int i = rl - 1; i >= 0; i--) if (o < outcap) out[o++] = rev[i];
        if (code >= next && o < outcap) out[o++] = (unsigned char)first_of(prefix, clear, prev);
        if (prev >= 0 && next < 4096) { prefix[next] = (short)prev; suffix[next] = (unsigned char)first_of(prefix, clear, c); next++; if (next == (1 << codesize) && codesize < 12) codesize++; }
        prev = code;
    }
    *outn = o; free(prefix); free(suffix); free(rev); return 0;
}

int mm_gif_read(const char* path, mm_image* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 13) { fclose(f); return -1; }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);
    if (memcmp(buf, "GIF", 3)) { free(buf); return -1; }
    size_t pos = 13;
    unsigned char* gct = NULL; int gctn = 0;
    if (buf[10] & 0x80) { gctn = 2 << (buf[10] & 7); if (pos + (size_t)gctn * 3 > (size_t)sz) { free(buf); return -1; } gct = buf + pos; pos += (size_t)gctn * 3; }
    int transparent = -1;
    while (pos < (size_t)sz) {
        int blk = buf[pos++];
        if (blk == 0x3B) break;
        if (blk == 0x21) {
            if (pos >= (size_t)sz) break;
            int label = buf[pos++];
            if (label == 0xF9 && pos + 5 <= (size_t)sz) { if (buf[pos] & 1) transparent = buf[pos + 3]; }
            while (pos < (size_t)sz) { int s = buf[pos++]; if (s == 0) break; pos += (size_t)s; }
            continue;
        }
        if (blk == 0x2C) {
            if (pos + 9 > (size_t)sz) break;
            int iw = buf[pos + 4] | (buf[pos + 5] << 8), ih = buf[pos + 6] | (buf[pos + 7] << 8);
            int iflags = buf[pos + 8]; pos += 9;
            unsigned char* ct = gct; int ctn = gctn;
            if (iflags & 0x80) { ctn = 2 << (iflags & 7); if (pos + (size_t)ctn * 3 > (size_t)sz) break; ct = buf + pos; pos += (size_t)ctn * 3; }
            int interlaced = (iflags & 0x40) != 0;
            if (pos >= (size_t)sz) break;
            int mincode = buf[pos++];
            unsigned char* data; size_t dn;
            if (read_subblocks(buf, (size_t)sz, &pos, &data, &dn)) break;
            size_t npix = (size_t)iw * ih;
            unsigned char* idx = (unsigned char*)malloc(npix ? npix : 1);
            size_t got = 0;
            int ok = idx && lzw_decode(data, dn, mincode, idx, npix, &got) == 0 && got >= npix;
            free(data);
            if (!ok || !ct) { free(idx); break; }
            unsigned char* ord = idx;
            if (interlaced) {
                unsigned char* lin = (unsigned char*)malloc(npix); size_t o = 0;
                int starts[4] = { 0, 4, 2, 1 }, steps[4] = { 8, 8, 4, 2 };
                for (int pass = 0; pass < 4; pass++) for (int y = starts[pass]; y < ih; y += steps[pass]) { memcpy(lin + (size_t)y * iw, idx + o, (size_t)iw); o += (size_t)iw; }
                ord = lin;
            }
            int ch = (transparent >= 0) ? 4 : 3;
            out->width = iw; out->height = ih; out->channels = ch;
            out->data = (unsigned char*)malloc(npix * ch);
            for (size_t i = 0; i < npix; i++) {
                unsigned ix = ord[i], pi = ix * 3; int oob = (pi + 2 >= (unsigned)ctn * 3);
                out->data[i * ch] = oob ? 0 : ct[pi];
                out->data[i * ch + 1] = oob ? 0 : ct[pi + 1];
                out->data[i * ch + 2] = oob ? 0 : ct[pi + 2];
                if (ch == 4) out->data[i * ch + 3] = ((int)ix == transparent) ? 0 : 255;
            }
            if (ord != idx) free(ord);
            free(idx); free(buf);
            return 0;
        }
        break;
    }
    free(buf);
    return -1;
}

/* ── LZW encode (LSB-first) ── */
typedef struct { unsigned char* p; size_t len, cap; } wbuf;
static void wb_put(wbuf* b, unsigned char c) { if (b->len == b->cap) { b->cap = b->cap ? b->cap * 2 : 256; unsigned char* np = (unsigned char*)realloc(b->p, b->cap); if (!np) return; b->p = np; } b->p[b->len++] = c; }
typedef struct { wbuf* b; unsigned cur; int bit; } lzw_w;
static void lzw_emit(lzw_w* w, int code, int size) { for (int i = 0; i < size; i++) { if (code & (1 << i)) w->cur |= (1u << w->bit); if (++w->bit == 8) { wb_put(w->b, (unsigned char)w->cur); w->cur = 0; w->bit = 0; } } }
#define HT 16384
static int* ht_key; static int* ht_val;
static void ht_clear(void) { for (int i = 0; i < HT; i++) ht_key[i] = -1; }
static unsigned ht_hash(int key) { return ((unsigned)key * 2654435761u) & (HT - 1); }
static int ht_get(int key) { unsigned h = ht_hash(key); for (int i = 0; i < HT; i++) { unsigned j = (h + i) & (HT - 1); if (ht_key[j] == -1) return -1; if (ht_key[j] == key) return ht_val[j]; } return -1; }
static void ht_put(int key, int val) { unsigned h = ht_hash(key); for (int i = 0; i < HT; i++) { unsigned j = (h + i) & (HT - 1); if (ht_key[j] == -1) { ht_key[j] = key; ht_val[j] = val; return; } } }

/* scrive un GIF con LZW su una palette a 256 voci (indici in idx). */
static int gif_emit(const char* path, int w, int h, const unsigned char* idx, const unsigned char* palette) {
    ht_key = (int*)malloc(sizeof(int) * HT); ht_val = (int*)malloc(sizeof(int) * HT);
    if (!ht_key || !ht_val) { free(ht_key); free(ht_val); return -1; }
    wbuf body = { NULL, 0, 0 }; wb_put(&body, 8);
    lzw_w lw = { &body, 0, 0 };
    int clear = 256, eoi = 257, nexts = 258, codesize = 9;
    ht_clear();
    lzw_emit(&lw, clear, codesize);
    int prev = idx[0];
    for (int i = 1; i < w * h; i++) {
        int key = (prev << 8) | idx[i];
        int found = ht_get(key);
        if (found >= 0) { prev = found; continue; }
        lzw_emit(&lw, prev, codesize);
        if (nexts < 4096) { ht_put(key, nexts); nexts++; if (nexts == (1 << codesize) && codesize < 12) codesize++; }
        else { lzw_emit(&lw, clear, codesize); ht_clear(); nexts = 258; codesize = 9; }
        prev = idx[i];
    }
    lzw_emit(&lw, prev, codesize);
    lzw_emit(&lw, eoi, codesize);
    if (lw.bit) wb_put(&body, (unsigned char)lw.cur);

    FILE* f = fopen(path, "wb"); if (!f) { free(body.p); free(ht_key); free(ht_val); return -1; }
    unsigned char hdr[13]; memcpy(hdr, "GIF89a", 6);
    hdr[6] = w & 0xff; hdr[7] = (w >> 8) & 0xff; hdr[8] = h & 0xff; hdr[9] = (h >> 8) & 0xff;
    hdr[10] = 0xF7; hdr[11] = 0; hdr[12] = 0;
    fwrite(hdr, 1, 13, f);
    fwrite(palette, 1, 768, f);
    unsigned char idsc[10] = { 0x2C, 0, 0, 0, 0, w & 0xff, (w >> 8) & 0xff, h & 0xff, (h >> 8) & 0xff, 0 };
    fwrite(idsc, 1, 10, f);
    fwrite(body.p, 1, 1, f);
    size_t o = 1;
    while (o < body.len) { size_t take = body.len - o; if (take > 255) take = 255; unsigned char ssz = (unsigned char)take; fwrite(&ssz, 1, 1, f); fwrite(body.p + o, 1, take, f); o += take; }
    unsigned char z = 0; fwrite(&z, 1, 1, f);
    unsigned char tr = 0x3B; fwrite(&tr, 1, 1, f);
    fclose(f);
    free(body.p); free(ht_key); free(ht_val);
    return 0;
}

int mm_gif_write(const char* path, const mm_image* img) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1) return -1;
    int w = img->width, h = img->height, ch = img->channels;
    unsigned char* idx = (unsigned char*)malloc((size_t)w * h);
    if (!idx) return -1;
    for (int i = 0; i < w * h; i++) { const unsigned char* p = &img->data[(size_t)i * ch];
        int r = p[0], g = (ch == 1) ? p[0] : p[1], b = (ch == 1) ? p[0] : p[2];
        idx[i] = (unsigned char)(((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6)); }
    unsigned char pal[768];
    for (int i = 0; i < 256; i++) { pal[i * 3] = (unsigned char)(((i >> 5) & 7) * 255 / 7); pal[i * 3 + 1] = (unsigned char)(((i >> 2) & 7) * 255 / 7); pal[i * 3 + 2] = (unsigned char)((i & 3) * 255 / 3); }
    int rc = gif_emit(path, w, h, idx, pal);
    free(idx); return rc;
}

/* ── median-cut (palette adattiva) ── */
typedef struct { unsigned char r, g, b; } rgb_t;
static int cmpr(const void* a, const void* b) { const rgb_t* x = (const rgb_t*)a; const rgb_t* y = (const rgb_t*)b; if (x->r != y->r) return x->r - y->r; if (x->g != y->g) return x->g - y->g; return x->b - y->b; }
static int cmpg(const void* a, const void* b) { const rgb_t* x = (const rgb_t*)a; const rgb_t* y = (const rgb_t*)b; if (x->g != y->g) return x->g - y->g; if (x->r != y->r) return x->r - y->r; return x->b - y->b; }
static int cmpb(const void* a, const void* b) { const rgb_t* x = (const rgb_t*)a; const rgb_t* y = (const rgb_t*)b; if (x->b != y->b) return x->b - y->b; if (x->r != y->r) return x->r - y->r; return x->g - y->g; }
static void mc_rec(rgb_t* px, long lo, long hi, int* budget, rgb_t* pal, int* npal) {
    if (*npal >= 256) return;
    int rmin = 255, rmax = 0, gmin = 255, gmax = 0, bmin = 255, bmax = 0;
    for (long i = lo; i < hi; i++) { rgb_t v = px[i]; if (v.r < rmin) rmin = v.r; if (v.r > rmax) rmax = v.r; if (v.g < gmin) gmin = v.g; if (v.g > gmax) gmax = v.g; if (v.b < bmin) bmin = v.b; if (v.b > bmax) bmax = v.b; }
    int rr = rmax - rmin, gr = gmax - gmin, br = bmax - bmin;
    if ((rr == 0 && gr == 0 && br == 0) || *budget <= 0 || hi - lo <= 1) {   /* box uniforme: foglia, non consuma budget */
        long R = 0, G = 0, B = 0, n = hi - lo;
        for (long i = lo; i < hi; i++) { R += px[i].r; G += px[i].g; B += px[i].b; }
        pal[*npal].r = (unsigned char)(R / n); pal[*npal].g = (unsigned char)(G / n); pal[*npal].b = (unsigned char)(B / n); (*npal)++;
        return;
    }
    if (rr >= gr && rr >= br) qsort(px + lo, (size_t)(hi - lo), sizeof(rgb_t), cmpr);
    else if (gr >= br) qsort(px + lo, (size_t)(hi - lo), sizeof(rgb_t), cmpg);
    else qsort(px + lo, (size_t)(hi - lo), sizeof(rgb_t), cmpb);
    long mid = (lo + hi) / 2; (*budget)--;
    mc_rec(px, lo, mid, budget, pal, npal);
    mc_rec(px, mid, hi, budget, pal, npal);
}
int mm_gif_write_mc(const char* path, const mm_image* img, int maxcolors) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1) return -1;
    if (maxcolors < 2) maxcolors = 2; if (maxcolors > 256) maxcolors = 256;
    int w = img->width, h = img->height, ch = img->channels; long N = (long)w * h;
    rgb_t* px = (rgb_t*)malloc((size_t)N * sizeof(rgb_t));
    if (!px) return -1;
    for (long i = 0; i < N; i++) { const unsigned char* p = &img->data[(size_t)i * ch]; px[i].r = p[0]; px[i].g = (ch == 1) ? p[0] : p[1]; px[i].b = (ch == 1) ? p[0] : p[2]; }
    rgb_t pal[256]; int npal = 0, budget = maxcolors - 1;
    mc_rec(px, 0, N, &budget, pal, &npal);
    unsigned char* idx = (unsigned char*)malloc((size_t)N);
    unsigned char palette[768]; memset(palette, 0, sizeof palette);
    for (int i = 0; i < npal; i++) { palette[i * 3] = pal[i].r; palette[i * 3 + 1] = pal[i].g; palette[i * 3 + 2] = pal[i].b; }
    if (!idx) { free(px); return -1; }
    for (long i = 0; i < N; i++) {                         /* mappa i pixel ORIGINALI (px è stato riordinato da mc_rec) */
        const unsigned char* p = &img->data[(size_t)i * ch];
        int pr = p[0], pg = (ch == 1) ? p[0] : p[1], pb = (ch == 1) ? p[0] : p[2];
        int best = 0; long bd = 0x7fffffff;
        for (int k = 0; k < npal; k++) { long dr = (long)pr - pal[k].r, dg = (long)pg - pal[k].g, db = (long)pb - pal[k].b; long d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; best = k; } }
        idx[i] = (unsigned char)best;
    }
    free(px);
    int rc = gif_emit(path, w, h, idx, palette);
    free(idx); return rc;
}
