/* gmb_context.h — Context Engine (P2 AI-Runtime consolidato, asse B).
 * Budget, selector, policy, view curata. Parità col C# AiContextEngine.
 * Tutte le funzioni operano su registry per-agente (globali statiche in gmb_context.c). */
#ifndef GMB_CONTEXT_H
#define GMB_CONTEXT_H

#include "gmb_runtime.h"

/* comandi (scrittura) — ritornano 1 su successo, 0 su errore */
double gmb_ctx_set_budget(const char* agent, double chars);
double gmb_ctx_set_selector(const char* agent, const char* mode);
double gmb_ctx_set_policy(const char* agent, const char* mode);
double gmb_ctx_set_view(const char* agent, const char* text);

/* ── PARSEC §3.6 (lato AGENTE): context builder a SEGMENTI ──
 * Lo store di segmenti (kind: SYSTEM|HISTORY|TOOL|OTHER + testo) vive accanto alla view.
 * CREATE azzera; ADD appende; MAP stampa la mappa; TRIM riduce al budget; GC evince
 * (duplicati + HISTORY vecchia); COMMIT costruisce la view dai segmenti. */
double gmb_ctxseg_create(const char* agent);                               /* azzera i segmenti -> 1 */
double gmb_ctx_add(const char* agent, const char* kind, const char* text);  /* append -> 1 (0 se pieno) */
double gmb_ctx_map(const char* agent);                                     /* stampa la mappa (report) -> 1 */
double gmb_ctx_trim(const char* agent);                                    /* drop fino al budget -> #rimossi */
double gmb_ctx_gc(const char* agent);                                      /* evict duplicati+vecchi -> #rimossi */
double gmb_ctx_commit(const char* agent);                                  /* view = segmenti -> 1 (0 policy err) */

/* PARSEC §3.6 — cache con invalidazione per VERSION HASH (coordinamento context-cache ↔ prefix KV):
 * il hash della view curata è la chiave della prefix-cache KV; `valid` confronta con una revisione. */
gmb_string_t gmb_ctx_hash(const char* agent);                              /* FNV-1a hex della view */
double       gmb_ctx_valid(const char* agent, const char* rev);            /* 1 se "rev="+hash == rev */

/* funzioni di lettura */
double       gmb_ctx_used(const char* agent);
double       gmb_ctx_budget(const char* agent);
double       gmb_ctx_overflow(const char* agent);
/* Policy corrente (compact|truncate|error) — per lo stop del run di AI WORKER LOOP. */
const char*  gmb_ctx_get_policy(const char* agent);
gmb_string_t gmb_ctx_view(const char* agent);

#endif