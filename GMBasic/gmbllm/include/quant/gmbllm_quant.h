/* gmbllm/include/quant/gmbllm_quant.h — API PUL del quant-ENCODE (produce).
 * Implementazioni in gmbllm/src/quant/. La controparte Apache (LLMEnv/LLMCodec) ha
 * solo il DEQUANT + q8_K_encode (actquant). */
#ifndef GMBLLM_QUANT_H
#define GMBLLM_QUANT_H
#include <stdint.h>
/* encode per-formato ggml (model quant) */
int llmcodec_ggml_quant_row(int ggml_type, const float* src, void* dst, int64_t n, const float* imatrix);
/* helper condiviso (anche Apache): scala del blocco */
float llmcodec_make_qp_quants(int n, int nmax, const float* x, uint8_t* L, const float* quant_weights);
/* encode IQ */
void llmcodec_iq2_xxs_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
void llmcodec_iq2_xs_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
void llmcodec_iq2_s_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
void llmcodec_iq1_s_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
void llmcodec_iq1_m_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
void llmcodec_iq3_xxs_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
void llmcodec_iq3_s_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
/* IQ4_XS: encoder PUL (256-block, codebook k_iq4nl, scale super d + 8 sotto-scale ls 6-bit). */
void llmcodec_iq4_xs_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
/* IQ4_NL: encoder PUL (32-block, codebook k_iq4nl, scala d fp16). */
void llmcodec_iq4_nl_quantize(const float* x, int64_t n, uint8_t* out, const float* qw, int64_t qwl, int64_t rl);
/* Byte-per-blocco + valori-per-blocco di un tipo ggml (formato del codec). 0 ok / -1. */
int llmcodec_ggml_block(int t, uint64_t* bb, uint64_t* bv);
/* encode affine MoE */
int llmcodec_moe_affine_quantize(const float* x, uint32_t rows, uint32_t cols, uint32_t group, int bits,
                                 void* out, void* scales, void* biases);
#endif
