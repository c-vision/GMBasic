/* ============================================================================
 * gmb_optfit.c — scipy.optimize nel runtime C: curve_fit (Levenberg-Marquardt con
 * Jacobiano numerico) e differential_evolution (DE/rand/1/bin). Callback context-aware
 * (la VM/C passa vm/task/closure). Porting di CommonLib.OptimizeFit.cs.
 * ============================================================================ */

#include "gmb_runtime.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double gmb_l2_sq(const double* v, int n)
{
    double s = 0;
    for (int i = 0; i < n; i++) s += v[i] * v[i];
    return s;
}

/* Soluzione di un sistema lineare np×np con eliminazione di Gauss (pivot parziale).
 * a è np×np (row-major), b è np; a viene distrutta. Ritorna 1 se ok. */
static int gmb_linsolve(double* a, double* b, int n, double* out)
{
    for (int col = 0; col < n; col++)
    {
        int piv = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(a[r * n + col]) > fabs(a[piv * n + col])) piv = r;
        if (piv != col)
        {
            for (int j = col; j < n; j++) { double t = a[col * n + j]; a[col * n + j] = a[piv * n + j]; a[piv * n + j] = t; }
            double tb = b[col]; b[col] = b[piv]; b[piv] = tb;
        }
        if (fabs(a[col * n + col]) < 1e-300) return 0;
        for (int r = col + 1; r < n; r++)
        {
            double f = a[r * n + col] / a[col * n + col];
            for (int j = col; j < n; j++) a[r * n + j] -= f * a[col * n + j];
            b[r] -= f * b[col];
        }
    }
    for (int r = n - 1; r >= 0; r--)
    {
        double s = b[r];
        for (int j = r + 1; j < n; j++) s -= a[r * n + j] * out[j];
        out[r] = s / a[r * n + r];
    }
    return 1;
}

/* ── curve_fit: Levenberg-Marquardt con Jacobiano numerico (scipy curve_fit, popt) ──
 * model(p, np, x, ctx) → previsione. Out: np parametri ottimizzati. */
/* ── LM: replica di CommonLib.Numerics.LevenbergMarquardt(Vec) via MathNet
 * NonlinearObjectiveFunction (accuracyOrder=2, weights=1, no bounds) +
 * LevenbergMarquardtMinimizer (initialMu, gtol/stol/ftol, max=200*(np+1)). ── */
typedef void (*gmb_lm_vmodel_fn)(const double* p, int np, const double* x, int nx, double* out, void* ctx);
/* Adapter: modello scalare f(p, x_i) → vettore previsioni. */
typedef struct { double (*model)(const double*, int, double, void*); void* ctx; } gmb_lm_scalar_ctx;
static void gmb_lm_scalar_as_vec(const double* p, int np, const double* x, int nx, double* out, void* ctx)
{
    gmb_lm_scalar_ctx* w = (gmb_lm_scalar_ctx*)ctx;
    for (int i = 0; i < nx; i++) out[i] = w->model(p, np, x[i], w->ctx);
}
static double gmb_lm_eval_vec(gmb_lm_vmodel_fn vmodel, void* vctx,
                              const double* x, const double* y, int nx, const double* P, int np,
                              double* R, double* mv)
{
    vmodel(P, np, x, nx, mv, vctx);
    double rss = 0;
    for (int i = 0; i < nx; i++) { double r = y[i] - mv[i]; R[i] = r; rss += r * r; }
    return rss;
}
static void gmb_lm_jac_vec(gmb_lm_vmodel_fn vmodel, void* vctx,
                           const double* x, int nx, const double* P, int np,
                           const double* R, double* J, double* g, double* H, double* mp, double* mm)
{
    const double sqrtEps = 1.4901161193847656250E-8;
    double* pp = (double*)malloc(sizeof(double) * (size_t)np);
    for (int j = 0; j < np; j++) pp[j] = P[j];
    for (int j = 0; j < np; j++)
    {
        double d = 0.000003 * fmax(fabs(P[j]), sqrtEps);   /* central diff, accuracyOrder=2 */
        pp[j] = P[j] + d; vmodel(pp, np, x, nx, mp, vctx);
        pp[j] = P[j] - d; vmodel(pp, np, x, nx, mm, vctx);
        pp[j] = P[j];
        for (int i = 0; i < nx; i++) J[i * np + j] = (mp[i] - mm[i]) / (2.0 * d);
    }
    free(pp);
    for (int j = 0; j < np; j++)
    {
        double s = 0;
        for (int i = 0; i < nx; i++) s += J[i * np + j] * R[i];
        g[j] = -s;   /* g = -J'R */
        for (int k = 0; k < np; k++)
        {
            double a = 0;
            for (int i = 0; i < nx; i++) a += J[i * np + j] * J[i * np + k];
            H[j * np + k] = a;   /* H = J'J */
        }
    }
}
static void gmb_lm_fit_core(gmb_lm_vmodel_fn vmodel, void* vctx,
                            const double* x, const double* y, int nx,
                            const double* p0, int np, double initialMu, double tol, double* out)
{
    if (np <= 0) return;
    double gtol = tol, stol = tol, ftol = tol;
    int maxIterations = 200 * (np + 1);
    double* P = (double*)malloc(sizeof(double) * (size_t)np);
    double* Pstep = (double*)malloc(sizeof(double) * (size_t)np);
    double* Pnew = (double*)malloc(sizeof(double) * (size_t)np);
    double* R = (double*)malloc(sizeof(double) * (size_t)(nx > 0 ? nx : 1));
    double* Rnew = (double*)malloc(sizeof(double) * (size_t)(nx > 0 ? nx : 1));
    double* mv = (double*)malloc(sizeof(double) * (size_t)(nx > 0 ? nx : 1));
    double* mp = (double*)malloc(sizeof(double) * (size_t)(nx > 0 ? nx : 1));
    double* mm = (double*)malloc(sizeof(double) * (size_t)(nx > 0 ? nx : 1));
    double* J = (double*)malloc(sizeof(double) * (size_t)(nx > 0 ? nx : 1) * (size_t)np);
    double* g = (double*)malloc(sizeof(double) * (size_t)np);
    double* H = (double*)malloc(sizeof(double) * (size_t)np * (size_t)np);
    double* A = (double*)malloc(sizeof(double) * (size_t)np * (size_t)np);
    double* bb = (double*)malloc(sizeof(double) * (size_t)np);
    if (!P || !Pstep || !Pnew || !R || !Rnew || !mv || !mp || !mm || !J || !g || !H || !A || !bb) goto lm_done;
    for (int i = 0; i < np; i++) P[i] = p0[i];
    double RSS = gmb_lm_eval_vec(vmodel, vctx, x, y, nx, P, np, R, mv);
    if (!(RSS == RSS) || RSS <= ftol) goto lm_done;
    gmb_lm_jac_vec(vmodel, vctx, x, nx, P, np, R, J, g, H, mp, mm);
    {
        double ginf = 0; for (int j = 0; j < np; j++) ginf = fmax(ginf, fabs(g[j]));
        if (ginf <= gtol) goto lm_done;
    }
    {
        double dmax = 0; for (int j = 0; j < np; j++) dmax = fmax(dmax, H[j * np + j]);
        double mu = initialMu * dmax;
        double nu = 2;
        int iterations = 0, exitc = 0;
        while (iterations < maxIterations && !exitc)
        {
            iterations++;
            for (;;)
            {
                memcpy(A, H, sizeof(double) * (size_t)np * (size_t)np);
                for (int j = 0; j < np; j++) { A[j * np + j] += mu; bb[j] = -g[j]; }
                if (gmb_linsolve(A, bb, np, Pstep) == 0) { exitc = 1; break; }
                double pn = 0, pl = 0;
                for (int j = 0; j < np; j++) { pn += Pstep[j] * Pstep[j]; pl += P[j] * P[j]; }
                if (sqrt(pn) <= stol * (sqrt(pl) + stol)) { exitc = 1; break; }
                for (int j = 0; j < np; j++) Pnew[j] = P[j] + Pstep[j];
                double RSSnew = gmb_lm_eval_vec(vmodel, vctx, x, y, nx, Pnew, np, Rnew, mv);
                if (!(RSSnew == RSSnew)) { exitc = 1; break; }
                double pred = 0;
                for (int j = 0; j < np; j++) pred += Pstep[j] * (mu * Pstep[j] - g[j]);
                double rho = (pred != 0.0) ? (RSS - RSSnew) / pred : 0.0;
                if (rho > 0.0)
                {
                    memcpy(P, Pnew, sizeof(double) * (size_t)np);
                    RSS = RSSnew; memcpy(R, Rnew, sizeof(double) * (size_t)nx);
                    gmb_lm_jac_vec(vmodel, vctx, x, nx, P, np, R, J, g, H, mp, mm);
                    double ginf = 0; for (int j = 0; j < np; j++) ginf = fmax(ginf, fabs(g[j]));
                    if (ginf <= gtol) exitc = 1;
                    if (RSS <= ftol) exitc = 1;
                    mu *= fmax(1.0 / 3.0, 1.0 - pow(2.0 * rho - 1.0, 3.0));
                    nu = 2;
                    break;
                }
                else { mu *= nu; nu *= 2; }
            }
        }
    }
lm_done:
    for (int i = 0; i < np; i++) out[i] = P ? P[i] : p0[i];
    free(P); free(Pstep); free(Pnew); free(R); free(Rnew); free(mv); free(mp); free(mm); free(J); free(g); free(H); free(A); free(bb);
}
void gmb_lm_fit(double (*model)(const double*, int, double, void*), void* ctx,
                const double* x, const double* y, int nx,
                const double* p0, int np, int maxiter, double tol, double* out)
{
    gmb_lm_scalar_ctx w = { model, ctx };
    gmb_lm_fit_core(gmb_lm_scalar_as_vec, &w, x, y, nx, p0, np, 1e-3, tol, out);
}
void gmb_lm_fit_vec(gmb_lm_vmodel_fn vmodel, void* vctx,
                    const double* x, const double* y, int nx,
                    const double* p0, int np, int maxiter, double tol, double* out)
{
    gmb_lm_fit_core(vmodel, vctx, x, y, nx, p0, np, 1e-3, tol, out);
}

