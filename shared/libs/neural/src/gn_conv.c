/*
 * gn_conv.c — convoluzione/im2col/pooling (da pjreddie/darknet, public domain).
 */
#include "gn_conv.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GN_IDX(c_, h_, w_, y_, x_) (((c_) * (h_) + (y_)) * (w_) + (x_))

gn_status gn_im2col(const gn_real *in, int c, int h, int w, int kh, int kw,
                    int stride, int pad, gn_real *col, int *oh_out, int *ow_out)
{
    int oh, ow, oy, ox, ci, ky, kx;
    if (!in || !col || c < 1 || h < 1 || w < 1 || kh < 1 || kw < 1 || stride < 1) return GN_ERR_ARG;
    oh = (h + 2 * pad - kh) / stride + 1;
    ow = (w + 2 * pad - kw) / stride + 1;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    for (ci = 0; ci < c; ci++)
        for (ky = 0; ky < kh; ky++)
            for (kx = 0; kx < kw; kx++) {
                size_t row = (size_t)((ci * kh + ky) * kw + kx) * (size_t)(oh * ow);
                for (oy = 0; oy < oh; oy++)
                    for (ox = 0; ox < ow; ox++) {
                        int iy = oy * stride - pad + ky, ix = ox * stride - pad + kx;
                        gn_real v = 0.0;
                        if (iy >= 0 && iy < h && ix >= 0 && ix < w)
                            v = in[GN_IDX(ci, h, w, iy, ix)];
                        col[row + (size_t)(oy * ow + ox)] = v;
                    }
            }
    if (oh_out) *oh_out = oh;
    if (ow_out) *ow_out = ow;
    return GN_OK;
}

gn_status gn_col2im(const gn_real *col, int c, int h, int w, int kh, int kw,
                    int stride, int pad, gn_real *out)
{
    int oh, ow, oy, ox, ci, ky, kx;
    if (!col || !out || c < 1 || h < 1 || w < 1 || kh < 1 || kw < 1 || stride < 1) return GN_ERR_ARG;
    oh = (h + 2 * pad - kh) / stride + 1;
    ow = (w + 2 * pad - kw) / stride + 1;
    memset(out, 0, (size_t)c * h * w * sizeof(gn_real));
    for (ci = 0; ci < c; ci++)
        for (ky = 0; ky < kh; ky++)
            for (kx = 0; kx < kw; kx++) {
                size_t row = (size_t)((ci * kh + ky) * kw + kx) * (size_t)(oh * ow);
                for (oy = 0; oy < oh; oy++)
                    for (ox = 0; ox < ow; ox++) {
                        int iy = oy * stride - pad + ky, ix = ox * stride - pad + kx;
                        if (iy >= 0 && iy < h && ix >= 0 && ix < w)
                            out[GN_IDX(ci, h, w, iy, ix)] += col[row + (size_t)(oy * ow + ox)];
                    }
            }
    return GN_OK;
}

gn_status gn_conv2d(gn_real *out, const gn_real *in, int c, int h, int w,
                    const gn_real *weight, const gn_real *bias,
                    int out_c, int kh, int kw, int stride, int pad)
{
    int oh, ow, oc, k, npix, kk, p;
    gn_real *col;
    if (!out || !in || !weight || c < 1 || h < 1 || w < 1 || out_c < 1) return GN_ERR_ARG;
    oh = (h + 2 * pad - kh) / stride + 1;
    ow = (w + 2 * pad - kw) / stride + 1;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    npix = oh * ow;
    kk = c * kh * kw;
    col = (gn_real *)malloc((size_t)kk * (size_t)npix * sizeof(gn_real));
    if (!col) return GN_ERR_NO_MEM;
    if (gn_im2col(in, c, h, w, kh, kw, stride, pad, col, NULL, NULL) != GN_OK) { free(col); return GN_ERR_ARG; }
    for (oc = 0; oc < out_c; oc++) {
        const gn_real *wk = weight + (size_t)oc * kk;
        gn_real b = bias ? bias[oc] : 0.0;
        for (p = 0; p < npix; p++) out[(size_t)oc * npix + p] = b;
        for (k = 0; k < kk; k++) {
            gn_real wv = wk[k];
            const gn_real *crow = col + (size_t)k * npix;
            gn_real *orow = out + (size_t)oc * npix;
            if (wv == 0.0) continue;
            for (p = 0; p < npix; p++) orow[p] += wv * crow[p];
        }
    }
    free(col);
    return GN_OK;
}

