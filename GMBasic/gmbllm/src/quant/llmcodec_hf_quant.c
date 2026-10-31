/* llmcodec_hf_quant.c — ENCODER (PUL) per i formati HF decodificati dal codec Apache:
 * NF4 (bitsandbytes), AWQ int4 GEMM (AutoAWQ), GPTQ (AutoGPTQ), FP4 e8m0-block (DS4.1).
 * Simmetria encode/decode completa. Riscrittura propria dal FORMATO dei blocchi/packing
 * (i decoder sono in LLMEnv/LLMCodec/src/llmcodec_quant_{nf4,awq,gptq}.c e llmcodec_fp8_fp4.c). */
#include "llmcodec_hf_quant.h"
#include "llmcodec_fp16.h"
#include "llmcodec_fp8_fp4.h"
#include <string.h>
#include <math.h>

/* ── NF4 (bitsandbytes) ─────────────────────────────────────────────────────── */
static const float GMB_NF4_CODE[16] = {
    -1.0f, -0.6961928009986877f, -0.5250730514526367f, -0.39491748809814453f,
    -0.28444138169288635f, -0.18477343022823334f, -0.09105003625154495f, 0.0f,
    0.07958029955625534f, 0.16093020141124725f, 0.24611230194568634f, 0.33791524171829224f,
    0.44070982933044434f, 0.5626170039176941f, 0.7229568362236023f, 1.0f
};

int llmcodec_nf4_quant(const float* src, int blocksize, size_t n, uint8_t* packed, float* absmax) {
    if (src == NULL || packed == NULL || absmax == NULL || blocksize <= 0 || n == 0) return -1;
    const size_t nblocks = (n + (size_t)blocksize - 1) / (size_t)blocksize;
    for (size_t b = 0; b < nblocks; ++b) {
        size_t s = b * (size_t)blocksize, e = s + (size_t)blocksize; if (e > n) e = n;
        float amax = 0.0f;
        for (size_t j = s; j < e; ++j) { float a = fabsf(src[j]); if (a > amax) amax = a; }
        absmax[b] = amax > 0.0f ? amax : 1.0f;
    }
    memset(packed, 0, (n + 1) / 2);
    for (size_t j = 0; j < n; ++j) {
        const float sc = absmax[j / (size_t)blocksize];
        float v = src[j] / sc; if (v < -1.0f) v = -1.0f; if (v > 1.0f) v = 1.0f;
        int best = 0; float bd = 1e30f;
        for (int i = 0; i < 16; ++i) { float d = fabsf(v - GMB_NF4_CODE[i]); if (d < bd) { bd = d; best = i; } }
        if (j & 1u) packed[j >> 1] |= (uint8_t)(best & 0x0F);
        else        packed[j >> 1] = (uint8_t)((best & 0x0F) << 4);
    }
    return 0;
}

/* ── AWQ int4 GEMM (AutoAWQ) ─────────────────────────────────────────────────── */
/* order_map nibble: il nibble che tiene il canale output (col*8+r) e' AWQ_REVERSE_ORDER[r]. */
static const int GMB_AWQ_REVERSE[8] = { 0, 4, 1, 5, 2, 6, 3, 7 };

int llmcodec_awq_quant(const float* w, int group_size, int out, int in,
                       uint32_t* qweight, uint32_t* qzeros, uint16_t* scales) {
    if (w == NULL || qweight == NULL || qzeros == NULL || scales == NULL) return -1;
    if (group_size <= 0 || in <= 0 || out <= 0 || (in % group_size) != 0 || (out % 8) != 0) return -1;
    const int op = out / 8, ng = in / group_size;
    memset(qweight, 0, (size_t)in * (size_t)op * 4u);
    memset(qzeros, 0, (size_t)ng * (size_t)op * 4u);
    for (int g = 0; g < ng; ++g)
        for (int o = 0; o < out; ++o) {
            const int col = o >> 3, nib = GMB_AWQ_REVERSE[o & 7];
            float mn = 1e30f, mx = -1e30f;
            for (int k = 0; k < group_size; ++k) {
                float v = w[(size_t)o * in + g * group_size + k]; if (v < mn) mn = v; if (v > mx) mx = v;
            }
            float s = (mx - mn) / 15.0f; if (s < 1e-8f) s = 1e-8f;
            int z = (int)lroundf(-mn / s); if (z < 0) z = 0; if (z > 15) z = 15;
            scales[(size_t)g * out + o] = llmcodec_f16_from_f32(s);
            qzeros[(size_t)g * op + col] |= ((uint32_t)z & 0xFu) << (4 * nib);
            for (int k = 0; k < group_size; ++k) {
                const int i = g * group_size + k;
                int q = (int)lroundf(w[(size_t)o * in + i] / s + z);
                if (q < 0) q = 0; if (q > 15) q = 15;
                qweight[(size_t)i * op + col] |= ((uint32_t)q & 0xFu) << (4 * nib);
            }
        }
    return 0;
}

