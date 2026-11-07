/*
 * veccalc_ffi.c — shim ABI per il namespace BASIC VECCALC (calcolo vettoriale di
 * gmbmath-numerical).
 *
 * gm_veccalc lavora con callback di quattro forme:
 *   campo scalare  f(x,y,z) -> double          (callback FFI scalare, arità 3)
 *   campo vettoriale F(x,y,z) -> vec3          (forma 192+3 = 195, ritorno array)
 *   curva r(t) -> vec3                          (forma 192+1 = 193)
 *   patch S(u,v) -> vec3                        (forma 192+2 = 194)
 *
 * Il ponte usa uno stato globale per forma (le chiamate sono sincrone, thread singolo,
 * come gli altri shim con callback).
 *
 * Ritorno: 0 = ok, -1 = argomenti non validi (dove c'è un array di uscita).
 */
#include "gm_veccalc.h"

#include <stddef.h>

typedef long long i64;

typedef double (*vc_scalar_cb)(double x, double y, double z);
typedef double (*vc_field_cb)(double x, double y, double z, double *out);
typedef double (*vc_curve_cb)(double t, double *out);
typedef double (*vc_patch_cb)(double u, double v, double *out);

static vc_scalar_cb g_fs;
static vc_field_cb g_fv;
static vc_curve_cb g_c;
static vc_patch_cb g_p;

static gm_real vc_bridge_scalar(gm_real x, gm_real y, gm_real z, void *ctx)
{
    (void)ctx;
    return g_fs ? (gm_real)g_fs((double)x, (double)y, (double)z) : (gm_real)0.0;
}

static gm_vec3 vc_from(const double *o)
{
    gm_vec3 r;
    r.x = o[0]; r.y = o[1]; r.z = o[2];
    return r;
}

static gm_vec3 vc_bridge_field(gm_real x, gm_real y, gm_real z, void *ctx)
{
    double o[3] = { 0.0, 0.0, 0.0 };
    (void)ctx;
    if (g_fv) (void)g_fv((double)x, (double)y, (double)z, o);
    return vc_from(o);
}

static gm_vec3 vc_bridge_curve(gm_real t, void *ctx)
{
    double o[3] = { 0.0, 0.0, 0.0 };
    (void)ctx;
    if (g_c) (void)g_c((double)t, o);
    return vc_from(o);
}

static gm_vec3 vc_bridge_patch(gm_real u, gm_real v, void *ctx)
{
    double o[3] = { 0.0, 0.0, 0.0 };
    (void)ctx;
    if (g_p) (void)g_p((double)u, (double)v, o);
    return vc_from(o);
}

static void vc_out(gm_vec3 v, double *out)
{
    out[0] = v.x; out[1] = v.y; out[2] = v.z;
}

/* ── campo scalare (gradiente, laplaciano) ────────────────────────────────── */

/* Gradiente per differenze centrali: out_() riceve le 3 derivate parziali. */
i64 ofx_vc_gradient(vc_scalar_cb f, double x, double y, double z, double h,
                    double *out, i64 out_n)
{
    if (!f || !out || out_n < 3 || h == 0.0) return -1;
    g_fs = f;
    vc_out(gm_vc_gradient(vc_bridge_scalar, NULL, x, y, z, h), out);
    g_fs = NULL;
    return 0;
}

/* Laplaciano (stencil a 7 punti). */
double ofx_vc_laplacian(vc_scalar_cb f, double x, double y, double z, double h)
{
    double r;
    if (!f || h == 0.0) return 0.0;
    g_fs = f;
    r = (double)gm_vc_laplacian(vc_bridge_scalar, NULL, x, y, z, h);
    g_fs = NULL;
    return r;
}

/* ── campo vettoriale (divergenza, rotore, solenoidale, irrotazionale) ───── */

double ofx_vc_divergence(vc_field_cb f, double x, double y, double z, double h)
{
    double r;
    if (!f || h == 0.0) return 0.0;
    g_fv = f;
    r = (double)gm_vc_divergence(vc_bridge_field, NULL, x, y, z, h);
    g_fv = NULL;
    return r;
}

