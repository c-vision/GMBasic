/* signal_fir.c — FIR e utilità: firwin (design FIR a finestra), find_peaks,
 * savgol_filter (smoothing Savitzky-Golay) e decimate.
 *
 * Codice spostato dal runtime C di GMBasic (gmb_filters.c) con il solo cambio di
 * prefisso (gmb_ -> gs_) e di tipo dei count (gmb_int_t -> long).
 */
#include "signal_internal.h"
#include "gmbsignal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── firwin ─────────────────────────────────────────────────────────────────── */

static double gs_fir_ideal_lowpass(double fc, double x)
{
    if (x == 0) return 2.0 * fc;
    double arg = 2.0 * M_PI * fc * x;
    return 2.0 * fc * sin(arg) / arg;
}

static void gs_fir_window(double* w, int n, const char* window)
{
    if (window && strcmp(window, "hamming") == 0)
    {
        for (int i = 0; i < n; i++) w[i] = 0.54 - 0.46 * cos(2.0 * M_PI * i / (n - 1));
    }
    else if (window && strcmp(window, "blackman") == 0)
    {
        for (int i = 0; i < n; i++)
            w[i] = 0.42 - 0.5 * cos(2.0 * M_PI * i / (n - 1)) + 0.08 * cos(4.0 * M_PI * i / (n - 1));
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
double* gs_firwin(int numtaps, const double* cutoff, int ncut,
                  const char* btype, const char* window)
{
    if (numtaps < 1 || ncut < 1) return NULL;
    double* taps = (double*)calloc((size_t)numtaps, sizeof(double));
    double* w = (double*)malloc(sizeof(double) * (size_t)numtaps);
    if (!taps || !w) { free(taps); free(w); return NULL; }
    gs_fir_window(w, numtaps, window);
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
            h = (x == 0 ? 1.0 : 0.0) - gs_fir_ideal_lowpass(cutoff[0], x);
        }
        else if (isBP && ncut >= 2)
        {
            h = gs_fir_ideal_lowpass(cutoff[1], x) - gs_fir_ideal_lowpass(cutoff[0], x);
        }
        else if (isBS && ncut >= 2)
        {
            h = (x == 0 ? 1.0 : 0.0)
                - (gs_fir_ideal_lowpass(cutoff[1], x) - gs_fir_ideal_lowpass(cutoff[0], x));
        }
        else
        {
            h = gs_fir_ideal_lowpass(cutoff[0], x);
        }
        taps[n] = h * w[n] + 0.0; /* +0.0 normalizza -0.0 (parita' stampa: "0", non "-0") */
    }
    free(w);
    return taps;
}

/* ── find_peaks ─────────────────────────────────────────────────────────────── */

