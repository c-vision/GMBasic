/* gmb_decision_clm.h — provider `clm` del primitivo `AI DECIDE` (JEV/SystemOne).
 *
 * CLM (Contrastive Language Models, Apache-2.0) NON è una primitiva nuova: è un PROVIDER del
 * contratto SystemOne (stesso `POST /v1/systemone` di TypeSafe/Jev). Questo modulo è C99 puro,
 * portabile, senza MLX né rete: contiene
 *   - il rendering in PROSA dello stato (`to_text`/`state_text`: le head sono addestrate su
 *     prosa, mai su JSON);
 *   - il wire `/v1/systemone` (costruzione richiesta + parsing risposta);
 *   - la head di proiezione (MLP 4096→width→proj, pesi safetensors) e il punteggio
 *     `softmax(min(exp(logit_scale),100) · cos(state_head(s), action_head(c)))`.
 * Il recupero degli embedding (pooling LAST-TOKEN dell'encoder) è iniettato dal chiamante
 * tramite `gmb_clm_embed_fn`: l'encoder resta nel motore, il core non dipende da lui.
 */
#ifndef GMB_DECISION_CLM_H
#define GMB_DECISION_CLM_H

#include <stddef.h>

#include "gmb_ai_decide.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Prosa ───────────────────────────────────────────────────────────────────────────────── */

/* Stato → testo. Una stringa NON-JSON (o JSON malformato) torna invariata; un oggetto JSON
 * diventa righe `chiave: valore`, un array righe `- elemento`, i contenitori annidati sono
 * indentati di 2 spazi (ordine dei campi preservato); scalari: stringhe grezze, numeri e
 * true/false/null come nel JSON. Nessun newline finale. Heap: libera con free(). */
char* gmb_clm_to_text(const char* state);

/* Testo visto dallo state head: `stato` + "\n\n" + `istruzioni` (una sola delle due se l'altra
 * è vuota). `state` è reso con gmb_clm_to_text. Heap: libera con free(). */
char* gmb_clm_state_text(const char* state, const char* instructions);

/* ── Wire /v1/systemone ──────────────────────────────────────────────────────────────────── */

/* Corpo della richiesta per UNA domanda (id "q"). `kind` = NOUL|CHOICE|SCORE (forma canonica),
 * `options` = dominio risolto (choice: id→label, score: livelli ordinati). Se `state` è un
 * oggetto/array JSON valido viene incorporato come JSON (così il server lo rende in prosa),
 * altrimenti come stringa. `instructions` può essere NULL/"". Heap: libera con free(). */
char* gmb_clm_build_request(const char* state, const char* kind,
                            const gmb_decision_option_t* options, int n_options,
                            const char* instructions);

/* Risposta → risultato. Accetta la risposta della domanda sia al livello superiore sia sotto
 * l'id ("q"). noul = P(true); choice = chiave + probabilities; score = livello atteso +
 * probabilities (si sceglie l'argmax). `out->option_id/value` puntano a `options[i]`.
 * 1 = ok, -1 = errore ESPLICITO in `err` (mai uno 0 silenzioso). */
int gmb_clm_parse_response(const char* body, gmb_decision_kind_t kind,
                           const gmb_decision_option_t* options, int n_options,
                           gmb_decision_result_t* out, char* err, size_t errcap);

/* confidence = max(0, min(1, p_top − media(resto))); 1.0 con meno di 2 opzioni. */
double gmb_clm_confidence(const float* p, int n);

/* ── Head di proiezione ──────────────────────────────────────────────────────────────────── */

typedef struct gmb_clm_head gmb_clm_head_t;

/* Carica la coppia di head da un safetensors con i tensori `state_head.*`, `action_head.*`
 * (inp.weight/bias, hidden.i.*, norms.i.*, out.weight/bias) e `logit_scale`. Dimensioni e
 * LayerNorm sono dedotti dalle shape; `activation` (gelu|relu|silu, default gelu) e `residual`
 * (default false) si leggono dal file gemello `.json` se presente. NULL + `err` su errore. */
gmb_clm_head_t* gmb_clm_head_load(const char* safetensors_path, char* err, size_t errcap);
void gmb_clm_head_free(gmb_clm_head_t* head);
int gmb_clm_head_hidden_size(const gmb_clm_head_t* head);      /* dim dell'embedding in ingresso */
int gmb_clm_head_projection_dim(const gmb_clm_head_t* head);

/* Embedding di un testo (pooling last-token dell'encoder). Scrive `dim` float; 0 = ok. */
typedef int (*gmb_clm_embed_fn)(const char* text, float* out, int dim, void* ctx);

/* Punteggio locale: embedding di (stato+domanda) e dei candidati → proiezioni L2-normalizzate →
 * softmax(scale·cos). Candidati: choice = label (o id); score = testo del livello; noul =
 * "false: <istruzioni>" / "true: <istruzioni>". `out->option_id/value` puntano a `options[i]`.
 * 1 = ok, -1 = errore esplicito. */
int gmb_clm_decide_local(const gmb_clm_head_t* head, gmb_clm_embed_fn embed, void* ctx,
                         const char* state, const char* instructions,
                         gmb_decision_kind_t kind,
                         const gmb_decision_option_t* options, int n_options,
                         gmb_decision_result_t* out, char* err, size_t errcap);

/* Passo puro (senza encoder): dato l'embedding grezzo dello stato e dei candidati, restituisce le
 * probabilità. `cands` = n × hidden float. 0 = ok, -1 = parametri invalidi. */
int gmb_clm_score(const gmb_clm_head_t* head, const float* state_emb, const float* cands,
                  int n, float* probs);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DECISION_CLM_H */
