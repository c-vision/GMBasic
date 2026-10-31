/* ============================================================================
 * gmb_dscope.h — Scope dinamico BASIC (`DYNAMIC`), tipo Clipper/xBase.
 *
 * Catena di frame per-task: un frame è spinto all'ingresso di ogni procedura
 * (SUB/FUNCTION) e rimosso all'uscita. `DYNAMIC x = v` definisce x nel frame
 * corrente (shadowing delle x esterne); la lettura cerca dal frame più interno
 * verso l'esterno (cascata); la scrittura aggiorna il frame più interno che
 * definisce il nome, altrimenti definisce nel frame corrente (top).
 *
 * Semantica canonica: Guide/Variables-Scope-Spec.md §2.5 (Visibilità a cascata,
 * Shadowing, Ripristino automatico). Condiviso da VM/C e C99Native
 * (runtime C unico, AGENTS §0bis).
 *
 * Per-task: lo stack è thread-local (__thread). Un task GPM gira in un solo
 * worker alla volta; per il codice a singolo thread (tutti i sample) equivale a
 * per-task. Vedi nota in gmb_dscope.c.
 *
 * Ownership: declare/set COPIANO il valore (le stringhe sono duplicate e
 * liberate al pop/sovrascrittura); get restituisce un PRESTITO valido finché il
 * frame non viene rimosso/sovrascritto (uso immediato in espressione).
 * ============================================================================ */
#ifndef GMB_DSCOPE_H
#define GMB_DSCOPE_H

#include "gmb_runtime.h"

/* Crea/azzera la catena (un frame base). Idempotente. */
void gmb_dscope_reset(void);

/* Ingresso/uscita di una procedura: spinge/rimuove un frame. */
void gmb_dscope_push(void);
void gmb_dscope_pop(void);

/* Numero di frame correnti (>= 1 dopo il primo uso, 0 dopo reset senza uso). */
int gmb_dscope_depth(void);

/* DYNAMIC x = v: definisce (o sovrascrive) x nel frame CORRENTE (top). */
void gmb_dscope_declare(const char* name, gmb_val_t v);

/* x: cerca dal top verso l'esterno. 1 e *out = prestito se definito, 0 altrimenti. */
int gmb_dscope_get(const char* name, gmb_val_t* out);

/* Nome definito? (ricerca dal top verso l'esterno). */
int gmb_dscope_has(const char* name);

/* x = v: aggiorna il frame più interno che definisce x; se assente, definisce nel top. */
void gmb_dscope_set(const char* name, gmb_val_t v);

/* Accessor TIPIZZATI per il codegen C (il tipo statico del riferimento DYNAMIC è noto
 * al compilatore): se il nome è assente o di tipo diverso → default (""/0). Copie. */
gmb_val_t    gmb_dscope_get_val(const char* name);
gmb_string_t gmb_dscope_get_string(const char* name);
gmb_double_t gmb_dscope_get_double(const char* name);
gmb_int_t    gmb_dscope_get_int(const char* name);

#endif /* GMB_DSCOPE_H */
