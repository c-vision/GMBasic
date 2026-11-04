/*
 * gor_ls.h — motore di ricerca locale per ottimizzazione combinatoria.
 *
 * Modellato sull'API di un motore meta-euristico generico (Timefold/OptaPlanner,
 * come il progetto Apache-2.0 `optimaru`): il dominio fornisce lo stato, la
 * funzione di costo e (opzionale) una distanza; il motore fornisce acceptors,
 * terminazioni, selettore di mosse, nearby selection, ALNS e multi-start.
 *
 * Contenuto:
 *   - punteggio HardSoftScore (hard intero dominante, soft decimale);
 *   - RNG deterministico;
 *   - 5 terminazioni + composizione OR;
 *   - 7 acceptors (hill climbing, late acceptance, entity tabu, simulated
 *     annealing, great deluge, step counting hill climbing, record-to-record);
 *   - mosse su permutazione (relocate, swap, 2-opt, Or-opt) e su assegnazione
 *     (change, swap), mescolate per peso;
 *   - nearby selection (k vicini) e ALNS ruin&recreate adattivo
 *     (roulette wheel, ruine random/related/worst, repair greedy);
 *   - risoluzione singola o multi-start.
 *
 * Non contiene codice di dominio (nessun prezzo/rotta/turno): il chiamante
 * descrive stato e costo. C99 puro, dipendenze libc/libm.
 *
 * Nota sulle prestazioni: la valutazione è un callback sul vettore di stato
 * completo (O(n)); le mosse non usano delta incrementali. Per istanze grandi
 * conviene fornire un eval incrementale nel proprio ctx.
 */
#ifndef GMBORTOOLS_GOR_LS_H
#define GMBORTOOLS_GOR_LS_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Punteggio                                                                  */
/* ------------------------------------------------------------------------- */

/* Punteggio a due livelli: hard intero (dominante), soft decimale.
 * Più alto è meglio. Una soluzione è ammissibile se hard >= 0. */
typedef struct gor_ls_score {
    gor_int hard;
    gor_real soft;
} gor_ls_score;

/* <0 se a è peggiore di b, 0 se uguali, >0 se a è migliore. */
int gor_ls_score_cmp(gor_ls_score a, gor_ls_score b);
int gor_ls_score_feasible(gor_ls_score s);
gor_ls_score gor_ls_score_add(gor_ls_score a, gor_ls_score b);

/* ------------------------------------------------------------------------- */
/* RNG deterministico (xorshift64*)                                           */
/* ------------------------------------------------------------------------- */

typedef struct gor_ls_rng { unsigned long long state; } gor_ls_rng;

void gor_ls_rng_seed(gor_ls_rng *r, unsigned long long seed);
unsigned long long gor_ls_rng_next(gor_ls_rng *r);
/* Intero in [0, n). n <= 0 ritorna 0. */
int gor_ls_rng_below(gor_ls_rng *r, int n);
/* Reale in [0, 1). */
gor_real gor_ls_rng_unit(gor_ls_rng *r);

/* Millisecondi di tempo CPU (clock(), portabile C99; non wall-clock). */
long long gor_ls_now_ms(void);

/* ------------------------------------------------------------------------- */
/* Terminazioni (OR-composite: si ferma al primo criterio soddisfatto)        */
/* ------------------------------------------------------------------------- */

typedef struct gor_ls_termination {
    long long spent_ms;            /* tempo totale (0 = disattivo) */
    long long unimproved_ms;       /* tempo dall'ultimo miglioramento */
    long long step_count;          /* numero di passi */
    long long unimproved_step_count;
    gor_ls_score best_score_limit; /* stop quando best >= limite */
    int has_best_score_limit;
} gor_ls_termination;

typedef struct gor_ls_termination_state {
    gor_ls_termination cfg;
    long long start_ms;
    long long last_improve_ms;
    long long steps;
    long long last_improve_step;
    gor_ls_score best;
} gor_ls_termination_state;

void gor_ls_termination_init(gor_ls_termination_state *st,
                             const gor_ls_termination *cfg,
                             gor_ls_score start_best);
/* Aggiorna il contatore dei passi e valuta i criteri. Ritorna 1 se si deve
 * fermare. `now_ms` = gor_ls_now_ms(). */
int gor_ls_termination_done(gor_ls_termination_state *st, long long now_ms,
                            gor_ls_score current_best);

/* ------------------------------------------------------------------------- */
/* Acceptors                                                                  */
/* ------------------------------------------------------------------------- */

typedef enum gor_ls_acceptor_kind {
    GOR_LS_ACCEPT_HILL_CLIMBING = 0,
    GOR_LS_ACCEPT_LATE_ACCEPTANCE = 1,
    GOR_LS_ACCEPT_ENTITY_TABU = 2,
    GOR_LS_ACCEPT_SIMULATED_ANNEALING = 3,
    GOR_LS_ACCEPT_GREAT_DELUGE = 4,
    GOR_LS_ACCEPT_STEP_COUNTING_HILL_CLIMBING = 5,
    GOR_LS_ACCEPT_RECORD_TO_RECORD = 6
} gor_ls_acceptor_kind;

#define GOR_LS_LA_SIZE 400
#define GOR_LS_TABU_SIZE 64

