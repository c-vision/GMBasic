/*
 * gor_optb.h — superficie FFI delle 34 funzioni del namespace BASIC `OPTIMIZE`.
 *
 * "optb" = optimize-basic: stessi nomi/semantica di `OPTIMIZE.*`, ma con firme
 * adatte al confine FFI:
 *   - array passati come puntatori a double **piatti** + dimensione esplicita
 *     (row-major per le matrici);
 *   - risultati array scritti in un buffer `out` fornito dal chiamante
 *     (niente ritorni composti: l'ABI FFI non li supporta);
 *   - callback nella forma dell'ABI FFI (senza `ctx`, lunghezza `int64_t`):
 *       gor_optb_fx    double (*)(double x)                                  (0-1 scalari)
 *       gor_optb_fv    double (*)(const double *x, int64_t n)                 (array -> scalare)
 *       gor_optb_fvv   void   (*)(const double *x, int64_t n, double *out)    (array -> array)
 *       gor_optb_fm    double (*)(const double *p, int64_t np, double x)      (modello LM)
 *       gor_optb_fvm   void   (*)(const double *p, int64_t np,
 *                                 const double *x, int64_t nx, double *out)   (modello LM vettoriale)
 *
 * `bounds`/`ranges` sono sempre appiattiti: n coppie (lo0, hi0, lo1, hi1, ...),
 * NULL = illimitato. Le funzioni che restituiscono `int` ritornano 0 = OK.
 */
#ifndef GMBORTOOLS_GOR_OPTB_H
#define GMBORTOOLS_GOR_OPTB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef double (*gor_optb_fx)(double x);
typedef double (*gor_optb_fv)(const double *x, int64_t n);
typedef void (*gor_optb_fvv)(const double *x, int64_t n, double *out);
typedef double (*gor_optb_fm)(const double *p, int64_t np, double x);
typedef void (*gor_optb_fvm)(const double *p, int64_t np, const double *x, int64_t nx, double *out);

/* ── 1D: radici e minimo (ritorno scalare) ─────────────────────────────────── */
double gor_optb_newton(gor_optb_fx f, double guess);
double gor_optb_bisection(gor_optb_fx f, double a, double b);
double gor_optb_brent(gor_optb_fx f, double a, double b);
double gor_optb_secant(gor_optb_fx f, double x0, double x1);
double gor_optb_golden(gor_optb_fx f, double a, double b);
double gor_optb_brentq(gor_optb_fx f, double a, double b);
double gor_optb_brenth(gor_optb_fx f, double a, double b);
double gor_optb_ridder(gor_optb_fx f, double a, double b);
double gor_optb_toms748(gor_optb_fx f, double a, double b);
double gor_optb_brentmin(gor_optb_fx f, double a, double b);
double gor_optb_minimize_scalar(gor_optb_fx f, double a, double b, const char *method);
/* Come sopra, metodo di default (brent). */
double gor_optb_minimize_scalar_default(gor_optb_fx f, double a, double b);

/* ── minimizzazione multivariata: out[n] ───────────────────────────────────── */
void gor_optb_nelder_mead(gor_optb_fv f, const double *x0, int n, double *out);
void gor_optb_bfgs(gor_optb_fv f, const double *x0, int n, double *out);
void gor_optb_lbfgs(gor_optb_fv f, const double *x0, int n, double *out);
void gor_optb_conjugate_gradient(gor_optb_fv f, const double *x0, int n, double *out);
void gor_optb_powell(gor_optb_fv f, const double *x0, int n, double *out);
void gor_optb_tnc(gor_optb_fv f, const double *x0, int n, const double *bounds, double *out);
void gor_optb_cobyla(gor_optb_fv f, const double *x0, int n, const double *bounds, double *out);
void gor_optb_slsqp(gor_optb_fv f, const double *x0, int n, const double *bounds, double *out);
int gor_optb_minimize(gor_optb_fv f, const double *x0, int n,
                      const char *method, const double *bounds, double *out);

/* ── globali ───────────────────────────────────────────────────────────────── */
void gor_optb_brute(gor_optb_fv f, const double *ranges, int n, int N, double *out);
void gor_optb_basin_hopping(gor_optb_fv f, const double *x0, int n, int niter, double *out);
void gor_optb_dual_annealing(gor_optb_fv f, const double *bounds, int n, int maxiter, double *out);
void gor_optb_differential_evolution(gor_optb_fv f, const double *bounds, int n, double *out);

/* ── sistemi e minimi quadrati non lineari ─────────────────────────────────── */
void gor_optb_fsolve(gor_optb_fvv f, const double *x0, int n, double *out);
void gor_optb_broyden(gor_optb_fvv f, const double *x0, int n, double *out);
/* m = numero di residui (>= n); out[m] */
void gor_optb_least_squares_fit(gor_optb_fvv f, const double *x0, int n, int m,
                                const double *bounds, double *out);

/* ── curve fitting (Levenberg-Marquardt) — out[np] ─────────────────────────── */
void gor_optb_levenberg_marquardt(gor_optb_fm model, const double *p0, int np,
                                  const double *x, const double *y, int nx, double *out);
void gor_optb_levenberg_marquardt_vec(gor_optb_fvm model, const double *p0, int np,
                                      const double *x, const double *y, int nx, double *out);
void gor_optb_curve_fit(gor_optb_fm model, const double *x, const double *y, int nx,
                        const double *p0, int np, double *out);

/* ── algebra lineare / programmazione lineare ──────────────────────────────── */
/* Minimi quadrati lineari: A (rows×cols, row-major), b[rows] -> out[cols]. */
int gor_optb_least_squares(const double *A, int rows, int cols, const double *b, double *out);
/* Cholesky: A (n×n simmetrica definita positiva, row-major), b[n] -> out[n]. */
int gor_optb_cholesky_solve(const double *A, const double *b, int n, double *out);
/* SIMPLEX: **max** c'x s.t. A x <= b, x >= 0. A (m×n). out[n+1] = x + obiettivo. */
int gor_optb_simplex(const double *c, int n, const double *A, int m, const double *b, double *out);
/* LINPROG: minimizza c'x con vincoli A_ub x <= b_ub (mub×n, row-major) e bounds
 * (n coppie lo,hi). Le uguaglianze vanno espresse come due disuguaglianze dal
 * chiamante (il binding BASIC le espande): cosi' la firma resta entro gli 8
 * argomenti dell'ABI FFI. out[n]. */
int gor_optb_linprog(const double *c, int n,
                     const double *A_ub, int mub, const double *b_ub,
                     const double *bounds, double *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_OPTB_H */
