/* conic_ffi.c — shim ABI per i coni esponenziali/potenza (namespace OPTIMIZE). */
#include "expcone.h"

#include <stddef.h>

typedef long long i64;

i64 ofx_expcone_barrier(i64 kind, double alpha, const double *x, double *out)
{
    double f = 0.0;
    int r;
    if (x == NULL || out == NULL) return -1;
    r = expcone_barrier((int)kind, alpha, x, &f);
    out[0] = f;
    return (i64)r;
}

i64 ofx_expcone_grad(i64 kind, double alpha, const double *x, double *g)
{
    if (x == NULL || g == NULL) return -1;
    return (i64)expcone_grad((int)kind, alpha, x, g);
}

i64 ofx_expcone_hess(i64 kind, double alpha, const double *x, double *H)
{
    if (x == NULL || H == NULL) return -1;
    return (i64)expcone_hess((int)kind, alpha, x, H);
}

i64 ofx_expcone_dual_in(i64 kind, double alpha, const double *s)
{
    if (s == NULL) return 0;
    return (i64)expcone_dual_in((int)kind, alpha, s);
}

i64 ofx_expcone_dual_point(i64 kind, double alpha, const double *s, double *w)
{
    if (s == NULL || w == NULL) return -1;
    return (i64)expcone_dual_point((int)kind, alpha, s, w);
}

i64 ofx_expcone_nt_metrics(i64 kind, double alpha, const double *z, const double *s, double *m)
{
    if (z == NULL || s == NULL || m == NULL) return -1;
    return (i64)expcone_nt_metrics((int)kind, alpha, z, s, m);
}

/* out = Theta[9] | Thin[9] | rt[9] | scale (28). */
i64 ofx_expcone_scaling(i64 kind, double alpha, const double *z, const double *s, double smu,
                        double *out)
{
    double scale = 0.0;
    int r;
    if (z == NULL || s == NULL || out == NULL) return -1;
    r = expcone_scaling((int)kind, alpha, z, s, smu, out, out + 9, out + 18, &scale);
    out[27] = scale;
    return (i64)r;
}
