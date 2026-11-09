/* signal_filters.c — filtri IIR (scipy.signal): design Butterworth/Chebyshev
 * I-II/ellittico (prototipo analogico + trasformazioni di banda + bilineare) e
 * applicazione lfilter/filtfilt/freqz, con registro degli handle.
 *
 * Codice spostato dal runtime C di GMBasic (gmb_filters.c) con il solo cambio di
 * prefisso (gmb_ -> gs_) e di tipo degli handle/count (gmb_int_t -> long).
 * NON sono stati portati gmb_filter_lowpass/highpass/bandpass/bandstop: sono i
 * wrapper del dominio FILTER.* e restano nel runtime.
 */
#include "signal_internal.h"
#include "gmbsignal.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── registro degli handle filtro (pattern MATRIX/VECTOR) ──────────────────── */
typedef struct {
    double b[GS_FILTER_MAXTAPS];
    double a[GS_FILTER_MAXTAPS];
    int nb, na;
    int in_use;
} gs_filter_t;

static gs_filter_t g_filters[GS_FILTER_MAX];

static gs_filter_t* gs_filter_by_handle(long h)
{
    if (h <= 0 || h >= GS_FILTER_MAX) return NULL;
    gs_filter_t* f = &g_filters[h];
    return f->in_use ? f : NULL;
}

static long gs_filter_store(const double* b, int nb, const double* a, int na)
{
    for (int i = 1; i < GS_FILTER_MAX; i++)
    {
        if (!g_filters[i].in_use)
        {
            gs_filter_t* f = &g_filters[i];
            memset(f, 0, sizeof(*f));
            for (int j = 0; j < nb && j < GS_FILTER_MAXTAPS; j++) f->b[j] = b[j];
            for (int j = 0; j < na && j < GS_FILTER_MAXTAPS; j++) f->a[j] = a[j];
            f->nb = nb; f->na = na; f->in_use = 1;
            return i;
        }
    }
    return 0;
}

long gs_filter_count(void)
{
    long n = 0;
    for (int i = 1; i < GS_FILTER_MAX; i++) if (g_filters[i].in_use) n++;
    return n;
}

/* ── helper numerici ───────────────────────────────────────────────────────── */
static double gs_asinh(double x) { return log(x + sqrt(x * x + 1.0)); }
static double gs_atanh(double x) { return 0.5 * log((1.0 + x) / (1.0 - x)); }

static double complex gs_casin(double complex z)
{
    /* asin(z) = -i log(i z + sqrt(1 - z^2)) */
    return -I * clog(I * z + csqrt(1.0 - z * z));
}

static double complex gs_cprod_neg(double complex* v, int n)
{
    double complex r = 1.0 + 0.0 * I;
    for (int i = 0; i < n; i++) r *= -v[i];
    return r;
}

/* ── funzioni ellittiche ───────────────────────────────────────────────────── */
static double gs_ellipk(double m)
{
    if (m >= 1.0) return INFINITY;
    if (m <= 0.0) return GS_PI / 2.0;
    double a = 1.0, b = sqrt(1.0 - m);
    for (int i = 0; i < 40; i++)
    {
        double an = 0.5 * (a + b);
        b = sqrt(a * b);
        a = an;
        if (fabs(a - b) < 1e-15 * a) break;
    }
    return (GS_PI / 2.0) / a;
}

/* sn di Jacobi via Landen discendente + relazioni per cn/dn (u reale in [0,K]) */
static void gs_ellip_scndn(double u, double m, double* sn, double* cn, double* dn)
{
    if (m <= 0.0) { *sn = sin(u); *cn = cos(u); *dn = 1.0; return; }
    if (m >= 1.0) { *sn = tanh(u); double ch = cosh(u); *cn = 1.0 / ch; *dn = 1.0 / ch; return; }
    double kk = sqrt(m);
    double uu = u;
    double ks[64];
    int nk = 0;
    while (kk > 1e-14 && nk < 63)
    {
        double kp = sqrt((1.0 - kk) * (1.0 + kk));
        double kk2 = (1.0 - kp) / (1.0 + kp);
        uu = uu / (1.0 + kk2);
        ks[nk++] = kk2;
        kk = kk2;
    }
    double s = sin(uu);
    for (int i = nk - 1; i >= 0; i--)
    {
        double k1 = ks[i];
        s = (1.0 + k1) * s / (1.0 + k1 * s * s);
    }
    if (s > 1.0) s = 1.0;
    if (s < -1.0) s = -1.0;
    *sn = s;
    *cn = sqrt(fmax(0.0, 1.0 - s * s));
    *dn = sqrt(fmax(0.0, 1.0 - m * s * s));
}