/* ── Bridge C99: adatta una lambda BASIC transpilata (gmb_fn_t) al callback del runtime.
   La lambda ha firma `double (void* ctx, gmb_double_t* arr[, gmb_double_t x])`. ── */
static double gmb_opt_c99_f_vec(const double* v, int n, void* ctx)
{
    gmb_fn_t fn = *(const gmb_fn_t*)ctx;
    gmb_int_t prev = gmb_lambda_bound(0);
    double r;
    gmb_lambda_bound_set(0, (gmb_int_t)n - 1);
    r = ((double(*)(void*, double*))fn.fn)(fn.ctx, (double*)v);
    gmb_lambda_bound_set(0, prev);
    return r;
}
static double gmb_opt_c99_f_scalar(double x, void* ctx)
{
    gmb_fn_t fn = *(const gmb_fn_t*)ctx;
    return ((double(*)(void*, double))fn.fn)(fn.ctx, x);
}
static double gmb_opt_c99_lm_model(const double* p, int np, double x, void* ctx)
{
    gmb_fn_t fn = *(const gmb_fn_t*)ctx;
    gmb_int_t prev = gmb_lambda_bound(0);
    double r;
    gmb_lambda_bound_set(0, (gmb_int_t)np - 1);
    r = ((double(*)(void*, double*, double))fn.fn)(fn.ctx, (double*)p, x);
    gmb_lambda_bound_set(0, prev);
    return r;
}
/* Modello vettoriale C99: lambda `void f(void* ctx, double* p, double* x, double* out)`. */
static void gmb_opt_c99_lm_model_vec(const double* p, int np, const double* x, int nx, double* out, void* ctx)
{
    gmb_fn_t fn = *(const gmb_fn_t*)ctx;
    gmb_int_t b0 = gmb_lambda_bound(0), b1 = gmb_lambda_bound(1);
    gmb_lambda_bound_set(0, (gmb_int_t)np - 1);
    gmb_lambda_bound_set(1, (gmb_int_t)nx - 1);
    ((void(*)(void*, double*, double*, double*))fn.fn)(fn.ctx, (double*)p, (double*)x, out);
    gmb_lambda_bound_set(0, b0);
    gmb_lambda_bound_set(1, b1);
}

/* Entry C99: la VM/C usa il bridge proprio; qui il contesto è direttamente la lambda.
   Allocano e ritornano il double* (pattern delle altre array-builtin C99), *outn = n. */
double* gmb_opt_de_c99(gmb_fn_t fn, gmb_double_t** bounds, int n, gmb_int_t* outn)
{
    if (n <= 0 || !bounds || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    double lo[32], hi[32];
    for (int i = 0; i < n; i++) { lo[i] = bounds[i][0]; hi[i] = bounds[i][1]; }
    gmb_de_minimize(gmb_opt_c99_f_vec, &fn, lo, hi, n, 0, 1000, 1e-6, out);
    *outn = n;
    return out;
}

double* gmb_opt_lmfit_c99(gmb_fn_t fn, const double* x, int nx, const double* y, int ny,
                          const double* p0, int np, gmb_int_t* outn)
{
    if (np <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)np);
    if (!out) { *outn = 0; return NULL; }
    gmb_lm_fit(gmb_opt_c99_lm_model, &fn, x, y, nx, p0, np, 200, 1e-8, out);
    *outn = np;
    return out;
}


static uint64_t g_de_state = 0x123456789abcdefULL;
static double gmb_de_rnd(void)
{
    g_de_state ^= g_de_state << 13;
    g_de_state ^= g_de_state >> 7;
    g_de_state ^= g_de_state << 17;
    return (double)(g_de_state & 0xFFFFFFFFFFFF) / (double)0x1000000000000;
}

/* ── differential_evolution: DE/rand/1/bin deterministico (seed fisso) ──
 * fn(v, n, ctx) → scalare da minimizzare sui bounds [lo, hi]. Out: n ottimi. */
