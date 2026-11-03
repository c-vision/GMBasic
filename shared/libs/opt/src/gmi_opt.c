/* gmi_opt.c — ottimizzazione numerica (da GMBasic; spostata da gmi_math.c).
 *
 * Newton, bisection, Brent, golden, Nelder-Mead, BFGS, CG, L-BFGS, secant.
 * Function pointer + libm, nessuna dipendenza VM.
 */
#include "gmi_runtime.h"
#include "gmi_opt.h"
#include "opt_linalg.h"
#define HFT_MAXQ 256
#include <math.h>
#include <stdlib.h>
#include <string.h>

double gmi_opt_newton(double (*fn)(double, void*), double guess, double tol, int max_iter, void* ctx)
{
    const double h = 1e-6;
    double x = guess;
    for (int i = 0; i < max_iter; i++)
    {
        double fx = fn(x, ctx);
        double dfx = (fn(x + h, ctx) - fn(x - h, ctx)) / (2 * h);
        if (fabs(dfx) < 1e-12) break;
        double nx = x - fx / dfx;
        if (fabs(nx - x) < tol) return nx;
        x = nx;
    }
    return x;
}

/* Bisection: replica esatta di MathNet.Numerics.RootFinding.Bisection.TryFindRoot
 * (oracolo IL/PCode): stop quando (hi-lo) <= 2*accuracy E |f(root)| <= accuracy. */
static int gmi_opt_msign(double x) { return (x > 0) - (x < 0); }
double gmi_opt_bisection(double (*fn)(double, void*), double lowerBound, double upperBound, double accuracy, int maxIterations, void* ctx)
{
    if (accuracy <= 0) accuracy = 1e-8;
    if (upperBound < lowerBound) { double t = upperBound; upperBound = lowerBound; lowerBound = t; }
    double fmin = fn(lowerBound, ctx);
    if (gmi_opt_msign(fmin) == 0) return lowerBound;
    double fmax = fn(upperBound, ctx);
    if (gmi_opt_msign(fmax) == 0) return upperBound;
    double root = 0.5 * (lowerBound + upperBound);
    if (gmi_opt_msign(fmin) == gmi_opt_msign(fmax)) return root;   /* bad bracketing */
    for (int i = 0; i <= maxIterations; i++)
    {
        double froot = fn(root, ctx);
        if (upperBound - lowerBound <= 2 * accuracy && fabs(froot) <= accuracy) return root;
        if (lowerBound == root || upperBound == root) return root;
        if (gmi_opt_msign(froot) == gmi_opt_msign(fmin)) { lowerBound = root; fmin = froot; }
        else if (gmi_opt_msign(froot) == gmi_opt_msign(fmax)) { upperBound = root; fmax = froot; }
        else return root;
        root = 0.5 * (lowerBound + upperBound);
    }
    return root;
}

/* Brent: replica esatta di MathNet.Numerics.RootFinding.Brent.TryFindRoot
 * (oracolo IL/PCode; Numerical Recipes), incluso il criterio di convergenza. */
