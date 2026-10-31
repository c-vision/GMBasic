/* gmb_codec_f16.c — PLUGIN CODEC d'esempio: F16 (half) puro. Usa gli atomi f16 del CODEC (llmcodec_fp16.h).
 *
 * Dimostra l'ABI dei plugin (llmengine_plugin.h): un FORMATO = una dylib dichiarata dalla `.gmbarch`,
 * senza ricompilare compiler/core. L'engine lo carica con gmb_codec_get("f16") (dlopen). */
#include "llmengine_plugin.h"
#include "llmcodec_fp16.h"   /* atomi f16 (round-to-nearest-even) */
#include <stdint.h>

static size_t c_bytes_for(size_t n) { return n * 2; }
static int c_quantize(const float* in, size_t n, void* out, size_t cap)
{
    if (cap < n * 2) return -1;
    uint16_t* o = (uint16_t*)out;
    for (size_t i = 0; i < n; i++) o[i] = llmcodec_f16_from_f32(in[i]);
    return 0;
}
static int c_dequantize(const void* in, size_t n, float* out)
{
    const uint16_t* p = (const uint16_t*)in;
    for (size_t i = 0; i < n; i++) out[i] = llmcodec_f16_to_f32(p[i]);
    return 0;
}

static const LlmCodec g_codec = { "f16", -1, 1, c_bytes_for, c_quantize, c_dequantize };
const LlmCodec* llmcodec_describe(void) { return &g_codec; }
