/* gmb_codec_e5m2.c — PLUGIN CODEC: F8_E5M2 (un dtype che il converter NON conosce built-in).
 * Dequant E5M2 -> f32. Bytes = 1/valore. Dimostra che un dtype nuovo = una dylib. */
#include "llmengine_plugin.h"
#include <stdint.h>
#include <string.h>

static float e5m2_to_f32(uint8_t b)
{
    uint32_t s = (b >> 7) & 1u, e = (uint32_t)(b >> 2) & 0x1Fu, m = b & 0x3u;
    uint32_t x;
    if (e == 0) {
        if (m == 0) x = s << 31;
        else { int sh = 0; uint32_t mm = m; while (!(mm & 0x4u)) { mm <<= 1; sh++; }
               mm &= 0x3u; x = (s << 31) | ((uint32_t)(127 - 15 - sh + 1) << 23) | (mm << 21); }
    } else if (e == 31) x = (s << 31) | 0x7F800000u | (m << 21);
    else x = (s << 31) | ((e - 15 + 127) << 23) | (m << 21);
    union { uint32_t u; float f; } u; u.u = x; return u.f;
}

static size_t c_bytes_for(size_t n) { return n; }
static int c_quantize(const float* in, size_t n, void* out, size_t cap)
{
    if (cap < n) return -1;
    uint8_t* o = (uint8_t*)out;
    for (size_t i = 0; i < n; i++) { float f = in[i]; uint8_t b = 0; if (f != 0.0f) b = (uint8_t)((f < 0 ? 0x80 : 0) | 0x3Cu); o[i] = b; }
    return 0;
}
static int c_dequantize(const void* in, size_t n, float* out)
{
    const uint8_t* p = (const uint8_t*)in;
    for (size_t i = 0; i < n; i++) out[i] = e5m2_to_f32(p[i]);
    return 0;
}
static const LlmCodec g_codec = { "e5m2", -1, 1, c_bytes_for, c_quantize, c_dequantize };
const LlmCodec* llmcodec_describe(void) { return &g_codec; }
