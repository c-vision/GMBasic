/* gc_blas1.c — BLAS di livello 1 con stride (da stdlib-js blas-base). */
#include "gc_blas1.h"

#include <math.h>

float gc_blas_sasum(int n, const float *x, int incx)
{
    float s = 0.0f;
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return 0.0f;
    for (i = 0, ix = 0; i < n; i++, ix += incx) s += fabsf(x[ix]);
    return s;
}

double gc_blas_dasum(int n, const double *x, int incx)
{
    double s = 0.0;
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return 0.0;
    for (i = 0, ix = 0; i < n; i++, ix += incx) s += fabs(x[ix]);
    return s;
}

float gc_blas_snrm2(int n, const float *x, int incx)
{
    /* scala per evitare overflow (stile BLAS) */
    float scale = 0.0f, ssq = 1.0f;
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return 0.0f;
    for (i = 0, ix = 0; i < n; i++, ix += incx) {
        if (x[ix] != 0.0f) {
            float ax = fabsf(x[ix]);
            if (scale < ax) { float t = scale / ax; ssq = 1.0f + ssq * t * t; scale = ax; }
            else { float t = ax / scale; ssq += t * t; }
        }
    }
    return scale * sqrtf(ssq);
}

double gc_blas_dnrm2(int n, const double *x, int incx)
{
    double scale = 0.0, ssq = 1.0;
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return 0.0;
    for (i = 0, ix = 0; i < n; i++, ix += incx) {
        if (x[ix] != 0.0) {
            double ax = fabs(x[ix]);
            if (scale < ax) { double t = scale / ax; ssq = 1.0 + ssq * t * t; scale = ax; }
            else { double t = ax / scale; ssq += t * t; }
        }
    }
    return scale * sqrt(ssq);
}

int gc_blas_isamax(int n, const float *x, int incx)
{
    int i, ix, best = -1;
    float bv = -1.0f;
    if (n <= 0 || x == NULL || incx <= 0) return -1;
    for (i = 0, ix = 0; i < n; i++, ix += incx) {
        float ax = fabsf(x[ix]);
        if (ax > bv) { bv = ax; best = ix; }
    }
    return best;
}

int gc_blas_idamax(int n, const double *x, int incx)
{
    int i, ix, best = -1;
    double bv = -1.0;
    if (n <= 0 || x == NULL || incx <= 0) return -1;
    for (i = 0, ix = 0; i < n; i++, ix += incx) {
        double ax = fabs(x[ix]);
        if (ax > bv) { bv = ax; best = ix; }
    }
    return best;
}

void gc_blas_sscal(int n, float alpha, float *x, int incx)
{
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0 || alpha == 1.0f) return;
    for (i = 0, ix = 0; i < n; i++, ix += incx) x[ix] *= alpha;
}

void gc_blas_dscal(int n, double alpha, double *x, int incx)
{
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0 || alpha == 1.0) return;
    for (i = 0, ix = 0; i < n; i++, ix += incx) x[ix] *= alpha;
}

void gc_blas_saxpy(int n, float alpha, const float *x, int incx, float *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) y[iy] += alpha * x[ix];
}

void gc_blas_daxpy(int n, double alpha, const double *x, int incx, double *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) y[iy] += alpha * x[ix];
}

float gc_blas_sdot(int n, const float *x, int incx, const float *y, int incy)
{
    float s = 0.0f;
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return 0.0f;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) s += x[ix] * y[iy];
    return s;
}

double gc_blas_ddot(int n, const double *x, int incx, const double *y, int incy)
{
    double s = 0.0;
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return 0.0;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) s += x[ix] * y[iy];
    return s;
}

void gc_blas_sswap(int n, float *x, int incx, float *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) {
        float t = x[ix]; x[ix] = y[iy]; y[iy] = t;
    }
}

void gc_blas_dswap(int n, double *x, int incx, double *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) {
        double t = x[ix]; x[ix] = y[iy]; y[iy] = t;
    }
}

void gc_blas_srot(int n, float *x, int incx, float *y, int incy, float c, float s)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) {
        float t = c * x[ix] + s * y[iy];
        y[iy] = c * y[iy] - s * x[ix];
        x[ix] = t;
    }
}

void gc_blas_drot(int n, double *x, int incx, double *y, int incy, double c, double s)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) {
        double t = c * x[ix] + s * y[iy];
        y[iy] = c * y[iy] - s * x[ix];
        x[ix] = t;
    }
}

void gc_blas_csrot(int n, float *cx, int incx, float *cy, int incy, float c, float s)
{
    /* complesso interleaved (re, im): stride effettivo = incx*2 */
    int i, ix, iy;
    if (n <= 0 || cx == NULL || cy == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx * 2, iy += incy * 2) {
        float t;
        t = c * cx[ix] + s * cy[iy];
        cy[iy] = c * cy[iy] - s * cx[ix];
        cx[ix] = t;
        t = c * cx[ix + 1] + s * cy[iy + 1];
        cy[iy + 1] = c * cy[iy + 1] - s * cx[ix + 1];
        cx[ix + 1] = t;
    }
}

