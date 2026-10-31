/* llmcodec_iq4_xs.c — encoder IQ4_XS + IQ4_NL (PUL). Port FEDELE di `quantize_row_iq4_nl_impl`
 * (llama.cpp ggml-quants.c): init della scala da `max/values[0]`, FIT ai minimi quadrati
 * `d = Σ(w·q·x)/Σ(w·q²)`, `itry` (iq4_xs ntry=7) e scala super-block (6 bit) per iq4_xs.
 * I pesi `weight` sono `x²` (senza imatrix) o `qw·sqrt(σ²+x²)` (imatrix, iq4_xs).
 * Verificato byte-exact vs upstream con `tools/gmb_codec_bytecompare` (IQ4_NL). */
#include "llmcodec_fp16.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define IQ4NL_EPS 1e-15f

static const int8_t k_iq4nl_xs[16] = {
    -127, -104, -83, -65, -49, -35, -22, -10, 1, 13, 25, 38, 53, 69, 89, 113
};

/* best_index_int8 (id. upstream): indice del valore piu' vicino nella tabella ordinata. */
static int iq4_best_index(float v) {
    const int8_t* val = k_iq4nl_xs;
    if (v <= (float)val[0]) return 0;
    if (v >= (float)val[15]) return 15;
    int ml = 0, mu = 15;
    while (mu - ml > 1) { int mav = (ml + mu) / 2; if (v < (float)val[mav]) mu = mav; else ml = mav; }
    return (v - (float)val[mu-1] < (float)val[mu] - v) ? mu-1 : mu;
}

/* nearest_int (id. upstream, bit-trick round-half-to-even). */
static int iq4_nearest_int(float fval) {
    float v = fval + 12582912.f; int i; memcpy(&i, &v, sizeof(int));
    return (i & 0x007fffff) - 0x00400000;
}

/* Impl fedele: super_block_size ∈ {32 (iq4_nl), 256 (iq4_xs)}, block_size=32.
 * quant_weights != NULL → pesi imatrix (solo iq4_xs). ntry: -1 (iq4_nl) | 7 (iq4_xs). */
static void iq4_impl(const int super_block_size, const int block_size, const float* x,
                     uint16_t* dh, uint8_t* q4, uint16_t* scales_h, uint8_t* scales_l,
                     float* scales, float* weight, uint8_t* L,
                     const float* quant_weights, const int ntry) {
    float sigma2 = 0; for (int j = 0; j < super_block_size; ++j) sigma2 += x[j]*x[j];
    sigma2 *= 2.f/super_block_size;

    memset(q4, 0, (size_t)(super_block_size/2));
    *dh = llmcodec_f16_from_f32(0.f);

    float max_scale = 0, amax_scale = 0;
    const int nblk = super_block_size/block_size;
    for (int ib = 0; ib < nblk; ++ib) {
        const float* xb = x + ib*block_size;
        uint8_t* Lb = L + ib*block_size;
        if (quant_weights) {
            const float* qw = quant_weights + ib*block_size;
            for (int j = 0; j < block_size; ++j) weight[j] = qw[j]*sqrtf(sigma2 + xb[j]*xb[j]);
        } else {
            for (int j = 0; j < block_size; ++j) weight[j] = xb[j]*xb[j];
        }
        float amax = 0, max = 0;
        for (int j = 0; j < block_size; ++j) {
            float ax = fabsf(xb[j]);
            if (ax > amax) { amax = ax; max = xb[j]; }
        }
        if (amax < IQ4NL_EPS) { scales[ib] = 0; continue; }
        float d = ntry > 0 ? -max/(float)k_iq4nl_xs[0] : max/(float)k_iq4nl_xs[0];
        float id = 1/d;
        float sumqx = 0, sumq2 = 0;
        for (int j = 0; j < block_size; ++j) {
            float al = id*xb[j];
            int l = iq4_best_index(al); Lb[j] = (uint8_t)l;
            float q = (float)k_iq4nl_xs[l]; float w = weight[j];
            sumqx += w*q*xb[j]; sumq2 += w*q*q;
        }
        d = sumq2 > 0 ? sumqx/sumq2 : 0.f;
        float best = d*sumqx;
        for (int itry = -ntry; itry <= ntry; ++itry) {
            id = (itry + (float)k_iq4nl_xs[0])/max;
            sumqx = sumq2 = 0;
            for (int j = 0; j < block_size; ++j) {
                float al = id*xb[j];
                int l = iq4_best_index(al);
                float q = (float)k_iq4nl_xs[l]; float w = weight[j];
                sumqx += w*q*xb[j]; sumq2 += w*q*q;
            }
            if (sumq2 > 0 && sumqx*sumqx > best*sumq2) { d = sumqx/sumq2; best = d*sumqx; }
        }
        scales[ib] = d;
        float abs_d = fabsf(d);
        if (abs_d > amax_scale) { amax_scale = abs_d; max_scale = d; }
    }

    if (nblk > 1) {
        memset(scales_h, 0, (size_t)(((nblk+7)/8)*sizeof(uint16_t)));
        float d = -max_scale/32;
        *dh = llmcodec_f16_from_f32(d);
        float id = d ? 1/d : 0.f;
        for (int ib = 0; ib < nblk; ++ib) {
            int l = iq4_nearest_int(id*scales[ib]);
            l = l < -32 ? -32 : (l > 31 ? 31 : l);
            float dl = d*l, idl = dl ? 1/dl : 0.f;
            uint8_t* Lb = L + ib*block_size;
            const float* xb = x + ib*block_size;
            for (int j = 0; j < block_size; ++j) Lb[j] = (uint8_t)iq4_best_index(idl*xb[j]);
            l += 32;
            uint8_t l_l = (uint8_t)(l & 0xf), l_h = (uint8_t)(l >> 4);
            if (ib % 2 == 0) scales_l[ib/2] = l_l; else scales_l[ib/2] |= (uint8_t)(l_l << 4);
            scales_h[ib/8] |= (uint16_t)(l_h << 2*(ib % 8));
        }
    } else {
        *dh = llmcodec_f16_from_f32(scales[0]);
        if (ntry > 0) {
            float id = scales[0] ? 1/scales[0] : 0;
            for (int j = 0; j < super_block_size; ++j) L[j] = (uint8_t)iq4_best_index(id*x[j]);
        }
    }

    for (int i = 0; i < super_block_size/32; ++i)
        for (int j = 0; j < 16; ++j)
            q4[16*i + j] = (uint8_t)(L[32*i + j] | (L[32*i + 16 + j] << 4));
}

