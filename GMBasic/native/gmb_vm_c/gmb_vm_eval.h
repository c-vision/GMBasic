/* ============================================================================
 * gmb_vm_eval.h — EVAL / & a runtime nella VM/C: compilatore C VERO.
 * Il frammento BASIC viene compilato (gmb_compile_snippet) contro le tabelle di
 * simboli del chunk ospite e UNITO al chunk, poi eseguito inline nella task.
 * Nessun interprete di espressioni separato.
 * ============================================================================ */
#ifndef GMB_VM_EVAL_H
#define GMB_VM_EVAL_H

#include "gmb_gpm.h"
#include "gmb_chunk.h"
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gmb_task;

/* 0 = ok. `as_stmt`=1 esegue statement (PRINT inclusi), 0 valuta un'espressione
 * e ne restituisce il valore in *out (malloc-gestito come gmb_val_t). */
int gmb_vm_eval_text(struct gmb_task* task, const char* src, int as_stmt, gmb_val_t* out);

/* Variante con AMBIENTE esterno (REQUIRE/AI, filtri): i nomi ignoti del frammento
 * vengono letti con `get` e riscritti con `set` (0 = trovato/ok, <0 = assente).
 * Con get/set NULL si usano i campi del record DBF attivo (gmb_vm_eval_text). */
typedef int (*gmb_eval_env_get_fn)(void* env, const char* name, gmb_val_t* out);
typedef int (*gmb_eval_env_set_fn)(void* env, const char* name, gmb_val_t in);
int gmb_vm_eval_text_env(struct gmb_task* task, const char* src, int as_stmt,
                         gmb_eval_env_get_fn get, gmb_eval_env_set_fn set,
                         void* env, gmb_val_t* out);

const char* gmb_vm_eval_last_error(void);
void gmb_vm_eval_cache_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_VM_EVAL_H */
