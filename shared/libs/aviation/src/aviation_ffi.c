/*
 * aviation_ffi.c — shim ABI per il dominio AVIATION del namespace OPTIMIZE.
 * Adatta int64<->int e gli array di servizio; nessuna callback.
 */
#include "gor_aviation.h"
#include "gor_aviation2.h"

#include <stdlib.h>

typedef long long i64;

static int *to_iarr(const i64 *a, i64 n)
{
    i64 i;
    int *r;
    if (n <= 0 || a == NULL) return NULL;
    r = (int *)malloc((size_t)n * sizeof(int));
    if (r == NULL) return NULL;
    for (i = 0; i < n; i++) r[i] = (int)a[i];
    return r;
}

static void out_iarr(const int *a, i64 *out, i64 n)
{
    i64 i;
    for (i = 0; i < n; i++) out[i] = (i64)a[i];
}

/* ── revenue management ─────────────────────────────────────────────────── */

double ofx_av_emsr_b(const double *fares, const double *mean, const double *sd, i64 n,
                     double capacity, double *protection)
{
    if (n <= 0 || fares == NULL || mean == NULL || sd == NULL || protection == NULL) return -1.0;
    return (double)gor_av_emsr_b((int)n, (const gor_real *)fares, (const gor_real *)mean,
                                 (const gor_real *)sd, (gor_real)capacity, (gor_real *)protection);
}

double ofx_av_overbooking(double capacity, double fare, double denied, double show_prob,
                          double show_std, double *bookings)
{
    gor_real b = 0.0;
    double r;
    if (bookings == NULL) return -1.0;
    r = (double)gor_av_overbooking((gor_real)capacity, (gor_real)fare, (gor_real)denied,
                                   (gor_real)show_prob, (gor_real)show_std, &b);
    bookings[0] = (double)b;
    return r;
}

double ofx_av_bid_price(double capacity, double demand, double fare_low, double fare_high)
{
    return (double)gor_av_bid_price((gor_real)capacity, (gor_real)demand,
                                    (gor_real)fare_low, (gor_real)fare_high);
}

/* ── assegnazioni ───────────────────────────────────────────────────────── */

double ofx_av_fleet(const double *profit, const double *capacity, const double *demand,
                    const i64 *avail, i64 nf, i64 nt, i64 *assign)
{
    int *av, *as;
    double r;
    if (nf <= 0 || nt <= 0 || profit == NULL || capacity == NULL || demand == NULL ||
        avail == NULL || assign == NULL) return -1.0;
    av = to_iarr(avail, nt);
    as = (int *)malloc((size_t)nf * sizeof(int));
    if (av == NULL || as == NULL) { free(av); free(as); return -1.0; }
    r = (double)gor_av_fleet_assignment((int)nf, (int)nt, (const gor_real *)profit,
                                        (const gor_real *)capacity, (const gor_real *)demand,
                                        av, as);
    out_iarr(as, assign, nf);
    free(av); free(as);
    return r;
}

double ofx_av_crew_pairing(const i64 *voli, const double *cost, i64 nf, i64 np,
                           i64 *chosen, i64 *n_chosen)
{
    int *v, *c;
    int nc = 0;
    double r;
    if (nf <= 0 || np <= 0 || voli == NULL || cost == NULL || chosen == NULL || n_chosen == NULL)
        return -1.0;
    v = to_iarr(voli, nf * np);
    c = (int *)malloc((size_t)np * sizeof(int));
    if (v == NULL || c == NULL) { free(v); free(c); return -1.0; }
    r = (double)gor_av_crew_pairing((int)nf, (int)np, v, (const gor_real *)cost, c, &nc);
    out_iarr(c, chosen, nc);
    n_chosen[0] = (i64)nc;
    free(v); free(c);
    return r;
}

double ofx_av_tail_assignment(const double *cost, i64 na, i64 nf, i64 *assign)
{
    int *a;
    double r;
    if (na <= 0 || nf <= 0 || cost == NULL || assign == NULL) return -1.0;
    a = (int *)malloc((size_t)na * sizeof(int));
    if (a == NULL) return -1.0;
    r = (double)gor_av_tail_assignment((int)na, (int)nf, (const gor_real *)cost, a);
    out_iarr(a, assign, na);
    free(a);
    return r;
}

double ofx_av_slot(const double *bid, i64 na, i64 ns, i64 *alloc)
{
    int *a;
    double r;
    if (na <= 0 || ns <= 0 || bid == NULL || alloc == NULL) return -1.0;
    a = (int *)malloc((size_t)na * sizeof(int));
    if (a == NULL) return -1.0;
    r = (double)gor_av_slot_allocation((int)na, (int)ns, (const gor_real *)bid, a);
    out_iarr(a, alloc, na);
    free(a);
    return r;
}

