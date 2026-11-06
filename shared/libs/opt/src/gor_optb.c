/*
 * gor_optb.c — superficie FFI delle 34 funzioni `OPTIMIZE` (vedi gor_optb.h).
 *
 * Tutto il motore numerico vive in gmi_opt.c / gmi_optfit.c / simplex.c e negli
 * adattatori gor_opt_api.c: qui ci sono solo i thunk di callback (l'ABI FFI non
 * ha `ctx`) e la conversione dei parametri piatti (matrici row-major, bounds a
 * coppie) verso quella interna.
 */
#include "gor_optb.h"
#include "gor_optimize.h"
#include "gmi_opt.h"
#include "gmi_optfit.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Thunk callback: il puntatore BASIC viaggia nel `ctx` interno                */
/* ------------------------------------------------------------------------- */

typedef struct { gor_optb_fx f; } optb_sx;
typedef struct { gor_optb_fv f; } optb_sv;
typedef struct { gor_optb_fvv f; } optb_svv;
typedef struct { gor_optb_fm f; } optb_sm;
typedef struct { gor_optb_fvm f; } optb_svm;

static double optb_t_fx(double x, void *ctx) { return ((optb_sx *)ctx)->f(x); }
static double optb_t_fv(const double *x, int n, void *ctx)
{
    return ((optb_sv *)ctx)->f(x, (int64_t)n);
}
static void optb_t_fvv(const double *x, int n, double *out, void *ctx)
{
    ((optb_svv *)ctx)->f(x, (int64_t)n, out);
}
static double optb_t_fm(const double *p, int np, double x, void *ctx)
{
    return ((optb_sm *)ctx)->f(p, (int64_t)np, x);
}
static void optb_t_fvm(const double *p, int np, const double *x, int nx, double *out, void *ctx)
{
    ((optb_svm *)ctx)->f(p, (int64_t)np, x, (int64_t)nx, out);
}

/* Residui con m != n: il runtime FFI deduce la lunghezza dell'array di uscita
 * dalla lunghezza dell'array in ingresso, quindi si ripad x a m (i residui
 * leggono solo i primi n elementi; m >= n e' il caso reale). */
typedef struct { gor_optb_fvv f; int n; int m; double *buf; } optb_lsq;
static void optb_t_lsq(const double *x, int n, double *out, void *ctx)
{
    optb_lsq *w = (optb_lsq *)ctx;
    int i;
    for (i = 0; i < w->m; i++) w->buf[i] = i < n ? x[i] : 0.0;
    w->f(w->buf, (int64_t)w->m, out);
    (void)n;
}

/* ------------------------------------------------------------------------- */
/* Bounds: coppie piatte (lo0,hi0,...) -> due vettori                        */
/* ------------------------------------------------------------------------- */

static void optb_unpack_bounds(const double *bounds, int n, double **lo, double **hi)
{
    int i;
    *lo = NULL;
    *hi = NULL;
    if (bounds == NULL || n <= 0) return;
    *lo = (double *)malloc((size_t)n * sizeof(double));
    *hi = (double *)malloc((size_t)n * sizeof(double));
    if (*lo == NULL || *hi == NULL) { free(*lo); free(*hi); *lo = NULL; *hi = NULL; return; }
    for (i = 0; i < n; i++) { (*lo)[i] = bounds[2 * i]; (*hi)[i] = bounds[2 * i + 1]; }
}

static void optb_norm_method(const char *in, char *out, size_t cap)
{
    size_t i = 0, j = 0;
    if (in == NULL) { out[0] = '\0'; return; }
    while (in[i] != '\0' && j + 1 < cap) {
        unsigned char ch = (unsigned char)in[i++];
        if (ch == '-' || ch == '_' || ch == ' ') continue;
        out[j++] = (char)tolower(ch);
    }
    out[j] = '\0';
}

static int optb_same(const char *a, const char *b) { return strcmp(a, b) == 0; }

/* ------------------------------------------------------------------------- */
/* 1D                                                                        */
/* ------------------------------------------------------------------------- */

double gor_optb_newton(gor_optb_fx f, double guess)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_newton(optb_t_fx, guess, 1e-10, 100, &s);
}

