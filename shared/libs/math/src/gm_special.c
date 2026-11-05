/* gm_special.c — funzioni speciali (MathNet/SciPy), progetto gmbmath-special.
 * Estratte da gmi_math.c legacy: gamma/beta/erf, inverse, Bessel (AMOS). */
#include "gm_special.h"

#include <math.h>
#include <stdlib.h>

/* ── MathNet.Numerics 5.0.0 GammaLn/Gamma (Lanczos, Pugh 2004) — port fedele per
 * parità bit-esatta col canone C# (SpecialFunctions.GammaLn/Gamma). Sostituisce
 * lgamma()/tgamma() di libm, che differivano di ~1e-13. ── */
#define GMB_MT_GAMMA_N 10
#define GMB_MT_GAMMA_R 10.900511
static const double g_mt_gammadk[11] = {
    2.48574089138753565546e-5, 1.05142378581721974210, -3.45687097222016235469,
    4.51227709466894823700, -2.98285225323576655721, 1.05639711577126713077,
    -1.95428773191645869583e-1, 1.70970543404441224307e-2, -5.71926117404305781283e-4,
    4.63399473359905636708e-6, -2.71994908488607703910e-9
};
#define GMB_MT_LNPI 1.1447298858494001741434273513530587116472948129153
#define GMB_MT_LOGTWOSQRTEOPI 0.6207822376352452223455184457816472122518527279025978
#define GMB_MT_TWOSQRTEOPI 1.8603827342052657173362492472666631120594218414085755
#define GMB_MT_E 2.7182818284590452353602874713526624977572470937000

static double gmi_mathnet_gammaln(double z)
{
    if (z < 0.5) {
        double s = g_mt_gammadk[0];
        for (int i = 1; i <= GMB_MT_GAMMA_N; i++) s += g_mt_gammadk[i] / (i - z);
        return GMB_MT_LNPI - log(sin(M_PI * z)) - log(s)
             - GMB_MT_LOGTWOSQRTEOPI - ((0.5 - z) * log((0.5 - z + GMB_MT_GAMMA_R) / GMB_MT_E));
    } else {
        double s = g_mt_gammadk[0];
        for (int i = 1; i <= GMB_MT_GAMMA_N; i++) s += g_mt_gammadk[i] / (z + i - 1.0);
        return log(s) + GMB_MT_LOGTWOSQRTEOPI + ((z - 0.5) * log((z - 0.5 + GMB_MT_GAMMA_R) / GMB_MT_E));
    }
}

static double gmi_mathnet_gamma(double z)
{
    if (z < 0.5) {
        double s = g_mt_gammadk[0];
        for (int i = 1; i <= GMB_MT_GAMMA_N; i++) s += g_mt_gammadk[i] / (i - z);
        return M_PI / (sin(M_PI * z) * s * GMB_MT_TWOSQRTEOPI
                       * pow((0.5 - z + GMB_MT_GAMMA_R) / GMB_MT_E, 0.5 - z));
    } else {
        double s = g_mt_gammadk[0];
        for (int i = 1; i <= GMB_MT_GAMMA_N; i++) s += g_mt_gammadk[i] / (z + i - 1.0);
        return s * GMB_MT_TWOSQRTEOPI * pow((z - 0.5 + GMB_MT_GAMMA_R) / GMB_MT_E, z - 0.5);
    }
}

/* Parità con CommonLib.Numerics.SpecialGamma/SpecialLGamma (wrapper NaN/Inf). */
double gm_special_lgamma(double x) { return (isnan(x) || x <= 0) ? NAN : gmi_mathnet_gammaln(x); }
double gm_special_beta(double a, double b)
{
    if (isnan(a) || isnan(b) || a <= 0 || b <= 0) return NAN;
    return exp(gmi_mathnet_gammaln(a) + gmi_mathnet_gammaln(b) - gmi_mathnet_gammaln(a + b));
}

