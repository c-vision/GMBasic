/* gs_frac.c — calcolo frazionario (da Libs/differint).
 *
 * - gs_gamma_godfrey  : funzione Gamma di Godfrey (Lanczos, reale/complessa).
 * - gs_frac_gl        : derivata Grünwald-Letnikov di ordine alpha.
 * - gs_frac_gl_coeffs : coefficienti binomiali generalizzati.
 * - gs_frac_rl        : integrale/derivata Riemann-Liouville (trapezoid).
 */
#include "gs_frac.h"
#include <math.h>
#include <complex.h>
#include <stdlib.h>

/* ---- Gamma (Lanczos g=7, n=9, accurate ~1e-15 reale). ---- */
double gs_gamma_godfrey(double z)
{
    static const double c[9] = {
        0.99999999999980993, 676.5203681218851, -1259.1392167224028,
        771.32342877765313, -176.61502916214059, 12.507343278686905,
        -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7
    };
    double g = 7.0, x, t;
    int i;
    if (z < 0.5)
        return M_PI / (sin(M_PI * z) * gs_gamma_godfrey(1.0 - z));
    z -= 1.0;
    x = c[0];
    for (i = 1; i < 9; i++) x += c[i] / (z + (double)i);
    t = z + g + 0.5;
    return sqrt(2.0 * M_PI) * pow(t, z + 0.5) * exp(-t) * x;
}

/* ---- coefficienti GL: b_0=1, b_{k+1} = b_k (alpha-k)/(k+1). ---- */
int gs_frac_gl_coeffs(double alpha, int n, double *b)
{
    int k;
    if (!b || n < 1) return GC_ERR_ARG;
    b[0] = 1.0;
    for (k = 0; k < n - 1; k++)
        b[k + 1] = b[k] * ((double)k - alpha) / (double)(k + 1);
    return GC_OK;
}

/* ---- Grünwald-Letnikov: D^alpha f(x_j) = h^{-alpha} sum_{k=0}^{j} b_k f(x_{j-k}).
 * f: n valori su griglia uniforme, h passo. out: n valori. ---- */
int gs_frac_gl(double alpha, const double *f, int n, double h, double *out)
{
    double *b;
    int i, k;
    if (!f || !out || n < 1 || h <= 0) return GC_ERR_ARG;
    b = (double *)malloc((size_t)n * sizeof(double));
    if (!b) return GC_ERR_NO_MEM;
    gs_frac_gl_coeffs(alpha, n, b);
    for (i = 0; i < n; i++) {
        double s = 0.0;
        for (k = 0; k <= i; k++) s += b[k] * f[i - k];
        out[i] = s * pow(h, -alpha);
    }
    free(b);
    return GC_OK;
}

/* ---- Riemann-Liouville (trapezoid): D^alpha f(x_j) =
 * h^{-alpha}/Gamma(2-alpha) [ w f ] con pesi dalle differenze seconde. ---- */
int gs_frac_rl(double alpha, const double *f, int n, double h, double *out)
{
    int i, j;
    if (!f || !out || n < 1 || h <= 0) return GC_ERR_ARG;
    for (i = 0; i < n; i++) {
        double s = 0.0;
        for (j = 0; j <= i; j++) {
            double w;
            double k = (double)i - (double)j;
            if (j == 0) {
                w = pow((double)(i + 1), 1.0 - alpha) - (double)(i + 1) * pow((double)(i + 1), -alpha);
                /* usa forma standard: w = (k+1)^{1-a} - 2 k^{1-a} + (k-1)^{1-a} */
                w = (k + 1.0 > 0) ? pow(k + 1.0, 1.0 - alpha) : 0.0;
                if (k > 0) w -= 2.0 * pow(k, 1.0 - alpha);
                if (k > 1) w += pow(k - 1.0, 1.0 - alpha);
                else if (k == 1) w += pow(1e-300, 1.0 - alpha);
            } else {
                w = pow(k + 1.0, 1.0 - alpha) - 2.0 * pow(k, 1.0 - alpha);
                if (k > 1) w += pow(k - 1.0, 1.0 - alpha);
            }
            s += w * f[j];
        }
        out[i] = s * pow(h, -alpha) / tgamma(2.0 - alpha);
    }
    return GC_OK;
}