static double gs_ellip_sn(double u, double m)
{
    double sn, cn, dn;
    gs_ellip_scndn(u, m, &sn, &cn, &dn);
    return sn;
}

/* inversa sn complessa via Landen (scipy _arc_jac_sn) */
static double complex gs_arc_jac_sn(double complex w, double m)
{
    double k = sqrt(m);
    if (k > 1.0) return 0.0 / 0.0;
    if (k == 1.0) return gs_atanh(creal(w));
    double ks[64];
    int nk = 0;
    ks[nk++] = k;
    while (ks[nk - 1] != 0.0 && nk < 63)
    {
        double kk = ks[nk - 1];
        double kp = sqrt((1.0 - kk) * (1.0 + kk));
        ks[nk++] = (1.0 - kp) / (1.0 + kp);
    }
    double K = GS_PI / 2.0;
    for (int i = 1; i < nk; i++) K *= 1.0 + ks[i];
    double complex wns = w;
    for (int i = 0; i < nk - 1; i++)
    {
        double kn = ks[i];
        double knext = ks[i + 1];
        wns = 2.0 * wns / ((1.0 + knext) * (1.0 + csqrt((1.0 - kn * wns) * (1.0 + kn * wns))));
    }
    double complex u = (2.0 / GS_PI) * gs_casin(wns);
    return K * u;
}

static double gs_arc_jac_sc1(double w, double m)
{
    return cimag(gs_arc_jac_sn(I * w, m));
}

static double gs_ellipdeg(int n, double m1)
{
    double K1 = gs_ellipk(m1);
    double K1p = gs_ellipk(1.0 - m1);
    double q1 = exp(-GS_PI * K1p / K1);
    double q = pow(q1, 1.0 / (double)n);
    double num = 0.0;
    for (int mn = 0; mn <= 7; mn++) num += pow(q, (double)(mn * (mn + 1)));
    double den = 1.0;
    for (int md = 1; md <= 8; md++) den += 2.0 * pow(q, (double)(md * md));
    return 16.0 * q * pow(num / den, 4.0);
}

/* ── design (parita' scipy iirfilter) ──────────────────────────────────────── */

typedef struct { double complex z[128]; double complex p[128]; double k; int nz, np; } gs_zpk_t;

static int gs_rel_degree(const gs_zpk_t* s) { return s->np - s->nz; }

static void gs_lp2lp(gs_zpk_t* s, double wo)
{
    for (int i = 0; i < s->nz; i++) s->z[i] *= wo;
    for (int i = 0; i < s->np; i++) s->p[i] *= wo;
    s->k *= pow(wo, (double)gs_rel_degree(s));
}

static void gs_lp2hp(gs_zpk_t* s, double wo)
{
    double complex zold[128], pold[128];
    int onz = s->nz, onp = s->np;
    memcpy(zold, s->z, sizeof(double complex) * (size_t)onz);
    memcpy(pold, s->p, sizeof(double complex) * (size_t)onp);
    for (int i = 0; i < onz; i++) s->z[i] = wo / zold[i];
    for (int i = 0; i < onp; i++) s->p[i] = wo / pold[i];
    int deg = onp - onz;
    for (int i = 0; i < deg; i++) s->z[onz + i] = 0.0;
    s->nz = onz + deg;
    s->k *= creal(gs_cprod_neg(zold, onz) / gs_cprod_neg(pold, onp));
}

