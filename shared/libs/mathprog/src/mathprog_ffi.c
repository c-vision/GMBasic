/*
 * mathprog_ffi.c — shim ABI per LP/MILP e model builder (namespace OPTIMIZE).
 *
 * gmbortools usa int/double e handle opachi (gor_model*); l'FFI passa LONG come
 * int64, array LONG come int64* e array DOUBLE come double*. Dove la firma C
 * supera gli 8 argomenti FFI, gli scalari di servizio viaggiano impacchettati
 * in un array `info()` documentato in mathprog.inc.
 */
#include "gor_lp.h"
#include "gor_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

static int *to_iarr(const i64 *a, i64 n)
{
    i64 i;
    int *r;
    if (n <= 0 || a == NULL) return NULL;
    r = (int *)malloc((size_t)n * sizeof(int));
    if (r == NULL) return NULL;
    for (i = 0; i < n; i++) r[i] = (int)a[i];
    return r;
}

/* ── LP ─────────────────────────────────────────────────────────────────── */

i64 ofx_lp_solve(const double *A, const double *b, const double *c, i64 m, i64 n,
                 double *x, double *y, double *info)
{
    double obj = 0.0;
    int r;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || x == NULL || y == NULL)
        return -1;
    r = gor_lp_solve((int)m, (int)n, A, b, c, x, y, &obj, 10000);
    if (info != NULL) info[0] = obj;
    return (i64)r;
}

i64 ofx_lp_revised(const double *A, const double *b, const double *c, const i64 *basis,
                   double *x, i64 m, i64 n)
{
    int *bs;
    int r;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || basis == NULL || x == NULL)
        return -1;
    bs = to_iarr(basis, m);
    if (bs == NULL) return -1;
    r = gor_lp_revised_simplex((int)m, (int)n, A, b, c, bs, 10000, x);
    free(bs);
    return (i64)r;
}

/* bounds() = s[m] | b[m] | l[n] | u[n] */
i64 ofx_lp_bounds(const double *A, const double *bounds, const double *c, double *x,
                  i64 m, i64 n)
{
    if (m <= 0 || n <= 0 || A == NULL || bounds == NULL || c == NULL || x == NULL) return -1;
    return (i64)gor_lp_solve_bounds((int)m, (int)n, A, bounds, bounds + m,
                                    bounds + 2 * m, bounds + 2 * m + n, c, x);
}

i64 ofx_lp_dual(const double *A, const double *b, const double *c, const i64 *basis,
                double *x, i64 m, i64 n)
{
    int *bs;
    int r;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || basis == NULL || x == NULL)
        return -1;
    bs = to_iarr(basis, m);
    if (bs == NULL) return -1;
    r = gor_lp_dual_simplex((int)m, (int)n, A, b, c, bs, 10000, x);
    free(bs);
    return (i64)r;
}

i64 ofx_lp_reduced_costs(const double *A, const double *c, const double *y, double *red,
                         i64 m, i64 n)
{
    if (m <= 0 || n <= 0 || A == NULL || c == NULL || y == NULL || red == NULL) return -1;
    gor_lp_reduced_costs((int)m, (int)n, A, c, y, red);
    return 1;
}

i64 ofx_lp_alternate(const double *A, const double *b, const double *c, double *x2,
                     i64 m, i64 n, double *z2)
{
    double z = 0.0;
    int r;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || x2 == NULL) return -1;
    r = gor_lp_alt_optimum((int)m, (int)n, A, b, c, x2, &z);
    if (z2 != NULL) z2[0] = z;
    return (i64)r;
}

/* intIdx() = nint | idx[0..nint-1]; info(0) = obj. */
i64 ofx_milp_bb(const double *A, const double *b, const double *c, const i64 *intIdx,
                double *x, double *info, i64 m, i64 n)
{
    int *idx;
    int nint, r;
    double obj = 0.0;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || intIdx == NULL || x == NULL)
        return -1;
    nint = (int)intIdx[0];
    if (nint < 0 || nint > n) return -1;
    idx = to_iarr(intIdx + 1, nint);
    if (idx == NULL && nint > 0) return -1;
    r = gor_milp_bb((int)m, (int)n, A, b, c, nint, idx, x, &obj, 5000, 1e-6);
    if (info != NULL) info[0] = obj;
    free(idx);
    return (i64)r;
}