void gmb_de_minimize(double (*fn)(const double*, int, void*), void* ctx,
                     const double* lo, const double* hi, int n,
                     int np, int maxiter, double tol, double* out)
{
    if (n <= 0) return;
    if (np <= 0) np = 10 * n > 20 ? 10 * n : 20;
    g_de_state = 0x123456789abcdefULL;
    double** pop = (double**)malloc(sizeof(double*) * (size_t)np);
    double* fit = (double*)malloc(sizeof(double) * (size_t)np);
    double* trial = (double*)malloc(sizeof(double) * (size_t)n);
    if (!pop || !fit || !trial) { free(pop); free(fit); free(trial); return; }
    for (int i = 0; i < np; i++)
    {
        pop[i] = (double*)malloc(sizeof(double) * (size_t)n);
        for (int j = 0; j < n; j++) pop[i][j] = lo[j] + gmb_de_rnd() * (hi[j] - lo[j]);
        fit[i] = fn(pop[i], n, ctx);
    }
    const double F = 0.8, CR = 0.9;
    for (int gen = 0; gen < maxiter; gen++)
    {
        double bestNow = fit[0];
        for (int i = 1; i < np; i++) if (fit[i] < bestNow) bestNow = fit[i];
        if (bestNow < tol) break;
        for (int i = 0; i < np; i++)
        {
            int r1, r2, r3;
            do { r1 = (int)(gmb_de_rnd() * np); if (r1 >= np) r1 = np - 1; } while (r1 == i);
            do { r2 = (int)(gmb_de_rnd() * np); if (r2 >= np) r2 = np - 1; } while (r2 == i || r2 == r1);
            do { r3 = (int)(gmb_de_rnd() * np); if (r3 >= np) r3 = np - 1; } while (r3 == i || r3 == r1 || r3 == r2);
            int jrand = (int)(gmb_de_rnd() * n); if (jrand >= n) jrand = n - 1;
            for (int j = 0; j < n; j++)
            {
                double u = (gmb_de_rnd() < CR || j == jrand)
                    ? pop[r1][j] + F * (pop[r2][j] - pop[r3][j])
                    : pop[i][j];
                if (u < lo[j]) u = lo[j];
                if (u > hi[j]) u = hi[j];
                trial[j] = u;
            }
            double tf = fn(trial, n, ctx);
            if (tf <= fit[i])
            {
                for (int j = 0; j < n; j++) pop[i][j] = trial[j];
                fit[i] = tf;
            }
        }
    }
    double bestF = fit[0];
    int bestI = 0;
    for (int i = 1; i < np; i++) if (fit[i] < bestF) { bestF = fit[i]; bestI = i; }
    for (int j = 0; j < n; j++) out[j] = pop[bestI][j];
    for (int i = 0; i < np; i++) free(pop[i]);
    free(pop); free(fit); free(trial);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Completamento scipy.optimize (C): radici scalari, minimize, fsolve,
 * least_squares, linprog, globali. Callback:
 *   scalare:        double f(double x, void* ctx)
 *   vettore→scalare: double f(const double* v, int n, void* ctx)
 *   vettore→vettore: void  f(const double* v, int n, double* out, void* ctx)
 * ════════════════════════════════════════════════════════════════════════════ */

static double gmb_opt_sign(double x) { return x == 0 ? 0.0 : (x > 0 ? 1.0 : -1.0); }
static double gmb_opt_sq(double x) { return x * x; }

/* ---- radici scalari ---- */
double gmb_opt_brentq(double (*f)(double, void*), void* ctx, double a, double b)
{
    double fa = f(a, ctx), fb = f(b, ctx);
    if (fa * fb > 0) return NAN;
    double c = a, fc = fa, d = b - a, e = d;
    for (int it = 0; it < 200; it++)
    {
        if (fabs(fc) < fabs(fb)) { a = b; b = c; c = a; fa = fb; fb = fc; fc = fa; }
        double tol = 2.0 * 1e-14 * fabs(b) + 1e-13;
        double m = 0.5 * (c - b);
        if (fabs(m) <= tol || fb == 0) return b;
        double p, q;
        if (fabs(e) < tol || fabs(fa) <= fabs(fb)) { e = d = m; }
        else
        {
            double s = fb / fa;
            if (a == c) { p = 2.0 * m * s; q = 1.0 - s; }
            else
            {
                q = fa / fc;
                double r = fb / fc;
                p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0) q = -q;
            p = fabs(p);
            double min1 = 3.0 * m * q - fabs(tol * q), min2 = fabs(e * q);
            if (2.0 * p < (min1 < min2 ? min1 : min2)) { e = d; d = p / q; }
            else { d = m; e = m; }
        }
        a = b; fa = fb;
        b += fabs(d) > tol ? d : (m > 0 ? tol : -tol);
        fb = f(b, ctx);
        if ((fb > 0) == (fc > 0)) { c = a; fc = fa; e = d = b - a; }
    }
    return b;
}

double gmb_opt_brenth(double (*f)(double, void*), void* ctx, double a, double b)
{
    double fa = f(a, ctx), fb = f(b, ctx);
    if (fa * fb > 0) return NAN;
    double c = a, fc = fa, d = b - a, e = d;
    for (int it = 0; it < 200; it++)
    {
        if (fabs(fc) < fabs(fb)) { a = b; b = c; c = a; fa = fb; fb = fc; fc = fa; }
        double tol = 2.0 * 1e-14 * fabs(b) + 1e-13;
        double m = 0.5 * (c - b);
        if (fabs(m) <= tol || fb == 0) return b;
        double p, q;
        if (fabs(e) < tol || fabs(fa) <= fabs(fb)) { e = d = m; }
        else
        {
            double s = fb / fa;
            if (a == c) { p = 2.0 * m * s; q = 1.0 - s; }
            else
            {
                q = fa / fc;
                double r = fb / fc;
                p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0) q = -q;
            p = fabs(p);
            double min1 = 3.0 * m * q - fabs(tol * q), min2 = fabs(e * q);
            if (2.0 * p < (min1 < min2 ? min1 : min2)) { e = d; d = p / q; }
            else { d = m; e = m; }
        }
        a = b; fa = fb;
        b += fabs(d) > tol ? d : (m > 0 ? tol : -tol);
        fb = f(b, ctx);
        if ((fb > 0) == (fc > 0)) { c = a; fc = fa; e = d = b - a; }
    }
    return b;
}

double gmb_opt_ridder(double (*f)(double, void*), void* ctx, double a, double b)
{
    double fa = f(a, ctx), fb = f(b, ctx);
    if (fa * fb > 0) return NAN;
    for (int it = 0; it < 100; it++)
    {
        double m = 0.5 * (a + b), fm = f(m, ctx);
        double s = sqrt(fm * fm - fa * fb);
        if (s == 0) return m;
        double dx = (m - a) * fm / s;
        double x = (fa - fb < 0) ? m - dx : m + dx;
        double fx = f(x, ctx);
        if (fabs(fx) < 1e-12 || fabs(b - a) < 1e-13) return x;
        if (fx * fm < 0) { a = m; fa = fm; b = x; fb = fx; }
        else if (fx * fa < 0) { b = x; fb = fx; }
        else { a = x; fa = fx; }
    }
    return 0.5 * (a + b);
}

double gmb_opt_toms748(double (*f)(double, void*), void* ctx, double a, double b)
{
    double fa = f(a, ctx), fb = f(b, ctx);
    if (fa * fb > 0) return NAN;
    if (fabs(fa) < fabs(fb)) { double t = a; a = b; b = t; t = fa; fa = fb; fb = t; }
    double c = a, fc = fa, d = b - a, e = d;
    for (int it = 0; it < 100; it++)
    {
        double tol = 2.0 * 1e-14 * fabs(b) + 1e-13;
        double m = 0.5 * (c - b);
        if (fabs(m) <= tol || fb == 0) return b;
        double p, q;
        if (fabs(e) < tol || fabs(fa) <= fabs(fb)) { e = d = m; }
        else
        {
            double s = fb / fa;
            if (a == c) { p = 2.0 * m * s; q = 1.0 - s; }
            else
            {
                q = fa / fc;
                double r = fb / fc;
                p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0) q = -q;
            p = fabs(p);
            double min1 = 3.0 * m * q - fabs(tol * q), min2 = fabs(e * q);
            if (2.0 * p < (min1 < min2 ? min1 : min2)) { e = d; d = p / q; }
            else { d = m; e = m; }
        }
        a = b; fa = fb;
        b += fabs(d) > tol ? d : (m > 0 ? tol : -tol);
        fb = f(b, ctx);
        if ((fb > 0) == (fc > 0)) { c = a; fc = fa; e = d = b - a; }
    }
    return b;
}

/* ---- minimizzatore scalare (brent) ---- */
double gmb_opt_brentmin(double (*f)(double, void*), void* ctx, double lower, double upper)
{
    const double golden = 0.3819660112501051;
    double a = lower, b = upper;
    double x = a + golden * (b - a), w = x, v = x;
    double fx = f(x, ctx), fw = fx, fv = fx;
    double d = 0, e = 0;
    for (int it = 0; it < 200; it++)
    {
        double tol = 1e-8 * fabs(x) + 1e-12;
        double m = 0.5 * (a + b);
        if (fabs(x - m) <= 2.0 * tol - 0.5 * (b - a)) return x;
        double u, p = 0, q = 0;
        if (fabs(e) > tol)
        {
            double r = (x - w) * (fx - fv);
            q = (x - v) * (fx - fw);
            p = (x - v) * q - (x - w) * r;
            q = 2.0 * (q - r);
            if (q > 0) p = -p;
            q = fabs(q);
            double etemp = e;
            e = d;
            if (fabs(p) < fabs(0.5 * q * etemp) && p > q * (a - x) && p < q * (b - x))
            {
                d = p / q;
                u = x + d;
                if (u - a < 2 * tol || b - u < 2 * tol) d = (x < m ? 1 : -1) * tol;
            }
            else { e = (x < m ? b - x : a - x); d = golden * e; }
        }
        else { e = (x < m ? b - x : a - x); d = golden * e; }
        u = fabs(d) >= tol ? x + d : x + (d > 0 ? tol : -tol);
        double fu = f(u, ctx);
        if (fu <= fx)
        {
            if (u < x) b = x; else a = x;
            v = w; w = x; x = u;
            fv = fw; fw = fx; fx = fu;
        }
        else
        {
            if (u < x) a = u; else b = u;
            if (fu <= fw || w == x) { v = w; w = u; fv = fw; fw = fu; }
            else if (fu <= fv || v == x || v == w) { v = u; fv = fu; }
        }
    }
    return x;
}

/* ---- vettore→scalare: powell / tnc / cobyla / slsqp ---- */
typedef double (*gmb_opt_fvec_fn)(const double* v, int n, void* ctx);

static void gmb_opt_clamp(double* x, const double* lo, const double* hi, int n)
{
    for (int i = 0; i < n; i++)
        if (lo && !isnan(lo[i])) x[i] = x[i] < lo[i] ? lo[i] : x[i];
    for (int i = 0; i < n; i++)
        if (hi && !isnan(hi[i])) x[i] = x[i] > hi[i] ? hi[i] : x[i];
}

typedef struct { gmb_opt_fvec_fn f; void* ctx; const double* x; const double* dir; int n; double* buf; } gmb_opt_lmctx_t;
static double gmb_opt_lm_g(double t, void* c)
{
    gmb_opt_lmctx_t* g = (gmb_opt_lmctx_t*)c;
    for (int i = 0; i < g->n; i++) g->buf[i] = g->x[i] + t * g->dir[i];
    return g->f(g->buf, g->n, g->ctx);
}
static double gmb_opt_line_min(gmb_opt_fvec_fn f, void* ctx, const double* x, const double* dir, int n)
{
    /* Campionamento grossolano + rifinitura brent. Campioni SIMMETRICI (2026-11-03):
     * il minimo lungo la direzione puo' stare a t<0 (es. Powell su f=(x-1)^2+(y+2)^2 da
     * (0,0): la direzione +y richiede t=-2). Con soli t>=0 la ricerca si bloccava a t=0. */
    double ts[17] = { -50.0, -20.0, -10.0, -5.0, -2.0, -1.0, -0.5, -0.2, 0.0,
                      0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0 };
    double* buf = (double*)malloc(sizeof(double) * (size_t)n);
    double bestT, bestG, lo, hi;
    if (!buf) return 0;
    gmb_opt_lmctx_t g = { f, ctx, x, dir, n, buf };
    bestT = 0;
    bestG = gmb_opt_lm_g(0, &g);
    for (int i = 0; i < 17; i++)
    {
        double gv = gmb_opt_lm_g(ts[i], &g);
        if (gv < bestG) { bestG = gv; bestT = ts[i]; }
    }
    free(buf);
    if (bestT == 0) return 0;
    buf = (double*)malloc(sizeof(double) * (size_t)n);
    if (!buf) return 0;
    g.buf = buf;
    if (bestT > 0) { lo = bestT * 0.5; hi = bestT * 2.0; }
    else { lo = bestT * 2.0; hi = bestT * 0.5; }
    double res = gmb_opt_brentmin(gmb_opt_lm_g, &g, lo, hi);
    free(buf);
    return res;
}

void gmb_opt_powell(gmb_opt_fvec_fn f, void* ctx, const double* x0, int n, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    double* start = (double*)malloc(sizeof(double) * (size_t)n);
    double* nd = (double*)malloc(sizeof(double) * (size_t)n);
    double** dirs = (double**)malloc(sizeof(double*) * (size_t)n);
    for (int i = 0; i < n; i++) { x[i] = x0[i]; dirs[i] = (double*)calloc((size_t)n, sizeof(double)); dirs[i][i] = 1.0; }
    for (int it = 0; it < 200; it++)
    {
        for (int i = 0; i < n; i++) start[i] = x[i];
        double prevF = f(x, n, ctx);
        double bestRed = 0;
        int maxIdx = -1;
        for (int i = 0; i < n; i++)
        {
            double fBefore = f(x, n, ctx);
            double t = gmb_opt_line_min(f, ctx, x, dirs[i], n);
            for (int k = 0; k < n; k++) x[k] += t * dirs[i][k];
            double red = fBefore - f(x, n, ctx);
            if (red > bestRed) { bestRed = red; maxIdx = i; }
        }
        double dist = 0;
        for (int k = 0; k < n; k++) dist += (x[k] - start[k]) * (x[k] - start[k]);
        if (sqrt(dist) < 1e-9) break;
        for (int k = 0; k < n; k++) nd[k] = x[k] - start[k];
        double nnorm = 0;
        for (int k = 0; k < n; k++) nnorm += nd[k] * nd[k];
        if (nnorm < 1e-24) break;
        nnorm = sqrt(nnorm);
        for (int k = 0; k < n; k++) nd[k] /= nnorm;
        double t2 = gmb_opt_line_min(f, ctx, x, nd, n);
        for (int k = 0; k < n; k++) x[k] += t2 * nd[k];
        if (maxIdx >= 0) for (int k = 0; k < n; k++) dirs[maxIdx][k] = nd[k];
        if (fabs(prevF - f(x, n, ctx)) < 1e-13) break;
    }
    for (int i = 0; i < n; i++) out[i] = x[i];
    for (int i = 0; i < n; i++) free(dirs[i]);
    free(dirs); free(x); free(start); free(nd);
}

static void gmb_opt_fd_grad(gmb_opt_fvec_fn f, void* ctx, const double* x, int n, double* g)
{
    for (int i = 0; i < n; i++)
    {
        double h = 1e-6 * (1.0 + fabs(x[i]));
        double* xp = (double*)malloc(sizeof(double) * (size_t)n);
        double* xm = (double*)malloc(sizeof(double) * (size_t)n);
        for (int k = 0; k < n; k++) { xp[k] = x[k]; xm[k] = x[k]; }
        xp[i] += h; xm[i] -= h;
        g[i] = (f(xp, n, ctx) - f(xm, n, ctx)) / (2 * h);
        free(xp); free(xm);
    }
}

void gmb_opt_tnc(gmb_opt_fvec_fn f, void* ctx, const double* x0, int n, const double* lo, const double* hi, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    double* g = (double*)malloc(sizeof(double) * (size_t)n);
    double* d = (double*)malloc(sizeof(double) * (size_t)n);
    double* r = (double*)malloc(sizeof(double) * (size_t)n);
    double* p = (double*)malloc(sizeof(double) * (size_t)n);
    double* hp = (double*)malloc(sizeof(double) * (size_t)n);
    double* xnew = (double*)malloc(sizeof(double) * (size_t)n);
    double* xpp = (double*)malloc(sizeof(double) * (size_t)n);
    double* xmm = (double*)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) x[i] = x0[i];
    gmb_opt_clamp(x, lo, hi, n);
    for (int it = 0; it < 100; it++)
    {
        gmb_opt_fd_grad(f, ctx, x, n, g);
        double gnorm = 0;
        for (int i = 0; i < n; i++) gnorm += g[i] * g[i];
        if (sqrt(gnorm) < 1e-7) break;
        /* CG su H·d = -g */
        for (int i = 0; i < n; i++) { d[i] = 0; r[i] = -g[i]; p[i] = r[i]; }
        double rsold = 0;
        for (int i = 0; i < n; i++) rsold += r[i] * r[i];
        for (int itcg = 0; itcg < 2 * n; itcg++)
        {
            const double h = 1e-5;
            for (int k = 0; k < n; k++) { xpp[k] = x[k] + h * p[k]; xmm[k] = x[k] - h * p[k]; }
            gmb_opt_fd_grad(f, ctx, xpp, n, hp);
            double* gm = (double*)malloc(sizeof(double) * (size_t)n);
            gmb_opt_fd_grad(f, ctx, xmm, n, gm);
            for (int k = 0; k < n; k++) hp[k] = (hp[k] - gm[k]) / (2 * h);
            free(gm);
            double ap = 0;
            for (int i2 = 0; i2 < n; i2++) ap += p[i2] * hp[i2];
            if (fabs(ap) < 1e-16) break;
            double alpha = rsold / ap;
            for (int i2 = 0; i2 < n; i2++) d[i2] += alpha * p[i2];
            for (int i2 = 0; i2 < n; i2++) r[i2] -= alpha * hp[i2];
            double rsnew = 0;
            for (int i2 = 0; i2 < n; i2++) rsnew += r[i2] * r[i2];
            if (rsnew < 1e-22) break;
            for (int i2 = 0; i2 < n; i2++) p[i2] = r[i2] + (rsnew / rsold) * p[i2];
            rsold = rsnew;
        }
        double fx = f(x, n, ctx);
        double gd = 0;
        for (int i = 0; i < n; i++) gd += g[i] * d[i];
        int moved = 0;
        double alpha = 1.0;
        for (int ls = 0; ls < 40; ls++)
        {
            for (int k = 0; k < n; k++) xnew[k] = x[k] + alpha * d[k];
            gmb_opt_clamp(xnew, lo, hi, n);
            double fnew = f(xnew, n, ctx);
            if (fnew <= fx + 1e-4 * alpha * gd) { for (int k = 0; k < n; k++) x[k] = xnew[k]; moved = 1; break; }
            alpha *= 0.5;
        }
        if (!moved) break;
    }
    for (int i = 0; i < n; i++) out[i] = x[i];
    free(x); free(g); free(d); free(r); free(p); free(hp); free(xnew); free(xpp); free(xmm);
}