void gc_blas_zdrot(int n, double *cx, int incx, double *cy, int incy, double c, double s)
{
    int i, ix, iy;
    if (n <= 0 || cx == NULL || cy == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx * 2, iy += incy * 2) {
        double t;
        t = c * cx[ix] + s * cy[iy];
        cy[iy] = c * cy[iy] - s * cx[ix];
        cx[ix] = t;
        t = c * cx[ix + 1] + s * cy[iy + 1];
        cy[iy + 1] = c * cy[iy + 1] - s * cx[ix + 1];
        cx[ix + 1] = t;
    }
}

void gc_blas_srotm(int n, float *x, int incx, float *y, int incy, const float *param)
{
    float flag, h11, h12, h21, h22;
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || param == NULL || incx <= 0 || incy <= 0) return;
    flag = param[0];
    if (flag == -2.0f) return;
    if (flag < 0.0f) { h11 = param[1]; h21 = param[2]; h12 = param[3]; h22 = param[4]; }
    else if (flag == 0.0f) { h11 = 1.0f; h21 = param[2]; h12 = param[3]; h22 = 1.0f; }
    else { h11 = param[1]; h21 = -1.0f; h12 = 1.0f; h22 = param[4]; }
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) {
        float w = x[ix], z = y[iy];
        x[ix] = w * h11 + z * h12;
        y[iy] = w * h21 + z * h22;
    }
}

void gc_blas_drotm(int n, double *x, int incx, double *y, int incy, const double *param)
{
    double flag, h11, h12, h21, h22;
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || param == NULL || incx <= 0 || incy <= 0) return;
    flag = param[0];
    if (flag == -2.0) return;
    if (flag < 0.0) { h11 = param[1]; h21 = param[2]; h12 = param[3]; h22 = param[4]; }
    else if (flag == 0.0) { h11 = 1.0; h21 = param[2]; h12 = param[3]; h22 = 1.0; }
    else { h11 = param[1]; h21 = -1.0; h12 = 1.0; h22 = param[4]; }
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) {
        double w = x[ix], z = y[iy];
        x[ix] = w * h11 + z * h12;
        y[iy] = w * h21 + z * h22;
    }
}

float gc_blas_sdsdot(int n, float sb, const float *x, int incx, const float *y, int incy)
{
    double s = (double)sb;
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return sb;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) s += (double)x[ix] * (double)y[iy];
    return (float)s;
}

/* ---- BLAS-1 complesse (interleaved) ---- */

void gc_blas_caxpy(int n, gc_cxf alpha, const float *x, int incx, float *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    if (alpha.re == 0.0f && alpha.im == 0.0f) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx * 2, iy += incy * 2) {
        float xr = x[ix], xi = x[ix + 1];
        y[iy] += alpha.re * xr - alpha.im * xi;
        y[iy + 1] += alpha.re * xi + alpha.im * xr;
    }
}

void gc_blas_cscal(int n, gc_cxf alpha, float *x, int incx)
{
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return;
    if (alpha.re == 1.0f && alpha.im == 0.0f) return;
    for (i = 0, ix = 0; i < n; i++, ix += incx * 2) {
        float xr = x[ix], xi = x[ix + 1];
        x[ix] = alpha.re * xr - alpha.im * xi;
        x[ix + 1] = alpha.re * xi + alpha.im * xr;
    }
}

float gc_blas_scasum(int n, const float *x, int incx)
{
    float s = 0.0f;
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return 0.0f;
    for (i = 0, ix = 0; i < n; i++, ix += incx * 2) s += fabsf(x[ix]) + fabsf(x[ix + 1]);
    return s;
}

float gc_blas_scnrm2(int n, const float *x, int incx)
{
    float scale = 0.0f, ssq = 1.0f;
    int i, ix, c;
    if (n <= 0 || x == NULL || incx <= 0) return 0.0f;
    for (i = 0, ix = 0; i < n; i++, ix += incx * 2)
        for (c = 0; c < 2; c++) {
            float v = x[ix + c];
            if (v != 0.0f) {
                float av = fabsf(v);
                if (scale < av) { float t = scale / av; ssq = 1.0f + ssq * t * t; scale = av; }
                else { float t = av / scale; ssq += t * t; }
            }
        }
    return scale * sqrtf(ssq);
}

void gc_blas_zaxpy(int n, gc_cxd alpha, const double *x, int incx, double *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    if (alpha.re == 0.0 && alpha.im == 0.0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx * 2, iy += incy * 2) {
        double xr = x[ix], xi = x[ix + 1];
        y[iy] += alpha.re * xr - alpha.im * xi;
        y[iy + 1] += alpha.re * xi + alpha.im * xr;
    }
}

