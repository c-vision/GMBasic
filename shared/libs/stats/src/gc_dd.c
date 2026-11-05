/* gc_dd.c — aritmetica double-float (da apple-bottom, Dekker/Knuth). */
#include "gc_dd.h"

#include <math.h>

void gc_dd_two_sum(float a, float b, float *s, float *e)
{
    float v;
    *s = a + b;
    v = *s - a;
    *e = (a - (*s - v)) + (b - v);
}

void gc_dd_fast_two_sum(float a, float b, float *s, float *e)
{
    *s = a + b;
    *e = b - (*s - a);
}

void gc_dd_two_product(float a, float b, float *p, float *e)
{
    *p = a * b;
    *e = fmaf(a, b, -*p);
}

gc_dd gc_dd_from_real(gc_real x)
{
    gc_dd d;
    d.hi = (float)x;
    d.lo = (float)(x - (gc_real)d.hi);
    return d;
}

gc_real gc_dd_to_real(gc_dd d)
{
    return (gc_real)d.hi + (gc_real)d.lo;
}

gc_dd gc_dd_add(gc_dd a, gc_dd b)
{
    float s1, e1, s2, e2, t1, t2;
    gc_dd r;
    gc_dd_two_sum(a.hi, b.hi, &s1, &e1);
    gc_dd_two_sum(a.lo, b.lo, &s2, &e2);
    gc_dd_two_sum(e1, s2, &t1, &t2);
    t2 += e2;
    gc_dd_two_sum(s1, t1, &s1, &e1);
    e1 += t2;
    gc_dd_fast_two_sum(s1, e1, &r.hi, &r.lo);
    return r;
}

gc_dd gc_dd_neg(gc_dd a)
{
    gc_dd r; r.hi = -a.hi; r.lo = -a.lo; return r;
}

gc_dd gc_dd_sub(gc_dd a, gc_dd b)
{
    return gc_dd_add(a, gc_dd_neg(b));
}

gc_dd gc_dd_mul(gc_dd a, gc_dd b)
{
    /* DWTimesDW2 (Joldes-Muller-Popescu 2017, Alg. 11). */
    float p1, e1, s, e;
    gc_dd r;
    gc_dd_two_product(a.hi, b.hi, &p1, &e1);
    e1 = fmaf(a.hi, b.lo, fmaf(a.lo, b.hi, e1));
    gc_dd_fast_two_sum(p1, e1, &s, &e);
    r.hi = s; r.lo = e;
    return r;
}

gc_dd gc_dd_fma(gc_dd a, gc_dd b, gc_dd c)
{
    gc_dd p = gc_dd_mul(a, b);
    return gc_dd_add(p, c);
}

gc_dd gc_dd_div(gc_dd a, gc_dd b)
{
    float q1 = a.hi / b.hi;
    gc_dd q, r, q2;
    q.hi = q1; q.lo = 0.0f;
    r = gc_dd_sub(a, gc_dd_mul(q, b));
    q2.hi = r.hi / b.hi; q2.lo = 0.0f;
    q = gc_dd_add(q, q2);
    /* rinormalizza */
    {
        float s, e;
        gc_dd_fast_two_sum(q.hi, q.lo, &s, &e);
        q.hi = s; q.lo = e;
    }
    return q;
}

gc_dd gc_dd_sqrt(gc_dd a)
{
    gc_dd r;
    if (a.hi <= 0.0f) { r.hi = 0.0f; r.lo = 0.0f; return r; }
    {
        float x = sqrtf(a.hi);
        gc_dd xd; xd.hi = x; xd.lo = 0.0f;
        /* Newton: r = 0.5*(x + a/x) */
        gc_dd half; half.hi = 0.5f; half.lo = 0.0f;
        r = gc_dd_mul(half, gc_dd_add(xd, gc_dd_div(a, xd)));
        /* una seconda iterazione per la precisione DD */
        r = gc_dd_mul(half, gc_dd_add(r, gc_dd_div(a, r)));
    }
    return r;
}

gc_dd gc_dd_abs(gc_dd a)
{
    gc_dd r; r.hi = fabsf(a.hi); r.lo = fabsf(a.lo); return r;
}

gc_dd gc_dd_dot(int n, const gc_dd *a, const gc_dd *b)
{
    gc_dd s; int i;
    s.hi = 0.0f; s.lo = 0.0f;
    for (i = 0; i < n; i++) s = gc_dd_fma(a[i], b[i], s);
    return s;
}

void gc_dd_gemm(int m, int n, int k, const gc_dd *A, const gc_dd *B, gc_dd *C)
{
    int i, j, l;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) {
            gc_dd s; s.hi = 0.0f; s.lo = 0.0f;
            for (l = 0; l < k; l++)
                s = gc_dd_fma(A[(size_t)i * (size_t)k + (size_t)l], B[(size_t)l * (size_t)n + (size_t)j], s);
            C[(size_t)i * (size_t)n + (size_t)j] = s;
        }
}

void gc_dd_from_reals(const gc_real *x, gc_dd *out, int n)
{
    int i;
    for (i = 0; i < n; i++) out[i] = gc_dd_from_real(x[i]);
}

void gc_dd_to_reals(const gc_dd *x, gc_real *out, int n)
{
    int i;
    for (i = 0; i < n; i++) out[i] = gc_dd_to_real(x[i]);
}

/* ===================== dispatch accelerato (Metal/CPU) ===================== */

#ifdef GC_HAS_METAL
void gc_dd_metal_gemm(int m, int n, int k, const gc_dd *A, const gc_dd *B, gc_dd *C);
gc_dd gc_dd_metal_dot(int n, const gc_dd *a, const gc_dd *b);
#endif

gc_dd gc_dd_dot_accel(int n, const gc_dd *a, const gc_dd *b)
{
#ifdef GC_HAS_METAL
    return gc_dd_metal_dot(n, a, b);
#else
    return gc_dd_dot(n, a, b);
#endif
}

void gc_dd_gemm_accel(int m, int n, int k, const gc_dd *A, const gc_dd *B, gc_dd *C)
{
#ifdef GC_HAS_METAL
    gc_dd_metal_gemm(m, n, k, A, B, C);
#else
    gc_dd_gemm(m, n, k, A, B, C);
#endif
}
