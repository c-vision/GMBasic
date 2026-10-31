/* gmb_bitserializers.c — parità CommonLib/BitSerializers.cs:154 (LE primitives + ToStruct/FromStruct) */
#include "gmb_bitserializers.h"
#include <string.h>

uint16_t gmb_bs_to_u16(const uint8_t* data, int offset)
{ return (uint16_t)(data[offset] | data[offset + 1] << 8); }
uint32_t gmb_bs_to_u32(const uint8_t* data, int offset)
{ return (uint32_t)(data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16 | (uint32_t)data[offset + 3] << 24); }
uint64_t gmb_bs_to_u64(const uint8_t* data, int offset)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | data[offset + i];
    return v;
}
int16_t gmb_bs_to_i16(const uint8_t* data, int offset) { return (int16_t)gmb_bs_to_u16(data, offset); }
int32_t gmb_bs_to_i32(const uint8_t* data, int offset) { return (int32_t)gmb_bs_to_u32(data, offset); }
int64_t gmb_bs_to_i64(const uint8_t* data, int offset) { return (int64_t)gmb_bs_to_u64(data, offset); }
float gmb_bs_to_f32(const uint8_t* data, int offset) { uint32_t u = gmb_bs_to_u32(data, offset); float f; memcpy(&f, &u, 4); return f; }
double gmb_bs_to_f64(const uint8_t* data, int offset) { uint64_t u = gmb_bs_to_u64(data, offset); double d; memcpy(&d, &u, 8); return d; }

void gmb_bs_from_u16(uint8_t* out, uint16_t v) { out[0] = (uint8_t)v; out[1] = (uint8_t)(v >> 8); }
void gmb_bs_from_u32(uint8_t* out, uint32_t v) { for (int i = 0; i < 4; i++) out[i] = (uint8_t)(v >> (8 * i)); }
void gmb_bs_from_u64(uint8_t* out, uint64_t v) { for (int i = 0; i < 8; i++) out[i] = (uint8_t)(v >> (8 * i)); }
void gmb_bs_from_i16(uint8_t* out, int16_t v) { gmb_bs_from_u16(out, (uint16_t)v); }
void gmb_bs_from_i32(uint8_t* out, int32_t v) { gmb_bs_from_u32(out, (uint32_t)v); }
void gmb_bs_from_i64(uint8_t* out, int64_t v) { gmb_bs_from_u64(out, (uint64_t)v); }
void gmb_bs_from_f32(uint8_t* out, float v) { uint32_t u; memcpy(&u, &v, 4); gmb_bs_from_u32(out, u); }
void gmb_bs_from_f64(uint8_t* out, double v) { uint64_t u; memcpy(&u, &v, 8); gmb_bs_from_u64(out, u); }

void gmb_bs_to_struct(const uint8_t* data, int offset, void* out, size_t size)
{ if (data && out) memcpy(out, data + offset, size); }
void gmb_bs_from_struct(uint8_t* out, const void* v, size_t size)
{ if (out && v) memcpy(out, v, size); }

int gmb_bs_to_string(const uint8_t* data, int offset, int length, char* out, int out_cap)
{
    if (!data || !out || out_cap <= 0) return 0;
    int n = length < out_cap - 1 ? length : out_cap - 1;
    memcpy(out, data + offset, (size_t)n);
    out[n] = '\0';
    return n;
}
int gmb_bs_from_string(uint8_t* out, const char* s, int fixedLength)
{
    if (!out) return 0;
    memset(out, 0, (size_t)fixedLength);
    if (s) {
        int n = (int)strlen(s);
        if (n > fixedLength) n = fixedLength;
        memcpy(out, s, (size_t)n);
    }
    return fixedLength;
}
int gmb_bs_to_lp_string(const uint8_t* data, int offset, char* out, int out_cap)
{
    if (!data || !out || out_cap <= 0) return 0;
    int len = (int)gmb_bs_to_i32(data, offset);
    return gmb_bs_to_string(data, offset + 4, len, out, out_cap);
}
int gmb_bs_from_lp_string(uint8_t* out, int out_cap, const char* s)
{
    if (!out) return 0;
    int len = s ? (int)strlen(s) : 0;
    if (4 + len > out_cap) len = out_cap - 4;
    gmb_bs_from_i32(out, len);
    if (len > 0) memcpy(out + 4, s, (size_t)len);
    return 4 + len;
}
