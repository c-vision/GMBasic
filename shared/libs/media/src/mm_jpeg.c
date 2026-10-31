/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_jpeg.c — JPEG self-contained: decode baseline (SOF0/SOF1) e progressivo (SOF2: più scan,
 * selezione spettrale Ss/Se, approssimazione successiva Ah/Al, restart) + IDCT + upsample +
 * YCbCr->RGB; encode (4:2:0/4:4:4 + FDCT + quant + Huffman standard). */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define GMB_NEON 1
#endif

/* ZZ[k] = posizione NATURALE (raster 8x8) della posizione ZIGZAG k */
static const int ZZ[64] = {
    0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63 };
static const unsigned char QLUM[64] = {
    16,11,10,16,24,40,51,61,12,12,14,19,26,58,60,55,14,13,16,24,40,57,69,56,14,17,22,29,51,87,80,62,
    18,22,37,56,68,109,103,77,24,35,55,64,81,104,113,92,49,64,78,87,103,121,120,101,72,92,95,98,112,100,103,99 };
static const unsigned char QCHR[64] = {
    17,18,24,47,99,99,99,99,18,21,26,66,99,99,99,99,24,26,56,99,99,99,99,99,47,66,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99 };

static double COS[8][8];
static void init_cos(void) { for (int u = 0; u < 8; u++) for (int x = 0; x < 8; x++) COS[u][x] = cos((2 * x + 1) * u * M_PI / 16.0); }
static int clampi(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
/* YCbCr->RGB (Q14, NEON 8 pixel/iter). */
static void rgb_from_ycc(const unsigned char* y, const unsigned char* cb, const unsigned char* cr, unsigned char* out, int n) {
    int x = 0;
#ifdef GMB_NEON
    {
        const int16x8_t zz = vdupq_n_s16(128);
        const int32x4_t bias = vdupq_n_s32(8192);
        for (; x + 8 <= n; x += 8) {
            int16x8_t Y = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(y + x)));
            int16x8_t Cb = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vld1_u8(cb + x))), zz);
            int16x8_t Cr = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vld1_u8(cr + x))), zz);
            int16x4_t Yl = vget_low_s16(Y), Yh = vget_high_s16(Y);
            int16x4_t bl = vget_low_s16(Cb), bh = vget_high_s16(Cb);
            int16x4_t rl = vget_low_s16(Cr), rh = vget_high_s16(Cr);
            int16x4_t R0 = vadd_s16(Yl, vqrshrn_n_s32(vmlal_n_s16(bias, rl, 22970), 14));
            int16x4_t R1 = vadd_s16(Yh, vqrshrn_n_s32(vmlal_n_s16(bias, rh, 22970), 14));
            int16x4_t G0 = vsub_s16(Yl, vqrshrn_n_s32(vmlal_n_s16(vmlal_n_s16(bias, bl, 5638), rl, 11700), 14));
            int16x4_t G1 = vsub_s16(Yh, vqrshrn_n_s32(vmlal_n_s16(vmlal_n_s16(bias, bh, 5638), rh, 11700), 14));
            int16x4_t B0 = vadd_s16(Yl, vqrshrn_n_s32(vmlal_n_s16(bias, bl, 29031), 14));
            int16x4_t B1 = vadd_s16(Yh, vqrshrn_n_s32(vmlal_n_s16(bias, bh, 29031), 14));
            uint8x8x3_t px = { vqmovun_s16(vcombine_s16(R0, R1)), vqmovun_s16(vcombine_s16(G0, G1)), vqmovun_s16(vcombine_s16(B0, B1)) };
            vst3_u8(out + (size_t)x * 3, px);
        }
    }
#endif
    for (; x < n; x++) {
        int Y = y[x], Cb = cb[x] - 128, Cr = cr[x] - 128;
        unsigned char* o = out + (size_t)x * 3;
        o[0] = (unsigned char)clampi(Y + ((22970 * Cr + 8192) >> 14));
        o[1] = (unsigned char)clampi(Y - ((5638 * Cb + 11700 * Cr + 8192) >> 14));
        o[2] = (unsigned char)clampi(Y + ((29031 * Cb + 8192) >> 14));
    }
}

static void __attribute__((unused)) idct(const int in[64], unsigned char* out, int stride) {
    double tmp[64];
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        double s = 0;
        for (int v = 0; v < 8; v++) { double cv = v ? 1.0 : 0.707106781; for (int u = 0; u < 8; u++) { double cu = u ? 1.0 : 0.707106781; s += cu * cv * in[v * 8 + u] * COS[u][x] * COS[v][y]; } }
        tmp[y * 8 + x] = s * 0.25 + 128.0;
    }
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) out[y * stride + x] = (unsigned char)clampi((int)(tmp[y * 8 + x] + 0.5));
}
/* ── IDCT intero "islow" (Loeffler) su int32x4: 2 gruppi di 4 colonne per passata + trasposizione.
 *    Meno operazioni della forma matriciale (butterfly) e SIMD a 4 lane. Costanti Q13. ── */
