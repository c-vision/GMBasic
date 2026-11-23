/* ============================================================================
 * gmi_interp.c — scipy.interpolate splrep/splev (spline cubica not-a-knot).
 * splrep ritorna un array combinato: [n, x0..x_{n-1}, coeff(4(n-1))].
 * ============================================================================ */

#include "gmi_runtime.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int gmi_interp_solve(double* a, double* b, int n, double* x)
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

/* Seconda derivata/2 della spline not-a-knot con Thomas O(n) (port del canone
 * C#/scipy): prima qui c'era un sistema DENSE n×n, che su input grandi (200k punti)
 * richiedeva centinaia di GB. Ritorna 1 se risolto, 0 se x non è strettamente crescente. */
static int gmi_splrep_thomas(const double* y, const double* h, int n, double* c)
{
    for (int i = 0; i < n - 1; i++) if (h[i] == 0.0) return 0;
    int m = n - 2;
    double* b = (double*)calloc((size_t)n, sizeof(double));
    double* lo = (double*)calloc((size_t)m, sizeof(double));
    double* diag = (double*)calloc((size_t)m, sizeof(double));
    double* up = (double*)calloc((size_t)m, sizeof(double));
    double* rhs = (double*)calloc((size_t)m, sizeof(double));
    double* cprime = (double*)calloc((size_t)m, sizeof(double));
    double* rprime = (double*)calloc((size_t)m, sizeof(double));
    double* ci = (double*)calloc((size_t)m, sizeof(double));
    int ok = 1;
    if (!b || !lo || !diag || !up || !rhs || !cprime || !rprime || !ci) { ok = 0; goto done; }

    for (int i = 1; i < n - 1; i++)
        b[i] = 3.0 * ((y[i + 1] - y[i]) / h[i] - (y[i] - y[i - 1]) / h[i - 1]);

    diag[0] = h[0] * (h[0] + h[1]) / h[1] + 2.0 * (h[0] + h[1]);
    up[0] = h[1] - h[0] * h[0] / h[1];
    rhs[0] = b[1];
    for (int i = 1; i < m - 1; i++)
    {
        int k = i + 1;
        lo[i] = h[k - 1];
        diag[i] = 2.0 * (h[k - 1] + h[k]);
        up[i] = h[k];
        rhs[i] = b[k];
    }
    int last = m - 1;
    lo[last] = h[n - 3] - h[n - 2] * h[n - 2] / h[n - 3];
    diag[last] = 2.0 * (h[n - 3] + h[n - 2]) + h[n - 2] * (h[n - 3] + h[n - 2]) / h[n - 3];
    rhs[last] = b[n - 2];

    cprime[0] = up[0] / diag[0];
    rprime[0] = rhs[0] / diag[0];
    for (int i = 1; i < m; i++)
    {
        double denom = diag[i] - lo[i] * cprime[i - 1];
        cprime[i] = up[i] / denom;
        rprime[i] = (rhs[i] - lo[i] * rprime[i - 1]) / denom;
    }
    ci[m - 1] = rprime[m - 1];
    for (int i = m - 2; i >= 0; i--) ci[i] = rprime[i] - cprime[i] * ci[i + 1];

    for (int i = 0; i < m; i++) c[i + 1] = ci[i];
    c[0] = ((h[0] + h[1]) * c[1] - h[0] * c[2]) / h[1];
    c[n - 1] = ((h[n - 3] + h[n - 2]) * c[n - 2] - h[n - 2] * c[n - 3]) / h[n - 3];
done:
    free(b); free(lo); free(diag); free(up); free(rhs); free(cprime); free(rprime); free(ci);
    return ok;
}

/* Ritorna array combinato allocato; *out_len = 1 + n + 4(n-1). */
double* gmi_splrep(const double* x, const double* y, int n, gmb_int_t* out_len)
{
    if (n < 3) { *out_len = 0; return NULL; }
    double* h = (double*)malloc(sizeof(double) * (size_t)(n - 1));
    if (!h) { *out_len = 0; return NULL; }
    for (int i = 0; i < n - 1; i++) h[i] = x[i + 1] - x[i];
    double* A = NULL;
    double* b = NULL;
    double* c = (double*)calloc((size_t)n, sizeof(double));
    if (!c) { free(h); *out_len = 0; return NULL; }
    if (n >= 4)
    {
        if (!gmi_splrep_thomas(y, h, n, c)) { free(h); free(c); *out_len = 0; return NULL; }
    }
    else
    {
        /* n == 3: il sistema 3×3 denso è più semplice del Thomas e costa nulla. */
        A = (double*)calloc((size_t)n * (size_t)n, sizeof(double));
        b = (double*)calloc((size_t)n, sizeof(double));
        if (!A || !b) { free(h); free(A); free(b); free(c); *out_len = 0; return NULL; }
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
        if (gmi_interp_solve(A, b, n, c) != 0) { free(h); free(A); free(b); free(c); *out_len = 0; return NULL; }
    }
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

/* spline = array combinato da gmi_splrep; valuta su xq. */
double* gmi_splev(const double* spline, int slen, const double* xq, int nq, gmb_int_t* outn)
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

double* gmi_splrep_c99(const double* x, const double* y, int n, gmb_int_t* outn)
{ return gmi_splrep(x, y, n, outn); }
double* gmi_splev_c99(const double* spline, int slen, const double* xq, int nq, gmb_int_t* outn)
{ return gmi_splev(spline, slen, xq, nq, outn); }
