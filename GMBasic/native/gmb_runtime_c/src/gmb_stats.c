/* ============================================================================
 * gmb_stats.c — funzioni statistiche e test di ipotesi (TTEST, ANOVA, FISHER, etc.)
 * Implementazione portante delle funzioni CommonLib/Numerics/HypothesisTests.cs
 * ============================================================================ */

#include "gmb_runtime.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* API reali del runtime (l'altra AI usava gmb_make_dict/gmb_make_error, inesistenti):
 * i dict si costruiscono con gmb_dict_new + gmb_dict_set + gmb_make_val_dict. */
static gmb_val_t gmb_make_error(const char* msg) { (void)msg; return gmb_make_val_double(0.0 / 0.0); }

static gmb_val_t gmb_make_dict(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    gmb_dict_t d = gmb_dict_new();
    for (int i = 0; i < n; i++)
    {
        const char* key = va_arg(ap, const char*);
        gmb_val_t v = va_arg(ap, gmb_val_t);
        gmb_dict_set(&d, key, v);
    }
    va_end(ap);
    return gmb_make_val_dict_take(&d);
}

/* ============================================================================
 * Funzioni di supporto per distribuzioni
 * ============================================================================ */

/* CDF della distribuzione t di Student (approssimazione di Abramowitz & Stegun) */
static double student_t_cdf(double t, double df) {
    if (df <= 0) return 0.5;
    double a = 0.5 * df, b = 0.5;
    double x = df / (df + t * t);
    double ibeta = gmb_incbeta(a, b, x);
    return t >= 0 ? 1.0 - 0.5 * ibeta : 0.5 * ibeta;
}

/* CDF della distribuzione F di Fisher-Snedecor */
static double f_distribution_cdf(double f, double df1, double df2) {
    if (df1 <= 0 || df2 <= 0) return 0.5;
    if (f <= 0) return 0.0;
    double x = df1 * f / (df2 + df1 * f);
    return gmb_incbeta(0.5 * df1, 0.5 * df2, x);
}

/* ============================================================================
 * T-Test (Indipendente, Appaiato, One-Sample)
 * ============================================================================ */

/* T-Test indipendente (two-sample, equal variance assumed) */
gmb_val_t gmb_ttest(gmb_val_t x_val, gmb_val_t y_val) {
    /* Estrai array */
    gmb_list_t x = gmb_val_as_list(x_val);
    gmb_list_t y = gmb_val_as_list(y_val);
    
    int n1 = gmb_list_count(x);
    int n2 = gmb_list_count(y);
    
    if (n1 < 2 || n2 < 2) {
        return gmb_make_error("TTEST richiede almeno 2 elementi per gruppo");
    }
    
    /* Calcola medie */
    double sum1 = 0, sum2 = 0;
    for (int i = 0; i < n1; i++) {
        sum1 += gmb_val_to_double(gmb_list_get(x, i));
    }
    for (int i = 0; i < n2; i++) {
        sum2 += gmb_val_to_double(gmb_list_get(y, i));
    }
    double mean1 = sum1 / n1;
    double mean2 = sum2 / n2;
    
    /* Calcola varianze campionarie */
    double var1 = 0, var2 = 0;
    for (int i = 0; i < n1; i++) {
        double d = gmb_val_to_double(gmb_list_get(x, i)) - mean1;
        var1 += d * d;
    }
    for (int i = 0; i < n2; i++) {
        double d = gmb_val_to_double(gmb_list_get(y, i)) - mean2;
        var2 += d * d;
    }
    var1 /= (n1 - 1);
    var2 /= (n2 - 1);
    
    /* Pooled standard error */
    double pooled_var = ((n1 - 1) * var1 + (n2 - 1) * var2) / (n1 + n2 - 2);
    double se = sqrt(pooled_var * (1.0/n1 + 1.0/n2));
    
    if (se == 0) {
        return gmb_make_dict(2, 
            "Statistic", gmb_make_val_double(0),
            "PValue", gmb_make_val_double(1.0));
    }
    
    double t_stat = (mean1 - mean2) / se;
    double df = n1 + n2 - 2;
    double p_value = 2 * (1 - student_t_cdf(fabs(t_stat), df));
    
    return gmb_make_dict(3,
        "Statistic", gmb_make_val_double(t_stat),
        "PValue", gmb_make_val_double(p_value),
        "DegreesOfFreedom", gmb_make_val_double(df));
}

/* T-Test appaiato (paired) */
gmb_val_t gmb_ttestpaired(gmb_val_t x_val, gmb_val_t y_val) {
    gmb_list_t x = gmb_val_as_list(x_val);
    gmb_list_t y = gmb_val_as_list(y_val);
    
    int n = gmb_list_count(x);
    if (n != gmb_list_count(y) || n < 2) {
        return gmb_make_error("TTESTPAIRED richiede array della stessa lunghezza >= 2");
    }
    
    /* Calcola differenze */
    double *diff = malloc(n * sizeof(double));
    double sum_diff = 0;
    for (int i = 0; i < n; i++) {
        diff[i] = gmb_val_to_double(gmb_list_get(x, i)) - gmb_val_to_double(gmb_list_get(y, i));
        sum_diff += diff[i];
    }
    double mean_diff = sum_diff / n;
    
    /* Varianza differenze */
    double var_diff = 0;
    for (int i = 0; i < n; i++) {
        double d = diff[i] - mean_diff;
        var_diff += d * d;
    }
    var_diff /= (n - 1);
    
    double se = sqrt(var_diff / n);
    if (se == 0) {
        free(diff);
        return gmb_make_dict(2,
            "Statistic", gmb_make_val_double(0),
            "PValue", gmb_make_val_double(1.0));
    }
    
    double t_stat = mean_diff / se;
    double df = n - 1;
    double p_value = 2 * (1 - student_t_cdf(fabs(t_stat), df));
    
    free(diff);
    return gmb_make_dict(3,
        "Statistic", gmb_make_val_double(t_stat),
        "PValue", gmb_make_val_double(p_value),
        "DegreesOfFreedom", gmb_make_val_double(df));
}

/* T-Test one-sample */
gmb_val_t gmb_ttestonesample(gmb_val_t x_val, gmb_val_t mu_val) {
    gmb_list_t x = gmb_val_as_list(x_val);
    int n = gmb_list_count(x);
    if (n < 2) {
        return gmb_make_error("TTESTONESAMPLE richiede almeno 2 elementi");
    }
    
    double mu = gmb_val_to_double(mu_val);
    
    /* Calcola media e varianza */
    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum += gmb_val_to_double(gmb_list_get(x, i));
    }
    double mean = sum / n;
    
    double var = 0;
    for (int i = 0; i < n; i++) {
        double d = gmb_val_to_double(gmb_list_get(x, i)) - mean;
        var += d * d;
    }
    var /= (n - 1);
    
    double se = sqrt(var / n);
    if (se == 0) {
        return gmb_make_dict(2,
            "Statistic", gmb_make_val_double(0),
            "PValue", gmb_make_val_double(1.0));
    }
    
    double t_stat = (mean - mu) / se;
    double df = n - 1;
    double p_value = 2 * (1 - student_t_cdf(fabs(t_stat), df));
    
    return gmb_make_dict(3,
        "Statistic", gmb_make_val_double(t_stat),
        "PValue", gmb_make_val_double(p_value),
        "DegreesOfFreedom", gmb_make_val_double(df));
}