static void gs_lp2bp(gs_zpk_t* s, double wo, double bw)
{
    double complex zb[128], pb[128];
    int nz = 0, np = 0;
    for (int i = 0; i < s->nz; i++)
    {
        double complex zl = s->z[i] * bw / 2.0;
        double complex r = csqrt(zl * zl - wo * wo);
        zb[nz++] = zl + r;
        zb[nz++] = zl - r;
    }
    for (int i = 0; i < s->np; i++)
    {
        double complex pl = s->p[i] * bw / 2.0;
        double complex r = csqrt(pl * pl - wo * wo);
        pb[np++] = pl + r;
        pb[np++] = pl - r;
    }
    int deg = gs_rel_degree(s);
    for (int i = 0; i < deg; i++) zb[nz++] = 0.0;
    memcpy(s->z, zb, sizeof(double complex) * (size_t)nz);
    memcpy(s->p, pb, sizeof(double complex) * (size_t)np);
    s->nz = nz; s->np = np;
    s->k *= pow(bw, (double)deg);
}

static void gs_lp2bs(gs_zpk_t* s, double wo, double bw)
{
    double complex zold[128], pold[128];
    int onz = s->nz, onp = s->np;
    memcpy(zold, s->z, sizeof(double complex) * (size_t)onz);
    memcpy(pold, s->p, sizeof(double complex) * (size_t)onp);
    double complex zb[128], pb[128];
    int nz = 0, np = 0;
    for (int i = 0; i < onz; i++)
    {
        double complex zh = (bw / 2.0) / zold[i];
        double complex r = csqrt(zh * zh - wo * wo);
        zb[nz++] = zh + r;
        zb[nz++] = zh - r;
    }
    for (int i = 0; i < onp; i++)
    {
        double complex ph = (bw / 2.0) / pold[i];
        double complex r = csqrt(ph * ph - wo * wo);
        pb[np++] = ph + r;
        pb[np++] = ph - r;
    }
    int deg = onp - onz;
    for (int i = 0; i < deg; i++)
    {
        zb[nz++] = I * wo;
        zb[nz++] = -I * wo;
    }
    memcpy(s->z, zb, sizeof(double complex) * (size_t)nz);
    memcpy(s->p, pb, sizeof(double complex) * (size_t)np);
    s->nz = nz; s->np = np;
    s->k *= creal(gs_cprod_neg(zold, onz) / gs_cprod_neg(pold, onp));
}

static void gs_bilinear_zpk(gs_zpk_t* s, double fs)
{
    double fs2 = 2.0 * fs;
    double complex zold[128], pold[128];
    int onz = s->nz, onp = s->np;
    memcpy(zold, s->z, sizeof(double complex) * (size_t)onz);
    memcpy(pold, s->p, sizeof(double complex) * (size_t)onp);
    double complex zz[128];
    for (int i = 0; i < onz; i++) zz[i] = (fs2 + zold[i]) / (fs2 - zold[i]);
    for (int i = 0; i < onp; i++) s->p[i] = (fs2 + pold[i]) / (fs2 - pold[i]);
    int deg = onp - onz;
    for (int i = 0; i < onz; i++) s->z[i] = zz[i];
    for (int i = 0; i < deg; i++) s->z[onz + i] = -1.0;
    s->nz = onz + deg;
    double complex num = 1.0 + 0.0 * I;
    double complex den = 1.0 + 0.0 * I;
    for (int i = 0; i < onz; i++) num *= fs2 - zold[i];
    for (int i = 0; i < onp; i++) den *= fs2 - pold[i];
    s->k *= creal(num / den);
}

