/* ============================================================================
 * gmi_filters.c — filtri IIR (scipy.signal): design Butterworth/Chebyshev I-II/
 * ellittico (prototipo analogico + trasformazioni di banda + bilineare) e
 * applicazione lfilter/filtfilt/freqz. Porting di CommonLib.SignalFilters.cs.
 * ============================================================================ */

#include "gmi_runtime.h"
#include "gmi_matrix.h"
#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Prototipi cross-modulo (definiti in gmi_math.c): dichiarati qui perché nel
 * repo GMBasic provenivano da un header condiviso. */
void gmi_fft(double* re, double* im, int n, int inverse);
double* gmi_rfft(const double* x, int n, gmb_int_t* outn);
double* gmi_irfft(const double* re, const double* im, int m, gmb_int_t* outn);

/* ── Store degli handle filtro (pattern MATRIX/VECTOR) ── */
#define GMB_FILTER_MAX 256
#define GMB_FILTER_MAXTAPS 64
typedef struct {
    double b[GMB_FILTER_MAXTAPS];
    double a[GMB_FILTER_MAXTAPS];
    int nb, na;
    int in_use;
} gmi_filter_t;
static gmi_filter_t g_filters[GMB_FILTER_MAX];

static gmi_filter_t* gmi_filter_by_handle(int64_t h)
{
    if (h <= 0 || h >= GMB_FILTER_MAX) return NULL;
    gmi_filter_t* f = &g_filters[h];
    return f->in_use ? f : NULL;
}

static int64_t gmi_filter_store(const double* b, int nb, const double* a, int na)
{
    for (int i = 1; i < GMB_FILTER_MAX; i++)
    {
        if (!g_filters[i].in_use)
        {
            gmi_filter_t* f = &g_filters[i];
            memset(f, 0, sizeof(*f));
            for (int j = 0; j < nb && j < GMB_FILTER_MAXTAPS; j++) f->b[j] = b[j];
            for (int j = 0; j < na && j < GMB_FILTER_MAXTAPS; j++) f->a[j] = a[j];
            f->nb = nb; f->na = na; f->in_use = 1;
            return i;
        }
    }
    return 0;
}

/* ── helper numerici ── */
static double gmi_asinh(double x) { return log(x + sqrt(x * x + 1.0)); }
static double gmi_atanh(double x) { return 0.5 * log((1.0 + x) / (1.0 - x)); }

static double complex gmi_casin(double complex z)
{
    /* asin(z) = -i log(i z + sqrt(1 - z^2)) */
    return -I * clog(I * z + csqrt(1.0 - z * z));
}

/* prodotto di (-p_i) per i poli/zeri */
static double complex gmi_cprod_neg(double complex* v, int n)
{
    double complex r = 1.0 + 0.0 * I;
    for (int i = 0; i < n; i++) r *= -v[i];
    return r;
}

/* ── funzioni ellittiche ── */
static double gmi_ellipk(double m)
{
    if (m >= 1.0) return INFINITY;
    if (m <= 0.0) return 3.14159265358979323846 / 2.0;
    double a = 1.0, b = sqrt(1.0 - m);
    for (int i = 0; i < 40; i++)
    {
        double an = 0.5 * (a + b);
        b = sqrt(a * b);
        a = an;
        if (fabs(a - b) < 1e-15 * a) break;
    }
    return (3.14159265358979323846 / 2.0) / a;
}

/* sn di Jacobi via Landen discendente + relazioni per cn/dn (u reale in [0,K]) */
static void gmi_ellip_scndn(double u, double m, double* sn, double* cn, double* dn)
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

static double gmi_ellip_sn(double u, double m)
{
    double sn, cn, dn;
    gmi_ellip_scndn(u, m, &sn, &cn, &dn);
    return sn;
}

/* inversa sn complessa via Landen (scipy _arc_jac_sn) */
static double complex gmi_arc_jac_sn(double complex w, double m)
{
    double k = sqrt(m);
    if (k > 1.0) return 0.0 / 0.0;
    if (k == 1.0) return gmi_atanh(creal(w)); /* caso reale */
    double ks[64];
    int nk = 0;
    ks[nk++] = k; /* k_0 */
    while (ks[nk - 1] != 0.0 && nk < 63)
    {
        double kk = ks[nk - 1];
        double kp = sqrt((1.0 - kk) * (1.0 + kk));
        ks[nk++] = (1.0 - kp) / (1.0 + kp);
    }
    double K = 3.14159265358979323846 / 2.0;
    for (int i = 1; i < nk; i++) K *= 1.0 + ks[i];
    double complex wns = w;
    for (int i = 0; i < nk - 1; i++)
    {
        double kn = ks[i];
        double knext = ks[i + 1];
        wns = 2.0 * wns / ((1.0 + knext) * (1.0 + csqrt((1.0 - kn * wns) * (1.0 + kn * wns))));
    }
    double complex u = (2.0 / 3.14159265358979323846) * gmi_casin(wns);
    return K * u;
}