/* ============================================================================
 * ANOVA (One-Way)
 * ============================================================================ */

gmb_val_t gmb_anova(gmb_val_t groups_val) {
    gmb_list_t groups = gmb_val_as_list(groups_val);
    int k = gmb_list_count(groups);
    
    if (k < 2) {
        return gmb_make_error("ANOVA richiede almeno 2 gruppi");
    }
    
    /* Calcola grand mean e SS */
    double grand_sum = 0, grand_n = 0;
    double *group_means = malloc(k * sizeof(double));
    double *group_ns = malloc(k * sizeof(double));
    
    for (int i = 0; i < k; i++) {
        gmb_list_t group = gmb_val_as_list(gmb_list_get(groups, i));
        int n = gmb_list_count(group);
        group_ns[i] = n;
        
        double sum = 0;
        for (int j = 0; j < n; j++) {
            double val = gmb_val_to_double(gmb_list_get(group, j));
            sum += val;
        }
        group_means[i] = sum / n;
        grand_sum += sum;
        grand_n += n;
    }
    
    double grand_mean = grand_sum / grand_n;
    
    /* SS Between */
    double ss_between = 0;
    for (int i = 0; i < k; i++) {
        ss_between += group_ns[i] * (group_means[i] - grand_mean) * (group_means[i] - grand_mean);
    }
    
    /* SS Within */
    double ss_within = 0;
    for (int i = 0; i < k; i++) {
        gmb_list_t group = gmb_val_as_list(gmb_list_get(groups, i));
        int n = group_ns[i];
        double mean = group_means[i];
        for (int j = 0; j < n; j++) {
            double val = gmb_val_to_double(gmb_list_get(group, j));
            ss_within += (val - mean) * (val - mean);
        }
    }
    
    double df_between = k - 1;
    double df_within = grand_n - k;
    
    if (df_within <= 0) {
        free(group_means);
        free(group_ns);
        return gmb_make_error("ANOVA: gradi di libertà within <= 0");
    }
    
    double ms_between = ss_between / df_between;
    double ms_within = ss_within / df_within;
    
    double f_stat = (ms_within == 0) ? INFINITY : ms_between / ms_within;
    double p_value = 1 - f_distribution_cdf(f_stat, df_between, df_within);
    
    free(group_means);
    free(group_ns);
    
    /* STATS.ANOVA e STATS.Anova sono lo stesso identificatore: il dict è il SUPERSET dei
     * due contratti (HypothesisResult + AnovaResult), con gli stessi valori. */
    return gmb_make_dict(10,
        "Statistic", gmb_make_val_double(f_stat),
        "PValue", gmb_make_val_double(p_value),
        "DegreesOfFreedom", gmb_make_val_double(df_within),
        "FValue", gmb_make_val_double(f_stat),
        "DFBetween", gmb_make_val_double(df_between),
        "DFWithin", gmb_make_val_double(df_within),
        "SSBetween", gmb_make_val_double(ss_between),
        "SSWithin", gmb_make_val_double(ss_within),
        "MSBetween", gmb_make_val_double(ms_between),
        "MSWithin", gmb_make_val_double(ms_within));
}

/* STATS.Anova → STATS.AnovaResult: dict ESTESO (canone IlRuntime.Anova) con
 * FValue, PValue, DFBetween, DFWithin, SSBetween, SSWithin, MSBetween, MSWithin — i
 * membri del tipo UDT (gmb_anova qui sopra è il dict HypothesisResult di STATS.ANOVA,
 * che ha solo Statistic/PValue/DegreesOfFreedom). */
gmb_val_t gmb_anova_result(gmb_val_t groups_val) {
    gmb_list_t groups = gmb_val_as_list(groups_val);
    int k = gmb_list_count(groups);
    if (k < 2) return gmb_make_error("ANOVA richiede almeno 2 gruppi");
    double* gmeans = (double*)malloc(sizeof(double) * (size_t)k);
    double* gns = (double*)malloc(sizeof(double) * (size_t)k);
    if (!gmeans || !gns) { free(gmeans); free(gns); return gmb_make_error("ANOVA oom"); }
    double grand_sum = 0, grand_n = 0;
    for (int i = 0; i < k; i++)
    {
        gmb_list_t group = gmb_val_as_list(gmb_list_get(groups, i));
        int n = gmb_list_count(group);
        gns[i] = n;
        double s = 0;
        for (int j = 0; j < n; j++) s += gmb_val_to_double(gmb_list_get(group, j));
        gmeans[i] = s / (n > 0 ? n : 1);
        grand_sum += s;
        grand_n += n;
    }
    if (grand_n <= (double)k) { free(gmeans); free(gns); return gmb_make_error("ANOVA: gradi di libertà within <= 0"); }
    double grand_mean = grand_sum / grand_n;
    double ssb = 0, ssw = 0;
    for (int i = 0; i < k; i++)
    {
        ssb += gns[i] * (gmeans[i] - grand_mean) * (gmeans[i] - grand_mean);
        gmb_list_t group = gmb_val_as_list(gmb_list_get(groups, i));
        int n = (int)gns[i];
        for (int j = 0; j < n; j++) { double d = gmb_val_to_double(gmb_list_get(group, j)) - gmeans[i]; ssw += d * d; }
    }
    double dfb = (double)(k - 1), dfw = grand_n - (double)k;
    double msb = ssb / dfb, msw = ssw / dfw;
    double fv = (msw == 0) ? INFINITY : msb / msw;
    double pv = 1.0 - f_distribution_cdf(fv, dfb, dfw);
    free(gmeans); free(gns);
    return gmb_make_dict(10,
        "FValue", gmb_make_val_double(fv),
        "PValue", gmb_make_val_double(pv),
        "DFBetween", gmb_make_val_double(dfb),
        "DFWithin", gmb_make_val_double(dfw),
        "SSBetween", gmb_make_val_double(ssb),
        "SSWithin", gmb_make_val_double(ssw),
        "MSBetween", gmb_make_val_double(msb),
        "MSWithin", gmb_make_val_double(msw),
        "Statistic", gmb_make_val_double(fv),
        "DegreesOfFreedom", gmb_make_val_double(dfw));
}

/* ============================================================================
 * Chi-Square Goodness of Fit
 * ============================================================================ */

gmb_val_t gmb_chi2(gmb_val_t observed_val, gmb_val_t expected_val) {
    gmb_list_t observed = gmb_val_as_list(observed_val);
    gmb_list_t expected = gmb_val_as_list(expected_val);
    
    int n = gmb_list_count(observed);
    if (n != gmb_list_count(expected) || n < 2) {
        return gmb_make_error("CHI2 richiede array della stessa lunghezza >= 2");
    }
    
    double chi2 = 0;
    for (int i = 0; i < n; i++) {
        double obs = gmb_val_to_double(gmb_list_get(observed, i));
        double exp = gmb_val_to_double(gmb_list_get(expected, i));
        if (exp <= 0) {
            return gmb_make_error("CHI2: valori attesi devono essere > 0");
        }
        chi2 += (obs - exp) * (obs - exp) / exp;
    }
    
    double df = n - 1;
    /* Approssimazione p-value per chi-square */
    double p_value = 1.0 - gmb_gamma_p(0.5 * df, 0.5 * chi2); /* chi2 CDF esatta */
    
    return gmb_make_dict(3,
        "Statistic", gmb_make_val_double(chi2),
        "PValue", gmb_make_val_double(p_value),
        "DegreesOfFreedom", gmb_make_val_double(df));
}

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