void gmb_opt_cobyla(gmb_opt_fvec_fn f, void* ctx, const double* x0, int n, const double* lo, const double* hi, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    double* bestX = (double*)malloc(sizeof(double) * (size_t)n);
    double* cand = (double*)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) x[i] = x0[i];
    gmb_opt_clamp(x, lo, hi, n);
    double rho = 1.0;
    for (int it = 0; it < 200 && rho > 1e-7; it++)
    {
        double best = f(x, n, ctx);
        for (int i = 0; i < n; i++) bestX[i] = x[i];
        int improved = 0;
        for (int i = 0; i < n; i++)
            for (int sg = 0; sg < 2; sg++)
            {
                for (int k = 0; k < n; k++) cand[k] = x[k];
                cand[i] += (sg == 0 ? 1.0 : -1.0) * rho;
                gmb_opt_clamp(cand, lo, hi, n);
                double v = f(cand, n, ctx);
                if (v < best) { best = v; for (int k = 0; k < n; k++) bestX[k] = cand[k]; improved = 1; }
            }
        if (improved) for (int k = 0; k < n; k++) x[k] = bestX[k];
        else rho *= 0.5;
    }
    for (int i = 0; i < n; i++) out[i] = x[i];
    free(x); free(bestX); free(cand);
}

void gmb_opt_slsqp(gmb_opt_fvec_fn f, void* ctx, const double* x0, int n, const double* lo, const double* hi, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    double* g = (double*)malloc(sizeof(double) * (size_t)n);
    double* gnew = (double*)malloc(sizeof(double) * (size_t)n);
    double* d = (double*)malloc(sizeof(double) * (size_t)n);
    double* s = (double*)malloc(sizeof(double) * (size_t)n);
    double* y = (double*)malloc(sizeof(double) * (size_t)n);
    double* rhs = (double*)malloc(sizeof(double) * (size_t)n);
    double* hs = (double*)malloc(sizeof(double) * (size_t)n);
    double* xnew = (double*)malloc(sizeof(double) * (size_t)n);
    double** H = (double**)malloc(sizeof(double*) * (size_t)n);
    for (int i = 0; i < n; i++) { H[i] = (double*)calloc((size_t)n, sizeof(double)); H[i][i] = 1.0; x[i] = x0[i]; }
    gmb_opt_clamp(x, lo, hi, n);
    gmb_opt_fd_grad(f, ctx, x, n, g);
    for (int it = 0; it < 200; it++)
    {
        double gnorm = 0;
        for (int i = 0; i < n; i++) gnorm += g[i] * g[i];
        if (sqrt(gnorm) < 1e-7) break;
        for (int i = 0; i < n; i++) rhs[i] = -g[i];
        /* eliminazione gaussiana */
        double** m = (double**)malloc(sizeof(double*) * (size_t)n);
        for (int i = 0; i < n; i++) { m[i] = (double*)malloc(sizeof(double) * (size_t)(n + 1)); for (int j = 0; j < n; j++) m[i][j] = H[i][j]; m[i][n] = rhs[i]; }
        int singular = 0;
        for (int col = 0; col < n; col++)
        {
            int piv = col;
            for (int r = col + 1; r < n; r++) if (fabs(m[r][col]) > fabs(m[piv][col])) piv = r;
            if (fabs(m[piv][col]) < 1e-14) { singular = 1; break; }
            if (piv != col) for (int j = 0; j <= n; j++) { double t = m[col][j]; m[col][j] = m[piv][j]; m[piv][j] = t; }
            for (int r = col + 1; r < n; r++)
            {
                double f2 = m[r][col] / m[col][col];
                for (int j = col; j <= n; j++) m[r][j] -= f2 * m[col][j];
            }
        }
        if (!singular)
        {
            for (int i = n - 1; i >= 0; i--)
            {
                double sum = m[i][n];
                for (int j = i + 1; j < n; j++) sum -= m[i][j] * d[j];
                d[i] = sum / m[i][i];
            }
        }
        else for (int i = 0; i < n; i++) d[i] = -g[i];
        for (int i = 0; i < n; i++) free(m[i]);
        free(m);
        double fx = f(x, n, ctx);
        double gd = 0;
        for (int i = 0; i < n; i++) gd += g[i] * d[i];
        int moved = 0;
        double alpha = 1.0;
        for (int ls = 0; ls < 40; ls++)
        {
            for (int k = 0; k < n; k++) xnew[k] = x[k] + alpha * d[k];
            gmb_opt_clamp(xnew, lo, hi, n);
            double fnew = f(xnew, n, ctx);
            if (fnew <= fx + 1e-4 * alpha * gd) { for (int k = 0; k < n; k++) x[k] = xnew[k]; moved = 1; break; }
            alpha *= 0.5;
        }
        if (!moved) break;
        gmb_opt_fd_grad(f, ctx, x, n, gnew);
        for (int k = 0; k < n; k++) s[k] = alpha * d[k];
        for (int k = 0; k < n; k++) y[k] = gnew[k] - g[k];
        double sy = 0;
        for (int k = 0; k < n; k++) sy += s[k] * y[k];
        if (sy > 1e-12)
        {
            for (int i = 0; i < n; i++) { hs[i] = 0; for (int j = 0; j < n; j++) hs[i] += H[i][j] * s[j]; }
            double shs = 0;
            for (int i = 0; i < n; i++) shs += s[i] * hs[i];
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    H[i][j] += y[i] * y[j] / sy - (shs > 1e-14 ? hs[i] * hs[j] / shs : 0);
        }
        for (int k = 0; k < n; k++) g[k] = gnew[k];
    }
    for (int i = 0; i < n; i++) out[i] = x[i];
    for (int i = 0; i < n; i++) free(H[i]);
    free(H); free(x); free(g); free(gnew); free(d); free(s); free(y); free(rhs); free(hs); free(xnew);
}

/* ---- fsolve (vettore→vettore) e least_squares ---- */
typedef void (*gmb_opt_fvv_fn)(const double* v, int n, double* out, void* ctx);

