/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_png.c — codec PNG self-contained: CRC32/ADLER32, INFLATE (RFC1951: stored/fixed/dynamic),
 * DEFLATE a blocchi stored (scrittura), filtri PNG. Legge 8/16-bit gray/RGB/gray+alpha/RGBA/palette,
 * non interlacciato o Adam7 (16-bit -> 8-bit via byte alto); scrive 8-bit gray/RGB/RGBA. */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define GMB_NEON 1
#endif

/* ── zlib helpers ── */
static unsigned crc_tab[256]; static int crc_ready = 0;
static void crc_init(void) { for (unsigned n = 0; n < 256; n++) { unsigned c = n; for (int k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1); crc_tab[n] = c; } crc_ready = 1; }
static unsigned crc32z(unsigned crc, const unsigned char* p, size_t n) { if (!crc_ready) crc_init(); crc = ~crc; for (size_t i = 0; i < n; i++) crc = crc_tab[(crc ^ *p++) & 0xff] ^ (crc >> 8); return ~crc; }
static unsigned adler32z(const unsigned char* p, size_t n) { unsigned a = 1, b = 0; for (size_t i = 0; i < n; i++) { a = (a + p[i]) % 65521; b = (b + a) % 65521; } return (b << 16) | a; }

/* ── grow buffer (encoder) ── */
typedef struct { unsigned char* p; size_t len, cap; } gb;
static int gb_put(gb* g, unsigned char c) { if (g->len == g->cap) { size_t nc = g->cap ? g->cap * 2 : 256; unsigned char* np = (unsigned char*)realloc(g->p, nc); if (!np) return -1; g->p = np; g->cap = nc; } g->p[g->len++] = c; return 0; }
static int gb_write(gb* g, const void* d, size_t n) { const unsigned char* q = (const unsigned char*)d; for (size_t i = 0; i < n; i++) if (gb_put(g, q[i])) return -1; return 0; }
static void gb_u32(gb* g, unsigned v) { gb_put(g, (v >> 24) & 0xff); gb_put(g, (v >> 16) & 0xff); gb_put(g, (v >> 8) & 0xff); gb_put(g, v & 0xff); }

/* zlib "stored" (valido, non compresso) */
static int zlib_store(gb* g, const unsigned char* data, size_t n) {
    if (gb_put(g, 0x78) || gb_put(g, 0x01)) return -1;
    size_t off = 0;
    for (;;) {
        size_t take = n - off; if (take > 65535) take = 65535;
        int final = (off + take >= n);
        gb_put(g, final ? 1 : 0);
        unsigned len = (unsigned)take, nl = (~len) & 0xffff;
        gb_put(g, len & 0xff); gb_put(g, (len >> 8) & 0xff); gb_put(g, nl & 0xff); gb_put(g, (nl >> 8) & 0xff);
        gb_write(g, data + off, take);
        off += take;
        if (final) break;
    }
    unsigned ad = adler32z(data, n);
    gb_u32(g, ad);
    return 0;
}

/* ── INFLATE (RFC1951) ── */
typedef struct { const unsigned char* p; size_t n, pos; unsigned bitbuf; int bitcnt; } bits_t;
static int getbits(bits_t* b, int k) { while (b->bitcnt < k) { if (b->pos >= b->n) return -1; b->bitbuf |= (unsigned)b->p[b->pos++] << b->bitcnt; b->bitcnt += 8; } int v = (int)(b->bitbuf & ((1u << k) - 1)); b->bitbuf >>= k; b->bitcnt -= k; return v; }

