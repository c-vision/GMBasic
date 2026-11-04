/*
 * gor_sched.h — scheduling e project management.
 *
 * Macchina singola (SPT/WSPT/EDD/Moore-Hodgson/Lawler), flow shop (Johnson/NEH),
 * macchine parallele (LPT), RCPSP (serial SGS), CPM/PERT.
 */
#ifndef GMBORTOOLS_GOR_SCHED_H
#define GMBORTOOLS_GOR_SCHED_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Macchina singola: p[n] tempi. order[n] sequenza. Ritorna l'obiettivo. */
gor_real gor_sched_spt(int n, const gor_real *p, int *order);       /* min ΣC_j */
gor_real gor_sched_wspt(int n, const gor_real *p, const gor_real *w, int *order); /* min Σw_jC_j */
gor_real gor_sched_edd(int n, const gor_real *p, const gor_real *due, int *order); /* min L_max */
int gor_sched_moore_hodgson(int n, const gor_real *p, const gor_real *due, int *order); /* min #tardivi */
gor_real gor_sched_lawler(int n, const gor_real *p, const gor_real *due, int *order); /* min L_max (prec. generiche) */

/* Flow shop */
/* Interval scheduling (activity selection, da AirScheduling/runway): numero
 * massimo di intervalli non sovrapposti (greedy per end time). selected[n]:
 * 1 se scelto. Ritorna il numero di intervalli scelti. */
int gor_sched_interval_max(int n, const gor_real *start, const gor_real *end, int *selected);
/* Min numero di risorse per coprire tutti gli intervalli (= massimo overlap,
 * interval graph coloring). Ritorna il numero di risorse. */
int gor_sched_interval_partition(int n, const gor_real *start, const gor_real *end);

gor_real gor_sched_johnson(int n, const gor_real *p1, const gor_real *p2, int *seq); /* 2 macchine, Cmax */
gor_real gor_sched_neh(int n, int m, const gor_real *p, int *seq);                   /* m macchine, Cmax */
/* Johnson esteso a 3 macchine (flow shop): ottimo se min_i p1_i >= max_i p2_i.
 * p1,p2,p3 = tempi per job. seq[n]. Ritorna Cmax. */
gor_real gor_sched_johnson3(int n, const gor_real *p1, const gor_real *p2,
                            const gor_real *p3, int *seq);
/* CDS (Campbell-Dudek-Smith) per flow shop a m macchine, Cmax.
 * p n*m row-major (job i su macchina k = p[i*m+k]). seq[n] riceve la sequenza
 * migliore tra gli m-1 problemi ridotti a 2 macchine. Ritorna il Cmax. */
gor_real gor_sched_cds(int n, int m, const gor_real *p, int *seq);

/* Macchine parallele identiche: LPT per P||Cmax. assign[i]=macchina. Ritorna Cmax. */
gor_real gor_sched_lpt(int n, const gor_real *p, int m, int *assign);

/* Project management: CPM. dur[n], preds[n*npred] (predecessori per attività, -1 padding).
 * est/lst/slack[n]. Ritorna la durata del progetto (cammino critico). */
gor_real gor_cpm(int n, int npred, const gor_real *dur, const int *preds,
                 gor_real *est, gor_real *lst, gor_real *slack);
/* PERT: come CPM ma con tempi (ottimistico, probabile, pessimistico). Ritorna E[progetto]. */
gor_real gor_pert(int n, int npred, const gor_real *to, const gor_real *tm, const gor_real *tp,
                  const int *preds, gor_real *est, gor_real *variance);

/* RCPSP (serial SGS): dur[n], preds, resource demand[n*r] e capacity[r]. start[n]. Ritorna makespan. */
gor_real gor_rcpsp_sgs(int n, int nr, const gor_real *dur, const int *preds, int npred,
                       const gor_real *demand, const gor_real *capacity, int *start);
/* Job shop: route[job*nmach + k] = macchina della k-esima operazione; dur idem.
 * start[job*nmach+k] = inizio. Euristica dispatch (MWKR). Ritorna il makespan. */
gor_real gor_job_shop(int njobs, int nmach, const int *route, const gor_real *dur, int *start);
/* Timetabling: n eventi, conflicts[n*n] (1=incompatibili). slot_of[n]=slot.
 * Ritorna il numero di slot usati (colorazione). */
int gor_timetabling(int n, const int *conflicts, int *slot_of);
/* Nurse rostering: nnurses, ndays, demand[d] = infermieri richiesti al giorno d,
 * max_work = turni consecutivi max. schedule[nurse*ndays]=1 se assegnato.
 * Ritorna il numero di turni non coperti (0 = copertura completa). */
int gor_rostering(int nnurses, int ndays, const int *demand, int max_work, int *schedule);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_SCHED_H */