gn_status gn_maxpool2d(gn_real *out, const gn_real *in, int c, int h, int w,
                       int size, int stride, int *argmax)
{
    int oh, ow, ci, oy, ox, ky, kx;
    if (!out || !in || c < 1 || h < 1 || w < 1 || size < 1 || stride < 1) return GN_ERR_ARG;
    oh = (h - size) / stride + 1;
    ow = (w - size) / stride + 1;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    for (ci = 0; ci < c; ci++)
        for (oy = 0; oy < oh; oy++)
            for (ox = 0; ox < ow; ox++) {
                gn_real bv = -1e300; int bi = 0;
                for (ky = 0; ky < size; ky++)
                    for (kx = 0; kx < size; kx++) {
                        int iy = oy * stride + ky, ix = ox * stride + kx;
                        gn_real v = in[GN_IDX(ci, h, w, iy, ix)];
                        if (v > bv) { bv = v; bi = ky * size + kx; }
                    }
                out[GN_IDX(ci, oh, ow, oy, ox)] = bv;
                if (argmax) argmax[GN_IDX(ci, oh, ow, oy, ox)] = bi;
            }
    return GN_OK;
}

gn_status gn_avgpool2d(gn_real *out, const gn_real *in, int c, int h, int w,
                       int size, int stride)
{
    int oh, ow, ci, oy, ox, ky, kx;
    gn_real inv;
    if (!out || !in || c < 1 || h < 1 || w < 1 || size < 1 || stride < 1) return GN_ERR_ARG;
    oh = (h - size) / stride + 1;
    ow = (w - size) / stride + 1;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    inv = 1.0 / (gn_real)(size * size);
    for (ci = 0; ci < c; ci++)
        for (oy = 0; oy < oh; oy++)
            for (ox = 0; ox < ow; ox++) {
                gn_real s = 0.0;
                for (ky = 0; ky < size; ky++)
                    for (kx = 0; kx < size; kx++) {
                        int iy = oy * stride + ky, ix = ox * stride + kx;
                        s += in[GN_IDX(ci, h, w, iy, ix)];
                    }
                out[GN_IDX(ci, oh, ow, oy, ox)] = s * inv;
            }
    return GN_OK;
}

/* ===================== Winograd F(m×m, 3×3) (da NNPACK) ===================== */

static const gn_real WGR_B2[4][4] = {
    {1,0,-1,0},{0,1,1,0},{0,-1,1,0},{0,1,0,-1}
};
static const gn_real WGR_G2[4][3] = {
    {1,0,0},{0.5,0.5,0.5},{0.5,-0.5,0.5},{0,0,1}
};
static const gn_real WGR_A2[2][4] = {
    {1,1,1,0},{0,1,-1,-1}
};
static const gn_real WGR_B4[6][6] = {
    {4,0,-5,0,1,0},{0,-4,-4,1,1,0},{0,4,-4,-1,1,0},
    {0,-2,-1,2,1,0},{0,2,-1,-2,1,0},{0,4,0,-5,0,1}
};
static const gn_real WGR_G4[6][3] = {
    {0.25,0,0},{-1.0/6,-1.0/6,-1.0/6},{-1.0/6,1.0/6,-1.0/6},
    {1.0/24,1.0/12,1.0/6},{1.0/24,-1.0/12,1.0/6},{0,0,1}
};
static const gn_real WGR_A4[4][6] = {
    {1,1,1,1,1,0},{0,1,-1,2,-2,0},{0,1,1,4,4,0},{0,1,-1,8,-8,1}
};

static void wgr_gt(const gn_real *G, int r, int c3, const gn_real *g, gn_real *out)
{
    int i, j, k;
    for (i = 0; i < r; i++)
        for (j = 0; j < c3; j++) {
            gn_real s = 0.0;
            for (k = 0; k < c3; k++) s += G[i * c3 + k] * g[k * 3 + j];
            out[i * c3 + j] = s;
        }
}