static void gs_zpk2ba(const gs_zpk_t* s, double* b, double* a, int* nb, int* na)
{
    double complex bc[130], ac[130];
    int lb = 1, la = 1;
    bc[0] = 1.0 + 0.0 * I;
    ac[0] = 1.0 + 0.0 * I;
    for (int i = 0; i < s->nz; i++)
    {
        double complex r = s->z[i];
        double complex pb[130];
        memcpy(pb, bc, sizeof(double complex) * (size_t)lb);
        for (int j = 0; j <= lb; j++)
        {
            double complex lower = j > 0 ? pb[j - 1] : 0.0;
            double complex upper = j < lb ? pb[j] : 0.0;
            bc[j] = lower - r * upper;
        }
        lb++;
    }
    for (int i = 0; i < s->np; i++)
    {
        double complex r = s->p[i];
        double complex pa[130];
        memcpy(pa, ac, sizeof(double complex) * (size_t)la);
        for (int j = 0; j <= la; j++)
        {
            double complex lower = j > 0 ? pa[j - 1] : 0.0;
            double complex upper = j < la ? pa[j] : 0.0;
            ac[j] = lower - r * upper;
        }
        la++;
    }
    /* b/a nell'ordine scipy: coefficiente di grado massimo per primo (a[0]=1) */
    for (int j = 0; j < lb; j++) b[j] = creal(bc[lb - 1 - j]) * s->k;
    for (int j = 0; j < la; j++) a[j] = creal(ac[la - 1 - j]);
    *nb = lb;
    *na = la;
}

/* prototipi analogici */
static void gs_butter_zpk(int n, gs_zpk_t* s)
{
    s->nz = 0; s->np = n; s->k = 1.0;
    for (int i = 0; i < n; i++)
    {
        double m = (double)(-n + 1 + 2 * i);
        s->p[i] = -cexp(I * GS_PI * m / (2.0 * (double)n));
    }
}

static void gs_cheby1_zpk(int n, double rp, gs_zpk_t* s)
{
    if (n == 0) { s->nz = 0; s->np = 0; s->k = pow(10.0, -rp / 20.0); return; }
    double eps = sqrt(pow(10.0, 0.1 * rp) - 1.0);
    double mu = (1.0 / (double)n) * gs_asinh(1.0 / eps);
    s->nz = 0; s->np = n;
    for (int i = 0; i < n; i++)
    {
        double m = (double)(-n + 1 + 2 * i);
        double th = GS_PI * m / (2.0 * (double)n);
        s->p[i] = -csinh(mu + I * th);
    }
    s->k = creal(gs_cprod_neg(s->p, n));
    if (n % 2 == 0) s->k = s->k / sqrt(1.0 + eps * eps);
}

static void gs_cheby2_zpk(int n, double rs, gs_zpk_t* s)
{
    if (n == 0) { s->nz = 0; s->np = 0; s->k = 1.0; return; }
    double de = 1.0 / sqrt(pow(10.0, 0.1 * rs) - 1.0);
    double mu = gs_asinh(1.0 / de) / (double)n;
    double mz[128];
    int nz = 0;
    if (n % 2 == 1)
    {
        for (int v = -n + 1; v < 0; v += 2) mz[nz++] = (double)v;
        for (int v = 2; v < n; v += 2) mz[nz++] = (double)v;
    }
    else
    {
        for (int i = 0; i < n; i++) mz[nz++] = (double)(-n + 1 + 2 * i);
    }
    s->nz = nz; s->np = n;
    for (int i = 0; i < nz; i++) s->z[i] = I / csin(mz[i] * GS_PI / (2.0 * (double)n));
    for (int i = 0; i < n; i++)
    {
        double m1 = (double)(-n + 1 + 2 * i);
        double th = GS_PI * m1 / (2.0 * (double)n);
        s->p[i] = -1.0 / csinh(mu + I * th);
    }
    s->k = creal(gs_cprod_neg(s->p, n) / gs_cprod_neg(s->z, nz));
}