double gor_optb_bisection(gor_optb_fx f, double a, double b)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_bisection(optb_t_fx, a, b, 1e-12, 200, &s);
}

double gor_optb_brent(gor_optb_fx f, double a, double b)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_brent(optb_t_fx, a, b, 1e-12, 200, &s);
}

double gor_optb_secant(gor_optb_fx f, double x0, double x1)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_secant(optb_t_fx, x0, x1, 1e-10, 200, &s);
}

double gor_optb_golden(gor_optb_fx f, double a, double b)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_golden(optb_t_fx, a, b, 1e-10, 500, &s);
}

double gor_optb_brentq(gor_optb_fx f, double a, double b)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_brentq(optb_t_fx, &s, a, b);
}

double gor_optb_brenth(gor_optb_fx f, double a, double b)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_brenth(optb_t_fx, &s, a, b);
}

double gor_optb_ridder(gor_optb_fx f, double a, double b)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_ridder(optb_t_fx, &s, a, b);
}

double gor_optb_toms748(gor_optb_fx f, double a, double b)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_toms748(optb_t_fx, &s, a, b);
}

double gor_optb_brentmin(gor_optb_fx f, double a, double b)
{
    optb_sx s;
    if (f == NULL) return 0.0;
    s.f = f;
    return gmi_opt_brentmin(optb_t_fx, &s, a, b);
}

double gor_optb_minimize_scalar(gor_optb_fx f, double a, double b, const char *method)
{
    optb_sx s;
    char m[32];
    if (f == NULL) return 0.0;
    s.f = f;
    optb_norm_method(method, m, sizeof(m));
    if (m[0] == '\0' || optb_same(m, "brent") || optb_same(m, "brentmin"))
        return gmi_opt_brentmin(optb_t_fx, &s, a, b);
    if (optb_same(m, "golden") || optb_same(m, "goldensection"))
        return gmi_opt_golden(optb_t_fx, a, b, 1e-10, 500, &s);
    return gmi_opt_brentmin(optb_t_fx, &s, a, b);
}

double gor_optb_minimize_scalar_default(gor_optb_fx f, double a, double b)
{
    return gor_optb_minimize_scalar(f, a, b, "brent");
}

/* ------------------------------------------------------------------------- */
/* Multivariata                                                              */
/* ------------------------------------------------------------------------- */

void gor_optb_nelder_mead(gor_optb_fv f, const double *x0, int n, double *out)
{
    optb_sv s;
    double *start;
    int i;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    start = (double *)malloc((size_t)n * sizeof(double));
    if (start == NULL) return;
    for (i = 0; i < n; i++) start[i] = x0[i];
    s.f = f;
    gmi_opt_nelder_mead(optb_t_fv, start, n, 1e-10, 500, &s, out);
    free(start);
}

void gor_optb_bfgs(gor_optb_fv f, const double *x0, int n, double *out)
{
    optb_sv s;
    double *start;
    int i;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    start = (double *)malloc((size_t)n * sizeof(double));
    if (start == NULL) return;
    for (i = 0; i < n; i++) start[i] = x0[i];
    s.f = f;
    gmi_opt_bfgs(optb_t_fv, start, n, 1e-10, 500, &s, out);
    free(start);
}

void gor_optb_lbfgs(gor_optb_fv f, const double *x0, int n, double *out)
{
    optb_sv s;
    double *start;
    int i;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    start = (double *)malloc((size_t)n * sizeof(double));
    if (start == NULL) return;
    for (i = 0; i < n; i++) start[i] = x0[i];
    s.f = f;
    gmi_opt_lbfgs(optb_t_fv, start, n, 1e-10, 10, 500, &s, out);
    free(start);
}

void gor_optb_conjugate_gradient(gor_optb_fv f, const double *x0, int n, double *out)
{
    optb_sv s;
    double *start;
    int i;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    start = (double *)malloc((size_t)n * sizeof(double));
    if (start == NULL) return;
    for (i = 0; i < n; i++) start[i] = x0[i];
    s.f = f;
    gmi_opt_cg(optb_t_fv, start, n, 1e-10, 500, &s, out);
    free(start);
}