#ifdef GMB_NEON
#define ISLOW_CB 13
#define ISLOW_PB 2
static void islow1d(int32x4_t v[8]) {
    int32x4_t z2 = v[2], z3 = v[6];
    int32x4_t z1 = vmulq_n_s32(vaddq_s32(z2, z3), 4433);
    int32x4_t t2 = vaddq_s32(z1, vmulq_n_s32(z3, -15137));
    int32x4_t t3 = vaddq_s32(z1, vmulq_n_s32(z2, 6270));
    int32x4_t t0 = vshlq_n_s32(vaddq_s32(v[0], v[4]), ISLOW_CB);
    int32x4_t t1 = vshlq_n_s32(vsubq_s32(v[0], v[4]), ISLOW_CB);
    int32x4_t tmp10 = vaddq_s32(t0, t3), tmp13 = vsubq_s32(t0, t3);
    int32x4_t tmp11 = vaddq_s32(t1, t2), tmp12 = vsubq_s32(t1, t2);
    int32x4_t o0 = v[7], o1 = v[5], o2 = v[3], o3 = v[1];
    z1 = vaddq_s32(o0, o3); z2 = vaddq_s32(o1, o2); z3 = vaddq_s32(o0, o2); int32x4_t z4 = vaddq_s32(o1, o3);
    int32x4_t z5 = vmulq_n_s32(vaddq_s32(z3, z4), 9633);
    o0 = vmulq_n_s32(o0, 2446); o1 = vmulq_n_s32(o1, 16819); o2 = vmulq_n_s32(o2, 25172); o3 = vmulq_n_s32(o3, 12299);
    z1 = vmulq_n_s32(z1, -7373); z2 = vmulq_n_s32(z2, -20995); z3 = vmulq_n_s32(z3, -16069); z4 = vmulq_n_s32(z4, -3196);
    z3 = vaddq_s32(z3, z5); z4 = vaddq_s32(z4, z5);
    o0 = vaddq_s32(o0, vaddq_s32(z1, z3)); o1 = vaddq_s32(o1, vaddq_s32(z2, z4));
    o2 = vaddq_s32(o2, vaddq_s32(z2, z3)); o3 = vaddq_s32(o3, vaddq_s32(z1, z4));
    v[0] = vaddq_s32(tmp10, o3); v[7] = vsubq_s32(tmp10, o3);
    v[1] = vaddq_s32(tmp11, o2); v[6] = vsubq_s32(tmp11, o2);
    v[2] = vaddq_s32(tmp12, o1); v[5] = vsubq_s32(tmp12, o1);
    v[3] = vaddq_s32(tmp13, o0); v[4] = vsubq_s32(tmp13, o0);
}
static void islow_pass(int32_t* p, int db) {
    int32x4_t rnd = vdupq_n_s32(1 << (db - 1)), sh = vdupq_n_s32(-db);
    for (int g = 0; g < 8; g += 4) {
        int32x4_t v[8];
        for (int k = 0; k < 8; k++) v[k] = vld1q_s32(&p[k * 8 + g]);
        islow1d(v);
        for (int k = 0; k < 8; k++) vst1q_s32(&p[k * 8 + g], vshlq_s32(vaddq_s32(v[k], rnd), sh));
    }
}
static void idct_islow_neon(const int in[64], unsigned char* out, int stride) {
    int32_t p[64], t[64];
    for (int i = 0; i < 64; i++) p[i] = in[i];
    islow_pass(p, ISLOW_CB - ISLOW_PB);
    for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) t[j * 8 + i] = p[i * 8 + j];
    memcpy(p, t, sizeof p);
    islow_pass(p, ISLOW_CB + ISLOW_PB + 3);
    for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) t[j * 8 + i] = p[i * 8 + j];
    for (int r = 0; r < 8; r++) { unsigned char row[8]; for (int c = 0; c < 8; c++) { int s = t[r * 8 + c] + 128; row[c] = (unsigned char)(s < 0 ? 0 : (s > 255 ? 255 : s)); } memcpy(out + (size_t)r * stride, row, 8); }
}
/* Dequant SIMD: coeff[n] = blk[n] * q[n] (64 int32, data-parallel, nessuna dipendenza). */
static void dequant_neon(const int* blk, const unsigned short* q, int* out) {
    for (int n = 0; n < 64; n += 4)
        vst1q_s32(out + n, vmulq_s32(vld1q_s32(blk + n), vreinterpretq_s32_u32(vmovl_u16(vld1_u16(q + n)))));
}
#endif
static void fdct(const unsigned char* in, int stride, double out[64]) {
    for (int v = 0; v < 8; v++) for (int u = 0; u < 8; u++) {
        double s = 0;
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) s += (in[y * stride + x] - 128) * COS[u][x] * COS[v][y];
        double cu = u ? 1.0 : 0.707106781, cv = v ? 1.0 : 0.707106781;
        out[v * 8 + u] = 0.25 * cu * cv * s;
    }
}

/* ── Huffman ── */
typedef struct { unsigned char bits[17]; unsigned char vals[256]; unsigned char lnb[512]; unsigned short lsym[512]; } jhuff;
static void huff_build_look(jhuff* h) {
    memset(h->lnb, 0, sizeof h->lnb);
    int k = 0, code = 0;
    for (int l = 1; l <= 16; l++) {
        for (int i = 0; i < h->bits[l]; i++) {
            int sym = h->vals[k++];
            if (l <= 9) { int sh = 9 - l, base = code << sh, rp = 1 << sh; for (int j = 0; j < rp; j++) { h->lnb[base | j] = (unsigned char)l; h->lsym[base | j] = (unsigned short)sym; } }
            code++;
        }
        code <<= 1;
    }
}
typedef struct { const unsigned char* D; size_t n, pos; unsigned long long bb; int bc; } jbr;
static void jbr_init(jbr* b, const unsigned char* D, size_t n, size_t pos) { b->D = D; b->n = n; b->pos = pos; b->bb = 0; b->bc = 0; }
static void jbr_fill(jbr* b) {
    if (b->bc <= 31 && b->pos + 4 <= b->n) {
        unsigned w; memcpy(&w, b->D + b->pos, 4); unsigned nw = ~w;
        if (!((nw - 0x01010101u) & w & 0x80808080u)) { b->bb = (b->bb << 32) | (unsigned long long)__builtin_bswap32(w); b->bc += 32; b->pos += 4; }
    }
    while (b->bc <= 55) {
        unsigned long long c = 0;
        if (b->pos < b->n) { c = b->D[b->pos++]; if (c == 0xFF && b->pos < b->n && b->D[b->pos] == 0x00) b->pos++; }
        b->bb = (b->bb << 8) | c; b->bc += 8;
    }
}
static int jbits(jbr* b, int n, int* bits) {
    if (n == 0) return 0;
    if (b->bc < n) jbr_fill(b);
    int v = (int)((b->bb >> (b->bc - n)) & ((1ull << n) - 1));
    b->bc -= n; if (bits) *bits += n;
    return v;
}
static int jh_decode(const jhuff* h, jbr* b) {
    if (b->bc < 9) jbr_fill(b);
    unsigned idx = (b->bb >> (b->bc - 9)) & 0x1FF;
    int l = h->lnb[idx];
    if (l) { b->bc -= l; return h->lsym[idx]; }
    int code = 0, k = 0, first = 0;
    for (int len = 1; len <= 16; len++) {
        int bit; if (b->bc == 0) jbr_fill(b); bit = (b->bb >> --b->bc) & 1;
        code = (code << 1) | bit;
        if ((unsigned)(code - first) < h->bits[len]) return h->vals[k + (code - first)];
        k += h->bits[len]; first = (first + h->bits[len]) << 1;
    }
    return -1;
}
static int extend_val(int v, int size) { return v < (1 << (size - 1)) ? v - (1 << size) + 1 : v; }

