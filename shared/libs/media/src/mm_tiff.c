/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_tiff.c — TIFF (baseline): scrittura non compressa o PackBits (scelta automatica),
 * lettura Compression=1 (none) e 32773 (PackBits), little/big-endian, 8-bit
 * grigio/RGB/RGBA. Strip singolo o multiplo, PlanarConfiguration=1 (chunky). */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIFF_NONE     1
#define TIFF_PACKBITS 32773
#define TIFF_MAXSTRIP 4096

static unsigned rd16(const unsigned char* p, int le) { return le ? (unsigned)(p[0] | (p[1] << 8)) : (unsigned)((p[0] << 8) | p[1]); }
static unsigned rd32(const unsigned char* p, int le) {
    if (le) return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}
static void wr16le(unsigned char* p, unsigned v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }
static void wr32le(unsigned char* p, unsigned v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff; }

/* ── PackBits (RFC del formato TIFF) ── */
static long packbits_enc(const unsigned char* in, long n, unsigned char* out) {
    long i = 0, o = 0;
    while (i < n) {
        long run = 1;
        while (i + run < n && run < 128 && in[i + run] == in[i]) run++;
        if (run >= 3) {                                    /* run: -(run-1) poi il byte */
            out[o++] = (unsigned char)(257 - run);
            out[o++] = in[i];
            i += run;
        } else {                                           /* letterali */
            long start = i;
            while (i < n && (i - start) < 128) {
                if (i + 2 < n && in[i] == in[i + 1] && in[i + 1] == in[i + 2]) break;
                i++;
            }
            long len = i - start;
            out[o++] = (unsigned char)(len - 1);
            memcpy(out + o, in + start, (size_t)len);
            o += len;
        }
    }
    return o;
}
static long packbits_dec(const unsigned char* in, long n, unsigned char* out, long cap) {
    long i = 0, o = 0;
    while (i < n) {
        int c = (signed char)in[i++];
        if (c >= 0) {                                      /* c+1 letterali */
            long len = (long)c + 1;
            if (i + len > n) len = n - i;
            if (o + len > cap) len = cap - o;
            if (len <= 0) break;
            memcpy(out + o, in + i, (size_t)len);
            o += len; i += len;
        } else if (c != -128) {                            /* -127..-1 = run */
            long len = (long)(-c) + 1;
            unsigned char b = (i < n) ? in[i++] : 0;
            for (long k = 0; k < len && o < cap; k++) out[o++] = b;
        }
    }
    return o;
}

