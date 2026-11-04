/* gor_aviation.c — dominio airline: RM, fleet/tail assignment, crew, slot, recovery, ULD. */
#include "gor_aviation.h"
#include "gor_assign.h"
#include "gor_cover.h"
#include "gor_pack.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* inversa della normale standard (Acklam). */
static gor_real norm_ppf(gor_real p)
{
    static const gor_real a[6] = {-3.969683028665376e+01, 2.209460984245205e+02,
        -2.759285104469687e+02, 1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00};
    static const gor_real b[5] = {-5.447609879822406e+01, 1.615858368580409e+02,
        -1.556989798598866e+02, 6.680131188771972e+01, -1.328068155288572e+01};
    static const gor_real c[6] = {-7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00};
    static const gor_real d[4] = {7.784695709041462e-03, 3.224671290700398e-01,
        2.445134137142996e+00, 3.754408661907416e+00};
    gor_real q, r;
    if (p <= 0.0) return -1e30;
    if (p >= 1.0) return 1e30;
    if (p < 0.02425) {
        q = sqrt(-2.0 * log(p));
        return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
    if (p > 1.0 - 0.02425) {
        q = sqrt(-2.0 * log(1.0 - p));
        return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
    q = p - 0.5; r = q * q;
    return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q /
           (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1.0);
}

/* ============================== EMSR-b ============================== */

gor_real gor_av_emsr_b(int n, const gor_real *fares, const gor_real *demand_mean,
                       const gor_real *demand_std, gor_real capacity, gor_real *protection)
{
    gor_real cum = 0.0, i;
    if (n <= 0 || fares == NULL || demand_mean == NULL || demand_std == NULL || capacity <= 0) return 0.0;
    for (i = 0; i < n; i++) {
        gor_real y;
        if (i < n - 1) {
            gor_real ratio = fares[(int)i + 1] / fares[(int)i];
            if (ratio >= 1.0) ratio = 0.999999;
            y = demand_mean[(int)i] + norm_ppf(1.0 - ratio) * demand_std[(int)i];
        } else {
            y = demand_mean[(int)i];
        }
        if (y < 0.0) y = 0.0;
        cum += y;
        if (protection) protection[(int)i] = (cum < capacity) ? cum : capacity;
    }
    return (cum < capacity) ? cum : capacity;
}

/* ============================== overbooking ============================== */

gor_real gor_av_overbooking(gor_real capacity, gor_real fare, gor_real denied,
                            gor_real show_prob, gor_real show_std, gor_real *bookings)
{
    gor_real z, lo, hi, mid;
    int it;
    if (capacity <= 0.0 || fare <= 0.0 || denied < 0.0) { if (bookings) *bookings = capacity; return 0.0; }
    if (show_prob <= 0.0) show_prob = 1.0;
    if (show_prob > 1.0) show_prob = 1.0;
    z = norm_ppf(fare / (fare + denied));
    /* risolvi B p + z sqrt(B p (1-p)) = C  (bisection) */
    lo = 0.0; hi = capacity / show_prob + 10.0 * (show_std + 1.0) + capacity;
    for (it = 0; it < 200; it++) {
        gor_real sd;
        mid = 0.5 * (lo + hi);
        sd = sqrt(mid * show_prob * (1.0 - show_prob) + show_std * show_std);
        if (mid * show_prob + z * sd < capacity) lo = mid; else hi = mid;
    }
    mid = 0.5 * (lo + hi);
    if (bookings) *bookings = mid;
    return mid - capacity;
}

/* ============================== bid price ============================== */

gor_real gor_av_bid_price(gor_real capacity, gor_real demand, gor_real fare_low, gor_real fare_high)
{
    if (capacity <= 0.0) return fare_high;
    return (demand >= capacity) ? fare_high : fare_low;
}

/* ============================== fleet assignment ============================== */

gor_real gor_av_fleet_assignment(int nf, int nt, const gor_real *profit, const gor_real *capacity,
                                 const gor_real *demand, const int *avail, int *assign)
{
    int *rem, *used, i, t, k;
    gor_real total = 0.0;
    if (nf <= 0 || nt <= 0 || profit == NULL || capacity == NULL || demand == NULL || assign == NULL) return 0.0;
    rem = (int *)malloc((size_t)nt * sizeof(int));
    used = (int *)calloc((size_t)nf, sizeof(int));
    if (!rem || !used) { free(rem); free(used); return 0.0; }
    for (t = 0; t < nt; t++) rem[t] = avail ? avail[t] : 1;
    /* assegna i voli per profitto decrescente, il tipo più piccolo capiente disponibile */
    for (k = 0; k < nf; k++) {
        int bestf = -1, bestt = -1;
        gor_real bestp = -1e300;
        for (i = 0; i < nf; i++) {
            if (used[i]) continue;
            for (t = 0; t < nt; t++) {
                if (rem[t] <= 0 || capacity[t] < demand[i]) continue;
                if (profit[(size_t)i * (size_t)nt + (size_t)t] > bestp) { bestp = profit[(size_t)i * (size_t)nt + (size_t)t]; bestf = i; bestt = t; }
            }
        }
        if (bestf < 0) { /* nessun tipo capiente: assegna il più grande con profitto */
            for (i = 0; i < nf && bestf < 0; i++) {
                if (used[i]) continue;
                for (t = 0; t < nt; t++) if (rem[t] > 0) { bestf = i; bestt = t; break; }
            }
            if (bestf < 0) break;
            bestp = profit[(size_t)bestf * (size_t)nt + (size_t)bestt];
        }
        assign[bestf] = bestt; used[bestf] = 1; rem[bestt]--; total += bestp;
    }
    for (i = 0; i < nf; i++) if (!used[i]) assign[i] = -1;
    free(rem); free(used);
    return total;
}

/* ============================== crew pairing ============================== */

gor_real gor_av_crew_pairing(int nf, int np, const int *voli_in_pairing,
                             const gor_real *cost, int *chosen, int *n_chosen)
{
    return gor_set_cover_greedy(nf, np, voli_in_pairing, cost, chosen, n_chosen);
}

/* ============================== tail assignment ============================== */

gor_real gor_av_tail_assignment(int na, int nf, const gor_real *cost, int *assign)
{
    if (na <= nf) return gor_assign_hungarian(na, nf, cost, assign);
    else {
        /* più aerei che voli: assegna i voli agli aerei (trasposta) */
        gor_real *t = (gor_real *)malloc((size_t)na * (size_t)nf * sizeof(gor_real));
        int *as = (int *)malloc((size_t)nf * sizeof(int));
        int i, j;
        gor_real total;
        if (!t || !as) { free(t); free(as); return 0.0; }
        for (i = 0; i < nf; i++) for (j = 0; j < na; j++) t[(size_t)i * (size_t)na + (size_t)j] = cost[(size_t)j * (size_t)nf + (size_t)i];
        total = gor_assign_hungarian(nf, na, t, as);
        for (j = 0; j < na; j++) assign[j] = -1;
        for (i = 0; i < nf; i++) if (as[i] >= 0) assign[as[i]] = i;
        free(t); free(as);
        return total;
    }
}

/* ============================== slot allocation ============================== */

gor_real gor_av_slot_allocation(int na, int ns, const gor_real *bid, int *alloc)
{
    gor_real *cost;
    int i, j;
    gor_real total = 0.0;
    if (na <= 0 || ns <= 0 || bid == NULL || alloc == NULL) return 0.0;
    cost = (gor_real *)malloc((size_t)na * (size_t)ns * sizeof(gor_real));
    if (!cost) return 0.0;
    for (i = 0; i < na; i++) for (j = 0; j < ns; j++) cost[(size_t)i * (size_t)ns + (size_t)j] = -bid[(size_t)i * (size_t)ns + (size_t)j];
    if (na <= ns) {
        gor_assign_hungarian(na, ns, cost, alloc);
    } else {
        gor_real *t = (gor_real *)malloc((size_t)ns * (size_t)na * sizeof(gor_real));
        int *as = (int *)malloc((size_t)ns * sizeof(int));
        if (t && as) {
            for (j = 0; j < ns; j++) for (i = 0; i < na; i++) t[(size_t)j * (size_t)na + (size_t)i] = cost[(size_t)i * (size_t)ns + (size_t)j];
            gor_assign_hungarian(ns, na, t, as);
            for (i = 0; i < na; i++) alloc[i] = -1;
            for (j = 0; j < ns; j++) if (as[j] >= 0) alloc[as[j]] = j;
        }
        free(t); free(as);
    }
    total = 0.0;
    for (i = 0; i < na; i++) if (alloc[i] >= 0) total += bid[(size_t)i * (size_t)ns + (size_t)alloc[i]];
    free(cost);
    return total;
}

/* ============================== recovery ============================== */

gor_real gor_av_recovery(int nf, int na, const gor_real *delay_cost, int *assign)
{
    int *used, i, a, k;
    gor_real total = 0.0;
    if (nf <= 0 || na <= 0 || delay_cost == NULL || assign == NULL) return 0.0;
    used = (int *)calloc((size_t)na, sizeof(int));
    if (!used) return 0.0;
    for (k = 0; k < nf; k++) {
        int bestf = -1, besta = -1;
        gor_real bestc = 1e300;
        for (i = 0; i < nf; i++) {
            if (assign[i] >= 0) continue;
            for (a = 0; a < na; a++) {
                if (used[a]) continue;
                if (delay_cost[(size_t)i * (size_t)na + (size_t)a] < bestc) {
                    bestc = delay_cost[(size_t)i * (size_t)na + (size_t)a]; bestf = i; besta = a;
                }
            }
        }
        if (bestf < 0) break;
        assign[bestf] = besta; used[besta] = 1; total += bestc;
    }
    free(used);
    return total;
}

/* ============================== ULD packing ============================== */

int gor_av_uld_packing(int n, const gor_real *items, gor_real W, gor_real H, gor_real D, int *uld_of)
{
    return gor_bin_packing_3d(n, items, W, H, D, uld_of);
}