static double gmb_fisher_hypergeom(int v, int firstRow, int firstColumn, int total)
{
    double l = binomial_log(firstColumn, v) + binomial_log(total - firstColumn, firstRow - v) - binomial_log(total, firstRow);
    return exp(l);
}

gmb_val_t gmb_fisher(gmb_int_t a, gmb_int_t b, gmb_int_t c, gmb_int_t d) {
    if (a < 0 || b < 0 || c < 0 || d < 0) return gmb_make_error("FISHER: conteggi devono essere non negativi");
    int firstRow = (int)a + (int)b, secondRow = (int)c + (int)d, firstColumn = (int)a + (int)c;
    int total = firstRow + secondRow;
    if (total == 0) return gmb_make_dict(3, "Statistic", gmb_make_val_double(NAN), "PValue", gmb_make_val_double(1), "DegreesOfFreedom", gmb_make_val_double(NAN));
    int lower = firstColumn - secondRow > 0 ? firstColumn - secondRow : 0;
    int upper = firstColumn < firstRow ? firstColumn : firstRow;
    double observed = gmb_fisher_hypergeom((int)a, firstRow, firstColumn, total);
    double p = 0;
    for (int v = lower; v <= upper; v++)
    {
        double pr = gmb_fisher_hypergeom(v, firstRow, firstColumn, total);
        if (pr <= observed * (1.0 + 1e-12)) p += pr;
    }
    if (p > 1) p = 1;
    double odds = (b == 0 || c == 0) ? INFINITY : (double)a * (double)d / ((double)b * (double)c);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(odds), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(NAN));
}

/* ============================================================================
 * Z-Test (One-Sample)
 * ============================================================================ */

gmb_val_t gmb_ztest(gmb_val_t x_val, gmb_val_t mu_val, gmb_val_t sigma_val) {
    gmb_list_t x = gmb_val_as_list(x_val);
    int n = gmb_list_count(x);
    if (n < 1) {
        return gmb_make_error("ZTEST richiede almeno 1 elemento");
    }
    
    double mu = gmb_val_to_double(mu_val);
    double sigma = gmb_val_to_double(sigma_val);
    
    if (sigma <= 0) {
        return gmb_make_error("ZTEST: sigma deve essere > 0");
    }
    
    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum += gmb_val_to_double(gmb_list_get(x, i));
    }
    double mean = sum / n;
    
    double z = (mean - mu) / (sigma / sqrt(n));
    
    /* CDF normale standard approssimata */
    double p_value = 2 * (1 - 0.5 * (1 + erf(z / sqrt(2))));
    
    return gmb_make_dict(3,
        "Statistic", gmb_make_val_double(z),
        "PValue", gmb_make_val_double(p_value),
        "DegreesOfFreedom", gmb_make_val_double(n - 1));
}

/* ============================================================================
 * Kolmogorov-Smirnov Two-Sample
 * ============================================================================ */

static int compare_doubles(const void *a, const void *b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

static double gmb_ks_inside(int fc, int sc, double stat)
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

gmb_val_t gmb_ks(gmb_val_t x_val, gmb_val_t y_val) {
    gmb_list_t x = gmb_val_as_list(x_val);
    gmb_list_t y = gmb_val_as_list(y_val);
    
    int n1 = gmb_list_count(x);
    int n2 = gmb_list_count(y);
    
    if (n1 == 0 || n2 == 0) {
        return gmb_make_error("KS: array non vuoti richiesti");
    }
    
    /* Copia e ordina i dati */
    double *x_sorted = malloc(n1 * sizeof(double));
    double *y_sorted = malloc(n2 * sizeof(double));
    
    for (int i = 0; i < n1; i++) {
        x_sorted[i] = gmb_val_to_double(gmb_list_get(x, i));
    }
    for (int i = 0; i < n2; i++) {
        y_sorted[i] = gmb_val_to_double(gmb_list_get(y, i));
    }
    
    qsort(x_sorted, n1, sizeof(double), compare_doubles);
    qsort(y_sorted, n2, sizeof(double), compare_doubles);
    
    /* Calcola D statistic */
    /* Statistica KS (merge con parità C#/scipy: <=, contatore che avanza) */
    double d = 0;
    int i = 0, j = 0;
    while (i < n1 || j < n2)
    {
        int take_first = (j >= n2) || (i < n1 && x_sorted[i] <= y_sorted[j]);
        double value = take_first ? x_sorted[i] : y_sorted[j];
        while (i < n1 && x_sorted[i] <= value) i++;
        while (j < n2 && y_sorted[j] <= value) j++;
        double diff = fabs((double)i / n1 - (double)j / n2);
        if (diff > d) d = diff;
    }
    free(x_sorted);
    free(y_sorted);
    /* p-value esatto: probabilità interna alla banda (DP C#/scipy) */
    double inside = gmb_ks_inside(n1, n2, d);
    double p_value = 1.0 - inside;
    if (p_value < 0) p_value = 0;
    if (p_value > 1) p_value = 1;
    
    return gmb_make_dict(3,
        "Statistic", gmb_make_val_double(d),
        "PValue", gmb_make_val_double(p_value),
        "DegreesOfFreedom", gmb_make_val_double(-1));
}

/* ============================================================================
 * Shapiro-Wilk Test (Approssimazione)
 * ============================================================================ */

static double gmb_poly(const double* c, int len, double x)
{
    double r = 0;
    for (int i = len - 1; i >= 0; i--) r = r * x + c[i];
    return r;
}

gmb_val_t gmb_shapiro(gmb_val_t x_val) {
    gmb_list_t x = gmb_val_as_list(x_val);
    int n = gmb_list_count(x);
    if (n < 3 || n > 5000) return gmb_make_error("SHAPIRO: richiede 3-5000 osservazioni");
    double* s = (double*)malloc(sizeof(double) * (size_t)n);
    if (!s) return gmb_make_error("SHAPIRO oom");
    for (int i = 0; i < n; i++) s[i] = gmb_val_to_double(gmb_list_get(x, i));
    qsort(s, (size_t)n, sizeof(double), compare_doubles);
    if (s[0] == s[n - 1]) { free(s); return gmb_make_dict(3, "Statistic", gmb_make_val_double(1), "PValue", gmb_make_val_double(1), "DegreesOfFreedom", gmb_make_val_double(NAN)); }
    int half = n / 2;
    double* ns = (double*)malloc(sizeof(double) * (size_t)half);
    if (!ns) { free(s); return gmb_make_error("SHAPIRO oom"); }
    double ssq = 0;
    for (int i = 0; i < half; i++)
    {
        double prob = (i + 1.0 - 0.375) / (n + 0.25);
        ns[i] = gmb_dist_normal_ppf(0, 1, prob);
        ssq += ns[i] * ns[i];
    }
    ssq *= 2.0;
    double rn = 1.0 / sqrt(n);
    double c1[6] = {0, 0.221157, -0.147981, -2.07119, 4.434685, -2.706056};
    double first = gmb_poly(c1, 6, rn) - ns[0] / sqrt(ssq);
    double c2[6] = {0, 0.042981, -0.293762, -1.752461, 5.682633, -3.582633};
    double second = n > 5 ? gmb_poly(c2, 6, rn) - ns[1] / sqrt(ssq) : 0;
    double remsq = n > 5 ? 2.0 * (ns[0] * ns[0] + ns[1] * ns[1]) : 2.0 * ns[0] * ns[0];
    double remwq = n > 5 ? 2.0 * (first * first + second * second) : 2.0 * first * first;
    double scale = sqrt((ssq - remsq) / (1.0 - remwq));
    double wr = 0;
    for (int i = 0; i < half; i++)
    {
        double w = i == 0 ? first : (i == 1 && n > 5 ? second : -ns[i] / scale);
        wr += w * (s[n - 1 - i] - s[i]);
    }
    double mean = 0;
    for (int i = 0; i < n; i++) mean += s[i];
    mean /= n;
    double ts = 0;
    for (int i = 0; i < n; i++) { double d = s[i] - mean; ts += d * d; }
    double stat = wr * wr / ts;
    if (stat < 0) stat = 0;
    if (stat > 1) stat = 1;
    if (n == 3)
    {
        double p = 6.0 / M_PI * (asin(sqrt(stat)) - asin(sqrt(0.75)));
        if (p < 0) p = 0;
        if (p > 1) p = 1;
        free(s); free(ns);
        return gmb_make_dict(3, "Statistic", gmb_make_val_double(stat), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(NAN));
    }
    double trans = log(1.0 - stat);
    double loc, sc;
    if (n <= 11)
    {
        double cg[2] = {-2.273, 0.459};
        double gamma = gmb_poly(cg, 2, n);
        if (trans >= gamma) { free(s); free(ns); return gmb_make_dict(3, "Statistic", gmb_make_val_double(stat), "PValue", gmb_make_val_double(2.220446049250313e-16), "DegreesOfFreedom", gmb_make_val_double(NAN)); }
        trans = -log(gamma - trans);
        double cl[4] = {0.544, -0.39978, 0.025054, -0.0006714};
        double cs[4] = {1.3822, -0.77857, 0.062767, -0.0020322};
        loc = gmb_poly(cl, 4, n);
        sc = exp(gmb_poly(cs, 4, n));
    }
    else
    {
        double lg = log(n);
        double cl[4] = {-1.5861, -0.31082, -0.083751, 0.0038915};
        double cs[3] = {-0.4803, -0.082676, 0.0030302};
        loc = gmb_poly(cl, 4, lg);
        sc = exp(gmb_poly(cs, 3, lg));
    }
    double z = (trans - loc) / sc;
    double pv = 1.0 - gmb_dist_normal_cdf(0, 1, z);
    free(s); free(ns);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(stat), "PValue", gmb_make_val_double(pv), "DegreesOfFreedom", gmb_make_val_double(NAN));
}

