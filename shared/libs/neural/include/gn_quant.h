/*
 * gn_quant.h — quantizzazione affine INT8 (da google/XNNPACK, BSD-3).
 *
 * Quantizzazione/dequantizzazione affine: q = round(x/scale) + zero_point,
 * x = (q - zero_point) * scale. Requantizzazione tra due scale (accumulatori
 * INT32 -> INT8), tipica di conv/gemm quantizzate.
 */
#ifndef GMBNEURAL_GN_QUANT_H
#define GMBNEURAL_GN_QUANT_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Quantizza x[n] a int8 con scala e zero-point dati (clamp [-128,127]). */
void gn_quant_i8(const gn_real *x, int n, gn_real scale, int zero_point, signed char *out);
/* Dequantizza q[n] a float. */
void gn_dequant_i8(const signed char *q, int n, gn_real scale, int zero_point, gn_real *out);
/* Requantizza un accumulatore int32 da (in_scale,in_zp) a (out_scale,out_zp) int8. */
void gn_requant_i8(const int *acc, int n, gn_real in_scale, int in_zp,
                   gn_real out_scale, int out_zp, signed char *out);
/* Calcola scala e zero-point da min/max dei dati per la griglia int8 [-128,127]. */
void gn_quant_params(const gn_real *x, int n, gn_real *scale, int *zero_point);

/* GEMM quantizzato uint8 (da pytorch/QNNPACK): out = A(m×k) B(k×n) con
 * accumulo int32, correzione degli zero-point, bias int32, requantizzazione.
 * A: (scale_a, zp_a); B: (scale_b, zp_b); out: (scale_out, zp_out). */
void gn_quant_gemm_u8(unsigned char *out, const unsigned char *a, const unsigned char *b,
                      int m, int k, int n, gn_real scale_a, int zp_a,
                      gn_real scale_b, int zp_b, const int *bias,
                      gn_real scale_out, int zp_out);

/* Requantizzazione a shift (da majianjia/nnom, schema CMSIS-NN):
 * out[i] = clamp((acc[i] + (1<<(rshift-1))) >> rshift, -128, 127).
 * acc tipicamente = sum(in*w) + (bias << bias_shift). */
void gn_quant_requant_shift_i8(const int *acc, int n, int rshift, signed char *out);

/* Convoluzione 2D quantizzata (da NNoM): in uint8 (c*h*w), weight int8
 * (out_c*c*kh*kw), bias int32 (out_c), out int8 (out_c*oh*ow).
 * acc = sum(in*w) + (bias<<bias_shift); out = requant_shift(acc, out_shift). */
gn_status gn_quant_conv2d_i8(signed char *out, const unsigned char *in, int c, int h, int w,
                             const signed char *weight, const int *bias, int out_c,
                             int kh, int kw, int stride, int pad,
                             int bias_shift, int out_shift);

/* Clamp di un tensore uint8 a [lo, hi]. */
void gn_quant_clamp_u8(unsigned char *x, int n, int lo, int hi);

/* Quantizzazione affine PER-CANALE (da ArtyZe/yolo_quantization): dati i pesi
 * w (out_c × per_ch), calcola scala e zero-point per canale dai min/max (griglia
 * uint8 [0,255]). */
void gn_quant_params_channel(const gn_real *w, int out_c, int per_ch,
                             gn_real *scale, int *zp);

/* Convoluzione quantizzata uint8 con scala di uscita PER-CANALE:
 * acc = sum (in - zp_in)*(w - zp_w[oc]); out = clamp(round(M[oc]*acc + bias[oc]) + zp_out).
 * M[oc] = scale_in * scale_w[oc] / scale_out. bias: int32 (out_c). */
gn_status gn_quant_conv2d_i8_channel(unsigned char *out, const unsigned char *in,
                                     int c, int h, int w,
                                     const signed char *weight, const int *bias,
                                     const gn_real *M, int out_c, int kh, int kw,
                                     int stride, int pad, int zp_in, int zp_out);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_QUANT_H */
