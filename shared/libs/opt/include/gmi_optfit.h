/*
 * gmi_optfit.h — dichiarazioni pubbliche del motore legacy gmi_optfit.c.
 *
 * Le funzioni erano definite senza header: i chiamanti usavano dichiarazioni
 * implicite. Qui si espone la superficie usata dagli adattatori scipy-like di
 * gor_optimize. Stessi tipi, senza typedef locali (il .c ha i suoi).
 */
#ifndef GMBORTOOLS_GMI_OPTFIT_H
#define GMBORTOOLS_GMI_OPTFIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* curve fitting (Levenberg-Marquardt, Jacobiano numerico).
 * model(p, np, x, ctx) -> y. */
void gmi_lm_fit(double (*model)(const double *, int, double, void *), void *ctx,
                const double *x, const double *y, int nx,
                const double *p0, int np, int maxiter, double tol, double *out);

/* Variante vettoriale del modello per curve fitting:
 * vmodel(p, np, x, nx, out, ctx) riempie out[nx]. */
typedef void (*gmi_lm_vmodel_fn)(const double *p, int np, const double *x, int nx,
                                 double *out, void *ctx);
void gmi_lm_fit_vec(gmi_lm_vmodel_fn vmodel, void *vctx,
                    const double *x, const double *y, int nx,
                    const double *p0, int np, int maxiter, double tol, double *out);

/* differential_evolution (DE/rand/1/bin, seed fisso) sui bounds [lo, hi]
 * (lunghezza n; NULL = illimitato). out[n]. */
void gmi_de_minimize(double (*fn)(const double *, int, void *), void *ctx,
                     const double *lo, const double *hi, int n,
                     int np, int maxiter, double tol, double *out);

/* radici scalari */
double gmi_opt_brentq(double (*f)(double, void *), void *ctx, double a, double b);
double gmi_opt_brenth(double (*f)(double, void *), void *ctx, double a, double b);
double gmi_opt_ridder(double (*f)(double, void *), void *ctx, double a, double b);
double gmi_opt_toms748(double (*f)(double, void *), void *ctx, double a, double b);
double gmi_opt_brentmin(double (*f)(double, void *), void *ctx, double lower, double upper);

/* minimizzazione multivariata: f(v, n, ctx) -> scalare */
void gmi_opt_powell(double (*f)(const double *, int, void *), void *ctx,
                    const double *x0, int n, double *out);
void gmi_opt_tnc(double (*f)(const double *, int, void *), void *ctx,
                 const double *x0, int n, const double *lo, const double *hi, double *out);
void gmi_opt_cobyla(double (*f)(const double *, int, void *), void *ctx,
                    const double *x0, int n, const double *lo, const double *hi, double *out);
void gmi_opt_slsqp(double (*f)(const double *, int, void *), void *ctx,
                   const double *x0, int n, const double *lo, const double *hi, double *out);
void gmi_opt_brute(double (*f)(const double *, int, void *), void *ctx,
                   const double *lo, const double *hi, int n, int N, double *out);
void gmi_opt_basin_hopping(double (*f)(const double *, int, void *), void *ctx,
                           const double *x0, int n, int niter, double *out);
void gmi_opt_dual_annealing(double (*f)(const double *, int, void *), void *ctx,
                            const double *lo, const double *hi, int n, int maxiter, double *out);

/* sistemi: f(x, n, out, ctx) */
void gmi_opt_fsolve(void (*f)(const double *, int, double *, void *), void *ctx,
                    const double *x0, int n, double *out);
void gmi_opt_broyden(void (*f)(const double *, int, double *, void *), void *ctx,
                     const double *x0, int n, double *out);
void gmi_opt_least_squares(void (*r)(const double *, int, double *, void *), void *ctx,
                           const double *x0, int n, int m,
                           const double *lo, const double *hi, double *out);

/* algebra / LP */
int gmi_opt_lin_lstsq(const double *const *a, const double *b, int rows, int cols, double *x);
void gmi_opt_linprog(const double *c, int n, const double **aub, const double *bub, int mub,
                     const double **aeq, const double *beq, int meq,
                     const double *lo, const double *hi, double *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GMI_OPTFIT_H */
