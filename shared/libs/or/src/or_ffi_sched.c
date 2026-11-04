/*
 * or_ffi_sched.c — shim ABI per il dominio SCHEDULING del namespace OPTIMIZE
 * (macchina singola, flow shop, progetto, job shop, rostering).
 */
#include "gor_sched.h"

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

/* ── Macchina singola ───────────────────────────────────────────────────── */

double ofx_sched_spt(const double *p, i64 n, i64 *order)
{
    int *o; double r;
    if (n <= 0 || p == NULL || order == NULL) return -1.0;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) return -1.0;
    r = (double)gor_sched_spt((int)n, (const gor_real *)p, o);
    out_iarr(o, order, n); free(o); return r;
}

double ofx_sched_wspt(const double *p, const double *w, i64 n, i64 *order)
{
    int *o; double r;
    if (n <= 0 || p == NULL || w == NULL || order == NULL) return -1.0;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) return -1.0;
    r = (double)gor_sched_wspt((int)n, (const gor_real *)p, (const gor_real *)w, o);
    out_iarr(o, order, n); free(o); return r;
}

double ofx_sched_edd(const double *p, const double *due, i64 n, i64 *order)
{
    int *o; double r;
    if (n <= 0 || p == NULL || due == NULL || order == NULL) return -1.0;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) return -1.0;
    r = (double)gor_sched_edd((int)n, (const gor_real *)p, (const gor_real *)due, o);
    out_iarr(o, order, n); free(o); return r;
}

i64 ofx_sched_moore(const double *p, const double *due, i64 n, i64 *order)
{
    int *o; int r;
    if (n <= 0 || p == NULL || due == NULL || order == NULL) return -1;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) return -1;
    r = gor_sched_moore_hodgson((int)n, (const gor_real *)p, (const gor_real *)due, o);
    out_iarr(o, order, n); free(o); return (i64)r;
}

double ofx_sched_lawler(const double *p, const double *due, i64 n, i64 *order)
{
    int *o; double r;
    if (n <= 0 || p == NULL || due == NULL || order == NULL) return -1.0;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) return -1.0;
    r = (double)gor_sched_lawler((int)n, (const gor_real *)p, (const gor_real *)due, o);
    out_iarr(o, order, n); free(o); return r;
}

/* ── Intervalli ─────────────────────────────────────────────────────────── */

i64 ofx_interval_max(const double *start, const double *end, i64 n, i64 *selected)
{
    int *s; int r;
    if (n <= 0 || start == NULL || end == NULL || selected == NULL) return -1;
    s = (int *)malloc((size_t)n * sizeof(int));
    if (s == NULL) return -1;
    r = gor_sched_interval_max((int)n, (const gor_real *)start, (const gor_real *)end, s);
    out_iarr(s, selected, n); free(s); return (i64)r;
}

i64 ofx_interval_partition(const double *start, const double *end, i64 n)
{
    if (n <= 0 || start == NULL || end == NULL) return -1;
    return (i64)gor_sched_interval_partition((int)n, (const gor_real *)start, (const gor_real *)end);
}

/* ── Flow shop ──────────────────────────────────────────────────────────── */

double ofx_johnson(const double *p1, const double *p2, i64 n, i64 *seq)
{
    int *s; double r;
    if (n <= 0 || p1 == NULL || p2 == NULL || seq == NULL) return -1.0;
    s = (int *)malloc((size_t)n * sizeof(int));
    if (s == NULL) return -1.0;
    r = (double)gor_sched_johnson((int)n, (const gor_real *)p1, (const gor_real *)p2, s);
    out_iarr(s, seq, n); free(s); return r;
}

double ofx_neh(const double *p, i64 n, i64 m, i64 *seq)
{
    int *s; double r;
    if (n <= 0 || m <= 0 || p == NULL || seq == NULL) return -1.0;
    s = (int *)malloc((size_t)n * sizeof(int));
    if (s == NULL) return -1.0;
    r = (double)gor_sched_neh((int)n, (int)m, (const gor_real *)p, s);
    out_iarr(s, seq, n); free(s); return r;
}

double ofx_johnson3(const double *p1, const double *p2, const double *p3, i64 n, i64 *seq)
{
    int *s; double r;
    if (n <= 0 || p1 == NULL || p2 == NULL || p3 == NULL || seq == NULL) return -1.0;
    s = (int *)malloc((size_t)n * sizeof(int));
    if (s == NULL) return -1.0;
    r = (double)gor_sched_johnson3((int)n, (const gor_real *)p1, (const gor_real *)p2,
                                   (const gor_real *)p3, s);
    out_iarr(s, seq, n); free(s); return r;
}

double ofx_cds(const double *p, i64 n, i64 m, i64 *seq)
{
    int *s; double r;
    if (n <= 0 || m <= 0 || p == NULL || seq == NULL) return -1.0;
    s = (int *)malloc((size_t)n * sizeof(int));
    if (s == NULL) return -1.0;
    r = (double)gor_sched_cds((int)n, (int)m, (const gor_real *)p, s);
    out_iarr(s, seq, n); free(s); return r;
}