gn_status gn_winograd_conv2d(gn_real *out, const gn_real *in, int c, int h, int w,
                             const gn_real *weight, const gn_real *bias,
                             int out_c, int tile)
{
    int m, alpha, ty, tx, ci, co, i, j, u, v;
    const gn_real *BT, *G, *AT;
    gn_real *U, *V, *M, *tmp;
    if (!out || !in || !weight || c < 1 || h < 1 || w < 1 || out_c < 1) return GN_ERR_ARG;
    if (tile == 2) { m = 2; alpha = 4; BT = &WGR_B2[0][0]; G = &WGR_G2[0][0]; AT = &WGR_A2[0][0]; }
    else if (tile == 4) { m = 4; alpha = 6; BT = &WGR_B4[0][0]; G = &WGR_G4[0][0]; AT = &WGR_A4[0][0]; }
    else return GN_ERR_ARG;
    if (h % m || w % m) return GN_ERR_ARG;
    U = (gn_real *)malloc((size_t)out_c * c * alpha * alpha * sizeof(gn_real));
    V = (gn_real *)malloc((size_t)alpha * alpha * sizeof(gn_real));
    M = (gn_real *)malloc((size_t)alpha * alpha * sizeof(gn_real));
    tmp = (gn_real *)malloc((size_t)alpha * alpha * sizeof(gn_real));
    if (!U || !V || !M || !tmp) { free(U); free(V); free(M); free(tmp); return GN_ERR_NO_MEM; }
    /* precalcola U[co][ci] = G g G^T */
    for (co = 0; co < out_c; co++)
        for (ci = 0; ci < c; ci++) {
            const gn_real *g = weight + ((size_t)co * c + ci) * 9;
            gn_real *up = U + ((size_t)co * c + ci) * alpha * alpha;
            gn_real t[6 * 3];
            wgr_gt(G, alpha, 3, g, t);              /* t = G g (alpha×3) */
            for (i = 0; i < alpha; i++)
                for (j = 0; j < alpha; j++) {
                    gn_real s = 0.0;
                    for (u = 0; u < 3; u++) s += t[i * 3 + u] * G[j * 3 + u];
                    up[i * alpha + j] = s;
                }
        }
    for (co = 0; co < out_c; co++) {
        for (ty = 0; ty < h / m; ty++)
            for (tx = 0; tx < w / m; tx++) {
                int oy0 = ty * m, ox0 = tx * m;
                for (i = 0; i < alpha * alpha; i++) M[i] = 0.0;
                for (ci = 0; ci < c; ci++) {
                    /* V = B^T d B : d = tile alpha×alpha con pad 1 */
                    for (i = 0; i < alpha; i++)
                        for (j = 0; j < alpha; j++) {
                            int iy = oy0 + i - 1, ix = ox0 + j - 1;
                            tmp[i * alpha + j] = (iy >= 0 && iy < h && ix >= 0 && ix < w)
                                ? in[GN_IDX(ci, h, w, iy, ix)] : 0.0;
                        }
                    /* V = B^T tmp  (alpha×alpha) */
                    for (i = 0; i < alpha; i++)
                        for (j = 0; j < alpha; j++) {
                            gn_real s = 0.0;
                            for (u = 0; u < alpha; u++) s += BT[i * alpha + u] * tmp[u * alpha + j];
                            V[i * alpha + j] = s;
                        }
                    /* tmp = V B  (V * B^T^T = V * B) */
                    for (i = 0; i < alpha; i++)
                        for (j = 0; j < alpha; j++) {
                            gn_real s = 0.0;
                            for (u = 0; u < alpha; u++) s += V[i * alpha + u] * BT[j * alpha + u];
                            tmp[i * alpha + j] = s;
                        }
                    /* M += U ⊙ tmp */
                    {
                        const gn_real *up = U + ((size_t)co * c + ci) * alpha * alpha;
                        for (i = 0; i < alpha * alpha; i++) M[i] += up[i] * tmp[i];
                    }
                }
                /* Y = A^T M A (m×m) */
                for (i = 0; i < m; i++)
                    for (j = 0; j < m; j++) {
                        gn_real s = 0.0, s2;
                        for (u = 0; u < alpha; u++) {
                            gn_real row = 0.0;
                            for (v = 0; v < alpha; v++) row += M[u * alpha + v] * AT[j * alpha + v];
                            s += AT[i * alpha + u] * row;
                        }
                        s2 = s + (bias ? bias[co] : 0.0);
                        out[GN_IDX(co, h, w, oy0 + i, ox0 + j)] = s2;
                    }
            }
    }
    free(U); free(V); free(M); free(tmp);
    return GN_OK;
}