static int gmi_opt_almost_equal_norm_rel(double a, double b, double diff, double maxErr)
{
    diff = fabs(diff);
    if (a == 0.0 || b == 0.0 || diff < 4.9406564584124654e-324) return diff < maxErr;
    return (diff / (fabs(a) + fabs(b))) < maxErr;
}
static int gmi_opt_almost_equal_rel(double a, double b)
{
    return gmi_opt_almost_equal_norm_rel(a, b, fabs(a - b), 1e-15);
}
double gmi_opt_brent(double (*fn)(double, void*), double lowerBound, double upperBound, double accuracy, int maxIterations, void* ctx)
{
    if (accuracy <= 0) accuracy = 1e-8;
    double fL = fn(lowerBound, ctx);
    double fU = fn(upperBound, ctx);
    double fR = fU;
    double d = 0.0, e = 0.0;
    double root = upperBound;
    double xMid = NAN;
    if (gmi_opt_msign(fL) == gmi_opt_msign(fU)) return root;
    for (int i = 0; i <= maxIterations; i++)
    {
        if (gmi_opt_msign(fR) == gmi_opt_msign(fU))
        {
            upperBound = lowerBound; fU = fL; e = d = root - lowerBound;
        }
        if (fabs(fU) < fabs(fR))
        {
            lowerBound = root; root = upperBound; upperBound = lowerBound;
            fL = fR; fR = fU; fU = fL;
        }
        double xAcc1 = 2.2204460492503131e-16 * fabs(root) + 0.5 * accuracy;
        double xMidOld = xMid;
        xMid = (upperBound - root) / 2.0;
        if (fabs(xMid) <= xAcc1 || gmi_opt_almost_equal_norm_rel(fR, 0.0, fR, accuracy)) return root;
        if (xMid == xMidOld) return root;
        if (fabs(e) >= xAcc1 && fabs(fL) > fabs(fR))
        {
            double s = fR / fL;
            double p, q;
            if (gmi_opt_almost_equal_rel(lowerBound, upperBound))
            {
                p = 2.0 * xMid * s;
                q = 1.0 - s;
            }
            else
            {
                q = fL / fU;
                double r = fR / fU;
                p = s * (2.0 * xMid * q * (q - r) - (root - lowerBound) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0) q = -q;
            p = fabs(p);
            double r1 = 3.0 * xMid * q - fabs(xAcc1 * q);
            double r2 = fabs(e * q);
            if (2.0 * p < (r1 < r2 ? r1 : r2)) { e = d; d = p / q; }
            else { d = xMid; e = d; }
        }
        else { d = xMid; e = d; }
        lowerBound = root; fL = fR;
        if (fabs(d) > xAcc1) root += d;
        else root += copysign(xAcc1, xMid);
        fR = fn(root, ctx);
    }
    return root;
}

/* Golden section: minimo di fn in [lo, hi]. */
double gmi_opt_golden(double (*fn)(double, void*), double lo, double hi, double tol, int max_iter, void* ctx)
{
    const double phi = 0.618033988749895;
    double a = lo, b = hi;
    double x1 = b - phi * (b - a);
    double x2 = a + phi * (b - a);
    double f1 = fn(x1, ctx), f2 = fn(x2, ctx);
    for (int i = 0; i < max_iter && (b - a) > tol; i++)
    {
        if (f1 < f2)
        {
            b = x2; x2 = x1; f2 = f1;
            x1 = b - phi * (b - a);
            f1 = fn(x1, ctx);
        }
        else
        {
            a = x1; x1 = x2; f1 = f2;
            x2 = a + phi * (b - a);
            f2 = fn(x2, ctx);
        }
    }
    return (a + b) / 2;
}

/* Nelder-Mead (simplex): minimizza fn(v[0..n-1]) partendo da start. Ritorna il punto minimo. */
/* TryToScaleSimplex MathNet: ricalcola il centroide (escluso il vertice high),
 * scala e valuta; sostituisce high se migliore; ritorna il nuovo valore. */
static double gmi_nm_try_scale(double scale, double (*fn)(const double*, int, void*), void* ctx,
                               double* simplex, double* vals, int n, int nv, int high)
{
    double centroid[64], newp[64];
    for (int j = 0; j < n; j++)
    {
        double s = 0;
        for (int i = 0; i < nv; i++) if (i != high) s += simplex[i * n + j];
        centroid[j] = s / (double)(nv - 1);
    }
    for (int j = 0; j < n; j++) newp[j] = (simplex[high * n + j] - centroid[j]) * scale + centroid[j];
    double ne = fn(newp, n, ctx);
    if (ne < vals[high]) { for (int j = 0; j < n; j++) simplex[high * n + j] = newp[j]; vals[high] = ne; }
    return ne;
}

/* Nelder-Mead: replica esatta di MathNet.Numerics.Optimization.NelderMeadSimplex
 * (oracolo IL/PCode, tol=1e-6 max=500 da IlRuntime.NelderMead). */
void gmi_opt_nelder_mead(double (*fn)(const double*, int, void*), double* start, int n,
                         double tol, int max_iter, void* ctx, double* out)
{
    if (n <= 0 || n > 63) { for (int j = 0; j < n; j++) out[j] = start[j]; return; }
    const double JITTER = 1e-10;
    int nv = n + 1;
    double* simplex = (double*)malloc(sizeof(double) * (size_t)nv * n);
    double* vals = (double*)malloc(sizeof(double) * nv);
    double pert[64];
    if (!simplex || !vals) { free(simplex); free(vals); return; }
    for (int j = 0; j < n; j++) pert[j] = (start[j] == 0.0) ? 0.00025 : start[j] * 0.05;
    for (int j = 0; j < n; j++) simplex[j] = start[j];
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++) simplex[(i + 1) * n + j] = start[j];
        simplex[(i + 1) * n + i] = start[i] + pert[i];
    }
    for (int i = 0; i < nv; i++) vals[i] = fn(&simplex[i * n], n, ctx);

    int evaluationCount = 0, numConverged = 0;
    int high = 0, next = 0, low = 0;
    for (;;)
    {
        /* EvaluateSimplex */
        if (vals[0] > vals[1]) { high = 0; next = 1; } else { high = 1; next = 0; }
        low = 0;
        for (int index = 0; index < nv; index++)
        {
            double ev = vals[index];
            if (ev <= vals[low]) low = index;
            if (ev > vals[high]) { next = high; high = index; }
            else if (ev > vals[next] && index != high) next = index;
        }
        /* HasConverged */
        double range = 2.0 * fabs(vals[high] - vals[low]) /
                       (fabs(vals[high]) + fabs(vals[low]) + JITTER);
        if (range < tol) numConverged++; else numConverged = 0;
        if (numConverged == 2) break;

        /* reflection */
        double rv = gmi_nm_try_scale(-1.0, fn, ctx, simplex, vals, n, nv, high);
        evaluationCount++;
        if (rv <= vals[low])
        {
            gmi_nm_try_scale(2.0, fn, ctx, simplex, vals, n, nv, high);
            evaluationCount++;
        }
        else if (rv >= vals[next])
        {
            double currentWorst = vals[high];
            double cv = gmi_nm_try_scale(0.5, fn, ctx, simplex, vals, n, nv, high);
            evaluationCount++;
            if (cv >= currentWorst)
            {
                for (int i = 0; i < nv; i++)
                    if (i != low)
                    {
                        for (int j = 0; j < n; j++) simplex[i * n + j] = (simplex[i * n + j] + simplex[low * n + j]) * 0.5;
                        vals[i] = fn(&simplex[i * n], n, ctx);
                    }
                evaluationCount += nv;
            }
        }
        if (evaluationCount >= max_iter) break;
    }
    for (int j = 0; j < n; j++) out[j] = simplex[low * n + j];
    free(simplex);
    free(vals);
}

