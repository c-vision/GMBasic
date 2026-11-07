/* dims_ext.c — test FFI per array/matrici a rango N (S2, 2026-11-06).
 * Il buffer è piatto row-major, le dimensioni arrivano come argomenti LONG
 * (design decision #1: nessun bound implicito nell'ABI). */
#include <stdint.h>

double sum3(const double *a, int64_t d0, int64_t d1, int64_t d2)
{
    double s = 0;
    int64_t n = d0 * d1 * d2;
    for (int64_t i = 0; i < n; i++) s += a[i];
    return s;
}

/* scala in place: verifica il copy-back di una matrice 3-D */
void scale3(double *a, int64_t d0, int64_t d1, int64_t d2, double k)
{
    int64_t n = d0 * d1 * d2;
    for (int64_t i = 0; i < n; i++) a[i] *= k;
}

/* matrice 2-D con dimensioni esplicite */
double sum2(const double *a, int64_t d0, int64_t d1)
{
    double s = 0;
    int64_t n = d0 * d1;
    for (int64_t i = 0; i < n; i++) s += a[i];
    return s;
}

/* rango 4: solo lettura */
double sum4(const double *a, int64_t d0, int64_t d1, int64_t d2, int64_t d3)
{
    double s = 0;
    int64_t n = d0 * d1 * d2 * d3;
    for (int64_t i = 0; i < n; i++) s += a[i];
    return s;
}

/* Prima estensione: valida con qualunque rango (gli argomenti oltre la prima restano ignorati). */
double first_ext(const double *a, int64_t d0)
{
    (void)a;
    return (double)d0;
}
