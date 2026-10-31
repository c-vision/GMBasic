/* gmb_resolver.h — Resolver C: risolve il TIPO di un nome (eventualmente puntato `a.b.c`) usando
 * la symbol table (variabili/tipi) + il type registry (CLASS/UDT + campi). Fondamento di §6 (il
 * linker C risolve tipi/membri/firme come il C# `Linker.InferTypes`). Compilatore C autonomo.
 */
#ifndef GMB_RESOLVER_H
#define GMB_RESOLVER_H

#include "gmb_symtab.h"
#include "gmb_types.h"
#include <stddef.h>

/* Risolve il tipo dell'espressione-nome `dotted` (es. "p", "p.x", "a.b.c") e lo scrive in `out`
 * (cap). Ritorna 0 su successo, -1 se non risolvibile. `base_out` (opz., cap bcap) riceve il tipo
 * della BASE (per `p.x`, il tipo di `p`) se != NULL. */
int gmb_resolve_name_type(const gmb_symtab_t* st, const gmb_typereg_t* tr,
                          const char* dotted, char* out, size_t cap,
                          char* base_out, size_t bcap);

/* Verifica un accesso a membro `obj.membro`: ritorna 0 se il membro esiste sul tipo di `obj`,
 * -1 altrimenti (con `err` riempito). */
int gmb_resolve_member(const gmb_symtab_t* st, const gmb_typereg_t* tr,
                       const char* obj, const char* member, char* err, size_t ecap);

/* Verifica l'ARITY di una chiamata `name(args_n)` rispetto ai simboli (FUNCTION/SUB). Ritorna 0 se
 * il nome non e' una procedura nota o l'arity combacia; -1 se mismatch (con `err`). */
int gmb_resolve_call_arity(const gmb_symtab_t* st, const char* name, int argc, char* err, size_t ecap);

/* Risoluzione OVERLOAD: tra i simboli PROCEDURE/FUNCTION con il nome dato, sceglie quello
 * la cui arità (parametri della firma) combacia con `argc`. Scrive l'id in `out_id`.
 * Ritorna 0 se trovato, -1 altrimenti (con `err`). */
int gmb_resolve_overload(const gmb_symtab_t* st, const char* name, int argc, int* out_id, char* err, size_t ecap);

/* Cerca la direttiva IMPORT per `alias` (o nome-ns) nell'AST e scrive il namespace reale in `out`. */
int gmb_resolve_import(const gmb_stmt_t** stmts, int n, const char* alias, char* out, size_t cap);

/* Come sopra, ma verifica che `member` sia nell'elenco selettivo di IMPORT [alias].
 * Se l'IMPORT non ha membri, tutti sono visibili. Ritorna 0 + ns, o -1. */
int gmb_resolve_import_member(const gmb_stmt_t** stmts, int n, const char* alias, const char* member, char* out, size_t cap);

#endif /* GMB_RESOLVER_H */