/* ── F1: nucleo MathNet.Optimization (port fedele per parità col canone IL) ──
 * NumericalGradientObjective (forward difference H=1e-8), WeakWolfeLineSearch,
 * MinimizerBase.ExitCriteriaSatisfied. Riusato da BFGS/LBFGS/Powell/CG. */
typedef struct {
    double (*f)(const double*, int, void*);
    void* ctx;
    int n;
    double* point;
    double value;
    double* grad;
} mn_obj_t;

static void mn_init(mn_obj_t* o, double (*f)(const double*, int, void*), void* ctx, int n)
{
    o->f = f; o->ctx = ctx; o->n = n;
    o->point = (double*)malloc(sizeof(double) * (size_t)n);
    o->grad = (double*)malloc(sizeof(double) * (size_t)n);
    o->value = 0.0;
}
static void mn_free(mn_obj_t* o) { free(o->point); free(o->grad); }
static void mn_copy(mn_obj_t* dst, const mn_obj_t* src)
{
    for (int i = 0; i < src->n; i++) { dst->point[i] = src->point[i]; dst->grad[i] = src->grad[i]; }
    dst->value = src->value;
}
/* NumericalGradientObjective.EvaluateAt: value=f(x), grad[i]=(f(x+H e_i)-value)/H */
static void mn_evaluate(mn_obj_t* o, const double* x)
{
    const double H = 1e-8;
    for (int i = 0; i < o->n; i++) o->point[i] = x[i];
    o->value = o->f(x, o->n, o->ctx);
    double* xt = (double*)malloc(sizeof(double) * (size_t)o->n);
    for (int i = 0; i < o->n; i++) xt[i] = x[i];
    for (int i = 0; i < o->n; i++)
    {
        double sv = xt[i]; xt[i] += H;
        double fp = o->f(xt, o->n, o->ctx);
        xt[i] = sv;
        o->grad[i] = (fp - o->value) / H;
    }
    free(xt);
}
static double mn_dot(const double* a, const double* b, int n)
{ double s = 0; for (int i = 0; i < n; i++) s += a[i] * b[i]; return s; }

/* MinimizerBase.ExitCriteriaSatisfied (GetProjectedGradient = grad[ii]).
 * Ritorna 1 se UNA condizione di uscita è soddisfatta (ExitCondition != None). */
static int mn_exit_satisfied(mn_obj_t* cand, mn_obj_t* last, int iterations,
                             double gradTol, double paramTol, double funcTol)
{
    double relativeGradient = 0.0;
    double normalizer = fmax(fabs(cand->value), 1.0);
    for (int i = 0; i < cand->n; i++)
    {
        double tmp = cand->grad[i] * fmax(fabs(cand->point[i]), 1.0) / normalizer;
        relativeGradient = fmax(relativeGradient, fabs(tmp));
    }
    if (relativeGradient < gradTol) return 1;
    if (last)
    {
        double mostProgress = 0.0;
        for (int i = 0; i < cand->n; i++)
        {
            double tmp = fabs(cand->point[i] - last->point[i]) / fmax(fabs(last->point[i]), 1.0);
            mostProgress = fmax(mostProgress, tmp);
        }
        if (mostProgress < paramTol) return 1;
        double functionChange = cand->value - last->value;
        if (iterations > 500 && functionChange < 0 && fabs(functionChange) < funcTol) return 1;
    }
    return 0;
}