/* ── scrittura ── */
static void put_entry(unsigned char* e, unsigned tag, unsigned type, unsigned count, unsigned val) {
    wr16le(e, tag); wr16le(e + 2, type); wr32le(e + 4, count); wr32le(e + 8, val);
}
int mm_tiff_write(const char* path, const mm_image* img) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1) return -1;
    int ch = img->channels;
    if (ch != 1 && ch != 3 && ch != 4) ch = 3;             /* 2ch: promosso a RGB */
    long W = img->width, H = img->height, pix = W * H, raw_n = pix * ch;
    unsigned char* raw = (unsigned char*)malloc((size_t)raw_n);
    if (!raw) return -1;
    for (long i = 0; i < pix; i++) {
        const unsigned char* px = &img->data[(size_t)i * img->channels];
        unsigned char r, g, b, a;
        if (img->channels == 1) { r = g = b = px[0]; a = 255; }
        else { r = px[0]; g = px[1]; b = px[2]; a = (img->channels == 4) ? px[3] : 255; }
        if (ch == 1) raw[i] = r;
        else if (ch == 3) { raw[i * 3] = r; raw[i * 3 + 1] = g; raw[i * 3 + 2] = b; }
        else { raw[i * 4] = r; raw[i * 4 + 1] = g; raw[i * 4 + 2] = b; raw[i * 4 + 3] = a; }
    }
    unsigned char* packed = (unsigned char*)malloc((size_t)raw_n + (size_t)(raw_n / 128) + 2);
    long pn = packed ? packbits_enc(raw, raw_n, packed) : 0;
    int comp; const unsigned char* data; long dsize;
    if (packed && pn > 0 && pn < raw_n) { comp = TIFF_PACKBITS; data = packed; dsize = pn; }
    else { comp = TIFF_NONE; data = raw; dsize = raw_n; }

    int isRGBA = (ch == 4);
    int N = isRGBA ? 11 : 10;
    long ifd_off = 8, ifd_size = 2 + (long)N * 12 + 4;
    long arr = ifd_off + ifd_size, bps_off = 0;
    if (ch > 2) { bps_off = arr; arr += ch * 2; }
    long data_off = arr;

    FILE* f = fopen(path, "wb");
    if (!f) { free(raw); free(packed); return -1; }
    unsigned char hd[8]; hd[0] = 'I'; hd[1] = 'I'; wr16le(hd + 2, 42); wr32le(hd + 4, (unsigned)ifd_off);
    fwrite(hd, 1, 8, f);

    unsigned char* ifd = (unsigned char*)calloc((size_t)ifd_size, 1);
    if (!ifd) { fclose(f); free(raw); free(packed); return -1; }
    wr16le(ifd, (unsigned)N);
    unsigned char* e = ifd + 2; int idx = 0;
    put_entry(e + idx++ * 12, 256, 4, 1, (unsigned)W);                 /* ImageWidth */
    put_entry(e + idx++ * 12, 257, 4, 1, (unsigned)H);                 /* ImageLength */
    if (ch <= 2) put_entry(e + idx++ * 12, 258, 3, 1, 8);              /* BitsPerSample (inline) */
    else put_entry(e + idx++ * 12, 258, 3, (unsigned)ch, (unsigned)bps_off);
    put_entry(e + idx++ * 12, 259, 3, 1, (unsigned)comp);              /* Compression */
    put_entry(e + idx++ * 12, 262, 3, 1, ch == 1 ? 1u : 2u);           /* Photometric */
    put_entry(e + idx++ * 12, 273, 4, 1, (unsigned)data_off);          /* StripOffsets */
    put_entry(e + idx++ * 12, 277, 3, 1, (unsigned)ch);                /* SamplesPerPixel */
    put_entry(e + idx++ * 12, 278, 4, 1, (unsigned)H);                 /* RowsPerStrip */
    put_entry(e + idx++ * 12, 279, 4, 1, (unsigned)dsize);             /* StripByteCounts */
    put_entry(e + idx++ * 12, 284, 3, 1, 1);                           /* PlanarConfiguration */
    if (isRGBA) put_entry(e + idx++ * 12, 338, 3, 1, 2);               /* ExtraSamples = alpha */
    wr32le(ifd + 2 + (long)idx * 12, 0);                               /* next IFD */
    fwrite(ifd, 1, (size_t)ifd_size, f);
    free(ifd);
    if (ch > 2) { unsigned char bps[8]; for (int i = 0; i < ch; i++) wr16le(bps + i * 2, 8); fwrite(bps, 1, (size_t)ch * 2, f); }
    fwrite(data, 1, (size_t)dsize, f);
    fclose(f); free(raw); free(packed);
    return 0;
}

