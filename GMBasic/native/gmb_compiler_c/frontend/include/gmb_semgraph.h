/* gmb_semgraph.h — GMB Semantic Graph (C): grafo semantico del programma BASIC, pari al
 * `common/SemanticGraph` C# (§4.10). Nodi = simboli ES espressioni/statement (riferimenti);
 * edge = relazioni; query definition/references/callers/callees/impact.
 *
 * COMPILATORE C A SE' STANTE: nessuna dipendenza da C#, nessuna chiamata inversa.
 * Costruito dall'AST C (`gmb_ast.h`) dopo il link. Pre-requisito (in questo stesso mondo) per
 * il grafo ricco: symbol table + scope (vedi Bugs/(open) - C99Native-Parita-...).
 */
#ifndef GMB_SEMGRAPH_H
#define GMB_SEMGRAPH_H

#include "gmb_ast.h"
#include <stddef.h>

/* Specchio di SemanticSymbolKind (C#). */
typedef enum {
    GMB_SYM_NAMESPACE, GMB_SYM_MODULE, GMB_SYM_PROCEDURE, GMB_SYM_FUNCTION, GMB_SYM_METHOD,
    GMB_SYM_VARIABLE, GMB_SYM_PARAMETER, GMB_SYM_CONSTANT, GMB_SYM_TYPE, GMB_SYM_CLASS,
    GMB_SYM_INTERFACE, GMB_SYM_ENUM, GMB_SYM_FIELD, GMB_SYM_PROPERTY, GMB_SYM_OPERATOR,
    GMB_SYM_UNKNOWN
} gmb_sym_kind_t;

/* Specchio di SemanticEdgeKind (C#). */
typedef enum {
    GMB_EDGE_DEFINES, GMB_EDGE_CALLS, GMB_EDGE_USES, GMB_EDGE_READS, GMB_EDGE_WRITES,
    GMB_EDGE_RETURNS, GMB_EDGE_INHERITS, GMB_EDGE_IMPLEMENTS, GMB_EDGE_IMPORTS,
    GMB_EDGE_CONTAINS, GMB_EDGE_DEPENDSON
} gmb_edge_kind_t;

typedef struct {
    char id[256];          /* id STABILE: ns + kind + nome + firma (non l'id di compilazione) */
    gmb_sym_kind_t kind;
    char name[128];
    char ns[128];          /* namespace, "" se globale */
    char container[256];   /* id del simbolo contenitore (Defines), "" se nessuno */
    char file[256];
    int  line, col;
    char type_name[64];    /* tipo (se noto) */
    char signature[256];   /* firma (parametri) */
    int  is_exported;
} gmb_symnode_t;

typedef struct {
    char id[256];          /* id del nodo-riferimento (enclosing + pos + kind) */
    char node_kind[24];    /* "VarRef" | "Call" | "Assign" | "Expr" */
    char text[128];        /* resa testuale breve */
    char file[256];
    int  line, col;
    char enclosing[256];   /* id del simbolo che contiene il riferimento */
    char sym_id[256];      /* id stabile del simbolo RISOLTO (definizione), "" se non risolto */
    char type_name[64];    /* tipo risolto del riferimento (se noto), come il C# InferTypes */
} gmb_refnode_t;

typedef struct {
    char from[256];        /* id o ref-id */
    char to[256];          /* id simbolo */
    gmb_edge_kind_t kind;
    char file[256];
    int  line, col;
} gmb_edge_t;

typedef struct {
    gmb_symnode_t* syms;  size_t nsyms;
    gmb_refnode_t* refs;  size_t nrefs;
    gmb_edge_t*    edges; size_t nedges;
} gmb_semgraph_t;

/* Costruisce il grafo dall'AST (dopo il link). `file` = nome file (per i nodi). */
void gmb_semgraph_extract(gmb_semgraph_t* g, gmb_stmt_t** stmts, int n, const char* file);
void gmb_semgraph_free(gmb_semgraph_t* g);

/* Query (come il C# Queries.cs). Ritornano 0 se non trovato.
 * `out`/`out_n` = vettore allocato (gmb_semgraph_free_query) o NULL. */
const gmb_symnode_t* gmb_semgraph_definition(const gmb_semgraph_t* g, const char* name);
size_t gmb_semgraph_references(const gmb_semgraph_t* g, const char* sym_id, gmb_edge_t** out);
size_t gmb_semgraph_callers(const gmb_semgraph_t* g, const char* sym_id, gmb_edge_t** out);
size_t gmb_semgraph_callees(const gmb_semgraph_t* g, const char* sym_id, gmb_edge_t** out);
/* Impact set (chiusura transitiva dei caller entro maxdepth). out = array di id (strdup). */
size_t gmb_semgraph_impact(const gmb_semgraph_t* g, const char* sym_id, int maxdepth, char*** out);
void   gmb_semgraph_free_query(void* p);

/* ── Diff tra due snapshot (pari a SemanticGraph/GraphDiff.cs) ───────────── */
typedef enum {
    GMB_SYMCHG_ADDED, GMB_SYMCHG_REMOVED, GMB_SYMCHG_MOVED,
    GMB_SYMCHG_TYPE, GMB_SYMCHG_SIGNATURE, GMB_SYMCHG_VISIBILITY
} gmb_symchg_kind_t;

typedef struct { char id[256]; gmb_symchg_kind_t kind; } gmb_sym_change_t;

typedef struct {
    gmb_sym_change_t* changes; size_t n_changes;
    int n_added, n_removed, n_changed;
    size_t n_added_edges, n_removed_edges;
    char** impacted; size_t n_impacted;   /* id (strdup) dei simboli impattati */
} gmb_semgraph_diff_t;

/* Differenzia due grafi per id stabile: cambi simbolo (added/removed/moved/type/sig/visibility),
 * conteggio edge aggiunti/rimossi, e impatto (chi referenzia i simboli rimossi/modificati nel `to`). */
void gmb_semgraph_diff(const gmb_semgraph_t* from, const gmb_semgraph_t* to, gmb_semgraph_diff_t* out);
void gmb_semgraph_diff_free(gmb_semgraph_diff_t* d);

#endif /* GMB_SEMGRAPH_H */
