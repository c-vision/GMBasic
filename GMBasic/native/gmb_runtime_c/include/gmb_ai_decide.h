/* gmb_ai_decide.h — contratto del decision layer (JEV/SystemOne) per le gambe C.
 *
 * Mirror C di GMBasicSharp/common/AI/Decision: tipi, 24 slot canonici e provider
 * deterministici (Mock/Rules). Il core di inferenza (logits + softmax calibrata) è in
 * LLMEnv (ABI v8 CAP_DECIDE); qui c'è solo il contratto/policy/provider, come per IL.
 *
 * Regola: una decisione produce EVIDENZA (value/probability/confidence), mai side effect.
 * `confidence`/`calibrated` sono opzionali: quando non disponibili restano a 0 con has_*=0
 * (il JSON li rende null) e NON vanno mai copiati da probability.
 */
#ifndef GMB_AI_DECIDE_H
#define GMB_AI_DECIDE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GMB_DECISION_NOUL = 0,     /* false/true */
    GMB_DECISION_CHOICE = 1,   /* A..P (o label utente) */
    GMB_DECISION_SCORE = 2     /* 0..5 */
} gmb_decision_kind_t;

#define GMB_DECISION_MAX_OPTIONS 16

typedef struct {
    const char* id;         /* identificatore stabile dello slot */
    const char* label;      /* etichetta/testo (NULL = id) */
    int token_id;           /* token singolo risolto dal tokenizer; -1 = non risolto */
    float bias;             /* bias dello slot (scoring_head.json); 0 = neutro */
    float temperature;      /* T per-kind (>0; <=0 = 1) */
} gmb_decision_option_t;

typedef struct {
    gmb_decision_kind_t kind;
    const char* option_id;  /* id dell'opzione scelta */
    const char* value;      /* valore tipato come testo: "true"/"false", "A", "3"... */
    double probability;     /* 0..1 */
    double confidence; int has_confidence;
    double calibrated; int has_calibrated;
} gmb_decision_result_t;

typedef struct gmb_decision_provider gmb_decision_provider_t;

/* Provider deterministici. mock_option_id NULL = prima opzione del dominio. */
gmb_decision_provider_t* gmb_decision_provider_mock(const char* option_id, double probability);
gmb_decision_provider_t* gmb_decision_provider_rules(void);
void gmb_decision_provider_free(gmb_decision_provider_t* provider);

/* Valuta `state` sul dominio di opzioni. Ritorna 1 = risultato scritto, 0 = nessun match
 * (solo regole), -1 = errore (provider/opzioni invalide: NOUL != 2 opzioni, id duplicati,
 * n_options fuori 1..16). `out->option_id/value` puntano a `options[i]` (vita del chiamante). */
int gmb_decision_evaluate(const gmb_decision_provider_t* provider, const char* state,
                          gmb_decision_kind_t kind,
                          const gmb_decision_option_t* options, int n_options,
                          gmb_decision_result_t* out);

/* 24 slot canonici (spec upstream ms-swift): NOUL 2, SCORE 6, CHOICE 16. */
extern const gmb_decision_option_t gmb_decision_slots_noul[2];
extern const gmb_decision_option_t gmb_decision_slots_score[6];
extern const gmb_decision_option_t gmb_decision_slots_choice[16];

/* JSON compatto del risultato (per la superficie BASIC/CLI):
 * {"kind":"NOUL","option":"true","value":"true","probability":0.93,
 *  "confidence":null,"calibrated":null}
 * Ritorna la lunghezza scritta (senza NUL) o -1 se il buffer è troppo piccolo. */
int gmb_decision_result_json(const gmb_decision_result_t* result, char* out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_AI_DECIDE_H */