/* ── scan di entropia: baseline (SOF0/1) = un solo scan a spettro pieno; progressivo (SOF2)
 *    = più scan con selezione spettrale (Ss/Se) e approssimazione successiva (Ah/Al). ── */
typedef struct { int cid, hs, vs, tq, bw, bh, sx, sy, pred; int* coef; } jcomp;

/* decodifica un blocco (64 coeff quantizzati, ordine naturale) per lo scan corrente. */
static int jblock(const jhuff* hdc, const jhuff* hac, jbr* br, int* pred, int* coef,
                  int Ss, int Se, int Ah, int Al, int* eobrun) {
    if (Ss == 0) {
        if (Ah == 0) {
            int t = jh_decode(hdc, br); if (t < 0) return -1;
            int diff = 0; if (t) { int v = jbits(br, t, NULL); if (v < 0) return -1; diff = extend_val(v, t); }
            *pred += diff; coef[0] = *pred << Al;
        } else {
            int b = jbits(br, 1, NULL); if (b < 0) return -1;
            if (b) coef[0] |= (1 << Al);
        }
    }
    if (Se >= 1) {
        if (Ah == 0) {                                     /* AC, prima scansione */
            if (*eobrun > 0) { (*eobrun)--; return 0; }
            int k = (Ss == 0) ? 1 : Ss;
            while (k <= Se) {
                int rs = jh_decode(hac, br); if (rs < 0) return -1;
                int r = rs >> 4, s = rs & 15;
                if (s) {
                    k += r; if (k > Se) return -1;
                    int v = jbits(br, s, NULL); if (v < 0) return -1;
                    coef[ZZ[k]] = extend_val(v, s) << Al; k++;
                } else {
                    if (r != 15) { int e; *eobrun = (1 << r); if (r) { e = jbits(br, r, NULL); if (e < 0) return -1; *eobrun += e; } (*eobrun)--; break; }
                    k += 16;
                }
            }
        } else {                                           /* AC, raffinamento */
            int k = Ss, p1 = 1 << Al, m1 = -p1;
            if (*eobrun == 0) {
                while (k <= Se) {                          /* k++ sempre a fine iterazione (semantica del for di libjpeg) */
                    int rs = jh_decode(hac, br); if (rs < 0) return -1;
                    int r = rs >> 4, s = rs & 15, newval = 0;
                    if (s) {
                        if (s != 1) return -1;
                        int b = jbits(br, 1, NULL); if (b < 0) return -1;
                        newval = b ? p1 : m1;
                    } else if (r != 15) {
                        int e; *eobrun = (1 << r); if (r) { e = jbits(br, r, NULL); if (e < 0) return -1; *eobrun += e; }
                        break;
                    }
                    do {
                        int pz = ZZ[k];
                        if (coef[pz] != 0) {
                            int b = jbits(br, 1, NULL); if (b < 0) return -1;
                            if (b && (coef[pz] & p1) == 0) coef[pz] += (coef[pz] >= 0) ? p1 : m1;
                        } else if (--r < 0) break;
                        k++;
                    } while (k <= Se);
                    if (s && k <= Se) coef[ZZ[k]] = newval;
                    k++;
                }
            }
            if (*eobrun > 0) {
                for (; k <= Se; k++) {
                    int pz = ZZ[k];
                    if (coef[pz] != 0) {
                        int b = jbits(br, 1, NULL); if (b < 0) return -1;
                        if (b && (coef[pz] & p1) == 0) coef[pz] += (coef[pz] >= 0) ? p1 : m1;
                    }
                }
                (*eobrun)--;
            }
        }
    }
    return 0;
}

/* trova il prossimo marker reale (salta stuffing FF00 e RSTn). */
static size_t scan_end(const unsigned char* D, size_t sz, size_t from) {
    size_t q2 = from;
    while (q2 + 1 < sz) {
        if (D[q2] == 0xFF) {
            int b = D[q2 + 1];
            if (b == 0x00 || b == 0xFF || (b >= 0xD0 && b <= 0xD7)) { q2++; continue; }
            return q2;
        }
        q2++;
    }
    return sz;
}

