/* ============================================================================
 * gmb_dag.h — DAG generico a fan-out/fan-in bounded (parità di
 * common/AI/Agents/AgentDag.cs, AGENTS §0bis: implementazione condivisa nel
 * runtime C per VM/C, C99Native — mai bridge C↔C#).
 * Fan-out parallelo a concorrenza bounded; fan-in: ogni nodo riceve i risultati
 * dei suoi DependsOn concatenati (una riga l'uno), nell'ordine del DependsOn.
 * ============================================================================ */
#ifndef GMB_DAG_H
#define GMB_DAG_H

#include <stddef.h>
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compute di un nodo: riceve `input` (risultati dei DependsOn, una riga l'uno)
 * e ritorna una stringa allocata (malloc) posseduta dal chiamante DAG; se
 * ritorna NULL viene trattata come stringa vuota. */
typedef char* (*gmb_dag_compute_fn)(const char* input, void* user);

typedef struct gmb_dag_node {
    const char* id;               /* identificativo unico (case-insensitive) */
    const char* const* deps;      /* id dei DependsOn (case-insensitive) */
    int ndeps;
    gmb_dag_compute_fn compute;   /* mai NULL */
    void* user;
} gmb_dag_node_t;

typedef struct gmb_dag_outcome {
    int ok;                       /* 1 = eseguito, 0 = errore esplicito */
    char* error;                  /* strdup del messaggio, NULL se ok (liberare) */
    int n;                        /* numero nodi (solo se ok) */
    char** ids;                   /* [n] id (strdup) — ordine dei nodi in input */
    char** results;               /* [n] risultato (strdup) per ids[i] */
    int peak;                     /* picco di concorrenza osservato */
} gmb_dag_outcome_t;

/* Esegue il DAG (Kahn + fan-out/fan-in a concorrenza bounded). max_concurrency
 * < 1 => 1. Riempe *out (azzerare con gmb_dag_outcome_init prima). Ritorna 1 se
 * *out è popolato (ok o errore), 0 solo per argomenti non validi. */
int gmb_dag_init_outcome(gmb_dag_outcome_t* out);
void gmb_dag_free_outcome(gmb_dag_outcome_t* out);
int gmb_dag_run(const gmb_dag_node_t* nodes, int n, int max_concurrency,
                gmb_dag_outcome_t* out);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DAG_H */