/* Trasformate Z D'Agostino precise (parità scipy.stats/C# HypothesisTests.cs):
 * UNICA implementazione condivisa da SKEWTEST/KURTOSISTEST standalone e NORMALTEST
 * (mai due formule divergenti). */
static double gmb_skewtest_z(const double* a, int n)
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

static double gmb_kurtosistest_z(const double* a, int n)
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

gmb_val_t gmb_normaltest(gmb_val_t x_val) {
    gmb_list_t x = gmb_val_as_list(x_val);
    int n = gmb_list_count(x);
    if (n < 20) return gmb_make_error("NORMALTEST: richiede almeno 20 osservazioni");
    double* vals = (double*)malloc(sizeof(double) * (size_t)n);
    if (!vals) return gmb_make_error("NORMALTEST oom");
    double sum = 0;
    for (int i = 0; i < n; i++) { vals[i] = gmb_val_to_double(gmb_list_get(x, i)); sum += vals[i]; }
    double mean = sum / n;
    double m2 = 0, m3 = 0, m4 = 0;
    for (int i = 0; i < n; i++) { double d = vals[i] - mean; m2 += d * d; m3 += d * d * d; m4 += d * d * d * d; }
    m2 /= n; m3 /= n; m4 /= n;
    double cn = (double)n;
    if (m2 == 0) { free(vals); return gmb_make_dict(3, "Statistic", gmb_make_val_double(NAN), "PValue", gmb_make_val_double(NAN), "DegreesOfFreedom", gmb_make_val_double(2)); }
    double skewZ = gmb_skewtest_z(vals, n);
    double kurtZ = gmb_kurtosistest_z(vals, n);
    double stat = skewZ * skewZ + kurtZ * kurtZ;
    double p = 1.0 - gmb_gamma_p(1.0, stat / 2.0);
    free(vals);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(stat), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(2));
}

static int gmb_stats_len(const gmb_val_t* v) { return (int)gmb_list_count(gmb_val_as_list(*v)); }
static double gmb_stats_at(const gmb_val_t* v, int i) { return gmb_val_to_double(gmb_list_get(gmb_val_as_list(*v), i)); }

static double* gmb_stats_to_arr(const gmb_val_t* v, int* n)
{
    gmb_list_t l = gmb_val_as_list(*v);
    *n = (int)gmb_list_count(l);
    if (*n <= 0) return NULL;
    double* a = (double*)malloc(sizeof(double) * (size_t)*n);
    if (!a) return NULL;
    for (int i = 0; i < *n; i++) a[i] = gmb_val_to_double(gmb_list_get(l, i));
    return a;
}

static int cmp_dbl(const void* a, const void* b) { double da = *(const double*)a, db = *(const double*)b; return (da > db) - (da < db); }

static double gmb_stats_rank_sum(double* data, int n, double* out_ranks)
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

static double gmb_stats_pearson(const double* x, const double* y, int n, double* p)
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

gmb_val_t gmb_pearsonr(gmb_val_t x, gmb_val_t y)
{
    int nx = 0, ny = 0;
    double* a = gmb_stats_to_arr(&x, &nx);
    double* b = gmb_stats_to_arr(&y, &ny);
    int n = nx < ny ? nx : ny;
    double p;
    double r = gmb_stats_pearson(a, b, n, &p);
    free(a); free(b);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(r), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(n - 2));
}

gmb_val_t gmb_spearmanr(gmb_val_t x, gmb_val_t y)
{
    int nx = 0, ny = 0;
    double* a = gmb_stats_to_arr(&x, &nx);
    double* b = gmb_stats_to_arr(&y, &ny);
    int n = nx < ny ? nx : ny;
    double* ra = (double*)malloc(sizeof(double) * (size_t)n);
    double* rb = (double*)malloc(sizeof(double) * (size_t)n);
    gmb_stats_rank_sum(a, n, ra);
    gmb_stats_rank_sum(b, n, rb);
    double p;
    double r = gmb_stats_pearson(ra, rb, n, &p);
    free(a); free(b); free(ra); free(rb);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(r), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(n - 2));
}

