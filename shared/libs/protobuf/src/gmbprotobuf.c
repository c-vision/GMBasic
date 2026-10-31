/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbprotobuf.c — shim FFI del pacchetto gmbsuite `protobuf`: codec wire-format Protocol Buffers
 * in C PURO (NESSUNA libreria esterna, nessun compilatore .proto) -> self-contained. Build dal
 * sorgente (build.sh).
 *
 * Copre i wire type 0 (varint: int/bool/enum), 1 (64-bit), 2 (length-delimited: string/bytes),
 * 5 (32-bit). I byte serializzati sono esposti come STRINGA ESADECIMALE (evita il troncamento a
 * NUL delle stringhe GMBasic). Interfaccia BASIC: namespace PROTOBUF, firma OMOGENEA (LONG). */
#include "gmbprotobuf.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define PB_MAX 64
#define PB_NAME 256

typedef struct { int used; unsigned char* buf; int len, cap; char hex[PB_NAME * 4]; } pb_w;
typedef struct { int used; unsigned char* buf; int len; char tmp[16384]; } pb_r;
static pb_w g_w[PB_MAX];
static pb_r g_r[PB_MAX];

int gmbprotobuf_available(void) { return 1; }

static int pb_varint(unsigned char* out, uint64_t v)
{
    int n = 0;
    do { unsigned char b = (unsigned char)(v & 0x7f); v >>= 7; if (v) b |= 0x80; out[n++] = b; } while (v);
    return n;
}

static int w_grow(pb_w* w, int need)
{
    if (w->len + need <= w->cap) return 0;
    int nc = w->cap ? w->cap : 256;
    while (nc < w->len + need) nc *= 2;
    unsigned char* nb = (unsigned char*)realloc(w->buf, (size_t)nc);
    if (!nb) return -1;
    w->buf = nb; w->cap = nc;
    return 0;
}
static int w_put(pb_w* w, const unsigned char* p, int n)
{
    if (w_grow(w, n) != 0) return -1;
    memcpy(w->buf + w->len, p, (size_t)n); w->len += n;
    return 0;
}
static int w_varint(pb_w* w, uint64_t v)
{
    unsigned char b[10]; int n = pb_varint(b, v); return w_put(w, b, n);
}

int gmbprotobuf_new(void)
{
    for (int i = 0; i < PB_MAX; i++) if (!g_w[i].used) {
        g_w[i].used = 1; g_w[i].buf = NULL; g_w[i].len = 0; g_w[i].cap = 0; g_w[i].hex[0] = 0;
        return i + 1;
    }
    return -1;
}
static pb_w* wget(long h) { return (h >= 1 && h <= PB_MAX && g_w[h - 1].used) ? &g_w[h - 1] : NULL; }

int gmbprotobuf_addint(long h, long field, long value)
{
    pb_w* w = wget(h); if (!w || field <= 0) return -1;
    if (w_varint(w, ((uint64_t)field << 3) | 0) != 0) return -1;
    return w_varint(w, (uint64_t)value);
}

int gmbprotobuf_addstring(long h, long field, const char* s)
{
    pb_w* w = wget(h); if (!w || field <= 0) return -1;
    const char* p = s ? s : "";
    int n = (int)strlen(p);
    if (w_varint(w, ((uint64_t)field << 3) | 2) != 0) return -1;
    if (w_varint(w, (uint64_t)n) != 0) return -1;
    return w_put(w, (const unsigned char*)p, n);
}

const char* gmbprotobuf_serialize(long h)
{
    pb_w* w = wget(h); if (!w) return "";
    static const char* HX = "0123456789abcdef";
    for (int i = 0; i < w->len; i++) {
        w->hex[i * 2]     = HX[(w->buf[i] >> 4) & 0xf];
        w->hex[i * 2 + 1] = HX[w->buf[i] & 0xf];
    }
    w->hex[w->len * 2] = 0;
    return w->hex;
}

static int hexval(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; }

