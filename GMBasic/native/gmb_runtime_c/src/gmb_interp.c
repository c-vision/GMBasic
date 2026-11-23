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
    /* Non modifica a/b (contratto storico): copia e usa il solver unico. */
    double* ac = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    double* bc = (double*)malloc(sizeof(double) * (size_t)n);
    if (!ac || !bc) { free(ac); free(bc); return -1; }
    for (int i = 0; i < n * n; i++) ac[i] = a[i];
    for (int i = 0; i < n; i++) bc[i] = b[i];
    int ok = gmb_solve_lin_tol(n, ac, bc, x, 1e-14);
    free(ac); free(bc);
    return ok ? 0 : -1;
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
