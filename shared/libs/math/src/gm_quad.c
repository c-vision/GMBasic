/* gm_quad.c — quadratura numerica (riscrittura). */
#include "gm_quad.h"

#include <math.h>
#include <stdlib.h>

gm_real gm_quad_trapezoid(gm_fn1 f, void *ctx, gm_real a, gm_real b, int n)
{
    gm_real h = (b - a) / (gm_real)n, s;
    int i;
    if (n < 1) return 0.0;
    s = 0.5 * (f(a, ctx) + f(b, ctx));
    for (i = 1; i < n; i++) s += f(a + h * (gm_real)i, ctx);
    return s * h;
}

gm_real gm_quad_midpoint(gm_fn1 f, void *ctx, gm_real a, gm_real b, int n)
{
    gm_real h = (b - a) / (gm_real)n, s = 0.0;
    int i;
    if (n < 1) return 0.0;
    for (i = 0; i < n; i++) s += f(a + h * ((gm_real)i + 0.5), ctx);
    return s * h;
}

gm_real gm_quad_simpson(gm_fn1 f, void *ctx, gm_real a, gm_real b, int n)
{
    gm_real h, s;
    int i;
    if (n < 2) return gm_quad_trapezoid(f, ctx, a, b, n < 1 ? 1 : n);
    if (n % 2) n++; /* n pari */
    h = (b - a) / (gm_real)n;
    s = f(a, ctx) + f(b, ctx);
    for (i = 1; i < n; i++) s += ((i % 2) ? 4.0 : 2.0) * f(a + h * (gm_real)i, ctx);
    return s * h / 3.0;
}

static void legendre_pd(int n, gm_real x, gm_real *p, gm_real *dp)
{
    gm_real p0 = 1.0, p1 = x;
    int k;
    if (n == 0) { *p = 1.0; *dp = 0.0; return; }
    if (n == 1) { *p = x; *dp = 1.0; return; }
    for (k = 2; k <= n; k++) {
        gm_real p2 = ((2.0 * k - 1.0) * x * p1 - (k - 1.0) * p0) / (gm_real)k;
        p0 = p1; p1 = p2;
    }
    *p = p1;
    *dp = (gm_real)n * (x * p1 - p0) / (x * x - 1.0);
}

gm_status gm_gauss_legendre_nodes(int n, gm_real *x, gm_real *w)
{
    int i, k;
    if (x == NULL || w == NULL || n < 1) return GM_ERR_ARG;
    for (i = 0; i < (n + 1) / 2; i++) {
        gm_real xi = cos(3.14159265358979323846 * ((gm_real)i + 0.75) / ((gm_real)n + 0.5));
        gm_real p, dp;
        for (k = 0; k < 100; k++) {
            legendre_pd(n, xi, &p, &dp);
            {
                gm_real dx = p / dp;
                xi -= dx;
                if (fabs(dx) < 1e-15) break;
            }
        }
        legendre_pd(n, xi, &p, &dp);
        x[i] = -xi; x[n - 1 - i] = xi;
        w[i] = w[n - 1 - i] = 2.0 / ((1.0 - xi * xi) * dp * dp);
    }
    return GM_OK;
}

gm_real gm_quad_gauss_legendre(gm_fn1 f, void *ctx, gm_real a, gm_real b, int n)
{
    gm_real *x, *w, s = 0.0;
    int i;
    if (n < 1) return 0.0;
    x = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!x || !w) { free(x); free(w); return 0.0; }
    gm_gauss_legendre_nodes(n, x, w);
    for (i = 0; i < n; i++) {
        gm_real t = 0.5 * (b - a) * x[i] + 0.5 * (b + a);
        s += w[i] * f(t, ctx);
    }
    free(x); free(w);
    return 0.5 * (b - a) * s;
}

/* G7-K15 (nodi positivi; simmetrici). */
static const gm_real k15x[8] = {
    0.991455371120813, 0.949107912342759, 0.864864423359769, 0.741531185599394,
    0.586087235467691, 0.405845151377397, 0.207784955007898, 0.000000000000000
};
static const gm_real k15w[8] = {
    0.022935322010529, 0.063092092629979, 0.104790010322250, 0.140653259715525,
    0.169004726639267, 0.190350578064785, 0.204432940075298, 0.209482141084728
};
static const gm_real g7w[4] = {
    0.129484966168870, 0.279705391489277, 0.381830050505119, 0.417959183673469
};

