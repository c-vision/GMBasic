/* gm_veccalc.c — calcolo vettoriale numerico su R^3 (nuovo, gap vector-calculus). */
#include "gm_veccalc.h"

#include "gm_integrate.h"

#include <math.h>

gm_real gm_vc_divergence(gm_vc_field F, void *ctx,
                         gm_real x, gm_real y, gm_real z, gm_real h)
{
    gm_vec3 fx1 = F(x + h, y, z, ctx);
    gm_vec3 fx0 = F(x - h, y, z, ctx);
    gm_vec3 fy1 = F(x, y + h, z, ctx);
    gm_vec3 fy0 = F(x, y - h, z, ctx);
    gm_vec3 fz1 = F(x, y, z + h, ctx);
    gm_vec3 fz0 = F(x, y, z - h, ctx);
    return (fx1.x - fx0.x + fy1.y - fy0.y + fz1.z - fz0.z) / (2.0 * h);
}

gm_vec3 gm_vc_curl(gm_vc_field F, void *ctx,
                   gm_real x, gm_real y, gm_real z, gm_real h)
{
    gm_vec3 fx1 = F(x + h, y, z, ctx);
    gm_vec3 fx0 = F(x - h, y, z, ctx);
    gm_vec3 fy1 = F(x, y + h, z, ctx);
    gm_vec3 fy0 = F(x, y - h, z, ctx);
    gm_vec3 fz1 = F(x, y, z + h, ctx);
    gm_vec3 fz0 = F(x, y, z - h, ctx);
    gm_vec3 c;
    c.x = ((fy1.z - fy0.z) - (fz1.y - fz0.y)) / (2.0 * h);
    c.y = ((fz1.x - fz0.x) - (fx1.z - fx0.z)) / (2.0 * h);
    c.z = ((fx1.y - fx0.y) - (fy1.x - fy0.x)) / (2.0 * h);
    return c;
}

gm_vec3 gm_vc_gradient(gm_vc_scalar f, void *ctx,
                       gm_real x, gm_real y, gm_real z, gm_real h)
{
    gm_vec3 g;
    g.x = (f(x + h, y, z, ctx) - f(x - h, y, z, ctx)) / (2.0 * h);
    g.y = (f(x, y + h, z, ctx) - f(x, y - h, z, ctx)) / (2.0 * h);
    g.z = (f(x, y, z + h, ctx) - f(x, y, z - h, ctx)) / (2.0 * h);
    return g;
}

gm_real gm_vc_laplacian(gm_vc_scalar f, void *ctx,
                        gm_real x, gm_real y, gm_real z, gm_real h)
{
    gm_real c = f(x, y, z, ctx);
    gm_real dxx = (f(x + h, y, z, ctx) - 2.0 * c + f(x - h, y, z, ctx)) / (h * h);
    gm_real dyy = (f(x, y + h, z, ctx) - 2.0 * c + f(x, y - h, z, ctx)) / (h * h);
    gm_real dzz = (f(x, y, z + h, ctx) - 2.0 * c + f(x, y, z - h, ctx)) / (h * h);
    return dxx + dyy + dzz;
}

int gm_vc_is_solenoidal(gm_vc_field F, void *ctx,
                        gm_real x, gm_real y, gm_real z, gm_real h, gm_real tol)
{
    return fabs(gm_vc_divergence(F, ctx, x, y, z, h)) <= tol ? 1 : 0;
}

int gm_vc_is_irrotational(gm_vc_field F, void *ctx,
                          gm_real x, gm_real y, gm_real z, gm_real h, gm_real tol)
{
    gm_vec3 c = gm_vc_curl(F, ctx, x, y, z, h);
    return gm_vec3_length(c) <= tol ? 1 : 0;
}

/* Derivate prima/seconda/terza di una curva per differenze centrali.
 * Il passo cresce con l'ordine (h, 10h, 100h): l'errore di arrotondamento
 * scala come eps/h^k e con un passo unico la terza derivata affogherebbe.
 * La terza usa Richardson sui due stencil a H e H/2 (errore O(H^4)). */
