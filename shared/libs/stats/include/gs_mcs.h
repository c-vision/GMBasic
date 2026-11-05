/*
 * gs_mcs.h — Model Confidence Set (da et_al. inference/mcs.h).
 *
 * Procedura di Hansen, Lunde e Nason (2011) con statistica T_max: elimina
 * iterativamente i modelli peggiori finché il set rimasto non è
 * statisticamente distinguibile al livello alpha, usando un moving block
 * bootstrap per la distribuzione della statistica.
 */
#ifndef GMBSTATS_GS_MCS_H
#define GMBSTATS_GS_MCS_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gc_real alpha;              /* livello del test (es. 0.05) */
    int bootstrap;             /* numero di ricampionamenti (es. 1000) */
    int block_length;          /* lunghezza dei blocchi (es. 10) */
    unsigned long long seed;
} gs_mcs_options;

typedef struct {
    int n_models;
    int n_kept;
    gc_real pvalue;            /* p-value dell'ultimo round */
} gs_mcs_result;

/* losses: n×m row-major (losses[t*m + i]). in_set (m) riceve 1/0 per modello.
 * Ritorna il numero di modelli nel set (0 su errore). */
int gs_mcs(int n, int m, const gc_real *losses, const gs_mcs_options *opt,
           int *in_set, gs_mcs_result *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_MCS_H */