gm_real gm_quad_gauss_kronrod(gm_fn1 f, void *ctx, gm_real a, gm_real b, gm_real *err)
{
    gm_real c = 0.5 * (b - a), d = 0.5 * (b + a);
    gm_real k = 0.0, g = 0.0;
    int i;
    for (i = 0; i < 8; i++) {
        if (k15x[i] == 0.0) {
            gm_real fv = f(d, ctx);
            k += k15w[i] * fv;
            g += g7w[3] * fv;
        } else {
            gm_real f1 = f(d + c * k15x[i], ctx), f2 = f(d - c * k15x[i], ctx);
            k += k15w[i] * (f1 + f2);
            /* nodi Gauss corrispondenti: 1,3,5 -> indici 1,3,5 e g7w 0,1,2 */
            if (i == 1) g += g7w[0] * (f1 + f2);
            else if (i == 3) g += g7w[1] * (f1 + f2);
            else if (i == 5) g += g7w[2] * (f1 + f2);
        }
    }
    if (err) *err = fabs(k - g) * c;
    return k * c;
}

gm_real gm_quad_romberg(gm_fn1 f, void *ctx, gm_real a, gm_real b, gm_real tol, int maxit)
{
    gm_real *T, res;
    int i, j, k, kend = 0;
    if (maxit < 1) return 0.0;
    T = (gm_real *)calloc((size_t)maxit * (size_t)maxit, sizeof(gm_real));
    if (!T) return 0.0;
    T[0] = 0.5 * (b - a) * (f(a, ctx) + f(b, ctx));
    for (k = 1; k < maxit; k++) {
        int n = 1 << (k - 1);
        gm_real H = (b - a) / (gm_real)n, h = 0.5 * H, sum = 0.0;
        for (i = 0; i < n; i++) sum += f(a + H * ((gm_real)i + 0.5), ctx);
        T[(size_t)k * (size_t)maxit] = 0.5 * T[(size_t)(k - 1) * (size_t)maxit] + h * sum;
        for (j = 1; j <= k; j++) {
            gm_real p = pow(4.0, (gm_real)j);
            T[(size_t)k * (size_t)maxit + (size_t)j] =
                T[(size_t)k * (size_t)maxit + (size_t)(j - 1)]
                + (T[(size_t)k * (size_t)maxit + (size_t)(j - 1)]
                   - T[(size_t)(k - 1) * (size_t)maxit + (size_t)(j - 1)]) / (p - 1.0);
        }
        kend = k;
        if (fabs(T[(size_t)k * (size_t)maxit + (size_t)k]
                 - T[(size_t)(k - 1) * (size_t)maxit + (size_t)(k - 1)])
            <= tol * (1.0 + fabs(T[(size_t)k * (size_t)maxit + (size_t)k])))
            break;
    }
    res = T[(size_t)kend * (size_t)maxit + (size_t)kend];
    free(T);
    return res;
}

/* Gauss-Hermite (nodi/pesi per int f(x) e^{-x^2} dx su R). Polinomi di Hermite
 * del "fisico": H_0=1, H_1=2x, H_{j+1}=2x H_j - 2j H_{j-1}; radici via Newton
 * con H_n'(x) = 2n H_{n-1}(x); pesi 2^{n+1} n! sqrt(pi) / H_n'(x)^2.
 * Da estimating-dynamic-corporate (#176, clean-room). n <= 100. */
gm_status gm_gauss_hermite_nodes(int n, gm_real *x, gm_real *w)
{
    const gm_real eps = 3.0e-14;
    const gm_real pi = 3.14159265358979323846;
    int i, its, j, m;
    gm_real z = 0.0, z1 = 0.0, pp = 0.0;
    if (!x || !w || n < 1 || n > 100) return GM_ERR_ARG;
    m = (n + 1) / 2;
    for (i = 0; i < m; i++) {
        if (i == 0)
            z = sqrt(2.0 * n + 1.0 - 1.85575 * pow(2.0 * n + 1.0, -1.0 / 6.0));
        else if (i == 1)
            z -= 1.14 * pow((gm_real)n, 0.426) / z;
        else if (i == 2)
            z = 1.86 * z - 0.86 * x[0];
        else if (i == 3)
            z = 1.91 * z - 0.91 * x[1];
        else
            z = 2.0 * z - x[i - 2];
        for (its = 0; its < 100; its++) {
            gm_real h0 = 1.0, h1 = 2.0 * z, hval, hprev;
            for (j = 1; j < n; j++) {
                gm_real h2 = 2.0 * z * h1 - 2.0 * (gm_real)j * h0;
                h0 = h1; h1 = h2;
            }
            hprev = h0;   /* H_{n-1} */
            hval = h1;    /* H_n */
            pp = 2.0 * (gm_real)n * hprev;   /* H_n' */
            z1 = z;
            z = z1 - hval / pp;
            if (fabs(z - z1) <= eps) break;
        }
        if (its >= 100) return GM_ERR_NOCONV;
        x[i] = z;
        x[n - 1 - i] = -z;
        w[i] = pow(2.0, (gm_real)(n + 1)) * tgamma((gm_real)n + 1.0) * sqrt(pi)
               / (pp * pp);
        w[n - 1 - i] = w[i];
    }
    return GM_OK;
}