void gor_optb_powell(gor_optb_fv f, const double *x0, int n, double *out)
{
    optb_sv s;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    s.f = f;
    gmi_opt_powell(optb_t_fv, &s, x0, n, out);
}

void gor_optb_tnc(gor_optb_fv f, const double *x0, int n, const double *bounds, double *out)
{
    optb_sv s;
    double *lo, *hi;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    s.f = f;
    optb_unpack_bounds(bounds, n, &lo, &hi);
    gmi_opt_tnc(optb_t_fv, &s, x0, n, lo, hi, out);
    free(lo); free(hi);
}

void gor_optb_cobyla(gor_optb_fv f, const double *x0, int n, const double *bounds, double *out)
{
    optb_sv s;
    double *lo, *hi;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    s.f = f;
    optb_unpack_bounds(bounds, n, &lo, &hi);
    gmi_opt_cobyla(optb_t_fv, &s, x0, n, lo, hi, out);
    free(lo); free(hi);
}

void gor_optb_slsqp(gor_optb_fv f, const double *x0, int n, const double *bounds, double *out)
{
    optb_sv s;
    double *lo, *hi;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    s.f = f;
    optb_unpack_bounds(bounds, n, &lo, &hi);
    gmi_opt_slsqp(optb_t_fv, &s, x0, n, lo, hi, out);
    free(lo); free(hi);
}

int gor_optb_minimize(gor_optb_fv f, const double *x0, int n,
                      const char *method, const double *bounds, double *out)
{
    char m[32];
    double *lo = NULL, *hi = NULL;
    int rc = GOR_OK;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return GOR_ERR_ARG;
    optb_norm_method(method, m, sizeof(m));
    if (m[0] == '\0') strcpy(m, "neldermead");
    optb_unpack_bounds(bounds, n, &lo, &hi);
    if (optb_same(m, "nelder-mead") || optb_same(m, "neldermead"))
        gor_optb_nelder_mead(f, x0, n, out);
    else if (optb_same(m, "bfgs"))
        gor_optb_bfgs(f, x0, n, out);
    else if (optb_same(m, "l-bfgs") || optb_same(m, "lbfgs"))
        gor_optb_lbfgs(f, x0, n, out);
    else if (optb_same(m, "cg") || optb_same(m, "conjugategradient"))
        gor_optb_conjugate_gradient(f, x0, n, out);
    else if (optb_same(m, "powell"))
        gor_optb_powell(f, x0, n, out);
    else if (optb_same(m, "tnc"))
        gor_optb_tnc(f, x0, n, bounds, out);
    else if (optb_same(m, "cobyla"))
        gor_optb_cobyla(f, x0, n, bounds, out);
    else if (optb_same(m, "slsqp"))
        gor_optb_slsqp(f, x0, n, bounds, out);
    else
        rc = GOR_ERR_ARG;
    free(lo); free(hi);
    return rc;
}

/* ------------------------------------------------------------------------- */
/* Globali                                                                   */
/* ------------------------------------------------------------------------- */

void gor_optb_brute(gor_optb_fv f, const double *ranges, int n, int N, double *out)
{
    optb_sv s;
    double *lo, *hi;
    if (f == NULL || ranges == NULL || out == NULL || n <= 0) return;
    s.f = f;
    optb_unpack_bounds(ranges, n, &lo, &hi);
    if (lo == NULL || hi == NULL) { free(lo); free(hi); return; }
    if (N <= 0) N = 20;
    gmi_opt_brute(optb_t_fv, &s, lo, hi, n, N, out);
    free(lo); free(hi);
}

void gor_optb_basin_hopping(gor_optb_fv f, const double *x0, int n, int niter, double *out)
{
    optb_sv s;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    if (niter <= 0) niter = 50;
    s.f = f;
    gmi_opt_basin_hopping(optb_t_fv, &s, x0, n, niter, out);
}