static void gs_ellip_zpk(int n, double rp, double rs, gs_zpk_t* s)
{
    if (n == 0) { s->nz = 0; s->np = 0; s->k = pow(10.0, -rp / 20.0); return; }
    if (n == 1)
    {
        double p1 = -sqrt(1.0 / (pow(10.0, 0.1 * rp) - 1.0));
        s->nz = 0; s->np = 1; s->p[0] = p1; s->k = -p1;
        return;
    }
    double eps_sq = pow(10.0, 0.1 * rp) - 1.0;
    double eps = sqrt(eps_sq);
    double ck1_sq = eps_sq / (pow(10.0, 0.1 * rs) - 1.0);
    if (ck1_sq == 0.0) { s->nz = 0; s->np = 0; s->k = 0.0; return; }

    double val0 = gs_ellipk(ck1_sq);
    double m = gs_ellipdeg(n, ck1_sq);
    double capk = gs_ellipk(m);

    int jc[64];
    int nj = n - (n % 2);
    for (int i = 0; i < nj; i++) jc[i] = 1 - (n % 2) + 2 * i;

    double zl[128];
    int nzl = 0;
    for (int i = 0; i < nj; i++)
    {
        double sn = gs_ellip_sn((double)jc[i] * capk / (double)n, m);
        if (fabs(sn) > 2.2e-16) zl[nzl++] = sn;
    }
    double complex zc[128];
    int nzc = 0;
    for (int i = 0; i < nzl; i++)
    {
        double complex zv = I / (sqrt(m) * zl[i]);
        zc[nzc++] = zv;
        zc[nzc++] = conj(zv);
    }

    double r = gs_arc_jac_sc1(1.0 / eps, ck1_sq);
    double v0 = capk * r / ((double)n * val0);

    double sv, cv, dv;
    gs_ellip_scndn(v0, 1.0 - m, &sv, &cv, &dv);
    double complex pc[128];
    int npc = 0;
    for (int i = 0; i < nj; i++)
    {
        double sj, cj, dj;
        gs_ellip_scndn((double)jc[i] * capk / (double)n, m, &sj, &cj, &dj);
        double complex denom = 1.0 - (dj * sv) * (dj * sv);
        double complex num = -(cj * dj * sv * cv + I * sj * dv);
        pc[npc++] = num / denom;
    }
    double norm = 0.0;
    for (int i = 0; i < npc; i++) norm += creal(pc[i]) * creal(pc[i]) + cimag(pc[i]) * cimag(pc[i]);
    norm = sqrt(norm);
    if (n % 2 == 1)
    {
        int base = npc;
        for (int i = 0; i < base; i++)
            if (fabs(cimag(pc[i])) > 2.2e-16 * norm) pc[npc++] = conj(pc[i]);
    }
    else
    {
        int base = npc;
        for (int i = 0; i < base; i++) pc[npc++] = conj(pc[i]);
    }

    memcpy(s->z, zc, sizeof(double complex) * (size_t)nzc);
    memcpy(s->p, pc, sizeof(double complex) * (size_t)npc);
    s->nz = nzc; s->np = npc;
    s->k = creal(gs_cprod_neg(s->p, npc) / gs_cprod_neg(s->z, nzc));
    if (n % 2 == 0) s->k = s->k / sqrt(1.0 + eps_sq);
}

static int gs_design_ba(int n, const double* wn, int nwn, const char* btype, gs_zpk_t* s)
{
    (void)n; /* l'ordine e' gia' nel prototipo zpk */
    /* warped (bilinear prewarping, fs=2) */
    double warped[2];
    for (int i = 0; i < nwn && i < 2; i++) warped[i] = 4.0 * tan(GS_PI * wn[i] / 2.0);
    int bt = 0; /* low */
    if (btype)
    {
        if (strncmp(btype, "high", 4) == 0) bt = 3;
        /* "bandstop" inizia con "band": controlla "stop" per intero PRIMA di "band". */
        else if (strstr(btype, "stop") != NULL) bt = 2;
        else if (strncmp(btype, "band", 4) == 0) bt = 1;
    }
    switch (bt)
    {
        case 0: default: gs_lp2lp(s, warped[0]); break;
        case 3: gs_lp2hp(s, warped[0]); break;
        case 1:
        case 2:
        {
            double bw = warped[1] - warped[0];
            double wo = sqrt(warped[0] * warped[1]);
            if (bt == 1) gs_lp2bp(s, wo, bw);
            else gs_lp2bs(s, wo, bw);
            break;
        }
    }
    gs_bilinear_zpk(s, 2.0);
    return 0;
}