/* ===================== LRN e Resize (da onnx2c/ONNX) ===================== */

gn_status gn_lrn(gn_real *out, const gn_real *in, int c, int h, int w,
                 int size, gn_real alpha, gn_real beta, gn_real bias)
{
    int ch, y, x, cc, lo, hi;
    gn_real norm;
    if (!out || !in || c < 1 || h < 1 || w < 1 || size < 1) return GN_ERR_ARG;
    for (ch = 0; ch < c; ch++) {
        lo = ch - (size - 1) / 2;
        hi = ch + size / 2;
        if (lo < 0) lo = 0;
        if (hi > c - 1) hi = c - 1;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                gn_real s = 0.0;
                for (cc = lo; cc <= hi; cc++) {
                    gn_real v = in[GN_IDX(cc, h, w, y, x)];
                    s += v * v;
                }
                norm = bias + (alpha / (gn_real)size) * s;
                out[GN_IDX(ch, h, w, y, x)] = in[GN_IDX(ch, h, w, y, x)] / pow(norm, beta);
            }
    }
    return GN_OK;
}

static gn_real half_pixel(int i, int in_size, int out_size)
{
    return ((gn_real)i + 0.5) * ((gn_real)in_size / (gn_real)out_size) - 0.5;
}

gn_status gn_resize_nearest(gn_real *out, const gn_real *in, int c, int h, int w, int oh, int ow)
{
    int ch, y, x;
    if (!out || !in || c < 1 || h < 1 || w < 1 || oh < 1 || ow < 1) return GN_ERR_ARG;
    for (ch = 0; ch < c; ch++)
        for (y = 0; y < oh; y++)
            for (x = 0; x < ow; x++) {
                int sy = (int)floor(half_pixel(y, h, oh) + 0.5);
                int sx = (int)floor(half_pixel(x, w, ow) + 0.5);
                if (sy < 0) sy = 0; if (sy >= h) sy = h - 1;
                if (sx < 0) sx = 0; if (sx >= w) sx = w - 1;
                out[GN_IDX(ch, oh, ow, y, x)] = in[GN_IDX(ch, h, w, sy, sx)];
            }
    return GN_OK;
}

gn_status gn_resize_bilinear(gn_real *out, const gn_real *in, int c, int h, int w, int oh, int ow)
{
    int ch, y, x;
    if (!out || !in || c < 1 || h < 1 || w < 1 || oh < 1 || ow < 1) return GN_ERR_ARG;
    for (ch = 0; ch < c; ch++)
        for (y = 0; y < oh; y++) {
            gn_real fy = half_pixel(y, h, oh);
            int y0 = (int)floor(fy), y1;
            gn_real wy;
            if (y0 < 0) y0 = 0;
            if (y0 > h - 1) y0 = h - 1;
            y1 = y0 + 1 < h ? y0 + 1 : h - 1;
            wy = fy - (gn_real)y0;
            if (wy < 0) wy = 0; if (wy > 1) wy = 1;
            for (x = 0; x < ow; x++) {
                gn_real fx = half_pixel(x, w, ow);
                int x0 = (int)floor(fx), x1;
                gn_real wx, v;
                if (x0 < 0) x0 = 0;
                if (x0 > w - 1) x0 = w - 1;
                x1 = x0 + 1 < w ? x0 + 1 : w - 1;
                wx = fx - (gn_real)x0;
                if (wx < 0) wx = 0; if (wx > 1) wx = 1;
                v = (1 - wy) * ((1 - wx) * in[GN_IDX(ch,h,w,y0,x0)] + wx * in[GN_IDX(ch,h,w,y0,x1)])
                  + wy * ((1 - wx) * in[GN_IDX(ch,h,w,y1,x0)] + wx * in[GN_IDX(ch,h,w,y1,x1)]);
                out[GN_IDX(ch, oh, ow, y, x)] = v;
            }
        }
    return GN_OK;
}

/* ===================== Convoluzione dilatata (da SpleeterRT) ===================== */