gmb_val_t gmb_kendalltau(gmb_val_t x, gmb_val_t y)
{
    int nx = 0, ny = 0;
    double* a = gmb_stats_to_arr(&x, &nx);
    double* b = gmb_stats_to_arr(&y, &ny);
    int n = nx < ny ? nx : ny;
    int conc = 0, disc = 0, tx = 0, ty = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            double dx = a[j] - a[i], dy = b[j] - b[i];
            double pr = dx * dy;
            if (pr > 0) conc++; else if (pr < 0) disc++;
            if (dx == 0) tx++;
            if (dy == 0) ty++;
        }
    int n0 = n * (n - 1) / 2;
    double den = sqrt((double)(n0 - tx) * (double)(n0 - ty));
    double tau = den > 0 ? (double)(conc - disc) / den : 0.0;
    double v = n > 1 ? (2.0 * (2 * n + 5)) / (9.0 * n * (n - 1)) : NAN;
    double p = v > 0 ? 2.0 * gmb_dist_normal_cdf(0.0, 1.0, -fabs(tau / sqrt(v))) : NAN;
    free(a); free(b);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(tau), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(NAN));
}
gmb_val_t gmb_ftest(gmb_val_t x, gmb_val_t y)
{
    int nx = 0, ny = 0;
    double* a = gmb_stats_to_arr(&x, &nx);
    double* b = gmb_stats_to_arr(&y, &ny);
    int n1 = nx, n2 = ny;
    if (n1 < 2 || n2 < 2) { free(a); free(b); return gmb_make_dict(3, "Statistic", gmb_make_val_double(NAN), "PValue", gmb_make_val_double(NAN), "DegreesOfFreedom", gmb_make_val_double(NAN)); }
    double m1 = 0, m2 = 0;
    for (int i = 0; i < n1; i++) m1 += a[i];
    for (int i = 0; i < n2; i++) m2 += b[i];
    m1 /= n1; m2 /= n2;
    double v1 = 0, v2 = 0;
    for (int i = 0; i < n1; i++) v1 += (a[i] - m1) * (a[i] - m1);
    for (int i = 0; i < n2; i++) v2 += (b[i] - m2) * (b[i] - m2);
    v1 /= (n1 - 1); v2 /= (n2 - 1);
    free(a); free(b);
    if (v1 <= 0 || v2 <= 0) return gmb_make_dict(3, "Statistic", gmb_make_val_double(NAN), "PValue", gmb_make_val_double(NAN), "DegreesOfFreedom", gmb_make_val_double(NAN));
    double f = v1 / v2, df1 = (double)(n1 - 1), df2 = (double)(n2 - 1);
    double cdf = gmb_dist_f_cdf(df1, df2, f);
    double p = 2.0 * (cdf < 0.5 ? cdf : 1.0 - cdf);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(f), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(df1));
}

gmb_val_t gmb_linregress(gmb_val_t x, gmb_val_t y)
{
    int nx = 0, ny = 0;
    double* a = gmb_stats_to_arr(&x, &nx);
    double* b = gmb_stats_to_arr(&y, &ny);
    int n = nx < ny ? nx : ny;
    double mx = 0, my = 0;
    for (int i = 0; i < n; i++) { mx += a[i]; my += b[i]; }
    mx /= n; my /= n;
    double sxx = 0, sxy = 0;
    for (int i = 0; i < n; i++) { double dx = a[i] - mx; sxx += dx * dx; sxy += dx * (b[i] - my); }
    double slope = sxx != 0 ? sxy / sxx : NAN;
    double intercept = my - slope * mx;
    double ss = 0, sstot = 0;
    for (int i = 0; i < n; i++)
    {
        double e = b[i] - (intercept + slope * a[i]);
        ss += e * e;
        sstot += (b[i] - my) * (b[i] - my);
    }
    double df = n - 2;
    double stderr_x = sxx > 0 ? sqrt(ss / df / sxx) : NAN;   /* NON `stderr`: su Windows è un macro */
    double t = stderr_x > 0 ? slope / stderr_x : 0;
    double p = 2.0 * student_t_cdf(-fabs(t), df);
    double r = sstot > 0 ? (slope >= 0 ? 1 : -1) * sqrt(ss / sstot < 1 ? 1 - ss / sstot : 0) : 0;
    /* "R2" = coefficiente di determinazione = r² (canone CommonLib.Numerics.LinearRegression
     * `rSquared`, IlRuntime.Regress). Prima vi si metteva `r` (il coefficiente di correlazione
     * SIGNED): REGRESSION.Regress stampava 0.9998 (r) invece di 0.9996 (r²). */
    double r2 = r * r;
    free(a); free(b);
    return gmb_make_dict(6,
        "Slope", gmb_make_val_double(slope),
        "Intercept", gmb_make_val_double(intercept),
        "R", gmb_make_val_double(r),
        /* STATS.LINREGRESS espone "R" (IlRuntime.LinRegress); REGRESSION.Regress espone
         * "R2" (IlRuntime.Regress) — stesso runtime condiviso: entrambe le chiavi. */
        "R2", gmb_make_val_double(r2),
        "PValue", gmb_make_val_double(p),
        "Stderr", gmb_make_val_double(stderr_x));
}

gmb_val_t gmb_chi2con_flat(const double* t, int nr, int nc)
{
    if (nr <= 0 || nc <= 0) return gmb_make_error("CHI2CON richiede una tabella");    double* rs = (double*)calloc((size_t)nr, sizeof(double));
    double* cs = (double*)calloc((size_t)nc, sizeof(double));
    if (!rs || !cs) { free(rs); free(cs); return gmb_make_error("CHI2CON oom"); }
    double total = 0;
    for (int i = 0; i < nr; i++) for (int j = 0; j < nc; j++) { rs[i] += t[i * nc + j]; cs[j] += t[i * nc + j]; total += t[i * nc + j]; }
    double chi2 = 0;
    for (int i = 0; i < nr; i++)
        for (int j = 0; j < nc; j++)
        {
            double exp = rs[i] * cs[j] / total;
            if (exp > 0) { double d = t[i * nc + j] - exp; chi2 += d * d / exp; }
        }
    double dof = (nr - 1.0) * (nc - 1.0);
    double p = 1.0 - gmb_gamma_p(dof / 2.0, chi2 / 2.0);
    free(rs); free(cs);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(chi2), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(dof));
}

gmb_val_t gmb_chi2con_jagged(const double** rows, int nr, int nc)
{
    if (nr <= 0 || nc <= 0 || !rows) return gmb_make_error("CHI2CON richiede una tabella");
    double* t = (double*)malloc(sizeof(double) * (size_t)nr * (size_t)nc);
    if (!t) return gmb_make_error("CHI2CON oom");
    for (int i = 0; i < nr; i++)
        for (int j = 0; j < nc; j++)
            t[i * nc + j] = rows[i] ? rows[i][j] : 0.0;
    gmb_val_t r = gmb_chi2con_flat(t, nr, nc);
    free(t);
    return r;
}

gmb_val_t gmb_mannwhitney(gmb_val_t x, gmb_val_t y)
{
    int nx = 0, ny = 0;
    double* a = gmb_stats_to_arr(&x, &nx);
    double* b = gmb_stats_to_arr(&y, &ny);
    double u = 0;
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++)
        {
            if (a[i] > b[j]) u += 1.0;
            else if (a[i] == b[j]) u += 0.5;
        }
    double mean = nx * ny / 2.0;
    int n = nx + ny;
    double* all = (double*)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < nx; i++) all[i] = a[i];
    for (int i = 0; i < ny; i++) all[nx + i] = b[i];
    qsort(all, (size_t)n, sizeof(double), cmp_dbl);
    double ties = 0;
    int i = 0;
    while (i < n)
    {
        int j = i;
        while (j + 1 < n && all[j + 1] == all[i]) j++;
        double t = j - i + 1;
        ties += t * (t - 1) * (t + 1);
        i = j + 1;
    }
    double varU = (nx * ny / 12.0) * ((n + 1.0) - ties / (n * (n - 1.0)));
    double z = varU > 0 ? (u - mean) / sqrt(varU) : 0;
    double p = 2.0 * gmb_dist_normal_cdf(0.0, 1.0, -fabs(z));
    free(a); free(b); free(all);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(u), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(NAN));
}

