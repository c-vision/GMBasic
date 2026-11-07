/* conic_ffi.c — shim ABI per i coni esponenziali/potenza (namespace OPTIMIZE). */
#include "expcone.h"

#include <stddef.h>
#include <stdint.h>

typedef long long i64;

i64 ofx_expcone_barrier(i64 kind, double alpha, const double *x, int64_t x_n, double *out, int64_t out_n)
{
    double f = 0.0;
    int r;
    if (x == NULL || out == NULL) return -1;
    r = expcone_barrier((int)kind, alpha, x, &f);
    out[0] = f;
    return (i64)r;
}

i64 ofx_expcone_grad(i64 kind, double alpha, const double *x, int64_t x_n, double *g, int64_t g_n)
{
    if (x == NULL || g == NULL) return -1;
    return (i64)expcone_grad((int)kind, alpha, x, g);
}

i64 ofx_expcone_hess(i64 kind, double alpha, const double *x, int64_t x_n, double *H, int64_t H_n)
{
    if (x == NULL || H == NULL) return -1;
    return (i64)expcone_hess((int)kind, alpha, x, H);
}

i64 ofx_expcone_dual_in(i64 kind, double alpha, const double *s, int64_t s_n)
{
    if (s == NULL) return 0;
    return (i64)expcone_dual_in((int)kind, alpha, s);
}

i64 ofx_expcone_dual_point(i64 kind, double alpha, const double *s, int64_t s_n, double *w, int64_t w_n)
{
    if (s == NULL || w == NULL) return -1;
    return (i64)expcone_dual_point((int)kind, alpha, s, w);
}

i64 ofx_expcone_nt_metrics(i64 kind, double alpha, const double *z, int64_t z_n, const double *s, int64_t s_n, double *m, int64_t m_n)
{
    if (z == NULL || s == NULL || m == NULL) return -1;
    return (i64)expcone_nt_metrics((int)kind, alpha, z, s, m);
}

/* out = Theta[9] | Thin[9] | rt[9] | scale (28). */
i64 ofx_expcone_scaling(i64 kind, double alpha, const double *z, int64_t z_n, const double *s, int64_t s_n, double smu,
                        double *out, int64_t out_n)
{
    double scale = 0.0;
    int r;
    if (z == NULL || s == NULL || out == NULL) return -1;
    r = expcone_scaling((int)kind, alpha, z, s, smu, out, out + 9, out + 18, &scale);
    out[27] = scale;
    return (i64)r;
}
