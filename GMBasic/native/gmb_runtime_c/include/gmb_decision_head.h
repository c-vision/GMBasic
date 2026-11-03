/* gmb_decision_head.h — testa di scoring condivisa JEV/SystemOne (T3).
 *
 * Carica `scoring_head.json` (bias per i 24 slot canonici, T per kind, T globale,
 * verbalizer_ids) e applica la formula del piano:
 *
 *     z_k = (logprob_k + bias_slot(k)) / (head_T * T_kind)
 *     softmax MASCHERATA sulle sole opzioni della domanda (i pad non esistono qui: n = opzioni)
 *     confidence = p_top − media(resto), clampata in [0,1]
 *
 * Vive nel runtime C condiviso (Apache): la usano i provider C che NON passano dal motore
 * (es. HTTP/TypeSafe) e il provider locale per leggere l'artefatto. Il motore LLMEnv resta
 * la fonte del readout quando c'è una sessione MLX.
 */
#ifndef GMB_DECISION_HEAD_H
#define GMB_DECISION_HEAD_H

#include <stddef.h>

#include "gmb_ai_decide.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_DECISION_HEAD_SLOTS 24

typedef struct {
    float bias[GMB_DECISION_HEAD_SLOTS];      /* 0 = neutro */
    int token_id[GMB_DECISION_HEAD_SLOTS];    /* -1 = non risolto nell'artefatto */
    float head_T;                             /* temperatura globale (>0) */
    float T_kind[3];                          /* noul, choice, score (>0) */
    int calibrated;                           /* 1 = artefatto presente (calibrated != false) */
} gmb_decision_head_t;

/* Identità: head_T=1, T_kind=1, bias=0, token_id=-1, calibrated=0. */
void gmb_decision_head_init(gmb_decision_head_t* head);

/* Carica `scoring_head.json` dalla cartella modello.
 * 1 = artefatto caricato, 0 = assente, -1 = presente ma illeggibile/malformato. */
int gmb_decision_head_load(const char* model_dir, gmb_decision_head_t* head);

/* Carica l'artefatto da un PATH ESPLICITO (config provider `decideCalibration`). Stessi
 * codici di ritorno di `gmb_decision_head_load`. */
int gmb_decision_head_load_file(const char* path, gmb_decision_head_t* head);

/* Slot canonico 0..23 dell'opzione (prima id, poi label), o -1 se non canonico. */
int gmb_decision_head_slot(const gmb_decision_option_t* option);

/* Applica bias/T + softmax e calcola argmax/confidence.
 * `logprob` e `options` hanno `n` elementi; `out_prob` ne ha `n`.
 * Ritorna 0, oppure -1 su parametri invalidi. */
int gmb_decision_head_apply(const gmb_decision_head_t* head, gmb_decision_kind_t kind,
                            const float* logprob, const gmb_decision_option_t* options, int n,
                            float* out_prob, int* out_argmax, double* out_confidence);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DECISION_HEAD_H */
