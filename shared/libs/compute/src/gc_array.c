/* gc_array.c — kernel densi SIMD (NEON aarch64, fallback scalare). */
#include "gc_array.h"

#include <math.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define GC_NEON 1
#endif

/* ============================== riduzioni ============================== */

gc_real gc_array_sum(const gc_real *a, int n)
{
    int i = 0;
    gc_real s = 0.0;
    if (a == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 8) {
        float64x2_t a0 = vdupq_n_f64(0.0), a1 = vdupq_n_f64(0.0);
        float64x2_t a2 = vdupq_n_f64(0.0), a3 = vdupq_n_f64(0.0);
        for (; i + 7 < n; i += 8) {
            a0 = vaddq_f64(a0, vld1q_f64(a + i));
            a1 = vaddq_f64(a1, vld1q_f64(a + i + 2));
            a2 = vaddq_f64(a2, vld1q_f64(a + i + 4));
            a3 = vaddq_f64(a3, vld1q_f64(a + i + 6));
        }
        s = vaddvq_f64(vaddq_f64(vaddq_f64(a0, a1), vaddq_f64(a2, a3)));
    }
#endif
    for (; i < n; i++) s += a[i];
    return s;
}

gc_real gc_array_sum_sq(const gc_real *a, int n)
{
    int i = 0;
    gc_real s = 0.0;
    if (a == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 8) {
        float64x2_t a0 = vdupq_n_f64(0.0), a1 = vdupq_n_f64(0.0);
        float64x2_t a2 = vdupq_n_f64(0.0), a3 = vdupq_n_f64(0.0);
        for (; i + 7 < n; i += 8) {
            float64x2_t v0 = vld1q_f64(a + i), v1 = vld1q_f64(a + i + 2);
            float64x2_t v2 = vld1q_f64(a + i + 4), v3 = vld1q_f64(a + i + 6);
            a0 = vfmaq_f64(a0, v0, v0);
            a1 = vfmaq_f64(a1, v1, v1);
            a2 = vfmaq_f64(a2, v2, v2);
            a3 = vfmaq_f64(a3, v3, v3);
        }
        s = vaddvq_f64(vaddq_f64(vaddq_f64(a0, a1), vaddq_f64(a2, a3)));
    }
#endif
    for (; i < n; i++) s += a[i] * a[i];
    return s;
}

gc_real gc_array_dot(const gc_real *a, const gc_real *b, int n)
{
    int i = 0;
    gc_real s = 0.0;
    if (a == NULL || b == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 8) {
        float64x2_t a0 = vdupq_n_f64(0.0), a1 = vdupq_n_f64(0.0);
        float64x2_t a2 = vdupq_n_f64(0.0), a3 = vdupq_n_f64(0.0);
        for (; i + 7 < n; i += 8) {
            a0 = vfmaq_f64(a0, vld1q_f64(a + i), vld1q_f64(b + i));
            a1 = vfmaq_f64(a1, vld1q_f64(a + i + 2), vld1q_f64(b + i + 2));
            a2 = vfmaq_f64(a2, vld1q_f64(a + i + 4), vld1q_f64(b + i + 4));
            a3 = vfmaq_f64(a3, vld1q_f64(a + i + 6), vld1q_f64(b + i + 6));
        }
        s = vaddvq_f64(vaddq_f64(vaddq_f64(a0, a1), vaddq_f64(a2, a3)));
    }
#endif
    for (; i < n; i++) s += a[i] * b[i];
    return s;
}

gc_real gc_array_norm2(const gc_real *a, int n)
{
    return sqrt(gc_array_sum_sq(a, n));
}

gc_real gc_array_norm_inf(const gc_real *a, int n)
{
    int i = 0;
    gc_real m = 0.0;
    if (a == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 2) {
        float64x2_t acc = vdupq_n_f64(0.0);
        for (; i + 1 < n; i += 2) acc = vmaxq_f64(acc, vabsq_f64(vld1q_f64(a + i)));
        m = vmaxvq_f64(acc);
    }
#endif
    for (; i < n; i++) {
        gc_real v = fabs(a[i]);
        if (v > m) m = v;
    }
    return m;
}