/* Rotore: out_() riceve le 3 componenti. */
i64 ofx_vc_curl(vc_field_cb f, double x, double y, double z, double h,
                double *out, i64 out_n)
{
    if (!f || !out || out_n < 3 || h == 0.0) return -1;
    g_fv = f;
    vc_out(gm_vc_curl(vc_bridge_field, NULL, x, y, z, h), out);
    g_fv = NULL;
    return 0;
}

/* 1 se |div F| <= tol. */
i64 ofx_vc_is_solenoidal(vc_field_cb f, double x, double y, double z, double h, double tol)
{
    i64 r;
    if (!f || h == 0.0) return -1;
    g_fv = f;
    r = (i64)gm_vc_is_solenoidal(vc_bridge_field, NULL, x, y, z, h, tol);
    g_fv = NULL;
    return r;
}

/* 1 se |curl F| <= tol. */
i64 ofx_vc_is_irrotational(vc_field_cb f, double x, double y, double z, double h, double tol)
{
    i64 r;
    if (!f || h == 0.0) return -1;
    g_fv = f;
    r = (i64)gm_vc_is_irrotational(vc_bridge_field, NULL, x, y, z, h, tol);
    g_fv = NULL;
    return r;
}

/* ── curva: lunghezza d'arco e triedro di Frenet ─────────────────────────── */

/* Lunghezza d'arco su [a,b] (Simpson adattiva, tol). */
double ofx_vc_arc_length(vc_curve_cb r, double a, double b, double tol)
{
    double v;
    if (!r) return 0.0;
    g_c = r;
    v = (double)gm_vc_arc_length(vc_bridge_curve, NULL, a, b, tol);
    g_c = NULL;
    return v;
}

/* Triedro di Frenet: out_(0..2) = T, out_(3..5) = N, out_(6..8) = B,
 * out_(9) = curvatura kappa, out_(10) = torsione tau. */
i64 ofx_vc_tnb(vc_curve_cb r, double t, double h, double *out, i64 out_n)
{
    gm_vec3 T, N, B;
    gm_real kappa = 0.0, tau = 0.0;
    if (!r || !out || out_n < 11 || h == 0.0) return -1;
    g_c = r;
    if (gm_vc_tnb(vc_bridge_curve, NULL, t, h, &T, &N, &B, &kappa, &tau) != GM_OK)
    {
        g_c = NULL;
        return -1;
    }
    g_c = NULL;
    out[0] = T.x; out[1] = T.y; out[2] = T.z;
    out[3] = N.x; out[4] = N.y; out[5] = N.z;
    out[6] = B.x; out[7] = B.y; out[8] = B.z;
    out[9] = (double)kappa;
    out[10] = (double)tau;
    return 0;
}

/* ── integrali di linea e flusso (due callback) ──────────────────────────── */

/* Integrale di linea di un campo scalare: f(r(t))·|r'(t)| su [a,b]. */
double ofx_vc_line_scalar(vc_scalar_cb f, vc_curve_cb r, double a, double b, double tol)
{
    double v;
    if (!f || !r) return 0.0;
    g_fs = f; g_c = r;
    v = (double)gm_vc_line_scalar(vc_bridge_scalar, NULL, vc_bridge_curve, NULL, a, b, tol);
    g_fs = NULL; g_c = NULL;
    return v;
}

/* Lavoro: integrale di linea di un campo vettoriale F(r(t))·r'(t) su [a,b]. */
double ofx_vc_line_work(vc_field_cb f, vc_curve_cb r, double a, double b, double tol)
{
    double v;
    if (!f || !r) return 0.0;
    g_fv = f; g_c = r;
    v = (double)gm_vc_line_work(vc_bridge_field, NULL, vc_bridge_curve, NULL, a, b, tol);
    g_fv = NULL; g_c = NULL;
    return v;
}

/* Flusso di F attraverso la patch S(u,v): doppio integrale di F(S)·(Su x Sv). */
double ofx_vc_flux(vc_field_cb f, vc_patch_cb s,
                   double u0, double u1, double v0, double v1, double tol)
{
    double v;
    if (!f || !s) return 0.0;
    g_fv = f; g_p = s;
    v = (double)gm_vc_flux(vc_bridge_field, NULL, vc_bridge_patch, NULL, u0, u1, v0, v1, tol);
    g_fv = NULL; g_p = NULL;
    return v;
}
