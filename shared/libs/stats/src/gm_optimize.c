/*
 * gm_optimize.c — ottimizzazione numerica (da scipy.optimize).
 */
#include "gm_optimize.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ---- Nelder-Mead ---- */
gm_opt_result gm_opt_nelder_mead(gm_opt_fn f, void *ctx, gm_real *x, int d,
                                 gm_real tol, int maxit)
{
    gm_opt_result res = {0.0, 0, 0};
    gm_real *sim, *fc, *xc, *xr, *xe, *xcc, *cent;
    int n, i, j, it;
    if (!f || !x || d < 1) return res;
    if (maxit < 1) maxit = 200 * d;
    if (tol <= 0) tol = 1e-8;
    n = d + 1;
    sim = (gm_real *)malloc((size_t)n * d * sizeof(gm_real));
    fc = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    xc = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    xr = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    xe = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    xcc = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    cent = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    if (!sim || !fc || !xc || !xr || !xe || !xcc || !cent) {
        free(sim); free(fc); free(xc); free(xr); free(xe); free(xcc); free(cent); return res;
    }
    for (j = 0; j < d; j++) sim[j] = x[j];
    for (i = 1; i < n; i++) {
        for (j = 0; j < d; j++) sim[(size_t)i * d + j] = x[j];
        sim[(size_t)i * d + (i - 1)] += (x[i - 1] != 0.0 ? 0.05 * x[i - 1] : 0.00025);
    }
    for (i = 0; i < n; i++) fc[i] = f(sim + (size_t)i * d, d, ctx);
    for (it = 0; it < maxit; it++) {
        int lo = 0, hi = 0;
        for (i = 1; i < n; i++) { if (fc[i] < fc[lo]) lo = i; if (fc[i] > fc[hi]) hi = i; }
        { gm_real spread = 0.0;
          for (i = 0; i < n; i++) { gm_real dd = fabs(fc[i] - fc[lo]); if (dd > spread) spread = dd; }
          if (spread < tol) break; }
        for (j = 0; j < d; j++) { gm_real s = 0.0; for (i = 0; i < n; i++) if (i != hi) s += sim[(size_t)i * d + j]; cent[j] = s / (n - 1); }
        for (j = 0; j < d; j++) xr[j] = cent[j] + (cent[j] - sim[(size_t)hi * d + j]);
        { gm_real fr = f(xr, d, ctx);
          if (fr < fc[lo]) {
              for (j = 0; j < d; j++) xe[j] = cent[j] + 2.0 * (cent[j] - sim[(size_t)hi * d + j]);
              { gm_real fe = f(xe, d, ctx);
                if (fe < fr) { memcpy(sim + (size_t)hi * d, xe, (size_t)d * sizeof(gm_real)); fc[hi] = fe; }
                else { memcpy(sim + (size_t)hi * d, xr, (size_t)d * sizeof(gm_real)); fc[hi] = fr; } }
          } else if (fr < fc[hi]) {
              memcpy(sim + (size_t)hi * d, xr, (size_t)d * sizeof(gm_real)); fc[hi] = fr;
          } else {
              for (j = 0; j < d; j++) xcc[j] = cent[j] + 0.5 * (sim[(size_t)hi * d + j] - cent[j]);
              { gm_real fcc = f(xcc, d, ctx);
                if (fcc < fc[hi]) { memcpy(sim + (size_t)hi * d, xcc, (size_t)d * sizeof(gm_real)); fc[hi] = fcc; }
                else { for (i = 0; i < n; i++) if (i != lo) { for (j = 0; j < d; j++) sim[(size_t)i * d + j] = 0.5 * (sim[(size_t)i * d + j] + sim[(size_t)lo * d + j]); fc[i] = f(sim + (size_t)i * d, d, ctx); } } }
          }
        }
    }
    { int lo = 0; for (i = 1; i < n; i++) if (fc[i] < fc[lo]) lo = i;
      for (j = 0; j < d; j++) x[j] = sim[(size_t)lo * d + j]; res.fval = fc[lo]; }
    res.iters = it; res.success = 1;
    free(sim); free(fc); free(xc); free(xr); free(xe); free(xcc); free(cent);
    return res;
}

/* ---- BFGS con gradiente numerico ---- */
static void num_grad(gm_opt_fn f, void *ctx, const gm_real *x, int d, gm_real *g)
{
    gm_real h = 1e-6; int i;
    for (i = 0; i < d; i++) {
        gm_real xi = x[i];
        ((gm_real *)x)[i] = xi + h;
        { gm_real fp = f(x, d, ctx); ((gm_real *)x)[i] = xi - h; { gm_real fm = f(x, d, ctx); g[i] = (fp - fm) / (2.0 * h); } }
        ((gm_real *)x)[i] = xi;
    }
}