gc_real gc_array_asum(const gc_real *a, int n)
{
    int i = 0;
    gc_real s = 0.0;
    if (a == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 2) {
        float64x2_t acc = vdupq_n_f64(0.0);
        for (; i + 1 < n; i += 2) acc = vaddq_f64(acc, vabsq_f64(vld1q_f64(a + i)));
        s = vaddvq_f64(acc);
    }
#endif
    for (; i < n; i++) s += fabs(a[i]);
    return s;
}

gc_real gc_array_min(const gc_real *a, int n)
{
    int i = 0;
    gc_real m;
    if (a == NULL || n <= 0) return 0.0;
    m = a[0];
#ifdef GC_NEON
    if (n >= 2) {
        float64x2_t acc = vdupq_n_f64(a[0]);
        i = 0;
        for (; i + 1 < n; i += 2) acc = vminq_f64(acc, vld1q_f64(a + i));
        m = vminvq_f64(acc);
    }
#endif
    for (; i < n; i++) if (a[i] < m) m = a[i];
    return m;
}

gc_real gc_array_max(const gc_real *a, int n)
{
    int i = 0;
    gc_real m;
    if (a == NULL || n <= 0) return 0.0;
    m = a[0];
#ifdef GC_NEON
    if (n >= 2) {
        float64x2_t acc = vdupq_n_f64(a[0]);
        for (; i + 1 < n; i += 2) acc = vmaxq_f64(acc, vld1q_f64(a + i));
        m = vmaxvq_f64(acc);
    }
#endif
    for (; i < n; i++) if (a[i] > m) m = a[i];
    return m;
}

gc_real gc_array_amax(const gc_real *a, int n)
{
    int i = 0;
    gc_real m = 0.0;
    if (a == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 2) {
        float64x2_t acc = vdupq_n_f64(0.0);
        for (; i + 1 < n; i += 2) {
            float64x2_t v = vabsq_f64(vld1q_f64(a + i));
            acc = vmaxq_f64(acc, v);
        }
        m = vmaxvq_f64(acc);
    }
#endif
    for (; i < n; i++) {
        gc_real v = fabs(a[i]);
        if (v > m) m = v;
    }
    return m;
}

gc_real gc_array_amin(const gc_real *a, int n)
{
    int i = 0;
    gc_real m = 0.0;
    if (a == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 2) {
        float64x2_t acc = vdupq_n_f64(fabs(a[0]));
        for (; i + 1 < n; i += 2) acc = vminq_f64(acc, vabsq_f64(vld1q_f64(a + i)));
        m = vminvq_f64(acc);
    } else {
        m = fabs(a[0]);
    }
#else
    m = fabs(a[0]);
#endif
    for (; i < n; i++) {
        gc_real v = fabs(a[i]);
        if (v < m) m = v;
    }
    return m;
}

int gc_array_imin(const gc_real *a, int n)
{
    int i, k = 0;
    if (a == NULL || n <= 0) return -1;
    for (i = 1; i < n; i++) if (a[i] < a[k]) k = i;
    return k;
}

int gc_array_imax(const gc_real *a, int n)
{
    int i, k = 0;
    if (a == NULL || n <= 0) return -1;
    for (i = 1; i < n; i++) if (a[i] > a[k]) k = i;
    return k;
}

/* ============================== elementwise ============================== */

void gc_array_add(const gc_real *a, const gc_real *b, gc_real *c, int n)
{
    int i = 0;
    if (a == NULL || b == NULL || c == NULL || n <= 0) return;
#ifdef GC_NEON
    for (; i + 1 < n; i += 2)
        vst1q_f64(c + i, vaddq_f64(vld1q_f64(a + i), vld1q_f64(b + i)));
#endif
    for (; i < n; i++) c[i] = a[i] + b[i];
}

void gc_array_sub(const gc_real *a, const gc_real *b, gc_real *c, int n)
{
    int i = 0;
    if (a == NULL || b == NULL || c == NULL || n <= 0) return;
#ifdef GC_NEON
    for (; i + 1 < n; i += 2)
        vst1q_f64(c + i, vsubq_f64(vld1q_f64(a + i), vld1q_f64(b + i)));
#endif
    for (; i < n; i++) c[i] = a[i] - b[i];
}

