/* ============================================================================
 * gmi_stats.c — funzioni statistiche e test di ipotesi (TTEST, ANOVA, FISHER, etc.)
 * Implementazione portante delle funzioni CommonLib/Numerics/HypothesisTests.cs
 * ============================================================================ */

#include "gmi_runtime.h"
#include "gmi_legacy_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* API reali del runtime (l'altra AI usava gmi_make_dict/gmi_make_error, inesistenti):
 * i dict si costruiscono con gmi_dict_new + gmi_dict_set + gmi_make_val_dict. */


/* ============================================================================
 * Funzioni di supporto per distribuzioni
 * ============================================================================ */

/* CDF della distribuzione t di Student (approssimazione di Abramowitz & Stegun) */
static double student_t_cdf(double t, double df) {
    if (df <= 0) return 0.5;
    double a = 0.5 * df, b = 0.5;
    double x = df / (df + t * t);
    double ibeta = gm_special_betainc(a, b, x);
    return t >= 0 ? 1.0 - 0.5 * ibeta : 0.5 * ibeta;
}

/* CDF della distribuzione F di Fisher-Snedecor */
double f_distribution_cdf(double f, double df1, double df2) {
    if (df1 <= 0 || df2 <= 0) return 0.5;
    if (f <= 0) return 0.0;
    double x = df1 * f / (df2 + df1 * f);
    return gm_special_betainc(0.5 * df1, 0.5 * df2, x);
}

/* ============================================================================
 * T-Test (Indipendente, Appaiato, One-Sample)
 * ============================================================================ */

/* T-Test indipendente (two-sample, equal variance assumed) */

/* T-Test appaiato (paired) */

/* T-Test one-sample */

/* ============================================================================
 * ANOVA (One-Way)
 * ============================================================================ */


/* ANALYSIS.ANOVA → Analysis.AnovaResult: dict ESTESO (canone IlRuntime.Anova) con
 * FValue, PValue, DFBetween, DFWithin, SSBetween, SSWithin, MSBetween, MSWithin — i
 * membri del tipo UDT (gmi_anova qui sopra è il dict HypothesisResult di STATS.ANOVA,
 * che ha solo Statistic/PValue/DegreesOfFreedom). */

/* ============================================================================
 * Chi-Square Goodness of Fit
 * ============================================================================ */


/* ============================================================================
 * Fisher's Exact Test
 * ============================================================================ */

static double factorial_log(int n) {
    if (n <= 1) return 0;
    double sum = 0;
    for (int i = 2; i <= n; i++) {
        sum += log(i);
    }
    return sum;
}

static double binomial_log(int n, int k) {
    if (k < 0 || k > n) return -INFINITY;
    return factorial_log(n) - factorial_log(k) - factorial_log(n - k);
}

double gmi_fisher_hypergeom(int v, int firstRow, int firstColumn, int total)
{
    double l = binomial_log(firstColumn, v) + binomial_log(total - firstColumn, firstRow - v) - binomial_log(total, firstRow);
    return exp(l);
}


/* ============================================================================
 * Z-Test (One-Sample)
 * ============================================================================ */


/* ============================================================================
 * Kolmogorov-Smirnov Two-Sample
 * ============================================================================ */