#define HUFF_LOOK 9
typedef struct { int count[16]; short sym[288]; unsigned short fast[1 << HUFF_LOOK]; } huff;
static int rev_bits(int v, int len) { int r = 0; for (int i = 0; i < len; i++) { r = (r << 1) | (v & 1); v >>= 1; } return r; }
static void huff_build(huff* h, const unsigned char* lens, int n) {
    for (int i = 0; i < 16; i++) h->count[i] = 0;
    for (int i = 0; i < n; i++) h->count[lens[i]]++;
    h->count[0] = 0;
    int offs[16]; offs[0] = 0;
    for (int i = 1; i < 16; i++) offs[i] = offs[i - 1] + h->count[i - 1];
    for (int i = 0; i < n; i++) if (lens[i]) h->sym[offs[lens[i]]++] = (short)i;
    memset(h->fast, 0, sizeof h->fast);
    int code = 0, k = 0;
    for (int len = 1; len < 16; len++) {
        for (int i = 0; i < h->count[len]; i++) {
            if (len <= HUFF_LOOK) {
                int base = rev_bits(code, len);
                int fill = 1 << (HUFF_LOOK - len);
                unsigned short e = (unsigned short)((h->sym[k] << 4) | len);
                for (int j = 0; j < fill; j++) h->fast[base | (j << len)] = e;
            }
            k++; code++;
        }
        code <<= 1;
    }
}
static int huff_decode(bits_t* b, const huff* h) {
    while (b->bitcnt < HUFF_LOOK && b->pos < b->n) { b->bitbuf |= (unsigned)b->p[b->pos++] << b->bitcnt; b->bitcnt += 8; }
    if (b->bitcnt >= HUFF_LOOK) {
        unsigned short e = h->fast[b->bitbuf & ((1u << HUFF_LOOK) - 1)];
        if (e) { int len = e & 15; b->bitbuf >>= len; b->bitcnt -= len; return e >> 4; }
    }
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        int bit = getbits(b, 1); if (bit < 0) return -1;
        code |= bit;
        int count = h->count[len];
        if (code - first < count) return h->sym[index + (code - first)];
        index += count; first = (first + count) << 1; code <<= 1;
    }
    return -1;
}