static int gmb_opt_solve_linear(double** a, const double* b, int n, double* x)
{
    double** m = (double**)malloc(sizeof(double*) * (size_t)n);
    for (int i = 0; i < n; i++) { m[i] = (double*)malloc(sizeof(double) * (size_t)(n + 1)); for (int j = 0; j < n; j++) m[i][j] = a[i][j]; m[i][n] = b[i]; }
    for (int col = 0; col < n; col++)
    {
        int piv = col;
        for (int r = col + 1; r < n; r++) if (fabs(m[r][col]) > fabs(m[piv][col])) piv = r;
        if (fabs(m[piv][col]) < 1e-14) { for (int i = 0; i < n; i++) free(m[i]); free(m); return -1; }
        if (piv != col) for (int j = 0; j <= n; j++) { double t = m[col][j]; m[col][j] = m[piv][j]; m[piv][j] = t; }
        for (int r = col + 1; r < n; r++)
        {
            double f2 = m[r][col] / m[col][col];
            for (int j = col; j <= n; j++) m[r][j] -= f2 * m[col][j];
        }
    }
    for (int i = n - 1; i >= 0; i--)
    {
        double sum = m[i][n];
        for (int j = i + 1; j < n; j++) sum -= m[i][j] * x[j];
        x[i] = sum / m[i][i];
    }
    for (int i = 0; i < n; i++) free(m[i]);
    free(m);
    return 0;
}

void gmb_opt_fsolve(gmb_opt_fvv_fn f, void* ctx, const double* x0, int n, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    double* fx = (double*)malloc(sizeof(double) * (size_t)n);
    double* fp = (double*)malloc(sizeof(double) * (size_t)n);
    /* Buffer input separato per la valutazione perturbata: f(fp,n,fp,ctx) aliaserebbe input
       e output — le lambda C99 scrivono out[] mentre leggono p[] → jacobiano corrotto
       (come gmb_opt_least_squares). */
    double* xp = (double*)malloc(sizeof(double) * (size_t)n);
    double* d = (double*)malloc(sizeof(double) * (size_t)n);
    double* xnew = (double*)malloc(sizeof(double) * (size_t)n);
    double* fnew = (double*)malloc(sizeof(double) * (size_t)n);
    double** jac = (double**)malloc(sizeof(double*) * (size_t)n);
    for (int i = 0; i < n; i++) jac[i] = (double*)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) x[i] = x0[i];
    for (int it = 0; it < 200; it++)
    {
        f(x, n, fx, ctx);
        double fnorm = 0;
        for (int i = 0; i < n; i++) fnorm += fx[i] * fx[i];
        if (sqrt(fnorm) < 1e-10) break;
        for (int col = 0; col < n; col++)
        {
            double h = 1e-7 * (1.0 + fabs(x[col]));
            for (int k = 0; k < n; k++) xp[k] = x[k];
            xp[col] += h;
            f(xp, n, fp, ctx);
            for (int i = 0; i < n; i++) jac[i][col] = (fp[i] - fx[i]) / h;
        }
        double* rhs = (double*)malloc(sizeof(double) * (size_t)n);
        for (int i = 0; i < n; i++) rhs[i] = -fx[i];
        if (gmb_opt_solve_linear(jac, rhs, n, d) != 0) { free(rhs); break; }
        free(rhs);
        double alpha = 1.0;
        int moved = 0;
        for (int ls = 0; ls < 40; ls++)
        {
            for (int k = 0; k < n; k++) xnew[k] = x[k] + alpha * d[k];
            f(xnew, n, fnew, ctx);
            double n2 = 0;
            for (int i = 0; i < n; i++) n2 += fnew[i] * fnew[i];
            if (n2 < fnorm) { for (int k = 0; k < n; k++) x[k] = xnew[k]; moved = 1; break; }
            alpha *= 0.5;
        }
        if (!moved) break;
    }
    for (int i = 0; i < n; i++) out[i] = x[i];
    for (int i = 0; i < n; i++) free(jac[i]);
    free(jac); free(x); free(fx); free(fp); free(xp); free(d); free(xnew); free(fnew);
}

void gmb_opt_least_squares(gmb_opt_fvv_fn r, void* ctx, const double* x0, int n, int m, const double* lo, const double* hi, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    /* I buffer dei residui (rx/rp/rn) ospitano i valori scritti dal lambda: m residui,
       ma rp è anche usato come vettore di ingresso (n elementi). Si dimensionano a
       max(m,n) — il bug C99 storico (m=n con lambda a piu residui) scriveva oltre. */
    int rsz = (m > n) ? m : n;
    double* rx = (double*)malloc(sizeof(double) * (size_t)rsz);
    double* rp = (double*)malloc(sizeof(double) * (size_t)rsz);
    double* rn = (double*)malloc(sizeof(double) * (size_t)rsz);
    /* Buffer input separato per la valutazione perturbata: r(rp,n,rp) aliaserebbe input e
       output — le lambda C99 scrivono out[] mentre leggono p[] → il jacobiano risultava
       corrotto (fit sbagliato). */
    double* xp = (double*)malloc(sizeof(double) * (size_t)n);
    double* d = (double*)malloc(sizeof(double) * (size_t)n);
    double* xnew = (double*)malloc(sizeof(double) * (size_t)n);
    double** jac = NULL;
    double lambda = 1e-3;
    if (m <= 0) m = n;
    for (int i = 0; i < n; i++) x[i] = x0[i];
    gmb_opt_clamp(x, lo, hi, n);
    for (int it = 0; it < 200; it++)
    {
        r(x, n, rx, ctx);
        double cost = 0;
        for (int i = 0; i < m; i++) cost += rx[i] * rx[i];
        if (sqrt(cost) < 1e-10) break;
        if (!jac) { jac = (double**)malloc(sizeof(double*) * (size_t)m); for (int i = 0; i < m; i++) jac[i] = (double*)malloc(sizeof(double) * (size_t)n); }
        for (int col = 0; col < n; col++)
        {
            double h = 1e-7 * (1.0 + fabs(x[col]));
            for (int k = 0; k < n; k++) xp[k] = x[k];
            xp[col] += h;
            r(xp, n, rp, ctx);
            for (int i = 0; i < m; i++) jac[i][col] = (rp[i] - rx[i]) / h;
        }
        double* jtj = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
        double* jtr = (double*)malloc(sizeof(double) * (size_t)n);
        for (int i = 0; i < n; i++)
        {
            jtr[i] = 0;
            for (int j = 0; j < n; j++)
            {
                double s = 0;
                for (int k = 0; k < m; k++) s += jac[k][i] * jac[k][j];
                jtj[i * n + j] = s;
            }
            jtj[i * n + i] += lambda;
            for (int k = 0; k < m; k++) jtr[i] += jac[k][i] * rx[k];
        }
        double* rhs = (double*)malloc(sizeof(double) * (size_t)n);
        for (int i = 0; i < n; i++) rhs[i] = -jtr[i];
        double** aj = (double**)malloc(sizeof(double*) * (size_t)n);
        for (int i = 0; i < n; i++) { aj[i] = jtj + i * n; }
        if (gmb_opt_solve_linear(aj, rhs, n, d) == 0)
        {
            for (int k = 0; k < n; k++) xnew[k] = x[k] + d[k];
            gmb_opt_clamp(xnew, lo, hi, n);
            r(xnew, n, rn, ctx);
            double nc = 0;
            for (int i = 0; i < m; i++) nc += rn[i] * rn[i];
            if (nc < cost) { for (int k = 0; k < n; k++) x[k] = xnew[k]; lambda *= 0.3; }
            else lambda *= 2.0;
        }
        else lambda *= 2.0;
        free(jtj); free(jtr); free(rhs); free(aj);
        if (lambda > 1e12) break;
    }
    for (int i = 0; i < n; i++) out[i] = x[i];
    if (jac) { for (int i = 0; i < m; i++) free(jac[i]); free(jac); }
    free(x); free(rx); free(rp); free(rn); free(xp); free(d); free(xnew);
}

static void gmb_opt_pivot(double* tab, double* cost, int rows, int cols, int leaving, int entering)
{
    double piv = tab[leaving * cols + entering];
    for (int j = 0; j < cols; j++) tab[leaving * cols + j] /= piv;
    for (int r = 0; r < rows; r++)
    {
        if (r == leaving) continue;
        double f2 = tab[r * cols + entering];
        if (fabs(f2) < 1e-15) continue;
        for (int j = 0; j < cols; j++) tab[r * cols + j] -= f2 * tab[leaving * cols + j];
    }
    double cf = cost[entering];
    for (int j = 0; j < cols - 1; j++) cost[j] -= cf * tab[leaving * cols + j];
    cost[entering] = 0;
}