/* ── lettura ── */
static int tiff_vals(FILE* f, int le, unsigned type, unsigned count, unsigned val_field, long* out, unsigned max) {
    if (count == 0) return 0;
    if (count == 1 && (type == 3 || type == 4 || type == 5)) { out[0] = (type == 3) ? (val_field & 0xffff) : val_field; return 1; }
    if (count > max) count = max;
    if (fseek(f, (long)val_field, SEEK_SET) != 0) return 0;
    for (unsigned i = 0; i < count; i++) {
        unsigned char b[4];
        if (type == 3) { if (fread(b, 1, 2, f) != 2) return (int)i; out[i] = rd16(b, le); }
        else { if (fread(b, 1, 4, f) != 4) return (int)i; out[i] = rd32(b, le); }
    }
    return (int)count;
}
static int tiff_read_impl(const char* path, int page, mm_image* out) {
    if (!path || !out) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char h[8];
    if (fread(h, 1, 8, f) != 8) { fclose(f); return -1; }
    int le;
    if (h[0] == 'I' && h[1] == 'I') le = 1; else if (h[0] == 'M' && h[1] == 'M') le = 0; else { fclose(f); return -1; }
    if (rd16(h + 2, le) != 42) { fclose(f); return -1; }
    unsigned ifd = rd32(h + 4, le);
    for (int pg = 0; pg < page; pg++) {                    /* segui la catena next-IFD fino alla pagina richiesta */
        unsigned char c2[2], nb[4];
        if (fseek(f, (long)ifd, SEEK_SET) != 0) { fclose(f); return -1; }
        if (fread(c2, 1, 2, f) != 2) { fclose(f); return -1; }
        unsigned n2 = rd16(c2, le);
        if (fseek(f, (long)ifd + 2 + (long)n2 * 12, SEEK_SET) != 0) { fclose(f); return -1; }
        if (fread(nb, 1, 4, f) != 4) { fclose(f); return -1; }
        unsigned nx = rd32(nb, le);
        if (nx == 0) { fclose(f); return -1; }
        ifd = nx;
    }
    if (fseek(f, (long)ifd, SEEK_SET) != 0) { fclose(f); return -1; }
    unsigned char cb[2];
    if (fread(cb, 1, 2, f) != 2) { fclose(f); return -1; }
    unsigned n = rd16(cb, le);

    unsigned W = 0, H = 0, Spp = 1, Comp = 1, Photo = 2, Planar = 1, Rps = 0;
    unsigned bps_count = 0, bps_first = 8, all8 = 1;
    long soff[TIFF_MAXSTRIP], scnt[TIFF_MAXSTRIP]; unsigned nso = 0, nsc = 0;
    long cmap[768]; unsigned ncmap = 0;
    unsigned char entry[12];
    for (unsigned k = 0; k < n; k++) {
        if (fseek(f, (long)ifd + 2 + (long)k * 12, SEEK_SET) != 0) break;
        if (fread(entry, 1, 12, f) != 12) break;
        unsigned tag = rd16(entry, le), type = rd16(entry + 2, le), count = rd32(entry + 4, le);
        unsigned v16 = rd16(entry + 8, le), v32 = rd32(entry + 8, le);
        switch (tag) {
        case 256: W = (type == 3) ? v16 : v32; break;
        case 257: H = (type == 3) ? v16 : v32; break;
        case 258: {
            bps_count = count;
            long tmp[8];
            int got = tiff_vals(f, le, type, count, v32, tmp, 8);
            bps_first = got > 0 ? (unsigned)tmp[0] : 8;
            for (int i = 0; i < got; i++) if (tmp[i] != 8) all8 = 0;
            break;
        }
        case 259: Comp = v16; break;
        case 262: Photo = v16; break;
        case 273: nso = (unsigned)tiff_vals(f, le, type, count, v32, soff, TIFF_MAXSTRIP); break;
        case 277: Spp = v16; break;
        case 278: Rps = v32; break;
        case 279: nsc = (unsigned)tiff_vals(f, le, type, count, v32, scnt, TIFF_MAXSTRIP); break;
        case 284: Planar = v16; break;
        case 320: ncmap = (unsigned)tiff_vals(f, le, type, count, v32, cmap, 768); break;   /* ColorMap (palette) */
        default: break;
        }
    }
    (void)bps_count;
    if (W < 1 || H < 1 || Spp < 1 || Spp > 4 || !all8 || bps_first != 8) { fclose(f); return -1; }
    if (Photo == 3 && (Spp != 1 || ncmap < 768)) { fclose(f); return -1; }   /* palette: 1 campione + ColorMap completa */
    if (Photo == 5 && Spp != 4) { fclose(f); return -1; }                    /* CMYK: 4 campioni */
    if (Comp != TIFF_NONE && Comp != TIFF_PACKBITS) { fclose(f); return -1; }
    if (Planar != 1) { fclose(f); return -1; }             /* solo chunky */
    if (Rps == 0) Rps = H;
    long rowbytes = (long)W * (long)Spp;
    long nstrip = ((long)H + (long)Rps - 1) / (long)Rps;
    if (nso == 0) { fclose(f); return -1; }                /* StripOffsets obbligatorio */
    if (nsc == 0) for (long s = 0; s < nstrip; s++) { long rows = (s == nstrip - 1) ? ((long)H - s * (long)Rps) : (long)Rps; scnt[s] = rows * rowbytes; nsc = nstrip; }

    long cap = rowbytes * (long)Rps;
    long sbuf_cap = cap + cap / 128 + 16;                  /* PackBits: worst case leggermente oltre il raw */
    unsigned char* raw = (unsigned char*)malloc((size_t)rowbytes * (size_t)H);
    unsigned char* sbuf = (unsigned char*)malloc((size_t)sbuf_cap);
    unsigned char* dec = (unsigned char*)malloc((size_t)cap);
    if (!raw || !sbuf || !dec) { free(raw); free(sbuf); free(dec); fclose(f); return -1; }
    for (long s = 0; s < nstrip; s++) {
        long need = (s < (long)nsc) ? scnt[s] : 0;
        if (need < 0 || need > sbuf_cap) need = cap;
        if (fseek(f, (long)soff[s], SEEK_SET) != 0) { free(raw); free(sbuf); free(dec); fclose(f); return -1; }
        size_t rd = fread(sbuf, 1, (size_t)need, f);
        long cap = rowbytes * (long)Rps;
        long dn = (Comp == TIFF_PACKBITS) ? packbits_dec(sbuf, (long)rd, dec, cap) : ((long)rd < cap ? (long)rd : cap);
        if (Comp == TIFF_NONE) memcpy(dec, sbuf, (size_t)((long)rd < cap ? (long)rd : cap));
        long rows = (s == nstrip - 1) ? ((long)H - s * (long)Rps) : (long)Rps;
        long copy = rows * rowbytes;
        if (dn < copy) copy = dn;
        memcpy(raw + (size_t)(s * (long)Rps) * (size_t)rowbytes, dec, (size_t)copy);
    }
    free(sbuf); free(dec); fclose(f);

    if (Photo == 3) {                                      /* palette -> RGB (ColorMap 16-bit) */
        unsigned char* conv = (unsigned char*)malloc((size_t)W * (size_t)H * 3);
        if (!conv) { free(raw); return -1; }
        for (long i = 0; i < (long)W * (long)H; i++) {
            unsigned idx = raw[i]; if (idx > 255) idx = 255;
            conv[i * 3] = (unsigned char)(cmap[idx] >> 8);
            conv[i * 3 + 1] = (unsigned char)(cmap[256 + idx] >> 8);
            conv[i * 3 + 2] = (unsigned char)(cmap[512 + idx] >> 8);
        }
        free(raw);
        out->data = conv; out->width = (int)W; out->height = (int)H; out->channels = 3;
        return 0;
    }
    if (Photo == 5) {                                      /* CMYK -> RGB */
        unsigned char* conv = (unsigned char*)malloc((size_t)W * (size_t)H * 3);
        if (!conv) { free(raw); return -1; }
        for (long i = 0; i < (long)W * (long)H; i++) {
            int C = raw[i * 4], M = raw[i * 4 + 1], Y = raw[i * 4 + 2], K = raw[i * 4 + 3];
            conv[i * 3] = (unsigned char)((255 - C) * (255 - K) / 255);
            conv[i * 3 + 1] = (unsigned char)((255 - M) * (255 - K) / 255);
            conv[i * 3 + 2] = (unsigned char)((255 - Y) * (255 - K) / 255);
        }
        free(raw);
        out->data = conv; out->width = (int)W; out->height = (int)H; out->channels = 3;
        return 0;
    }
    int outch;
    if (Photo == 2) outch = (Spp >= 4) ? 4 : 3;
    else if (Photo == 0 || Photo == 1) outch = (Spp == 1) ? 1 : 0;
    else outch = 0;                                        /* altro: non supportato */
    if (outch == 0) { free(raw); return -1; }
    if (outch == (int)Spp) { out->data = raw; }
    else {                                                 /* RGB→RGBA con alpha opaco */
        unsigned char* conv = (unsigned char*)malloc((size_t)W * (size_t)H * (size_t)outch);
        if (!conv) { free(raw); return -1; }
        for (long i = 0; i < (long)W * (long)H; i++) {
            conv[i * outch] = raw[i * Spp];
            if (outch >= 3) { conv[i * outch + 1] = raw[i * Spp + 1]; conv[i * outch + 2] = raw[i * Spp + 2]; }
            if (outch == 4) conv[i * outch + 3] = 255;
        }
        free(raw); out->data = conv;
    }
    out->width = (int)W; out->height = (int)H; out->channels = outch;
    return 0;
}

