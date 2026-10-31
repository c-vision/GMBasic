/* ============================================================================
 * gmb_vm_expr_compile.h — Fase 8: compile AST &/EVAL → snippet opcode VM.
 * Il dispatcher di gmb_vm_core.c non viene toccato: lo snippet gira su un
 * interprete locale (stessi numeri di opcode) con const pool proprio.
 * ============================================================================ */
#ifndef GMB_VM_EXPR_COMPILE_H
#define GMB_VM_EXPR_COMPILE_H

#include "gmb_gpm.h"
#include "gmb_chunk.h"
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 0 = eseguito (expr: valore sullo stack se result==NULL; se result!=NULL
 *     il valore è convertito e rimosso). Stmt: stack ripristinato.
 * 1 = non compilabile → il chiamante fa tree-walk.
 * -1 = errore runtime dello snippet. */
int gmb_vm_expr_try_compiled(gmb_task_t* task, const gmb_chunk_t* chunk,
                             const char* src, int as_stmt, gmb_val_t* result);

#ifdef __cplusplus
}
#endif

#endif /* GMB_VM_EXPR_COMPILE_H */
