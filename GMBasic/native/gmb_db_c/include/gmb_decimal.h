/* FILE: native/gmb_db_c/include/gmb_decimal.h
 * Software System.Decimal: mantissa unsigned 96-bit, scale 0-28, sign.
 * No libmpdec (no new dependency). Arithmetic via unsigned __int128 (GCC/Clang).
 */
#ifndef GMB_DECIMAL_H
#define GMB_DECIMAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GmbDecimal {
    uint32_t lo;
    uint32_t mid;
    uint32_t hi;
    uint32_t flags; /* bit 31 = sign, bits 16-23 = scale 0-28 (System.Decimal layout) */
} GmbDecimal;

GmbDecimal GmbDecimal_Zero(void);
GmbDecimal GmbDecimal_FromI64(int64_t v);
GmbDecimal GmbDecimal_FromString(const char* s);
GmbDecimal GmbDecimal_FromDouble(double v); /* ABI boundary only */
double GmbDecimal_ToDouble(GmbDecimal d);
int GmbDecimal_ToString(GmbDecimal d, char* out, int cap);
int GmbDecimal_Scale(GmbDecimal d);
int GmbDecimal_IsNegative(GmbDecimal d);
int GmbDecimal_IsZero(GmbDecimal d);
GmbDecimal GmbDecimal_Neg(GmbDecimal d);
GmbDecimal GmbDecimal_Abs(GmbDecimal d);
GmbDecimal GmbDecimal_Add(GmbDecimal a, GmbDecimal b);
GmbDecimal GmbDecimal_Sub(GmbDecimal a, GmbDecimal b);
GmbDecimal GmbDecimal_Mul(GmbDecimal a, GmbDecimal b);
int GmbDecimal_Cmp(GmbDecimal a, GmbDecimal b);
int GmbDecimal_SelfCheck(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DECIMAL_H */