gm_opt_result gm_opt_bfgs(gm_opt_fn f, void *ctx, gm_real *x, int d, gm_real tol, int maxit)
{
    gm_opt_result res = {0.0, 0, 0};
    gm_real *g, *gn, *p, *H, *s, *y, *tmp;
    int i, j, k, it;
    if (!f || !x || d < 1) return res;
    if (maxit < 1) maxit = 200;
    if (tol <= 0) tol = 1e-8;
    g = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    gn = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    p = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    H = (gm_real *)calloc((size_t)d * d, sizeof(gm_real));
    s = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    y = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    tmp = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    if (!g || !gn || !p || !H || !s || !y || !tmp) { free(g); free(gn); free(p); free(H); free(s); free(y); free(tmp); return res; }
    for (i = 0; i < d; i++) H[(size_t)i * d + i] = 1.0;
    num_grad(f, ctx, x, d, g);
    for (it = 0; it < maxit; it++) {
        gm_real gn2 = 0.0, alpha, f0, slope;
        for (i = 0; i < d; i++) gn2 += g[i] * g[i];
        if (sqrt(gn2) < tol) break;
        for (i = 0; i < d; i++) { p[i] = 0.0; for (j = 0; j < d; j++) p[i] -= H[(size_t)i * d + j] * g[j]; }
        slope = 0.0; for (i = 0; i < d; i++) slope += g[i] * p[i];
        if (slope >= 0) { for (i = 0; i < d; i++) p[i] = -g[i]; slope = -gn2; }
        /* line search backtracking */
        f0 = f(x, d, ctx);
        alpha = 1.0;
        for (k = 0; k < 40; k++) {
            for (i = 0; i < d; i++) tmp[i] = x[i] + alpha * p[i];
            if (f(tmp, d, ctx) < f0 + 1e-4 * alpha * slope) break;
            alpha *= 0.5;
        }
        for (i = 0; i < d; i++) { s[i] = alpha * p[i]; x[i] += s[i]; }
        num_grad(f, ctx, x, d, gn);
        for (i = 0; i < d; i++) y[i] = gn[i] - g[i];
        /* BFGS update: H = (I - rho s y^T) H (I - rho y s^T) + rho s s^T */
        {
            gm_real sy = 0.0, rho;
            for (i = 0; i < d; i++) sy += s[i] * y[i];
            if (sy > 1e-12) {
                gm_real *Hy = (gm_real *)malloc((size_t)d * sizeof(gm_real));
                gm_real *Hs = (gm_real *)malloc((size_t)d * sizeof(gm_real));
                if (Hy && Hs) {
                    rho = 1.0 / sy;
                    for (i = 0; i < d; i++) { Hy[i] = 0; Hs[i] = 0; for (j = 0; j < d; j++) { Hy[i] += H[(size_t)i * d + j] * y[j]; Hs[i] += H[(size_t)i * d + j] * s[j]; } }
                    for (i = 0; i < d; i++)
                        for (j = 0; j < d; j++)
                            H[(size_t)i * d + j] += rho * s[i] * s[j] - rho * (Hy[i] * s[j] + s[i] * Hy[j]) + rho * rho * sy * Hy[i] * Hy[j] + rho * (Hs[i] * y[j] + y[i] * Hs[j]) - rho * rho * sy * Hs[i] * Hs[j];
                }
                free(Hy); free(Hs);
            }
        }
        memcpy(g, gn, (size_t)d * sizeof(gm_real));
    }
    res.fval = f(x, d, ctx); res.iters = it; res.success = 1;
    free(g); free(gn); free(p); free(H); free(s); free(y); free(tmp);
    return res;
}

