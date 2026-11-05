/* gm_ellip.c — ellittiche/Airy (riscrittura). */
#include "gm_ellip.h"

#include <math.h>

/* Carlson RF via teorema di duplicazione. */
gm_real gm_ellip_rf(gm_real x, gm_real y, gm_real z)
{
    gm_real mu = 0.0, X = 0.0, Y = 0.0, Z = 0.0, E2, E3;
    int i;
    for (i = 0; i < 100; i++) {
        gm_real sx, sy, sz, lx;
        mu = (x + y + z) / 3.0;
        X = 1.0 - x / mu; Y = 1.0 - y / mu; Z = 1.0 - z / mu;
        if (fabs(X) < 1e-15 && fabs(Y) < 1e-15 && fabs(Z) < 1e-15) break;
        sx = sqrt(x); sy = sqrt(y); sz = sqrt(z);
        lx = sx * (sy + sz) + sy * sz;
        x = 0.25 * (x + lx);
        y = 0.25 * (y + lx);
        z = 0.25 * (z + lx);
    }
    E2 = X * Y + Y * Z + Z * X;
    E3 = X * Y * Z;
    return (1.0 - E2 / 10.0 + E3 / 14.0 + E2 * E2 / 24.0 - 3.0 * E2 * E3 / 44.0) / sqrt(mu);
}

gm_real gm_ellip_rd(gm_real x, gm_real y, gm_real z)
{
    gm_real sum = 0.0, fac = 1.0, alamb, ave, delx, dely, delz, ea, eb, ec, ed, ee;
    int i;
    for (i = 0; i < 100; i++) {
        gm_real sqrtx = sqrt(x), sqrty = sqrt(y), sqrtz = sqrt(z);
        alamb = sqrtx * (sqrty + sqrtz) + sqrty * sqrtz;
        sum += fac / (sqrtz * (z + alamb));
        fac *= 0.25;
        x = 0.25 * (x + alamb);
        y = 0.25 * (y + alamb);
        z = 0.25 * (z + alamb);
        if (fabs((sum - fac) / sum) < 1e-15) break;
    }
    ave = 0.2 * (x + y + 3.0 * z);
    delx = (ave - x) / ave; dely = (ave - y) / ave; delz = (ave - z) / ave;
    ea = delx * dely; eb = delz * delz;
    ec = ea - eb; ed = ea - 6.0 * eb; ee = ed + ec + ec;
    return 3.0 * sum + fac * (1.0 + ed * (-3.0 / 14.0 + 9.0 / 88.0 * ed - 4.5 / 26.0 * delz * ee)
           + delz * (ee / 6.0 + delz * (-9.0 / 22.0 + 3.0 / 26.0 * delz))) / (ave * sqrt(ave));
}

gm_real gm_ellip_K(gm_real k) { return gm_ellip_rf(0.0, 1.0 - k * k, 1.0); }
gm_real gm_ellip_E(gm_real k)
{
    return gm_ellip_rf(0.0, 1.0 - k * k, 1.0) - k * k / 3.0 * gm_ellip_rd(0.0, 1.0 - k * k, 1.0);
}
gm_real gm_ellip_F(gm_real phi, gm_real k)
{
    gm_real s = sin(phi), c = cos(phi);
    return s * gm_ellip_rf(c * c, 1.0 - k * k * s * s, 1.0);
}
gm_real gm_ellip_Einc(gm_real phi, gm_real k)
{
    gm_real s = sin(phi), c = cos(phi);
    return s * gm_ellip_rf(c * c, 1.0 - k * k * s * s, 1.0)
         - k * k / 3.0 * s * s * s * gm_ellip_rd(c * c, 1.0 - k * k * s * s, 1.0);
}

/* Jacobi via AGM discendente. */
static void jacobi_agm(gm_real u, gm_real k, gm_real *sn, gm_real *cn, gm_real *dn)
{
    gm_real a[64], b[64], c[64], phi;
    int n = 0, i;
    a[0] = 1.0; b[0] = sqrt(1.0 - k * k); c[0] = k;
    while (fabs(c[n]) > 1e-15 && n < 60) {
        a[n + 1] = 0.5 * (a[n] + b[n]);
        b[n + 1] = sqrt(a[n] * b[n]);
        c[n + 1] = 0.5 * (a[n] - b[n]);
        n++;
    }
    phi = ldexp(a[n], n) * u;
    for (i = n; i > 0; i--) {
        phi = 0.5 * (phi + asin(c[i] / a[i] * sin(phi)));
    }
    if (sn) *sn = sin(phi);
    if (cn) *cn = cos(phi);
    if (dn) *dn = sqrt(1.0 - k * k * sin(phi) * sin(phi));
}

