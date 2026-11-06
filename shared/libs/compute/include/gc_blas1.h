/*
 * gc_blas1.h — BLAS di livello 1 (da stdlib-js blas-base).
 *
 * Operazioni vettoriali con stride (incx/incy): asum, scal, axpy, dot, nrm2,
 * iamax, rot/swap (reale e complesso interleaved). Varianti float32 e float64.
 * CSROT/ZDROT: rotazione piana di vettori complessi.
 */
#ifndef GMBCOMPUTE_GC_BLAS1_H
#define GMBCOMPUTE_GC_BLAS1_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Riduzioni. */
float  gc_blas_sasum(int n, const float *x, int incx);
double gc_blas_dasum(int n, const double *x, int incx);
float  gc_blas_snrm2(int n, const float *x, int incx);
double gc_blas_dnrm2(int n, const double *x, int incx);
int    gc_blas_isamax(int n, const float *x, int incx);
int    gc_blas_idamax(int n, const double *x, int incx);

/* Scaling. */
void gc_blas_sscal(int n, float alpha, float *x, int incx);
void gc_blas_dscal(int n, double alpha, double *x, int incx);

/* y += alpha x. */
void gc_blas_saxpy(int n, float alpha, const float *x, int incx, float *y, int incy);
void gc_blas_daxpy(int n, double alpha, const double *x, int incx, double *y, int incy);

/* Prodotto scalare. */
float  gc_blas_sdot(int n, const float *x, int incx, const float *y, int incy);
double gc_blas_ddot(int n, const double *x, int incx, const double *y, int incy);

/* Scambio. */
void gc_blas_sswap(int n, float *x, int incx, float *y, int incy);
void gc_blas_dswap(int n, double *x, int incx, double *y, int incy);

/* Rotazione piana reale: [x;y] <- [c s; -s c][x;y]. */
void gc_blas_srot(int n, float *x, int incx, float *y, int incy, float c, float s);
void gc_blas_drot(int n, double *x, int incx, double *y, int incy, double c, double s);

/* Rotazione piana complessa (interleaved re,im): cx/cy come coppie di reali. */
void gc_blas_csrot(int n, float *cx, int incx, float *cy, int incy, float c, float s);
void gc_blas_zdrot(int n, double *cx, int incx, double *cy, int incy, double c, double s);

/* Rotazione di Givens modificata (param: flag, h11, h21, h12, h22). */
void gc_blas_srotm(int n, float *x, int incx, float *y, int incy, const float *param);
void gc_blas_drotm(int n, double *x, int incx, double *y, int incy, const double *param);

/* Prodotto scalare in doppia precisione con accumulo double (SDSDOT). */
float gc_blas_sdsdot(int n, float sb, const float *x, int incx, const float *y, int incy);

/* Scalari complessi. */
typedef struct { float re, im; } gc_cxf;
typedef struct { double re, im; } gc_cxd;

/* BLAS-1 complesse (array interleaved re,im). */
void  gc_blas_caxpy(int n, gc_cxf alpha, const float *x, int incx, float *y, int incy);
void  gc_blas_cscal(int n, gc_cxf alpha, float *x, int incx);
float gc_blas_scasum(int n, const float *x, int incx);
float gc_blas_scnrm2(int n, const float *x, int incx);
void  gc_blas_zaxpy(int n, gc_cxd alpha, const double *x, int incx, double *y, int incy);
void  gc_blas_zscal(int n, gc_cxd alpha, double *x, int incx);
double gc_blas_dzasum(int n, const double *x, int incx);
double gc_blas_dznrm2(int n, const double *x, int incx);

/* Copia e scambio complessi (array interleaved). */
void gc_blas_ccopy(int n, const float *x, int incx, float *y, int incy);
void gc_blas_zcopy(int n, const double *x, int incx, double *y, int incy);
void gc_blas_cswap(int n, float *x, int incx, float *y, int incy);
void gc_blas_zswap(int n, double *x, int incx, double *y, int incy);

/* Copia reale. */
void gc_blas_scopy(int n, const float *x, int incx, float *y, int incy);
void gc_blas_dcopy(int n, const double *x, int incx, double *y, int incy);
/* Scalatura di complessi per uno scalare REALE (CSScal/ZDSCAL). */
void gc_blas_csscal(int n, float alpha, float *x, int incx);
void gc_blas_zdscal(int n, double alpha, double *x, int incx);
/* |re|+|im| di uno scalare complesso (SCABS1/DCABS1). */
float  gc_blas_scabs1(gc_cxf z);
double gc_blas_dcabs1(gc_cxd z);
/* Prodotto scalare single con accumulo double, risultato double (DSDOT). */
double gc_blas_dsdot(int n, const float *x, int incx, const float *y, int incy);
/* Genera una rotazione di Givens (c,s) da (a,b); restituisce r. */
float  gc_blas_srotg(float a, float b, float *c, float *s);
double gc_blas_drotg(double a, double b, double *c, double *s);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_BLAS1_H */
