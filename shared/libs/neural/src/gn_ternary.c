/* gn_ternary.c — inferenza ternaria (1.58-bit) con bit-slicing (da aethelarch). */
#include "gn_ternary.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

size_t gn_ternary_act_bytes(int dim)
{
    size_t raw = (size_t)(dim + 7) / 8;
    return (raw + 7) & ~(size_t)7;   /* multiplo di 8 byte */
}

gn_status gn_ternary_from_mat(gn_ternary *t, const gn_mat *W)
{
    int i, n;
    gn_real sum = 0.0, scale;
    if (!t || !W) return GN_ERR_ARG;
    n = W->rows * W->cols;
    for (i = 0; i < n; i++) sum += fabs(W->data[i]);
    scale = (n > 0) ? sum / (gn_real)n : 0.0;
    if (scale <= 0.0) scale = 1.0;
    t->rows = W->rows;
    t->cols = W->cols;
    t->scale = scale;
    t->stride = (size_t)((W->cols + 7) / 8);
    t->stride = (t->stride + 7) & ~(size_t)7;
    t->w_pos = (unsigned char *)calloc(t->stride * (size_t)W->rows, 1);
    t->w_neg = (unsigned char *)calloc(t->stride * (size_t)W->rows, 1);
    if (!t->w_pos || !t->w_neg) { gn_ternary_free(t); return GN_ERR_NO_MEM; }
    for (i = 0; i < W->rows; i++) {
        int j;
        unsigned char *rp = t->w_pos + (size_t)i * t->stride;
        unsigned char *rn = t->w_neg + (size_t)i * t->stride;
        for (j = 0; j < W->cols; j++) {
            gn_real q = W->data[(size_t)i * W->cols + j] / scale;
            int byte = j / 8, bit = 1u << (j % 8);
            if (q > 0.5) rp[byte] |= (unsigned char)bit;
            else if (q < -0.5) rn[byte] |= (unsigned char)bit;
        }
    }
    return GN_OK;
}

void gn_ternary_free(gn_ternary *t)
{
    if (!t) return;
    free(t->w_pos); free(t->w_neg);
    t->w_pos = t->w_neg = NULL;
}

/* popcount portabile. */
static int pc8(unsigned char x)
{
    int c = 0;
    while (x) { c += x & 1; x >>= 1; }
    return c;
}

int gn_ternary_dot(const unsigned char *w_pos, const unsigned char *w_neg,
                   const unsigned char *a_bits, int nbits)
{
    int nbytes = (nbits + 7) / 8, i, acc = 0;
    for (i = 0; i < nbytes; i++) {
        unsigned char a = a_bits[i];
        acc += pc8((unsigned char)(a & w_pos[i])) - pc8((unsigned char)(a & w_neg[i]));
    }
    return acc;
}

gn_status gn_ternary_forward(const gn_ternary *t, const gn_mat *x, gn_mat *out)
{
    int b, r;
    size_t abytes;
    unsigned char *abits;
    if (!t || !x || !out || x->cols != t->cols || out->rows != x->rows || out->cols != t->rows)
        return GN_ERR_ARG;
    abytes = gn_ternary_act_bytes(t->cols);
    abits = (unsigned char *)calloc(abytes, 1);
    if (!abits) return GN_ERR_NO_MEM;
    for (b = 0; b < x->rows; b++) {
        int j;
        memset(abits, 0, abytes);
        for (j = 0; j < t->cols; j++)
            if (x->data[(size_t)b * x->cols + j] > 0.0) abits[j / 8] |= (unsigned char)(1u << (j % 8));
        for (r = 0; r < t->rows; r++) {
            const unsigned char *rp = t->w_pos + (size_t)r * t->stride;
            const unsigned char *rn = t->w_neg + (size_t)r * t->stride;
            GN_AT(*out, b, r) = t->scale * (gn_real)gn_ternary_dot(rp, rn, abits, t->cols);
        }
    }
    free(abits);
    return GN_OK;
}

gn_status gn_ternary_to_mat(const gn_ternary *t, gn_mat *W)
{
    int i, j;
    if (!t || !W || W->rows != t->rows || W->cols != t->cols) return GN_ERR_ARG;
    for (i = 0; i < t->rows; i++) {
        const unsigned char *rp = t->w_pos + (size_t)i * t->stride;
        const unsigned char *rn = t->w_neg + (size_t)i * t->stride;
        for (j = 0; j < t->cols; j++) {
            int bit = 1u << (j % 8);
            gn_real v = 0.0;
            if (rp[j / 8] & bit) v = 1.0;
            else if (rn[j / 8] & bit) v = -1.0;
            GN_AT(*W, i, j) = v * t->scale;
        }
    }
    return GN_OK;
}
