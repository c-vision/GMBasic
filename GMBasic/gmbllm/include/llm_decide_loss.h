/* llm_decide_loss.h — loss decisionali JEV (T4, PUL).
 *
 * Loss e gradiente rispetto ai LOGITS delle opzioni (il trainer MLX fornisce i logits del
 * forward; qui c'è solo la matematica, nessuna dipendenza MLX):
 *   CE       masked cross-entropy sulle opzioni valide
 *   BRIER    Brier multi-classe
 *   RPS      Ranked Probability Score (loss ordinale sulle CDF, per SCORE)
 *   CE_RPS   CE + w·RPS (proper scoring rule mista)
 * Label smoothing opzionale sul target one-hot. `mask` esclude le opzioni pad dalla
 * normalizzazione (softmax SOLO sulle opzioni valide, come il readout del motore).
 */
#ifndef LLM_DECIDE_LOSS_H
#define LLM_DECIDE_LOSS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLM_DECIDE_LOSS_MAX 16

typedef enum {
    LLM_DECIDE_CRIT_CE = 0,
    LLM_DECIDE_CRIT_BRIER = 1,
    LLM_DECIDE_CRIT_RPS = 2,
    LLM_DECIDE_CRIT_CE_RPS = 3,
} llm_decide_criterion_t;

typedef struct {
    int n_options;                    /* 1..16 */
    int label;                        /* indice dell'opzione corretta */
    unsigned mask;                    /* bit k = opzione valida (0 = tutte) */
    float logits[LLM_DECIDE_LOSS_MAX];
} llm_decide_loss_sample_t;

typedef struct {
    llm_decide_criterion_t criterion;
    double label_smoothing;           /* 0 = one-hot (clamp 0..0.5) */
    double rps_weight;                /* usato solo da CE_RPS (default 0.5 se <=0) */
} llm_decide_loss_opts_t;

typedef struct {
    double loss;
    double grad[LLM_DECIDE_LOSS_MAX]; /* dLoss/dlogit[k]; 0 per opzioni mascherate */
} llm_decide_loss_result_t;

/* Ritorna 0, oppure -1 su input invalido (n/label/mask/criterio). */
int llm_decide_loss_compute(const llm_decide_loss_sample_t* sample,
                            const llm_decide_loss_opts_t* opts,
                            llm_decide_loss_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* LLM_DECIDE_LOSS_H */