/* Ritorna array allocato di indici picco; *outn = lunghezza. */
double* gs_find_peaks(const double* x, int n, double distance, int* outn)
{
    *outn = 0;
    if (n < 3) return NULL;
    int* idx = (int*)malloc(sizeof(int) * (size_t)n);
    double* hts = (double*)malloc(sizeof(double) * (size_t)n);
    if (!idx || !hts) { free(idx); free(hts); return NULL; }
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
    if (!order) { free(idx); free(hts); return NULL; }
    for (int i = 0; i < npk; i++) order[i] = i;
    for (int i = 0; i < npk; i++)
        for (int j = i + 1; j < npk; j++)
            if (hts[order[j]] > hts[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    int* kept = (int*)malloc(sizeof(int) * (size_t)npk);
    if (!kept) { free(idx); free(hts); free(order); return NULL; }
    int nk = 0;
    for (int k = 0; k < npk; k++)
    {
        int ok = 1;
        for (int j = 0; j < nk; j++)
            if ((double)abs(kept[j] - idx[order[k]]) < d) { ok = 0; break; }
        if (ok) kept[nk++] = idx[order[k]];
    }
    /* ordina per indice crescente */
    for (int i = 0; i < nk; i++)
        for (int j = i + 1; j < nk; j++)
            if (kept[j] < kept[i]) { int t = kept[i]; kept[i] = kept[j]; kept[j] = t; }
    double* out = nk > 0 ? (double*)malloc(sizeof(double) * (size_t)nk) : NULL;
    if (out) for (int i = 0; i < nk; i++) out[i] = kept[i];
    *outn = nk;
    free(idx); free(hts); free(order); free(kept);
    return out;
}

/* ── savgol_filter ──────────────────────────────────────────────────────────── */

static int gs_savgol_solve(double* a, double* b, int n, double* x);
static void gs_poly_fit(const double* data, int start, int end, int order, double* coeff);
static double gs_poly_eval(const double* coeff, double t, int order);

static int gs_savgol_coeffs(int half, int order, double* coeffs)
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
    /* risolvi ata*b = atb (gauss) */
    double* b = (double*)malloc(sizeof(double) * (size_t)(order + 1));
    if (!b) { free(A); free(ata); free(atb); return -1; }
    int ok = gs_savgol_solve(ata, atb, order + 1, b);
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

static int gs_savgol_solve(double* a, double* b, int n, double* x)
{
    /* eliminazione gaussiana con pivot (a e' n x n flat, b rhs, x soluzione) */
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
        for (int r = col + 1; r < n; r++)
            if (fabs(m[r * (n + 1) + col]) > fabs(m[piv * (n + 1) + col])) piv = r;
        if (fabs(m[piv * (n + 1) + col]) < 1e-14) { free(m); return -1; }
        if (piv != col)
            for (int j = 0; j <= n; j++)
            { double t = m[col * (n + 1) + j]; m[col * (n + 1) + j] = m[piv * (n + 1) + j]; m[piv * (n + 1) + j] = t; }
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

static void gs_poly_fit(const double* data, int start, int end, int order, double* coeff)
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
    if (gs_savgol_solve(ata, atb, order + 1, coeff) != 0)
        for (int i = 0; i <= order; i++) coeff[i] = 0;
    free(ata); free(atb);
}

static double gs_poly_eval(const double* coeff, double t, int order)
{
    double s = 0;
    for (int p = order; p >= 0; p--) s = s * t + coeff[p];
    return s;
}

double* gs_savgol_filter(const double* x, int n, int window, int polyorder, long* outn)
{
    if (window > n) window = n;
    if (window % 2 == 0) window++;
    if (window < 3)
    {
        double* r = (double*)malloc(sizeof(double) * (size_t)n);
        if (!r) { *outn = 0; return NULL; }
        for (int i = 0; i < n; i++) r[i] = x[i];
        *outn = n;
        return r;
    }
    int half = (window - 1) / 2;
    double* coeffs = (double*)malloc(sizeof(double) * (size_t)window);
    if (!coeffs) { *outn = 0; return NULL; }
    if (gs_savgol_coeffs(half, polyorder, coeffs) != 0) { free(coeffs); *outn = 0; return NULL; }
    /* estende i bordi con estrapolazione polinomiale */
    double* ext = (double*)malloc(sizeof(double) * (size_t)(n + 2 * half));
    double* pl = (double*)malloc(sizeof(double) * (size_t)(polyorder + 1));
    double* pr = (double*)malloc(sizeof(double) * (size_t)(polyorder + 1));
    if (!ext || !pl || !pr) { free(coeffs); free(ext); free(pl); free(pr); *outn = 0; return NULL; }
    gs_poly_fit(x, 0, window < n ? window : n, polyorder, pl);
    gs_poly_fit(x, n - window > 0 ? n - window : 0, n, polyorder, pr);
    int wl = window < n ? window : n;
    for (int i = 0; i < half; i++) ext[i] = gs_poly_eval(pl, i - half, polyorder);
    for (int i = 0; i < n; i++) ext[half + i] = x[i];
    for (int i = 0; i < half; i++) ext[half + n + i] = gs_poly_eval(pr, wl + i, polyorder);
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

/* ── decimate ──────────────────────────────────────────────────────────────── */

double* gs_decimate(const double* x, int n, int q, long* outn)
{
    if (q <= 1)
    {
        double* r = (double*)malloc(sizeof(double) * (size_t)n);
        if (!r) { *outn = 0; return NULL; }
        for (int i = 0; i < n; i++) r[i] = x[i];
        *outn = n;
        return r;
    }
    double fc = 0.8 / q;
    int nt = (int)ceil(4.0 / fc) * 2 + 1;
    if (nt > n) nt = n % 2 == 0 ? n - 1 : n;
    if (nt < 3) nt = 3;
    double* taps = gs_firwin(nt, &fc, 1, "lowpass", "hann");
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