static double gmi_arc_jac_sc1(double w, double m)
{
    return cimag(gmi_arc_jac_sn(I * w, m));
}

static double gmi_ellipdeg(int n, double m1)
{
    double K1 = gmi_ellipk(m1);
    double K1p = gmi_ellipk(1.0 - m1);
    double q1 = exp(-3.14159265358979323846 * K1p / K1);
    double q = pow(q1, 1.0 / (double)n);
    double num = 0.0;
    for (int mn = 0; mn <= 7; mn++) num += pow(q, (double)(mn * (mn + 1)));
    double den = 1.0;
    for (int md = 1; md <= 8; md++) den += 2.0 * pow(q, (double)(md * md));
    return 16.0 * q * pow(num / den, 4.0);
}

/* ── design (parity scipy iirfilter) ── */

typedef struct { double complex z[128]; double complex p[128]; double k; int nz, np; } gmi_zpk_t;

static int gmi_rel_degree(const gmi_zpk_t* s) { return s->np - s->nz; }

static void gmi_lp2lp(gmi_zpk_t* s, double wo)
{
    for (int i = 0; i < s->nz; i++) s->z[i] *= wo;
    for (int i = 0; i < s->np; i++) s->p[i] *= wo;
    s->k *= pow(wo, (double)gmi_rel_degree(s));
}

static void gmi_lp2hp(gmi_zpk_t* s, double wo)
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
    s->k *= creal(gmi_cprod_neg(zold, onz) / gmi_cprod_neg(pold, onp));
}

static void gmi_lp2bp(gmi_zpk_t* s, double wo, double bw)
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
    int deg = gmi_rel_degree(s);
    for (int i = 0; i < deg; i++) zb[nz++] = 0.0;
    memcpy(s->z, zb, sizeof(double complex) * (size_t)nz);
    memcpy(s->p, pb, sizeof(double complex) * (size_t)np);
    s->nz = nz; s->np = np;
    s->k *= pow(bw, (double)deg);
}

static void gmi_lp2bs(gmi_zpk_t* s, double wo, double bw)
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
    s->k *= creal(gmi_cprod_neg(zold, onz) / gmi_cprod_neg(pold, onp));
}

static void gmi_bilinear_zpk(gmi_zpk_t* s, double fs)
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

static void gmi_zpk2ba(const gmi_zpk_t* s, double* b, double* a, int* nb, int* na)
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
static void gmi_butter_zpk(int n, gmi_zpk_t* s)
{
    s->nz = 0; s->np = n; s->k = 1.0;
    for (int i = 0; i < n; i++)
    {
        double m = (double)(-n + 1 + 2 * i);
        s->p[i] = -cexp(I * 3.14159265358979323846 * m / (2.0 * (double)n));
    }
}

static void gmi_cheby1_zpk(int n, double rp, gmi_zpk_t* s)
{
    if (n == 0) { s->nz = 0; s->np = 0; s->k = pow(10.0, -rp / 20.0); return; }
    double eps = sqrt(pow(10.0, 0.1 * rp) - 1.0);
    double mu = (1.0 / (double)n) * gmi_asinh(1.0 / eps);
    s->nz = 0; s->np = n;
    for (int i = 0; i < n; i++)
    {
        double m = (double)(-n + 1 + 2 * i);
        double th = 3.14159265358979323846 * m / (2.0 * (double)n);
        s->p[i] = -csinh(mu + I * th);
    }
    s->k = creal(gmi_cprod_neg(s->p, n));
    if (n % 2 == 0) s->k = s->k / sqrt(1.0 + eps * eps);
}

static void gmi_cheby2_zpk(int n, double rs, gmi_zpk_t* s)
{
    if (n == 0) { s->nz = 0; s->np = 0; s->k = 1.0; return; }
    double de = 1.0 / sqrt(pow(10.0, 0.1 * rs) - 1.0);
    double mu = gmi_asinh(1.0 / de) / (double)n;
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
    for (int i = 0; i < nz; i++) s->z[i] = I / csin(mz[i] * 3.14159265358979323846 / (2.0 * (double)n));
    for (int i = 0; i < n; i++)
    {
        double m1 = (double)(-n + 1 + 2 * i);
        double th = 3.14159265358979323846 * m1 / (2.0 * (double)n);
        s->p[i] = -1.0 / csinh(mu + I * th);
    }
    s->k = creal(gmi_cprod_neg(s->p, n) / gmi_cprod_neg(s->z, nz));
}