/* Continued fraction for I_x(a,b) (Numerical Recipes betacf). */
 double gmi_betacf(double a, double b, double x)
{
    const double maxit = 200, eps = 3e-14, fmin = 1e-300;
    double qab = a + b, qap = a + 1, qam = a - 1;
    double c = 1.0, d = 1.0 - qab * x / qap;
    if (fabs(d) < fmin) d = fmin;
    d = 1.0 / d;
    double h = d;
    for (int m = 1; m <= maxit; m++)
    {
        int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d; if (fabs(d) < fmin) d = fmin;
        c = 1.0 + aa / c; if (fabs(c) < fmin) c = fmin;
        d = 1.0 / d; h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d; if (fabs(d) < fmin) d = fmin;
        c = 1.0 + aa / c; if (fabs(c) < fmin) c = fmin;
        d = 1.0 / d; h *= d * c;
        if (fabs(d * c - 1) < eps) break;
    }
    return h;
}

/* Regolarized incomplete beta I_x(a,b). */
double gm_special_betainc(double a, double b, double x)
{
    /* I_x(a,b) — port fedele di MathNet BetaRegularized (continued fraction,
     * eps=2.22e-16, fpmin=DBL_MIN). Sostituisce gmi_betacf (eps 3e-14). */
    if (a < 0 || b < 0) return NAN;
    if (x < 0 || x > 1) return NAN;
    double bt = (x == 0.0 || x == 1.0) ? 0.0
        : exp(gmi_mathnet_gammaln(a + b) - gmi_mathnet_gammaln(a) - gmi_mathnet_gammaln(b)
              + (a * log(x)) + (b * log(1.0 - x)));
    int sym = x >= (a + 1.0) / (a + b + 2.0);
    const double eps = 2.220446049250313e-16;
    const double fpmin = 2.2250738585072014e-308;
    if (sym) { x = 1.0 - x; double t = a; a = b; b = t; }
    double qab = a + b, qap = a + 1.0, qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - (qab * x / qap);
    if (fabs(d) < fpmin) d = fpmin;
    d = 1.0 / d;
    double h = d;
    for (int m = 1, m2 = 2; m <= 50000; m++, m2 += 2) {
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + (aa * d);
        if (fabs(d) < fpmin) d = fpmin;
        c = 1.0 + (aa / c);
        if (fabs(c) < fpmin) c = fpmin;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + (aa * d);
        if (fabs(d) < fpmin) d = fpmin;
        c = 1.0 + (aa / c);
        if (fabs(c) < fpmin) c = fpmin;
        d = 1.0 / d;
        double del = d * c;
        h *= del;
        if (fabs(del - 1.0) <= eps) break;
    }
    return sym ? 1.0 - (bt * h / a) : bt * h / a;
}

double gm_special_erfc(double x) { return erfc(x); }

double gm_special_gammainc(double a, double x)
{
    /* regularized lower incomplete gamma P(a,x) via series/continued fraction */
    if (x < 0 || a <= 0) return 0;
    if (x == 0) return 0;
    (void)tgamma(a);
    if (x < a + 1)
    {
        double ap = a, sum = 1.0 / a, del = sum;
        for (int n = 1; n <= 300; n++) { ap += 1; del *= x / ap; sum += del; if (fabs(del) < fabs(sum) * 1e-14) break; }
        return sum * exp(-x + a * log(x) - lgamma(a));
    }
    else
    {
        double b = x + 1 - a, c = 1e30, d = 1.0 / b, h = d;
        for (int i = 1; i <= 300; i++)
        {
            double an = -i * (i - a);
            b += 2;
            d = an * d + b; if (fabs(d) < 1e-30) d = 1e-30;
            c = b + an / c; if (fabs(c) < 1e-30) c = 1e-30;
            d = 1.0 / d;
            double del = d * c;
            h *= del;
            if (fabs(del - 1) < 1e-14) break;
        }
        return 1 - exp(-x + a * log(x) - lgamma(a)) * h;
    }
}