/* WolfeLineSearch.FindConformingStep (WeakWolfe): scrive l'ultima valutazione in out. */
static void mn_wolfe(mn_obj_t* start, const double* dir, double initialStep, double upperBound,
                     double c1, double c2, double paramTol, int maxIter, mn_obj_t* out, double* outStep)
{
    int n = start->n;
    double lowerBound = 0.0, step = initialStep;
    double initialValue = start->value;
    double initialDd = mn_dot(dir, start->grad, n);
    double* p = (double*)malloc(sizeof(double) * (size_t)n);
    for (int ii = 0; ii < maxIter; ii++)
    {
        for (int j = 0; j < n; j++) p[j] = start->point[j] + dir[j] * step;
        mn_evaluate(out, p);
        double stepDd = mn_dot(dir, out->grad, n);
        if (out->value > initialValue + c1 * step * initialDd)
        {
            upperBound = step; step = 0.5 * (lowerBound + upperBound);
        }
        else if (stepDd < c2 * initialDd)   /* WeakWolfeCondition */
        {
            lowerBound = step;
            step = isinf(upperBound) ? 2 * lowerBound : 0.5 * (lowerBound + upperBound);
        }
        else break;
        if (!isinf(upperBound))
        {
            double maxRelChange = 0.0;
            for (int j = 0; j < n; j++)
            {
                double tmp = fabs(dir[j] * (upperBound - lowerBound)) / fmax(fabs(out->point[j]), 1.0);
                maxRelChange = fmax(maxRelChange, tmp);
            }
            if (maxRelChange < paramTol) break;
        }
    }
    if (outStep) *outStep = step;
    free(p);
}

/* BFGS: replica esatta di MathNet BfgsMinimizer (tol=1e-6, max=200 da IlRuntime.Bfgs). */
void gmi_opt_bfgs(double (*fn)(const double*, int, void*), double* start, int n,
                  double tol, int max_iter, void* ctx, double* out)
{
    if (n <= 0) return;
    double gradTol = tol, paramTol = tol, funcTol = tol;
    double c1 = 1e-4, c2 = 0.9, lsTol = fmax(paramTol, 1e-10);
    int lsMax = 1000;
    double* H = (double*)calloc((size_t)n * n, sizeof(double));
    for (int i = 0; i < n; i++) H[i * n + i] = 1.0;
    double* dir = (double*)malloc(sizeof(double) * (size_t)n);
    double* stepv = (double*)malloc(sizeof(double) * (size_t)n);
    double* y = (double*)malloc(sizeof(double) * (size_t)n);
    double* Hy = (double*)malloc(sizeof(double) * (size_t)n);
    double* Hs = (double*)malloc(sizeof(double) * (size_t)n);
    double* newH = (double*)malloc(sizeof(double) * (size_t)n * n);
    mn_obj_t prev, cand, res;
    mn_init(&prev, fn, ctx, n); mn_init(&cand, fn, ctx, n); mn_init(&res, fn, ctx, n);

    mn_evaluate(&cand, start);
    if (mn_exit_satisfied(&cand, NULL, 0, gradTol, paramTol, funcTol)) { mn_copy(&res, &cand); goto finish; }

    for (int i = 0; i < n; i++) dir[i] = -cand.grad[i];
    {
        double stepSize = 100.0 * gradTol / mn_dot(dir, dir, n);
        mn_copy(&prev, &cand);                 /* previousPoint = objective (initial) */
        mn_wolfe(&cand, dir, stepSize, INFINITY, c1, c2, lsTol, lsMax, &res, NULL);
        for (int i = 0; i < n; i++) stepv[i] = res.point[i] - start[i];
        /* candidate = line search result */
        mn_copy(&cand, &res);
    }
    for (int iterations = 1; iterations < max_iter; iterations++)
    {
        /* CalculateSearchDirection: y = cand.grad - prev.grad; H update; dir=-H*g */
        for (int i = 0; i < n; i++) y[i] = cand.grad[i] - prev.grad[i];
        double sy = mn_dot(stepv, y, n);
        for (int i = 0; i < n; i++) { double s = 0; for (int k = 0; k < n; k++) s += H[i * n + k] * y[k]; Hy[i] = s; }
        double yHy = mn_dot(y, Hy, n);
        double coef1 = (sy + yHy) / (sy * sy);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                newH[i * n + j] = H[i * n + j]
                    + coef1 * stepv[i] * stepv[j]
                    - (Hy[i] * stepv[j] + stepv[i] * Hy[j]) * (1.0 / sy);
        memcpy(H, newH, sizeof(double) * (size_t)n * n);
        for (int i = 0; i < n; i++) { double s = 0; for (int k = 0; k < n; k++) s += H[i * n + k] * cand.grad[k]; dir[i] = -s; }
        if (mn_dot(dir, cand.grad, n) >= 0.0)
        {
            for (int i = 0; i < n; i++) dir[i] = -cand.grad[i];
            for (int i = 0; i < n * n; i++) H[i] = 0.0;
            for (int i = 0; i < n; i++) H[i * n + i] = 1.0;
        }
        /* line search 1.0 */
        mn_wolfe(&cand, dir, 1.0, INFINITY, c1, c2, lsTol, lsMax, &res, NULL);
        for (int i = 0; i < n; i++) stepv[i] = res.point[i] - cand.point[i];
        mn_copy(&prev, &cand);
        mn_copy(&cand, &res);
        if (mn_exit_satisfied(&cand, &prev, iterations, gradTol, paramTol, funcTol)) break;
    }
finish:
    for (int i = 0; i < n; i++) out[i] = cand.point[i];
    free(H); free(dir); free(stepv); free(y); free(Hy); free(Hs); free(newH);
    mn_free(&prev); mn_free(&cand); mn_free(&res);
}

