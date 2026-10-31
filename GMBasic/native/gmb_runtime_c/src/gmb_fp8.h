#ifndef GMB_FP8_H
#define GMB_FP8_H
#include <stdint.h>
/* Decoder per i formati NATIVI DeepSeek V4.1 (safetensors): F8_E4M3 (pesi densi),
 * F8_E8M0 (scale a potenza di 2), FP4 packed (2 per byte, esperti). Port dalle spec OCP MX. */
float llmcodec_f8_e4m3_to_f32(uint8_t x);
float llmcodec_f8_e8m0_to_f32(uint8_t e);
float llmcodec_fp4_nibble_to_f32(uint8_t nibble);   /* e2m1 NON doubled (0,0.5,1,1.5,2,3,4,6) */
/* Dequantizza un blocco FP4-packed: dst = n_out float; src = i byte packed (2 FP4/byte) scalati da
 * scale (E8M0). Usato per gli esperti DeepSeek. */
void llmcodec_fp4_dequant(const uint8_t* src, int64_t n_packed, float scale, float* dst);
#endif