static gm_vec3 vc_rp(gm_vc_curve r, void *ctx, gm_real t, gm_real h)
{
    return gm_vec3_scale(gm_vec3_sub(r(t + h, ctx), r(t - h, ctx)), 1.0 / (2.0 * h));
}

static gm_vec3 vc_rpp(gm_vc_curve r, void *ctx, gm_real t, gm_real h)
{
    gm_real h2 = 10.0 * h;
    gm_vec3 d = gm_vec3_sub(r(t + h2, ctx), gm_vec3_scale(r(t, ctx), 2.0));
    d = gm_vec3_add(d, r(t - h2, ctx));
    return gm_vec3_scale(d, 1.0 / (h2 * h2));
}

static gm_vec3 vc_stencil3(gm_vc_curve r, void *ctx, gm_real t, gm_real H)
{
    gm_vec3 d = gm_vec3_sub(r(t + 2.0 * H, ctx), gm_vec3_scale(r(t + H, ctx), 2.0));
    d = gm_vec3_add(d, gm_vec3_scale(r(t - H, ctx), 2.0));
    d = gm_vec3_sub(d, r(t - 2.0 * H, ctx));
    return gm_vec3_scale(d, 1.0 / (2.0 * H * H * H));
}

static gm_vec3 vc_rppp(gm_vc_curve r, void *ctx, gm_real t, gm_real h)
{
    gm_real H = 100.0 * h;
    gm_vec3 d1 = vc_stencil3(r, ctx, t, H);
    gm_vec3 d2 = vc_stencil3(r, ctx, t, 0.5 * H);
    return gm_vec3_scale(gm_vec3_sub(gm_vec3_scale(d2, 4.0), d1), 1.0 / 3.0);
}

gm_status gm_vc_tnb(gm_vc_curve r, void *ctx, gm_real t, gm_real h,
                    gm_vec3 *T, gm_vec3 *N, gm_vec3 *B,
                    gm_real *kappa, gm_real *tau)
{
    gm_vec3 rp, rpp, rppp, cross, bv, tv, nv;
    gm_real speed, cross_len;
    if (!r || !T || !N || !B || !kappa || !tau || h == 0.0) return GM_ERR_ARG;
    rp = vc_rp(r, ctx, t, h);
    speed = gm_vec3_length(rp);
    if (speed == 0.0) return GM_ERR_ARG;
    tv = gm_vec3_scale(rp, 1.0 / speed);
    rpp = vc_rpp(r, ctx, t, h);
    rppp = vc_rppp(r, ctx, t, h);
    cross = gm_vec3_cross(rp, rpp);
    cross_len = gm_vec3_length(cross);
    *T = tv;
    if (cross_len <= 1e-9 * speed * speed * speed) {
        /* Curva rettilinea in t (kappa <= 1e-9, sotto il rumore delle
         * differenze finite): curvatura e torsione nulle, frame non definito. */
        *N = gm_vec3_zero();
        *B = gm_vec3_zero();
        *kappa = 0.0;
        *tau = 0.0;
        return GM_OK;
    }
    bv = gm_vec3_scale(cross, 1.0 / cross_len);
    nv = gm_vec3_cross(bv, tv);
    *B = bv;
    *N = nv;
    *kappa = cross_len / (speed * speed * speed);
    *tau = gm_vec3_dot(cross, rppp) / (cross_len * cross_len);
    return GM_OK;
}

/* Contesto condiviso per gli integrandi 1D su curva. */
typedef struct {
    gm_vc_curve r;
    void *rctx;
    gm_vc_field F;
    gm_vc_scalar f;
    void *fctx;
    gm_real h;
} vc_line_ctx;

static gm_real vc_speed(gm_real t, void *ctx)
{
    vc_line_ctx *c = (vc_line_ctx *)ctx;
    return gm_vec3_length(vc_rp(c->r, c->rctx, t, c->h));
}

static gm_real vc_scalar_ds(gm_real t, void *ctx)
{
    vc_line_ctx *c = (vc_line_ctx *)ctx;
    gm_vec3 p = c->r(t, c->rctx);
    return c->f(p.x, p.y, p.z, c->fctx) * vc_speed(t, ctx);
}