long gs_filter_butter(long n, const double* wn, int nwn, const char* btype)
{
    gs_zpk_t s;
    gs_butter_zpk((int)n, &s);
    gs_design_ba((int)n, wn, nwn, btype, &s);
    double b[GS_FILTER_MAXTAPS], a[GS_FILTER_MAXTAPS];
    int nb, na;
    gs_zpk2ba(&s, b, a, &nb, &na);
    return gs_filter_store(b, nb, a, na);
}

long gs_filter_cheby1(long n, double rp, const double* wn, int nwn, const char* btype)
{
    gs_zpk_t s;
    gs_cheby1_zpk((int)n, rp, &s);
    gs_design_ba((int)n, wn, nwn, btype, &s);
    double b[GS_FILTER_MAXTAPS], a[GS_FILTER_MAXTAPS];
    int nb, na;
    gs_zpk2ba(&s, b, a, &nb, &na);
    return gs_filter_store(b, nb, a, na);
}

long gs_filter_cheby2(long n, double rs, const double* wn, int nwn, const char* btype)
{
    gs_zpk_t s;
    gs_cheby2_zpk((int)n, rs, &s);
    gs_design_ba((int)n, wn, nwn, btype, &s);
    double b[GS_FILTER_MAXTAPS], a[GS_FILTER_MAXTAPS];
    int nb, na;
    gs_zpk2ba(&s, b, a, &nb, &na);
    return gs_filter_store(b, nb, a, na);
}

long gs_filter_ellip(long n, double rp, double rs, const double* wn, int nwn, const char* btype)
{
    gs_zpk_t s;
    gs_ellip_zpk((int)n, rp, rs, &s);
    gs_design_ba((int)n, wn, nwn, btype, &s);
    double b[GS_FILTER_MAXTAPS], a[GS_FILTER_MAXTAPS];
    int nb, na;
    gs_zpk2ba(&s, b, a, &nb, &na);
    return gs_filter_store(b, nb, a, na);
}

/* ── applicazione ──────────────────────────────────────────────────────────── */

static double* gs_lfilter_core(const double* b, int nb, const double* a, int na,
                               const double* x, int nx, const double* zi, int nzi)
{
    int M = nb > na ? nb : na;
    double bp[GS_FILTER_MAXTAPS];
    double ap[GS_FILTER_MAXTAPS];
    for (int i = 0; i < M; i++) { bp[i] = i < nb ? b[i] : 0.0; ap[i] = i < na ? a[i] : 0.0; }
    double* y = (double*)malloc(sizeof(double) * (size_t)(nx > 0 ? nx : 1));
    if (!y) return NULL;
    double z[GS_FILTER_MAXTAPS];
    for (int i = 0; i < M - 1; i++) z[i] = (zi && i < nzi) ? zi[i] : 0.0;
    for (int t = 0; t < nx; t++)
    {
        double xv = x[t];
        double yv = bp[0] * xv + z[0];
        for (int i = 1; i < M; i++)
        {
            double zz = i < M - 1 ? z[i] : 0.0;
            z[i - 1] = bp[i] * xv - ap[i] * yv + zz;
        }
        y[t] = yv;
    }
    return y;
}

double* gs_filter_lfilter(long h, const double* x, int nx, long* outn)
{
    gs_filter_t* f = gs_filter_by_handle(h);
    if (!f || !x || nx < 0) { *outn = 0; return NULL; }
    *outn = nx;
    return gs_lfilter_core(f->b, f->nb, f->a, f->na, x, nx, NULL, 0);
}

