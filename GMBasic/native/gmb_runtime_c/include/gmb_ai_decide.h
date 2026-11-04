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
#define GMB_DECISION_ERR_CAP 512

typedef struct {
    const char* id;         /* identificatore stabile dello slot */
    const char* label;      /* etichetta/testo (NULL = id) */
    int token_id;           /* token singolo risolto dal tokenizer; -1 = non risolto */
    float bias;             /* bias dello slot (scoring_head.json); 0 = neutro */
    float temperature;      /* T per-kind (>0; <=0 = 1) */
} gmb_decision_option_t;

/* Dominio RISOLTO di un kind ("NOUL", "CHOICE", "CHOICE:a|b", "SCORE", "SCORE:0:10"):
 * `options` punta a `custom` (o agli slot canonici) e resta valido finché vive `d`. Usato dal
 * dispatcher e dal provider locale (batch con prefill condiviso): una sola risoluzione del
 * kind, nessuna copia della grammatica dei domini. */
typedef struct {
    gmb_decision_kind_t kind;
    const gmb_decision_option_t* options;
    int n_options;
    char labelbuf[512];
    char score_labels[11][4];
    gmb_decision_option_t custom[GMB_DECISION_MAX_OPTIONS];
} gmb_decision_domain_t;

/* Risolve `kind` nel dominio canonico/custom. 0 ok / -1 su kind non valido. */
int gmb_decision_resolve_domain(const char* kind, gmb_decision_domain_t* d);

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

/* Superficie BASIC v0: esegue il provider di default (Rules) sul dominio canonico del kind
 * ("NOUL"/"SCORE"/"CHOICE") e restituisce il JSON del risultato in heap (libera con free).
 * NULL su kind invalido/errore. Stessa forma del JSON C# (IlRuntime.AiDecide). */
char* gmb_ai_decide_run_json(const char* state, const char* kind, const char* provider);

/* Catena di fallback ESPLICITA (superficie `FALLBACK "p1","p2"`): prova `provider` e poi i
 * nomi in `fallback_csv` (separatore `|`). Il primo risultato non-errore vince; se tutti
 * falliscono ritorna `{"error":"tutti i provider falliti: …"}`. Nessun fallback implicito. */
char* gmb_ai_decide_run_json_chain(const char* state, const char* kind,
                                   const char* provider, const char* fallback_csv);

/* Provider locale MLX (JEV/SystemOne), implementato in `gmb_ai_decide_local.c`: vive nella
 * dylib `gmb_runtime_ai` (non-Windows, che linka LLMEnv) o nel core (Windows) ed è invocato
 * dal dispatcher tramite il gateway. Usa il modello attivo del ruolo completion, il
 * chat_template reale, il tokenizer e l'ABI v8 CAP_DECIDE; applica `scoring_head.json` se
 * presente. `out->option_id/value` puntano a `options[i]` (vita del chiamante).
 * Ritorna 1 = risultato, -1 = errore con motivo in `err` (mai un fallback silenzioso). */
int gmb_ai_decide_local_run(const char* state, const char* kind,
                            const gmb_decision_option_t* options, int n_options,
                            gmb_decision_result_t* out, char* err, size_t errcap);

/* Match deterministico di una RISPOSTA GENERATA contro il dominio (usato dall'adapter
 * locale e da quello remoto): esatto (id/label) → numero → parola intera con label più lunga
 * ("1" non cattura "10") → slot A..P. Ritorna l'indice, o -1 se nessuna opzione corrisponde. */
int gmb_decision_match_text(const gmb_decision_option_t* options, int n_options, const char* text);

/* P6 — provider ADAPTER (generativo): usa il MODELLO LOCALE per GENERARE la risposta e
 * riconosce l'opzione scelta (id/label esatti → numero → parola intera → slot A..P).
 * È il corrispettivo C di `AdapterDecisionProvider` (managed): per i modelli/endpoint senza
 * logprob. La probabilità è 1/0 (adapter, non calibrato); nessun match = errore esplicito. */
int gmb_ai_decide_local_adapter_run(const char* state, const char* kind,
                                    const gmb_decision_option_t* options, int n_options,
                                    gmb_decision_result_t* out, char* err, size_t errcap);

/* Batch del provider locale: N domini SULLO STESSO stato con prefill/KV CONDIVISO. Il
 * dispatcher risolve i domini (`gmb_decision_resolve_domain`) e il provider costruisce i
 * prompt, calcola il prefisso comune di token, lo prefill-a UNA volta e lo salva nella
 * prefix-cache del motore; ogni domanda riusa la KV (`session_append_auto_prefix`). Se il
 * prefisso è corto o il motore non espone la prefix-cache, degrada al prefill lineare —
 * stessi risultati. Ritorna 1 = N risultati in `out`, -1 = errore in `err`. */
int gmb_ai_decide_local_run_group(const char* state, const gmb_decision_domain_t* domains,
                                  int n_domains, gmb_decision_result_t* out,
                                  char* err, size_t errcap);

/* Ponte STABILE core → dylib AI: `gmb_ai_decide_local.c` vive in `gmb_runtime_ai`, che risolve
 * i simboli del core a load-time. Chiamare direttamente gli interni di `gmb_ai.c`/`gmb_paths.c`
 * li lascerebbe fuori dal link dei programmi che usano solo DECIDE (dead-strip degli oggetti
 * non referenziati) → salto a simbolo nullo al primo uso. Questi due wrapper garantiscono che
 * l'oggetto del core sia linkato e che la dylib abbia un entry point stabile. */
const char* gmb_ai_decide_active_model_path(void);

/* 1 se la decisione in corso usa un provider `%DECIDE` di kind `verbalizer` (profilo da applicare). */
int gmb_ai_decide_profile_active(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_AI_DECIDE_H */