static void gmi_ellip_zpk(int n, double rp, double rs, gmi_zpk_t* s)
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

    double val0 = gmi_ellipk(ck1_sq);
    double m = gmi_ellipdeg(n, ck1_sq);
    double capk = gmi_ellipk(m);

    int jc[64];
    int nj = n - (n % 2);
    for (int i = 0; i < nj; i++) jc[i] = 1 - (n % 2) + 2 * i;

    double zl[128];
    int nzl = 0;
    for (int i = 0; i < nj; i++)
    {
        double sn = gmi_ellip_sn((double)jc[i] * capk / (double)n, m);
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

    double r = gmi_arc_jac_sc1(1.0 / eps, ck1_sq);
    double v0 = capk * r / ((double)n * val0);

    double sv, cv, dv;
    gmi_ellip_scndn(v0, 1.0 - m, &sv, &cv, &dv);
    double complex pc[128];
    int npc = 0;
    for (int i = 0; i < nj; i++)
    {
        double sj, cj, dj;
        gmi_ellip_scndn((double)jc[i] * capk / (double)n, m, &sj, &cj, &dj);
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
    s->k = creal(gmi_cprod_neg(s->p, npc) / gmi_cprod_neg(s->z, nzc));
    if (n % 2 == 0) s->k = s->k / sqrt(1.0 + eps_sq);
}

typedef void (*gmi_proto_fn)(int, gmi_zpk_t*);
typedef void (*gmi_proto_fn2)(int, double, gmi_zpk_t*);
typedef void (*gmi_proto_fn3)(int, double, double, gmi_zpk_t*);

static int gmi_design_ba(int n, const double* wn, int nwn, const char* btype,
                         gmi_zpk_t* s)
{
    (void)n;
    /* warped (bilinear prewarping, fs=2) */
    double warped[2];
    for (int i = 0; i < nwn && i < 2; i++) warped[i] = 4.0 * tan(3.14159265358979323846 * wn[i] / 2.0);
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
        case 0: default: gmi_lp2lp(s, warped[0]); break;
        case 3: gmi_lp2hp(s, warped[0]); break;
        case 1:
        case 2:
        {
            double bw = warped[1] - warped[0];
            double wo = sqrt(warped[0] * warped[1]);
            if (bt == 1) gmi_lp2bp(s, wo, bw);
            else gmi_lp2bs(s, wo, bw);
            break;
        }
    }
    gmi_bilinear_zpk(s, 2.0);
    return 0;
}

long gm_signal_butter(int n, const double* wn, int nwn, const char* btype)
{
    gmi_zpk_t s;
    gmi_butter_zpk((int)n, &s);
    gmi_design_ba((int)n, wn, nwn, btype, &s);
    double b[GMB_FILTER_MAXTAPS], a[GMB_FILTER_MAXTAPS];
    int nb, na;
    gmi_zpk2ba(&s, b, a, &nb, &na);
        return gmi_filter_store(b, nb, a, na);
}

long gm_signal_cheby1(int n, double rp, const double* wn, int nwn, const char* btype)
{
    gmi_zpk_t s;
    gmi_cheby1_zpk((int)n, rp, &s);
    gmi_design_ba((int)n, wn, nwn, btype, &s);
    double b[GMB_FILTER_MAXTAPS], a[GMB_FILTER_MAXTAPS];
    int nb, na;
    gmi_zpk2ba(&s, b, a, &nb, &na);
    return gmi_filter_store(b, nb, a, na);
}

long gm_signal_cheby2(int n, double rs, const double* wn, int nwn, const char* btype)
{
    gmi_zpk_t s;
    gmi_cheby2_zpk((int)n, rs, &s);
    gmi_design_ba((int)n, wn, nwn, btype, &s);
    double b[GMB_FILTER_MAXTAPS], a[GMB_FILTER_MAXTAPS];
    int nb, na;
    gmi_zpk2ba(&s, b, a, &nb, &na);
    return gmi_filter_store(b, nb, a, na);
}

long gm_signal_ellip(int n, double rp, double rs, const double* wn, int nwn, const char* btype)
{
    gmi_zpk_t s;
    gmi_ellip_zpk((int)n, rp, rs, &s);
    gmi_design_ba((int)n, wn, nwn, btype, &s);
    double b[GMB_FILTER_MAXTAPS], a[GMB_FILTER_MAXTAPS];
    int nb, na;
    gmi_zpk2ba(&s, b, a, &nb, &na);
    return gmi_filter_store(b, nb, a, na);
}

/* ── applicazione ── */