/* ConjugateGradient: replica esatta di MathNet ConjugateGradientMinimizer
 * (WeakWolfe c1=1e-4 c2=0.1 paramTol=1e-4; tol=1e-6 max=200 da IlRuntime). */
void gmi_opt_cg(double (*fn)(const double*, int, void*), double* start, int n,
                double tol, int max_iter, void* ctx, double* out)
{
    if (n <= 0) return;
    double gradTol = tol;
    double c1 = 1e-4, c2 = 0.1, paramTol = 1e-4;
    int lsMax = 1000;
    double* dir = (double*)malloc(sizeof(double) * (size_t)n);
    double* steep = (double*)malloc(sizeof(double) * (size_t)n);
    double* prevSteep = (double*)malloc(sizeof(double) * (size_t)n);
    mn_obj_t obj, res;
    mn_init(&obj, fn, ctx, n); mn_init(&res, fn, ctx, n);
    mn_evaluate(&obj, start);
    double gn = 0; for (int i = 0; i < n; i++) gn += obj.grad[i] * obj.grad[i];
    if (sqrt(gn) < gradTol) { for (int i = 0; i < n; i++) out[i] = obj.point[i]; goto cg_done; }
    for (int i = 0; i < n; i++) steep[i] = -obj.grad[i];
    for (int i = 0; i < n; i++) dir[i] = steep[i];
    double stepSize = 100.0 * gradTol / mn_dot(obj.grad, obj.grad, n);
    mn_wolfe(&obj, dir, stepSize, INFINITY, c1, c2, paramTol, lsMax, &res, &stepSize);
    mn_copy(&obj, &res);
    {
        int iterations = 1;
        for (;;)
        {
            double g2 = 0; for (int i = 0; i < n; i++) g2 += obj.grad[i] * obj.grad[i];
            if (!(sqrt(g2) >= gradTol && iterations < max_iter)) break;
            for (int i = 0; i < n; i++) prevSteep[i] = steep[i];
            for (int i = 0; i < n; i++) steep[i] = -obj.grad[i];
            double num = 0, den = 0;
            for (int i = 0; i < n; i++) { num += steep[i] * (steep[i] - prevSteep[i]); den += prevSteep[i] * prevSteep[i]; }
            double adjuster = 0.0;
            if (den != 0.0) { adjuster = num / den; if (adjuster < 0.0) adjuster = 0.0; }
            for (int i = 0; i < n; i++) dir[i] = steep[i] + adjuster * dir[i];
            if (mn_dot(dir, obj.grad, n) >= 0.0) { for (int i = 0; i < n; i++) dir[i] = steep[i]; }
            mn_wolfe(&obj, dir, stepSize, INFINITY, c1, c2, paramTol, lsMax, &res, &stepSize);
            mn_copy(&obj, &res);
            iterations++;
        }
    }
    for (int i = 0; i < n; i++) out[i] = obj.point[i];
cg_done:
    free(dir); free(steep); free(prevSteep);
    mn_free(&obj); mn_free(&res);
}

/* LimitedMemoryBfgs: replica esatta di MathNet LimitedMemoryBfgsMinimizer.
 * CommonLib.LBfgs: memory=maxIterations(=200), maximumIterations=1000 (default). */