static int decode_scan(const unsigned char* D, size_t sz, size_t ent_off, int ri, int ns, const int* sci,
                       const int* sdc, const int* sac, jhuff* hdc, jhuff* hac, int Ss, int Se, int Ah, int Al,
                       jcomp* comps, int W, int H) {
    const unsigned char* ent = D + ent_off; size_t entn = sz - ent_off;
    jbr b; jbr_init(&b, ent, entn, 0); size_t jrst = 0; int eobrun = 0;
    for (int i = 0; i < ns; i++) comps[sci[i]].pred = 0;
    int inter = (ns > 1), hms = 1, vms = 1;
    for (int i = 0; i < ns; i++) { if (comps[sci[i]].hs > hms) hms = comps[sci[i]].hs; if (comps[sci[i]].vs > vms) vms = comps[sci[i]].vs; }
    int mcu_x = inter ? (W + 8 * hms - 1) / (8 * hms) : comps[sci[0]].bw;
    int mcu_y = inter ? (H + 8 * vms - 1) / (8 * vms) : comps[sci[0]].bh;
    long mcu = 0;
    for (int my = 0; my < mcu_y; my++) for (int mx = 0; mx < mcu_x; mx++, mcu++) {
        if (ri && mcu > 0 && (mcu % ri) == 0) {
            size_t q = jrst;
            while (q + 1 < entn && !(ent[q] == 0xFF && ent[q + 1] >= 0xD0 && ent[q + 1] <= 0xD7)) q++;
            jrst = (q + 1 < entn) ? q + 2 : q;
            jbr_init(&b, ent, entn, jrst);
            for (int i = 0; i < ns; i++) comps[sci[i]].pred = 0;
            eobrun = 0;
        }
        if (inter) {
            for (int i = 0; i < ns; i++) {
                jcomp* cp = &comps[sci[i]];
                for (int by = 0; by < cp->vs; by++) for (int bx = 0; bx < cp->hs; bx++) {
                    long gx = (long)mx * cp->hs + bx, gy = (long)my * cp->vs + by;
                    int* blk = &cp->coef[((size_t)gy * cp->sx + gx) * 64];
                    if (jblock(&hdc[sdc[i]], &hac[sac[i]], &b, &cp->pred, blk, Ss, Se, Ah, Al, &eobrun)) return -1;
                }
            }
        } else {
            jcomp* cp = &comps[sci[0]];
            int* blk = &cp->coef[((size_t)my * cp->sx + mx) * 64];
            if (jblock(&hdc[sdc[0]], &hac[sac[0]], &b, &cp->pred, blk, Ss, Se, Ah, Al, &eobrun)) return -1;
        }
    }
    return (int)(scan_end(D, sz, ent_off + b.pos - (size_t)(b.bc / 8)) - ent_off);   /* consumo relativo usato dal parser */
}

