/* gm_special2.c — funzioni speciali avanzate (riscrittura). */
#include "gm_special2.h"

#include <math.h>

#define GM_PI_D 3.14159265358979323846

/* Gamma reale via approssimazione di Lanczos (g=7, n=9). */
static gm_real lgam(gm_real x)
{
    static const gm_real c[9] = {
        0.99999999999980993, 676.5203681218851, -1259.1392167224028,
        771.32342877765313, -176.61502916214059, 12.507343278686905,
        -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7
    };
    gm_real a, t;
    int i;
    if (x < 0.5)
        return log(GM_PI_D / sin(GM_PI_D * x)) - lgam(1.0 - x);
    x -= 1.0;
    a = c[0];
    t = x + 7.5;
    for (i = 1; i < 9; i++) a += c[i] / (x + (gm_real)i);
    return 0.5 * log(2.0 * GM_PI_D) + (x + 0.5) * log(t) - t + log(a);
}

static gm_real gam(gm_real x)
{
    if (x == floor(x) && x <= 0.0) return NAN;
    if (x < 0.5) return GM_PI_D / (sin(GM_PI_D * x) * gam(1.0 - x));
    return exp(lgam(x));
}

gm_real gm_hypergeom_2f1(gm_real a, gm_real b, gm_real c, gm_real x)
{
    gm_real s = 1.0, term = 1.0;
    int n;
    if (c == 0.0 || (c == floor(c) && c <= 0.0)) return NAN;
    if (x < -1.0) {
        /* trasformazione di Pfaff: x/(x-1) in (-1/2, 1) */
        gm_real y = x / (x - 1.0);
        return pow(1.0 - x, -a) * gm_hypergeom_2f1(a, c - b, c, y);
    }
    if (x < 0.0 || x < 1.0) {
        for (n = 0; n < 10000; n++) {
            term *= (a + (gm_real)n) * (b + (gm_real)n) / ((c + (gm_real)n) * ((gm_real)n + 1.0)) * x;
            s += term;
            if (fabs(term) < 1e-16 * fabs(s)) break;
        }
        return s;
    }
    if (x == 1.0) {
        /* teorema di Gauss */
        return gam(c) * gam(c - a - b) / (gam(c - a) * gam(c - b));
    }
    return NAN; /* |x|>=1 fuori dominio della serie */
}

