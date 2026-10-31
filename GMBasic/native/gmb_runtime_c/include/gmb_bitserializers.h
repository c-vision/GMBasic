/* gmb_bitserializers.h — parità CommonLib/BitSerializers.cs:154 (LE primitives + ToStruct/FromStruct) */
#ifndef GMB_BITSERIALIZERS_H
#define GMB_BITSERIALIZERS_H

#include <stdint.h>
#include <stddef.h>

/* LE read (offset byte) */
uint16_t gmb_bs_to_u16(const uint8_t* data, int offset);
uint32_t gmb_bs_to_u32(const uint8_t* data, int offset);
uint64_t gmb_bs_to_u64(const uint8_t* data, int offset);
int16_t  gmb_bs_to_i16(const uint8_t* data, int offset);
int32_t  gmb_bs_to_i32(const uint8_t* data, int offset);
int64_t  gmb_bs_to_i64(const uint8_t* data, int offset);
float    gmb_bs_to_f32(const uint8_t* data, int offset);
double   gmb_bs_to_f64(const uint8_t* data, int offset);

/* LE write in out */
void gmb_bs_from_u16(uint8_t* out, uint16_t v);
void gmb_bs_from_u32(uint8_t* out, uint32_t v);
void gmb_bs_from_u64(uint8_t* out, uint64_t v);
void gmb_bs_from_i16(uint8_t* out, int16_t v);
void gmb_bs_from_i32(uint8_t* out, int32_t v);
void gmb_bs_from_i64(uint8_t* out, int64_t v);
void gmb_bs_from_f32(uint8_t* out, float v);
void gmb_bs_from_f64(uint8_t* out, double v);

/* ToStruct<T>/FromStruct<T> → memcpy opaco (size = sizeof(struct)) */
void gmb_bs_to_struct(const uint8_t* data, int offset, void* out, size_t size);
void gmb_bs_from_struct(uint8_t* out, const void* v, size_t size);

/* stringhe */
int  gmb_bs_to_string(const uint8_t* data, int offset, int length, char* out, int out_cap);
int  gmb_bs_from_string(uint8_t* out, const char* s, int fixedLength);
int  gmb_bs_to_lp_string(const uint8_t* data, int offset, char* out, int out_cap);
int  gmb_bs_from_lp_string(uint8_t* out, int out_cap, const char* s);

#endif