static gm_real vc_work_dt(gm_real t, void *ctx)
{
    vc_line_ctx *c = (vc_line_ctx *)ctx;
    gm_vec3 p = c->r(t, c->rctx);
    gm_vec3 Fv = c->F(p.x, p.y, p.z, c->fctx);
    return gm_vec3_dot(Fv, vc_rp(c->r, c->rctx, t, c->h));
}

gm_real gm_vc_arc_length(gm_vc_curve r, void *ctx,
                         gm_real a, gm_real b, gm_real tol)
{
    vc_line_ctx c;
    c.r = r;
    c.rctx = ctx;
    c.F = NULL;
    c.f = NULL;
    c.fctx = NULL;
    c.h = 1e-6 * (1.0 + fabs((a + b) * 0.5));
    return gm_integrate_adaptive(vc_speed, &c, a, b, tol);
}

gm_real gm_vc_line_scalar(gm_vc_scalar f, void *fctx,
                          gm_vc_curve r, void *rctx,
                          gm_real a, gm_real b, gm_real tol)
{
    vc_line_ctx c;
    c.r = r;
    c.rctx = rctx;
    c.F = NULL;
    c.f = f;
    c.fctx = fctx;
    c.h = 1e-6 * (1.0 + fabs((a + b) * 0.5));
    return gm_integrate_adaptive(vc_scalar_ds, &c, a, b, tol);
}

gm_real gm_vc_line_work(gm_vc_field F, void *fctx,
                        gm_vc_curve r, void *rctx,
                        gm_real a, gm_real b, gm_real tol)
{
    vc_line_ctx c;
    c.r = r;
    c.rctx = rctx;
    c.F = F;
    c.f = NULL;
    c.fctx = fctx;
    c.h = 1e-6 * (1.0 + fabs((a + b) * 0.5));
    return gm_integrate_adaptive(vc_work_dt, &c, a, b, tol);
}

/* Contesto per il flusso: integrale esterno in u, interno in v. */
typedef struct {
    gm_vc_field F;
    void *fctx;
    gm_vc_patch S;
    void *sctx;
    gm_real u;
    gm_real h;
    gm_real v0;
    gm_real v1;
    gm_real tol;
} vc_flux_ctx;

static gm_real vc_flux_dv(gm_real v, void *ctx)
{
    vc_flux_ctx *c = (vc_flux_ctx *)ctx;
    gm_vec3 p = c->S(c->u, v, c->sctx);
    gm_vec3 su = gm_vec3_scale(gm_vec3_sub(c->S(c->u + c->h, v, c->sctx),
                                           c->S(c->u - c->h, v, c->sctx)),
                               1.0 / ( 2.0 * c->h));
    gm_vec3 sv = gm_vec3_scale(gm_vec3_sub(c->S(c->u, v + c->h, c->sctx),
                                           c->S(c->u, v - c->h, c->sctx)),
                               1.0 / ( 2.0 * c->h));
    gm_vec3 Fv = c->F(p.x, p.y, p.z, c->fctx);
    return gm_vec3_dot(Fv, gm_vec3_cross(su, sv));
}

static gm_real vc_flux_du(gm_real u, void *ctx)
{
    vc_flux_ctx *c = (vc_flux_ctx *)ctx;
    c->u = u;
    return gm_integrate_adaptive(vc_flux_dv, ctx, c->v0, c->v1, c->tol);
}

gm_real gm_vc_flux(gm_vc_field F, void *fctx,
                   gm_vc_patch S, void *sctx,
                   gm_real u0, gm_real u1, gm_real v0, gm_real v1, gm_real tol)
{
    vc_flux_ctx c;
    c.F = F;
    c.fctx = fctx;
    c.S = S;
    c.sctx = sctx;
    c.u = u0;
    c.h = 1e-6;
    c.v0 = v0;
    c.v1 = v1;
    c.tol = tol;
    return gm_integrate_adaptive(vc_flux_du, &c, u0, u1, tol);
}