void gor_optb_dual_annealing(gor_optb_fv f, const double *bounds, int n, int maxiter, double *out)
{
    optb_sv s;
    double *lo, *hi;
    if (f == NULL || bounds == NULL || out == NULL || n <= 0) return;
    if (maxiter <= 0) maxiter = 1000;
    s.f = f;
    optb_unpack_bounds(bounds, n, &lo, &hi);
    if (lo == NULL || hi == NULL) { free(lo); free(hi); return; }
    gmi_opt_dual_annealing(optb_t_fv, &s, lo, hi, n, maxiter, out);
    free(lo); free(hi);
}

void gor_optb_differential_evolution(gor_optb_fv f, const double *bounds, int n, double *out)
{
    optb_sv s;
    double *lo, *hi;
    if (f == NULL || bounds == NULL || out == NULL || n <= 0) return;
    s.f = f;
    optb_unpack_bounds(bounds, n, &lo, &hi);
    if (lo == NULL || hi == NULL) { free(lo); free(hi); return; }
    gmi_de_minimize(optb_t_fv, &s, lo, hi, n, 0, 500, 1e-12, out);
    free(lo); free(hi);
}

/* ------------------------------------------------------------------------- */
/* Sistemi                                                                   */
/* ------------------------------------------------------------------------- */

void gor_optb_fsolve(gor_optb_fvv f, const double *x0, int n, double *out)
{
    optb_svv s;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    s.f = f;
    gmi_opt_fsolve(optb_t_fvv, &s, x0, n, out);
}

void gor_optb_broyden(gor_optb_fvv f, const double *x0, int n, double *out)
{
    optb_svv s;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    s.f = f;
    gmi_opt_broyden(optb_t_fvv, &s, x0, n, out);
}

void gor_optb_least_squares_fit(gor_optb_fvv f, const double *x0, int n, int m,
                                const double *bounds, double *out)
{
    optb_lsq w;
    double *lo, *hi;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    if (m < n) m = n;
    w.f = f; w.n = n; w.m = m;
    w.buf = (double *)calloc((size_t)m, sizeof(double));
    if (w.buf == NULL) return;
    optb_unpack_bounds(bounds, n, &lo, &hi);
    gmi_opt_least_squares(optb_t_lsq, &w, x0, n, m, lo, hi, out);
    free(lo); free(hi); free(w.buf);
}

/* m dedotto: la callback dichiara quanti residui restituisce (valore di ritorno).
 * Buffer grande (512) perché la callback scrive PRIMA di dichiarare la lunghezza. */
#define GOR_OPTB_MAX_RES 512
void gor_optb_least_squares_fit_auto(gor_optb_fvv_len f, const double *x0, int n,
                                    const double *bounds, double *out)
{
    double probe[GOR_OPTB_MAX_RES];
    double m = 0.0;
    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return;
    m = f(x0, (int64_t)n, probe);
    if (m < 1.0) m = (double)n;              /* callback legacy (void): fallback m = n */
    if (m > (double)GOR_OPTB_MAX_RES) m = (double)GOR_OPTB_MAX_RES;
    gor_optb_least_squares_fit((gor_optb_fvv)f, x0, n, (int)m, bounds, out);
}

/* ------------------------------------------------------------------------- */
/* Curve fitting                                                             */
/* ------------------------------------------------------------------------- */

void gor_optb_levenberg_marquardt(gor_optb_fm model, const double *p0, int np,
                                  const double *x, const double *y, int nx, double *out)
{
    optb_sm s;
    if (model == NULL || p0 == NULL || x == NULL || y == NULL || out == NULL) return;
    if (np <= 0 || nx <= 0) return;
    s.f = model;
    gmi_lm_fit(optb_t_fm, &s, x, y, nx, p0, np, 200, 1e-10, out);
}

void gor_optb_levenberg_marquardt_vec(gor_optb_fvm model, const double *p0, int np,
                                      const double *x, const double *y, int nx, double *out)
{
    optb_svm s;
    if (model == NULL || p0 == NULL || x == NULL || y == NULL || out == NULL) return;
    if (np <= 0 || nx <= 0) return;
    s.f = model;
    gmi_lm_fit_vec(optb_t_fvm, &s, x, y, nx, p0, np, 200, 1e-10, out);
}