void gc_blas_zscal(int n, gc_cxd alpha, double *x, int incx)
{
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return;
    if (alpha.re == 1.0 && alpha.im == 0.0) return;
    for (i = 0, ix = 0; i < n; i++, ix += incx * 2) {
        double xr = x[ix], xi = x[ix + 1];
        x[ix] = alpha.re * xr - alpha.im * xi;
        x[ix + 1] = alpha.re * xi + alpha.im * xr;
    }
}

double gc_blas_dzasum(int n, const double *x, int incx)
{
    double s = 0.0;
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return 0.0;
    for (i = 0, ix = 0; i < n; i++, ix += incx * 2) s += fabs(x[ix]) + fabs(x[ix + 1]);
    return s;
}

double gc_blas_dznrm2(int n, const double *x, int incx)
{
    double scale = 0.0, ssq = 1.0;
    int i, ix, c;
    if (n <= 0 || x == NULL || incx <= 0) return 0.0;
    for (i = 0, ix = 0; i < n; i++, ix += incx * 2)
        for (c = 0; c < 2; c++) {
            double v = x[ix + c];
            if (v != 0.0) {
                double av = fabs(v);
                if (scale < av) { double t = scale / av; ssq = 1.0 + ssq * t * t; scale = av; }
                else { double t = av / scale; ssq += t * t; }
            }
        }
    return scale * sqrt(ssq);
}

void gc_blas_ccopy(int n, const float *x, int incx, float *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx * 2, iy += incy * 2) { y[iy] = x[ix]; y[iy + 1] = x[ix + 1]; }
}

void gc_blas_zcopy(int n, const double *x, int incx, double *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx * 2, iy += incy * 2) { y[iy] = x[ix]; y[iy + 1] = x[ix + 1]; }
}

void gc_blas_cswap(int n, float *x, int incx, float *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx * 2, iy += incy * 2) {
        float t;
        t = x[ix]; x[ix] = y[iy]; y[iy] = t;
        t = x[ix + 1]; x[ix + 1] = y[iy + 1]; y[iy + 1] = t;
    }
}

void gc_blas_zswap(int n, double *x, int incx, double *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx * 2, iy += incy * 2) {
        double t;
        t = x[ix]; x[ix] = y[iy]; y[iy] = t;
        t = x[ix + 1]; x[ix + 1] = y[iy + 1]; y[iy + 1] = t;
    }
}

void gc_blas_scopy(int n, const float *x, int incx, float *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) y[iy] = x[ix];
}

void gc_blas_dcopy(int n, const double *x, int incx, double *y, int incy)
{
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) y[iy] = x[ix];
}

void gc_blas_csscal(int n, float alpha, float *x, int incx)
{
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return;
    for (i = 0, ix = 0; i < n; i++, ix += incx * 2) { x[ix] *= alpha; x[ix + 1] *= alpha; }
}

void gc_blas_zdscal(int n, double alpha, double *x, int incx)
{
    int i, ix;
    if (n <= 0 || x == NULL || incx <= 0) return;
    for (i = 0, ix = 0; i < n; i++, ix += incx * 2) { x[ix] *= alpha; x[ix + 1] *= alpha; }
}

float gc_blas_scabs1(gc_cxf z) { return fabsf(z.re) + fabsf(z.im); }
double gc_blas_dcabs1(gc_cxd z) { return fabs(z.re) + fabs(z.im); }

double gc_blas_dsdot(int n, const float *x, int incx, const float *y, int incy)
{
    double s = 0.0;
    int i, ix, iy;
    if (n <= 0 || x == NULL || y == NULL || incx <= 0 || incy <= 0) return 0.0;
    for (i = 0, ix = 0, iy = 0; i < n; i++, ix += incx, iy += incy) s += (double)x[ix] * (double)y[iy];
    return s;
}

float gc_blas_srotg(float a, float b, float *c, float *s)
{
    float r, z;
    if (b == 0.0f) { *c = 1.0f; *s = 0.0f; r = a; }
    else if (a == 0.0f) { *c = 0.0f; *s = 1.0f; r = b; }
    else {
        float scale = fabsf(a) + fabsf(b);
        float na = a / scale, nb = b / scale;
        r = scale * sqrtf(na * na + nb * nb);
        r = (a > 0.0f) ? r : -r;
        *c = a / r; *s = b / r;
    }
    z = (fabsf(a) > fabsf(b)) ? *s : ((*c != 0.0f) ? 1.0f / *c : 1.0f);
    (void)z;
    return r;
}

double gc_blas_drotg(double a, double b, double *c, double *s)
{
    double r, z;
    if (b == 0.0) { *c = 1.0; *s = 0.0; r = a; }
    else if (a == 0.0) { *c = 0.0; *s = 1.0; r = b; }
    else {
        double scale = fabs(a) + fabs(b);
        double na = a / scale, nb = b / scale;
        r = scale * sqrt(na * na + nb * nb);
        r = (a > 0.0) ? r : -r;
        *c = a / r; *s = b / r;
    }
    z = (fabs(a) > fabs(b)) ? *s : ((*c != 0.0) ? 1.0 / *c : 1.0);
    (void)z;
    return r;
}