/* ---- linprog (two-phase simplex) ---- */
void gmb_opt_linprog(const double* c, int n, const double** aub, const double* bub, int mub,
    const double** aeq, const double* beq, int meq, const double* lo, const double* hi, double* out)
{
    int rows = mub + meq;
    int cols = n + mub + meq + 1;
    double* tab = (double*)calloc((size_t)rows * (size_t)cols, sizeof(double));
    int* basis = (int*)malloc(sizeof(int) * (size_t)rows);
    for (int i = 0; i < mub; i++)
    {
        for (int j = 0; j < n; j++) tab[i * cols + j] = aub[i][j];
        tab[i * cols + n + i] = 1;
        tab[i * cols + cols - 1] = bub[i];
        basis[i] = n + i;
    }
    for (int i = 0; i < meq; i++)
    {
        int r = mub + i;
        for (int j = 0; j < n; j++) tab[r * cols + j] = aeq[i][j];
        tab[r * cols + n + mub + i] = 1;
        tab[r * cols + cols - 1] = beq[i];
        basis[r] = n + mub + i;
    }
    for (int r = 0; r < rows; r++)
        if (tab[r * cols + cols - 1] < 0)
        {
            for (int j = 0; j < cols - 1; j++) tab[r * cols + j] = -tab[r * cols + j];
            tab[r * cols + cols - 1] = -tab[r * cols + cols - 1];
        }
    int artStart = n + mub;
    for (int r = 0; r < rows; r++)
        if (basis[r] >= artStart)
            for (int j = 0; j < cols - 1; j++) tab[r * cols + j] = -tab[r * cols + j];
    double* cost = (double*)calloc((size_t)(cols - 1), sizeof(double));
    for (int r = 0; r < rows; r++)
        if (basis[r] >= artStart)
            for (int j = 0; j < cols - 1; j++) cost[j] -= tab[r * cols + j];
    for (int it = 0; it < 2000; it++)
    {
        int ent = -1;
        double mc = -1e-9;
        for (int j = 0; j < cols - 1; j++)
            if (cost[j] < mc) { mc = cost[j]; ent = j; }
        if (ent < 0) break;
        int leave = -1;
        double best = 1e300;
        for (int r = 0; r < rows; r++)
        {
            if (tab[r * cols + ent] > 1e-12)
            {
                double ratio = tab[r * cols + cols - 1] / tab[r * cols + ent];
                if (ratio < best) { best = ratio; leave = r; }
            }
        }
        if (leave < 0) break;
        gmb_opt_pivot(tab, cost, rows, cols, leave, ent);
        basis[leave] = ent;
    }
    for (int r = 0; r < rows; r++)
        if (basis[r] >= artStart && fabs(tab[r * cols + cols - 1]) > 1e-8)
        {
            for (int i = 0; i < n; i++) out[i] = 0;
            free(tab); free(basis); free(cost);
            return;
        }
    for (int j = 0; j < n; j++) cost[j] = c[j];
    for (int j = n; j < n + mub; j++) cost[j] = 0;
    for (int j = n + mub; j < cols - 1; j++) cost[j] = 1e9;
    for (int r = 0; r < rows; r++)
        if (basis[r] < n)
        {
            double coef = cost[basis[r]];
            for (int j = 0; j < cols - 1; j++) cost[j] -= coef * tab[r * cols + j];
            cost[basis[r]] = 0;
        }
    for (int it = 0; it < 2000; it++)
    {
        int ent = -1;
        double mc = -1e-9;
        for (int j = 0; j < cols - 1; j++)
            if (cost[j] < mc && j < n + mub) { mc = cost[j]; ent = j; }
        if (ent < 0) break;
        int leave = -1;
        double best = 1e300;
        for (int r = 0; r < rows; r++)
        {
            if (tab[r * cols + ent] > 1e-12)
            {
                double ratio = tab[r * cols + cols - 1] / tab[r * cols + ent];
                if (ratio < best) { best = ratio; leave = r; }
            }
        }
        if (leave < 0) break;
        gmb_opt_pivot(tab, cost, rows, cols, leave, ent);
        basis[leave] = ent;
    }
    double* y = (double*)calloc((size_t)n, sizeof(double));
    for (int r = 0; r < rows; r++)
        if (basis[r] < n) y[basis[r]] = tab[r * cols + cols - 1];
    for (int i = 0; i < n; i++)
    {
        double v = lo && !isnan(lo[i]) ? lo[i] + (y[i] > 0 ? y[i] : 0) : y[i];
        if (hi && !isnan(hi[i])) v = v < hi[i] ? v : hi[i];
        out[i] = v;
    }
    free(y); free(tab); free(basis); free(cost);
}

/* ---- globali: brute / basin_hopping / dual_annealing ---- */
void gmb_opt_brute(gmb_opt_fvec_fn f, void* ctx, const double* lo, const double* hi, int n, int N, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    int* steps = (int*)calloc((size_t)n, sizeof(int));
    double bestF = 1e300;
    for (;;)
    {
        for (int i = 0; i < n; i++) x[i] = lo[i] + steps[i] * (hi[i] - lo[i]) / (N - 1);
        double val = f(x, n, ctx);
        if (val < bestF) { bestF = val; for (int i = 0; i < n; i++) out[i] = x[i]; }
        int k = 0;
        while (k < n && ++steps[k] >= N) { steps[k] = 0; k++; }
        if (k >= n) break;
    }
    free(x); free(steps);
}

/* BasinHopping: replica di CommonLib.Numerics via xorshift64 seed 12345 (stessa sequenza
 * di IlRuntime._rndState/RandomizeSeed dopo RANDOMIZE <12345>). Un solo RNG per iterazione:
 * n estrazioni per lo step ((2*r-1)), poi 1 per l'accettazione (Metropolis). */
typedef struct { unsigned long long s; } bh_rng_t;
static void bh_rng_seed(bh_rng_t* r, unsigned long long seed)
{ r->s = seed ? seed : 0x2545F4914F6CDD1DULL; }
static double bh_rng_next(bh_rng_t* r)
{
    r->s ^= r->s << 13; r->s ^= r->s >> 7; r->s ^= r->s << 17;
    return (double)(r->s >> 11) / 9007199254740992.0;
}
void gmb_opt_basin_hopping(gmb_opt_fvec_fn f, void* ctx, const double* x0, int n, int niter, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    double* step = (double*)malloc(sizeof(double) * (size_t)n);
    double* best = (double*)malloc(sizeof(double) * (size_t)n);
    double* local = (double*)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) { x[i] = x0[i]; best[i] = x0[i]; }
    double fx = f(x, n, ctx), bestF = fx;
    bh_rng_t r; bh_rng_seed(&r, 12345);
    for (int it = 0; it < niter; it++)
    {
        for (int i = 0; i < n; i++)
            step[i] = x[i] + (bh_rng_next(&r) * 2.0 - 1.0) * (1.0 + fabs(x[i]));
        gmb_opt_nelder_mead(f, step, n, 1e-7, 300, ctx, local);
        double fNew = f(local, n, ctx);
        if (fNew < bestF) { bestF = fNew; for (int i = 0; i < n; i++) best[i] = local[i]; }
        double u = bh_rng_next(&r);
        if (fNew <= fx || u < exp(-(fNew - fx) / fmax(1e-8, fabs(fx))))
        {
            for (int i = 0; i < n; i++) x[i] = local[i];
            fx = fNew;
        }
    }
    for (int i = 0; i < n; i++) out[i] = best[i];
    free(x); free(step); free(best); free(local);
}

static double gmb_opt_rnd(unsigned* s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s / 4294967296.0;
}

void gmb_opt_dual_annealing(gmb_opt_fvec_fn f, void* ctx, const double* lo, const double* hi, int n, int maxiter, double* out)
{
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    double* xnew = (double*)malloc(sizeof(double) * (size_t)n);
    double* best = (double*)malloc(sizeof(double) * (size_t)n);
    double* local = (double*)malloc(sizeof(double) * (size_t)n);
    unsigned s = 424242;
    for (int i = 0; i < n; i++) x[i] = lo[i] + gmb_opt_rnd(&s) * (hi[i] - lo[i]);
    double fx = f(x, n, ctx);
    for (int i = 0; i < n; i++) best[i] = x[i];
    double bestF = fx;
    double temp = 5000;
    for (int it = 0; it < maxiter; it++)
    {
        for (int i = 0; i < n; i++)
        {
            double w = pow(gmb_opt_rnd(&s), 1.0 / (1.0 + it / 50.0));
            double v = x[i] + (2.0 * gmb_opt_rnd(&s) - 1.0) * (hi[i] - lo[i]) * w;
            xnew[i] = v < lo[i] ? lo[i] : (v > hi[i] ? hi[i] : v);
        }
        double fNew = f(xnew, n, ctx);
        if (fNew < bestF) { bestF = fNew; for (int i = 0; i < n; i++) best[i] = xnew[i]; }
        if (fNew < fx || gmb_opt_rnd(&s) < exp(-(fNew - fx) / temp))
        {
            for (int i = 0; i < n; i++) x[i] = xnew[i];
            fx = fNew;
        }
        temp *= 0.999;
        if (it % 25 == 0)
        {
            gmb_opt_nelder_mead(f, x, n, 1e-7, 100, ctx, local);
            double lv = f(local, n, ctx);
            if (lv < bestF) { bestF = lv; for (int i = 0; i < n; i++) best[i] = local[i]; }
        }
    }
    gmb_opt_nelder_mead(f, best, n, 1e-7, 200, ctx, local);
    double lv2 = f(local, n, ctx);
    if (lv2 < bestF) { bestF = lv2; for (int i = 0; i < n; i++) best[i] = local[i]; }
    for (int i = 0; i < n; i++) out[i] = best[i];
    free(x); free(xnew); free(best); free(local);
}

/* ---- Bridge C99 per le nuove funzioni optimize (957-973) ----
   Lambda transpilata: gmb_fn_t. Scalare: double(void*,double).
   Vettore→scalare: double(void*,double*). Array-return: double* + outn. */

double gmb_opt_brentq_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_brentq(gmb_opt_c99_f_scalar, &fn, a, b); }
/* BRENT (MathNet RootFinding.Brent.TryFindRoot, parity IL CommonLib.Numerics.Brent:
 * tolerance 1e-8, maxIter 200) — distinto da BRENTH. */
double gmb_opt_brent_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_brent(gmb_opt_c99_f_scalar, a, b, 1e-8, 200, &fn); }
/* Adattatori C99 per quad/simpson (ANALYSIS.INTEGRATE/SIMPSON, closure BASIC): come gli
   altri gmb_*_c99, adattano gmb_fn_t al callback del runtime (double(*)(double,void*)). */
double gmb_quad_gk_c99(gmb_fn_t fn, double a, double b)
{ return gmb_quad_gk(gmb_opt_c99_f_scalar, &fn, a, b); }
double gmb_simpson_c99(gmb_fn_t fn, double a, double b, int n)
{ return gmb_simpson_ctx(gmb_opt_c99_f_scalar, &fn, a, b, n); }
double gmb_opt_golden_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_golden(gmb_opt_c99_f_scalar, a, b, 1e-8, 200, &fn); }

double gmb_opt_secant_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_secant(gmb_opt_c99_f_scalar, a, b, 1e-8, 200, &fn); }

double gmb_opt_bisection_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_bisection(gmb_opt_c99_f_scalar, a, b, 1e-8, 200, &fn); }

/* Gradiente numerico per CG: f(x) in RN, differenze centrali. */
static double gmb_opt_fv(gmb_fn_t fn, const double* x, int n)
{
    /* invoca la lambda (double(*)(void*, double)) — per funzione multidimensionale la
       lambda C99 è (void*, double*) → approssimiamo con un wrapper che legge x[0]. */
    return ((double(*)(void*, double))fn.fn)(fn.ctx, x[0]);
}