int mm_tiff_read(const char* path, mm_image* out) { return tiff_read_impl(path, 0, out); }
int mm_tiff_read_page(const char* path, int page, mm_image* out) { return page < 0 ? -1 : tiff_read_impl(path, page, out); }

int mm_tiff_pages(const char* path) {
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    unsigned char h[8];
    if (fread(h, 1, 8, f) != 8) { fclose(f); return -1; }
    int le;
    if (h[0] == 'I' && h[1] == 'I') le = 1; else if (h[0] == 'M' && h[1] == 'M') le = 0; else { fclose(f); return -1; }
    if (rd16(h + 2, le) != 42) { fclose(f); return -1; }
    unsigned ifd = rd32(h + 4, le);
    int count = 0;
    while (ifd != 0) {
        unsigned char c2[2], nb[4];
        count++;
        if (fseek(f, (long)ifd, SEEK_SET) != 0) break;
        if (fread(c2, 1, 2, f) != 2) break;
        unsigned n = rd16(c2, le);
        if (fseek(f, (long)ifd + 2 + (long)n * 12, SEEK_SET) != 0) break;
        if (fread(nb, 1, 4, f) != 4) break;
        ifd = rd32(nb, le);
        if (count > 1000000) break;
    }
    fclose(f);
    return count;
}