int gmbprotobuf_parse(const char* hex)
{
    if (!hex) hex = "";
    int n = (int)strlen(hex);
    int slot = -1;
    for (int i = 0; i < PB_MAX; i++) if (!g_r[i].used) { slot = i; break; }
    if (slot < 0) return -1;
    unsigned char* buf = (unsigned char*)malloc((size_t)(n / 2 + 1));
    if (!buf) return -1;
    int bl = 0;
    for (int i = 0; i + 1 < n; i += 2) {
        int hi = hexval(hex[i]), lo = hexval(hex[i + 1]);
        if (hi < 0 || lo < 0) { free(buf); return -1; }
        buf[bl++] = (unsigned char)((hi << 4) | lo);
    }
    g_r[slot].used = 1; g_r[slot].buf = buf; g_r[slot].len = bl; g_r[slot].tmp[0] = 0;
    return slot + 1;
}
static pb_r* rget(long h) { return (h >= 1 && h <= PB_MAX && g_r[h - 1].used) ? &g_r[h - 1] : NULL; }

/* Scorre i campi; ritorna 1 trovato e riempie wt/pos/vval, 0 non trovato, -1 errore.
 * pos = offset dei byte del valore (per wire type 2); vval = valore varint (wire type 0). */
static int scan(pb_r* r, long field, int* wt, int* pos, uint64_t* vval)
{
    int i = 0;
    while (i < r->len) {
        uint64_t tag = 0; int sh = 0;
        while (i < r->len) { unsigned char b = r->buf[i++]; tag |= (uint64_t)(b & 0x7f) << sh; sh += 7; if (!(b & 0x80)) break; if (sh > 63) return -1; }
        int fn = (int)(tag >> 3); int w = (int)(tag & 7);
        if (w == 0) {
            uint64_t v = 0; sh = 0;
            while (i < r->len) { unsigned char b = r->buf[i++]; v |= (uint64_t)(b & 0x7f) << sh; sh += 7; if (!(b & 0x80)) break; if (sh > 63) return -1; }
            if (fn == field) { *wt = 0; *vval = v; *pos = i; return 1; }
        } else if (w == 2) {
            uint64_t ln = 0; sh = 0;
            while (i < r->len) { unsigned char b = r->buf[i++]; ln |= (uint64_t)(b & 0x7f) << sh; sh += 7; if (!(b & 0x80)) break; if (sh > 63) return -1; }
            if (i + (int)ln > r->len) return -1;
            if (fn == field) { *wt = 2; *pos = i; *vval = ln; return 1; }
            i += (int)ln;
        } else if (w == 5) {
            if (i + 4 > r->len) return -1;
            if (fn == field) { *wt = 5; *pos = i; return 1; }
            i += 4;
        } else if (w == 1) {
            if (i + 8 > r->len) return -1;
            if (fn == field) { *wt = 1; *pos = i; return 1; }
            i += 8;
        } else return -1;
    }
    return 0;
}

int gmbprotobuf_hasfield(long h, long field)
{
    pb_r* r = rget(h); if (!r) return 0;
    int wt, pos; uint64_t v;
    return scan(r, field, &wt, &pos, &v) == 1 ? 1 : 0;
}

long gmbprotobuf_getint(long h, long field)
{
    pb_r* r = rget(h); if (!r) return 0;
    int wt, pos; uint64_t v = 0;
    int f = scan(r, field, &wt, &pos, &v);
    if (f != 1) return 0;
    if (wt == 0) return (long)v;
    if (wt == 5) { uint32_t x = 0; memcpy(&x, r->buf + pos, 4); return (long)x; }
    if (wt == 1) { uint64_t x = 0; memcpy(&x, r->buf + pos, 8); return (long)x; }
    return 0;
}

const char* gmbprotobuf_getstring(long h, long field)
{
    pb_r* r = rget(h); if (!r) return "";
    int wt, pos; uint64_t ln = 0;
    int f = scan(r, field, &wt, &pos, &ln);
    if (f != 1 || wt != 2) return "";
    if (ln > sizeof(r->tmp) - 1) ln = sizeof(r->tmp) - 1;
    memcpy(r->tmp, r->buf + pos, (size_t)ln);
    r->tmp[ln] = 0;
    return r->tmp;
}

int gmbprotobuf_free(long h)
{
    if (h >= 1 && h <= PB_MAX && g_w[h - 1].used) { free(g_w[h - 1].buf); memset(&g_w[h - 1], 0, sizeof(pb_w)); return 0; }
    if (h >= 1 && h <= PB_MAX && g_r[h - 1].used) { free(g_r[h - 1].buf); memset(&g_r[h - 1], 0, sizeof(pb_r)); return 0; }
    return -1;
}
