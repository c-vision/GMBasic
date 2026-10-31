/* ============================================================================
 * gmb_interp.c — scipy.interpolate splrep/splev (spline cubica not-a-knot).
 * splrep ritorna un array combinato: [n, x0..x_{n-1}, coeff(4(n-1))].
 * ============================================================================ */

#include "gmb_runtime.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int gmb_interp_solve(double* a, double* b, int n, double* x)
{
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

/* Ritorna array combinato allocato; *out_len = 1 + n + 4(n-1). */
double* gmb_splrep(const double* x, const double* y, int n, gmb_int_t* out_len)
{
    if (n < 3) { *out_len = 0; return NULL; }
    double* h = (double*)malloc(sizeof(double) * (size_t)(n - 1));
    if (!h) { *out_len = 0; return NULL; }
    for (int i = 0; i < n - 1; i++) h[i] = x[i + 1] - x[i];
    double* A = (double*)calloc((size_t)n * (size_t)n, sizeof(double));
    double* b = (double*)calloc((size_t)n, sizeof(double));
    if (!A || !b) { free(h); free(A); free(b); *out_len = 0; return NULL; }
    A[0] = h[1];
    A[1] = -(h[0] + h[1]);
    A[2] = h[0];
    for (int i = 1; i < n - 1; i++)
    {
        A[i * n + (i - 1)] = h[i - 1];
        A[i * n + i] = 2 * (h[i - 1] + h[i]);
        A[i * n + (i + 1)] = h[i];
        b[i] = 3 * ((y[i + 1] - y[i]) / h[i] - (y[i] - y[i - 1]) / h[i - 1]);
    }
    A[(n - 1) * n + (n - 3)] = h[n - 2];
    A[(n - 1) * n + (n - 2)] = -(h[n - 3] + h[n - 2]);
    A[(n - 1) * n + (n - 1)] = h[n - 3];
    double* c = (double*)malloc(sizeof(double) * (size_t)n);
    if (!c) { free(h); free(A); free(b); *out_len = 0; return NULL; }
    if (gmb_interp_solve(A, b, n, c) != 0) { free(h); free(A); free(b); free(c); *out_len = 0; return NULL; }
    int len = 1 + n + 4 * (n - 1);
    double* out = (double*)malloc(sizeof(double) * (size_t)len);
    if (!out) { free(h); free(A); free(b); free(c); *out_len = 0; return NULL; }
    out[0] = n;
    for (int i = 0; i < n; i++) out[1 + i] = x[i];
    for (int i = 0; i < n - 1; i++)
    {
        out[1 + n + 4 * i] = y[i];
        out[1 + n + 4 * i + 1] = (y[i + 1] - y[i]) / h[i] - h[i] * (2 * c[i] + c[i + 1]) / 3.0;
        out[1 + n + 4 * i + 2] = c[i];
        out[1 + n + 4 * i + 3] = (c[i + 1] - c[i]) / (3.0 * h[i]);
    }
    *out_len = len;
    free(h); free(A); free(b); free(c);
    return out;
}

/* spline = array combinato da gmb_splrep; valuta su xq. */
double* gmb_splev(const double* spline, int slen, const double* xq, int nq, gmb_int_t* outn)
{
    if (!spline || slen < 1) { *outn = 0; return NULL; }
    int n = (int)spline[0];
    if (slen < 1 + n + 4 * (n - 1)) { *outn = 0; return NULL; }
    const double* xs = spline + 1;
    const double* coeff = spline + 1 + n;
    double* result = (double*)malloc(sizeof(double) * (size_t)nq);
    if (!result) { *outn = 0; return NULL; }
    for (int k = 0; k < nq; k++)
    {
        double t = xq[k];
        if (t <= xs[0]) { result[k] = coeff[0]; continue; }
        if (t >= xs[n - 1]) { result[k] = coeff[4 * (n - 2)]; continue; }
        int i = 0;
        while (i < n - 2 && t > xs[i + 1]) i++;
        double dx = t - xs[i];
        result[k] = coeff[4 * i] + dx * (coeff[4 * i + 1] + dx * (coeff[4 * i + 2] + dx * coeff[4 * i + 3]));
    }
    *outn = nq;
    return result;
}

double* gmb_splrep_c99(const double* x, const double* y, int n, gmb_int_t* outn)
{ return gmb_splrep(x, y, n, outn); }
double* gmb_splev_c99(const double* spline, int slen, const double* xq, int nq, gmb_int_t* outn)
{ return gmb_splev(spline, slen, xq, nq, outn); }