void gor_optb_curve_fit(gor_optb_fm model, const double *x, const double *y, int nx,
                        const double *p0, int np, double *out)
{
    optb_sm s;
    if (model == NULL || p0 == NULL || x == NULL || y == NULL || out == NULL) return;
    if (np <= 0 || nx <= 0) return;
    s.f = model;
    gmi_lm_fit(optb_t_fm, &s, x, y, nx, p0, np, 500, 1e-12, out);
}

/* ------------------------------------------------------------------------- */
/* Algebra / LP                                                              */
/* ------------------------------------------------------------------------- */

int gor_optb_least_squares(const double *A, int rows, int cols, const double *b, double *out)
{
    const double **rows_ptr = NULL;
    int i, rc;
    if (A == NULL || b == NULL || out == NULL || rows <= 0 || cols <= 0) return GOR_ERR_ARG;
    rows_ptr = (const double **)malloc((size_t)rows * sizeof(double *));
    if (rows_ptr == NULL) return GOR_ERR_NOMEM;
    for (i = 0; i < rows; i++) rows_ptr[i] = A + (size_t)i * (size_t)cols;
    rc = gmi_opt_lin_lstsq(rows_ptr, b, rows, cols, out);
    free(rows_ptr);
    return rc;
}

int gor_optb_cholesky_solve(const double *A, const double *b, int n, double *out)
{
    return gor_opt_cholesky_solve(A, b, n, out);
}

int gor_optb_simplex(const double *c, int n, const double *A, int m, const double *b, double *out)
{
    int rc, j;
    if (c == NULL || A == NULL || b == NULL || out == NULL || n <= 0 || m <= 0) return GOR_ERR_ARG;
    rc = gor_opt_simplex(c, A, m, n, b, out);
    /* parita' con OPTIMIZE.SIMPLEX: out = x[0..n-1] + obiettivo in out[n] */
    out[n] = 0.0;
    if (rc == GOR_OK) for (j = 0; j < n; j++) out[n] += c[j] * out[j];
    return rc;
}

