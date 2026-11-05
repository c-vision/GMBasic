/* gm_xpr.c — doppio-doppio (riscrittura). */
#include "gm_xpr.h"

#include <math.h>
#include <stdlib.h>

gm_dd gm_dd_make(gm_real hi, gm_real lo) { gm_dd d; d.hi = hi; d.lo = lo; return d; }
gm_dd gm_dd_from_double(gm_real x) { return gm_dd_make(x, 0.0); }
gm_real gm_dd_to_double(gm_dd x) { return x.hi + x.lo; }

/* somma esatta (Knuth) */
static gm_dd two_sum(gm_real a, gm_real b)
{
    gm_real s = a + b;
    gm_real v = s - a;
    gm_real e = (a - (s - v)) + (b - v);
    return gm_dd_make(s, e);
}

/* prodotto esatto (FMA) */
static gm_dd two_prod(gm_real a, gm_real b)
{
    gm_real p = a * b;
    gm_real e = fma(a, b, -p);
    return gm_dd_make(p, e);
}

static gm_dd renormalize(gm_real hi, gm_real lo)
{
    gm_dd s = two_sum(hi, lo);
    return s;
}

gm_dd gm_dd_add(gm_dd a, gm_dd b)
{
    gm_dd s = two_sum(a.hi, b.hi);
    gm_dd t = two_sum(a.lo, b.lo);
    gm_real lo = s.lo + t.hi + t.lo;
    return renormalize(s.hi, lo);
}

gm_dd gm_dd_sub(gm_dd a, gm_dd b)
{
    return gm_dd_add(a, gm_dd_make(-b.hi, -b.lo));
}

gm_dd gm_dd_mul(gm_dd a, gm_dd b)
{
    gm_dd p = two_prod(a.hi, b.hi);
    gm_real lo = p.lo + a.hi * b.lo + a.lo * b.hi + a.lo * b.lo;
    return renormalize(p.hi, lo);
}

gm_dd gm_dd_div(gm_dd a, gm_dd b)
{
    gm_real q1 = a.hi / b.hi;
    gm_dd r = gm_dd_sub(a, gm_dd_mul(gm_dd_from_double(q1), b));
    gm_real q2 = r.hi / b.hi;
    r = gm_dd_sub(r, gm_dd_mul(gm_dd_from_double(q2), b));
    gm_real q3 = r.hi / b.hi;
    return gm_dd_add(gm_dd_from_double(q1), gm_dd_add(gm_dd_from_double(q2), gm_dd_from_double(q3)));
}

gm_dd gm_dd_neg(gm_dd a) { return gm_dd_make(-a.hi, -a.lo); }
gm_dd gm_dd_abs(gm_dd a) { return (a.hi < 0.0) ? gm_dd_neg(a) : a; }

gm_dd gm_dd_sqrt(gm_dd a)
{
    gm_dd x, half;
    int i;
    if (a.hi <= 0.0) return gm_dd_make(0.0, 0.0);
    x = gm_dd_from_double(sqrt(a.hi));
    half = gm_dd_from_double(0.5);
    for (i = 0; i < 5; i++) x = gm_dd_mul(gm_dd_add(x, gm_dd_div(a, x)), half);
    return x;
}

gm_dd gm_dd_exp(gm_dd a)
{
    /* exp(a) = exp(hi) * exp(lo), con Taylor su lo (piccolo) */
    gm_real e = exp(a.hi);
    gm_dd t = gm_dd_from_double(a.lo);
    gm_dd term = gm_dd_from_double(1.0);
    gm_dd sum = gm_dd_from_double(1.0);
    int i;
    for (i = 1; i <= 12; i++) {
        term = gm_dd_div(gm_dd_mul(term, t), gm_dd_from_double((gm_real)i));
        sum = gm_dd_add(sum, term);
    }
    return gm_dd_mul(gm_dd_from_double(e), sum);
}

gm_dd gm_dd_log(gm_dd a)
{
    /* log(a) = log(hi) + log(1 + lo/hi) */
    gm_dd r;
    if (a.hi <= 0.0) return gm_dd_make(-INFINITY, 0.0);
    r = gm_dd_from_double(log(a.hi));
    {
        gm_dd u = gm_dd_div(gm_dd_from_double(a.lo), gm_dd_from_double(a.hi));
        gm_dd term = u, sum = u;
        int i;
        for (i = 2; i <= 16; i++) {
            term = gm_dd_neg(gm_dd_mul(term, u));
            sum = gm_dd_add(sum, gm_dd_div(term, gm_dd_from_double((gm_real)i)));
        }
        r = gm_dd_add(r, sum);
    }
    return r;
}

gm_dd gm_dd_sin(gm_dd a)
{
    gm_dd t = gm_dd_from_double(a.hi);
    gm_dd term = t, sum = t;
    int i;
    (void)a;
    for (i = 1; i <= 10; i++) {
        term = gm_dd_neg(gm_dd_div(gm_dd_mul(term, gm_dd_mul(t, t)),
                                   gm_dd_from_double((gm_real)((2 * i) * (2 * i + 1)))));
        sum = gm_dd_add(sum, term);
    }
    return sum;
}

gm_dd gm_dd_cos(gm_dd a)
{
    gm_dd t = gm_dd_from_double(a.hi);
    gm_dd term = gm_dd_from_double(1.0), sum = gm_dd_from_double(1.0);
    int i;
    (void)a;
    for (i = 1; i <= 10; i++) {
        term = gm_dd_neg(gm_dd_div(gm_dd_mul(term, gm_dd_mul(t, t)),
                                   gm_dd_from_double((gm_real)((2 * i - 1) * (2 * i)))));
        sum = gm_dd_add(sum, term);
    }
    return sum;
}

gm_dd gm_dd_pow(gm_dd a, int n)
{
    gm_dd r = gm_dd_from_double(1.0);
    int m = n < 0 ? -n : n;
    while (m > 0) {
        if (m & 1) r = gm_dd_mul(r, a);
        a = gm_dd_mul(a, a);
        m >>= 1;
    }
    return (n < 0) ? gm_dd_div(gm_dd_from_double(1.0), r) : r;
}

int gm_dd_cmp(gm_dd a, gm_dd b)
{
    if (a.hi < b.hi) return -1;
    if (a.hi > b.hi) return 1;
    if (a.lo < b.lo) return -1;
    if (a.lo > b.lo) return 1;
    return 0;
}

gm_dd gm_dd_parse(const char *s)
{
    /* parsing a doppia precisione: usa strtod per hi, poi il resto */
    char *end;
    gm_real hi = strtod(s, &end);
    gm_real lo = 0.0;
    if (end != s) {
        gm_real rem = strtod(end, NULL);
        lo = rem;
    }
    return gm_dd_make(hi, lo);
}
