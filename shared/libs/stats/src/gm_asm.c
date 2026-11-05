/*
 * gm_asm.c — kernel NEON in assembler inline (AArch64).
 *
 * Unroll 4x (4 vettori di 2 double = 8 double per iterazione) con 4 accumulatori
 * indipendenti per rompere la catena di dipendenza delle FMA (latency hiding).
 * Fallback scalare su altre architetture o con -DGM_NO_ASM.
 */
#include "gm_asm.h"

#if defined(__aarch64__) && !defined(GM_NO_ASM)
#define GM_HAS_ASM 1
#endif

int gm_asm_available(void)
{
#ifdef GM_HAS_ASM
    return 1;
#else
    return 0;
#endif
}

#ifdef GM_HAS_ASM

/* y += a*x, 8 double per iterazione (4 vettori). */
void gm_asm_axpy(int n, gm_real a, const gm_real *x, gm_real *y)
{
    long blocks, tail;
    const double *px = x;
    double *py = y;
    double aa = a;
    const double *paa = &aa;
    if (n <= 0) return;
    blocks = n / 8;
    tail = blocks * 8;
    if (blocks > 0) {
        long c = blocks;
        __asm__ __volatile__(
            "ld1r {v0.2d}, [%[paa]]\n\t"
            "0:\n\t"
            "ld1 {v1.2d}, [%[px]], #16\n\t"
            "ld1 {v2.2d}, [%[px]], #16\n\t"
            "ld1 {v3.2d}, [%[px]], #16\n\t"
            "ld1 {v4.2d}, [%[px]], #16\n\t"
            "ld1 {v5.2d}, [%[py]], #16\n\t"
            "ld1 {v6.2d}, [%[py]], #16\n\t"
            "ld1 {v7.2d}, [%[py]], #16\n\t"
            "ld1 {v8.2d}, [%[py]], #16\n\t"
            "fmla v5.2d, v1.2d, v0.2d\n\t"
            "fmla v6.2d, v2.2d, v0.2d\n\t"
            "fmla v7.2d, v3.2d, v0.2d\n\t"
            "fmla v8.2d, v4.2d, v0.2d\n\t"
            "sub %[py], %[py], #64\n\t"
            "st1 {v5.2d}, [%[py]], #16\n\t"
            "st1 {v6.2d}, [%[py]], #16\n\t"
            "st1 {v7.2d}, [%[py]], #16\n\t"
            "st1 {v8.2d}, [%[py]], #16\n\t"
            "subs %[c], %[c], #1\n\t"
            "b.ne 0b\n\t"
            : [px] "+r"(px), [py] "+r"(py), [c] "+r"(c)
            : [paa] "r"(paa)
            : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "memory", "cc"
        );
    }
    {
        long i = tail;
        for (; i < n; i++) py[i] += aa * px[i];
    }
}

/* dot con 4 accumulatori indipendenti (8 double/iter). */
gm_real gm_asm_dot(int n, const gm_real *x, const gm_real *y)
{
    long blocks, tail;
    const double *px = x, *py = y;
    double acc = 0.0;
    if (n <= 0) return 0.0;
    blocks = n / 8;
    tail = blocks * 8;
    if (blocks > 0) {
        long c = blocks;
        __asm__ __volatile__(
            "movi v0.2d, #0\n\t"
            "movi v9.2d, #0\n\t"
            "movi v10.2d, #0\n\t"
            "movi v11.2d, #0\n\t"
            "0:\n\t"
            "ld1 {v1.2d}, [%[px]], #16\n\t"
            "ld1 {v2.2d}, [%[px]], #16\n\t"
            "ld1 {v3.2d}, [%[px]], #16\n\t"
            "ld1 {v4.2d}, [%[px]], #16\n\t"
            "ld1 {v5.2d}, [%[py]], #16\n\t"
            "ld1 {v6.2d}, [%[py]], #16\n\t"
            "ld1 {v7.2d}, [%[py]], #16\n\t"
            "ld1 {v8.2d}, [%[py]], #16\n\t"
            "fmla v0.2d, v1.2d, v5.2d\n\t"
            "fmla v9.2d, v2.2d, v6.2d\n\t"
            "fmla v10.2d, v3.2d, v7.2d\n\t"
            "fmla v11.2d, v4.2d, v8.2d\n\t"
            "subs %[c], %[c], #1\n\t"
            "b.ne 0b\n\t"
            "fadd v0.2d, v0.2d, v9.2d\n\t"
            "fadd v10.2d, v10.2d, v11.2d\n\t"
            "fadd v0.2d, v0.2d, v10.2d\n\t"
            "faddp d0, v0.2d\n\t"
            "fmov %d[acc], d0\n\t"
            : [px] "+r"(px), [py] "+r"(py), [c] "+r"(c), [acc] "+w"(acc)
            :
            : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "memory", "cc"
        );
    }
    {
        long i = tail;
        for (; i < n; i++) acc += px[i] * py[i];
    }
    return acc;
}

/* somma con 4 accumulatori indipendenti. */
gm_real gm_asm_sum(int n, const gm_real *x)
{
    long blocks, tail;
    const double *px = x;
    double acc = 0.0;
    if (n <= 0) return 0.0;
    blocks = n / 8;
    tail = blocks * 8;
    if (blocks > 0) {
        long c = blocks;
        __asm__ __volatile__(
            "movi v0.2d, #0\n\t"
            "movi v9.2d, #0\n\t"
            "movi v10.2d, #0\n\t"
            "movi v11.2d, #0\n\t"
            "0:\n\t"
            "ld1 {v1.2d}, [%[px]], #16\n\t"
            "ld1 {v2.2d}, [%[px]], #16\n\t"
            "ld1 {v3.2d}, [%[px]], #16\n\t"
            "ld1 {v4.2d}, [%[px]], #16\n\t"
            "fadd v0.2d, v0.2d, v1.2d\n\t"
            "fadd v9.2d, v9.2d, v2.2d\n\t"
            "fadd v10.2d, v10.2d, v3.2d\n\t"
            "fadd v11.2d, v11.2d, v4.2d\n\t"
            "subs %[c], %[c], #1\n\t"
            "b.ne 0b\n\t"
            "fadd v0.2d, v0.2d, v9.2d\n\t"
            "fadd v10.2d, v10.2d, v11.2d\n\t"
            "fadd v0.2d, v0.2d, v10.2d\n\t"
            "faddp d0, v0.2d\n\t"
            "fmov %d[acc], d0\n\t"
            : [px] "+r"(px), [c] "+r"(c), [acc] "+w"(acc)
            :
            : "v0", "v1", "v2", "v3", "v4", "v9", "v10", "v11", "memory", "cc"
        );
    }
    {
        long i = tail;
        for (; i < n; i++) acc += px[i];
    }
    return acc;
}

#else /* fallback scalare */

void gm_asm_axpy(int n, gm_real a, const gm_real *x, gm_real *y)
{
    int i;
    for (i = 0; i < n; i++) y[i] += a * x[i];
}
gm_real gm_asm_dot(int n, const gm_real *x, const gm_real *y)
{
    int i;
    gm_real s = 0.0;
    for (i = 0; i < n; i++) s += x[i] * y[i];
    return s;
}
gm_real gm_asm_sum(int n, const gm_real *x)
{
    int i;
    gm_real s = 0.0;
    for (i = 0; i < n; i++) s += x[i];
    return s;
}

#endif