gmb_val_t gmb_wilcoxon(gmb_val_t x, gmb_val_t y)
{
    int nx = 0, ny = 0;
    double* a = gmb_stats_to_arr(&x, &nx);
    double* b = gmb_stats_to_arr(&y, &ny);
    int n = nx < ny ? nx : ny;
    double* diffs = (double*)malloc(sizeof(double) * (size_t)n);
    double* absd = (double*)malloc(sizeof(double) * (size_t)n);
    double* ranks = (double*)malloc(sizeof(double) * (size_t)n);
    int m = 0;
    for (int i = 0; i < n; i++)
    {
        double d = a[i] - b[i];
        if (d != 0) { diffs[m] = d; absd[m] = fabs(d); m++; }
    }
    if (m == 0) { free(a); free(b); free(diffs); free(absd); free(ranks); return gmb_make_dict(3, "Statistic", gmb_make_val_double(0), "PValue", gmb_make_val_double(1), "DegreesOfFreedom", gmb_make_val_double(NAN)); }
    gmb_stats_rank_sum(absd, m, ranks);
    double w = 0;
    for (int i = 0; i < m; i++) if (diffs[i] > 0) w += ranks[i];
    double mean = m * (m + 1.0) / 4.0;
    double varW = m * (m + 1.0) * (2 * m + 1.0) / 24.0;
    double z = varW > 0 ? (w - mean) / sqrt(varW) : 0;
    double p = 2.0 * gmb_dist_normal_cdf(0.0, 1.0, -fabs(z));
    free(a); free(b); free(diffs); free(absd); free(ranks);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(w), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(NAN));
}

/* ── Varianti "flat" (array nativi double* + len) per il backend C99: il transpiler
   ha array native (non gmb_val_t list). Costruiscono una lista e delegano alla
   versione gmb_val_t. ── */

static gmb_val_t gmb_stats_list_from_arr(const double* a, int n)
{
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_double(a[i]));
    return gmb_make_val_list_take(&l);
}

gmb_val_t gmb_pearsonr_flat(const double* x, int nx, const double* y, int ny)
{
    gmb_val_t xl = gmb_stats_list_from_arr(x, nx), yl = gmb_stats_list_from_arr(y, ny);
    return gmb_pearsonr(xl, yl);
}
gmb_val_t gmb_spearmanr_flat(const double* x, int nx, const double* y, int ny)
{
    gmb_val_t xl = gmb_stats_list_from_arr(x, nx), yl = gmb_stats_list_from_arr(y, ny);
    return gmb_spearmanr(xl, yl);
}

gmb_val_t gmb_kendalltau_flat(const double* x, int nx, const double* y, int ny)
{
    gmb_val_t xl = gmb_stats_list_from_arr(x, nx), yl = gmb_stats_list_from_arr(y, ny);
    return gmb_kendalltau(xl, yl);
}
gmb_val_t gmb_ftest_flat(const double* x, int nx, const double* y, int ny)
{
    gmb_val_t xl = gmb_stats_list_from_arr(x, nx), yl = gmb_stats_list_from_arr(y, ny);
    return gmb_ftest(xl, yl);
}
gmb_val_t gmb_linregress_flat(const double* x, int nx, const double* y, int ny)
{
    gmb_val_t xl = gmb_stats_list_from_arr(x, nx), yl = gmb_stats_list_from_arr(y, ny);
    return gmb_linregress(xl, yl);
}

/* REGRESSION.Regress: canone CommonLib.Numerics.LinearRegression (una passata,
 * come IlRuntime.Regress e il builtin VM-C gmb_bf_91). NON gmb_linregress, che e'
 * la variante scipy-like di STATS.LINREGRESS (due passate) e diverge di 1 ULP
 * sull'intercetta. */
gmb_val_t gmb_regress_flat(const double* x, int nx, const double* y, int ny)
{
    int n = nx < ny ? nx : ny;
    double slope = 0, intercept = 0, r2 = 0;
    gmb_analysis_linear_regression(x, y, n, &slope, &intercept, &r2);
    return gmb_make_dict(3,
        "Slope", gmb_make_val_double(slope),
        "Intercept", gmb_make_val_double(intercept),
        "R2", gmb_make_val_double(r2));
}
gmb_val_t gmb_mannwhitney_flat(const double* x, int nx, const double* y, int ny)
{
    gmb_val_t xl = gmb_stats_list_from_arr(x, nx), yl = gmb_stats_list_from_arr(y, ny);
    return gmb_mannwhitney(xl, yl);
}
gmb_val_t gmb_wilcoxon_flat(const double* x, int nx, const double* y, int ny)
{
    gmb_val_t xl = gmb_stats_list_from_arr(x, nx), yl = gmb_stats_list_from_arr(y, ny);
    return gmb_wilcoxon(xl, yl);
}
gmb_val_t gmb_skewtest_flat(const double* a, int n)
{
    gmb_val_t l = gmb_stats_list_from_arr(a, n);
    return gmb_skewtest(l);
}
gmb_val_t gmb_kurtosistest_flat(const double* a, int n)
{
    gmb_val_t l = gmb_stats_list_from_arr(a, n);
    return gmb_kurtosistest(l);
}
gmb_val_t gmb_jarquebera_flat(const double* a, int n)
{
    gmb_val_t l = gmb_stats_list_from_arr(a, n);
    return gmb_jarquebera(l);
}
gmb_val_t gmb_describe_flat(const double* a, int n)
{
    gmb_val_t l = gmb_stats_list_from_arr(a, n);
    return gmb_describe(l);
}

gmb_val_t gmb_skewtest(gmb_val_t data)
{
    int n = 0;
    double* a = gmb_stats_to_arr(&data, &n);
    gmb_val_t r;
    if (n < 3) r = gmb_make_dict(3, "Statistic", gmb_make_val_double(NAN), "PValue", gmb_make_val_double(NAN), "DegreesOfFreedom", gmb_make_val_double(NAN));
    else {
        double z = gmb_skewtest_z(a, n);
        double p = 2.0 * gmb_dist_normal_cdf(0.0, 1.0, -fabs(z));
        r = gmb_make_dict(3, "Statistic", gmb_make_val_double(z), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(NAN));
    }
    free(a);
    return r;
}

gmb_val_t gmb_kurtosistest(gmb_val_t data)
{
    int n = 0;
    double* a = gmb_stats_to_arr(&data, &n);
    gmb_val_t r;
    if (n < 4) r = gmb_make_dict(3, "Statistic", gmb_make_val_double(NAN), "PValue", gmb_make_val_double(NAN), "DegreesOfFreedom", gmb_make_val_double(NAN));
    else {
        double z = gmb_kurtosistest_z(a, n);
        double p = 2.0 * gmb_dist_normal_cdf(0.0, 1.0, -fabs(z));
        r = gmb_make_dict(3, "Statistic", gmb_make_val_double(z), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(NAN));
    }
    free(a);
    return r;
}

