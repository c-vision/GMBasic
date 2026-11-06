/*
 * gn_conv.h — convoluzione, im2col e pooling per gmbneural.
 *
 * Da pjreddie/darknet (public domain): layout CHW (canale, altezza, larghezza),
 * convoluzione via im2col, max/avg pooling. Usabile per CNN di visione.
 */
#ifndef GMBNEURAL_GN_CONV_H
#define GMBNEURAL_GN_CONV_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* im2col: espande in (c*kh*kw) × (oh*ow) la matrice delle finestre.
 * in: c*h*w (CHW). col: (c*kh*kw)*(oh*ow), con oh/ow calcolati da stride/pad. */
gn_status gn_im2col(const gn_real *in, int c, int h, int w, int kh, int kw,
                    int stride, int pad, gn_real *col, int *oh_out, int *ow_out);
/* col2im: inverso di im2col (somma i contributi). */
gn_status gn_col2im(const gn_real *col, int c, int h, int w, int kh, int kw,
                    int stride, int pad, gn_real *out);

/* Convoluzione 2D (valid, con stride/pad). out: out_c*oh*ow (CHW).
 * weight: out_c*c*kh*kw; bias: out_c (puo' essere NULL). */
gn_status gn_conv2d(gn_real *out, const gn_real *in, int c, int h, int w,
                    const gn_real *weight, const gn_real *bias,
                    int out_c, int kh, int kw, int stride, int pad);

/* Convoluzione 2D via Winograd F(m×m, 3×3) (da Maratyszcza/NNPACK): stride 1,
 * pad 1 (SAME), filtro 3×3. tile m = 2 (F(2x2,3x3)) o 4 (F(4x4,3x3)).
 * in: c*h*w, out: out_c*h*w (CHW). Richiede h,w multipli di m. */
gn_status gn_winograd_conv2d(gn_real *out, const gn_real *in, int c, int h, int w,
                             const gn_real *weight, const gn_real *bias,
                             int out_c, int tile);

/* Local Response Normalization (da onnx2c/ONNX, CHW): normalizza ogni canale
 * sulla somma dei quadrati dei `size` canali vicini (finestra centrata).
 * y = x / (bias + alpha/size * sum(x_c'^2))^beta. */
gn_status gn_lrn(gn_real *out, const gn_real *in, int c, int h, int w,
                 int size, gn_real alpha, gn_real beta, gn_real bias);

/* Resize (upsampling/downsampling) con coordinate half_pixel.
 * in: c*h*w, out: c*oh*ow (CHW). nearest: arrotonda al pixel piu' vicino;
 * bilinear: interpolazione bilineare. */
gn_status gn_resize_nearest(gn_real *out, const gn_real *in, int c, int h, int w, int oh, int ow);
gn_status gn_resize_bilinear(gn_real *out, const gn_real *in, int c, int h, int w, int oh, int ow);

/* Convoluzione 2D DILATATA (da SpleeterRT): il kernel campiona a passo `dil`
 * (dimensione efficace (kh-1)*dil+1). out: out_c*oh*ow, oh=(h+2p-((kh-1)dil+1))/s+1. */
gn_status gn_conv2d_dilated(gn_real *out, const gn_real *in, int c, int h, int w,
                            const gn_real *weight, const gn_real *bias,
                            int out_c, int kh, int kw, int stride, int pad, int dil);

/* Deconvoluzione 2D / transposed convolution (da DeepC/FSRCNN). out: out_c*oh*ow
 * con oh=(h-1)*stride-2*pad+kh, ow=(w-1)*stride-2*pad+kw. Ogni input (iy,ix)
 * contribuisce a out[iy*stride-pad+ky][ix*stride-pad+kx]. bias opzionale. */
gn_status gn_deconv2d(gn_real *out, const gn_real *in, int c, int h, int w,
                      const gn_real *weight, const gn_real *bias,
                      int out_c, int kh, int kw, int stride, int pad);

/* Max pooling 2D. out: c*oh*ow. Se argmax != NULL riceve l'indice (ky*kw+kx). */
gn_status gn_maxpool2d(gn_real *out, const gn_real *in, int c, int h, int w,
                       int size, int stride, int *argmax);
/* Max unpooling 2D (da nnabla): sparge i valori di `in` (c*oh*ow) nelle
 * posizioni argmax della maxpool. argmax: indice ky*size+kx per output.
 * out (c*h*w) riempito con `fill` dove non toccato. */
gn_status gn_maxunpool2d(gn_real *out, const gn_real *in, const int *argmax,
                         int c, int h, int w, int size, int stride, gn_real fill);

/* Average pooling 2D. */
gn_status gn_avgpool2d(gn_real *out, const gn_real *in, int c, int h, int w,
                       int size, int stride);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_CONV_H */