/* info(0)=fixFrac (in), info(1)=rounds (in), info(2)=obj (out). */
i64 ofx_matheuristic_hard_fixing(const double *A, const double *b, const double *c,
                                 const i64 *intIdx, double *x, double *info, i64 m, i64 n)
{
    int *idx;
    int nint, r;
    double obj = 0.0;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || intIdx == NULL ||
        x == NULL || info == NULL) return -1;
    nint = (int)intIdx[0];
    if (nint < 0 || nint > n) return -1;
    idx = to_iarr(intIdx + 1, nint);
    if (idx == NULL && nint > 0) return -1;
    r = gor_matheuristic_hard_fixing((int)m, (int)n, A, b, c, nint, idx,
                                     (gor_real)info[0], (int)info[1], x, &obj, 5000, 1e-6, 12345ULL);
    info[2] = obj;
    free(idx);
    return (i64)r;
}

/* info(0)=kStart (in), info(1)=rounds (in), info(2)=obj (out). */
i64 ofx_matheuristic_local_branching(const double *A, const double *b, const double *c,
                                     const i64 *intIdx, double *x, double *info, i64 m, i64 n)
{
    int *idx;
    int nint, r;
    double obj = 0.0;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || intIdx == NULL ||
        x == NULL || info == NULL) return -1;
    nint = (int)intIdx[0];
    if (nint < 0 || nint > n) return -1;
    idx = to_iarr(intIdx + 1, nint);
    if (idx == NULL && nint > 0) return -1;
    r = gor_matheuristic_local_branching((int)m, (int)n, A, b, c, nint, idx,
                                         (int)info[0], (int)info[1], x, &obj, 5000, 1e-6);
    info[2] = obj;
    free(idx);
    return (i64)r;
}

i64 ofx_gomory_cut(const double *tabrow, i64 ncols, i64 rhs_col, double *cut)
{
    if (ncols <= 0 || tabrow == NULL || cut == NULL) return -1;
    return (i64)gor_gomory_cut((int)ncols, tabrow, (int)rhs_col, cut);
}

/* ── Model builder (handle = puntatore in un LONG) ──────────────────────── */

i64 ofx_model_new(void) { return (i64)(size_t)gor_model_new(); }

void ofx_model_free(i64 h) { if (h) gor_model_free((gor_model *)(size_t)h); }

i64 ofx_model_add_var(i64 h, i64 kind, double lo, double hi, const char *name)
{
    if (!h) return -1;
    return (i64)gor_model_add_var((gor_model *)(size_t)h, (gor_var_kind)kind, lo, hi, name);
}

i64 ofx_model_add_constr(i64 h, const i64 *vars, const double *coef, i64 ncoef, i64 cmp,
                         double rhs)
{
    int *v;
    int r;
    if (!h || ncoef < 0 || vars == NULL || coef == NULL) return -1;
    v = to_iarr(vars, ncoef);
    if (v == NULL && ncoef > 0) return -1;
    r = gor_model_add_constr((gor_model *)(size_t)h, (int)ncoef, v, coef, (gor_cmp)cmp, rhs);
    free(v);
    return (i64)r;
}

void ofx_model_set_obj(i64 h, const i64 *vars, const double *coef, i64 ncoef, i64 sense)
{
    int *v;
    if (!h || ncoef < 0 || vars == NULL || coef == NULL) return;
    v = to_iarr(vars, ncoef);
    if (v == NULL && ncoef > 0) return;
    gor_model_set_obj((gor_model *)(size_t)h, (int)ncoef, v, coef, (gor_obj_sense)sense);
    free(v);
}

i64 ofx_model_solve(i64 h, double *obj)
{
    double o = 0.0;
    int r;
    if (!h) return -1;
    r = gor_model_solve((gor_model *)(size_t)h, &o);
    if (obj != NULL) obj[0] = o;
    return (i64)r;
}

double ofx_model_var_value(i64 h, i64 i)
{
    if (!h || i < 0) return 0.0;
    return (double)gor_model_var_value((const gor_model *)(size_t)h, (int)i);
}

i64 ofx_model_num_vars(i64 h)
{
    if (!h) return 0;
    return (i64)gor_model_num_vars((const gor_model *)(size_t)h);
}

