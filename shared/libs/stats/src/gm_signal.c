/*
 * gm_signal.c — elaborazione dei segnali (wrapper del legacy GMBasic).
 */
#include "gm_signal.h"
#include <math.h>
#include <stdlib.h>

extern double *gmi_signal_convolve(const double *, long, const double *, long, long *);
extern double *gmi_signal_correlate(const double *, long, const double *, long, long *);
extern double *gmi_signal_window(long, int, long *);
extern double *gmi_firwin(int, const double *, int, const char *, const char *);
extern double *gmi_find_peaks(const double *, int, double, int *);
extern long gm_signal_butter(int, const double *, int, const char *);
extern long gm_signal_cheby1(int, double, const double *, int, const char *);
extern long gm_signal_cheby2(int, double, const double *, int, const char *);
extern long gm_signal_ellip(int, double, double, const double *, int, const char *);
extern double *gmi_filter_lfilter(long, const double *, int, long *);
extern double *gmi_filter_filtfilt(long, const double *, int, long *);
extern double *gmi_filter_freqz(long, int, long *);
extern double *gmi_cdist_flat(const double *, int, int, const double *, int, int, const char *, long *);
extern double *gmi_pdist_flat(const double *, int, int, const char *, long *);
extern double *gmi_convex_hull_flat(const double *, int, int, long *);
extern double *gmi_savgol_filter(const double *, int, int, int, long *);
extern double *gmi_resample(const double *, int, int, long *);
extern double *gmi_decimate(const double *, int, int, long *);
extern double *gmi_welch(const double *, int, int, double, int *);

gm_real *gm_signal_convolve(const gm_real *x, int nx, const gm_real *k, int nk, int *out_len)
{
    long on = 0; double *r = gmi_signal_convolve(x, nx, k, nk, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_correlate(const gm_real *x, int nx, const gm_real *k, int nk, int *out_len)
{
    long on = 0; double *r = gmi_signal_correlate(x, nx, k, nk, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_window(int n, int kind, int *out_len)
{
    long on = 0; double *r = gmi_signal_window(n, kind, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_firwin(int numtaps, const gm_real *cutoff, int ncut,
                          const char *btype, const char *window, int *out_len)
{
    double *r = gmi_firwin(numtaps, cutoff, ncut, btype, window);
    if (out_len) *out_len = numtaps; return r;
}
int *gm_signal_find_peaks(const gm_real *x, int n, gm_real distance, int *out_len)
{
    int on = 0; double *r = gmi_find_peaks(x, n, distance, &on);
    int *idx; int i;
    if (out_len) *out_len = on;
    if (!r) return NULL;
    idx = (int *)malloc((size_t)on * sizeof(int));
    if (idx) for (i = 0; i < on; i++) idx[i] = (int)r[i];
    free(r);
    return idx;
}
gm_real *gm_signal_lfilter(long h, const gm_real *x, int nx, int *out_len)
{
    long on = 0; double *r = gmi_filter_lfilter(h, x, nx, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_filtfilt(long h, const gm_real *x, int nx, int *out_len)
{
    long on = 0; double *r = gmi_filter_filtfilt(h, x, nx, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_freqz(long h, int npts, int *out_len)
{
    long on = 0; double *r = gmi_filter_freqz(h, npts, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_cdist(const gm_real *x, int nx, int dx, const gm_real *y, int ny, int dy,
                         const char *metric, int *out_len)
{
    long on = 0; double *r = gmi_cdist_flat(x, nx, dx, y, ny, dy, metric, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_pdist(const gm_real *x, int n, int d, const char *metric, int *out_len)
{
    long on = 0; double *r = gmi_pdist_flat(x, n, d, metric, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_convex_hull(const gm_real *pts, int n, int stride, int *out_len)
{
    long on = 0; double *r = gmi_convex_hull_flat(pts, n, stride, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_savgol(const gm_real *x, int n, int window, int polyorder, int *out_len)
{
    long on = 0; double *r = gmi_savgol_filter(x, n, window, polyorder, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_resample(const gm_real *x, int n, int num, int *out_len)
{
    long on = 0; double *r = gmi_resample(x, n, num, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_decimate(const gm_real *x, int n, int q, int *out_len)
{
    long on = 0; double *r = gmi_decimate(x, n, q, &on);
    if (out_len) *out_len = (int)on; return r;
}
gm_real *gm_signal_welch(const gm_real *x, int n, int nperseg, gm_real fs, int *out_len)
{
    int on = 0; double *r = gmi_welch(x, n, nperseg, fs, &on);
    if (out_len) *out_len = on; return r;
}

/* Gramian Angular Field (Wang & Oates 2015, da Moses #171, clean-room). */
static gm_real *gm_signal_gaf(const gm_real *x, int n, int diff, int *out_len)
{
    gm_real *out, *cs, *sn;
    gm_real minv, maxv, range;
    int i, j;
    if (!x || n <= 0) { if (out_len) *out_len = 0; return NULL; }
    out = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    cs = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    sn = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!out || !cs || !sn) { free(out); free(cs); free(sn); if (out_len) *out_len = 0; return NULL; }
    minv = maxv = x[0];
    for (i = 1; i < n; i++) {
        if (x[i] < minv) minv = x[i];
        if (x[i] > maxv) maxv = x[i];
    }
    range = (maxv - minv) < 1e-6 ? 1.0 : (maxv - minv);
    for (i = 0; i < n; i++) {
        gm_real v = ((x[i] - minv) / range) * 2.0 - 1.0;
        if (v > 1.0) v = 1.0; else if (v < -1.0) v = -1.0;
        cs[i] = v;
        sn[i] = sqrt(1.0 - v * v);
    }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            out[i * n + j] = diff ? (cs[j] * sn[i] - cs[i] * sn[j])
                                  : (cs[i] * cs[j] - sn[i] * sn[j]);
    free(cs); free(sn);
    if (out_len) *out_len = n * n;
    return out;
}

gm_real *gm_signal_gasf(const gm_real *x, int n, int *out_len)
{
    return gm_signal_gaf(x, n, 0, out_len);
}

gm_real *gm_signal_gadf(const gm_real *x, int n, int *out_len)
{
    return gm_signal_gaf(x, n, 1, out_len);
}
