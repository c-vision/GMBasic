/* gmb_stats_flat.c — PONTE SHARED dei test di ipotesi del runtime condiviso.
 *
 * Nessuna formula qui: i kernel `gmb_ttest*`, `gmb_chi2`, `gmb_fisher`, `gmb_ks`, `gmb_shapiro`
 * ricevono liste `gmb_val_t` e ritornano un dict. Questo file costruisce le liste dai buffer
 * C# e legge le tre chiavi del dict (marshalling, non numerica). */
#include "gmb_stats_flat.h"

/* Buffer → lista del runtime (adotta lo storage). */
static gmb_val_t stf_list(const double* a, int n) {
    gmb_list_t l = gmb_list_new();
    if (!a) n = 0;
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_double(a[i]));
    return gmb_make_val_list_take(&l);
}

/* Dict → tre out-param. 0 se il kernel ha rifiutato l'input (valore non-dict → dict vuoto). */
static int stf_take(gmb_val_t v, double* stat, double* p, double* df) {
    gmb_dict_t d = gmb_val_as_dict(v);
    if (gmb_dict_count(d) == 0) return 0;
    if (stat) *stat = gmb_dict_get_or_nan(d, "Statistic");
    if (p)    *p    = gmb_dict_get_or_nan(d, "PValue");
    if (df)   *df   = gmb_dict_get_or_nan(d, "DegreesOfFreedom");
    /* il dict dei test è un box "temporaneo" (rilascio esplicito dell'helper del runtime) */
    gmb_val_free_temp_dict(v);
    return 1;
}

int gmb_stf_ttest(const double* x, int nx, const double* y, int ny,
                  double* stat, double* p, double* df) {
    gmb_val_t xv = stf_list(x, nx), yv = stf_list(y, ny);
    return stf_take(gmb_ttest(xv, yv), stat, p, df);
}

int gmb_stf_ttest_paired(const double* x, int nx, const double* y, int ny,
                         double* stat, double* p, double* df) {
    gmb_val_t xv = stf_list(x, nx), yv = stf_list(y, ny);
    return stf_take(gmb_ttestpaired(xv, yv), stat, p, df);
}

int gmb_stf_ttest_1samp(const double* x, int nx, double mu,
                        double* stat, double* p, double* df) {
    gmb_val_t xv = stf_list(x, nx);
    return stf_take(gmb_ttestonesample(xv, gmb_make_val_double(mu)), stat, p, df);
}

int gmb_stf_chi2(const double* observed, int nobs, const double* expected, int nexp,
                 double* stat, double* p, double* df) {
    gmb_val_t ov = stf_list(observed, nobs), ev = stf_list(expected, nexp);
    return stf_take(gmb_chi2(ov, ev), stat, p, df);
}

int gmb_stf_fisher(long a, long b, long c, long d,
                   double* stat, double* p, double* df) {
    return stf_take(gmb_fisher((gmb_int_t)a, (gmb_int_t)b, (gmb_int_t)c, (gmb_int_t)d),
                    stat, p, df);
}

int gmb_stf_ks(const double* x, int nx, const double* y, int ny,
               double* stat, double* p, double* df) {
    gmb_val_t xv = stf_list(x, nx), yv = stf_list(y, ny);
    return stf_take(gmb_ks(xv, yv), stat, p, df);
}

int gmb_stf_shapiro(const double* x, int n, double* stat, double* p, double* df) {
    gmb_val_t xv = stf_list(x, n);
    return stf_take(gmb_shapiro(xv), stat, p, df);
}

int gmb_stf_chi2con(const double* table, int rows, int cols, double* stat, double* p, double* df) {
    return stf_take(gmb_chi2con_flat(table, rows, cols), stat, p, df);
}

int gmb_stf_skewtest(const double* x, int n, double* stat, double* p, double* df) {
    gmb_val_t xv = stf_list(x, n);
    return stf_take(gmb_skewtest(xv), stat, p, df);
}

int gmb_stf_kurtosistest(const double* x, int n, double* stat, double* p, double* df) {
    gmb_val_t xv = stf_list(x, n);
    return stf_take(gmb_kurtosistest(xv), stat, p, df);
}

int gmb_stf_jarquebera(const double* x, int n, double* stat, double* p, double* df) {
    return stf_take(gmb_jarquebera_flat(x, n), stat, p, df);
}

/* ── Test inferenziali: stesso dict Statistic/PValue/DegreesOfFreedom ─────────────────────── */

int gmb_stf_pearsonr(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df) {
    return stf_take(gmb_pearsonr_flat(x, nx, y, ny), stat, p, df);
}

int gmb_stf_spearmanr(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df) {
    return stf_take(gmb_spearmanr_flat(x, nx, y, ny), stat, p, df);
}

int gmb_stf_kendalltau(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df) {
    return stf_take(gmb_kendalltau_flat(x, nx, y, ny), stat, p, df);
}

int gmb_stf_ftest(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df) {
    return stf_take(gmb_ftest_flat(x, nx, y, ny), stat, p, df);
}

int gmb_stf_mannwhitney(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df) {
    return stf_take(gmb_mannwhitney_flat(x, nx, y, ny), stat, p, df);
}

int gmb_stf_wilcoxon(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df) {
    return stf_take(gmb_wilcoxon_flat(x, nx, y, ny), stat, p, df);
}

int gmb_stf_kruskal(const double* data, const int* sizes, int k, double* stat, double* p, double* df) {
    return stf_take(gmb_kruskal_flat(data, sizes, k), stat, p, df);
}

int gmb_stf_linregress(const double* x, int nx, const double* y, int ny,
                       double* slope, double* intercept, double* r, double* pvalue, double* stderr) {
    gmb_val_t v = gmb_linregress_flat(x, nx, y, ny);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    if (slope)     *slope     = gmb_dict_get_or_nan(d, "Slope");
    if (intercept) *intercept = gmb_dict_get_or_nan(d, "Intercept");
    if (r)         *r         = gmb_dict_get_or_nan(d, "R");
    if (pvalue)    *pvalue    = gmb_dict_get_or_nan(d, "PValue");
    if (stderr)    *stderr    = gmb_dict_get_or_nan(d, "Stderr");
    gmb_val_free_temp_dict(v);
    return ok;
}

int gmb_stf_describe(const double* a, int n, double* nobs, double* vmin, double* vmax, double* mean,
                     double* variance, double* skewness, double* kurtosis) {
    gmb_val_t v = gmb_describe_flat(a, n);
    gmb_dict_t d = gmb_val_as_dict(v);
    int ok = gmb_dict_count(d) > 0;
    if (nobs)     *nobs     = gmb_dict_get_or_nan(d, "Nobs");
    if (vmin)     *vmin     = gmb_dict_get_or_nan(d, "Min");
    if (vmax)     *vmax     = gmb_dict_get_or_nan(d, "Max");
    if (mean)     *mean     = gmb_dict_get_or_nan(d, "Mean");
    if (variance) *variance = gmb_dict_get_or_nan(d, "Variance");
    if (skewness) *skewness = gmb_dict_get_or_nan(d, "Skewness");
    if (kurtosis) *kurtosis = gmb_dict_get_or_nan(d, "Kurtosis");
    gmb_val_free_temp_dict(v);
    return ok;
}