/* Stringa statica riusata (l'FFI copia subito il valore; si libera l'originale). */
char *ofx_model_to_json(i64 h)
{
    static char *buf;
    char *js;
    if (!h) return NULL;
    js = gor_model_to_json((const gor_model *)(size_t)h);
    if (js == NULL) return NULL;
    free(buf);
    buf = js;
    return buf;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
i64 ofx_gomory_cut__ffi(const double *tabrow, int64_t tabrow_ext, i64 ncols, i64 rhs_col, double *cut, int64_t cut_ext) { return ofx_gomory_cut(tabrow, ncols, rhs_col, cut); }
i64 ofx_lp_alternate__ffi(const double *A, int64_t A_ext, const double *b, int64_t b_ext, const double *c, int64_t c_ext, double *x2, int64_t x2_ext, i64 m, i64 n, double *z2, int64_t z2_ext) { return ofx_lp_alternate(A, b, c, x2, m, n, z2); }
i64 ofx_lp_bounds__ffi(const double *A, int64_t A_ext, const double *bounds, int64_t bounds_ext, const double *c, int64_t c_ext, double *x, int64_t x_ext, i64 m, i64 n) { return ofx_lp_bounds(A, bounds, c, x, m, n); }
i64 ofx_lp_dual__ffi(const double *A, int64_t A_ext, const double *b, int64_t b_ext, const double *c, int64_t c_ext, const i64 *basis, int64_t basis_ext, double *x, int64_t x_ext, i64 m, i64 n) { return ofx_lp_dual(A, b, c, basis, x, m, n); }
i64 ofx_lp_reduced_costs__ffi(const double *A, int64_t A_ext, const double *c, int64_t c_ext, const double *y, int64_t y_ext, double *red, int64_t red_ext, i64 m, i64 n) { return ofx_lp_reduced_costs(A, c, y, red, m, n); }
i64 ofx_lp_revised__ffi(const double *A, int64_t A_ext, const double *b, int64_t b_ext, const double *c, int64_t c_ext, const i64 *basis, int64_t basis_ext, double *x, int64_t x_ext, i64 m, i64 n) { return ofx_lp_revised(A, b, c, basis, x, m, n); }
i64 ofx_lp_solve__ffi(const double *A, int64_t A_ext, const double *b, int64_t b_ext, const double *c, int64_t c_ext, i64 m, i64 n, double *x, int64_t x_ext, double *y, int64_t y_ext, double *info, int64_t info_ext) { return ofx_lp_solve(A, b, c, m, n, x, y, info); }
i64 ofx_matheuristic_hard_fixing__ffi(const double *A, int64_t A_ext, const double *b, int64_t b_ext, const double *c, int64_t c_ext, const i64 *intIdx, int64_t intIdx_ext, double *x, int64_t x_ext, double *info, int64_t info_ext, i64 m, i64 n) { return ofx_matheuristic_hard_fixing(A, b, c, intIdx, x, info, m, n); }
i64 ofx_matheuristic_local_branching__ffi(const double *A, int64_t A_ext, const double *b, int64_t b_ext, const double *c, int64_t c_ext, const i64 *intIdx, int64_t intIdx_ext, double *x, int64_t x_ext, double *info, int64_t info_ext, i64 m, i64 n) { return ofx_matheuristic_local_branching(A, b, c, intIdx, x, info, m, n); }
i64 ofx_milp_bb__ffi(const double *A, int64_t A_ext, const double *b, int64_t b_ext, const double *c, int64_t c_ext, const i64 *intIdx, int64_t intIdx_ext, double *x, int64_t x_ext, double *info, int64_t info_ext, i64 m, i64 n) { return ofx_milp_bb(A, b, c, intIdx, x, info, m, n); }
i64 ofx_model_add_constr__ffi(i64 h, const i64 *vars, int64_t vars_ext, const double *coef, int64_t coef_ext, i64 ncoef, i64 cmp, double rhs) { return ofx_model_add_constr(h, vars, coef, ncoef, cmp, rhs); }
void ofx_model_set_obj__ffi(i64 h, const i64 *vars, int64_t vars_ext, const double *coef, int64_t coef_ext, i64 ncoef, i64 sense) { ofx_model_set_obj(h, vars, coef, ncoef, sense); }
i64 ofx_model_solve__ffi(i64 h, double *obj, int64_t obj_ext) { return ofx_model_solve(h, obj); }