/* Quadratura tanh-sinh (doppio esponenziale, Takahasi-Mori).
 * x = tanh((pi/2) sinh(t)), w = (pi/2) cosh(t)/cosh^2((pi/2) sinh(t)).
 * Trapezoidale adattivo su dominio t in [-4,4] (l'integrando decade doppio-
 * esponenzialmente); ferma quando il delta tra livelli < abs_acc. */
gm_real gm_quad_tanh_sinh(gm_fn1 f, void *ctx, gm_real a, gm_real b,
                          gm_real abs_acc, int max_evals)
{
    const gm_real HALF_PI = 1.57079632679489661923;
    const gm_real W_MIN = 1e-15;
    const gm_real TMAX = 4.0;
    const int MAX_LEVELS = 15;
    gm_real mid, half, h, T, T_prev = 0.0;
    int level, evals;
    if (!f || b <= a || abs_acc <= 0.0) return 0.0 / 0.0;
    mid = 0.5 * (b + a);
    half = 0.5 * (b - a);
    h = 1.0;
    T = 0.0;
    evals = 0;
    for (level = 0; level <= MAX_LEVELS; level++) {
        int jmax = (int)(TMAX / h + 0.5);
        gm_real sum = 0.0;
        int j;
        for (j = 0; j <= jmax; j++) {
            double t = j * h;
            double u = HALF_PI * sinh(t);
            double e2 = exp(-2.0 * fabs(u));
            double w = HALF_PI * cosh(t) * 4.0 * e2 / ((1.0 + e2) * (1.0 + e2));
            if (w < W_MIN) break;
            if (j == 0) {
                gm_real fx = f(mid + half * tanh(u), ctx);
                /* j=0 e il punto centrale: nel trapezio sulla retta intera conta UNA volta
                 * (le due meta si sommano); il vecchio *0.5 perdeva mezzo nodo per livello,
                 * cioe O(h) di massa, e la quadratura convergeva linearmente. */
                sum += fx * w;
                evals++;
            } else {
                double x = tanh(u);
                gm_real fxp = f(mid + half * x, ctx);
                gm_real fxm = f(mid - half * x, ctx);
                sum += (fxp + fxm) * w;
                evals += 2;
            }
        }
        T = h * half * sum;
        if (level > 0 && fabs(T - T_prev) < abs_acc) break;
        if (evals > max_evals) break;
        T_prev = T;
        h *= 0.5;
    }
    return T;
}

/* Variante complement (singolarita' agli estremi): f(x, d) con d = distanza
 * firmata all'estremo piu' vicino, calcolata stabilmente dal transform
 * (d_phys = comp*half, comp = 1-|x_unit|). */
gm_real gm_quad_tanh_sinh_c(gm_real (*f)(gm_real x, gm_real d, void *), void *ctx,
                            gm_real a, gm_real b, gm_real abs_acc, int max_evals)
{
    const gm_real HALF_PI = 1.57079632679489661923;
    const gm_real W_MIN = 1e-15;
    const gm_real TMAX = 4.0;
    const int MAX_LEVELS = 15;
    gm_real mid, half, h, T, T_prev = 0.0;
    int level, evals;
    if (!f || b <= a || abs_acc <= 0.0) return 0.0 / 0.0;
    mid = 0.5 * (b + a);
    half = 0.5 * (b - a);
    h = 1.0;
    T = 0.0;
    evals = 0;
    for (level = 0; level <= MAX_LEVELS; level++) {
        int jmax = (int)(TMAX / h + 0.5);
        gm_real sum = 0.0;
        int j;
        for (j = 0; j <= jmax; j++) {
            double t = j * h;
            double u = HALF_PI * sinh(t);
            double e2 = exp(-2.0 * fabs(u));
            double w = HALF_PI * cosh(t) * 4.0 * e2 / ((1.0 + e2) * (1.0 + e2));
            double comp = 2.0 * e2 / (1.0 + e2);
            if (w < W_MIN) break;
            if (j == 0) {
                gm_real fx = f(mid, -half, ctx);  /* distanza all'estremo = half */
                /* j=0 e il punto centrale: nel trapezio sulla retta intera conta UNA volta
                 * (le due meta si sommano); il vecchio *0.5 perdeva mezzo nodo per livello,
                 * cioe O(h) di massa, e la quadratura convergeva linearmente. */
                sum += fx * w;
                evals++;
            } else {
                double x = tanh(u);
                double d = (u >= 0.0 ? -comp : comp) * half;
                gm_real fp = f(mid + half * x, d, ctx);
                gm_real fm = f(mid - half * x, -d, ctx);
                sum += (fp + fm) * w;
                evals += 2;
            }
        }
        T = h * half * sum;
        if (level > 0 && fabs(T - T_prev) < abs_acc) break;
        if (evals > max_evals) break;
        T_prev = T;
        h *= 0.5;
    }
    return T;
}