/* ── GPTQ (AutoGPTQ) ─────────────────────────────────────────────────────────── */
int llmcodec_gptq_quant(const float* w, int bits, int group_size, int out, int in,
                        uint32_t* qweight, uint32_t* qzeros, uint16_t* scales, uint32_t* g_idx) {
    if (w == NULL || qweight == NULL || qzeros == NULL || scales == NULL) return -1;
    if (bits != 2 && bits != 4 && bits != 8) return -1;
    if (group_size <= 0 || in <= 0 || out <= 0 || (in % group_size) != 0) return -1;
    const int per = 32 / bits, op = out / per, ng = in / group_size;
    if ((in % per) != 0 || (out % per) != 0) return -1;
    const uint32_t mask = (1u << bits) - 1u;
    memset(qweight, 0, (size_t)(in / per) * (size_t)out * 4u);
    memset(qzeros, 0, (size_t)ng * (size_t)op * 4u);
    for (int g = 0; g < ng; ++g)
        for (int o = 0; o < out; ++o) {
            float mn = 1e30f, mx = -1e30f;
            for (int k = 0; k < group_size; ++k) {
                float v = w[(size_t)o * in + g * group_size + k]; if (v < mn) mn = v; if (v > mx) mx = v;
            }
            float s = (mx - mn) / (float)mask; if (s < 1e-8f) s = 1e-8f;
            /* decode: zeros_eff = (qzeros campo) + 1 -> z in [1, mask+1]; si salva (z-1). */
            int z = (int)lroundf(-mn / s); if (z < 1) z = 1; if (z > (int)mask + 1) z = (int)mask + 1;
            scales[(size_t)g * out + o] = llmcodec_f16_from_f32(s);
            qzeros[(size_t)g * op + o / per] |= ((uint32_t)(z - 1) & mask) << (bits * (o % per));  /* decode: zeros+1 */
            for (int k = 0; k < group_size; ++k) {
                const int i = g * group_size + k;
                int q = (int)lroundf(w[(size_t)o * in + i] / s + z); if (q < 0) q = 0; if (q > (int)mask) q = (int)mask;
                qweight[(size_t)(i / per) * out + o] |= ((uint32_t)q & mask) << (bits * (i % per));
            }
        }
    if (g_idx != NULL) for (int i = 0; i < in; ++i) g_idx[i] = (uint32_t)(i / group_size);
    return 0;
}

/* ── FP4 e2m1 a blocchi 32x32, scala e8m0 (DS4.1) ────────────────────────────── */
static uint8_t gmb_e8m0_for_amax(float amax, float limit) {
    float minv = limit * ldexpf(1.0f, -126);
    if (amax < minv) amax = minv;
    float ex = ceilf(log2f(amax / limit));
    if (ex < -126.0f) ex = -126.0f;
    return (uint8_t)((int)ex + 127);
}

int llmcodec_dsv41_quant_fp4_block(const float* f, uint8_t* w, uint8_t* sc, int out, int in) {
    if (f == NULL || w == NULL || sc == NULL || out <= 0 || in <= 0 || (in & 1)) return -1;
    const int C2 = in / 2, scn = (in + 31) / 32;
    memset(w, 0, (size_t)out * (size_t)C2);
    for (int br = 0; br < out; br += 32)
        for (int bc = 0; bc < in; bc += 32) {
            float amax = 0.0f;
            for (int r = br; r < br + 32 && r < out; ++r)
                for (int c = bc; c < bc + 32 && c < in; ++c) { float a = fabsf(f[(size_t)r * in + c]); if (a > amax) amax = a; }
            const uint8_t sb = gmb_e8m0_for_amax(amax, 6.0f);   /* e2m1 max = 6 */
            sc[(size_t)(br / 32) * scn + bc / 32] = sb;
            const float scale = llmcodec_e8m0_to_f32(sb);
            for (int r = br; r < br + 32 && r < out; ++r)
                for (int c = bc; c < bc + 32 && c < in; ++c) {
                    float v = f[(size_t)r * in + c] / scale;
                    int code = llmcodec_fp4_e2m1_code(fabsf(v)); if (code < 0) code = 0; if (code > 7) code = 7;
                    if (v < 0.0f) code |= 8;
                    if (c & 1) w[(size_t)r * C2 + c / 2] |= (uint8_t)((code & 0x0F) << 4);
                    else       w[(size_t)r * C2 + c / 2] = (uint8_t)(code & 0x0F);
                }
        }
    return 0;
}
