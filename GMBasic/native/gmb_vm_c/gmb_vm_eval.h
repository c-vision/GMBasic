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

const char* gmb_vm_eval_last_error(void);
void gmb_vm_eval_cache_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_VM_EVAL_H */
