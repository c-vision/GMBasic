/* llm_decide_calib.h — calibrazione a temperatura 2 stadi della testa JEV (T5, PUL).
 *
 * Fit di `head_T` globale e `T_kind` (noul/choice/score) sulla distribuzione delle opzioni:
 *
 *     p_k = softmax_k( logprob_k / (head_T * T_kind[kind]) )     (bias = 0: T5 fitta solo T)
 *
 * Obiettivo: Brier multi-classe (proper scoring rule), solver deterministico (griglia +
 * golden section, nessuna dipendenza esterna). Soglia 10 domande: sotto soglia T=1 (identità)
 * e kind con <10 campioni restano a T=1. Le metriche out-of-fold sono 2-fold (indici pari/dispari,
 * nessun leakage); il fit finale usa tutte le domande. L'artefatto prodotto è
 * `scoring_head.json`, lo stesso letto da `gmb_decision_head_load` (T3) e dal provider locale.
 *
 * Confine PUL: produce un artefatto, non lo serve. Il runtime Apache lo consuma soltanto.
 */
#ifndef LLM_DECIDE_CALIB_H
#define LLM_DECIDE_CALIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLM_DECIDE_MAX_OPTIONS 16

typedef struct {
    int kind;        /* 0 = noul, 1 = choice, 2 = score */
    int n_options;   /* 1..16 */
    int label;       /* indice dell'opzione corretta (0..n-1) */
    float logprob[LLM_DECIDE_MAX_OPTIONS];
} llm_decide_sample_t;

typedef struct {
    float head_T;
    float T_kind[3];
    float bias[24];            /* T4: bias per i 24 slot canonici (0 = neutro) */
    int fit_bias;              /* 1 = il fit ha incluso i bias */
    int bias_slots;            /* slot con almeno T_THRESHOLD campioni (bias fittato) */
    int n_questions;
    int n_kind[3];
    int cv_folds;              /* 2 oppure 1 (sotto soglia) */
    int identity;              /* 1 = T=1 per soglia */
    float accuracy;            /* argmax: NON cambia con T */
    double nll_before, nll_after;
    double brier_before, brier_after;
    double ece_before, ece_after;
    double brier_bias, ece_bias;   /* dopo il fit dei bias (se fit_bias) */
    double brier_oof_before, brier_oof_after;
    double ece_oof_before, ece_oof_after;
} llm_decide_calib_result_t;

/* Fit 2 stadi. Se `out_path` non è NULL scrive il manifest `scoring_head.json`.
 * Ritorna 0, oppure -1 su input invalido (n<1, kind fuori 0..2, n_options fuori 1..16,
 * label fuori range). */
int llm_decide_calib_fit(const llm_decide_sample_t* samples, int n,
                         llm_decide_calib_result_t* result, const char* out_path);

/* Come `fit`, ma con flag `fit_bias` (T4): dopo la temperatura, fit dei 24 bias per slot
 * (solo slot con >= T_THRESHOLD campioni; altri restano 0) minimizzando lo stesso Brier.
 * Il manifest include `bias` e le metriche `brier_with_bias`/`ece_with_bias`. */
int llm_decide_calib_fit_ex(const llm_decide_sample_t* samples, int n,
                            llm_decide_calib_result_t* result, const char* out_path,
                            int fit_bias);

#ifdef __cplusplus
}
#endif

#endif /* LLM_DECIDE_CALIB_H */