void gmb_opt_bfgs_c99(gmb_fn_t fn, double* start, int n, double* out)
{
    gmb_opt_bfgs(gmb_opt_c99_f_vec, start, n, 1e-8, 1000, &fn, out);
}

void gmb_opt_nelder_mead_c99(gmb_fn_t fn, double* start, int n, double* out)
{
    gmb_opt_nelder_mead(gmb_opt_c99_f_vec, start, n, 1e-8, 1000, &fn, out);
}

double* gmb_opt_cg_c99(gmb_fn_t fn, const double* x0, int n, gmb_int_t* outn)
{
    *outn = 0;
    if (n <= 0) return NULL;
    double* x = (double*)malloc((size_t)n * sizeof(double));
    if (!x) return NULL;
    /* parity IL CommonLib.Numerics.ConjugateGradient(f, v0, 1e-6, 200) */
    gmb_opt_cg(gmb_opt_c99_f_vec, (double*)x0, n, 1e-6, 200, &fn, x);
    *outn = n;
    return x;
}

void gmb_opt_lbfgs_c99(gmb_fn_t fn, double* start, int n, double* out)
{
    /* parity IL CommonLib.Numerics.LBfgs(f, v0, 1e-6, 200): memory=200, max=1000. */
    gmb_opt_lbfgs(gmb_opt_c99_f_vec, start, n, 1e-6, 200, 1000, &fn, out);
}


