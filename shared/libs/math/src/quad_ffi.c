/*
 * quad_ffi.c — shim ABI per il namespace BASIC QUAD (quadratura di gmbmath-numerical).
 *
 * gm_quad lavora con callback `double (*)(double, void*)`; l'FFI passa una callback
 * BASIC come `double (*)(double)` (o `double (*)(double,double)` per la variante
 * complement). Il ponte usa un puntatore globale valido durante la chiamata, come
 * nello shim di gametheory: la quadratura è sincrona e a thread singolo.
 *
 * Metodi: Gauss-Kronrod G7-K15 (con stima d'errore), Gauss-Legendre a n nodi, Romberg,
 * tanh-sinh (integrandi con singolarità agli estremi) e la variante "complement" che
 * passa anche la distanza dall'estremo più vicino (per 1/sqrt(x) ecc.).
 *
 * I nodi/pesi di Gauss-Legendre e Gauss-Hermite non richiedono callback e servono per
 * costruire quadrature proprie.
 *
 * Ritorno: 0 = ok, -1 = argomento non valido (dove c'è un out()).
 */
#include "gm_quad.h"

#include <math.h>
#include <stddef.h>

typedef long long i64;

typedef double (*q_f1)(double);
typedef double (*q_f2)(double, double);

static q_f1 g_f1;
static q_f2 g_f2;

static gm_real q_bridge1(gm_real x, void *ctx)
{
    (void)ctx;
    return (gm_real)g_f1((double)x);
}

static gm_real q_bridge2(gm_real x, gm_real d, void *ctx)
{
    (void)ctx;
    return (gm_real)g_f2((double)x, (double)d);
}

/* Gauss-Kronrod G7-K15: out(0) = integrale, out(1) = errore stimato. */
i64 ofx_quad_kronrod(q_f1 f, double a, double b, double *out, i64 out_n)
{
    gm_real err = 0.0, v;
    if (!f || !out || out_n < 2) return -1;
    g_f1 = f;
    v = gm_quad_gauss_kronrod(q_bridge1, NULL, a, b, &err);
    out[0] = (double)v;
    out[1] = (double)err;
    return 0;
}

/* Gauss-Legendre a n nodi (n < 1 -> 1). */
double ofx_quad_gauss(q_f1 f, double a, double b, i64 n)
{
    if (!f) return NAN;
    g_f1 = f;
    if (n < 1) n = 1;
    return (double)gm_quad_gauss_legendre(q_bridge1, NULL, a, b, (int)n);
}

/* Romberg: tol assoluta, maxit livelli. */
double ofx_quad_romberg(q_f1 f, double a, double b, double tol, i64 maxit)
{
    if (!f) return NAN;
    g_f1 = f;
    if (maxit < 1) maxit = 1;
    return (double)gm_quad_romberg(q_bridge1, NULL, a, b, tol, (int)maxit);
}

/* tanh-sinh: robusta con singolarità agli estremi. absacc assoluta, maxeval limite. */
double ofx_quad_tanh_sinh(q_f1 f, double a, double b, double absacc, i64 maxeval)
{
    if (!f) return NAN;
    g_f1 = f;
    if (maxeval < 1) maxeval = 1000;
    return (double)gm_quad_tanh_sinh(q_bridge1, NULL, a, b, absacc, (int)maxeval);
}

/* tanh-sinh "complement": la callback riceve (x, d), d = distanza dall'estremo vicino. */
double ofx_quad_tanh_sinh_c(q_f2 f, double a, double b, double absacc, i64 maxeval)
{
    if (!f) return NAN;
    g_f2 = f;
    if (maxeval < 1) maxeval = 1000;
    return (double)gm_quad_tanh_sinh_c(q_bridge2, NULL, a, b, absacc, (int)maxeval);
}

/* Nodi e pesi di Gauss-Legendre su [-1,1] (n nodi). */
i64 ofx_quad_legendre_nodes(i64 n, double *x, i64 x_n, double *w, i64 w_n)
{
    if (n < 1 || !x || !w || x_n < n || w_n < n) return -1;
    if (gm_gauss_legendre_nodes((int)n, x, w) != GM_OK) return -1;
    return (i64)n;
}

/* Nodi e pesi di Gauss-Hermite (integrale su R con peso e^{-x^2}). */
i64 ofx_quad_hermite_nodes(i64 n, double *x, i64 x_n, double *w, i64 w_n)
{
    if (n < 1 || !x || !w || x_n < n || w_n < n) return -1;
    if (gm_gauss_hermite_nodes((int)n, x, w) != GM_OK) return -1;
    return (i64)n;
}