/* ── Nucleo scipy.special (SPECIAL namespace) — condiviso VM/C e C99 ── */

/* gamma completa Γ(x) (x>0); tgamma è la libreria C (precisione macchina). */
double gm_special_gamma(double x)
{
    if (isnan(x)) return NAN;
    if (isinf(x)) return x > 0 ? INFINITY : NAN;
    return gmi_mathnet_gamma(x);
}

/* erf(x) = 1 − erfc(x) */
double gm_special_erf(double x) { return 1.0 - erfc(x); }

/* gammaincc(a,x) = Q(a,x) = 1 − P(a,x) */
double gm_special_gammaincc(double a, double x)
{
    if (a <= 0 || x < 0) return NAN;
    return 1.0 - gm_special_gammainc(a, x);
}

/* betaln(a,b) = log B(a,b) */
double gm_special_betaln(double a, double b)
{
    if (a <= 0 || b <= 0) return NAN;
    return gmi_mathnet_gammaln(a) + gmi_mathnet_gammaln(b) - gmi_mathnet_gammaln(a + b);
}

/* digamma ψ(x) — port fedele di MathNet DiGamma (AS 103, Bernardo 1976 / Minka).
 * Sostituisce la ricorrenza con soglia 8 (differiva di ~3e-13). Wrapper C#:
 * NaN o x<=0 → NaN. */
double gm_special_digamma(double x)
{
    if (isnan(x) || x <= 0) return NAN;
    const double c = 12.0;
    const double d1 = -0.57721566490153286;
    const double d2 = 1.6449340668482264365;
    const double s = 1e-6;
    const double s3 = 1.0 / 12.0, s4 = 1.0 / 120.0, s5 = 1.0 / 252.0, s6 = 1.0 / 240.0, s7 = 1.0 / 132.0;
    if (x <= s) return d1 - (1.0 / x) + (d2 * x);
    double result = 0;
    while (x < c) { result -= 1.0 / x; x += 1.0; }
    double r = 1.0 / x;
    result += log(x) - (0.5 * r);
    r *= r;
    result -= r * (s3 - (r * (s4 - (r * (s5 - (r * (s6 - (r * s7))))))));
    return result;
}

/* erfinv(x): Newton su erf. */
double gm_special_erfinv(double x)
{
    if (x < -1 || x > 1) return NAN;
    if (x == 1) return INFINITY;
    if (x == -1) return -INFINITY;
    double y = x * 0.8862269254527580; /* sqrt(pi)/2 * x come stima */
    for (int i = 0; i < 40; i++)
    {
        double e = gm_special_erf(y) - x;
        if (fabs(e) < 1e-15) break;
        y -= e * 0.8862269254527580 * exp(y * y);
    }
    return y;
}

/* erfcinv(x): Newton su erfc. */
double gm_special_erfcinv(double x)
{
    if (x < 0 || x > 2) return NAN;
    if (x == 0) return INFINITY;
    if (x == 2) return -INFINITY;
    double y = 1.0 - x / 2.0;
    for (int i = 0; i < 40; i++)
    {
        double e = erfc(y) - x;
        if (fabs(e) < 1e-15) break;
        y += e * 0.8862269254527580 * exp(y * y);
    }
    return y;
}