gmb_val_t gmb_jarquebera(gmb_val_t data)
{
    int n = 0;
    double* a = gmb_stats_to_arr(&data, &n);
    double jb = 0, p = 0;
    if (n >= 3)
    {
        double mean = 0;
        for (int i = 0; i < n; i++) mean += a[i];
        mean /= n;
        double m2 = 0, m3 = 0, m4 = 0;
        for (int i = 0; i < n; i++) { double d = a[i] - mean; m2 += d * d; m3 += d * d * d; m4 += d * d * d * d; }
        m2 /= n; m3 /= n; m4 /= n;
        double s = m2 > 0 ? m3 / pow(m2, 1.5) : 0;
        double k = m2 > 0 ? m4 / (m2 * m2) : 0;
        jb = (n / 6.0) * (s * s + (k - 3.0) * (k - 3.0));
        p = 1.0 - gmb_gamma_p(1.0, jb / 2.0);
    }
    free(a);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(jb), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(NAN));
}

gmb_val_t gmb_describe(gmb_val_t data)
{
    int n = 0;
    double* a = gmb_stats_to_arr(&data, &n);
    double mean = 0;
    for (int i = 0; i < n; i++) mean += a[i];
    mean /= n;
    double var = 0;
    for (int i = 0; i < n; i++) { double d = a[i] - mean; var += d * d; }
    var /= (n > 1 ? n - 1 : 1);
    double mn = a[0], mx = a[0];
    for (int i = 1; i < n; i++) { if (a[i] < mn) mn = a[i]; if (a[i] > mx) mx = a[i]; }
    double sk = gmb_skewness(a, n);
    double ku = gmb_kurtosis(a, n);
    free(a);
    return gmb_make_dict(7,
        "Nobs", gmb_make_val_double(n),
        "Min", gmb_make_val_double(mn),
        "Max", gmb_make_val_double(mx),
        "Mean", gmb_make_val_double(mean),
        "Variance", gmb_make_val_double(var),
        "Skewness", gmb_make_val_double(sk),
        "Kurtosis", gmb_make_val_double(ku));
}

static double gmb_stats_pct(const double* sorted, int n, double q);

double gmb_mode(const double* a, int n)
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

double gmb_moment(const double* a, int n, int order)
{
    if (n <= 0) return NAN;
    double mean = 0;
    for (int i = 0; i < n; i++) mean += a[i];
    mean /= n;
    double s = 0;
    for (int i = 0; i < n; i++) s += pow(a[i] - mean, order);
    return s / n;
}

double gmb_iqr(const double* a, int n)
{
    if (n <= 0) return NAN;
    double* s = (double*)malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) s[i] = a[i];
    qsort(s, (size_t)n, sizeof(double), cmp_dbl);
    double q1 = gmb_stats_pct(s, n, 0.25);
    double q3 = gmb_stats_pct(s, n, 0.75);
    free(s);
    return q3 - q1;
}

static double gmb_stats_pct(const double* sorted, int n, double q)
{
    double pos = q * (n - 1);
    int lo = (int)floor(pos);
    double frac = pos - lo;
    if (lo + 1 >= n) return sorted[n - 1];
    return sorted[lo] + frac * (sorted[lo + 1] - sorted[lo]);
}

/* ANOVA con gruppi flat concatenati + sizes[k] (parità VM: la lista di liste
   annidata non è leggibile da gmb_val_as_list). */
gmb_val_t gmb_anova_flat(const double* data, const int* sizes, int k)
{
    if (k < 2) return gmb_make_error("ANOVA richiede almeno 2 gruppi");
    double* gmeans = (double*)malloc(sizeof(double) * (size_t)k);
    double* gns = (double*)malloc(sizeof(double) * (size_t)k);
    if (!gmeans || !gns) { free(gmeans); free(gns); return gmb_make_error("ANOVA oom"); }
    double grand_sum = 0, grand_n = 0;
    int off = 0;
    for (int i = 0; i < k; i++)
    {
        int n = sizes[i];
        gns[i] = n;
        double s = 0;
        for (int j = 0; j < n; j++) s += data[off + j];
        gmeans[i] = s / (n > 0 ? n : 1);
        grand_sum += s;
        grand_n += n;
        off += n;
    }
    double grand_mean = grand_n > 0 ? grand_sum / grand_n : 0;
    double ssb = 0, ssw = 0;
    off = 0;
    for (int i = 0; i < k; i++)
    {
        ssb += gns[i] * (gmeans[i] - grand_mean) * (gmeans[i] - grand_mean);
        for (int j = 0; j < sizes[i]; j++)
        {
            double d = data[off + j] - gmeans[i];
            ssw += d * d;
        }
        off += sizes[i];
    }
    int dfb = k - 1, dfw = (int)grand_n - k;
    double f = dfw > 0 ? (ssb / dfb) / (ssw / dfw) : 0;
    /* p = 1 − F_cdf(f, dfb, dfw) via beta incompleta */
    double x = dfb * f / (dfb * f + dfw);
    double p = 1.0 - gmb_incbeta(0.5 * dfb, 0.5 * dfw, x);
    free(gmeans); free(gns);
    /* Superset dei due contratti (STATS.ANOVA e STATS.Anova sono lo stesso identificatore):
     * HypothesisResult (Statistic/PValue/DegreesOfFreedom) + AnovaResult (FValue/DF.../MS...). */
    double msb = dfb > 0 ? ssb / dfb : 0;
    double msw = dfw > 0 ? ssw / dfw : 0;
    return gmb_make_dict(10,
        "Statistic", gmb_make_val_double(f),
        "PValue", gmb_make_val_double(p),
        "DegreesOfFreedom", gmb_make_val_double(dfw),
        "FValue", gmb_make_val_double(f),
        "DFBetween", gmb_make_val_double(dfb),
        "DFWithin", gmb_make_val_double(dfw),
        "SSBetween", gmb_make_val_double(ssb),
        "SSWithin", gmb_make_val_double(ssw),
        "MSBetween", gmb_make_val_double(msb),
        "MSWithin", gmb_make_val_double(msw));
}

/* STATS.ANOVA(groups) con matrice jagged (gmb_double_t* rows[k]): appiattisce le righe
 * (usando sizes[]) e delega a gmb_anova_flat — C99 emitter, parità PCodeVM. */
gmb_val_t gmb_anova_jagged(const double* const* rows, const int* sizes, int k)
{
    if (!rows || !sizes || k <= 0) return gmb_make_error("ANOVA: lunghezze riga non note (matrice jagged dinamica non supportata)");
    if (k < 2) return gmb_make_error("ANOVA richiede almeno 2 gruppi");
    int total = 0;
    for (int i = 0; i < k; i++) total += sizes[i] > 0 ? sizes[i] : 0;
    if (total <= 0) return gmb_make_error("ANOVA: gruppi vuoti");
    double* data = (double*)malloc((size_t)total * sizeof(double));
    if (!data) return gmb_make_error("ANOVA oom");
    int pos = 0;
    for (int i = 0; i < k; i++)
        for (int j = 0; j < sizes[i]; j++)
            data[pos++] = rows[i][j];
    gmb_val_t r = gmb_anova_flat(data, sizes, k);
    free(data);
    return r;
}

