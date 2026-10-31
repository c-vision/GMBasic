/* gmb_worker.h — AI WORKER + AI STATE (P3 AI-Runtime asse C). */
#ifndef GMB_WORKER_H
#define GMB_WORKER_H

#include "gmb_runtime.h"

double       gmb_worker_new(const char* name, const char* opts);
double       gmb_worker_run(const char* name);
/* AI WORKER "<TASK>" — INVOCAZIONE PULITA (C1, P3): vista curata = SOLO memoria esterna del
 * run (workspace: task, plan.md, notes.md, solution, state), normalizzata dal Context Engine
 * (chiave "worker:<name>", budget del worker) e inviata a gmb_llm_complete.
 * Ritorna 1 ok · 0 rifiutata dal budget (policy error: rescue, last_error) · -1 nessun worker
 * corrente · -2 errore provider (sentinella "Error: nessun provider LLM configurato").
 * Speculare a AiWorkerRuntime.WorkerInvoke (C#). */
double       gmb_worker_invoke(const char* task);
/* AI WORKER LOOP "<TASK>"[, rounds] — LOOP ISOLATO multi-round: vista curata aggiornata dal round
 * precedente (memoria esterna + BEST) e BUDGET CUMULATIVO del run (il budget del worker è il tetto
 * dell'intero run; ogni round vede `budget - speso` come cap).
 * Ritorna 1 completati · 0 budget del run esaurito (rescued, last_error) · -1 nessun worker
 * corrente · -2 errore provider. Speculare a AiWorkerRuntime.WorkerLoop (C#). */
/* `require` (opzionale, "" = nessun audit) è l'espressione dell'AUDIT indipendente valutata con
 * RESULT$ = risposta del round (gmb_verify_require): se è soddisfatta il loop si ferma
 * verificato (1, state/verified=1); se nessun round la soddisfa → -3 (rescued, last_error
 * "audit failed after N round(s)"), con l'esito dell'audit in state/audit_error per il REWORK. */
double       gmb_worker_loop(const char* task, double rounds, const char* require);
/* AI WORKER MANAGE [, cycles] [REQUIRE "<expr>"] — MANAGER LOOP (P4): prende dal task graph il
 * prossimo task lavorabile (dipendenze `done`, ordine alfabetico), lo esegue col worker (vista
 * curata + budget cumulativo) e lo marca `done` se l'auditor REQUIRE lo accetta; se lo rifiuta
 * registra il DISSENSO (state/dissent + state/dissents) e ritenta.
 * `plan` (opzionale, "" = intero task graph) limita il set di lavoro ai task del piano: il piano deve
 * esistere ed essere APPROVATO (`AI PLAN APPROVE`) → altrimenti esito -5.
 * Ritorna 1 fatto · 0 budget del run · -1 nessun worker · -2 errore provider · -3 cicli esauriti
 * con task pendenti · -4 deadlock di dipendenze · -5 piano non approvato/inesistente.
 * Ultimo esito in state/manage_exit. */
double       gmb_worker_manage(double cycles, const char* require, const char* plan);
double       gmb_state_save(const char* worker, const char* key, const char* value);
gmb_string_t gmb_state_load(const char* worker, const char* key);
double       gmb_worker_rounds(void);
double       gmb_worker_rescued(void);
double       gmb_worker_spend(void);

#endif