void gmi_opt_lbfgs(double (*fn)(const double*, int, void*), double* start, int n,
                   double tol, int memory, int max_iter, void* ctx, double* out)
{
    if (n <= 0) return;
    double gradTol = tol, paramTol = tol, funcTol = tol;
    double c1 = 1e-4, c2 = 0.9, lsTol = fmax(paramTol, 1e-10);
    int lsMax = 1000;
    if (memory < 1) memory = 1;
    double** yh = (double**)malloc(sizeof(double*) * (size_t)memory);
    double** sh = (double**)malloc(sizeof(double*) * (size_t)memory);
    double* rh = (double*)malloc(sizeof(double) * (size_t)memory);
    for (int i = 0; i < memory; i++) { yh[i] = (double*)malloc(sizeof(double) * (size_t)n); sh[i] = (double*)malloc(sizeof(double) * (size_t)n); }
    double* dir = (double*)malloc(sizeof(double) * (size_t)n);
    double* stepv = (double*)malloc(sizeof(double) * (size_t)n);
    double* ynew = (double*)malloc(sizeof(double) * (size_t)n);
    double* q = (double*)malloc(sizeof(double) * (size_t)n);
    double* alphas = (double*)malloc(sizeof(double) * (size_t)memory);
    mn_obj_t prev, cand, res;
    mn_init(&prev, fn, ctx, n); mn_init(&cand, fn, ctx, n); mn_init(&res, fn, ctx, n);
    int m = 0, iterations = 1;

    mn_evaluate(&cand, start);
    if (mn_exit_satisfied(&cand, NULL, 0, gradTol, paramTol, funcTol)) { mn_copy(&res, &cand); goto lb_done; }
    for (int i = 0; i < n; i++) dir[i] = -cand.grad[i];
    {
        double stepSize = 100.0 * gradTol / mn_dot(dir, dir, n);
        mn_copy(&prev, &cand);
        mn_wolfe(&cand, dir, stepSize, INFINITY, c1, c2, lsTol, lsMax, &res, NULL);
    }
    for (int i = 0; i < n; i++) stepv[i] = res.point[i] - start[i];
    for (int i = 0; i < n; i++) ynew[i] = res.grad[i] - prev.grad[i];
    memcpy(yh[0], ynew, sizeof(double) * (size_t)n);
    memcpy(sh[0], stepv, sizeof(double) * (size_t)n);
    rh[0] = 1.0 / mn_dot(ynew, stepv, n);
    m = 1;
    mn_copy(&prev, &res);

    for (;;)
    {
        int cont = (iterations < max_iter);
        iterations++;
        if (!cont) break;
        double gn = 0; for (int i = 0; i < n; i++) gn += prev.grad[i] * prev.grad[i];
        if (!(sqrt(gn) >= gradTol)) break;
        /* ApplyLbfgsUpdate → q; dir = -q */
        for (int i = 0; i < n; i++) q[i] = prev.grad[i];
        for (int k = m - 1; k >= 0; k--)
        {
            double al = rh[k] * mn_dot(q, sh[k], n);
            alphas[k] = al;
            for (int i = 0; i < n; i++) q[i] -= al * yh[k][i];
        }
        {
            double yk_dot_sk = mn_dot(yh[m - 1], sh[m - 1], n);
            double yk_dot_yk = mn_dot(yh[m - 1], yh[m - 1], n);
            double sc = yk_dot_sk / yk_dot_yk;
            for (int i = 0; i < n; i++) q[i] *= sc;
        }
        for (int k = 0; k < m; k++)
        {
            double beta = rh[k] * mn_dot(yh[k], q, n);
            for (int i = 0; i < n; i++) q[i] += sh[k][i] * (alphas[k] - beta);
        }
        for (int i = 0; i < n; i++) dir[i] = -q[i];
        mn_wolfe(&prev, dir, 1.0, INFINITY, c1, c2, lsTol, lsMax, &res, NULL);
        if (mn_exit_satisfied(&res, &prev, iterations, gradTol, paramTol, funcTol)) { mn_copy(&cand, &res); goto lb_done; }
        for (int i = 0; i < n; i++) stepv[i] = res.point[i] - prev.point[i];
        for (int i = 0; i < n; i++) ynew[i] = res.grad[i] - prev.grad[i];
        if (m >= memory)
        {
            for (int k = 1; k < memory; k++) { memcpy(yh[k - 1], yh[k], sizeof(double) * (size_t)n); memcpy(sh[k - 1], sh[k], sizeof(double) * (size_t)n); rh[k - 1] = rh[k]; }
            m = memory - 1;
        }
        memcpy(yh[m], ynew, sizeof(double) * (size_t)n);
        memcpy(sh[m], stepv, sizeof(double) * (size_t)n);
        rh[m] = 1.0 / mn_dot(ynew, stepv, n);
        m++;
        mn_copy(&prev, &res);
    }
    mn_copy(&cand, &prev);
lb_done:
    for (int i = 0; i < n; i++) out[i] = cand.point[i];
    for (int i = 0; i < memory; i++) { free(yh[i]); free(sh[i]); }
    free(yh); free(sh); free(rh); free(dir); free(stepv); free(ynew); free(q); free(alphas);
    mn_free(&prev); mn_free(&cand); mn_free(&res);
}