/* Upsampling "fancy" (filtro triangolo) — algoritmo del riferimento. Produce W×H da cw×ch. */
static void ups_fancy_h2v1(const unsigned char* in, int cw, int ch, unsigned char* out, int W, int H) {
    for (int r = 0; r < ch && r < H; r++) {
        const unsigned char* r0 = in + (size_t)r * cw; unsigned char* o = out + (size_t)r * W;
        if (W > 0) o[0] = r0[0];
        if (W > 1) o[1] = (unsigned char)((r0[0] * 3 + (cw > 1 ? r0[1] : r0[0]) + 2) >> 2);
        int k = 1;
#ifdef GMB_NEON
        for (; k + 8 <= cw - 1 && 2 * k + 16 <= W; k += 8) {
            uint8x8_t cur = vld1_u8(r0 + k), prev = vld1_u8(r0 + k - 1), next = vld1_u8(r0 + k + 1);
            uint16x8_t c3 = vmulq_n_u16(vmovl_u8(cur), 3);
            uint16x8_t ev = vshrq_n_u16(vaddq_u16(vaddq_u16(c3, vmovl_u8(prev)), vdupq_n_u16(1)), 2);
            uint16x8_t od = vshrq_n_u16(vaddq_u16(vaddq_u16(c3, vmovl_u8(next)), vdupq_n_u16(2)), 2);
            uint8x8x2_t z = vzip_u8(vmovn_u16(ev), vmovn_u16(od));
            vst1_u8(o + 2 * k, z.val[0]); vst1_u8(o + 2 * k + 8, z.val[1]);
        }
#endif
        for (; k < cw - 1; k++) {
            if (2 * k < W) o[2 * k] = (unsigned char)((r0[k] * 3 + r0[k - 1] + 1) >> 2);
            if (2 * k + 1 < W) o[2 * k + 1] = (unsigned char)((r0[k] * 3 + r0[k + 1] + 2) >> 2);
        }
        if (cw >= 2) {
            if (2 * (cw - 1) < W) o[2 * (cw - 1)] = (unsigned char)((r0[cw - 1] * 3 + r0[cw - 2] + 1) >> 2);
            if (2 * (cw - 1) + 1 < W) o[2 * (cw - 1) + 1] = r0[cw - 1];
        }
    }
}
static void ups_fancy_h2v2(const unsigned char* in, int cw, int ch, unsigned char* out, int W, int H) {
    for (int r = 0; r < ch; r++) {
        for (int v = 0; v < 2; v++) {
            int oy = 2 * r + v; if (oy >= H) break;
            int r1 = (v == 0) ? (r > 0 ? r - 1 : 0) : (r + 1 < ch ? r + 1 : r);
            const unsigned char* a = in + (size_t)r * cw; const unsigned char* b = in + (size_t)r1 * cw;
            unsigned char* o = out + (size_t)oy * W;
            int thisc = a[0] * 3 + b[0], nextc = (cw > 1) ? (a[1] * 3 + b[1]) : thisc;
            if (W > 0) o[0] = (unsigned char)((thisc * 4 + 8) >> 4);
            if (W > 1) o[1] = (unsigned char)((thisc * 3 + nextc + 7) >> 4);
            int lastc = thisc; thisc = nextc;
            int k = 1;
#ifdef GMB_NEON
            for (; k + 8 <= cw - 1 && 2 * k + 16 <= W; k += 8) {
                uint8x8_t ac = vld1_u8(a + k), bc = vld1_u8(b + k);
                uint8x8_t ap = vld1_u8(a + k - 1), bp = vld1_u8(b + k - 1);
                uint8x8_t an = vld1_u8(a + k + 1), bn = vld1_u8(b + k + 1);
                uint16x8_t cur = vaddq_u16(vmulq_n_u16(vmovl_u8(ac), 3), vmovl_u8(bc));
                uint16x8_t prv = vaddq_u16(vmulq_n_u16(vmovl_u8(ap), 3), vmovl_u8(bp));
                uint16x8_t nxt = vaddq_u16(vmulq_n_u16(vmovl_u8(an), 3), vmovl_u8(bn));
                uint16x8_t c3 = vmulq_n_u16(cur, 3);
                uint16x8_t ev = vshrq_n_u16(vaddq_u16(vaddq_u16(c3, prv), vdupq_n_u16(8)), 4);
                uint16x8_t od = vshrq_n_u16(vaddq_u16(vaddq_u16(c3, nxt), vdupq_n_u16(7)), 4);
                uint8x8x2_t z = vzip_u8(vmovn_u16(ev), vmovn_u16(od));
                vst1_u8(o + 2 * k, z.val[0]); vst1_u8(o + 2 * k + 8, z.val[1]);
            }
#endif
            lastc = a[k - 1] * 3 + b[k - 1]; thisc = a[k] * 3 + b[k];
            for (; k < cw - 1; k++) {
                nextc = a[k + 1] * 3 + b[k + 1];
                if (2 * k < W) o[2 * k] = (unsigned char)((thisc * 3 + lastc + 8) >> 4);
                if (2 * k + 1 < W) o[2 * k + 1] = (unsigned char)((thisc * 3 + nextc + 7) >> 4);
                lastc = thisc; thisc = nextc;
            }
            if (cw >= 2) {
                if (2 * (cw - 1) < W) o[2 * (cw - 1)] = (unsigned char)((thisc * 3 + lastc + 8) >> 4);
                if (2 * (cw - 1) + 1 < W) o[2 * (cw - 1) + 1] = (unsigned char)((thisc * 4 + 7) >> 4);
            }
        }
    }
}
int mm_jpeg_read(const char* path, mm_image* out) {
    if (!path || !out) return -1;
    init_cos();
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* D = (unsigned char*)malloc((size_t)sz);
    if (!D || fread(D, 1, (size_t)sz, f) != (size_t)sz) { free(D); fclose(f); return -1; }
    fclose(f);
    if (sz < 4 || D[0] != 0xFF || D[1] != 0xD8) { free(D); return -1; }
    unsigned short q[4][64]; memset(q, 0, sizeof q);
    jhuff hdc[4], hac[4]; memset(hdc, 0, sizeof hdc); memset(hac, 0, sizeof hac);
    jcomp comps[4]; memset(comps, 0, sizeof comps);
    int W = 0, H = 0, ncomp = 0, hmax = 1, vmax = 1, ri = 0, have_frame = 0, have_scan = 0;
    size_t p = 2;
    while (p + 4 <= (size_t)sz) {
        if (D[p] != 0xFF) { p++; continue; }
        int m = D[p + 1]; if (m == 0xFF) { p++; continue; } p += 2;
        if (m == 0xD9) break;
        if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01) continue;
        if (p + 2 > (size_t)sz) break;
        int len = (D[p] << 8) | D[p + 1];
        const unsigned char* seg = D + p + 2; int sl = len - 2;
        if (m == 0xDB) { int o = 0; while (o < sl) { int pq = seg[o] >> 4, tq = seg[o] & 15; o++; if (tq < 4) { for (int i = 0; i < 64; i++) { q[tq][ZZ[i]] = pq ? (unsigned short)((seg[o] << 8) | seg[o + 1]) : seg[o]; o += pq ? 2 : 1; } } else o += 64; } }
        else if (m == 0xC0 || m == 0xC1 || m == 0xC2) {
            H = (seg[1] << 8) | seg[2]; W = (seg[3] << 8) | seg[4]; ncomp = seg[5];
            hmax = vmax = 1;
            for (int i = 0; i < ncomp; i++) { int o = 6 + i * 3; comps[i].cid = seg[o]; comps[i].hs = seg[o + 1] >> 4; comps[i].vs = seg[o + 1] & 15; comps[i].tq = seg[o + 2]; if (comps[i].hs > hmax) hmax = comps[i].hs; if (comps[i].vs > vmax) vmax = comps[i].vs; }
            int mcux = (W + 8 * hmax - 1) / (8 * hmax), mcuy = (H + 8 * vmax - 1) / (8 * vmax);
            for (int i = 0; i < ncomp; i++) {
                int cwp = (W * comps[i].hs + hmax - 1) / hmax, chp = (H * comps[i].vs + vmax - 1) / vmax;
                comps[i].bw = (cwp + 7) / 8; comps[i].bh = (chp + 7) / 8;
                comps[i].sx = mcux * comps[i].hs; comps[i].sy = mcuy * comps[i].vs;
                comps[i].coef = (int*)calloc((size_t)comps[i].sx * comps[i].sy * 64, sizeof(int));
                if (!comps[i].coef) { for (int j = 0; j < i; j++) free(comps[j].coef); free(D); return -1; }
            }
            have_frame = 1;
        }
        else if (m == 0xC4) { int o = 0; while (o < sl) { int tc = seg[o] >> 4, th = seg[o] & 15; o++; if (th < 4) { int tot = 0; jhuff* hh = tc ? &hac[th] : &hdc[th]; for (int i = 1; i <= 16; i++) { hh->bits[i] = seg[o + i - 1]; tot += hh->bits[i]; } hh->bits[0] = 0; o += 16; memcpy(hh->vals, seg + o, (size_t)tot); o += tot; huff_build_look(hh); } else o += 1; } }
        else if (m == 0xDD) { ri = (seg[0] << 8) | seg[1]; }
        else if (m == 0xDA) {
            if (!have_frame) { for (int i = 0; i < ncomp; i++) free(comps[i].coef); free(D); return -1; }
            int ns = seg[0], sci[4], sdc[4], sac[4];
            for (int i = 0; i < ns; i++) { int cs = seg[1 + i * 2], tt = seg[2 + i * 2], ci = -1; for (int c = 0; c < ncomp; c++) if (comps[c].cid == cs) ci = c; if (ci < 0) { for (int k = 0; k < ncomp; k++) free(comps[k].coef); free(D); return -1; } sci[i] = ci; sdc[i] = tt >> 4; sac[i] = tt & 15; }
            int Ss = seg[1 + ns * 2], Se = seg[2 + ns * 2], AhAl = seg[3 + ns * 2];
            size_t ent_off = (size_t)(seg - D) + 1 + (size_t)ns * 2 + 3;
            int used = decode_scan(D, (size_t)sz, ent_off, ri, ns, sci, sdc, sac, hdc, hac, Ss, Se, AhAl >> 4, AhAl & 15, comps, W, H);
            if (used < 0) { for (int k = 0; k < ncomp; k++) free(comps[k].coef); free(D); return -1; }
            p = ent_off + (size_t)used;
            have_scan = 1;
            continue;
        }
        p += len;
    }
    if (!have_frame || !have_scan || ncomp < 1 || ncomp > 3) { for (int i = 0; i < ncomp; i++) free(comps[i].coef); free(D); return -1; }
    int cw[4], chh[4]; unsigned char* plane[4];
    for (int c = 0; c < ncomp; c++) {
        cw[c] = comps[c].sx * 8; chh[c] = comps[c].sy * 8;
        plane[c] = (unsigned char*)calloc((size_t)cw[c] * chh[c], 1);
        if (!plane[c]) { for (int j = 0; j < c; j++) free(plane[j]); for (int j = 0; j < ncomp; j++) free(comps[j].coef); free(D); return -1; }
        for (int by = 0; by < comps[c].sy; by++) for (int bx = 0; bx < comps[c].sx; bx++) {
            const int* blk = &comps[c].coef[((size_t)by * comps[c].sx + bx) * 64];
            int coeff[64];
#ifdef GMB_NEON
            dequant_neon(blk, q[comps[c].tq], coeff);
            idct_islow_neon(coeff, plane[c] + (size_t)(by * 8) * cw[c] + bx * 8, cw[c]);
#else
            for (int n = 0; n < 64; n++) coeff[n] = blk[n] * q[comps[c].tq][n];
            idct(coeff, plane[c] + (size_t)(by * 8) * cw[c] + bx * 8, cw[c]);
#endif
        }
    }
    out->width = W; out->height = H; out->channels = (ncomp == 1) ? 1 : 3;
    out->data = (unsigned char*)malloc((size_t)W * H * out->channels);
    if (!out->data) { for (int i = 0; i < ncomp; i++) { free(plane[i]); free(comps[i].coef); } free(D); return -1; }
    unsigned char* up[4];
    for (int c = 0; c < ncomp; c++) {
        int hs = comps[c].hs, vs = comps[c].vs;
        up[c] = (unsigned char*)malloc((size_t)W * H);
        if (!up[c]) { for (int j = 0; j < c; j++) free(up[j]); free(out->data); for (int i = 0; i < ncomp; i++) { free(plane[i]); free(comps[i].coef); } free(D); return -1; }
        if (hs == hmax && vs == vmax) { for (int y = 0; y < H; y++) memcpy(up[c] + (size_t)y * W, plane[c] + (size_t)y * cw[c], W); }
        else if (hs * 2 == hmax && vs == vmax) ups_fancy_h2v1(plane[c], cw[c], chh[c], up[c], W, H);
        else if (hs * 2 == hmax && vs * 2 == vmax) ups_fancy_h2v2(plane[c], cw[c], chh[c], up[c], W, H);
        else for (int y = 0; y < H; y++) { const unsigned char* r = plane[c] + (size_t)(y * vs / vmax) * cw[c]; unsigned char* o = up[c] + (size_t)y * W; for (int x = 0; x < W; x++) o[x] = r[x * hs / hmax]; }
    }
    for (int y = 0; y < H; y++) {
        unsigned char* orow = &out->data[(size_t)y * W * out->channels];
        if (ncomp == 1) memcpy(orow, up[0] + (size_t)y * W, (size_t)W);
        else rgb_from_ycc(up[0] + (size_t)y * W, up[1] + (size_t)y * W, up[2] + (size_t)y * W, orow, W);
    }
    for (int c = 0; c < ncomp; c++) free(up[c]);
    for (int i = 0; i < ncomp; i++) { free(plane[i]); free(comps[i].coef); }
    free(D);
    return 0;
}