gm_real gm_hypergeom_1f1(gm_real a, gm_real b, gm_real x)
{
    gm_real s, term, u;
    int n;
    if (b == 0.0 || (b == floor(b) && b <= 0.0)) return NAN;
    if (a <= 0.0 && a == floor(a)) {
        /* a intero non positivo: 1F1 è un polinomio esatto (la serie termina). */
        s = 1.0; term = 1.0;
        for (n = 0; n <= (int)(-a); n++) {
            term *= (a + (gm_real)n) / ((b + (gm_real)n) * ((gm_real)n + 1.0)) * x;
            s += term;
            if (!(fabs(s) < 1e300)) return NAN;
        }
        return s;
    }
    if (x > 15.0) {
        /* Asintotica per z -> +inf (Tricomi): 1F1(a;b;z) ~ Gamma(b)/Gamma(a)
         * e^z z^{a-b} sum_k (b-a)_k (1-a)_k / k! z^{-k}. Il segno di Gamma(a)
         * conta (es. a=-1/4: Gamma<0). */
        double lv, sign = (gam(a) < 0.0) ? -1.0 : 1.0;
        int k;
        if (fabs(a) > 60.0 || fabs(b) > 60.0 || x > 700.0) {
            lv = 0.0;
        } else {
            lv = lgamma(b) - lgamma(a) + x + (a - b) * log(fabs(x));
        }
        if (lv > 700.0) return sign * 1e308;
        if (lv < -700.0) return 0.0;
        s = 1.0; term = 1.0;
        for (k = 0; k < 12; k++) {
            term *= (b - a + (gm_real)k) * (1.0 - a + (gm_real)k) / ((gm_real)k + 1.0) / x;
            s += term;
        }
        return sign * exp(lv) * s;
    }
    if (x < -20.0) {
        /* Asintotica per z -> -inf (serie divergente in 1/u): 1F1(a;b;-u) ~
         * Gamma(b)/Gamma(b-a) u^{-a} sum_k (a)_k (a-b+1)_k / k! (-u)^{-k}.
         * Troncamento adattivo al termine di modulo minimo (i termini
         * decrescono fino a k ~ |u|); robusto, mai catastrofico. */
        double lv, sign = ((gam(b) < 0.0) ? -1.0 : 1.0) * ((gam(b - a) < 0.0) ? -1.0 : 1.0);
        double best;
        int cap;
        u = -x;
        if (fabs(a) > 60.0 || fabs(b - a) > 60.0) {
            lv = 0.0;
        } else {
            lv = lgamma(b) - lgamma(b - a) - a * log(u);
        }
        if (lv > 700.0) return sign * 1e308;
        if (lv < -700.0) return 0.0;
        cap = (int)(u) + 8;
        if (cap < 16) cap = 16;
        if (cap > 300) cap = 300;
        s = 1.0; term = 1.0; best = 1.0; {
            double minterm = 1.0;
            for (n = 1; n <= cap; n++) {
                term *= (a + (gm_real)(n - 1)) * (a - b + (gm_real)n) / (gm_real)n * (-1.0 / u);
                s += term;
                if (fabs(term) < minterm) { minterm = fabs(term); best = s; }
            }
        }
        return sign * exp(lv) * best;
    }
    s = 1.0; term = 1.0;
    for (n = 0; n < 100000; n++) {
        term *= (a + (gm_real)n) / ((b + (gm_real)n) * ((gm_real)n + 1.0)) * x;
        s += term;
        if (!(fabs(s) < 1e300)) return NAN;
        if (fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}

/* Bernoulli B_2..B_20 */
static const gm_real bern[] = {
    1.0 / 6.0, -1.0 / 30.0, 1.0 / 42.0, -1.0 / 30.0, 5.0 / 66.0,
    -691.0 / 2730.0, 7.0 / 6.0, -3617.0 / 510.0, 43867.0 / 798.0, -174611.0 / 330.0
};

static gm_real zeta_em(gm_real s, gm_real a)
{
    /* Euler-Maclaurin per Hurwitz: ζ(s,a) = Σ_{k=0}^{N-1} (k+a)^{-s} + ... */
    int N = 20, M = 10, k, j;
    gm_real sum = 0.0, Np;
    for (k = 0; k < N; k++) sum += pow((gm_real)k + a, -s);
    Np = (gm_real)N + a;
    sum += pow(Np, 1.0 - s) / (s - 1.0) + 0.5 * pow(Np, -s);
    for (j = 1; j <= M; j++) {
        gm_real fact = 1.0, term, rising = 1.0;
        int m2 = 2 * j;
        for (k = 1; k <= m2; k++) fact *= (gm_real)k;
        for (k = 0; k < m2 - 1; k++) rising *= (s + (gm_real)k);
        term = bern[j - 1] / fact * rising * pow(Np, -s - (gm_real)m2 + 1.0);
        sum += term;
    }
    return sum;
}

gm_real gm_zeta(gm_real s)
{
    if (s == 1.0) return INFINITY;
    if (s > 1.0) return zeta_em(s, 1.0);
    {
        /* equazione funzionale: ζ(s) = 2^s π^{s-1} sin(πs/2) Γ(1-s) ζ(1-s) */
        return pow(2.0, s) * pow(GM_PI_D, s - 1.0) * sin(GM_PI_D * s / 2.0) * gam(1.0 - s) * gm_zeta(1.0 - s);
    }
}

gm_real gm_zeta_hurwitz(gm_real s, gm_real a)
{
    if (a <= 0.0 && a == floor(a)) return INFINITY;
    return zeta_em(s, a);
}

gm_real gm_polygamma(int n, gm_real x)
{
    gm_real r = 0.0;
    int i;
    if (n < 0 || x <= 0.0) return NAN;
    if (n == 0) {
        /* digamma */
        gm_real res = 0.0;
        while (x < 6.0) { res -= 1.0 / x; x += 1.0; }
        {
            gm_real inv = 1.0 / x, inv2 = inv * inv, p = inv2;
            res += log(x) - 0.5 * inv;
            for (i = 1; i <= 10; i++) {
                res -= bern[i - 1] / (gm_real)(2 * i) * p;
                p *= inv2;
            }
        }
        return res;
    }
    /* n>=1: ricorrenza ψ^(n)(x) = ψ^(n)(x+1) + (-1)^n n!/x^{n+1} */
    {
        gm_real fact = 1.0, factn1 = 1.0, res;
        int sign = (n % 2 == 1) ? 1 : -1; /* (-1)^{n+1} */
        for (i = 1; i <= n; i++) fact *= (gm_real)i;
        for (i = 1; i <= n - 1; i++) factn1 *= (gm_real)i;
        while (x < 10.0) { r += (gm_real)sign * fact / pow(x, (gm_real)(n + 1)); x += 1.0; }
        /* asintotica: ψ^(n)(x) ~ (-1)^{n-1}[ (n-1)!/x^n + n!/(2x^{n+1}) + Σ B_{2k}(2k+n-1)!/((2k)! x^{2k+n}) ] */
        res = factn1 / pow(x, (gm_real)n) + 0.5 * fact / pow(x, (gm_real)(n + 1));
        for (i = 1; i <= 10; i++) {
            int m, k2 = 2 * i;
            gm_real ratio = 1.0;
            for (m = k2 + 1; m <= k2 + n - 1; m++) ratio *= (gm_real)m;
            res += bern[i - 1] * ratio / pow(x, (gm_real)(k2 + n));
        }
        r += ((n % 2 == 1) ? 1.0 : -1.0) * res;
    }
    return r;
}

gm_real gm_lambert_w(gm_real x)
{
    gm_real w;
    int i;
    if (x < -1.0 / 2.71828182845904523536) return NAN;
    if (x == 0.0) return 0.0;
    if (x == -1.0 / 2.71828182845904523536) return -1.0;
    if (x > 1.0) w = log(x) - log(log(x) + 1e-300);
    else w = x;
    for (i = 0; i < 100; i++) {
        gm_real ew = exp(w), f = w * ew - x;
        gm_real wp1 = w + 1.0;
        gm_real d = ew * wp1 - (w + 2.0) * f / (2.0 * wp1);
        gm_real dw = f / d;
        w -= dw;
        if (fabs(dw) < 1e-16 * (1.0 + fabs(w))) break;
    }
    return w;
}

gm_real gm_mittag_leffler(gm_real alpha, gm_real beta, gm_real x)
{
    gm_real s = 0.0;
    int k;
    for (k = 0; k < 2000; k++) {
        gm_real term = pow(x, (gm_real)k) / gam(alpha * (gm_real)k + beta);
        s += term;
        if (k > 0 && fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}

gm_real gm_struve_h(gm_real v, gm_real x)
{
    /* serie H_v(x) = Σ (-1)^k (x/2)^{2k+v+1} / (Γ(k+3/2) Γ(k+v+3/2)) */
    gm_real s = 0.0, h = x / 2.0;
    int k;
    for (k = 0; k < 500; k++) {
        gm_real term = pow(h, 2.0 * (gm_real)k + v + 1.0) / (gam((gm_real)k + 1.5) * gam((gm_real)k + v + 1.5));
        if (k % 2) term = -term;
        s += term;
        if (fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}

gm_real gm_kelvin_ber(gm_real x)
{
    gm_real s = 0.0, h = x / 2.0;
    int k;
    for (k = 0; k < 200; k++) {
        gm_real term = pow(h, 4.0 * (gm_real)k) / (gam(2.0 * (gm_real)k + 1.0) * gam(2.0 * (gm_real)k + 1.0));
        if (k % 2) term = -term;
        s += term;
        if (fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}

gm_real gm_kelvin_bei(gm_real x)
{
    gm_real s = 0.0, h = x / 2.0;
    int k;
    for (k = 0; k < 200; k++) {
        gm_real term = pow(h, 4.0 * (gm_real)k + 2.0) / (gam(2.0 * (gm_real)k + 2.0) * gam(2.0 * (gm_real)k + 2.0));
        if (k % 2) term = -term;
        s += term;
        if (fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}

gm_real gm_parabolic_cylinder(gm_real a, gm_real x)
{
    /* U(a,x) = D_{-a-1/2}(x) via DLMF 12.7.14 */
    gm_real nu = -a - 0.5;
    gm_real t1 = sqrt(GM_PI_D) / gam(0.5 * (1.0 - nu)) * gm_hypergeom_1f1(-0.5 * nu, 0.5, 0.5 * x * x);
    gm_real t2 = sqrt(2.0 * GM_PI_D) * x / gam(-0.5 * nu) * gm_hypergeom_1f1(0.5 * (1.0 - nu), 1.5, 0.5 * x * x);
    return pow(2.0, 0.5 * nu) * exp(-0.25 * x * x) * (t1 - t2);
}