/* gammaincinv(a,y): x tale che P(a,x)=y — bisection con upper crescente. */
double gm_special_gammaincinv(double a, double y)
{
    if (a <= 0 || y < 0 || y > 1) return NAN;
    if (y == 0) return 0;
    if (y == 1) return INFINITY;
    double lo = 0, hi = 1;
    while (gm_special_gammainc(a, hi) < y) hi *= 2;
    for (int i = 0; i < 60; i++)
    {
        double mid = 0.5 * (lo + hi);
        if (gm_special_gammainc(a, mid) < y) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

/* gammainccinv(a,y): x tale che Q(a,x)=y (Q=1−P). */
double gm_special_gammainccinv(double a, double y)
{
    if (a <= 0 || y < 0 || y > 1) return NAN;
    if (y == 0) return INFINITY;
    if (y == 1) return 0;
    return gm_special_gammaincinv(a, 1.0 - y);
}

/* betaincinv(a,b,y): x tale che I_x(a,b)=y — bisection su [0,1]. */
double gm_special_betaincinv(double a, double b, double y)
{
    if (a <= 0 || b <= 0 || y < 0 || y > 1) return NAN;
    if (y == 0) return 0;
    if (y == 1) return 1;
    double lo = 0, hi = 1;
    for (int i = 0; i < 60; i++)
    {
        double mid = 0.5 * (lo + hi);
        if (gm_special_betainc(a, b, mid) < y) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

/* ── Bessel (scipy.special jv/yv/iv/kv) — serie + formule con digamma ── */
#define GMB_EULER_GAMMA 0.5772156649015329

static double gmi_bessel_series(double v, double x, double sign)
{
    /* Σ sign^m (x/2)^(2m+v) / (m! Γ(m+v+1)) — Jv (sign=-1), Iv (sign=+1) */
    double half = 0.5 * x;
    double term = pow(half, v) / tgamma(v + 1);
    double sum = term;
    for (int m = 1; m <= 60; m++)
    {
        term *= (sign * half * half) / ((double)m * (v + m));
        sum += term;
        if (fabs(term) < fabs(sum) * 1e-15) break;
    }
    return sum;
}

double gm_special_besselj(double v, double x)
{
    if (x < 0) return gm_special_besselj(v, -x) * (fmod(v, 2.0) == 0 ? 1.0 : -1.0);
    return gmi_bessel_series(v, x, -1.0);
}

double gm_special_besseli(double v, double x)
{
    if (x < 0) return gmi_bessel_series(v, -x, 1.0) * (fmod(v, 2.0) == 0 ? 1.0 : -1.0);
    return gmi_bessel_series(v, x, 1.0);
}

 double gmi_bessel_jneg(double v, double x)
{
    /* J_{-v}(x) per v intero: J_{-n}=(-1)^n J_n */
    int n = (int)(v + 0.5);
    if (fabs(v - n) < 1e-12) return (n % 2 == 0 ? 1.0 : -1.0) * gm_special_besselj((double)n, x);
    return gmi_bessel_series(-v, x, -1.0);
}

/* ── Bessel Y/K: AMOS (D. E. Amos) via f2c — bit-esatto con MathNet 5.0.0.
 * Sostituisce la serie + limite d=1e-4 (errore ~1e-9). Sorgenti in src/amos/. ── */
extern int zbesy_(double*, double*, double*, int*, int*, double*, double*, int*, double*, double*, int*);
extern int zbesk_(double*, double*, double*, int*, int*, double*, double*, int*, int*);

double gm_special_bessely(double v, double x)
{
    if (isnan(v) || isnan(x) || x < 0.0) return NAN;
    double zr = x, zi = 0.0;
    int kode = 1, n = 1, nz = 0, ierr = 0;
    double cyr = 0.0, cyi = 0.0, cwrkr = 0.0, cwrki = 0.0;
    zbesy_(&zr, &zi, &v, &kode, &n, &cyr, &cyi, &nz, &cwrkr, &cwrki, &ierr);
    return cyr;
}

double gm_special_besselk(double v, double x)
{
    if (isnan(v) || isnan(x) || x < 0.0) return NAN;
    if (x == 0.0) return INFINITY;
    if (x > 710.0 * (1.0 + fabs(v))) return 0.0; /* underflow (parità MathNet) */
    double zr = x, zi = 0.0;
    int kode = 1, n = 1, nz = 0, ierr = 0;
    double cyr = 0.0, cyi = 0.0;
    zbesk_(&zr, &zi, &v, &kode, &n, &cyr, &cyi, &nz, &ierr);
    return cyr;
}

