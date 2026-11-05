/* gm_complex.c — aritmetica complessa (riscrittura). */
#include "gm_complex.h"

#include <math.h>

gm_cpx gm_cpx_make(gm_real re, gm_real im)
{
    gm_cpx z;
    z.re = re; z.im = im;
    return z;
}

gm_cpx gm_cpx_add(gm_cpx a, gm_cpx b) { return gm_cpx_make(a.re + b.re, a.im + b.im); }
gm_cpx gm_cpx_sub(gm_cpx a, gm_cpx b) { return gm_cpx_make(a.re - b.re, a.im - b.im); }
gm_cpx gm_cpx_mul(gm_cpx a, gm_cpx b)
{
    return gm_cpx_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}
gm_cpx gm_cpx_div(gm_cpx a, gm_cpx b)
{
    gm_real d = b.re * b.re + b.im * b.im;
    if (d == 0.0) return gm_cpx_make(0.0, 0.0);
    return gm_cpx_make((a.re * b.re + a.im * b.im) / d,
                       (a.im * b.re - a.re * b.im) / d);
}
gm_cpx gm_cpx_neg(gm_cpx a) { return gm_cpx_make(-a.re, -a.im); }
gm_cpx gm_cpx_conj(gm_cpx a) { return gm_cpx_make(a.re, -a.im); }
gm_cpx gm_cpx_scale(gm_cpx a, gm_real s) { return gm_cpx_make(a.re * s, a.im * s); }
gm_real gm_cpx_abs(gm_cpx a) { return hypot(a.re, a.im); }
gm_real gm_cpx_arg(gm_cpx a) { return atan2(a.im, a.re); }
gm_real gm_cpx_norm2(gm_cpx a) { return a.re * a.re + a.im * a.im; }
int gm_cpx_eq(gm_cpx a, gm_cpx b, gm_real tol)
{
    return (gm_near(a.re, b.re, tol) && gm_near(a.im, b.im, tol)) ? 1 : 0;
}

gm_cpx gm_cpx_exp(gm_cpx z)
{
    gm_real e = exp(z.re);
    return gm_cpx_make(e * cos(z.im), e * sin(z.im));
}

gm_cpx gm_cpx_log(gm_cpx z)
{
    return gm_cpx_make(log(gm_cpx_abs(z)), gm_cpx_arg(z));
}

gm_cpx gm_cpx_sqrt(gm_cpx z)
{
    gm_real r = gm_cpx_abs(z);
    gm_real re, im;
    if (r == 0.0) return gm_cpx_make(0.0, 0.0);
    re = sqrt((r + z.re) * 0.5);
    im = sqrt((r - z.re) * 0.5);
    if (z.im < 0.0) im = -im;
    return gm_cpx_make(re, im);
}

gm_cpx gm_cpx_pow(gm_cpx z, gm_cpx w)
{
    if (z.re == 0.0 && z.im == 0.0) return gm_cpx_make(0.0, 0.0);
    return gm_cpx_exp(gm_cpx_mul(w, gm_cpx_log(z)));
}

gm_cpx gm_cpx_sin(gm_cpx z)
{
    return gm_cpx_make(sin(z.re) * cosh(z.im), cos(z.re) * sinh(z.im));
}

gm_cpx gm_cpx_cos(gm_cpx z)
{
    return gm_cpx_make(cos(z.re) * cosh(z.im), -sin(z.re) * sinh(z.im));
}

gm_cpx gm_cpx_tan(gm_cpx z)
{
    return gm_cpx_div(gm_cpx_sin(z), gm_cpx_cos(z));
}

gm_cpx gm_cpx_sinh(gm_cpx z)
{
    return gm_cpx_make(sinh(z.re) * cos(z.im), cosh(z.re) * sin(z.im));
}

gm_cpx gm_cpx_cosh(gm_cpx z)
{
    return gm_cpx_make(cosh(z.re) * cos(z.im), sinh(z.re) * sin(z.im));
}

gm_cpx gm_cpx_tanh(gm_cpx z)
{
    return gm_cpx_div(gm_cpx_sinh(z), gm_cpx_cosh(z));
}

gm_cpx gm_cpx_asin(gm_cpx z)
{
    /* -i log(iz + sqrt(1 - z^2)) */
    gm_cpx iz = gm_cpx_make(-z.im, z.re);
    gm_cpx one = gm_cpx_make(1.0, 0.0);
    gm_cpx s = gm_cpx_sqrt(gm_cpx_sub(one, gm_cpx_mul(z, z)));
    gm_cpx l = gm_cpx_log(gm_cpx_add(iz, s));
    return gm_cpx_make(l.im, -l.re);
}

gm_cpx gm_cpx_acos(gm_cpx z)
{
    gm_cpx a = gm_cpx_asin(z);
    return gm_cpx_make(1.57079632679489661923 - a.re, -a.im);
}

gm_cpx gm_cpx_atan(gm_cpx z)
{
    /* (i/2) log((i+z)/(i-z)) */
    gm_cpx i = gm_cpx_make(0.0, 1.0);
    gm_cpx l = gm_cpx_log(gm_cpx_div(gm_cpx_add(i, z), gm_cpx_sub(i, z)));
    return gm_cpx_make(-0.5 * l.im, 0.5 * l.re);
}

gm_cpx gm_cpx_asinh(gm_cpx z)
{
    gm_cpx s = gm_cpx_sqrt(gm_cpx_add(gm_cpx_mul(z, z), gm_cpx_make(1.0, 0.0)));
    return gm_cpx_log(gm_cpx_add(z, s));
}

gm_cpx gm_cpx_acosh(gm_cpx z)
{
    gm_cpx s = gm_cpx_sqrt(gm_cpx_sub(gm_cpx_mul(z, z), gm_cpx_make(1.0, 0.0)));
    return gm_cpx_log(gm_cpx_add(z, s));
}

gm_cpx gm_cpx_atanh(gm_cpx z)
{
    gm_cpx one = gm_cpx_make(1.0, 0.0);
    return gm_cpx_scale(gm_cpx_log(gm_cpx_div(gm_cpx_add(one, z), gm_cpx_sub(one, z))), 0.5);
}
