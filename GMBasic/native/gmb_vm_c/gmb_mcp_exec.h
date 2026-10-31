/* native/gmb_vm_c/gmb_mcp_exec.h
 *
 * MCP nel mondo VM/C: handler dei builtin MCP (AI MCP REGISTER TOOL / SERVE / STOP) e
 * EXECUTOR che invoca la PROCEDURA BASIC di un tool MCP su richiesta del server
 * (runtime C condiviso, gmb_mcp_server.c).
 *
 * Perché l'executor vive qui e non nel runtime: il runtime condiviso NON conosce
 * l'engine di esecuzione (§0bis), la VM/C sì. La regola GPM (§0ter) è rispettata:
 *   - stdio  → chiamata SINCRONA annidata sul task che esegue AI MCP SERVE (nessun
 *              thread esterno: il loop stdio gira dentro il tick di quel task);
 *   - http   → task NUOVO accodato nello scheduler GPM (i worker eseguono la procedura),
 *              il thread HTTP attende e legge il risultato — stesso schema di
 *              gmb_vm_http_handler.
 */
#ifndef GMB_MCP_EXEC_H
#define GMB_MCP_EXEC_H

#include "gmb_vm_core.h"

/* Handler per la tabella gmb_bf_impl di gmb_builtin.c. */
void vmc_mcp_bf_register_tool(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                              gmb_builtin_out_t* bout, gmb_task_t* task);
void vmc_mcp_bf_serve(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                      gmb_builtin_out_t* bout, gmb_task_t* task);
void vmc_mcp_bf_stop(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                     gmb_builtin_out_t* bout, gmb_task_t* task);

/* Self-Consistency / Semantic Entropy (func_code 3538-3540): implementazione in
 * gmb_uncertainty_exec.c, che chiama il runtime C condiviso (§0bis). */
void vmc_bf_uncertainty(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                        gmb_builtin_out_t* bout, gmb_task_t* task);

/* Installa l'executor MCP sul runtime condiviso (idempotente).
 * `serve_task` = task che esegue AI MCP SERVE, usato per la chiamata sincrona in
 * modalità stdio; `stdio` = 1 se il serving è stdio (chiamata sullo stesso thread). */
void vmc_mcp_install(gmb_vm_t* vm, gmb_task_t* serve_task, int stdio);

#endif /* GMB_MCP_EXEC_H */
