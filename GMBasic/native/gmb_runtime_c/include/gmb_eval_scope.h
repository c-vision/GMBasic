/* ============================================================================
 * gmb_eval_scope.h — tipi condivisi per i frammenti BASIC a runtime (& / EVAL).
 *
 * Il VECCHIO valutatore tree-walk (gmb_expr.c) è stato RIMOSSO: i frammenti sono
 * compilati dal COMPILATORE C (gmb_compile_snippet) ed eseguiti dalla VM/C, in
 * processo oppure via libgmb_eval (dlopen) nei contesti senza VM.
 * Qui restano solo:
 *  - i typedef dei callback (get/set/call) usati dagli emitter e dal runtime;
 *  - lo SCOPE transiente S_eval (gmb_eval_scope.c), usato dall'emitter C99 per
 *    seed/write-back delle locali del chiamante;
 *  - i codici GMB_EXPR_OK/ERR usati dalle API che li espongono (BSP, DB).
 * ============================================================================ */
#ifndef GMB_EXPR_H
#define GMB_EXPR_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_EXPR_OK          0
#define GMB_EXPR_ERR_PARSE  -1
#define GMB_EXPR_ERR_EVAL   -2
#define GMB_EXPR_ERR_UNDEF  -3

typedef int (*gmb_expr_get_fn)(void* env, const char* name, gmb_val_t* out);
typedef int (*gmb_expr_set_fn)(void* env, const char* name, gmb_val_t in);
/* Funzione utente ospite: 0 = ok (out valorizzato), ≠0 = sconosciuta. */
typedef int (*gmb_expr_call_fn)(void* env, const char* name,
                                const gmb_val_t* args, int nargs, gmb_val_t* out);

/* ---- S_eval: scope transiente per &/EVAL (condiviso VM/C e C99) ----
 * Ogni esecuzione EVAL gira in un nuovo scope figlio (catena verso il parent);
 * DIM/LET/assegnazione-su-ignota creano qui e muoiono a fine eval (mai leak
 * nei globali/host); l'assegnazione implicita cerca scope→parent→backend e
 * scrive in place. Frame opaco, allocato per-esecuzione (GPM-safe). */
typedef struct gmb_expr_scope gmb_expr_scope_t;
gmb_expr_scope_t* gmb_expr_scope_push(gmb_expr_scope_t* parent);
void gmb_expr_scope_pop(gmb_expr_scope_t* s);
/* Cerca nella catena (corrente → parent): 1 trovato (out valorizzato), 0 no. */
int gmb_expr_scope_get(gmb_expr_scope_t* s, const char* name, gmb_val_t* out);
/* Crea/aggiorna NEL frame corrente (copia la stringa, alias per il resto). */
void gmb_expr_scope_set(gmb_expr_scope_t* s, const char* name, gmb_val_t v);
/* Aggiorna in place dov'è definito nella catena: 1 scritto, 0 assente. */
int gmb_expr_scope_update(gmb_expr_scope_t* s, const char* name, gmb_val_t in);

#ifdef __cplusplus
}
#endif

#endif /* GMB_EXPR_H */