int compare_doubles(const void *a, const void *b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

double gmi_ks_inside(int fc, int sc, double stat)
{
    double* prob = (double*)calloc((size_t)(fc + 1) * (size_t)(sc + 1), sizeof(double));
    if (!prob) return 0;
    prob[0] = 1.0;
    for (int i = 0; i <= fc; i++)
        for (int j = 0; j <= sc; j++)
        {
            if (i == 0 && j == 0) continue;
            double dist = fabs((double)i / fc - (double)j / sc);
            if (dist >= stat - 1e-15) continue;
            int visited = i + j;
            double pr = 0;
            if (i > 0) pr += prob[(i - 1) * (sc + 1) + j] * (double)i / visited;
            if (j > 0) pr += prob[i * (sc + 1) + (j - 1)] * (double)j / visited;
            prob[i * (sc + 1) + j] = pr;
        }
    double r = prob[fc * (sc + 1) + sc];
    free(prob);
    return r;
}


/* ============================================================================
 * Shapiro-Wilk Test (Approssimazione)
 * ============================================================================ */

double gmi_poly(const double* c, int len, double x)
{
    double r = 0;
    for (int i = len - 1; i >= 0; i--) r = r * x + c[i];
    return r;
}


/* Trasformate Z D'Agostino precise (parità scipy.stats/C# HypothesisTests.cs):
 * UNICA implementazione condivisa da SKEWTEST/KURTOSISTEST standalone e NORMALTEST
 * (mai due formule divergenti). */
double gmi_skewtest_z(const double* a, int n)
{
    double cn = (double)n, sum = 0;
    for (int i = 0; i < n; i++) sum += a[i];
    double mean = sum / cn;
    double m2 = 0, m3 = 0;
    for (int i = 0; i < n; i++) { double d = a[i] - mean; m2 += d * d; m3 += d * d * d; }
    m2 /= cn; m3 /= cn;
    if (m2 == 0.0) m2 = 1.0;   /* campione costante: come C# (secondMoment=1) */
    double skew = m3 / pow(m2, 1.5);
    double skewY = skew * sqrt((cn + 1.0) * (cn + 3.0) / (6.0 * (cn - 2.0)));
    if (skewY == 0.0) skewY = 1.0;   /* scipy: y=1 per campioni simmetrici */
    double beta2 = 3.0 * (cn * cn + 27.0 * cn - 70.0) * (cn + 1.0) * (cn + 3.0) /
                   ((cn - 2.0) * (cn + 5.0) * (cn + 7.0) * (cn + 9.0));
    double w2 = -1.0 + sqrt(2.0 * (beta2 - 1.0));
    double delta = 1.0 / sqrt(0.5 * log(w2));
    double alpha = sqrt(2.0 / (w2 - 1.0));
    return delta * asinh(skewY / alpha);
}

double gmi_kurtosistest_z(const double* a, int n)
{
    double cn = (double)n, sum = 0;
    for (int i = 0; i < n; i++) sum += a[i];
    double mean = sum / cn;
    double m2 = 0, m4 = 0;
    for (int i = 0; i < n; i++) { double d = a[i] - mean; m2 += d * d; m4 += d * d * d * d; }
    m2 /= cn; m4 /= cn;
    if (m2 == 0.0) return NAN;
    double kurtosis = m4 / (m2 * m2);
    double expected = 3.0 * (cn - 1.0) / (cn + 1.0);
    double variance = 24.0 * cn * (cn - 2.0) * (cn - 3.0) /
                      (pow(cn + 1.0, 2.0) * (cn + 3.0) * (cn + 5.0));
    double standardized = (kurtosis - expected) / sqrt(variance);
    double rootBeta = 6.0 * (cn * cn - 5.0 * cn + 2.0) / ((cn + 7.0) * (cn + 9.0)) *
                      sqrt(6.0 * (cn + 3.0) * (cn + 5.0) / (cn * (cn - 2.0) * (cn - 3.0)));
    double ab = 6.0 + 8.0 / rootBeta * (2.0 / rootBeta + sqrt(1.0 + 4.0 / (rootBeta * rootBeta)));
    double denominator = 1.0 + standardized * sqrt(2.0 / (ab - 4.0));
    double cubeRoot = copysign(pow(fabs((1.0 - 2.0 / ab) / denominator), 1.0 / 3.0), denominator);
    return (1.0 - 2.0 / (9.0 * ab) - cubeRoot) / sqrt(2.0 / (9.0 * ab));
}

/* ============================================================================
 * Normality Test (D'Agostino-Pearson)
 * ============================================================================ */




static int cmp_dbl(const void* a, const void* b) { double da = *(const double*)a, db = *(const double*)b; return (da > db) - (da < db); }

double gmi_stats_rank_sum(double* data, int n, double* out_ranks)
{
    int* idx = (int*)malloc(sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) idx[i] = i;
    /* insertion sort per indice */
    for (int i = 1; i < n; i++)
    {
        int key = idx[i];
        double kv = data[key];
        int j = i - 1;
        while (j >= 0 && data[idx[j]] > kv) { idx[j + 1] = idx[j]; j--; }
        idx[j + 1] = key;
    }
    int i = 0;
    while (i < n)
    {
        int j = i;
        while (j + 1 < n && data[idx[j + 1]] == data[idx[i]]) j++;
        double avg = 1.0 + (i + j) / 2.0;
        for (int k = i; k <= j; k++) out_ranks[idx[k]] = avg;
        i = j + 1;
    }
    free(idx);
    return 0;
}

double gmi_stats_pearson(const double* x, const double* y, int n, double* p)
{
    if (n < 2) { *p = NAN; return NAN; }
    double mx = 0, my = 0;
    for (int i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
    mx /= n; my /= n;
    double sxx = 0, syy = 0, sxy = 0;
    for (int i = 0; i < n; i++)
    {
        double dx = x[i] - mx, dy = y[i] - my;
        sxx += dx * dx; syy += dy * dy; sxy += dx * dy;
    }
    if (sxx == 0 || syy == 0) { *p = NAN; return NAN; }
    double r = sxy / sqrt(sxx * syy);
    if (fabs(r) >= 1.0) { *p = 0; return r; }
    double t = r * sqrt((n - 2) / (1 - r * r));
    *p = 2.0 * student_t_cdf(-fabs(t), n - 2);
    return r;
}








/* ── Varianti "flat" (array nativi double* + len) per il backend C99: il transpiler
   ha array native (non gmi_val_t list). Costruiscono una lista e delegano alla
   versione gmi_val_t. ── */







static double gmi_stats_pct(const double* sorted, int n, double q);

double gmi_mode(const double* a, int n)
{
    if (n <= 0) return NAN;
    /* copia e ordina, conta run più lungo (pareggi → valore minore) */
    double* s = (double*)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) s[i] = a[i];
    qsort(s, (size_t)n, sizeof(double), cmp_dbl);
    double best = s[0];
    int bestCnt = 1, cur = 1;
    for (int i = 1; i < n; i++)
    {
        if (s[i] == s[i - 1]) cur++;
        else cur = 1;
        if (cur > bestCnt) { bestCnt = cur; best = s[i]; }
    }
    free(s);
    return best;
}

double gmi_moment(const double* a, int n, int order)
{
    if (n <= 0) return NAN;
    double mean = 0;
    for (int i = 0; i < n; i++) mean += a[i];
    mean /= n;
    double s = 0;
    for (int i = 0; i < n; i++) s += pow(a[i] - mean, order);
    return s / n;
}

double gmi_iqr(const double* a, int n)
{
    if (n <= 0) return NAN;
    double* s = (double*)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) s[i] = a[i];
    qsort(s, (size_t)n, sizeof(double), cmp_dbl);
    double q1 = gmi_stats_pct(s, n, 0.25);
    double q3 = gmi_stats_pct(s, n, 0.75);
    free(s);
    return q3 - q1;
}

static double gmi_stats_pct(const double* sorted, int n, double q)
{
    double pos = q * (n - 1);
    int lo = (int)floor(pos);
    double frac = pos - lo;
    if (lo + 1 >= n) return sorted[n - 1];
    return sorted[lo] + frac * (sorted[lo + 1] - sorted[lo]);
}

/* ANOVA con gruppi flat concatenati + sizes[k] (parità VM: la lista di liste
   annidata non è leggibile da gmi_val_as_list). */

/* STATS.ANOVA(groups) con matrice jagged (gmb_double_t* rows[k]): appiattisce le righe
 * (usando sizes[]) e delega a gmi_anova_flat — C99 emitter, parità PCodeVM. */

/* ANALYSIS.ANOVA da array PIATTO (data = gruppi concatenati, sizes[i] per gruppo):
 * dict ESTESO Analysis.AnovaResult (canone IlRuntime.Anova), come gmi_anova_result ma
 * da puntatori (emissione C99Backend su matrice jagged). gmi_anova_flat è il dict
 * HypothesisResult di STATS.ANOVA. */


/* ANOVA su matrice RETTANGOLARE flat (C99Native DIM g(k-1, ncol-1)): i gruppi sono le
 * righe, tutte di lunghezza ncol. */
