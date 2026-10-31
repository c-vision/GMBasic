/* llmcodec_hf_quant.h — ENCODER (PUL) per i formati HF che il codec Apache DECODIFICA:
 * NF4 (bitsandbytes), AWQ int4 GEMM (AutoAWQ), GPTQ (AutoGPTQ/gptqmodel), FP4 e8m0-block (DS4.1).
 * Simmetria encode/decode: i decoder vivono in LLMEnv/LLMCodec (Apache), la PRODUZIONE dei pesi
 * (encoder) in gmbllm (PUL) — vedi AGENTS §Confine-PUL. Ogni firma rispecchia il decoder.
 */
#ifndef GMBLLM_HF_QUANT_H
#define GMBLLM_HF_QUANT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NF4 (bitsandbytes blockwise): absmax per blocco (F32), 2 nibble/byte (alto = elemento pari).
 * `packed` >= n/2 byte, `absmax` >= ceil(n/blocksize) float. 0 ok / -1. */
int llmcodec_nf4_quant(const float* src, int blocksize, size_t n, uint8_t* packed, float* absmax);

/* AWQ int4 GEMM (AutoAWQ): qweight [in, out/8] uint32 (order_map nibble), qzeros [in/gs, out/8],
 * scales [in/gs, out] F16. 0 ok / -1. */
int llmcodec_awq_quant(const float* w, int group_size, int out, int in,
                       uint32_t* qweight, uint32_t* qzeros, uint16_t* scales);

/* GPTQ (AutoGPTQ): bits in {2,4,8}. qweight [in/per, out], qzeros [in/gs, out/per],
 * scales [in/gs, out] F16, g_idx [in] (gruppo della riga). 0 ok / -1. */
int llmcodec_gptq_quant(const float* w, int bits, int group_size, int out, int in,
                        uint32_t* qweight, uint32_t* qzeros, uint16_t* scales, uint32_t* g_idx);

/* FP4 e2m1 a blocchi 32x32 con scala e8m0 (DS4.1): w = nibble packed (in/2 byte/riga, pari=low),
 * sc = (out/32)*(in/32) byte. 0 ok / -1. */
int llmcodec_dsv41_quant_fp4_block(const float* f, uint8_t* w, uint8_t* sc, int out, int in);

#ifdef __cplusplus
}
#endif
#endif /* GMBLLM_HF_QUANT_H */