gm_real gm_ellip_am(gm_real u, gm_real k) { gm_real s; jacobi_agm(u, k, &s, NULL, NULL); return asin(s); }
gm_real gm_ellip_sn(gm_real u, gm_real k) { gm_real s; jacobi_agm(u, k, &s, NULL, NULL); return s; }
gm_real gm_ellip_cn(gm_real u, gm_real k) { gm_real c; jacobi_agm(u, k, NULL, &c, NULL); return c; }
gm_real gm_ellip_dn(gm_real u, gm_real k) { gm_real d; jacobi_agm(u, k, NULL, NULL, &d); return d; }

gm_real gm_ellip_nome(gm_real k)
{
    gm_real kp = sqrt(1.0 - k * k);
    return exp(-3.14159265358979323846 * gm_ellip_K(kp) / gm_ellip_K(k));
}

/* Serie di Airy: f,g,f',g'. */
static void airy_series(gm_real x, gm_real *f, gm_real *g, gm_real *fp, gm_real *gp)
{
    gm_real x3 = x * x * x;
    gm_real ak = 1.0, bk = 1.0;   /* termini correnti a_k, b_k */
    gm_real ff = 1.0, gg = x, ffp = 0.0, ggp = 1.0;
    int k;
    for (k = 1; k <= 60; k++) {
        ak *= x3 / ((gm_real)(3 * k - 2) * (3 * k - 1) * (3 * k));
        bk *= x3 / ((gm_real)(3 * k - 1) * (3 * k) * (3 * k + 1));
        ff += ak;
        gg += bk * x;
        if (x != 0.0) {
            ffp += 3.0 * k * ak / x;
        }
        ggp += (3.0 * k + 1.0) * bk;
    }
    *f = ff; *g = gg; *fp = ffp; *gp = ggp;
}

gm_real gm_airy_ai(gm_real x)
{
    gm_real f, g, fp, gp;
    gm_real c1 = 0.35502805388781723926, c2 = 0.25881940379280679841;
    airy_series(x, &f, &g, &fp, &gp);
    return c1 * f - c2 * g;
}

gm_real gm_airy_bi(gm_real x)
{
    gm_real f, g, fp, gp;
    gm_real d1 = 0.61492662744600073515, d2 = 0.44828835735382635791;
    airy_series(x, &f, &g, &fp, &gp);
    return d1 * f + d2 * g;
}

gm_real gm_airy_ai_prime(gm_real x)
{
    gm_real f, g, fp, gp;
    gm_real c1 = 0.35502805388781723926, c2 = 0.25881940379280679841;
    airy_series(x, &f, &g, &fp, &gp);
    return c1 * fp - c2 * gp;
}

gm_real gm_airy_bi_prime(gm_real x)
{
    gm_real f, g, fp, gp;
    gm_real d1 = 0.61492662744600073515, d2 = 0.44828835735382635791;
    airy_series(x, &f, &g, &fp, &gp);
    return d1 * fp + d2 * gp;
}

/* Bessel sferiche via ricorrenza. */
gm_real gm_bessel_sph_j(int n, gm_real x)
{
    gm_real j0, j1, jn;
    int i;
    if (x == 0.0) return (n == 0) ? 1.0 : 0.0;
    j0 = sin(x) / x;
    if (n == 0) return j0;
    j1 = sin(x) / (x * x) - cos(x) / x;
    if (n == 1) return j1;
    jn = j1;
    for (i = 1; i < n; i++) {
        gm_real j2 = (2.0 * i + 1.0) / x * j1 - j0;
        j0 = j1; j1 = j2; jn = j2;
    }
    return jn;
}

gm_real gm_bessel_sph_y(int n, gm_real x)
{
    gm_real y0, y1, yn;
    int i;
    if (x == 0.0) return -INFINITY;
    y0 = -cos(x) / x;
    if (n == 0) return y0;
    y1 = -cos(x) / (x * x) - sin(x) / x;
    if (n == 1) return y1;
    yn = y1;
    for (i = 1; i < n; i++) {
        gm_real y2 = (2.0 * i + 1.0) / x * y1 - y0;
        y0 = y1; y1 = y2; yn = y2;
    }
    return yn;
}
