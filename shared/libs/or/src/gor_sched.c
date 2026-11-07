/* gor_sched.c — scheduling e project management. */
#include "gor_sched.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

gor_real gor_sched_spt(int n, const gor_real *p, int *order)
{
    int i, j, c = 0;
    gor_real t = 0.0;
    if (n <= 0 || p == NULL || order == NULL) return 0.0;
    for (i = 0; i < n; i++) order[i] = i;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (p[order[j]] < p[order[i]]) { int t2 = order[i]; order[i] = order[j]; order[j] = t2; }
    for (i = 0; i < n; i++) { c += (int)(p[order[i]] + 0.5); t += c; }
    return t;
}

gor_real gor_sched_wspt(int n, const gor_real *p, const gor_real *w, int *order)
{
    int i, j;
    gor_real t = 0.0, c = 0.0;
    if (n <= 0 || p == NULL || w == NULL || order == NULL) return 0.0;
    for (i = 0; i < n; i++) order[i] = i;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) {
            gor_real ri = (w[order[i]] > 0) ? p[order[i]] / w[order[i]] : 1e300;
            gor_real rj = (w[order[j]] > 0) ? p[order[j]] / w[order[j]] : 1e300;
            if (rj < ri) { int t2 = order[i]; order[i] = order[j]; order[j] = t2; }
        }
    for (i = 0; i < n; i++) { c += p[order[i]]; t += w[order[i]] * c; }
    return t;
}

gor_real gor_sched_edd(int n, const gor_real *p, const gor_real *due, int *order)
{
    int i, j;
    gor_real c = 0.0, lmax = -1e300;
    if (n <= 0 || p == NULL || due == NULL || order == NULL) return 0.0;
    for (i = 0; i < n; i++) order[i] = i;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (due[order[j]] < due[order[i]]) { int t2 = order[i]; order[i] = order[j]; order[j] = t2; }
    for (i = 0; i < n; i++) { c += p[order[i]]; if (c - due[order[i]] > lmax) lmax = c - due[order[i]]; }
    return lmax;
}

int gor_sched_moore_hodgson(int n, const gor_real *p, const gor_real *due, int *order)
{
    int *seq, *kept, nk = 0, i, j, tardy = 0;
    gor_real t = 0.0;
    if (n <= 0 || p == NULL || due == NULL || order == NULL) return 0;
    seq = (int *)malloc((size_t)n * sizeof(int));
    kept = (int *)malloc((size_t)n * sizeof(int));
    if (!seq || !kept) { free(seq); free(kept); return 0; }
    for (i = 0; i < n; i++) seq[i] = i;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (due[seq[j]] < due[seq[i]]) { int t2 = seq[i]; seq[i] = seq[j]; seq[j] = t2; }
    for (i = 0; i < n; i++) {
        kept[nk++] = seq[i]; t += p[seq[i]];
        if (t > due[seq[i]]) {
            /* rimuovi il job con p maggiore */
            int bi = 0, bj;
            for (bj = 1; bj < nk; bj++) if (p[kept[bj]] > p[kept[bi]]) bi = bj;
            t -= p[kept[bi]];
            for (bj = bi; bj < nk - 1; bj++) kept[bj] = kept[bj + 1];
            nk--; tardy++;
        }
    }
    for (i = 0; i < nk; i++) order[i] = kept[i];
    free(seq); free(kept);
    return tardy;
}

gor_real gor_sched_lawler(int n, const gor_real *p, const gor_real *due, int *order)
{
    /* senza precedenze: EDD è ottimo per Lmax */
    return gor_sched_edd(n, p, due, order);
}

/* ============================== flow shop ============================== */