double ofx_lpt(const double *p, i64 n, i64 m, i64 *assign)
{
    int *a; double r;
    if (n <= 0 || m <= 0 || p == NULL || assign == NULL) return -1.0;
    a = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL) return -1.0;
    r = (double)gor_sched_lpt((int)n, (const gor_real *)p, (int)m, a);
    out_iarr(a, assign, n); free(a); return r;
}

/* ── Progetto: CPM / PERT / RCPSP ───────────────────────────────────────── */

double ofx_cpm(const double *dur, const i64 *preds, i64 n, i64 npred,
               double *est, double *lst, double *slack)
{
    int *p; double r;
    if (n <= 0 || npred < 0 || dur == NULL || preds == NULL || est == NULL ||
        lst == NULL || slack == NULL) return -1.0;
    p = to_iarr(preds, n * npred);
    if (p == NULL) { if (n * npred == 0) p = (int *)malloc(1); if (p == NULL) return -1.0; }
    r = (double)gor_cpm((int)n, (int)npred, (const gor_real *)dur, p,
                        (gor_real *)est, (gor_real *)lst, (gor_real *)slack);
    free(p);
    return r;
}

double ofx_pert(const double *to, const double *tm, const double *tp, const i64 *preds,
                i64 n, i64 npred, double *est, double *variance)
{
    int *p; double r;
    if (n <= 0 || npred < 0 || to == NULL || tm == NULL || tp == NULL || preds == NULL ||
        est == NULL || variance == NULL) return -1.0;
    p = to_iarr(preds, n * npred);
    if (p == NULL) { if (n * npred == 0) p = (int *)malloc(1); if (p == NULL) return -1.0; }
    r = (double)gor_pert((int)n, (int)npred, (const gor_real *)to, (const gor_real *)tm,
                         (const gor_real *)tp, p, (gor_real *)est, (gor_real *)variance);
    free(p);
    return r;
}

double ofx_rcpsp(const double *dur, const i64 *preds, i64 n, i64 npred,
                 const double *demand, const double *capacity, i64 nr, i64 *start)
{
    int *p, *s; double r;
    if (n <= 0 || npred < 0 || nr <= 0 || dur == NULL || preds == NULL || demand == NULL ||
        capacity == NULL || start == NULL) return -1.0;
    p = to_iarr(preds, n * npred);
    s = (int *)malloc((size_t)n * sizeof(int));
    if (p == NULL && n * npred > 0) { free(s); return -1.0; }
    if (s == NULL) { free(p); return -1.0; }
    r = (double)gor_rcpsp_sgs((int)n, (int)nr, (const gor_real *)dur, p, (int)npred,
                              (const gor_real *)demand, (const gor_real *)capacity, s);
    out_iarr(s, start, n);
    free(p); free(s);
    return r;
}

/* ── Job shop / timetabling / rostering ─────────────────────────────────── */

double ofx_job_shop(const i64 *route, const double *dur, i64 njobs, i64 nmach, i64 *start)
{
    int *rt, *st; double r;
    if (njobs <= 0 || nmach <= 0 || route == NULL || dur == NULL || start == NULL) return -1.0;
    rt = to_iarr(route, njobs * nmach);
    st = (int *)malloc((size_t)(njobs * nmach) * sizeof(int));
    if (rt == NULL || st == NULL) { free(rt); free(st); return -1.0; }
    r = (double)gor_job_shop((int)njobs, (int)nmach, rt, (const gor_real *)dur, st);
    out_iarr(st, start, njobs * nmach);
    free(rt); free(st);
    return r;
}

i64 ofx_timetabling(const i64 *conflicts, i64 n, i64 *slot_of)
{
    int *c, *s; int r;
    if (n <= 0 || conflicts == NULL || slot_of == NULL) return -1;
    c = to_iarr(conflicts, n * n);
    s = (int *)malloc((size_t)n * sizeof(int));
    if (c == NULL || s == NULL) { free(c); free(s); return -1; }
    r = gor_timetabling((int)n, c, s);
    out_iarr(s, slot_of, n);
    free(c); free(s);
    return (i64)r;
}

i64 ofx_rostering(const i64 *demand, i64 nnurses, i64 ndays, i64 max_work, i64 *schedule)
{
    int *d, *s; int r;
    if (nnurses <= 0 || ndays <= 0 || demand == NULL || schedule == NULL) return -1;
    d = to_iarr(demand, ndays);
    s = (int *)malloc((size_t)(nnurses * ndays) * sizeof(int));
    if (d == NULL || s == NULL) { free(d); free(s); return -1; }
    r = gor_rostering((int)nnurses, (int)ndays, d, (int)max_work, s);
    out_iarr(s, schedule, nnurses * ndays);
    free(d); free(s);
    return (i64)r;
}