gm_opt_result gm_opt_conjugate_gradient(gm_opt_fn f, void *ctx, gm_real *x, int d, gm_real tol, int maxit)
{
    gm_opt_result res = {0.0, 0, 0};
    gm_real *g, *gn, *p, *tmp;
    int i, it, k;
    if (!f || !x || d < 1) return res;
    if (maxit < 1) maxit = 500;
    g = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    gn = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    p = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    tmp = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    if (!g || !gn || !p || !tmp) { free(g); free(gn); free(p); free(tmp); return res; }
    num_grad(f, ctx, x, d, g);
    for (i = 0; i < d; i++) p[i] = -g[i];
    for (it = 0; it < maxit; it++) {
        gm_real gn2 = 0.0, alpha, f0, slope, beta, num = 0.0, den = 0.0;
        for (i = 0; i < d; i++) gn2 += g[i] * g[i];
        if (sqrt(gn2) < tol) break;
        slope = 0.0; for (i = 0; i < d; i++) slope += g[i] * p[i];
        f0 = f(x, d, ctx); alpha = 1.0;
        for (k = 0; k < 40; k++) { for (i = 0; i < d; i++) tmp[i] = x[i] + alpha * p[i]; if (f(tmp, d, ctx) < f0 + 1e-4 * alpha * slope) break; alpha *= 0.5; }
        for (i = 0; i < d; i++) x[i] += alpha * p[i];
        num_grad(f, ctx, x, d, gn);
        for (i = 0; i < d; i++) { num += gn[i] * (gn[i] - g[i]); den += g[i] * g[i]; }
        beta = den > 1e-30 ? num / den : 0.0;
        for (i = 0; i < d; i++) p[i] = -gn[i] + beta * p[i];
        memcpy(g, gn, (size_t)d * sizeof(gm_real));
    }
    res.fval = f(x, d, ctx); res.iters = it; res.success = 1;
    free(g); free(gn); free(p); free(tmp);
    return res;
}

/* ---- 1D ---- */
gm_real gm_opt_golden_section(gm_real (*f)(gm_real, void *), void *ctx, gm_real a, gm_real b, gm_real tol)
{
    const gm_real gr = 0.6180339887498949;
    gm_real c, dd;
    if (tol <= 0) tol = 1e-8;
    c = b - gr * (b - a); dd = a + gr * (b - a);
    while (fabs(b - a) > tol) {
        if (f(c, ctx) < f(dd, ctx)) b = dd; else a = c;
        c = b - gr * (b - a); dd = a + gr * (b - a);
    }
    return 0.5 * (a + b);
}

gm_real gm_opt_brent(gm_real (*f)(gm_real, void *), void *ctx, gm_real a, gm_real b, gm_real tol)
{
    gm_real gr = 0.3819660112501051, x, w, v, fx, fw, fv, d = 0, e = 0;
    int it;
    if (tol <= 0) tol = 1e-8;
    x = w = v = a + gr * (b - a);
    fx = fw = fv = f(x, ctx);
    for (it = 0; it < 200; it++) {
        gm_real m = 0.5 * (a + b), tol1 = tol * fabs(x) + 1e-10, tol2 = 2.0 * tol1;
        if (fabs(x - m) <= tol2 - 0.5 * (b - a)) break;
        if (fabs(e) > tol1) {
            gm_real r = (x - w) * (fx - fv), q = (x - v) * (fx - fw), p = (x - v) * q - (x - w) * r;
            q = 2.0 * (q - r);
            if (q > 0) p = -p; else q = -q;
            r = e; e = d;
            if (fabs(p) < fabs(0.5 * q * r) && p > q * (a - x) && p < q * (b - x)) {
                d = p / q; { gm_real u = x + d; if (u - a < tol2 || b - u < tol2) d = (m > x ? tol1 : -tol1); }
            } else { e = (x < m ? b - x : a - x); d = gr * e; }
        } else { e = (x < m ? b - x : a - x); d = gr * e; }
        { gm_real u = x + (fabs(d) >= tol1 ? d : (d > 0 ? tol1 : -tol1));
          gm_real fu = f(u, ctx);
          if (fu <= fx) { if (u < x) b = x; else a = x; v = w; fv = fw; w = x; fw = fx; x = u; fx = fu; }
          else { if (u < x) a = u; else b = u; if (fu <= fw || w == x) { v = w; fv = fw; w = u; fw = fu; } else if (fu <= fv || v == x || v == w) { v = u; fv = fu; } } }
    }
    return x;
}