double gmb_opt_brenth_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_brenth(gmb_opt_c99_f_scalar, &fn, a, b); }
double gmb_opt_ridder_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_ridder(gmb_opt_c99_f_scalar, &fn, a, b); }
double gmb_opt_toms748_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_toms748(gmb_opt_c99_f_scalar, &fn, a, b); }
double gmb_opt_brentmin_c99(gmb_fn_t fn, double a, double b)
{ return gmb_opt_brentmin(gmb_opt_c99_f_scalar, &fn, a, b); }
double gmb_opt_minscalar_c99(gmb_fn_t fn, double a, double b, gmb_string_t method)
{
    const char* m = method.data ? method.data : "";
    if (m && strcasecmp(m, "golden") == 0)
    { return gmb_opt_golden(gmb_opt_c99_f_scalar, a, b, 1e-9, 200, &fn); }
    return gmb_opt_brentmin(gmb_opt_c99_f_scalar, &fn, a, b);
}
double* gmb_opt_powell_c99(gmb_fn_t fn, double* x0, int n, gmb_int_t* outn)
{
    if (n <= 0 || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    gmb_opt_powell(gmb_opt_c99_f_vec, &fn, x0, n, out);
    *outn = n; return out;
}
double* gmb_opt_tnc_c99(gmb_fn_t fn, double* x0, int n, gmb_double_t** bounds, gmb_int_t* outn)
{
    if (n <= 0 || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    double lo[32], hi[32];
    for (int i = 0; i < n; i++) { lo[i] = bounds ? bounds[i][0] : NAN; hi[i] = bounds ? bounds[i][1] : NAN; }
    gmb_opt_tnc(gmb_opt_c99_f_vec, &fn, x0, n, lo, hi, out);
    *outn = n; return out;
}
double* gmb_opt_cobyla_c99(gmb_fn_t fn, double* x0, int n, gmb_double_t** bounds, gmb_int_t* outn)
{
    if (n <= 0 || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    double lo[32], hi[32];
    for (int i = 0; i < n; i++) { lo[i] = bounds ? bounds[i][0] : NAN; hi[i] = bounds ? bounds[i][1] : NAN; }
    gmb_opt_cobyla(gmb_opt_c99_f_vec, &fn, x0, n, lo, hi, out);
    *outn = n; return out;
}
double* gmb_opt_slsqp_c99(gmb_fn_t fn, double* x0, int n, gmb_double_t** bounds, gmb_int_t* outn)
{
    if (n <= 0 || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    double lo[32], hi[32];
    for (int i = 0; i < n; i++) { lo[i] = bounds ? bounds[i][0] : NAN; hi[i] = bounds ? bounds[i][1] : NAN; }
    gmb_opt_slsqp(gmb_opt_c99_f_vec, &fn, x0, n, lo, hi, out);
    *outn = n; return out;
}
double* gmb_opt_minimize_c99(gmb_fn_t fn, double* x0, int n, gmb_string_t method, gmb_double_t** bounds, gmb_int_t* outn)
{
    if (n <= 0 || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    const char* m = method.data ? method.data : "";
    double lo[32], hi[32];
    for (int i = 0; i < n; i++) { lo[i] = bounds ? bounds[i][0] : NAN; hi[i] = bounds ? bounds[i][1] : NAN; }
    if (m && strcasecmp(m, "powell") == 0) gmb_opt_powell(gmb_opt_c99_f_vec, &fn, x0, n, out);
    else if (m && strcasecmp(m, "tnc") == 0) gmb_opt_tnc(gmb_opt_c99_f_vec, &fn, x0, n, lo, hi, out);
    else if (m && strcasecmp(m, "cobyla") == 0) gmb_opt_cobyla(gmb_opt_c99_f_vec, &fn, x0, n, lo, hi, out);
    else if (m && strcasecmp(m, "slsqp") == 0) gmb_opt_slsqp(gmb_opt_c99_f_vec, &fn, x0, n, lo, hi, out);
    else gmb_opt_nelder_mead(gmb_opt_c99_f_vec, x0, n, 1e-7, 500, &fn, out);
    *outn = n; return out;
}
double* gmb_opt_brute_c99(gmb_fn_t fn, gmb_double_t** ranges, int n, int N, gmb_int_t* outn)
{
    if (n <= 0 || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    double lo[32], hi[32];
    for (int i = 0; i < n; i++) { lo[i] = ranges[i][0]; hi[i] = ranges[i][1]; }
    gmb_opt_brute(gmb_opt_c99_f_vec, &fn, lo, hi, n, N > 1 ? N : 20, out);
    *outn = n; return out;
}
double* gmb_opt_basin_c99(gmb_fn_t fn, double* x0, int n, int niter, gmb_int_t* outn)
{
    if (n <= 0 || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    gmb_opt_basin_hopping(gmb_opt_c99_f_vec, &fn, x0, n, niter > 0 ? niter : 50, out);
    *outn = n; return out;
}
double* gmb_opt_dual_c99(gmb_fn_t fn, gmb_double_t** bounds, int n, int maxiter, gmb_int_t* outn)
{
    if (n <= 0 || n > 32) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    double lo[32], hi[32];
    for (int i = 0; i < n; i++) { lo[i] = bounds[i][0]; hi[i] = bounds[i][1]; }
    gmb_opt_dual_annealing(gmb_opt_c99_f_vec, &fn, lo, hi, n, maxiter > 0 ? maxiter : 1000, out);
    *outn = n; return out;
}

double* gmb_opt_linprog_c99(const double* c, int n, const double** aub, const double* bub, int mub,
    const double** aeq, const double* beq, int meq, const double** bounds, gmb_int_t* outn)
{
    if (n <= 0 || n > 64) { *outn = 0; return NULL; }
    if (mub < 0 || mub > 64) mub = 0;
    if (meq < 0 || meq > 64) meq = 0;
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    double lo[64], hi[64];
    for (int i = 0; i < n; i++) { lo[i] = bounds ? bounds[i][0] : NAN; hi[i] = bounds ? bounds[i][1] : NAN; }
    gmb_opt_linprog(c, n, aub, bub, mub, aeq, beq, meq, lo, hi, out);
    *outn = n;
    return out;
}

/* Adapter C99 vettore→vettore: la lambda transpilata ha firma
   void fn(void* ctx, double* p, double* out) — riempie out. */
static void gmb_opt_c99_fvv(const double* v, int n, double* out, void* ctx)
{
    gmb_fn_t fn = *(const gmb_fn_t*)ctx;
    gmb_int_t prev = gmb_lambda_bound(0);
    gmb_lambda_bound_set(0, (gmb_int_t)n - 1);
    ((void(*)(void*, double*, double*))fn.fn)(fn.ctx, (double*)v, out);
    gmb_lambda_bound_set(0, prev);
}

double gmb_opt_newton_c99(gmb_fn_t fn, double guess)
{
    return gmb_opt_newton(gmb_opt_c99_f_scalar, guess, 1e-10, 100, &fn);
}

double* gmb_opt_fsolve_c99(gmb_fn_t fn, double* x0, int n, gmb_int_t* outn)
{
    if (n <= 0 || n > 64) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    gmb_opt_fsolve(gmb_opt_c99_fvv, &fn, x0, n, out);
    *outn = n;
    return out;
}

double* gmb_opt_lstsq_c99(gmb_fn_t fn, double* x0, int n, int m, gmb_double_t** bounds, gmb_int_t* outn)
{
    if (n <= 0 || n > 64) { *outn = 0; return NULL; }
    if (m <= 0 || m > 256) m = n;
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    double lo[64], hi[64];
    for (int i = 0; i < n; i++) { lo[i] = bounds ? bounds[i][0] : NAN; hi[i] = bounds ? bounds[i][1] : NAN; }
    /* m = numero di residui del lambda (emesso dall'emitter C99 dal ArrayLiteralExpr);
       se m>n i buffer dei residui vanno dimensionati a max(m,n) — vedi
       gmb_opt_least_squares. */
    gmb_opt_least_squares(gmb_opt_c99_fvv, &fn, x0, n, m, lo, hi, out);
    *outn = n;
    return out;
}

/* Broyden per F(v) vettoriale: lambda BASIC (void*, double*) → double* (residui mallocati).
   Metodo quasi-Newton con aggiornamento rank-1 della matrice B ≈ J^{-1}. */
/* Broyden: replica di CommonLib.Numerics.Broyden (Jacobiano FD + solve + update). */
static int gmb_opt_lu_solve(double* A, double* b, int n)
{
    for (int col = 0; col < n; col++)
    {
        int piv = col;
        for (int r = col + 1; r < n; r++) if (fabs(A[r * n + col]) > fabs(A[piv * n + col])) piv = r;
        if (fabs(A[piv * n + col]) < 1e-300) return -1;
        if (piv != col)
        {
            for (int k = 0; k < n; k++) { double t = A[col * n + k]; A[col * n + k] = A[piv * n + k]; A[piv * n + k] = t; }
            double t = b[col]; b[col] = b[piv]; b[piv] = t;
        }
        for (int r = col + 1; r < n; r++)
        {
            double fac = A[r * n + col] / A[col * n + col];
            for (int k = col; k < n; k++) A[r * n + k] -= fac * A[col * n + k];
            b[r] -= fac * b[col];
        }
    }
    for (int r = n - 1; r >= 0; r--)
    {
        double s = b[r];
        for (int k = r + 1; k < n; k++) s -= A[r * n + k] * b[k];
        b[r] = s / A[r * n + r];
    }
    return 0;
}

void gmb_opt_broyden(gmb_opt_fvv_fn f, void* ctx, const double* x0, int n, double* out)
{
    const double tolerance = 1e-8;
    const int maxIterations = 200;
    if (n <= 0) return;
    double* point = (double*)malloc(sizeof(double) * (size_t)n);
    double* value = (double*)malloc(sizeof(double) * (size_t)n);
    double* J = (double*)calloc((size_t)(n * n), sizeof(double));
    double* probe = (double*)malloc(sizeof(double) * (size_t)n);
    double* pv = (double*)malloc(sizeof(double) * (size_t)n);
    double* delta = (double*)malloc(sizeof(double) * (size_t)n);
    double* nextPoint = (double*)malloc(sizeof(double) * (size_t)n);
    double* nextValue = (double*)malloc(sizeof(double) * (size_t)n);
    double* valueDelta = (double*)malloc(sizeof(double) * (size_t)n);
    double* Jd = (double*)malloc(sizeof(double) * (size_t)n);
    double* Acopy = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    for (int i = 0; i < n; i++) point[i] = x0[i];
    f(point, n, value, ctx);
    for (int i = 0; i < n; i++) { } /* no-op */
    {
        double vn = 0; for (int i = 0; i < n; i++) vn += value[i] * value[i];
        if (sqrt(vn) <= tolerance) { for (int i = 0; i < n; i++) out[i] = point[i]; goto bdone; }
    }
    {
        const double stepScale = 1.4901161193847656e-08;  /* sqrt(BitIncrement(1.0)-1) = 2^-26 */
        for (int col = 0; col < n; col++)
        {
            memcpy(probe, point, sizeof(double) * (size_t)n);
            double step = stepScale * fmax(1.0, fabs(point[col]));
            probe[col] += step;
            f(probe, n, pv, ctx);
            for (int r = 0; r < n; r++) J[r * n + col] = (pv[r] - value[r]) / step;
        }
    }
    for (int iter = 0; iter < maxIterations; iter++)
    {
        memcpy(Acopy, J, sizeof(double) * (size_t)n * (size_t)n);
        for (int i = 0; i < n; i++) delta[i] = -value[i];
        if (gmb_opt_lu_solve(Acopy, delta, n) != 0) { for (int i = 0; i < n; i++) out[i] = NAN; goto bdone; }
        for (int i = 0; i < n; i++) nextPoint[i] = point[i] + delta[i];
        f(nextPoint, n, nextValue, ctx);
        {
            double nn = 0, dn = 0;
            for (int i = 0; i < n; i++) { nn += nextValue[i] * nextValue[i]; dn += delta[i] * delta[i]; }
            if (sqrt(nn) <= tolerance || sqrt(dn) <= tolerance) { for (int i = 0; i < n; i++) out[i] = nextPoint[i]; goto bdone; }
        }
        for (int i = 0; i < n; i++) valueDelta[i] = nextValue[i] - value[i];
        {
            double denom = 0; for (int i = 0; i < n; i++) denom += delta[i] * delta[i];
            if (denom <= 2.2204460492503131e-16) { for (int i = 0; i < n; i++) out[i] = nextPoint[i]; goto bdone; }
            for (int i = 0; i < n; i++) { double a = 0; for (int k = 0; k < n; k++) a += J[i * n + k] * delta[k]; Jd[i] = a; }
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    J[i * n + j] += ((valueDelta[i] - Jd[i]) * delta[j]) / denom;
        }
        memcpy(point, nextPoint, sizeof(double) * (size_t)n);
        memcpy(value, nextValue, sizeof(double) * (size_t)n);
    }
    for (int i = 0; i < n; i++) out[i] = NAN;
bdone:
    free(point); free(value); free(J); free(probe); free(pv); free(delta);
    free(nextPoint); free(nextValue); free(valueDelta); free(Jd); free(Acopy);
}

void gmb_opt_broyden_c99(gmb_fn_t fn, double* x0, int n, double* out)
{ gmb_opt_broyden(gmb_opt_c99_fvv, &fn, x0, n, out); }

int gmb_opt_lin_lstsq(const double* const* a, const double* b, int rows, int cols, double* x)
{
    if (!a || !b || !x || rows <= 0 || cols <= 0 || cols > rows) return -1;
    double* ata = (double*)calloc((size_t)(cols * cols), sizeof(double));
    double* atb = (double*)calloc((size_t)cols, sizeof(double));
    if (!ata || !atb) { free(ata); free(atb); return -1; }
    for (int i = 0; i < cols; i++) for (int j = 0; j < cols; j++)
    { double s = 0; for (int r = 0; r < rows; r++) s += a[r][i] * a[r][j]; ata[i * cols + j] = s; }
    for (int i = 0; i < cols; i++)
    { double s = 0; for (int r = 0; r < rows; r++) s += a[r][i] * b[r]; atb[i] = s; }
    int rc = gmb_cholesky_solve(ata, atb, cols, x);
    free(ata); free(atb);
    return rc;
}

/* SIMPLEX — port fedele di CommonLib.Numerics.Simplex(tableau, maximize=true) usato da
 * IL/PCode/VMC: assembla il tableau (vincoli <= con slack + riga obiettivo), pivota per
 * massimizzare, restituisce [x0..x_{n-1}, obj] con obj NEGATO (parity IlRuntime.Simplex). */
double* gmb_opt_simplex_c99(const double* c, int n, const double* const* aub, const double* bub, int mub, gmb_int_t* outn)
{
    *outn = 0;
    if (n <= 0 || n > 64 || mub < 0 || mub > 64) return NULL;
    int numRows = mub + 1;
    int numCols = n + mub + 1;
    static double tableau[65][200];
    for (int i = 0; i < numRows; i++)
        for (int j = 0; j < numCols; j++) tableau[i][j] = 0.0;
    for (int i = 0; i < mub; i++) {
        for (int j = 0; j < n; j++) tableau[i][j] = aub[i][j];
        tableau[i][n + i] = 1.0;                 /* variabile slack */
        tableau[i][numCols - 1] = bub[i];
    }
    for (int j = 0; j < n; j++) tableau[mub][j] = c[j];
    const double eps = 1e-9;
    for (;;) {
        int pivotCol = -1; double maxVal = eps;
        for (int j = 0; j < numCols - 1; j++)
            if (tableau[numRows - 1][j] > maxVal) { maxVal = tableau[numRows - 1][j]; pivotCol = j; }
        if (pivotCol == -1) break;
        int pivotRow = -1; double minRatio = 1e308;
        for (int i = 0; i < numRows - 1; i++)
            if (tableau[i][pivotCol] > eps) {
                double ratio = tableau[i][numCols - 1] / tableau[i][pivotCol];
                if (ratio < minRatio) { minRatio = ratio; pivotRow = i; }
            }
        if (pivotRow == -1) return NULL;         /* problema illimitato */
        double piv = tableau[pivotRow][pivotCol];
        for (int j = 0; j < numCols; j++) tableau[pivotRow][j] /= piv;
        for (int i = 0; i < numRows; i++) {
            if (i == pivotRow) continue;
            double mul = tableau[i][pivotCol];
            for (int j = 0; j < numCols; j++) tableau[i][j] -= mul * tableau[pivotRow][j];
        }
    }
    int numVars = numCols - numRows;             /* = n */
    double* sol = (double*)calloc((size_t)numVars + 1, sizeof(double));
    if (!sol) return NULL;
    for (int j = 0; j < numVars; j++) {
        for (int i = 0; i < numRows - 1; i++) {
            if (fabs(tableau[i][j] - 1.0) < eps) {
                int other = 0;
                for (int ii = 0; ii < numRows - 1; ii++)
                    if (ii != i && fabs(tableau[ii][j]) > eps) { other = 1; break; }
                if (!other) { sol[j] = tableau[i][numCols - 1]; break; }
            }
        }
    }
    sol[numVars] = -tableau[numRows - 1][numCols - 1];   /* parity IlRuntime: obj negato */
    *outn = numVars + 1;
    return sol;
}

/* LEVENBERGMARQUARDTVEC: modello VETTORIALE (lambda `void(ctx, p, xs, out)`) che riempie
   le predizioni per tutti gli input. Levenberg-Marquardt con Jacobiano a differenze finite
   e normal equations risolte via cholesky. */
double* gmb_opt_lmfit_vec_c99(gmb_fn_t fn, const double* xs, const double* ys, int n, const double* p0, int np, gmb_int_t* outn)
{
    *outn = 0;
    if (!xs || !ys || !p0 || n <= 0 || np <= 0) return NULL;
    double* out = (double*)malloc((size_t)np * sizeof(double));
    if (!out) return NULL;
    /* parity IL CommonLib.Numerics.LevenbergMarquardtVec(f, p0, xs, ys, 1e-8, 200) */
    gmb_lm_fit_vec(gmb_opt_c99_lm_model_vec, &fn, xs, ys, n, p0, np, 200, 1e-8, out);
    *outn = np;
    return out;
}