gor_real gor_sched_johnson(int n, const gor_real *p1, const gor_real *p2, int *seq)
{
    int *jobset, head = 0, tail = n - 1, i, j;
    gor_real c1 = 0.0, c2 = 0.0, cmax = 0.0;
    if (n <= 0 || p1 == NULL || p2 == NULL || seq == NULL) return 0.0;
    jobset = (int *)malloc((size_t)n * sizeof(int));
    if (!jobset) return 0.0;
    for (i = 0; i < n; i++) jobset[i] = i;
    for (i = 0; i < n; i++) {
        int bi = -1, bj;
        gor_real bmin = 1e300;
        for (bj = 0; bj < n; bj++) {
            if (jobset[bj] < 0) continue;
            {
                gor_real m = p1[jobset[bj]] < p2[jobset[bj]] ? p1[jobset[bj]] : p2[jobset[bj]];
                if (m < bmin) { bmin = m; bi = bj; }
            }
        }
        if (bi < 0) break;
        if (p1[jobset[bi]] <= p2[jobset[bi]]) seq[head++] = jobset[bi];
        else seq[tail--] = jobset[bi];
        jobset[bi] = -1;
    }
    for (i = 0; i < n; i++) {
        c1 += p1[seq[i]];
        c2 = (c2 > c1 ? c2 : c1) + p2[seq[i]];
    }
    cmax = c2;
    (void)j;
    free(jobset);
    return cmax;
}

static gor_real neh_makespan(int n, int m, const gor_real *p, const int *seq)
{
    /* p: n×m (job×machine) */
    gor_real *ct = (gor_real *)calloc((size_t)m, sizeof(gor_real));
    gor_real cmax = 0.0;
    int i, k;
    if (!ct) return 0.0;
    for (i = 0; i < n; i++) {
        for (k = 0; k < m; k++) {
            gor_real prev = (k > 0) ? ct[k - 1] : 0.0;
            ct[k] = (ct[k] > prev ? ct[k] : prev) + p[(size_t)seq[i] * (size_t)m + (size_t)k];
        }
    }
    cmax = ct[m - 1];
    free(ct);
    return cmax;
}

gor_real gor_sched_neh(int n, int m, const gor_real *p, int *seq)
{
    int *ord, *tmp, i, j, k;
    if (n <= 0 || m <= 0 || p == NULL || seq == NULL) return 0.0;
    ord = (int *)malloc((size_t)n * sizeof(int));
    tmp = (int *)malloc((size_t)n * sizeof(int));
    if (!ord || !tmp) { free(ord); free(tmp); return 0.0; }
    for (i = 0; i < n; i++) ord[i] = i;
    /* ordina per tempo totale decrescente */
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) {
            gor_real ti = 0.0, tj = 0.0;
            for (k = 0; k < m; k++) { ti += p[(size_t)ord[i] * (size_t)m + (size_t)k]; tj += p[(size_t)ord[j] * (size_t)m + (size_t)k]; }
            if (tj > ti) { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
        }
    seq[0] = ord[0];
    for (i = 1; i < n; i++) {
        int blen = i + 1, bpos = 0;
        gor_real bmk = 1e300;
        for (j = 0; j <= i; j++) {
            gor_real mk;
            for (k = 0; k < j; k++) tmp[k] = seq[k];
            tmp[j] = ord[i];
            for (k = j; k < i; k++) tmp[k + 1] = seq[k];
            mk = neh_makespan(blen, m, p, tmp);
            if (mk < bmk) { bmk = mk; bpos = j; }
        }
        for (k = 0; k < bpos; k++) tmp[k] = seq[k];
        tmp[bpos] = ord[i];
        for (k = bpos; k < i; k++) tmp[k + 1] = seq[k];
        for (k = 0; k <= i; k++) seq[k] = tmp[k];
    }
    free(ord); free(tmp);
    return n > 0 ? neh_makespan(n, m, p, seq) : 0.0;
}

/* ============================== parallel machines ============================== */