gn_status gn_conv2d_dilated(gn_real *out, const gn_real *in, int c, int h, int w,
                            const gn_real *weight, const gn_real *bias,
                            int out_c, int kh, int kw, int stride, int pad, int dil)
{
    int ekh, ekw, oh, ow, oc, ci, ky, kx, oy, ox, kk, k;
    if (!out || !in || !weight || c < 1 || h < 1 || w < 1 || out_c < 1 || dil < 1) return GN_ERR_ARG;
    ekh = (kh - 1) * dil + 1;
    ekw = (kw - 1) * dil + 1;
    oh = (h + 2 * pad - ekh) / stride + 1;
    ow = (w + 2 * pad - ekw) / stride + 1;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    kk = c * kh * kw;
    for (oc = 0; oc < out_c; oc++) {
        const gn_real *wk = weight + (size_t)oc * kk;
        gn_real b = bias ? bias[oc] : 0.0;
        for (oy = 0; oy < oh; oy++)
            for (ox = 0; ox < ow; ox++) {
                gn_real acc = b;
                for (k = 0; k < kk; k++) {
                    int cidx = k / (kh * kw);
                    int rem = k % (kh * kw);
                    ky = rem / kw; kx = rem % kw;
                    {
                        int iy = oy * stride - pad + ky * dil;
                        int ix = ox * stride - pad + kx * dil;
                        if (iy >= 0 && iy < h && ix >= 0 && ix < w)
                            acc += wk[k] * in[((size_t)cidx * h + iy) * w + ix];
                    }
                }
                out[((size_t)oc * oh + oy) * ow + ox] = acc;
            }
    }
    (void)ci;
    return GN_OK;
}

/* ===================== Max unpooling (da nnabla) ===================== */

gn_status gn_maxunpool2d(gn_real *out, const gn_real *in, const int *argmax,
                         int c, int h, int w, int size, int stride, gn_real fill)
{
    int oh, ow, ci, oy, ox, i;
    if (!out || !in || !argmax || c < 1 || h < 1 || w < 1 || size < 1 || stride < 1) return GN_ERR_ARG;
    oh = (h - size) / stride + 1;
    ow = (w - size) / stride + 1;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    for (i = 0; i < c * h * w; i++) out[i] = fill;
    for (ci = 0; ci < c; ci++)
        for (oy = 0; oy < oh; oy++)
            for (ox = 0; ox < ow; ox++) {
                int a = argmax[GN_IDX(ci, oh, ow, oy, ox)];
                int ky = a / size, kx = a % size;
                int iy = oy * stride + ky, ix = ox * stride + kx;
                out[GN_IDX(ci, h, w, iy, ix)] = in[GN_IDX(ci, oh, ow, oy, ox)];
            }
    return GN_OK;
}

/* ===================== Deconvoluzione 2D (da DeepC) ===================== */

gn_status gn_deconv2d(gn_real *out, const gn_real *in, int c, int h, int w,
                      const gn_real *weight, const gn_real *bias,
                      int out_c, int kh, int kw, int stride, int pad)
{
    int oh, ow, oc, ci, iy, ix, ky, kx, kk, k;
    if (!out || !in || !weight || c < 1 || h < 1 || w < 1 || out_c < 1 || stride < 1) return GN_ERR_ARG;
    oh = (h - 1) * stride - 2 * pad + kh;
    ow = (w - 1) * stride - 2 * pad + kw;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    kk = c * kh * kw;
    /* azzera con il bias */
    for (oc = 0; oc < out_c; oc++)
        for (iy = 0; iy < oh * ow; iy++) out[(size_t)oc * oh * ow + iy] = bias ? bias[oc] : 0.0;
    for (oc = 0; oc < out_c; oc++) {
        const gn_real *wk = weight + (size_t)oc * kk;
        for (ci = 0; ci < c; ci++)
            for (iy = 0; iy < h; iy++)
                for (ix = 0; ix < w; ix++) {
                    gn_real v = in[GN_IDX(ci, h, w, iy, ix)];
                    if (v == 0.0) continue;
                    for (ky = 0; ky < kh; ky++)
                        for (kx = 0; kx < kw; kx++) {
                            int oy = iy * stride - pad + ky, ox = ix * stride - pad + kx;
                            if (oy < 0 || oy >= oh || ox < 0 || ox >= ow) continue;
                            k = (ci * kh + ky) * kw + kx;
                            out[((size_t)oc * oh + oy) * ow + ox] += v * wk[k];
                        }
                }
    }
    return GN_OK;
}