double ofx_av_recovery(const double *delay_cost, i64 nf, i64 na, i64 *assign)
{
    int *a;
    double r;
    if (nf <= 0 || na <= 0 || delay_cost == NULL || assign == NULL) return -1.0;
    a = (int *)malloc((size_t)nf * sizeof(int));
    if (a == NULL) return -1.0;
    r = (double)gor_av_recovery((int)nf, (int)na, (const gor_real *)delay_cost, a);
    out_iarr(a, assign, nf);
    free(a);
    return r;
}

i64 ofx_av_uld(const double *items, i64 n, double W, double H, double D, i64 *uld_of)
{
    int *u;
    int r;
    if (n <= 0 || items == NULL || uld_of == NULL) return -1;
    u = (int *)malloc((size_t)n * sizeof(int));
    if (u == NULL) return -1;
    r = gor_av_uld_packing((int)n, (const gor_real *)items, (gor_real)W, (gor_real)H,
                           (gor_real)D, u);
    out_iarr(u, uld_of, n);
    free(u);
    return (i64)r;
}

/* ── aviation avanzato ──────────────────────────────────────────────────── */

i64 ofx_av_aircraft_routing(const double *flights, i64 nf, double min_turn, i64 *route_of)
{
    int *r;
    int k;
    if (nf <= 0 || flights == NULL || route_of == NULL) return -1;
    r = (int *)malloc((size_t)nf * sizeof(int));
    if (r == NULL) return -1;
    k = gor_av_aircraft_routing((int)nf, flights, min_turn, r);
    out_iarr(r, route_of, nf);
    free(r);
    return (i64)k;
}

double ofx_av_crew_resched(const double *cost, const i64 *available, i64 ncrew, i64 nf,
                           i64 *assign)
{
    int *av, *as;
    double r;
    if (ncrew <= 0 || nf <= 0 || cost == NULL || available == NULL || assign == NULL) return -1.0;
    av = to_iarr(available, ncrew);
    as = (int *)malloc((size_t)ncrew * sizeof(int));
    if (av == NULL || as == NULL) { free(av); free(as); return -1.0; }
    r = gor_av_crew_rescheduling((int)ncrew, (int)nf, cost, av, as);
    out_iarr(as, assign, ncrew);
    free(av); free(as);
    return r;
}

double ofx_av_hub(const double *dist, const double *flow, i64 n, i64 p, double alpha, i64 *hubs)
{
    int *h;
    double r;
    if (n <= 0 || p <= 0 || dist == NULL || flow == NULL || hubs == NULL) return -1.0;
    h = (int *)malloc((size_t)p * sizeof(int));
    if (h == NULL) return -1.0;
    r = gor_av_hub_location((int)n, (int)p, dist, flow, alpha, h);
    out_iarr(h, hubs, p);
    free(h);
    return r;
}

double ofx_av_steiner(const double *w, i64 n, const i64 *terminals, i64 nt, i64 *in_tree)
{
    int *t, *tr;
    double r;
    if (n <= 0 || nt <= 0 || w == NULL || terminals == NULL || in_tree == NULL) return -1.0;
    t = to_iarr(terminals, nt);
    tr = (int *)malloc((size_t)(n * n) * sizeof(int));
    if (t == NULL || tr == NULL) { free(t); free(tr); return -1.0; }
    r = gor_av_network_steiner((int)n, w, t, (int)nt, tr);
    out_iarr(tr, in_tree, n * n);
    free(t); free(tr);
    return r;
}

double ofx_av_tail_continuity(const double *cost, i64 na, i64 nf, i64 *assign)
{
    int *a;
    double r;
    if (na <= 0 || nf <= 0 || cost == NULL || assign == NULL) return -1.0;
    a = (int *)malloc((size_t)na * sizeof(int));
    if (a == NULL) return -1.0;
    r = gor_av_tail_continuity((int)na, (int)nf, cost, a);
    out_iarr(a, assign, na);
    free(a);
    return r;
}

double ofx_av_column_gen(const i64 *voli, const double *cost, i64 nf, i64 np,
                         i64 *chosen, i64 *n_chosen, i64 maxiter)
{
    int *v, *c;
    int nc = 0;
    double r;
    if (nf <= 0 || np <= 0 || voli == NULL || cost == NULL || chosen == NULL ||
        n_chosen == NULL) return -1.0;
    v = to_iarr(voli, nf * np);
    c = (int *)malloc((size_t)np * sizeof(int));
    if (v == NULL || c == NULL) { free(v); free(c); return -1.0; }
    r = gor_av_column_generation((int)nf, (int)np, v, cost, c, &nc, (int)maxiter);
    out_iarr(c, chosen, nc);
    n_chosen[0] = (i64)nc;
    free(v); free(c);
    return r;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
double ofx_av_overbooking__ffi(double capacity, double fare, double denied, double show_prob, double show_std, double *bookings, int64_t bookings_ext) { return ofx_av_overbooking(capacity, fare, denied, show_prob, show_std, bookings); }