static const int LEN_BASE[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
static const int LEN_EXT[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const int DIST_BASE[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const int DIST_EXT[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };

typedef struct { unsigned char* p; size_t len, cap; } ob;
static int ob_reserve(ob* o, size_t need) {
    if (need <= o->cap) return 0;
    size_t nc = o->cap ? o->cap : 8192;
    while (nc < need) nc += nc / 2 + 64;
    unsigned char* np = (unsigned char*)realloc(o->p, nc);
    if (!np) return -1;
    o->p = np; o->cap = nc;
    return 0;
}

static int inflate_codes(bits_t* b, ob* out, const huff* lit, const huff* dist) {
    for (;;) {
        if (out->len + 300 > out->cap && ob_reserve(out, out->len + 300)) return -1;
        int sym = huff_decode(b, lit); if (sym < 0) return -1;
        if (sym < 256) out->p[out->len++] = (unsigned char)sym;
        else if (sym == 256) return 0;
        else {
            int li = sym - 257; if (li >= 29) return -1;
            int extra = getbits(b, LEN_EXT[li]); if (extra < 0) return -1;
            int len = LEN_BASE[li] + extra;
            int ds = huff_decode(b, dist); if (ds < 0 || ds >= 30) return -1;
            int dextra = getbits(b, DIST_EXT[ds]); if (dextra < 0) return -1;
            int d = DIST_BASE[ds] + dextra;
            if ((size_t)d > out->len) return -1;
            unsigned char* o = out->p + out->len;
            const unsigned char* s = o - d;
            out->len += (size_t)len;
            if (d >= len) memcpy(o, s, (size_t)len);
            else for (int i = 0; i < len; i++) o[i] = s[i];
        }
    }
}
static int inflate_block(bits_t* b, ob* out) {
    int final, type;
    do {
        final = getbits(b, 1); type = getbits(b, 2);
        if (final < 0 || type < 0) return -1;
        if (type == 0) {
            b->bitbuf = 0; b->bitcnt = 0;
            if (b->pos + 4 > b->n) return -1;
            unsigned len = b->p[b->pos] | (b->p[b->pos + 1] << 8);
            unsigned nl = b->p[b->pos + 2] | (b->p[b->pos + 3] << 8); b->pos += 4;
            if (((~len) & 0xffff) != nl) return -1;
            if (b->pos + len > b->n) return -1;
            if (ob_reserve(out, out->len + len)) return -1;
            memcpy(out->p + out->len, b->p + b->pos, len); out->len += len; b->pos += len;
        } else if (type == 1) {
            unsigned char ll[288], dl[32];
            for (int i = 0; i < 144; i++) ll[i] = 8; for (int i = 144; i < 256; i++) ll[i] = 9;
            for (int i = 256; i < 280; i++) ll[i] = 7; for (int i = 280; i < 288; i++) ll[i] = 8;
            for (int i = 0; i < 32; i++) dl[i] = 5;
            huff L, D; huff_build(&L, ll, 288); huff_build(&D, dl, 32);
            if (inflate_codes(b, out, &L, &D)) return -1;
        } else if (type == 2) {
            int hlit = getbits(b, 5), hdist = getbits(b, 5), hclen = getbits(b, 4);
            if (hlit < 0 || hdist < 0 || hclen < 0) return -1;
            hlit += 257; hdist += 1; hclen += 4;
            static const int order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
            unsigned char cl[19]; memset(cl, 0, sizeof cl);
            for (int i = 0; i < hclen; i++) { int v = getbits(b, 3); if (v < 0) return -1; cl[order[i]] = (unsigned char)v; }
            huff CL; huff_build(&CL, cl, 19);
            unsigned char lens[320]; int n = 0;
            while (n < hlit + hdist) {
                int s = huff_decode(b, &CL); if (s < 0) return -1;
                if (s < 16) lens[n++] = (unsigned char)s;
                else if (s == 16) { if (n == 0) return -1; int r = getbits(b, 2); if (r < 0) return -1; unsigned char pv = lens[n - 1]; for (int i = 0; i < 3 + r; i++) lens[n++] = pv; }
                else if (s == 17) { int r = getbits(b, 3); if (r < 0) return -1; for (int i = 0; i < 3 + r; i++) lens[n++] = 0; }
                else { int r = getbits(b, 7); if (r < 0) return -1; for (int i = 0; i < 11 + r; i++) lens[n++] = 0; }
            }
            if (n > hlit + hdist) return -1;
            huff L, D; huff_build(&L, lens, hlit); huff_build(&D, lens + hlit, hdist);
            if (inflate_codes(b, out, &L, &D)) return -1;
        } else return -1;
    } while (!final);
    return 0;
}
static int zlib_inflate(const unsigned char* in, size_t n, unsigned char** out, size_t* on) {
    if (n < 2) return -1;
    int cmf = in[0], flg = in[1];
    if ((cmf & 0x0f) != 8 || ((cmf << 8) + flg) % 31 != 0 || (flg & 0x20)) return -1;   /* zlib, no dict */
    bits_t b = { in + 2, n - 2, 0, 0, 0 };
    ob o = { NULL, 0, 0 };
    if (inflate_block(&b, &o)) { free(o.p); return -1; }
    *out = o.p; *on = o.len;
    return 0;
}

/* ── PNG ── */
static int paeth(int a, int b, int c) { int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c); return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); }

/* de-filtra una sotto-immagine pw×ph con bpp = fbpp byte di filtro (byte per pixel). */
static unsigned char* png_defilter(const unsigned char* s, size_t stride, int ph, int fbpp) {
    unsigned char* pix = (unsigned char*)malloc(stride * (size_t)ph ? stride * (size_t)ph : 1);
    if (!pix) return NULL;
    for (int y = 0; y < ph; y++) {
        int ft = s[(stride + 1) * (size_t)y];
        const unsigned char* row = s + (stride + 1) * (size_t)y + 1;
        unsigned char* d = pix + stride * (size_t)y;
        const unsigned char* up = (y > 0) ? pix + stride * (size_t)(y - 1) : NULL;
        if (ft == 0) { memcpy(d, row, stride); continue; }
        if (ft == 2 && up) {
            size_t x = 0;
#ifdef GMB_NEON
            for (; x + 16 <= stride; x += 16) vst1q_u8(d + x, vaddq_u8(vld1q_u8(row + x), vld1q_u8(up + x)));
#endif
            for (; x < stride; x++) d[x] = (unsigned char)(row[x] + up[x]);
            continue;
        }
        if (ft == 4) {
            size_t x = 0;
            for (; x < stride; x++) {
                int a = (x >= (size_t)fbpp) ? d[x - fbpp] : 0;
                int b = up ? up[x] : 0;
                int c = (up && x >= (size_t)fbpp) ? up[x - fbpp] : 0;
                d[x] = (unsigned char)(row[x] + paeth(a, b, c));
            }
            continue;
        }
        for (size_t x = 0; x < stride; x++) {
            int a = (x >= (size_t)fbpp) ? d[x - fbpp] : 0;
            int b = up ? up[x] : 0;
            int c = (up && x >= (size_t)fbpp) ? up[x - fbpp] : 0;
            int v = row[x];
            switch (ft) { case 0: break; case 1: v += a; break; case 2: v += b; break; case 3: v += (a + b) / 2; break; case 4: v += paeth(a, b, c); break; default: break; }
            d[x] = (unsigned char)v;
        }
    }
    return pix;
}

int mm_png_read(const char* path, mm_image* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 8) { fclose(f); return -1; }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);
    static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (memcmp(buf, sig, 8)) { free(buf); return -1; }
    int w = 0, h = 0, bitd = 0, ct = -1, interlace = 0;
    unsigned char* idat = NULL; size_t idatn = 0, idatcap = 0;
    unsigned char* plte = NULL; size_t plten = 0;
    size_t p = 8;
    while (p + 8 <= (size_t)sz) {
        unsigned clen = (buf[p] << 24) | (buf[p + 1] << 16) | (buf[p + 2] << 8) | buf[p + 3];
        const char* type = (const char*)(buf + p + 4);
        if (p + 12 + clen > (size_t)sz) break;
        const unsigned char* data = buf + p + 8;
        if (!memcmp(type, "IHDR", 4) && clen >= 13) {
            w = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            h = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
            bitd = data[8]; ct = data[9]; interlace = data[12];
        } else if (!memcmp(type, "PLTE", 4)) { plte = (unsigned char*)data; plten = clen; }
        else if (!memcmp(type, "IDAT", 4)) {
            if (idatn + clen > idatcap) { idatcap = (idatn + clen) * 2 + 64; idat = (unsigned char*)realloc(idat, idatcap); }
            memcpy(idat + idatn, data, clen); idatn += clen;
        } else if (!memcmp(type, "IEND", 4)) break;
        p += 12 + clen;
    }
    if (w < 1 || h < 1 || (bitd != 1 && bitd != 2 && bitd != 4 && bitd != 8 && bitd != 16) || (interlace != 0 && interlace != 1) || ct < 0 || !idat) { free(buf); free(idat); return -1; }
    if ((ct == 2 || ct == 4 || ct == 6) && bitd != 8 && bitd != 16) { free(buf); free(idat); return -1; }   /* RGB/RGBA/gray+alpha: 8/16-bit */
    if (ct == 3 && bitd == 16) { free(buf); free(idat); return -1; }   /* palette: 1/2/4/8-bit */
    int srcch = (ct == 0) ? 1 : (ct == 2) ? 3 : (ct == 3) ? 1 : (ct == 4) ? 2 : 4;
    int bps = (bitd >= 8) ? bitd / 8 : 1;                              /* byte per campione (>=8-bit) */
    int fbpp = (bitd >= 8) ? srcch * bps : 1;                          /* byte di filtro (bit-pack: 1) */
    size_t rowbytes = (bitd >= 8) ? (size_t)w * (size_t)fbpp : ((size_t)w * (size_t)srcch * (size_t)bitd + 7u) / 8u;
    unsigned char* raw = NULL; size_t rawn = 0;
    if (zlib_inflate(idat, idatn, &raw, &rawn)) { free(buf); free(idat); return -1; }
    free(idat);
    size_t pixstride = (bitd >= 8) ? (size_t)w * (size_t)fbpp : rowbytes;
    unsigned char* pix = (unsigned char*)malloc(pixstride * (size_t)h ? pixstride * (size_t)h : 1);
    if (!pix) { free(buf); free(raw); return -1; }
    if (interlace == 0) {
        if (rawn < (rowbytes + 1) * (size_t)h) { free(buf); free(raw); free(pix); return -1; }
        unsigned char* d = png_defilter(raw, rowbytes, h, fbpp);
        if (!d) { free(buf); free(raw); free(pix); return -1; }
        memcpy(pix, d, pixstride * (size_t)h); free(d);
    } else {                                                           /* Adam7: solo >=8-bit */
        if (bitd < 8) { free(buf); free(raw); free(pix); return -1; }
        static const int X0[7] = { 0,4,0,2,0,1,0 }, Y0[7] = { 0,0,4,0,2,0,1 }, XS[7] = { 8,8,4,4,2,2,1 }, YS[7] = { 8,8,8,4,4,2,2 };
        size_t off = 0;
        for (int pass = 0; pass < 7; pass++) {
            int pw = (w > X0[pass]) ? (w - X0[pass] + XS[pass] - 1) / XS[pass] : 0;
            int ph = (h > Y0[pass]) ? (h - Y0[pass] + YS[pass] - 1) / YS[pass] : 0;
            if (pw == 0 || ph == 0) continue;
            size_t pstride = (size_t)pw * (size_t)fbpp;
            size_t sub = (pstride + 1) * (size_t)ph;
            if (off + sub > rawn) { free(buf); free(raw); free(pix); return -1; }
            unsigned char* d = png_defilter(raw + off, pstride, ph, fbpp);
            if (!d) { free(buf); free(raw); free(pix); return -1; }
            for (int j = 0; j < ph; j++) for (int i = 0; i < pw; i++) {
                int dx = X0[pass] + i * XS[pass], dy = Y0[pass] + j * YS[pass];
                memcpy(pix + ((size_t)dy * w + dx) * fbpp, d + ((size_t)j * pw + i) * fbpp, (size_t)fbpp);
            }
            free(d);
            off += sub;
        }
    }
    free(raw);
    /* 16-bit: si prende il byte alto (downscale a 8-bit). */
    int outch = (ct == 3) ? 3 : (ct == 4) ? 4 : srcch;
    unsigned char* od = (unsigned char*)malloc((size_t)w * h * outch ? (size_t)w * h * outch : 1);
    if (!od) { free(pix); return -1; }
    if (bitd < 8) {   /* bit-packed (1/2/4-bit): grigio o indice palette */
        unsigned mask = (1u << bitd) - 1u;
        for (int y = 0; y < h; y++) {
            const unsigned char* row = pix + (size_t)y * rowbytes;
            for (int x = 0; x < w; x++) {
                size_t bo = (size_t)x * (size_t)bitd;
                unsigned byte = row[bo >> 3];
                int shift = 8 - bitd - (int)(bo & 7u);
                unsigned v = (byte >> shift) & mask;
                if (ct == 3) {
                    if (!plte) { free(pix); free(od); return -1; }
                    unsigned pi = v * 3; int oob = (pi + 2 >= (unsigned)plten);
                    unsigned char* o = od + ((size_t)y * w + x) * 3;
                    o[0] = oob ? 0 : plte[pi]; o[1] = oob ? 0 : plte[pi + 1]; o[2] = oob ? 0 : plte[pi + 2];
                } else {
                    int g = (bitd == 1) ? (v ? 255 : 0) : (bitd == 2) ? (int)(v * 85) : (int)(v * 17);
                    od[(size_t)y * w + x] = (unsigned char)g;
                }
            }
        }
    } else if (ct == 3) {   /* palette 8-bit -> RGB */
        if (!plte) { free(pix); free(od); return -1; }
        for (int i = 0; i < w * h; i++) {
            unsigned idx = pix[i]; unsigned pi = idx * 3; int oob = (pi + 2 >= plten);
            od[i * 3] = oob ? 0 : plte[pi]; od[i * 3 + 1] = oob ? 0 : plte[pi + 1]; od[i * 3 + 2] = oob ? 0 : plte[pi + 2];
        }
    } else if (ct == 4) {   /* gray+alpha -> RGBA */
        for (int i = 0; i < w * h; i++) {
            unsigned char g = pix[(size_t)i * 2 * bps], a = pix[((size_t)i * 2 + 1) * bps];
            od[i * 4] = od[i * 4 + 1] = od[i * 4 + 2] = g; od[i * 4 + 3] = a;
        }
    } else if (bps == 1) {   /* 8-bit gray/RGB/RGBA: layout di uscita = quello di pix */
        memcpy(od, pix, (size_t)w * (size_t)h * (size_t)srcch);
    } else {
        for (int i = 0; i < w * h; i++)
            for (int c = 0; c < srcch; c++)
                od[(size_t)i * srcch + c] = pix[((size_t)i * srcch + c) * bps];
    }
    free(pix);
    out->width = w; out->height = h; out->channels = outch; out->data = od;
    free(buf);   /* la fonte del PLTE va liberata DOPO l'uso della palette */
    return 0;
}