void gc_array_mul(const gc_real *a, const gc_real *b, gc_real *c, int n)
{
    int i = 0;
    if (a == NULL || b == NULL || c == NULL || n <= 0) return;
#ifdef GC_NEON
    for (; i + 1 < n; i += 2)
        vst1q_f64(c + i, vmulq_f64(vld1q_f64(a + i), vld1q_f64(b + i)));
#endif
    for (; i < n; i++) c[i] = a[i] * b[i];
}

void gc_array_div(const gc_real *a, const gc_real *b, gc_real *c, int n)
{
    int i = 0;
    if (a == NULL || b == NULL || c == NULL || n <= 0) return;
#ifdef GC_NEON
    for (; i + 1 < n; i += 2)
        vst1q_f64(c + i, vdivq_f64(vld1q_f64(a + i), vld1q_f64(b + i)));
#endif
    for (; i < n; i++) c[i] = a[i] / b[i];
}

void gc_array_scale(gc_real *a, gc_real s, int n)
{
    int i = 0;
    if (a == NULL || n <= 0) return;
#ifdef GC_NEON
    {
        float64x2_t vs = vdupq_n_f64(s);
        for (; i + 1 < n; i += 2) vst1q_f64(a + i, vmulq_f64(vld1q_f64(a + i), vs));
    }
#endif
    for (; i < n; i++) a[i] *= s;
}

void gc_array_add_scalar(gc_real *a, gc_real s, int n)
{
    int i = 0;
    if (a == NULL || n <= 0) return;
#ifdef GC_NEON
    {
        float64x2_t vs = vdupq_n_f64(s);
        for (; i + 1 < n; i += 2) vst1q_f64(a + i, vaddq_f64(vld1q_f64(a + i), vs));
    }
#endif
    for (; i < n; i++) a[i] += s;
}

void gc_array_copy(const gc_real *a, gc_real *c, int n)
{
    int i = 0;
    if (a == NULL || c == NULL || n <= 0) return;
#ifdef GC_NEON
    for (; i + 1 < n; i += 2) vst1q_f64(c + i, vld1q_f64(a + i));
#endif
    for (; i < n; i++) c[i] = a[i];
}

void gc_array_fill(gc_real *a, gc_real v, int n)
{
    int i = 0;
    if (a == NULL || n <= 0) return;
#ifdef GC_NEON
    {
        float64x2_t vv = vdupq_n_f64(v);
        for (; i + 1 < n; i += 2) vst1q_f64(a + i, vv);
    }
#endif
    for (; i < n; i++) a[i] = v;
}

void gc_array_sqrt(const gc_real *a, gc_real *c, int n)
{
    int i = 0;
    if (a == NULL || c == NULL || n <= 0) return;
#ifdef GC_NEON
    for (; i + 1 < n; i += 2) vst1q_f64(c + i, vsqrtq_f64(vld1q_f64(a + i)));
#endif
    for (; i < n; i++) c[i] = sqrt(a[i]);
}

void gc_array_abs(const gc_real *a, gc_real *c, int n)
{
    int i = 0;
    if (a == NULL || c == NULL || n <= 0) return;
#ifdef GC_NEON
    for (; i + 1 < n; i += 2) vst1q_f64(c + i, vabsq_f64(vld1q_f64(a + i)));
#endif
    for (; i < n; i++) c[i] = fabs(a[i]);
}

/* ============================== fuse ============================== */

void gc_array_axpy(gc_real alpha, const gc_real *x, gc_real *y, int n)
{
    int i = 0;
    if (x == NULL || y == NULL || n <= 0) return;
#ifdef GC_NEON
    {
        float64x2_t va = vdupq_n_f64(alpha);
        for (; i + 1 < n; i += 2)
            vst1q_f64(y + i, vfmaq_f64(vld1q_f64(y + i), va, vld1q_f64(x + i)));
    }
#endif
    for (; i < n; i++) y[i] += alpha * x[i];
}

