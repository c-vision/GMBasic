/*
 * complex_ffi.c — shim ABI per il namespace BASIC COMPLEX (numeri complessi di gmbmath-core).
 *
 * Il numero complesso è il TYPE BASIC `Complex { RE, IM AS DOUBLE }`, lo stesso layout
 * della struct C `gm_cpx` (2 double). Viaggia **per valore** (parametro e ritorno), come
 * GeoPoint di GEO: nessun handle, nessuno stato.
 *
 * Casi limite della libreria C, esposti così come sono (documentati nella guida):
 *   DIV per divisore nullo → (0,0)
 *   LOG di (0,0)           → (log 0, 0) = (-inf, 0)
 *   SQRT di (0,0)          → (0,0)
 *   POW con base (0,0)     → (0,0)
 */
#include "gm_complex.h"

#include <math.h>

typedef long long i64;

/* Stesso layout di gm_cpx (2 double). */
typedef struct { double re, im; } c2;

static gm_cpx c_from(c2 a)
{
    gm_cpx r;
    r.re = a.re; r.im = a.im;
    return r;
}

static c2 c_to(gm_cpx z)
{
    c2 r;
    r.re = z.re; r.im = z.im;
    return r;
}

c2 ofx_cpx_make(double re, double im)
{
    return c_to(gm_cpx_make(re, im));
}

c2 ofx_cpx_add(c2 a, c2 b)      { return c_to(gm_cpx_add(c_from(a), c_from(b))); }
c2 ofx_cpx_sub(c2 a, c2 b)      { return c_to(gm_cpx_sub(c_from(a), c_from(b))); }
c2 ofx_cpx_mul(c2 a, c2 b)      { return c_to(gm_cpx_mul(c_from(a), c_from(b))); }
c2 ofx_cpx_div(c2 a, c2 b)      { return c_to(gm_cpx_div(c_from(a), c_from(b))); }
c2 ofx_cpx_neg(c2 a)            { return c_to(gm_cpx_neg(c_from(a))); }
c2 ofx_cpx_conj(c2 a)           { return c_to(gm_cpx_conj(c_from(a))); }
c2 ofx_cpx_scale(c2 a, double s){ return c_to(gm_cpx_scale(c_from(a), s)); }

double ofx_cpx_abs(c2 a)   { return (double)gm_cpx_abs(c_from(a)); }
double ofx_cpx_arg(c2 a)   { return (double)gm_cpx_arg(c_from(a)); }
double ofx_cpx_norm2(c2 a) { return (double)gm_cpx_norm2(c_from(a)); }

/*
 * Uguaglianza con tolleranza RELATIVA fissa: 1e-9 scalata sui moduli.
 *
 * Perché non un parametro `tol`: su VM/C un DOUBLE dopo due struct piccoli passati per
 * valore non arriva al nativo (arriva 0), quindi `EQ(a, b, tol)` divergerebbe dalle altre
 * gambe. Con due soli argomenti la chiamata è identica su IL, PCodeVM, VM/C e C99Native.
 * Chi ha bisogno di una tolleranza propria scrive `COMPLEX.CABS(COMPLEX.SUB(a, b)) < tol`.
 */
i64 ofx_cpx_eq(c2 a, c2 b)
{
    gm_cpx x = c_from(a), y = c_from(b);
    double scale = 1.0 + gm_cpx_abs(x) + gm_cpx_abs(y);
    double tol = 1e-9 * scale;
    return (i64)gm_cpx_eq(x, y, tol);
}

c2 ofx_cpx_exp(c2 z) { return c_to(gm_cpx_exp(c_from(z))); }
c2 ofx_cpx_log(c2 z) { return c_to(gm_cpx_log(c_from(z))); }
c2 ofx_cpx_sqrt(c2 z) { return c_to(gm_cpx_sqrt(c_from(z))); }
c2 ofx_cpx_pow(c2 z, c2 w) { return c_to(gm_cpx_pow(c_from(z), c_from(w))); }

c2 ofx_cpx_sin(c2 z)  { return c_to(gm_cpx_sin(c_from(z))); }
c2 ofx_cpx_cos(c2 z)  { return c_to(gm_cpx_cos(c_from(z))); }
c2 ofx_cpx_tan(c2 z)  { return c_to(gm_cpx_tan(c_from(z))); }
c2 ofx_cpx_sinh(c2 z) { return c_to(gm_cpx_sinh(c_from(z))); }
c2 ofx_cpx_cosh(c2 z) { return c_to(gm_cpx_cosh(c_from(z))); }
c2 ofx_cpx_tanh(c2 z) { return c_to(gm_cpx_tanh(c_from(z))); }
c2 ofx_cpx_asin(c2 z) { return c_to(gm_cpx_asin(c_from(z))); }
c2 ofx_cpx_acos(c2 z) { return c_to(gm_cpx_acos(c_from(z))); }
c2 ofx_cpx_atan(c2 z) { return c_to(gm_cpx_atan(c_from(z))); }
c2 ofx_cpx_asinh(c2 z) { return c_to(gm_cpx_asinh(c_from(z))); }
c2 ofx_cpx_acosh(c2 z) { return c_to(gm_cpx_acosh(c_from(z))); }
c2 ofx_cpx_atanh(c2 z) { return c_to(gm_cpx_atanh(c_from(z))); }