gor_real gor_sched_lpt(int n, const gor_real *p, int m, int *assign)
{
    gor_real *load;
    int *ord, i, j, k;
    gor_real cmax = 0.0;
    if (n <= 0 || m <= 0 || p == NULL || assign == NULL) return 0.0;
    load = (gor_real *)calloc((size_t)m, sizeof(gor_real));
    ord = (int *)malloc((size_t)n * sizeof(int));
    if (!load || !ord) { free(load); free(ord); return 0.0; }
    for (i = 0; i < n; i++) ord[i] = i;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (p[ord[j]] > p[ord[i]]) { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    for (i = 0; i < n; i++) {
        int bm = 0;
        for (k = 1; k < m; k++) if (load[k] < load[bm]) bm = k;
        assign[ord[i]] = bm; load[bm] += p[ord[i]];
    }
    for (k = 0; k < m; k++) if (load[k] > cmax) cmax = load[k];
    free(load); free(ord);
    return cmax;
}

/* ============================== CPM / PERT ============================== */

static gor_real cpm_core(int n, int npred, const gor_real *dur, const int *preds,
                         gor_real *est, gor_real *lst, gor_real *slack)
{
    int *succ_head, *succ_next, i, k;
    gor_real proj = 0.0;
    if (n <= 0 || dur == NULL || preds == NULL) return 0.0;
    succ_head = (int *)malloc((size_t)n * sizeof(int));
    succ_next = (int *)malloc((size_t)(npred * n > 0 ? npred * n : 1) * sizeof(int));
    if (!succ_head || !succ_next) { free(succ_head); free(succ_next); return 0.0; }
    for (i = 0; i < n; i++) succ_head[i] = -1;
    /* forward pass (assume attività in ordine topologico: preds[j] < j) */
    for (i = 0; i < n; i++) {
        gor_real e = 0.0;
        for (k = 0; k < npred; k++) {
            int p = preds[(size_t)i * (size_t)npred + (size_t)k];
            if (p >= 0 && est[p] + dur[p] > e) e = est[p] + dur[p];
        }
        est[i] = e;
        if (e + dur[i] > proj) proj = e + dur[i];
    }
    /* successori */
    for (i = 0; i < n; i++)
        for (k = 0; k < npred; k++) {
            int p = preds[(size_t)i * (size_t)npred + (size_t)k];
            if (p >= 0) { succ_next[i * npred + k] = succ_head[p]; succ_head[p] = i * npred + k; }
        }
    /* backward pass */
    for (i = n - 1; i >= 0; i--) {
        gor_real l = proj;
        int e = succ_head[i];
        int has = 0;
        while (e >= 0) {
            int s = e / npred;
            if (!has || lst[s] < l) { l = lst[s]; has = 1; }
            e = succ_next[e];
        }
        lst[i] = l - dur[i];
        if (slack) slack[i] = lst[i] - est[i];
    }
    free(succ_head); free(succ_next);
    return proj;
}

gor_real gor_cpm(int n, int npred, const gor_real *dur, const int *preds,
                 gor_real *est, gor_real *lst, gor_real *slack)
{
    return cpm_core(n, npred, dur, preds, est, lst, slack);
}

gor_real gor_pert(int n, int npred, const gor_real *to, const gor_real *tm, const gor_real *tp,
                  const int *preds, gor_real *est, gor_real *variance)
{
    gor_real *dur, *var, *lst, *slack;
    gor_real proj;
    int i;
    if (n <= 0 || to == NULL || tm == NULL || tp == NULL) return 0.0;
    dur = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    var = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    lst = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    slack = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (!dur || !var || !lst || !slack) { free(dur); free(var); free(lst); free(slack); return 0.0; }
    for (i = 0; i < n; i++) { dur[i] = (to[i] + 4.0 * tm[i] + tp[i]) / 6.0; var[i] = ((tp[i] - to[i]) / 6.0) * ((tp[i] - to[i]) / 6.0); }
    proj = cpm_core(n, npred, dur, preds, est, lst, slack);
    if (variance) {
        /* varianza del cammino critico = somma varianze delle attività con slack ~ 0 */
        gor_real v = 0.0;
        for (i = 0; i < n; i++) if (fabs(slack[i]) < 1e-9) v += var[i];
        *variance = v;
    }
    free(dur); free(var); free(lst); free(slack);
    return proj;
}

/* ============================== RCPSP (serial SGS) ============================== */

gor_real gor_rcpsp_sgs(int n, int nr, const gor_real *dur, const int *preds, int npred,
                       const gor_real *demand, const gor_real *capacity, int *start)
{
    int *done;
    int i, k, scheduled = 0, guard = 0;
    gor_real makespan = 0.0;
    if (n <= 0 || nr <= 0 || dur == NULL || preds == NULL || demand == NULL || capacity == NULL || start == NULL) return 0.0;
    done = (int *)calloc((size_t)n, sizeof(int));
    if (!done) return 0.0;
    for (i = 0; i < n; i++) start[i] = 0;
    while (scheduled < n && guard++ < n * n + 10) {
        int progressed = 0;
        for (i = 0; i < n; i++) {
            int ready = 1;
            gor_real t;
            if (done[i]) continue;
            for (k = 0; k < npred; k++) {
                int p = preds[(size_t)i * (size_t)npred + (size_t)k];
                if (p >= 0 && !done[p]) { ready = 0; break; }
            }
            if (!ready) continue;
            /* earliest start dopo i predecessori */
            t = 0.0;
            for (k = 0; k < npred; k++) {
                int p = preds[(size_t)i * (size_t)npred + (size_t)k];
                if (p >= 0 && start[p] + dur[p] > t) t = start[p] + dur[p];
            }
            /* cerca il primo istante con risorse libere */
            for (;;) {
                int ok = 1, r;
                for (r = 0; r < nr && ok; r++) {
                    gor_real used = 0.0;
                    int j;
                    for (j = 0; j < n; j++) {
                        if (done[j] || j == i) continue;
                        if (start[j] <= t && t < start[j] + dur[j]) used += demand[(size_t)j * (size_t)nr + (size_t)r];
                    }
                    if (used + demand[(size_t)i * (size_t)nr + (size_t)r] > capacity[r] + 1e-9) ok = 0;
                }
                if (ok) break;
                t += 0.5;
            }
            start[i] = (int)(t + 0.5);
            done[i] = 1; scheduled++; progressed = 1;
            if (start[i] + dur[i] > makespan) makespan = start[i] + dur[i];
        }
        if (!progressed) break;
    }
    free(done);
    return makespan;
}

/* ============================== job shop (dispatch MWKR) ============================== */

gor_real gor_job_shop(int njobs, int nmach, const int *route, const gor_real *dur, int *start)
{
    int *op_next, *job_ready_op, *mach_ready;
    double *job_ready, *mach_free;
    int total = njobs * nmach, done = 0, guard = 0;
    double makespan = 0.0;
    if (njobs <= 0 || nmach <= 0 || route == NULL || dur == NULL || start == NULL) return 0.0;
    job_ready_op = (int *)calloc((size_t)njobs, sizeof(int));
    mach_ready = (int *)calloc((size_t)nmach, sizeof(int));
    job_ready = (double *)calloc((size_t)njobs, sizeof(double));
    mach_free = (double *)calloc((size_t)nmach, sizeof(double));
    op_next = (int *)malloc((size_t)total * sizeof(int));
    if (!job_ready_op || !mach_ready || !job_ready || !mach_free || !op_next) {
        free(job_ready_op); free(mach_ready); free(job_ready); free(mach_free); free(op_next); return 0.0;
    }
    /* ordina le operazioni per priorità MWKR: lavoro rimanente sulla macchina */
    while (done < total && guard++ < total * total + 10) {
        int bj = -1, best = 0;
        for (bj = 0; bj < njobs; bj++) {
            if (job_ready_op[bj] >= nmach) continue;
            if (best == 0) best = bj; /* primo disponibile */
            break;
        }
        /* Giffler-Thompson semplificato: scegli l'op col minimo completamento */
        {
            double bestc = 1e300;
            int chosen = -1;
            int j;
            for (j = 0; j < njobs; j++) {
                int k = job_ready_op[j];
                int m;
                double est, c;
                if (k >= nmach) continue;
                m = route[j * nmach + k];
                est = job_ready[j] > mach_free[m] ? job_ready[j] : mach_free[m];
                c = est + dur[j * nmach + k];
                if (c < bestc) { bestc = c; chosen = j; }
            }
            if (chosen < 0) break;
            {
                int k = job_ready_op[chosen];
                int m = route[chosen * nmach + k];
                double est = job_ready[chosen] > mach_free[m] ? job_ready[chosen] : mach_free[m];
                start[chosen * nmach + k] = (int)(est + 0.5);
                job_ready[chosen] = est + dur[chosen * nmach + k];
                mach_free[m] = job_ready[chosen];
                job_ready_op[chosen]++;
                done++;
                if (mach_free[m] > makespan) makespan = mach_free[m];
            }
        }
    }
    free(job_ready_op); free(mach_ready); free(job_ready); free(mach_free); free(op_next);
    return makespan;
}

/* ============================== timetabling ============================== */

int gor_timetabling(int n, const int *conflicts, int *slot_of)
{
    int *satdeg;
    char *assigned;
    int i, j, k, nslots = 0;
    if (n <= 0 || conflicts == NULL || slot_of == NULL) return 0;
    satdeg = (int *)calloc((size_t)n, sizeof(int));
    assigned = (char *)calloc((size_t)n, 1);
    if (!satdeg || !assigned) { free(satdeg); free(assigned); return 0; }
    for (i = 0; i < n; i++) slot_of[i] = -1;
    for (k = 0; k < n; k++) {
        int bu = -1, bs = -1, bd = -1;
        for (i = 0; i < n; i++) {
            int deg = 0;
            if (assigned[i]) continue;
            for (j = 0; j < n; j++) if (conflicts[(size_t)i * (size_t)n + (size_t)j]) deg++;
            if (satdeg[i] > bs || (satdeg[i] == bs && deg > bd)) { bs = satdeg[i]; bd = deg; bu = i; }
        }
        if (bu < 0) break;
        {
            char *used = (char *)calloc((size_t)(n + 1), 1);
            int c;
            for (j = 0; j < n; j++) if (conflicts[(size_t)bu * (size_t)n + (size_t)j] && assigned[j] && slot_of[j] >= 0) used[slot_of[j]] = 1;
            for (c = 0; c < n; c++) if (!used[c]) break;
            slot_of[bu] = c; assigned[bu] = 1;
            if (c + 1 > nslots) nslots = c + 1;
            for (j = 0; j < n; j++) if (conflicts[(size_t)bu * (size_t)n + (size_t)j] && !assigned[j]) satdeg[j]++;
            free(used);
        }
    }
    free(satdeg); free(assigned);
    return nslots;
}

/* ============================== nurse rostering ============================== */

int gor_rostering(int nnurses, int ndays, const int *demand, int max_work, int *schedule)
{
    int *assigned_day, *consec, *last_day;
    int d, nu, uncovered = 0;
    if (nnurses <= 0 || ndays <= 0 || demand == NULL || schedule == NULL) return 0;
    assigned_day = (int *)calloc((size_t)ndays, sizeof(int));
    consec = (int *)calloc((size_t)nnurses, sizeof(int));
    last_day = (int *)malloc((size_t)nnurses * sizeof(int));
    if (!assigned_day || !consec || !last_day) { free(assigned_day); free(consec); free(last_day); return 0; }
    for (nu = 0; nu < nnurses; nu++) last_day[nu] = -2;
    for (nu = 0; nu < nnurses * ndays; nu++) schedule[nu] = 0;
    for (d = 0; d < ndays; d++) {
        while (assigned_day[d] < demand[d]) {
            int best = -1, bconsec = 1 << 30;
            for (nu = 0; nu < nnurses; nu++) {
                if (last_day[nu] == d - 1 && consec[nu] >= max_work) continue; /* troppi consecutivi */
                if (last_day[nu] == d) continue;
                if (consec[nu] < bconsec) { bconsec = consec[nu]; best = nu; }
            }
            if (best < 0) break;
            schedule[(size_t)best * (size_t)ndays + (size_t)d] = 1;
            consec[best] = (last_day[best] == d - 1) ? consec[best] + 1 : 1;
            last_day[best] = d;
            assigned_day[d]++;
        }
        if (assigned_day[d] < demand[d]) uncovered += demand[d] - assigned_day[d];
    }
    free(assigned_day); free(consec); free(last_day);
    return uncovered;
}

/* ===================== Flow shop: Johnson 3 macchine e CDS (job sequencing) ===================== */

gor_real gor_sched_johnson3(int n, const gor_real *p1, const gor_real *p2,
                            const gor_real *p3, int *seq)
{
    gor_real *a, *b, c1 = 0.0, c2 = 0.0, c3 = 0.0, cmax = 0.0;
    int i;
    if (n <= 0 || !p1 || !p2 || !p3 || !seq) return 0.0;
    a = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    b = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (!a || !b) { free(a); free(b); return 0.0; }
    for (i = 0; i < n; i++) { a[i] = p1[i] + p2[i]; b[i] = p2[i] + p3[i]; }
    gor_sched_johnson(n, a, b, seq);
    for (i = 0; i < n; i++) {
        int j = seq[i];
        c1 += p1[j];
        c2 = (c2 > c1 ? c2 : c1) + p2[j];
        c3 = (c3 > c2 ? c3 : c2) + p3[j];
    }
    cmax = c3;
    free(a); free(b);
    return cmax;
}

gor_real gor_sched_cds(int n, int m, const gor_real *p, int *seq)
{
    gor_real *a, *b, best = 1e300;
    int *tmpseq;
    int k, i, r;
    if (n <= 0 || m < 2 || !p || !seq) return 0.0;
    a = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    b = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    tmpseq = (int *)malloc((size_t)n * sizeof(int));
    if (!a || !b || !tmpseq) { free(a); free(b); free(tmpseq); return 0.0; }
    for (k = 1; k < m; k++) {
        gor_real mk;
        for (i = 0; i < n; i++) {
            gor_real sa = 0.0, sb = 0.0;
            for (r = 0; r < k; r++) sa += p[(size_t)i * m + r];
            for (r = m - k; r < m; r++) sb += p[(size_t)i * m + r];
            a[i] = sa; b[i] = sb;
        }
        gor_sched_johnson(n, a, b, tmpseq);
        mk = neh_makespan(n, m, p, tmpseq);
        if (mk < best) {
            best = mk;
            for (i = 0; i < n; i++) seq[i] = tmpseq[i];
        }
    }
    free(a); free(b); free(tmpseq);
    return best;
}

/* ===================== Interval scheduling (da AirScheduling/runway) ===================== */

int gor_sched_interval_max(int n, const gor_real *start, const gor_real *end, int *selected)
{
    int *ord, i, cnt = 0;
    gor_real last = -1e300;
    if (n < 1 || !start || !end) return 0;
    ord = (int *)malloc((size_t)n * sizeof(int));
    if (!ord) return 0;
    for (i = 0; i < n; i++) ord[i] = i;
    /* ordina per end crescente (insertion) */
    for (i = 1; i < n; i++) {
        int k = ord[i], q = i - 1;
        while (q >= 0 && end[ord[q]] > end[k]) { ord[q + 1] = ord[q]; q--; }
        ord[q + 1] = k;
    }
    if (selected) for (i = 0; i < n; i++) selected[i] = 0;
    for (i = 0; i < n; i++) {
        int idx = ord[i];
        if (start[idx] >= last) { last = end[idx]; cnt++; if (selected) selected[idx] = 1; }
    }
    free(ord);
    return cnt;
}

int gor_sched_interval_partition(int n, const gor_real *start, const gor_real *end)
{
    /* min risorse = massimo numero di intervalli sovrapposti in un punto:
     * ordina gli eventi (start +1, end -1) e prendi il massimo prefisso. */
    gor_real *ev;
    int *type;
    int i, cnt = 0, mx = 0;
    if (n < 1 || !start || !end) return 0;
    ev = (gor_real *)malloc((size_t)(2 * n) * sizeof(gor_real));
    type = (int *)malloc((size_t)(2 * n) * sizeof(int));
    if (!ev || !type) { free(ev); free(type); return 0; }
    for (i = 0; i < n; i++) { ev[2*i] = start[i]; type[2*i] = 1; ev[2*i+1] = end[i]; type[2*i+1] = -1; }
    /* insertion sort per tempo; a parita', end (-1) prima di start (+1) */
    for (i = 1; i < 2*n; i++) {
        gor_real ke = ev[i]; int kt = type[i], q = i - 1;
        while (q >= 0 && (ev[q] > ke || (ev[q] == ke && type[q] > kt))) { ev[q+1] = ev[q]; type[q+1] = type[q]; q--; }
        ev[q+1] = ke; type[q+1] = kt;
    }
    for (i = 0; i < 2*n; i++) { cnt += type[i]; if (cnt > mx) mx = cnt; }
    free(ev); free(type);
    return mx;
}