static double* gmi_lfilter_core(const double* b, int nb, const double* a, int na,
                                const double* x, int nx, const double* zi, int nzi)
{
    int M = nb > na ? nb : na;
    double bp[GMB_FILTER_MAXTAPS];
    double ap[GMB_FILTER_MAXTAPS];
    for (int i = 0; i < M; i++) { bp[i] = i < nb ? b[i] : 0.0; ap[i] = i < na ? a[i] : 0.0; }
    double* y = (double*)malloc(sizeof(double) * (size_t)(nx > 0 ? nx : 1));
    if (!y) return NULL;
    double z[GMB_FILTER_MAXTAPS];
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

double* gmi_filter_lfilter(gmb_int_t h, const double* x, int nx, gmb_int_t* outn)
{
    gmi_filter_t* f = gmi_filter_by_handle(h);
    if (!f || !x || nx < 0) { *outn = 0; return NULL; }
    *outn = nx;
    return gmi_lfilter_core(f->b, f->nb, f->a, f->na, x, nx, NULL, 0);
}

double* gmi_filter_filtfilt(gmb_int_t h, const double* x, int nx, gmb_int_t* outn)
{
    gmi_filter_t* f = gmi_filter_by_handle(h);
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
    double t[GMB_FILTER_MAXTAPS];
    for (int i = 0; i < n; i++) t[i] = (i < f->nb ? f->b[i] : 0.0) - yinf * (i < f->na ? f->a[i] : 0.0);
    /* flip, cumsum, flip, drop first */
    for (int i = 0, j = n - 1; i < j; i++, j--) { double tmp = t[i]; t[i] = t[j]; t[j] = tmp; }
    double cs[GMB_FILTER_MAXTAPS];
    cs[0] = t[0];
    for (int i = 1; i < n; i++) cs[i] = cs[i - 1] + t[i];
    for (int i = 0, j = n - 1; i < j; i++, j--) { double tmp = cs[i]; cs[i] = cs[j]; cs[j] = tmp; }
    double zi[GMB_FILTER_MAXTAPS];
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

    double zix0[GMB_FILTER_MAXTAPS];
    for (int i = 0; i < nzi; i++) zix0[i] = zi[i] * ext[0];
    double* y1 = gmi_lfilter_core(f->b, f->nb, f->a, f->na, ext, extlen, zix0, nzi);
    free(ext);
    if (!y1) { *outn = 0; return NULL; }
    double y0 = y1[extlen - 1];
    for (int i = 0, j = extlen - 1; i < j; i++, j--) { double tmp = y1[i]; y1[i] = y1[j]; y1[j] = tmp; }
    double ziy0[GMB_FILTER_MAXTAPS];
    for (int i = 0; i < nzi; i++) ziy0[i] = zi[i] * y0;
    double* y2 = gmi_lfilter_core(f->b, f->nb, f->a, f->na, y1, extlen, ziy0, nzi);
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

double* gmi_filter_freqz(gmb_int_t h, int npts, gmb_int_t* outn)
{
    gmi_filter_t* f = gmi_filter_by_handle(h);
    if (!f || npts < 0) { *outn = 0; return NULL; }
    double* res = (double*)malloc(sizeof(double) * (size_t)(npts > 0 ? npts : 1));
    if (!res) { *outn = 0; return NULL; }
    for (int i = 0; i < npts; i++)
    {
        double w = 3.14159265358979323846 * (double)i / (double)(npts > 0 ? npts : 1);
        double complex num = 0.0 + 0.0 * I;
        for (int k = 0; k < f->nb; k++) num += f->b[k] * cexp(-I * w * (double)k);
        double complex den = 0.0 + 0.0 * I;
        for (int k = 0; k < f->na; k++) den += f->a[k] * cexp(-I * w * (double)k);
        res[i] = cabs(num / den);
    }
    *outn = npts;
    return res;
}

/* ── Wrapper TIMESERIES: design Butterworth + lfilter in un colpo ── */
double* gmi_filter_lowpass(const double* x, int nx, double wn, gmb_int_t n, gmb_int_t* outn)
{
    gmb_int_t h = gm_signal_butter(n, &wn, 1, "low");
    return gmi_filter_lfilter(h, x, nx, outn);
}

double* gmi_filter_highpass(const double* x, int nx, double wn, gmb_int_t n, gmb_int_t* outn)
{
    gmb_int_t h = gm_signal_butter(n, &wn, 1, "high");
    return gmi_filter_lfilter(h, x, nx, outn);
}

double* gmi_filter_bandpass(const double* x, int nx, double w1, double w2, gmb_int_t n, gmb_int_t* outn)
{
    double wn[2] = { w1, w2 };
    gmb_int_t h = gm_signal_butter(n, wn, 2, "bandpass");
    return gmi_filter_lfilter(h, x, nx, outn);
}

double* gmi_filter_bandstop(const double* x, int nx, double w1, double w2, gmb_int_t n, gmb_int_t* outn)
{
    double wn[2] = { w1, w2 };
    gmb_int_t h = gm_signal_butter(n, wn, 2, "bandstop");
    return gmi_filter_lfilter(h, x, nx, outn);
}

/* ════════════════════════════════════════════════════════════════════════════
 * scipy.signal P0: firwin (design FIR a finestra) e find_peaks.
 * ════════════════════════════════════════════════════════════════════════════ */

static double gmi_fir_ideal_lowpass(double fc, double x)
{
    if (x == 0) return 2.0 * fc;
    double arg = 2.0 * M_PI * fc * x;
    return 2.0 * fc * sin(arg) / arg;
}

static void gmi_fir_window(double* w, int n, const char* window)
{
    if (window && strcmp(window, "hamming") == 0)
    {
        for (int i = 0; i < n; i++) w[i] = 0.54 - 0.46 * cos(2.0 * M_PI * i / (n - 1));
    }
    else if (window && strcmp(window, "blackman") == 0)
    {
        for (int i = 0; i < n; i++) w[i] = 0.42 - 0.5 * cos(2.0 * M_PI * i / (n - 1)) + 0.08 * cos(4.0 * M_PI * i / (n - 1));
    }
    else if (window && strcmp(window, "rect") == 0)
    {
        for (int i = 0; i < n; i++) w[i] = 1.0;
    }
    else
    {
        for (int i = 0; i < n; i++) w[i] = 0.5 - 0.5 * cos(2.0 * M_PI * i / (n - 1));
    }
}

/* Ritorna un array allocato di numtaps; cutoff normalizzato (0..0.5). */
double* gmi_firwin(int numtaps, const double* cutoff, int ncut, const char* btype, const char* window)
{
    if (numtaps < 1 || ncut < 1) return NULL;
    double* taps = (double*)calloc((size_t)numtaps, sizeof(double));
    double* w = (double*)malloc(sizeof(double) * (size_t)numtaps);
    if (!taps || !w) { free(taps); free(w); return NULL; }
    gmi_fir_window(w, numtaps, window);
    double m = (numtaps - 1) / 2.0;
    int isHP = btype && strstr(btype, "high") != NULL;
    int isBP = btype && (strstr(btype, "bandpass") != NULL || strcmp(btype, "band") == 0);
    int isBS = btype && (strstr(btype, "bandstop") != NULL || strcmp(btype, "stop") == 0);
    for (int n = 0; n < numtaps; n++)
    {
        double x = n - m;
        double h = 0;
        if (isHP)
        {
            h = (x == 0 ? 1.0 : 0.0) - gmi_fir_ideal_lowpass(cutoff[0], x);
        }
        else if (isBP && ncut >= 2)
        {
            h = gmi_fir_ideal_lowpass(cutoff[1], x) - gmi_fir_ideal_lowpass(cutoff[0], x);
        }
        else if (isBS && ncut >= 2)
        {
            h = (x == 0 ? 1.0 : 0.0) - (gmi_fir_ideal_lowpass(cutoff[1], x) - gmi_fir_ideal_lowpass(cutoff[0], x));
        }
        else
        {
            h = gmi_fir_ideal_lowpass(cutoff[0], x);
        }
        taps[n] = h * w[n] + 0.0; /* +0.0 normalizza -0.0 (parità stampa IL: "0", non "-0") */
    }
    free(w);
    return taps;
}

/* Ritorna array allocato di indici picco; *outn = lunghezza. */
double* gmi_find_peaks(const double* x, int n, double distance, int* outn)
{
    *outn = 0;
    if (n < 3) return NULL;
    int* idx = (int*)malloc(sizeof(int) * (size_t)n);
    double* hts = (double*)malloc(sizeof(double) * (size_t)n);
    int npk = 0;
    for (int i = 1; i < n - 1; i++)
    {
        if (x[i] >= x[i - 1] && x[i] >= x[i + 1])
        {
            if (i > 1 && x[i] == x[i - 1] && x[i - 1] == x[i - 2]) continue;
            idx[npk] = i;
            hts[npk] = x[i];
            npk++;
        }
    }
    if (npk == 0) { free(idx); free(hts); return NULL; }
    double d = distance >= 1 ? distance : 1.0;
    /* ordina per altezza decrescente */
    int* order = (int*)malloc(sizeof(int) * (size_t)npk);
    for (int i = 0; i < npk; i++) order[i] = i;
    for (int i = 0; i < npk; i++)
        for (int j = i + 1; j < npk; j++)
            if (hts[order[j]] > hts[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    int* kept = (int*)malloc(sizeof(int) * (size_t)npk);
    int nk = 0;
    for (int k = 0; k < npk; k++)
    {
        int ok = 1;
        for (int j = 0; j < nk; j++)
            if (abs(kept[j] - idx[order[k]]) < d) { ok = 0; break; }
        if (ok) kept[nk++] = idx[order[k]];
    }
    /* ordina per indice crescente */
    for (int i = 0; i < nk; i++)
        for (int j = i + 1; j < nk; j++)
            if (kept[j] < kept[i]) { int t = kept[i]; kept[i] = kept[j]; kept[j] = t; }
    double* out = nk > 0 ? (double*)malloc(sizeof(double) * (size_t)nk) : NULL;
    for (int i = 0; i < nk; i++) out[i] = kept[i];
    *outn = nk;
    free(idx); free(hts); free(order); free(kept);
    return out;
}

/* ════════════════════════════════════════════════════════════════════════════
 * scipy.signal P1: welch, savgol_filter, resample, decimate.
 * ════════════════════════════════════════════════════════════════════════════ */

/* Welch: ritorna matrice flat [nfreqs][2] (freq, psd); *out_rows = nfreqs. */
double* gmi_welch(const double* x, int n, int nperseg, double fs, int* out_rows)
{
    if (nperseg <= 0 || nperseg > n) nperseg = n;
    int noverlap = nperseg / 2;
    int step = nperseg - noverlap;
    int nseg = (n - noverlap) / step; if (nseg < 1) nseg = 1;
    double* win = (double*)malloc(sizeof(double) * (size_t)nperseg);
    if (!win) { *out_rows = 0; return NULL; }
    for (int i = 0; i < nperseg; i++) win[i] = 0.5 - 0.5 * cos(2.0 * 3.141592653589793 * i / (nperseg - 1));
    double win2 = 0;
    for (int i = 0; i < nperseg; i++) win2 += win[i] * win[i];
    int nfreqs = nperseg / 2 + 1;
    double* psd = (double*)calloc((size_t)nfreqs, sizeof(double));
    double* re = (double*)malloc(sizeof(double) * (size_t)nperseg);
    double* im = (double*)malloc(sizeof(double) * (size_t)nperseg);
    if (!psd || !re || !im) { free(win); free(psd); free(re); free(im); *out_rows = 0; return NULL; }
    double scale = 1.0 / (fs * (win2 > 0 ? win2 : 1.0));
    for (int s = 0; s < nseg; s++)
    {
        int start = s * step;
        for (int i = 0; i < nperseg; i++) { re[i] = x[start + i] * win[i]; im[i] = 0; }
        gmi_fft(re, im, nperseg, 0);
        for (int k = 0; k < nfreqs; k++) psd[k] += (re[k] * re[k] + im[k] * im[k]) * scale;
    }
    for (int k = 0; k < nfreqs; k++) psd[k] /= nseg;
    double* out = (double*)malloc(sizeof(double) * (size_t)nfreqs * 2);
    if (!out) { free(win); free(psd); free(re); free(im); *out_rows = 0; return NULL; }
    for (int k = 0; k < nfreqs; k++)
    {
        out[k * 2] = k * fs / nperseg;
        out[k * 2 + 1] = psd[k];
    }
    *out_rows = nfreqs;
    free(win); free(psd); free(re); free(im);
    return out;
}

static int gmi_savgol_solve(double* a, double* b, int n, double* x);
static void gmi_poly_fit(const double* data, int start, int end, int order, double* coeff);
static double gmi_poly_eval(const double* coeff, double t, int order);

static int gmi_savgol_coeffs(int half, int order, double* coeffs)
{
    int w = 2 * half + 1;
    double* A = (double*)malloc(sizeof(double) * (size_t)w * (size_t)(order + 1));
    double* ata = (double*)malloc(sizeof(double) * (size_t)(order + 1) * (size_t)(order + 1));
    double* atb = (double*)calloc((size_t)(order + 1), sizeof(double));
    if (!A || !ata || !atb) { free(A); free(ata); free(atb); return -1; }
    for (int j = 0; j < w; j++)
        for (int p = 0; p <= order; p++)
            A[j * (order + 1) + p] = pow(j - half, p);
    for (int i = 0; i < w; i++)
        for (int p = 0; p <= order; p++)
            for (int q = 0; q <= order; q++)
                ata[p * (order + 1) + q] += A[i * (order + 1) + p] * A[i * (order + 1) + q];
    for (int p = 0; p <= order; p++) atb[p] = A[half * (order + 1) + p];
    /* risolvi ata·b = atb (gauss) */
    double* b = (double*)malloc(sizeof(double) * (size_t)(order + 1));
    if (!b) { free(A); free(ata); free(atb); return -1; }
    int ok = gmi_savgol_solve(ata, atb, order + 1, b);
    if (ok == 0)
    {
        for (int j = 0; j < w; j++)
        {
            double s = 0;
            for (int p = 0; p <= order; p++) s += A[j * (order + 1) + p] * b[p];
            coeffs[j] = s;
        }
    }
    free(A); free(ata); free(atb); free(b);
    return ok;
}

static int gmi_savgol_solve(double* a, double* b, int n, double* x)
{
    /* eliminazione gaussiana con pivot (a è n×n flat, b rhs, x soluzione) */
    double* m = (double*)malloc(sizeof(double) * (size_t)n * (size_t)(n + 1));
    if (!m) return -1;
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++) m[i * (n + 1) + j] = a[i * n + j];
        m[i * (n + 1) + n] = b[i];
    }
    for (int col = 0; col < n; col++)
    {
        int piv = col;
        for (int r = col + 1; r < n; r++) if (fabs(m[r * (n + 1) + col]) > fabs(m[piv * (n + 1) + col])) piv = r;
        if (fabs(m[piv * (n + 1) + col]) < 1e-14) { free(m); return -1; }
        if (piv != col) for (int j = 0; j <= n; j++) { double t = m[col * (n + 1) + j]; m[col * (n + 1) + j] = m[piv * (n + 1) + j]; m[piv * (n + 1) + j] = t; }
        for (int r = col + 1; r < n; r++)
        {
            double f = m[r * (n + 1) + col] / m[col * (n + 1) + col];
            for (int j = col; j <= n; j++) m[r * (n + 1) + j] -= f * m[col * (n + 1) + j];
        }
    }
    for (int i = n - 1; i >= 0; i--)
    {
        double s = m[i * (n + 1) + n];
        for (int j = i + 1; j < n; j++) s -= m[i * (n + 1) + j] * x[j];
        x[i] = s / m[i * (n + 1) + i];
    }
    free(m);
    return 0;
}

static void gmi_poly_fit(const double* data, int start, int end, int order, double* coeff)
{
    int len = end - start;
    double* ata = (double*)calloc((size_t)(order + 1) * (size_t)(order + 1), sizeof(double));
    double* atb = (double*)calloc((size_t)(order + 1), sizeof(double));
    if (!ata || !atb) { free(ata); free(atb); for (int i = 0; i <= order; i++) coeff[i] = 0; return; }
    for (int i = 0; i < len; i++)
    {
        for (int p = 0; p <= order; p++)
        {
            atb[p] += pow(i, p) * data[start + i];
            for (int q = 0; q <= order; q++) ata[p * (order + 1) + q] += pow(i, p + q);
        }
    }
    if (gmi_savgol_solve(ata, atb, order + 1, coeff) != 0)
        for (int i = 0; i <= order; i++) coeff[i] = 0;
    free(ata); free(atb);
}

static double gmi_poly_eval(const double* coeff, double t, int order)
{
    double s = 0;
    for (int p = order; p >= 0; p--) s = s * t + coeff[p];
    return s;
}

double* gmi_savgol_filter(const double* x, int n, int window, int polyorder, gmb_int_t* outn)
{
    if (window > n) window = n;
    if (window % 2 == 0) window++;
    if (window < 3) { double* r = (double*)malloc(sizeof(double) * (size_t)n); for (int i = 0; i < n; i++) r[i] = x[i]; *outn = n; return r; }
    int half = (window - 1) / 2;
    double* coeffs = (double*)malloc(sizeof(double) * (size_t)window);
    if (!coeffs) { *outn = 0; return NULL; }
    if (gmi_savgol_coeffs(half, polyorder, coeffs) != 0) { free(coeffs); *outn = 0; return NULL; }
    /* estende i bordi con estrapolazione polinomiale */
    double* ext = (double*)malloc(sizeof(double) * (size_t)(n + 2 * half));
    double* pl = (double*)malloc(sizeof(double) * (size_t)(polyorder + 1));
    double* pr = (double*)malloc(sizeof(double) * (size_t)(polyorder + 1));
    if (!ext || !pl || !pr) { free(coeffs); free(ext); free(pl); free(pr); *outn = 0; return NULL; }
    gmi_poly_fit(x, 0, window < n ? window : n, polyorder, pl);
    gmi_poly_fit(x, n - window > 0 ? n - window : 0, n, polyorder, pr);
    int wl = window < n ? window : n;
    for (int i = 0; i < half; i++) ext[i] = gmi_poly_eval(pl, i - half, polyorder);
    for (int i = 0; i < n; i++) ext[half + i] = x[i];
    for (int i = 0; i < half; i++) ext[half + n + i] = gmi_poly_eval(pr, wl + i, polyorder);
    double* result = (double*)malloc(sizeof(double) * (size_t)n);
    if (!result) { free(coeffs); free(ext); free(pl); free(pr); *outn = 0; return NULL; }
    for (int i = 0; i < n; i++)
    {
        double s = 0;
        for (int j = 0; j < window; j++) s += coeffs[j] * ext[i + j];
        result[i] = s;
    }
    *outn = n;
    free(coeffs); free(ext); free(pl); free(pr);
    return result;
}

double* gmi_resample(const double* x, int n, int num, gmb_int_t* outn)
{
    if (num <= 0) { *outn = 0; return NULL; }
    double* re = (double*)calloc((size_t)n, sizeof(double));
    double* im = (double*)calloc((size_t)n, sizeof(double));
    if (!re || !im) { free(re); free(im); *outn = 0; return NULL; }
    for (int i = 0; i < n; i++) re[i] = x[i];
    gmi_fft(re, im, n, 0);
    double* sre = (double*)calloc((size_t)num, sizeof(double));
    double* sim = (double*)calloc((size_t)num, sizeof(double));
    if (!sre || !sim) { free(re); free(im); free(sre); free(sim); *outn = 0; return NULL; }
    int keep = (n / 2 < num / 2) ? n / 2 : num / 2;
    sre[0] = re[0]; sim[0] = im[0];
    for (int k = 1; k < keep; k++) { sre[k] = re[k]; sim[k] = im[k]; }
    if (num % 2 == 0 && num / 2 < n)
    {
        int ny = num / 2 < n - 1 ? num / 2 : n - 1;
        sre[ny] = re[ny]; sim[ny] = im[ny];
    }
    for (int k = 1; k < keep; k++)
        if (num - k >= 0 && n - k >= 0) { sre[num - k] = re[n - k]; sim[num - k] = im[n - k]; }
    gmi_fft(sre, sim, num, 1); /* divide per num */
    double* result = (double*)malloc(sizeof(double) * (size_t)num);
    if (!result) { free(re); free(im); free(sre); free(sim); *outn = 0; return NULL; }
    for (int i = 0; i < num; i++) result[i] = sre[i] * ((double)num / n);
    *outn = num;
    free(re); free(im); free(sre); free(sim);
    return result;
}

double* gmi_decimate(const double* x, int n, int q, gmb_int_t* outn)
{
    if (q <= 1) { double* r = (double*)malloc(sizeof(double) * (size_t)n); for (int i = 0; i < n; i++) r[i] = x[i]; *outn = n; return r; }
    double fc = 0.8 / q;
    int nt = (int)ceil(4.0 / fc) * 2 + 1;
    if (nt > n) nt = n % 2 == 0 ? n - 1 : n;
    if (nt < 3) nt = 3;
    double* taps = gmi_firwin(nt, &fc, 1, "lowpass", "hann");
    if (!taps) { *outn = 0; return NULL; }
    double* smoothed = (double*)calloc((size_t)n, sizeof(double));
    if (!smoothed) { free(taps); *outn = 0; return NULL; }
    int half = nt / 2;
    for (int i = 0; i < n; i++)
    {
        double s = 0;
        for (int j = 0; j < nt; j++)
        {
            int idx = i + j - half;
            if (idx >= 0 && idx < n) s += taps[j] * x[idx];
        }
        smoothed[i] = s;
    }
    free(taps);
    int m = (n + q - 1) / q;
    double* result = (double*)malloc(sizeof(double) * (size_t)m);
    if (!result) { free(smoothed); *outn = 0; return NULL; }
    for (int i = 0; i < m; i++) result[i] = smoothed[i * q];
    *outn = m;
    free(smoothed);
    return result;
}

/* ---- Bridge C99 P0/P1: array alloc + matrici handle ---- */
gmb_int_t gmi_matrix_from_flat(const double* flat, int rows, int cols)
{
    int64_t mh = gmi_matrix_create(rows, cols);
    if (mh <= 0) return 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            gmi_matrix_set(mh, r, c, flat[r * cols + c]);
    return (gmb_int_t)mh;
}

gmb_int_t gmi_welch_c99(const double* x, int n, int nperseg, double fs)
{
    int rows = 0;
    double* w = gmi_welch(x, n, nperseg, fs, &rows);
    if (!w || rows <= 0) { free(w); return 0; }
    gmb_int_t h = gmi_matrix_from_flat(w, rows, 2);
    free(w);
    return h;
}

double* gmi_savgol_c99(const double* x, int n, int wl, int po, gmb_int_t* outn)
{ return gmi_savgol_filter(x, n, wl, po, outn); }
double* gmi_resample_c99(const double* x, int n, int num, gmb_int_t* outn)
{ return gmi_resample(x, n, num, outn); }
double* gmi_decimate_c99(const double* x, int n, int q, gmb_int_t* outn)
{ return gmi_decimate(x, n, q, outn); }

gmb_int_t gmi_rfft_c99(const double* x, int n)
{
    gmb_int_t m = 0;
    double* r = gmi_rfft(x, n, &m);
    if (!r || m <= 0) { free(r); return 0; }
    gmb_int_t h = gmi_matrix_from_flat(r, m, 2);
    free(r);
    return h;
}
double* gmi_irfft_c99(const double* re, const double* im, int m, gmb_int_t* outn)
{ return gmi_irfft(re, im, m, outn); }

double* gmi_firwin_c99(int numtaps, const double* cutoff, int ncut, const char* btype, const char* window, gmb_int_t* outn)
{
    double* t = numtaps > 0 && cutoff ? gmi_firwin(numtaps, cutoff, ncut, btype, window) : NULL;
    if (t) *outn = numtaps;
    return t;
}

double* gmi_find_peaks_alloc(const double* x, int n, double distance, gmb_int_t* outn)
{
    int on = 0;
    double* r = gmi_find_peaks(x, n, distance, &on);
    *outn = on;
    return r;
}