/* ── encode ── */
static const unsigned char DC_L_BITS[17] = { 0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0 };
static const unsigned char DC_L_VAL[12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };
static const unsigned char DC_C_BITS[17] = { 0,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };
static const unsigned char DC_C_VAL[12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };
static const unsigned char AC_L_BITS[17] = { 0,0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d };
static const unsigned char AC_L_VAL[162] = {
 0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,
 0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
 0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
 0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
 0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,
 0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
 0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa };
static const unsigned char AC_C_BITS[17] = { 0,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77 };
static const unsigned char AC_C_VAL[162] = {
 0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,
 0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
 0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,
 0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
 0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,
 0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
 0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa };

typedef struct { unsigned char* p; size_t len, cap; } jbuf;
static void jb_put(jbuf* b, unsigned char c) { if (b->len == b->cap) { b->cap = b->cap ? b->cap * 2 : 1024; b->p = (unsigned char*)realloc(b->p, b->cap); } b->p[b->len++] = c; }
typedef struct { jbuf* b; unsigned acc; int cnt; } jenc;
static void je_bits(jenc* e, int code, int size) { for (int i = size - 1; i >= 0; i--) { e->acc = (e->acc << 1) | ((code >> i) & 1); if (++e->cnt == 8) { jb_put(e->b, (unsigned char)e->acc); if (e->acc == 0xFF) jb_put(e->b, 0x00); e->acc = 0; e->cnt = 0; } } }
static void build_code(const unsigned char* bits, const unsigned char* vals, int* codes, int* sizes) { int k = 0, code = 0; for (int l = 1; l <= 16; l++) { for (int i = 0; i < bits[l]; i++) { int v = vals[k++]; codes[v] = code; sizes[v] = l; code++; } code <<= 1; } }

static void quant_tbl(int src[64], const unsigned char* base, int q) { int scale = (q < 50) ? (5000 / q) : (200 - 2 * q); for (int i = 0; i < 64; i++) { int v = (base[i] * scale + 50) / 100; if (v < 1) v = 1; if (v > 255) v = 255; src[i] = v; } }

