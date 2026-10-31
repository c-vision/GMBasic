/* FILE: native/gmb_runtime_c/include/gmb_vecquant.h
 * Quantizzazione SQ8 + scoring nel dominio int8 (unica implementazione §0bis).
 * Layout blob SQ8: [scale:float LE][n int8] — len = 4+n. FP32 legacy: len = 4*n
 * (rilevamento per lunghezza, nessuna migrazione dei DB esistenti).
 * C99 portabile (niente SIMD): NEON SDOT/x86 come follow-up DOPO misura (§0sexies). */
#ifndef GMB_VECQUANT_H
#define GMB_VECQUANT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Scala simmetrica: scale = max|x|/127 (0 se vettore nullo). q[i] = round(x[i]/scale). */
void gmb_sq8_encode(const float* x, int n, int8_t* q, float* scale);
/* x[i] = q[i]*scale. */
void gmb_sq8_decode(const int8_t* q, float scale, int n, float* x);
/* Prodotto scalare intero (accumulo int32). */
int32_t gmb_i8_dot(const int8_t* a, const int8_t* b, int n);
/* Coseno nel dominio int8: dot*sa*sb/(|qa|*sa*|qb|*sb); 0 se norma nulla. */
float gmb_sq8_cosine(const int8_t* qa, float sa, const int8_t* qb, float sb, int n);
/* Distanza L2 nel dominio int8 (dequantizzazione on-the-fly, niente alloc). */
float gmb_sq8_l2(const int8_t* qa, float sa, const int8_t* qb, float sb, int n);

/* Self-check con oracoli chiusi: 0 = ok, != 0 = fallimenti. */
int gmb_vecquant_selfcheck(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_VECQUANT_H */