double* gs_filter_filtfilt(long h, const double* x, int nx, long* outn)
{
    gs_filter_t* f = gs_filter_by_handle(h);
    if (!f || !x || nx <= 0) { *outn = 0; return NULL; }
    /* lfilter_zi */
    int ntaps = f->nb > f->na ? f->nb : f->na;
    int n = ntaps;
    double yinf = 0.0;
    for (int i = 0; i < f->nb; i++) yinf += f->b[i];
    double suma = 0.0;
    for (int i = 0; i < f->na; i++) suma += f->a[i];
    if (suma == 0.0) { *outn = 0; return NULL; }
    yinf /= suma;
    double t[GS_FILTER_MAXTAPS];
    for (int i = 0; i < n; i++)
        t[i] = (i < f->nb ? f->b[i] : 0.0) - yinf * (i < f->na ? f->a[i] : 0.0);
    /* flip, cumsum, flip, drop first */
    for (int i = 0, j = n - 1; i < j; i++, j--) { double tmp = t[i]; t[i] = t[j]; t[j] = tmp; }
    double cs[GS_FILTER_MAXTAPS];
    cs[0] = t[0];
    for (int i = 1; i < n; i++) cs[i] = cs[i - 1] + t[i];
    for (int i = 0, j = n - 1; i < j; i++, j--) { double tmp = cs[i]; cs[i] = cs[j]; cs[j] = tmp; }
    double zi[GS_FILTER_MAXTAPS];
    int nzi = n - 1;
    for (int i = 0; i < nzi; i++) zi[i] = cs[i + 1];

    int padlen = 3 * ntaps;
    if (padlen >= nx) padlen = nx - 1;
    if (padlen < 0) padlen = 0;
    int extlen = nx + 2 * padlen;
    double* ext = (double*)malloc(sizeof(double) * (size_t)extlen);
    if (!ext) { *outn = 0; return NULL; }
    for (int i = 0; i < padlen; i++) ext[i] = 2.0 * x[0] - x[padlen - i];
    memcpy(ext + padlen, x, sizeof(double) * (size_t)nx);
    for (int i = 0; i < padlen; i++) ext[padlen + nx + i] = 2.0 * x[nx - 1] - x[nx - 2 - i];

    double zix0[GS_FILTER_MAXTAPS];
    for (int i = 0; i < nzi; i++) zix0[i] = zi[i] * ext[0];
    double* y1 = gs_lfilter_core(f->b, f->nb, f->a, f->na, ext, extlen, zix0, nzi);
    free(ext);
    if (!y1) { *outn = 0; return NULL; }
    double y0 = y1[extlen - 1];
    for (int i = 0, j = extlen - 1; i < j; i++, j--) { double tmp = y1[i]; y1[i] = y1[j]; y1[j] = tmp; }
    double ziy0[GS_FILTER_MAXTAPS];
    for (int i = 0; i < nzi; i++) ziy0[i] = zi[i] * y0;
    double* y2 = gs_lfilter_core(f->b, f->nb, f->a, f->na, y1, extlen, ziy0, nzi);
    free(y1);
    if (!y2) { *outn = 0; return NULL; }
    for (int i = 0, j = extlen - 1; i < j; i++, j--) { double tmp = y2[i]; y2[i] = y2[j]; y2[j] = tmp; }
    double* res = (double*)malloc(sizeof(double) * (size_t)nx);
    if (!res) { free(y2); *outn = 0; return NULL; }
    memcpy(res, y2 + padlen, sizeof(double) * (size_t)nx);
    free(y2);
    *outn = nx;
    return res;
}

double* gs_filter_freqz(long h, int npts, long* outn)
{
    gs_filter_t* f = gs_filter_by_handle(h);
    if (!f || npts < 0) { *outn = 0; return NULL; }
    double* res = (double*)malloc(sizeof(double) * (size_t)(npts > 0 ? npts : 1));
    if (!res) { *outn = 0; return NULL; }
    for (int i = 0; i < npts; i++)
    {
        double w = GS_PI * (double)i / (double)(npts > 0 ? npts : 1);
        double complex num = 0.0 + 0.0 * I;
        for (int k = 0; k < f->nb; k++) num += f->b[k] * cexp(-I * w * (double)k);
        double complex den = 0.0 + 0.0 * I;
        for (int k = 0; k < f->na; k++) den += f->a[k] * cexp(-I * w * (double)k);
        res[i] = cabs(num / den);
    }
    *outn = npts;
    return res;
}
