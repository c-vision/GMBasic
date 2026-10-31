/* gmb_symtab.h — Symbol Table ID-based + SCOPE TREE del compilatore C (pari al C#
 * `common/Compilation/SymbolTable.cs`). Sostituisce la symbol table grossolana di gmb_linker.h
 * (name/kind{2}/decl_line/used) con: id stabili, 16 kind, namespace, container, tipo, firma,
 * scope, flags, is_exported. COMPILATORE C A SE' STANTE: solo C, nessuna dipendenza da C#.
 *
 * Architettura (Rev. C, specchio del C#): simbolo = INDICE in un array (O(1)); albero di Scope
 * (id/parent/owner) con mappa name -> lista di SymbolId; risoluzione dal piu' interno all'esterno.
 */
#ifndef GMB_SYMTAB_H
#define GMB_SYMTAB_H

#include "gmb_ast.h"
#include "gmb_semgraph.h"   /* gmb_sym_kind_t (16 kind) */

/* Flags di simbolo (bitmask). */
enum {
    GMB_SYMF_PRIVATE  = 1u << 0,
    GMB_SYMF_PUBLIC   = 1u << 1,
    GMB_SYMF_STATIC   = 1u << 2,
    GMB_SYMF_CONST    = 1u << 3,
    GMB_SYMF_PARAM    = 1u << 4,
    GMB_SYMF_BYREF    = 1u << 5,
    GMB_SYMF_ARRAY    = 1u << 6,
    GMB_SYMF_OPTIONAL = 1u << 7
};

typedef struct {
    int   id;                 /* indice stabile nella tabella */
    gmb_sym_kind_t kind;      /* 16 kind (specchio SemanticSymbolKind) */
    char* name;
    int   scope_id;           /* scope di dichiarazione */
    char* ns;                 /* namespace ("" = globale) */
    int   container_sym_id;   /* id del simbolo contenitore (-1 = nessuno) */
    char* type_name;          /* tipo (se noto) */
    char* signature;          /* firma (parametri, CSV) */
    int   decl_line, decl_col;
    unsigned flags;           /* GMB_SYMF_* */
    int   is_exported;
    gmb_stmt_t* decl_node;    /* nodo AST di dichiarazione (o NULL) */
} gmb_sym_t;

typedef struct {
    int id;
    int parent_id;            /* -1 = radice */
    int owner_sym_id;         /* simbolo che possiede lo scope (SUB/FUNCTION/...), -1 = nessuno */
    int* sym_ids;             /* id dei simboli dichiarati in questo scope */
    int  n_syms, cap_syms;
} gmb_scope_t;

typedef struct {
    gmb_sym_t*   syms;   int n_syms, cap_syms;
    gmb_scope_t* scopes; int n_scopes, cap_scopes;
    int cur_scope;            /* scope corrente */
    int* stack; int n_stack, cap_stack;   /* stack degli scope aperti */
} gmb_symtab_t;

void gmb_symtab_init(gmb_symtab_t* t);
void gmb_symtab_free(gmb_symtab_t* t);

/* Gestione scope. `owner` = id del simbolo che possiede lo scope (-1 se nessuno). Ritorna scope_id. */
int  gmb_symtab_push_scope(gmb_symtab_t* t, int owner);
void gmb_symtab_pop_scope(gmb_symtab_t* t);

/* Dichiara un simbolo nello scope corrente. Ritorna l'id (>=0) o -1. `parent_sym` = container. */
int  gmb_symtab_declare(gmb_symtab_t* t, gmb_sym_kind_t kind, const char* name, const char* ns,
                        const char* type_name, const char* signature, int line, int col,
                        unsigned flags, int is_exported, gmb_stmt_t* decl_node);

/* Risoluzione per nome dal piu' interno all'esterno. Ritorna l'id o -1. */
int  gmb_symtab_resolve(gmb_symtab_t* t, const char* name);
/* Cerca un simbolo per nome in TUTTI gli scope (per il resolver/grafo). */
int  gmb_symtab_find_any(gmb_symtab_t* t, const char* name);
/* Tutti gli id con il nome dato (per override/overload). Scrive in out (max) e ritorna il conteggio. */
int  gmb_symtab_find_all(gmb_symtab_t* t, const char* name, int* out, int max);

/* Helpers. */
gmb_sym_t*   gmb_symtab_get(gmb_symtab_t* t, int id);
gmb_scope_t* gmb_symtab_scope(gmb_symtab_t* t, int id);
const char*  gmb_sym_kind_name(gmb_sym_kind_t k);

/* Costruisce la tabella popolando dall'AST (SUB/FUNCTION/DIM/CONST/TYPE/CLASS + scope per
 * corpo/IF/loop). Ritorna il numero di simboli. */
int  gmb_symtab_build(gmb_symtab_t* t, gmb_stmt_t** stmts, int n);

#endif /* GMB_SYMTAB_H */
