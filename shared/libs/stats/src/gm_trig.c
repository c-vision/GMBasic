/* gm_trig.c — trigonometria piana/sferica/iperbolica (riscrittura). */
#include "gm_trig.h"

#include <math.h>

gm_real gm_trig_area(gm_real a, gm_real b, gm_real c)
{
    gm_real s = 0.5 * (a + b + c);
    gm_real v = s * (s - a) * (s - b) * (s - c);
    return (v > 0.0) ? sqrt(v) : 0.0;
}

gm_status gm_trig_sas(gm_real a, gm_real gamma, gm_real b, gm_real *c, gm_real *area)
{
    if (c) *c = sqrt(a * a + b * b - 2.0 * a * b * cos(gamma));
    if (area) *area = 0.5 * a * b * sin(gamma);
    return GM_OK;
}

gm_status gm_trig_sss(gm_real a, gm_real b, gm_real c,
                      gm_real *alpha, gm_real *beta, gm_real *gamma)
{
    if (a <= 0.0 || b <= 0.0 || c <= 0.0) return GM_ERR_ARG;
    if (alpha) *alpha = acos((b * b + c * c - a * a) / (2.0 * b * c));
    if (beta) *beta = acos((a * a + c * c - b * b) / (2.0 * a * c));
    if (gamma) *gamma = acos((a * a + b * b - c * c) / (2.0 * a * b));
    return GM_OK;
}

gm_status gm_trig_ssa(gm_real a, gm_real b, gm_real beta, gm_real *alpha)
{
    gm_real s;
    if (a <= 0.0 || b <= 0.0) return GM_ERR_ARG;
    s = a * sin(beta) / b;
    if (s > 1.0 || s < -1.0) return GM_ERR_ARG;
    if (alpha) *alpha = asin(s);
    return GM_OK;
}

gm_real gm_strig_area(gm_real a, gm_real b, gm_real c)
{
    /* formula di L'Huilier */
    gm_real s = 0.5 * (a + b + c);
    gm_real t = tan(0.5 * s) * tan(0.5 * (s - a)) * tan(0.5 * (s - b)) * tan(0.5 * (s - c));
    return (t > 0.0) ? 4.0 * atan(sqrt(t)) : 0.0;
}

gm_status gm_strig_sss(gm_real a, gm_real b, gm_real c,
                       gm_real *A, gm_real *B, gm_real *C)
{
    if (A) *A = acos((cos(a) - cos(b) * cos(c)) / (sin(b) * sin(c)));
    if (B) *B = acos((cos(b) - cos(a) * cos(c)) / (sin(a) * sin(c)));
    if (C) *C = acos((cos(c) - cos(a) * cos(b)) / (sin(a) * sin(b)));
    return GM_OK;
}

gm_status gm_strig_sas(gm_real a, gm_real C, gm_real b, gm_real *c, gm_real *area)
{
    gm_real cc;
    if (c) {
        cc = acos(cos(a) * cos(b) + sin(a) * sin(b) * cos(C));
        *c = cc;
    } else {
        cc = acos(cos(a) * cos(b) + sin(a) * sin(b) * cos(C));
    }
    if (area) {
        gm_real A, B;
        gm_strig_sss(a, b, cc, &A, &B, NULL);
        *area = A + B + C - 3.14159265358979323846;
    }
    return GM_OK;
}

gm_real gm_htrig_area(gm_real a, gm_real b, gm_real c)
{
    /* difetto: pi - (A+B+C); usa la formula con gli angoli */
    gm_real A, B, C;
    if (gm_htrig_sss(a, b, c, &A, &B, &C) != GM_OK) return 0.0;
    return 3.14159265358979323846 - (A + B + C);
}

gm_status gm_htrig_sss(gm_real a, gm_real b, gm_real c,
                       gm_real *A, gm_real *B, gm_real *C)
{
    if (A) *A = acos((cosh(b) * cosh(c) - cosh(a)) / (sinh(b) * sinh(c)));
    if (B) *B = acos((cosh(a) * cosh(c) - cosh(b)) / (sinh(a) * sinh(c)));
    if (C) *C = acos((cosh(a) * cosh(b) - cosh(c)) / (sinh(a) * sinh(b)));
    return GM_OK;
}

gm_status gm_htrig_sas(gm_real a, gm_real C, gm_real b, gm_real *c, gm_real *area)
{
    gm_real cc;
    cc = acosh(cosh(a) * cosh(b) - sinh(a) * sinh(b) * cos(C));
    if (c) *c = cc;
    if (area) {
        gm_real A, B;
        gm_htrig_sss(a, b, cc, &A, &B, NULL);
        *area = 3.14159265358979323846 - (A + B + C);
    }
    return GM_OK;
}