typedef struct gor_ls_acceptor {
    gor_ls_acceptor_kind kind;

    /* late acceptance */
    int la_size;
    gor_ls_score la_history[GOR_LS_LA_SIZE];
    long long la_pos;

    /* entity tabu */
    int tabu_size;
    int tabu_ids[GOR_LS_TABU_SIZE];
    int tabu_count;
    int tabu_pos;

    /* simulated annealing */
    gor_real sa_temperature;
    gor_real sa_min_temperature;

    /* great deluge */
    gor_ls_score gd_water;
    gor_ls_score gd_increment;

    /* step counting hill climbing */
    long long schc_size;
    gor_ls_score schc_threshold;
    long long schc_step;

    /* record-to-record travel */
    gor_real rrt_deviation;
    int rrt_compare_to_best;

    gor_ls_rng rng;
} gor_ls_acceptor;

/* Inizializza con i default del tipo scelto; i campi restano modificabili
 * dal chiamante prima della solve. `start` = punteggio della soluzione
 * iniziale (serve a late acceptance / great deluge). */
void gor_ls_acceptor_init(gor_ls_acceptor *a, gor_ls_acceptor_kind kind,
                          gor_ls_score start, unsigned long long seed);

/* Decide sul candidato (già valutato). `move_entities` = entità toccate
 * (per entity tabu). `time_gradient` in [0,1] = frazione del tempo trascorsa
 * (per SA/RRT). Un'accettazione registra la memoria tabu. */
int gor_ls_acceptor_accept(gor_ls_acceptor *a, gor_ls_score candidate,
                           gor_ls_score current, gor_ls_score best,
                           const int *move_entities, int n_entities,
                           double time_gradient);

/* Fine passo, comunque sia andata (aggiorna la storia di late acceptance). */
void gor_ls_acceptor_step_ended(gor_ls_acceptor *a, gor_ls_score current);

/* ------------------------------------------------------------------------- */
/* Modello del dominio e configurazione                                       */
/* ------------------------------------------------------------------------- */

/* Stato = vettore di int, lungo n. Il motore minimizza il punteggio. */
typedef gor_ls_score (*gor_ls_eval_fn)(const int *state, int n, void *ctx);

/* Distanza simmetrica fra due elementi (per nearby e per le ruine
 * related/worst). Opzionale: senza, nearby e quelle ruine sono disattivate. */
typedef gor_real (*gor_ls_dist_fn)(int a, int b, void *ctx);

typedef enum gor_ls_shape {
    GOR_LS_SHAPE_PERMUTATION = 0, /* permutazione di 0..n-1 (liste/rotte) */
    GOR_LS_SHAPE_ASSIGNMENT = 1    /* n entità -> valore in [0, value_count) */
} gor_ls_shape;

#define GOR_LS_NEARBY_MAX 32

typedef struct gor_ls_config {
    gor_ls_shape shape;
    int n;
    int value_count;      /* ASSIGNMENT */
    gor_ls_eval_fn eval;
    void *ctx;
    gor_ls_dist_fn dist;  /* opzionale */

    /* pesi del mix di mosse (0 = mossa disattivata) */
    gor_real w_relocate;
    gor_real w_swap;
    gor_real w_two_opt;
    gor_real w_or_opt;
    int or_opt_max;       /* lunghezza massima segmento Or-opt (default 3) */

    int nearby_k;         /* 0 = disattivo; <= GOR_LS_NEARBY_MAX */
    int use_nearby;

    /* ALNS ruin & recreate */
    int alns;
    long long alns_trigger;      /* passi senza miglioramento */
    gor_real ruin_min_fraction;  /* default 0.10 */
    gor_real ruin_max_fraction;  /* default 0.30 */
    int max_ruin;                /* default 60 */
    int polish_steps;            /* passi di pulizia dopo il repair (default 50) */
    int alns_adaptive;           /* 1 = pesi adattivi (roulette) */

    gor_ls_acceptor acceptor;
    gor_ls_termination termination;
    int starts;                  /* partenze indipendenti (default 1) */
    unsigned long long seed;
} gor_ls_config;

/* Default ragionevoli per forma/dimensione/eval. */
void gor_ls_config_default(gor_ls_config *cfg, gor_ls_shape shape, int n,
                           gor_ls_eval_fn eval, void *ctx);

typedef struct gor_ls_result {
    gor_ls_score best;
    gor_real best_cost;  /* = best.soft, comodità */
    int *best_state;     /* buffer del chiamante, n int */
    long long steps;     /* passi totali (tutte le partenze) */
    int starts;
} gor_ls_result;

/* Risolve a partire da `initial` (n int). res->best_state deve puntare a un
 * buffer di n int del chiamante. Ritorna GOR_OK o un gor_status. */
int gor_ls_solve(const gor_ls_config *cfg, const int *initial, gor_ls_result *res);

/* ------------------------------------------------------------------------- */
/* Costruttive per permutazione (richiedono dist)                             */
/* ------------------------------------------------------------------------- */

/* Nearest neighbour da `start` (0 = primo elemento). */
int gor_ls_construct_nearest(int n, gor_ls_dist_fn dist, void *ctx, int start,
                             int *perm);
/* Cheapest insertion: parte da {0}, inserisce ogni nodo nella posizione di
 * minimo aumento di costo. */
int gor_ls_construct_cheapest_insertion(int n, gor_ls_dist_fn dist, void *ctx,
                                        int *perm);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_LS_H */