int mm_png_write(const char* path, const mm_image* img) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1) return -1;
    int ct, srcch = img->channels;
    if (srcch == 1) ct = 0; else if (srcch == 3) ct = 2; else if (srcch == 4) ct = 6; else return -1;
    size_t stride = (size_t)img->width * srcch;
    /* filtra (filter 0 = None) */
    unsigned char* raw = (unsigned char*)malloc((stride + 1) * (size_t)img->height);
    for (int y = 0; y < img->height; y++) { raw[(stride + 1) * y] = 0; memcpy(raw + (stride + 1) * y + 1, img->data + stride * y, stride); }
    gb g = { NULL, 0, 0 };
    unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    gb_write(&g, sig, 8);
    /* IHDR */
    gb_u32(&g, 13); unsigned cs = crc32z(0, (const unsigned char*)"IHDR", 4); gb_write(&g, "IHDR", 4);
    unsigned char ih[13]; ih[0] = (img->width >> 24) & 0xff; ih[1] = (img->width >> 16) & 0xff; ih[2] = (img->width >> 8) & 0xff; ih[3] = img->width & 0xff;
    ih[4] = (img->height >> 24) & 0xff; ih[5] = (img->height >> 16) & 0xff; ih[6] = (img->height >> 8) & 0xff; ih[7] = img->height & 0xff;
    ih[8] = 8; ih[9] = (unsigned char)ct; ih[10] = 0; ih[11] = 0; ih[12] = 0;
    gb_write(&g, ih, 13); gb_u32(&g, crc32z(cs, ih, 13));
    /* IDAT (zlib stored) */
    gb id = { NULL, 0, 0 }; zlib_store(&id, raw, (stride + 1) * (size_t)img->height);
    gb_u32(&g, (unsigned)id.len); unsigned cs2 = crc32z(0, (const unsigned char*)"IDAT", 4); gb_write(&g, "IDAT", 4);
    gb_write(&g, id.p, id.len); gb_u32(&g, crc32z(cs2, id.p, id.len));
    /* IEND */
    gb_u32(&g, 0); unsigned cs3 = crc32z(0, (const unsigned char*)"IEND", 4); gb_write(&g, "IEND", 4); gb_u32(&g, crc32z(cs3, NULL, 0));
    FILE* f = fopen(path, "wb"); if (!f) { free(g.p); free(id.p); free(raw); return -1; }
    size_t wr = fwrite(g.p, 1, g.len, f);
    fclose(f);
    free(g.p); free(id.p); free(raw);
    return wr == g.len ? 0 : -1;
}