static int jpeg_encode_buf(const mm_image* img, const int q[3][64], jbuf* out) {
    init_cos();
    int W = img->width, H = img->height, ch = img->channels;
    int ncomp = (ch == 1) ? 1 : 3;
    int hmax = (ncomp == 3) ? 2 : 1, vmax = hmax;   /* 4:2:0 per colore */
    int mcux = (W + 8 * hmax - 1) / (8 * hmax), mcuy = (H + 8 * vmax - 1) / (8 * vmax);
    int cw[3], chh[3]; unsigned char* plane[3];
    for (int c = 0; c < ncomp; c++) { int t = (c == 0 || ncomp == 1) ? hmax : 1; int u = (c == 0 || ncomp == 1) ? vmax : 1; cw[c] = mcux * t * 8; chh[c] = mcuy * u * 8; plane[c] = (unsigned char*)calloc((size_t)cw[c] * chh[c], 1); }
    int yw = mcux * hmax * 8, yh = mcuy * vmax * 8;
    for (int y = 0; y < yh; y++) for (int x = 0; x < yw; x++) {
        int sx = x < W ? x : W - 1, sy = y < H ? y : H - 1;
        const unsigned char* p = &img->data[((size_t)sy * W + sx) * ch];
        int r = p[0], g = (ch == 1) ? p[0] : p[1], b = (ch == 1) ? p[0] : p[2];
        double Y = 0.299 * r + 0.587 * g + 0.114 * b, Cb = -0.168736 * r - 0.331264 * g + 0.5 * b + 128, Cr = 0.5 * r - 0.418688 * g - 0.081312 * b + 128;
        plane[0][(size_t)y * cw[0] + x] = (unsigned char)clampi((int)(Y + 0.5));
        if (ncomp == 3 && (y % vmax) == 0 && (x % hmax) == 0) { int cxx = x / hmax, cyy = y / vmax; plane[1][(size_t)cyy * cw[1] + cxx] = (unsigned char)clampi((int)(Cb + 0.5)); plane[2][(size_t)cyy * cw[2] + cxx] = (unsigned char)clampi((int)(Cr + 0.5)); }
    }
    jbuf body = { NULL, 0, 0 };
    jb_put(&body, 0xFF); jb_put(&body, 0xD8);
    for (int c = 0; c < ncomp; c++) { jb_put(&body, 0xFF); jb_put(&body, 0xDB); jb_put(&body, 0); jb_put(&body, 67); jb_put(&body, (unsigned char)c); for (int i = 0; i < 64; i++) jb_put(&body, (unsigned char)q[c][ZZ[i]]); }
    jb_put(&body, 0xFF); jb_put(&body, 0xC0); { int l = 8 + 3 * ncomp; jb_put(&body, l >> 8); jb_put(&body, l & 0xff); }
    jb_put(&body, 8); jb_put(&body, H >> 8); jb_put(&body, H & 0xff); jb_put(&body, W >> 8); jb_put(&body, W & 0xff); jb_put(&body, (unsigned char)ncomp);
    jb_put(&body, 1); jb_put(&body, (unsigned char)((hmax << 4) | vmax)); jb_put(&body, 0);
    if (ncomp == 3) { jb_put(&body, 2); jb_put(&body, 0x11); jb_put(&body, 1); jb_put(&body, 3); jb_put(&body, 0x11); jb_put(&body, 1); }
    { jb_put(&body, 0xFF); jb_put(&body, 0xC4); jb_put(&body, 0); jb_put(&body, 0x1f); jb_put(&body, 0x00); for (int i = 1; i <= 16; i++) jb_put(&body, DC_L_BITS[i]); for (int i = 0; i < 12; i++) jb_put(&body, DC_L_VAL[i]); }
    { jb_put(&body, 0xFF); jb_put(&body, 0xC4); jb_put(&body, 0); jb_put(&body, 0xB5); jb_put(&body, 0x10); for (int i = 1; i <= 16; i++) jb_put(&body, AC_L_BITS[i]); for (int i = 0; i < 162; i++) jb_put(&body, AC_L_VAL[i]); }
    if (ncomp == 3) {
        { jb_put(&body, 0xFF); jb_put(&body, 0xC4); jb_put(&body, 0); jb_put(&body, 0x1f); jb_put(&body, 0x01); for (int i = 1; i <= 16; i++) jb_put(&body, DC_C_BITS[i]); for (int i = 0; i < 12; i++) jb_put(&body, DC_C_VAL[i]); }
        { jb_put(&body, 0xFF); jb_put(&body, 0xC4); jb_put(&body, 0); jb_put(&body, 0xB5); jb_put(&body, 0x11); for (int i = 1; i <= 16; i++) jb_put(&body, AC_C_BITS[i]); for (int i = 0; i < 162; i++) jb_put(&body, AC_C_VAL[i]); }
    }
    jb_put(&body, 0xFF); jb_put(&body, 0xDA); { int l = 6 + 2 * ncomp; jb_put(&body, l >> 8); jb_put(&body, l & 0xff); }
    jb_put(&body, (unsigned char)ncomp); jb_put(&body, 1); jb_put(&body, 0x00);
    if (ncomp == 3) { jb_put(&body, 2); jb_put(&body, 0x11); jb_put(&body, 3); jb_put(&body, 0x11); }
    jb_put(&body, 0); jb_put(&body, 63); jb_put(&body, 0);
    int dcl_c[256] = { 0 }, dcl_s[256] = { 0 }, acl_c[256] = { 0 }, acl_s[256] = { 0 };
    int dcc_c[256] = { 0 }, dcc_s[256] = { 0 }, acc_c[256] = { 0 }, acc_s[256] = { 0 };
    build_code(DC_L_BITS, DC_L_VAL, dcl_c, dcl_s); build_code(AC_L_BITS, AC_L_VAL, acl_c, acl_s);
    build_code(DC_C_BITS, DC_C_VAL, dcc_c, dcc_s); build_code(AC_C_BITS, AC_C_VAL, acc_c, acc_s);
    jenc e = { &body, 0, 0 }; int pd[3] = { 0, 0, 0 }; int bad = 0;
    for (int my = 0; my < mcuy && !bad; my++) for (int mx = 0; mx < mcux && !bad; mx++) {
        for (int c = 0; c < ncomp && !bad; c++) {
            int bh = (c == 0 || ncomp == 1) ? hmax : 1, bv = (c == 0 || ncomp == 1) ? vmax : 1;
            for (int by = 0; by < bv && !bad; by++) for (int bx = 0; bx < bh; bx++) {
                unsigned char blk[64]; int ox = (mx * bh + bx) * 8, oy = (my * bv + by) * 8;
                for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 8; xx++) blk[yy * 8 + xx] = plane[c][(size_t)(oy + yy) * cw[c] + ox + xx];
                double f[64]; fdct(blk, 8, f);
                int coe[64]; for (int n = 0; n < 64; n++) { double d = f[n] / q[c][n]; coe[n] = (int)(d >= 0 ? d + 0.5 : d - 0.5); }
                int* dcp = (c == 0) ? dcl_c : dcc_c; int* dsp = (c == 0) ? dcl_s : dcc_s;
                int* acp = (c == 0) ? acl_c : acc_c; int* asp = (c == 0) ? acl_s : acc_s;
                int diff = coe[0] - pd[c]; pd[c] = coe[0];
                int ad = diff < 0 ? -diff : diff, nd = 0; while (ad) { nd++; ad >>= 1; }
                if (nd > 11) { bad = 1; break; }                    /* oltre le categorie DC standard (0..11) */
                je_bits(&e, dcp[nd], dsp[nd]); if (nd) je_bits(&e, diff < 0 ? diff - 1 : diff, nd);
                int run = 0;
                for (int k = 1; k < 64; k++) { int av = coe[ZZ[k]];
                    if (av == 0) { run++; continue; }
                    while (run > 15) { je_bits(&e, acp[0xF0], asp[0xF0]); run -= 16; }
                    int asz = 0, t = av < 0 ? -av : av; while (t) { asz++; t >>= 1; }
                    if (asz > 10) { bad = 1; break; }               /* oltre le categorie AC standard (1..10) */
                    je_bits(&e, acp[(run << 4) | asz], asp[(run << 4) | asz]);
                    je_bits(&e, av < 0 ? av - 1 : av, asz); run = 0;
                }
                if (bad) break;
                if (run > 0) je_bits(&e, acp[0x00], asp[0x00]);
            }
        }
    }
    if (bad) { for (int c = 0; c < ncomp; c++) free(plane[c]); free(body.p); return -1; }
    if (e.cnt) { e.acc <<= (8 - e.cnt); jb_put(&body, (unsigned char)e.acc); if (e.acc == 0xFF) jb_put(&body, 0x00); }
    jb_put(&body, 0xFF); jb_put(&body, 0xD9);
    for (int c = 0; c < ncomp; c++) free(plane[c]);
    *out = body;
    return 0;
}
static int jpeg_encode_q(const char* path, const mm_image* img, const int q[3][64]) {
    jbuf body; if (jpeg_encode_buf(img, q, &body) != 0) return -1;
    FILE* f = fopen(path, "wb"); if (!f) { free(body.p); return -1; }
    size_t wr = fwrite(body.p, 1, body.len, f); fclose(f); free(body.p);
    return wr == body.len ? 0 : -1;
}
int mm_jpeg_encode_mem(const mm_image* img, int quality, unsigned char** out, size_t* len) {
    if (!img || !img->data || !out || !len) return -1;
    if (quality < 1) quality = 1; if (quality > 100) quality = 100;
    int q[3][64]; quant_tbl(q[0], QLUM, quality); quant_tbl(q[1], QCHR, quality); quant_tbl(q[2], QCHR, quality);
    jbuf body; if (jpeg_encode_buf(img, q, &body) != 0) return -1;
    *out = body.p; *len = body.len; return 0;
}
static int jpeg_encode(const char* path, const mm_image* img, int quality) {
    int q[3][64];
    quant_tbl(q[0], QLUM, quality); quant_tbl(q[1], QCHR, quality); quant_tbl(q[2], QCHR, quality);
    return jpeg_encode_q(path, img, q);
}
int mm_jpeg_write(const char* path, const mm_image* img) { return jpeg_encode(path, img, 90); }
int mm_jpeg_write_q(const char* path, const mm_image* img, int quality) {
    if (quality < 1) quality = 1; if (quality > 100) quality = 100;
    return jpeg_encode(path, img, quality);
}
int mm_jpeg_write_tables(const char* path, const mm_image* img, const unsigned char lum[64], const unsigned char chr[64]) {
    int q[3][64];
    for (int i = 0; i < 64; i++) { int a = lum ? lum[i] : 16; if (a < 1) a = 1; if (a > 255) a = 255; q[0][i] = a; }
    for (int i = 0; i < 64; i++) { int a = chr ? chr[i] : 17; if (a < 1) a = 1; if (a > 255) a = 255; q[1][i] = q[2][i] = a; }
    return jpeg_encode_q(path, img, q);
}
/* Shrink-on-load: decodifica a 1/shrink (box downscale del full-decode). */
int mm_jpeg_read_scaled(const char* path, mm_image* out, int shrink) {
    if (shrink <= 1) return mm_jpeg_read(path, out);
    mm_image full; memset(&full, 0, sizeof full);
    if (mm_jpeg_read(path, &full) != 0) return -1;
    int nw = full.width / shrink, nh = full.height / shrink; if (nw < 1) nw = 1; if (nh < 1) nh = 1;
    out->width = nw; out->height = nh; out->channels = full.channels;
    out->data = (unsigned char*)malloc((size_t)nw * nh * full.channels);
    if (!out->data) { mm_image_free(&full); return -1; }
    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) {
        int s[4] = { 0, 0, 0, 0 };
        for (int dy = 0; dy < shrink; dy++) for (int dx = 0; dx < shrink; dx++) {
            int sy = y * shrink + dy; if (sy >= full.height) sy = full.height - 1;
            int sx = x * shrink + dx; if (sx >= full.width) sx = full.width - 1;
            const unsigned char* p = &full.data[((size_t)sy * full.width + sx) * full.channels];
            for (int c = 0; c < full.channels; c++) s[c] += p[c];
        }
        unsigned char* o = &out->data[((size_t)y * nw + x) * full.channels];
        for (int c = 0; c < full.channels; c++) o[c] = (unsigned char)(s[c] / (shrink * shrink));
    }
    mm_image_free(&full);
    return 0;
}