gmb_val_t gmb_kruskal_flat(const double* data, const int* sizes, int k)
{
    if (!data || !sizes || k < 2) return gmb_make_error("KRUSKAL richiede almeno 2 gruppi");
    int total = 0;
    for (int i = 0; i < k; i++) total += sizes[i] > 0 ? sizes[i] : 0;
    if (total <= 0) return gmb_make_error("KRUSKAL: gruppi vuoti");
    double* copy = (double*)malloc((size_t)total * sizeof(double));
    double* ranks = (double*)malloc((size_t)total * sizeof(double));
    if (!copy || !ranks) { free(copy); free(ranks); return gmb_make_error("KRUSKAL oom"); }
    for (int i = 0; i < total; i++) copy[i] = data[i];
    gmb_stats_rank_sum(copy, total, ranks);
    double h = 0;
    int off = 0;
    for (int g = 0; g < k; g++) {
        double rsum = 0;
        for (int i = 0; i < sizes[g]; i++) rsum += ranks[off++];
        if (sizes[g] > 0) h += rsum * rsum / sizes[g];
    }
    h = 12.0 / (total * (double)(total + 1)) * h - 3.0 * (double)(total + 1);
    double df = (double)(k - 1);
    double p = 1.0 - gmb_dist_chi2_cdf(df, h);
    free(copy); free(ranks);
    return gmb_make_dict(3, "Statistic", gmb_make_val_double(h), "PValue", gmb_make_val_double(p), "DegreesOfFreedom", gmb_make_val_double(df));
}
gmb_val_t gmb_kruskal_jagged(const double* const* rows, const int* sizes, int k)
{
    if (!rows || !sizes || k < 2) return gmb_make_error("KRUSKAL richiede almeno 2 gruppi");
    int total = 0;
    for (int i = 0; i < k; i++) total += sizes[i] > 0 ? sizes[i] : 0;
    if (total <= 0) return gmb_make_error("KRUSKAL: gruppi vuoti");
    double* data = (double*)malloc((size_t)total * sizeof(double));
    if (!data) return gmb_make_error("KRUSKAL oom");
    int pos = 0;
    for (int i = 0; i < k; i++)
        for (int j = 0; j < sizes[i]; j++)
            data[pos++] = rows[i][j];
    gmb_val_t r = gmb_kruskal_flat(data, sizes, k);
    free(data);
    return r;
}
gmb_val_t gmb_kruskal(gmb_val_t groups_val)
{
    gmb_list_t groups = gmb_val_as_list(groups_val);
    int k = gmb_list_count(groups);
    if (k < 2) return gmb_make_error("KRUSKAL richiede almeno 2 gruppi");
    int total = 0;
    int* sizes = (int*)malloc(sizeof(int) * (size_t)k);
    if (!sizes) return gmb_make_error("KRUSKAL oom");
    for (int i = 0; i < k; i++) {
        gmb_list_t g = gmb_val_as_list(gmb_list_get(groups, i));
        sizes[i] = gmb_list_count(g);
        total += sizes[i];
    }
    double* data = (double*)malloc(sizeof(double) * (size_t)(total ? total : 1));
    if (!data) { free(sizes); return gmb_make_error("KRUSKAL oom"); }
    int pos = 0;
    for (int i = 0; i < k; i++) {
        gmb_list_t g = gmb_val_as_list(gmb_list_get(groups, i));
        int n = gmb_list_count(g);
        for (int j = 0; j < n; j++) data[pos++] = gmb_val_to_double(gmb_list_get(g, j));
    }
    gmb_val_t r = gmb_kruskal_flat(data, sizes, k);
    free(data); free(sizes);
    return r;
}

/* STATS.Anova da array PIATTO (data = gruppi concatenati, sizes[i] per gruppo):
 * dict ESTESO STATS.AnovaResult (canone IlRuntime.Anova), come gmb_anova_result ma
 * da puntatori (emissione emitter C# su matrice jagged). gmb_anova_flat è il dict
 * HypothesisResult di STATS.ANOVA. */
gmb_val_t gmb_anova_result_flat(const double* data, const int* sizes, int k)
{
    if (k < 2) return gmb_make_error("ANOVA richiede almeno 2 gruppi");
    double* gmeans = (double*)malloc(sizeof(double) * (size_t)k);
    double* gns = (double*)malloc(sizeof(double) * (size_t)k);
    if (!gmeans || !gns) { free(gmeans); free(gns); return gmb_make_error("ANOVA oom"); }
    double grand_sum = 0, grand_n = 0;
    int off = 0;
    for (int i = 0; i < k; i++)
    {
        int n = sizes[i];
        gns[i] = n;
        double s = 0;
        for (int j = 0; j < n; j++) s += data[off + j];
        gmeans[i] = s / (n > 0 ? n : 1);
        grand_sum += s;
        grand_n += n;
        off += n;
    }
    if (grand_n <= (double)k) { free(gmeans); free(gns); return gmb_make_error("ANOVA: gradi di libertà within <= 0"); }
    double grand_mean = grand_sum / grand_n;
    double ssb = 0, ssw = 0;
    off = 0;
    for (int i = 0; i < k; i++)
    {
        ssb += gns[i] * (gmeans[i] - grand_mean) * (gmeans[i] - grand_mean);
        for (int j = 0; j < sizes[i]; j++)
        {
            double d = data[off + j] - gmeans[i];
            ssw += d * d;
        }
        off += sizes[i];
    }
    double dfb = (double)(k - 1), dfw = grand_n - (double)k;
    double msb = ssb / dfb, msw = ssw / dfw;
    double fv = (msw == 0) ? INFINITY : msb / msw;
    double x = dfb * fv / (dfb * fv + dfw);
    double pv = 1.0 - gmb_incbeta(0.5 * dfb, 0.5 * dfw, x);
    free(gmeans); free(gns);
    return gmb_make_dict(10,
        "FValue", gmb_make_val_double(fv),
        "PValue", gmb_make_val_double(pv),
        "DFBetween", gmb_make_val_double(dfb),
        "DFWithin", gmb_make_val_double(dfw),
        "SSBetween", gmb_make_val_double(ssb),
        "SSWithin", gmb_make_val_double(ssw),
        "MSBetween", gmb_make_val_double(msb),
        "MSWithin", gmb_make_val_double(msw),
        "Statistic", gmb_make_val_double(fv),
        "DegreesOfFreedom", gmb_make_val_double(dfw));
}

gmb_val_t gmb_anova_result_jagged(const double* const* rows, const int* sizes, int k)
{
    if (!rows || !sizes || k <= 0) return gmb_make_error("ANOVA: lunghezze riga non note (matrice jagged dinamica non supportata)");
    if (k < 2) return gmb_make_error("ANOVA richiede almeno 2 gruppi");
    int total = 0;
    for (int i = 0; i < k; i++) total += sizes[i] > 0 ? sizes[i] : 0;
    if (total <= 0) return gmb_make_error("ANOVA: gruppi vuoti");
    double* data = (double*)malloc((size_t)total * sizeof(double));
    if (!data) return gmb_make_error("ANOVA oom");
    int pos = 0;
    for (int i = 0; i < k; i++)
        for (int j = 0; j < sizes[i]; j++)
            data[pos++] = rows[i][j];
    gmb_val_t r = gmb_anova_result_flat(data, sizes, k);
    free(data);
    return r;
}

/* ANOVA su matrice RETTANGOLARE flat (C99Native DIM g(k-1, ncol-1)): i gruppi sono le
 * righe, tutte di lunghezza ncol. */
gmb_val_t gmb_anova_result_rect(const double* data, int k, int ncol)
{
    if (!data || k < 2 || ncol <= 0) return gmb_make_error("ANOVA: matrice non valida");
    int* sizes = (int*)malloc((size_t)k * sizeof(int));
    if (!sizes) return gmb_make_error("ANOVA oom");
    for (int i = 0; i < k; i++) sizes[i] = ncol;
    gmb_val_t r = gmb_anova_result_flat(data, sizes, k);
    free(sizes);
    return r;
}