void gc_array_axpby(gc_real alpha, const gc_real *x, gc_real beta, gc_real *y, int n)
{
    int i = 0;
    if (x == NULL || y == NULL || n <= 0) return;
#ifdef GC_NEON
    {
        float64x2_t va = vdupq_n_f64(alpha);
        float64x2_t vb = vdupq_n_f64(beta);
        for (; i + 1 < n; i += 2)
            vst1q_f64(y + i, vfmaq_f64(vmulq_f64(vb, vld1q_f64(y + i)), va, vld1q_f64(x + i)));
    }
#endif
    for (; i < n; i++) y[i] = alpha * x[i] + beta * y[i];
}

void gc_array_scaled(gc_real alpha, const gc_real *x, gc_real *y, int n)
{
    int i = 0;
    if (x == NULL || y == NULL || n <= 0) return;
#ifdef GC_NEON
    {
        float64x2_t va = vdupq_n_f64(alpha);
        for (; i + 1 < n; i += 2) vst1q_f64(y + i, vmulq_f64(va, vld1q_f64(x + i)));
    }
#endif
    for (; i < n; i++) y[i] = alpha * x[i];
}

void gc_array_add_scaled(const gc_real *a, gc_real s, const gc_real *b, gc_real *c, int n)
{
    int i = 0;
    if (a == NULL || b == NULL || c == NULL || n <= 0) return;
#ifdef GC_NEON
    {
        float64x2_t vs = vdupq_n_f64(s);
        for (; i + 1 < n; i += 2)
            vst1q_f64(c + i, vfmaq_f64(vld1q_f64(a + i), vs, vld1q_f64(b + i)));
    }
#endif
    for (; i < n; i++) c[i] = a[i] + s * b[i];
}

/* ===================== riduzioni con scarto (SIMD) ===================== */

gc_real gc_array_sum_sq_dev(const gc_real *a, int n, gc_real mean)
{
    int i = 0;
    gc_real s = 0.0;
    if (a == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 2) {
        float64x2_t vm = vdupq_n_f64(mean);
        float64x2_t acc = vdupq_n_f64(0.0);
        for (; i + 1 < n; i += 2) {
            float64x2_t d = vsubq_f64(vld1q_f64(a + i), vm);
            acc = vfmaq_f64(acc, d, d);
        }
        s = vaddvq_f64(acc);
    }
#endif
    for (; i < n; i++) { gc_real d = a[i] - mean; s += d * d; }
    return s;
}

gc_real gc_array_dot_dev(const gc_real *a, const gc_real *b, int n, gc_real ma, gc_real mb)
{
    int i = 0;
    gc_real s = 0.0;
    if (a == NULL || b == NULL || n <= 0) return 0.0;
#ifdef GC_NEON
    if (n >= 2) {
        float64x2_t va = vdupq_n_f64(ma), vb = vdupq_n_f64(mb);
        float64x2_t acc = vdupq_n_f64(0.0);
        for (; i + 1 < n; i += 2) {
            float64x2_t da = vsubq_f64(vld1q_f64(a + i), va);
            float64x2_t db = vsubq_f64(vld1q_f64(b + i), vb);
            acc = vfmaq_f64(acc, da, db);
        }
        s = vaddvq_f64(acc);
    }
#endif
    for (; i < n; i++) s += (a[i] - ma) * (b[i] - mb);
    return s;
}

/* ============================== linspace / arange ============================== */

void gc_linspace(gc_real a, gc_real b, int n, gc_real *out)
{
    int i;
    if (out == NULL || n <= 0) return;
    if (n == 1) { out[0] = a; return; }
    for (i = 0; i < n; i++) out[i] = a + (b - a) * (gc_real)i / (gc_real)(n - 1);
}

int gc_arange(gc_real start, gc_real stop, gc_real step, gc_real *out, int maxn)
{
    int n = 0;
    gc_real v;
    if (out == NULL || maxn <= 0 || step == 0.0) return 0;
    if (step > 0.0) {
        for (v = start; v < stop - 1e-12 && n < maxn; v += step) out[n++] = v;
    } else {
        for (v = start; v > stop + 1e-12 && n < maxn; v += step) out[n++] = v;
    }
    return n;
}