/* Secant: zero di fn tra x0 e x1. */
double gmi_opt_secant(double (*fn)(double, void*), double x0, double x1, double tol, int max_iter, void* ctx)
{
    double f0 = fn(x0, ctx), f1 = fn(x1, ctx);
    for (int i = 0; i < max_iter; i++)
    {
        if (f1 == f0) break;
        double nx = x1 - f1 * (x1 - x0) / (f1 - f0);
        x0 = x1; f0 = f1;
        x1 = nx; f1 = fn(x1, ctx);
        if (fabs(f1) < tol) return x1;
    }
    return x1;
}

/* ==================== Newton non lineare (pyfrbus) ==================== */

/* ---- Newton smorzato con riuso dello Jacobiano.
 * F(x)=0. J(x) n×n row-major. Se ||F_new|| > 0.5*||F_old||, ricalcola LU.
 * max_iter, tol. Ritorna 0/-1. ---- */
int gor_newton_damped(double (*F)(const double *x, double *out, void *ctx),
                      double (*J)(const double *x, double *out, void *ctx),
                      void *ctx, double *x, int n, int max_iter, double tol)
{
    double *fx, *fn, *Jn, *delta;
    int iter, i, ok = 0;
    if (!F || !J || !x || n < 1 || max_iter < 1) return -1;
    fx = (double *)malloc((size_t)n * sizeof(double));
    fn = (double *)malloc((size_t)n * sizeof(double));
    Jn = (double *)malloc((size_t)n * n * sizeof(double));
    delta = (double *)malloc((size_t)n * sizeof(double));
    if (!fx || !fn || !Jn || !delta) { free(fx); free(fn); free(Jn); free(delta); return -1; }
    F(x, fx, ctx);
    for (iter = 0; iter < max_iter; iter++) {
        double nf = 0.0, nf2, alpha = 1.0;
        for (i = 0; i < n; i++) nf += fx[i] * fx[i];
        nf = sqrt(nf);
        if (nf < tol) { ok = 1; break; }
        J(x, Jn, ctx);
        /* risolvi J * delta = -fx */
        for (i = 0; i < n; i++) delta[i] = -fx[i];
        if (gm_linalg_solve(n, Jn, delta, delta) != GM_OK) { ok = 0; break; }
        /* smorzamento */
        for (;;) {
            double nx[HFT_MAXQ];
            double nfn = 0.0;
            if (n > HFT_MAXQ) break;
            for (i = 0; i < n; i++) nx[i] = x[i] + alpha * delta[i];
            F(nx, fn, ctx);
            nfn = 0.0;
            for (i = 0; i < n; i++) nfn += fn[i] * fn[i];
            nfn = sqrt(nfn);
            if (nfn < nf || alpha < 1e-6) {
                for (i = 0; i < n; i++) x[i] = nx[i];
                for (i = 0; i < n; i++) fx[i] = fn[i];
                nf2 = nfn;
                (void)nf2;
                break;
            }
            alpha *= 0.5;
        }
    }
    free(fx); free(fn); free(Jn); free(delta);
    return ok ? 0 : -1;
}
/* ---- Dogleg trust-region (pyfrbus trust()). ---- */
int gor_dogleg(double (*F)(const double *x, double *out, void *ctx),
               double (*J)(const double *x, double *out, void *ctx),
               void *ctx, double *x, int n, int max_iter, double tol)
{
    double *fx, *fn, *Jn, *g, *pc, *pn, *p, *tmp;
    double rho_max = 1e3;
    double delta = 1.0;
    int iter, i, j, ok = 0;
    if (!F || !J || !x || n < 1 || max_iter < 1 || n > HFT_MAXQ) return -1;
    fx = (double *)malloc((size_t)n * sizeof(double));
    fn = (double *)malloc((size_t)n * sizeof(double));
    Jn = (double *)malloc((size_t)n * n * sizeof(double));
    g = (double *)malloc((size_t)n * sizeof(double));
    pc = (double *)malloc((size_t)n * sizeof(double));
    pn = (double *)malloc((size_t)n * sizeof(double));
    p = (double *)malloc((size_t)n * sizeof(double));
    tmp = (double *)malloc((size_t)n * sizeof(double));
    if (!fx || !fn || !Jn || !g || !pc || !pn || !p || !tmp) {
        free(fx); free(fn); free(Jn); free(g); free(pc); free(pn); free(p); free(tmp); return -1;
    }
    F(x, fx, ctx);
    for (iter = 0; iter < max_iter; iter++) {
        double nf = 0.0;
        for (i = 0; i < n; i++) nf += fx[i] * fx[i];
        nf = sqrt(nf);
        if (nf < tol) { ok = 1; break; }
        J(x, Jn, ctx);
        /* g = J^T F */
        for (i = 0; i < n; i++) { g[i] = 0.0; for (j = 0; j < n; j++) g[i] += Jn[j * n + i] * fx[j]; }
        {
            double gn2 = 0.0;
            for (i = 0; i < n; i++) gn2 += g[i] * g[i];
            /* Cauchy point p_c = -t * (rho/||g||^2) g */
            double t = 1.0;
            if (gn2 > 1e-12) {
                double denom = 0.0;
                for (i = 0; i < n; i++) {
                    double ji = 0.0;
                    for (j = 0; j < n; j++) ji += Jn[i * n + j] * g[j];
                    denom += ji * ji;
                }
                if (denom > 1e-12) {
                    double num = gn2 * gn2 / denom;
                    t = num / (gn2 > 0 ? gn2 : 1.0) < 1.0 ? num / (gn2 > 0 ? gn2 : 1.0) : 1.0;
                }
                for (i = 0; i < n; i++) pc[i] = -t * (gn2 > 0 ? 1.0 / gn2 : 0.0) * g[i];
            } else {
                for (i = 0; i < n; i++) pc[i] = 0.0;
            }
        }
        /* Newton point pn = -J^{-1} F */
        for (i = 0; i < n; i++) pn[i] = -fx[i];
        if (gm_linalg_solve(n, Jn, pn, pn) != GM_OK) { ok = 0; break; }
        /* dogleg: se ||pn|| <= delta usa pn, else interpolazione cauchy->newton */
        {
            double npn = 0.0;
            for (i = 0; i < n; i++) npn += pn[i] * pn[i];
            npn = sqrt(npn);
            if (npn <= delta) { for (i = 0; i < n; i++) p[i] = pn[i]; }
            else {
                double npc = 0.0;
                for (i = 0; i < n; i++) npc += pc[i] * pc[i];
                npc = sqrt(npc);

                if (npc >= delta) {
                    for (i = 0; i < n; i++) p[i] = pc[i] * (delta / (npc > 0 ? npc : 1.0));
                } else {
                    /* interpolazione: pc + tau (pn - pc), trova tau in [0,1] con ||p||=delta */
                    double a2 = 0.0, b2 = 0.0, c2 = 0.0, tau;
                    for (i = 0; i < n; i++) {
                        double d = pn[i] - pc[i];
                        a2 += d * d; b2 += 2.0 * pc[i] * d; c2 += pc[i] * pc[i];
                    }
                    c2 -= delta * delta;
                    {
                        double disc = b2 * b2 - 4.0 * a2 * c2;
                        tau = disc >= 0 ? (-b2 + sqrt(disc)) / (2.0 * a2 + 1e-12) : 0.5;
                        if (tau < 0) tau = 0; if (tau > 1) tau = 1;
                    }
                    for (i = 0; i < n; i++) p[i] = pc[i] + tau * (pn[i] - pc[i]);
                }
            }
        }
        /* valutazione riduzione */
        {
            double nx[HFT_MAXQ], fn_new, model;
            double nf2;
            for (i = 0; i < n; i++) nx[i] = x[i] + p[i];
            F(nx, fn, ctx);
            fn_new = 0.0;
            for (i = 0; i < n; i++) fn_new += fn[i] * fn[i];
            /* modello = 0.5||F||^2 + p^T g + 0.5 p^T J^T J p */
            model = 0.5 * nf * nf;
            for (i = 0; i < n; i++) model += p[i] * g[i];
            for (i = 0; i < n; i++) {
                double ji = 0.0;
                for (j = 0; j < n; j++) ji += Jn[i * n + j] * p[j];
                model += 0.5 * ji * ji;
            }
            {
                double rho = (0.5 * nf * nf - 0.5 * fn_new) / (model + 1e-12);
                if (rho < 0.25) delta *= 0.5;
                else if (rho > 0.75 && delta < rho_max) delta *= 2.0;
                if (rho > 0) {
                    for (i = 0; i < n; i++) x[i] = nx[i];
                    for (i = 0; i < n; i++) fx[i] = fn[i];
                }
            }
            nf2 = sqrt(fn_new);
            (void)nf2;
        }
    }
    free(fx); free(fn); free(Jn); free(g); free(pc); free(pn); free(p); free(tmp);
    return ok ? 0 : -1;
}