void llmcodec_iq4_xs_quantize(const float* x, int64_t n, uint8_t* out,
                              const float* qw, int64_t qwl, int64_t rl) {
    (void)qwl;
    if (x == NULL || out == NULL || n <= 0) return;
    const int64_t nb = n / 256;
    const int64_t bpr = rl > 0 ? rl/256 : nb;   /* blocchi per riga (allineati a quantize_iq4_xs) */
    uint8_t L[256]; float weight[32]; float scales[8];
    for (int64_t i = 0; i < nb; ++i) {
        uint8_t* yb = out + i * 136;   /* d(2) scales_h(2) scales_l(4) qs(128) */
        uint16_t dh = 0, sh = 0; uint8_t q4[128], sl[4] = {0,0,0,0};
        const float* qwi = qw ? qw + 256*(bpr > 0 ? (i % bpr) : i) : NULL;
        iq4_impl(256, 32, x + i*256, &dh, q4, &sh, sl, scales, weight, L, qwi, 7);
        memcpy(yb + 0, &dh, 2); memcpy(yb + 2, &sh, 2);
        memcpy(yb + 4, sl, 4); memcpy(yb + 8, q4, 128);
    }
}

/* IQ4_NL: blocco da 32 (blk_iq4_nl = d fp16 + qs[16]); un solo codebook, nessuna sotto-scala. */
void llmcodec_iq4_nl_quantize(const float* x, int64_t n, uint8_t* out,
                              const float* qw, int64_t qwl, int64_t rl) {
    (void)qw; (void)qwl; (void)rl;
    if (x == NULL || out == NULL || n <= 0) return;
    const int64_t nb = n / 32;
    const int64_t bpr = rl > 0 ? rl/32 : nb;   /* blocchi per riga (allineati a quantize_iq4_nl) */
    uint8_t L[32]; float weight[32]; float scales[1];
    for (int64_t i = 0; i < nb; ++i) {
        uint8_t* yb = out + i * 18;   /* d(2) qs(16) */
        uint16_t dh = 0; uint8_t q4[16];
        const float* qwi = qw ? qw + 32*(bpr > 0 ? (i % bpr) : i) : NULL;
        iq4_impl(32, 32, x + i*32, &dh, q4, NULL, NULL, scales, weight, L, qwi, 7);
        memcpy(yb + 0, &dh, 2); memcpy(yb + 2, q4, 16);
    }
}