int gor_optb_linprog(const double *c, int n,
                     const double *A_ub, int mub, const double *b_ub,
                     const double *bounds, double *out)
{
    const double **aub = NULL;
    double *lo = NULL, *hi = NULL;
    int i;
    if (c == NULL || out == NULL || n <= 0) return GOR_ERR_ARG;

    if (A_ub != NULL && mub > 0) {
        aub = (const double **)malloc((size_t)mub * sizeof(double *));
        if (aub == NULL) return GOR_ERR_NOMEM;
        for (i = 0; i < mub; i++) aub[i] = A_ub + (size_t)i * (size_t)n;
    }
    optb_unpack_bounds(bounds, n, &lo, &hi);

    gmi_opt_linprog(c, n, aub, b_ub, mub, NULL, NULL, 0, lo, hi, out);

    free(aub); free(lo); free(hi);
    return GOR_OK;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
void gor_optb_basin_hopping__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, int niter, double *out, int64_t out_ext) { gor_optb_basin_hopping(f, x0, n, niter, out); }
void gor_optb_bfgs__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, double *out, int64_t out_ext) { gor_optb_bfgs(f, x0, n, out); }
void gor_optb_broyden__ffi(gor_optb_fvv f, const double *x0, int64_t x0_ext, int n, double *out, int64_t out_ext) { gor_optb_broyden(f, x0, n, out); }
void gor_optb_brute__ffi(gor_optb_fv f, const double *ranges, int64_t ranges_ext, int n, int N, double *out, int64_t out_ext) { gor_optb_brute(f, ranges, n, N, out); }
int gor_optb_cholesky_solve__ffi(const double *A, int64_t A_ext, const double *b, int64_t b_ext, int n, double *out, int64_t out_ext) { return gor_optb_cholesky_solve(A, b, n, out); }
void gor_optb_cobyla__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, const double *bounds, int64_t bounds_ext, double *out, int64_t out_ext) { gor_optb_cobyla(f, x0, n, bounds, out); }
void gor_optb_conjugate_gradient__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, double *out, int64_t out_ext) { gor_optb_conjugate_gradient(f, x0, n, out); }
void gor_optb_curve_fit__ffi(gor_optb_fm model, const double *x, int64_t x_ext, const double *y, int64_t y_ext, int nx, const double *p0, int64_t p0_ext, int np, double *out, int64_t out_ext) { gor_optb_curve_fit(model, x, y, nx, p0, np, out); }
void gor_optb_differential_evolution__ffi(gor_optb_fv f, const double *bounds, int64_t bounds_ext, int n, double *out, int64_t out_ext) { gor_optb_differential_evolution(f, bounds, n, out); }
void gor_optb_dual_annealing__ffi(gor_optb_fv f, const double *bounds, int64_t bounds_ext, int n, int maxiter, double *out, int64_t out_ext) { gor_optb_dual_annealing(f, bounds, n, maxiter, out); }
void gor_optb_fsolve__ffi(gor_optb_fvv f, const double *x0, int64_t x0_ext, int n, double *out, int64_t out_ext) { gor_optb_fsolve(f, x0, n, out); }
void gor_optb_lbfgs__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, double *out, int64_t out_ext) { gor_optb_lbfgs(f, x0, n, out); }
int gor_optb_least_squares__ffi(const double *A, int64_t A_ext, int rows, int cols, const double *b, int64_t b_ext, double *out, int64_t out_ext) { return gor_optb_least_squares(A, rows, cols, b, out); }
void gor_optb_least_squares_fit__ffi(gor_optb_fvv f, const double *x0, int64_t x0_ext, int n, int m, const double *bounds, int64_t bounds_ext, double *out, int64_t out_ext) { gor_optb_least_squares_fit(f, x0, n, m, bounds, out); }
void gor_optb_least_squares_fit_auto__ffi(gor_optb_fvv_len f, const double *x0, int64_t x0_ext, int n, const double *bounds, int64_t bounds_ext, double *out, int64_t out_ext) { gor_optb_least_squares_fit_auto(f, x0, n, bounds, out); }
void gor_optb_levenberg_marquardt__ffi(gor_optb_fm model, const double *p0, int64_t p0_ext, int np, const double *x, int64_t x_ext, const double *y, int64_t y_ext, int nx, double *out, int64_t out_ext) { gor_optb_levenberg_marquardt(model, p0, np, x, y, nx, out); }
void gor_optb_levenberg_marquardt_vec__ffi(gor_optb_fvm model, const double *p0, int64_t p0_ext, int np, const double *x, int64_t x_ext, const double *y, int64_t y_ext, int nx, double *out, int64_t out_ext) { gor_optb_levenberg_marquardt_vec(model, p0, np, x, y, nx, out); }
int gor_optb_linprog__ffi(const double *c, int64_t c_ext, int n, const double *A_ub, int64_t A_ub_ext, int mub, const double *b_ub, int64_t b_ub_ext, const double *bounds, int64_t bounds_ext, double *out, int64_t out_ext) { return gor_optb_linprog(c, n, A_ub, mub, b_ub, bounds, out); }
int gor_optb_minimize__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, const char *method, const double *bounds, int64_t bounds_ext, double *out, int64_t out_ext) { return gor_optb_minimize(f, x0, n, method, bounds, out); }
void gor_optb_nelder_mead__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, double *out, int64_t out_ext) { gor_optb_nelder_mead(f, x0, n, out); }
void gor_optb_powell__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, double *out, int64_t out_ext) { gor_optb_powell(f, x0, n, out); }
int gor_optb_simplex__ffi(const double *c, int64_t c_ext, int n, const double *A, int64_t A_ext, int m, const double *b, int64_t b_ext, double *out, int64_t out_ext) { return gor_optb_simplex(c, n, A, m, b, out); }
void gor_optb_slsqp__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, const double *bounds, int64_t bounds_ext, double *out, int64_t out_ext) { gor_optb_slsqp(f, x0, n, bounds, out); }
void gor_optb_tnc__ffi(gor_optb_fv f, const double *x0, int64_t x0_ext, int n, const double *bounds, int64_t bounds_ext, double *out, int64_t out_ext) { gor_optb_tnc(f, x0, n, bounds, out); }