/* ---- Levenberg-Marquardt ---- */
gm_opt_result gm_opt_levenberg_marquardt(gm_opt_res_fn r, void *ctx, gm_real *x, int d,
                                         int m, gm_real tol, int maxit)
{
    gm_opt_result res = {0.0, 0, 0};
    gm_real *res0, *J, *JtJ, *Jtr, *dx, *rt, *xnew;
    gm_real lambda = 1e-3;
    int i, j, k, it;
    if (!r || !x || d < 1 || m < 1) return res;
    if (maxit < 1) maxit = 100;
    if (tol <= 0) tol = 1e-8;
    res0 = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    J = (gm_real *)malloc((size_t)m * d * sizeof(gm_real));
    JtJ = (gm_real *)malloc((size_t)d * d * sizeof(gm_real));
    Jtr = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    dx = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    rt = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    xnew = (gm_real *)malloc((size_t)d * sizeof(gm_real));
    if (!res0 || !J || !JtJ || !Jtr || !dx || !rt || !xnew) { free(res0); free(J); free(JtJ); free(Jtr); free(dx); free(rt); free(xnew); return res; }
    r(x, d, ctx, res0, m);
    for (it = 0; it < maxit; it++) {
        gm_real cost = 0.0;
        for (i = 0; i < m; i++) cost += res0[i] * res0[i];
        if (cost < tol) break;
        /* Jacobiano numerico */
        for (j = 0; j < d; j++) {
            gm_real h = 1e-6, xj = x[j];
            x[j] = xj + h; r(x, d, ctx, rt, m);
            x[j] = xj;
            for (i = 0; i < m; i++) J[(size_t)i * d + j] = (rt[i] - res0[i]) / h;
        }
        for (i = 0; i < d; i++) {
            for (j = 0; j < d; j++) { gm_real s = 0.0; for (k = 0; k < m; k++) s += J[(size_t)k * d + i] * J[(size_t)k * d + j]; JtJ[(size_t)i * d + j] = s; }
            { gm_real s = 0.0; for (k = 0; k < m; k++) s += J[(size_t)k * d + i] * res0[k]; Jtr[i] = s; }
        }
        /* (JtJ + lambda diag) dx = -Jtr */
        {
            gm_real *A = (gm_real *)malloc((size_t)d * d * sizeof(gm_real));
            gm_real *bb = (gm_real *)malloc((size_t)d * sizeof(gm_real));
            int ok = A && bb;
            if (ok) {
                int p, q;
                memcpy(A, JtJ, (size_t)d * d * sizeof(gm_real));
                for (k = 0; k < d; k++) A[(size_t)k * d + k] *= (1.0 + lambda);
                for (k = 0; k < d; k++) bb[k] = -Jtr[k];
                for (p = 0; p < d && ok; p++) {
                    int piv = p; gm_real mx = fabs(A[(size_t)p * d + p]);
                    for (q = p + 1; q < d; q++) if (fabs(A[(size_t)q * d + p]) > mx) { mx = fabs(A[(size_t)q * d + p]); piv = q; }
                    if (mx < 1e-14) { ok = 0; break; }
                    if (piv != p) { for (q = 0; q < d; q++) { gm_real t = A[(size_t)p*d+q]; A[(size_t)p*d+q] = A[(size_t)piv*d+q]; A[(size_t)piv*d+q] = t; } { gm_real t = bb[p]; bb[p] = bb[piv]; bb[piv] = t; } }
                    for (q = p + 1; q < d; q++) { gm_real fac = A[(size_t)q * d + p] / A[(size_t)p * d + p]; int w; for (w = p; w < d; w++) A[(size_t)q * d + w] -= fac * A[(size_t)p * d + w]; bb[q] -= fac * bb[p]; }
                }
                if (ok) for (p = d - 1; p >= 0; p--) { gm_real sm = bb[p]; for (q = p + 1; q < d; q++) sm -= A[(size_t)p * d + q] * dx[q]; dx[p] = sm / A[(size_t)p * d + p]; }
            }
            free(A); free(bb);
            if (!ok) break;
        }
        for (i = 0; i < d; i++) xnew[i] = x[i] + dx[i];
        r(xnew, d, ctx, rt, m);
        { gm_real costnew = 0.0; for (i = 0; i < m; i++) costnew += rt[i] * rt[i];
          if (costnew < cost) { memcpy(x, xnew, (size_t)d * sizeof(gm_real)); memcpy(res0, rt, (size_t)m * sizeof(gm_real)); lambda *= 0.3; if (lambda < 1e-10) lambda = 1e-10; }
          else { lambda *= 3.0; if (lambda > 1e10) lambda = 1e10; } }
    }
    { gm_real cost = 0.0; for (i = 0; i < m; i++) cost += res0[i] * res0[i]; res.fval = cost; }
    res.iters = it; res.success = 1;
    free(res0); free(J); free(JtJ); free(Jtr); free(dx); free(rt); free(xnew);
    return res;
}

typedef struct { gm_opt_model_fn model; void *ctx; const gm_real *x; const gm_real *y; int m; } cf_ctx;
static void cf_res(const gm_real *p, int d, void *vctx, gm_real *res, int m)
{
    cf_ctx *c = (cf_ctx *)vctx;
    int i;
    for (i = 0; i < m; i++) res[i] = c->model(c->x[i], p, d, c->ctx) - c->y[i];
}
gm_opt_result gm_opt_curve_fit(gm_opt_model_fn model, void *ctx, const gm_real *x,
                               const gm_real *y, int m, gm_real *p, int d,
                               gm_real tol, int maxit)
{
    cf_ctx c;
    c.model = model; c.ctx = ctx; c.x = x; c.y = y; c.m = m;
    return gm_opt_levenberg_marquardt(cf_res, &c, p, d, m, tol, maxit);
}
