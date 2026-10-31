/* gmb_semgraph.c — GMB Semantic Graph (C). Estrattore dall'AST + query.
 * Pari a common/SemanticGraph (C#): nodi-simbolo dalle dichiarazioni (SUB/FUNCTION/DIM),
 * riferimenti (VarRef/Call) dai corpi, edge (Defines/Calls/Uses/Reads/Writes).
 * COMPILATORE C A SE' STANTE: solo C, nessuna dipendenza da C#. */
#include "gmb_semgraph.h"
#include "gmb_resolver.h"
#include "gmb_types.h"
#include "gmb_symtab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ---------- array dinamici ---------- */
static void* xgrow(void* p, size_t* cap, size_t need, size_t elem) {
    if (need <= *cap) return p;
    size_t nc = *cap ? *cap * 2 : 16;
    while (nc < need) nc *= 2;
    void* q = realloc(p, nc * elem);
    if (!q) return p;   /* OOM: resta il vecchio (best-effort) */
    *cap = nc; return q;
}

static void id_stable(char* out, size_t cap, const char* ns, gmb_sym_kind_t kind, const char* name, const char* sig) {
    const char* k = "unknown";
    switch (kind) {
        case GMB_SYM_NAMESPACE: k="ns"; break; case GMB_SYM_MODULE: k="mod"; break;
        case GMB_SYM_PROCEDURE: k="sub"; break; case GMB_SYM_FUNCTION: k="fn"; break;
        case GMB_SYM_METHOD: k="method"; break; case GMB_SYM_VARIABLE: k="var"; break;
        case GMB_SYM_PARAMETER: k="param"; break; case GMB_SYM_CONSTANT: k="const"; break;
        case GMB_SYM_TYPE: k="type"; break; case GMB_SYM_CLASS: k="class"; break;
        case GMB_SYM_INTERFACE: k="iface"; break; case GMB_SYM_ENUM: k="enum"; break;
        case GMB_SYM_FIELD: k="field"; break; case GMB_SYM_PROPERTY: k="prop"; break;
        case GMB_SYM_OPERATOR: k="op"; break; default: k="unknown"; break;
    }
    snprintf(out, cap, "%s|%s|%s|%s", (ns && *ns) ? ns : "", k, name ? name : "", sig ? sig : "");
}

static void ref_id(char* out, size_t cap, const char* enclosing, const char* file, int line, int col, const char* kind) {
    snprintf(out, cap, "%s@%s:%d:%d:%s", enclosing ? enclosing : "", file ? file : "", line, col, kind);
}

static void add_sym(gmb_semgraph_t* g, const gmb_symnode_t* s) {
    static size_t caps = 0;
    if (!g->syms) caps = 0;   /* nuova istanza: la cache statica non vale */
    g->syms = xgrow(g->syms, &caps, g->nsyms + 1, sizeof(gmb_symnode_t));
    g->syms[g->nsyms++] = *s;
}
static void add_ref(gmb_semgraph_t* g, const gmb_refnode_t* r) {
    static size_t capr = 0;
    if (!g->refs) capr = 0;
    g->refs = xgrow(g->refs, &capr, g->nrefs + 1, sizeof(gmb_refnode_t));
    g->refs[g->nrefs++] = *r;
}
static void add_edge(gmb_semgraph_t* g, const char* from, const char* to, gmb_edge_kind_t k,
                     const char* file, int line, int col) {
    static size_t cape = 0;
    if (!g->edges) cape = 0;
    g->edges = xgrow(g->edges, &cape, g->nedges + 1, sizeof(gmb_edge_t));
    gmb_edge_t* e = &g->edges[g->nedges++];
    snprintf(e->from, sizeof e->from, "%s", from ? from : "");
    snprintf(e->to, sizeof e->to, "%s", to ? to : "");
    e->kind = k; snprintf(e->file, sizeof e->file, "%s", file ? file : "");
    e->line = line; e->col = col;
}

/* ---------- walk espressioni ---------- */
static void walk_expr(gmb_semgraph_t* g, const gmb_expr_t* e, const char* file,
                      const char* enclosing, int is_write) {
    if (!e) return;
    switch (e->kind) {
        case GMB_EXPR_VAR: {
            if (!e->var || !*e->var) break;
            gmb_refnode_t r; memset(&r, 0, sizeof r);
            ref_id(r.id, sizeof r.id, enclosing, file, e->line, 0, is_write ? "Assign" : "VarRef");
            snprintf(r.node_kind, sizeof r.node_kind, "%s", is_write ? "Assign" : "VarRef");
            snprintf(r.text, sizeof r.text, "%s", e->var);
            snprintf(r.file, sizeof r.file, "%s", file ? file : "");
            r.line = e->line; snprintf(r.enclosing, sizeof r.enclosing, "%s", enclosing ? enclosing : "");
            add_ref(g, &r);
            add_edge(g, r.id, e->var, is_write ? GMB_EDGE_WRITES : GMB_EDGE_USES, file, e->line, 0);
            break;
        }
        case GMB_EXPR_CALL: {
            if (!e->var || !*e->var) break;
            gmb_refnode_t r; memset(&r, 0, sizeof r);
            ref_id(r.id, sizeof r.id, enclosing, file, e->line, 0, "Call");
            snprintf(r.node_kind, sizeof r.node_kind, "Call");
            snprintf(r.text, sizeof r.text, "%s", e->var);
            snprintf(r.file, sizeof r.file, "%s", file ? file : "");
            r.line = e->line; snprintf(r.enclosing, sizeof r.enclosing, "%s", enclosing ? enclosing : "");
            add_ref(g, &r);
            add_edge(g, enclosing ? enclosing : r.id, e->var, GMB_EDGE_CALLS, file, e->line, 0);
            for (int i = 0; i < e->nargs; i++) walk_expr(g, e->args[i], file, enclosing, 0);
            break;
        }
        case GMB_EXPR_BINARY: walk_expr(g, e->left, file, enclosing, 0); walk_expr(g, e->right, file, enclosing, 0); break;
        case GMB_EXPR_UNARY:  walk_expr(g, e->left, file, enclosing, 0); break;
        case GMB_EXPR_COMPREHENSION:
            walk_expr(g, e->comp_src, file, enclosing, 0); walk_expr(g, e->comp_where, file, enclosing, 0);
            walk_expr(g, e->comp_order, file, enclosing, 0); walk_expr(g, e->comp_sel, file, enclosing, 0);
            break;
        case GMB_EXPR_LAMBDA:
            for (int i = 0; i < e->n_lambda_body; i++) { /* corpo lambda: stmt, gestito dal chiamante */ }
            walk_expr(g, e->lambda_expr, file, enclosing, 0);
            break;
        default: break;
    }
}

/* ---------- walk statement ---------- */
static void walk_stmt(gmb_semgraph_t* g, const gmb_stmt_t* s, const char* file, const char* enclosing);

static void walk_body(gmb_semgraph_t* g, gmb_stmt_t** body, int n, const char* file, const char* enclosing) {
    for (int i = 0; i < n; i++) walk_stmt(g, body[i], file, enclosing);
}

static void walk_stmt(gmb_semgraph_t* g, const gmb_stmt_t* s, const char* file, const char* enclosing) {
    if (!s) return;
    switch (s->kind) {
        case GMB_STMT_FUNCTION: {
            /* simbolo: Procedure (SUB) o Function */
            char sig[256]; size_t o = 0; sig[0] = 0;
            for (int i = 0; i < s->n_params; i++)
                o += (size_t)snprintf(sig + o, sizeof sig - o, "%s%s%s", i ? "," : "",
                                      s->param_byref && s->param_byref[i] ? "BYREF " : "",
                                      s->func_params ? s->func_params[i] : "");
            char fid[256]; id_stable(fid, sizeof fid, s->func_ns, s->is_sub ? GMB_SYM_PROCEDURE : GMB_SYM_FUNCTION, s->func_name, sig);
            walk_body(g, s->func_body, s->n_funcbody, file, fid);
            break;
        }
        case GMB_STMT_DIM: {
            if (s->dim_var && *s->dim_var) {
                /* il simbolo-var viene dal symtab; qui solo refs */
                
            }
            walk_expr(g, s->array_size, file, enclosing, 0);
            break;
        }
        case GMB_STMT_LET: case GMB_STMT_ASSIGN:
            walk_expr(g, s->value, file, enclosing, 0);
            if (s->target && *s->target) {
                char tid[256]; id_stable(tid, sizeof tid, NULL, GMB_SYM_VARIABLE, s->target, "");
                add_edge(g, enclosing ? enclosing : tid, tid, GMB_EDGE_WRITES, file, s->line, 0);
            }
            walk_expr(g, s->array_index, file, enclosing, 0);
            break;
        case GMB_STMT_CALL:
            walk_expr(g, s->expr, file, enclosing, 0);
            break;
        case GMB_STMT_IF:
            walk_expr(g, s->cond, file, enclosing, 0);
            walk_body(g, s->then_body, s->n_then, file, enclosing);
            walk_body(g, s->else_body, s->n_else, file, enclosing);
            break;
        case GMB_STMT_FOR:
            walk_expr(g, s->for_from, file, enclosing, 0); walk_expr(g, s->for_to, file, enclosing, 0);
            walk_expr(g, s->for_step, file, enclosing, 0);
            walk_body(g, s->for_body, s->n_for, file, enclosing);
            break;
        case GMB_STMT_WHILE: walk_expr(g, s->cond, file, enclosing, 0); walk_body(g, s->while_body, s->n_while, file, enclosing); break;
        case GMB_STMT_DO: walk_expr(g, s->do_until, file, enclosing, 0); walk_expr(g, s->do_while, file, enclosing, 0); walk_body(g, s->do_body, s->n_do, file, enclosing); break;
        case GMB_STMT_PRINT: for (int i = 0; i < s->nprint; i++) walk_expr(g, s->print_args[i], file, enclosing, 0); break;
        case GMB_STMT_RETURN: walk_expr(g, s->value, file, enclosing, 0); break;
        case GMB_STMT_EXPR: walk_expr(g, s->expr, file, enclosing, 0); break;
        default: break;
    }
}

void gmb_semgraph_extract(gmb_semgraph_t* g, gmb_stmt_t** stmts, int n, const char* file) {
    if (!g) return;
    memset(g, 0, sizeof *g);
    /* 1) nodi-simbolo dalla SYMBOL TABLE RICCA (kind/type/signature/ns/container/exported) —
     *    pari al C# (AllSymbols). 2) refs/edges dal walk dell'AST. */
    gmb_symtab_t st; gmb_symtab_init(&st);
    gmb_symtab_build(&st, stmts, n);
    for (int i = 0; i < st.n_syms; i++) {
        gmb_sym_t* sy = &st.syms[i];
        gmb_symnode_t nn; memset(&nn, 0, sizeof nn);
        id_stable(nn.id, sizeof nn.id, sy->ns, sy->kind, sy->name, sy->signature ? sy->signature : "");
        nn.kind = sy->kind;
        snprintf(nn.name, sizeof nn.name, "%s", sy->name ? sy->name : "");
        snprintf(nn.ns, sizeof nn.ns, "%s", sy->ns ? sy->ns : "");
        snprintf(nn.file, sizeof nn.file, "%s", file ? file : "");
        nn.line = sy->decl_line;
        snprintf(nn.type_name, sizeof nn.type_name, "%s", sy->type_name ? sy->type_name : "");
        snprintf(nn.signature, sizeof nn.signature, "%s", sy->signature ? sy->signature : "");
        nn.is_exported = sy->is_exported;
        if (sy->container_sym_id >= 0 && sy->container_sym_id < st.n_syms) {
            gmb_sym_t* par = &st.syms[sy->container_sym_id];
            id_stable(nn.container, sizeof nn.container, par->ns, par->kind, par->name, par->signature ? par->signature : "");
            add_edge(g, nn.container, nn.id, GMB_EDGE_DEFINES, file, sy->decl_line, 0);
        }
        add_sym(g, &nn);
    }
    /* refs + Calls/Uses/Reads/Writes */
    for (int i = 0; i < n; i++) walk_stmt(g, stmts[i], file, "");
    /* attribuzione TIPI sui riferimenti (pari a Linker.InferTypes): risolve il tipo di ogni
     * ref e collega il ref alla DEFINIZIONE (sym_id). st/tr ancora vivi. */
    gmb_typereg_t tr; gmb_typereg_init(&tr); gmb_typereg_build(&tr, stmts, n);
    for (size_t i = 0; i < g->nrefs; i++) {
        gmb_refnode_t* r = &g->refs[i];
        char ty[64] = "";
        if (gmb_resolve_name_type(&st, &tr, r->text, ty, sizeof ty, NULL, 0) == 0)
            snprintf(r->type_name, sizeof r->type_name, "%s", ty);
        int id = gmb_symtab_find_any(&st, r->text);
        if (id >= 0) {
            gmb_sym_t* sy = gmb_symtab_get(&st, id);
            if (sy) id_stable(r->sym_id, sizeof r->sym_id, sy->ns, sy->kind, sy->name, sy->signature ? sy->signature : "");
        }
    }
    gmb_typereg_free(&tr);
    gmb_symtab_free(&st);
}

void gmb_semgraph_free(gmb_semgraph_t* g) {
    if (!g) return;
    free(g->syms); free(g->refs); free(g->edges);
    memset(g, 0, sizeof *g);
}

/* ---------- query ---------- */
const gmb_symnode_t* gmb_semgraph_definition(const gmb_semgraph_t* g, const char* name) {
    if (!g || !name) return NULL;
    for (size_t i = 0; i < g->nsyms; i++)
        if (strcasecmp(g->syms[i].name, name) == 0) return &g->syms[i];
    return NULL;
}

static size_t edge_query(const gmb_semgraph_t* g, const char* sym_id, int callees, gmb_edge_t** out) {
    *out = NULL; if (!g || !sym_id) return 0;
    size_t cap = 0, n = 0; gmb_edge_t* v = NULL;
    for (size_t i = 0; i < g->nedges; i++) {
        const gmb_edge_t* e = &g->edges[i];
        int match = callees ? (strcasecmp(e->from, sym_id) == 0 || strcasecmp(e->to, sym_id) == 0)
                            : (strcasecmp(e->to, sym_id) == 0);
        if (callees) match = (strcasecmp(e->from, sym_id) == 0) || (strcasecmp(e->to, sym_id) == 0);
        else match = (strcasecmp(e->to, sym_id) == 0);
        if (match && (e->kind == GMB_EDGE_CALLS || e->kind == GMB_EDGE_USES ||
                      e->kind == GMB_EDGE_READS || e->kind == GMB_EDGE_WRITES)) {
            v = xgrow(v, &cap, n + 1, sizeof(gmb_edge_t)); v[n++] = *e;
        }
    }
    *out = v; return n;
}

size_t gmb_semgraph_references(const gmb_semgraph_t* g, const char* sym_id, gmb_edge_t** out) {
    return edge_query(g, sym_id, 0, out);
}
size_t gmb_semgraph_callers(const gmb_semgraph_t* g, const char* sym_id, gmb_edge_t** out) {
    *out = NULL; if (!g || !sym_id) return 0;
    size_t cap = 0, n = 0; gmb_edge_t* v = NULL;
    for (size_t i = 0; i < g->nedges; i++) {
        const gmb_edge_t* e = &g->edges[i];
        if (e->kind == GMB_EDGE_CALLS && strcasecmp(e->to, sym_id) == 0) { v = xgrow(v, &cap, n + 1, sizeof(gmb_edge_t)); v[n++] = *e; }
    }
    *out = v; return n;
}
size_t gmb_semgraph_callees(const gmb_semgraph_t* g, const char* sym_id, gmb_edge_t** out) {
    *out = NULL; if (!g || !sym_id) return 0;
    size_t cap = 0, n = 0; gmb_edge_t* v = NULL;
    for (size_t i = 0; i < g->nedges; i++) {
        const gmb_edge_t* e = &g->edges[i];
        if (e->kind == GMB_EDGE_CALLS && strcasecmp(e->from, sym_id) == 0) { v = xgrow(v, &cap, n + 1, sizeof(gmb_edge_t)); v[n++] = *e; }
    }
    *out = v; return n;
}

size_t gmb_semgraph_impact(const gmb_semgraph_t* g, const char* sym_id, int maxdepth, char*** out) {
    if (!g || !sym_id || maxdepth <= 0) { if (out) *out = NULL; return 0; }
    /* chiusura transitiva dei caller (BFS sul grafo inverso dei Calls). */
    char** frontier = NULL; size_t nf = 0, cf = 0;
    frontier = xgrow(frontier, &cf, nf + 1, sizeof(char*)); frontier[nf++] = strdup(sym_id);
    char** seen = NULL; size_t nseen = 0, cs = 0;
    char** res = NULL; size_t nres = 0, cr = 0;
    for (int d = 0; d < maxdepth && nf > 0; d++) {
        char** next = NULL; size_t nn = 0, cn = 0;
        for (size_t i = 0; i < nf; i++) {
            const char* cur = frontier[i];
            for (size_t e = 0; e < g->nedges; e++) {
                const gmb_edge_t* ed = &g->edges[e];
                if (ed->kind != GMB_EDGE_CALLS || strcasecmp(ed->to, cur) != 0) continue;
                const char* caller = ed->from;
                int dup = 0;
                for (size_t k = 0; k < nseen && !dup; k++) if (strcasecmp(seen[k], caller) == 0) dup = 1;
                if (dup) continue;
                seen = xgrow(seen, &cs, nseen + 1, sizeof(char*)); seen[nseen++] = strdup(caller);
                res = xgrow(res, &cr, nres + 1, sizeof(char*)); res[nres++] = strdup(caller);
                next = xgrow(next, &cn, nn + 1, sizeof(char*)); next[nn++] = strdup(caller);
            }
        }
        for (size_t i = 0; i < nf; i++) free(frontier[i]);
        free(frontier); frontier = next; nf = nn; cf = cn;
    }
    free(frontier); for (size_t i = 0; i < nseen; i++) free(seen[i]); free(seen);
    if (out) *out = res; else for (size_t i = 0; i < nres; i++) free(res[i]);
    return nres;
}

void gmb_semgraph_free_query(void* p) { free(p); }

/* ---------- diff tra snapshot ---------- */
static const gmb_symnode_t* find_sym(const gmb_semgraph_t* g, const char* id) {
    for (size_t i = 0; i < g->nsyms; i++) if (strcmp(g->syms[i].id, id) == 0) return &g->syms[i];
    return NULL;
}
static void diff_add_change(gmb_semgraph_diff_t* d, const char* id, gmb_symchg_kind_t k) {
    d->changes = realloc(d->changes, (d->n_changes + 1) * sizeof(gmb_sym_change_t));
    snprintf(d->changes[d->n_changes].id, sizeof d->changes[d->n_changes].id, "%s", id);
    d->changes[d->n_changes].kind = k; d->n_changes++;
    if (k == GMB_SYMCHG_ADDED) d->n_added++;
    else if (k == GMB_SYMCHG_REMOVED) d->n_removed++;
    else d->n_changed++;
}
static int edge_seen(const gmb_semgraph_t* g, const gmb_edge_t* e) {
    for (size_t i = 0; i < g->nedges; i++)
        if (strcmp(g->edges[i].from, e->from) == 0 && strcmp(g->edges[i].to, e->to) == 0
            && g->edges[i].kind == e->kind) return 1;
    return 0;
}
static int id_in_list(char** v, size_t n, const char* id) {
    for (size_t i = 0; i < n; i++) if (strcmp(v[i], id) == 0) return 1;
    return 0;
}
static void impacted_add(gmb_semgraph_diff_t* d, const char* id) {
    if (id_in_list(d->impacted, d->n_impacted, id)) return;
    d->impacted = realloc(d->impacted, (d->n_impacted + 1) * sizeof(char*));
    d->impacted[d->n_impacted++] = strdup(id);
}
void gmb_semgraph_diff(const gmb_semgraph_t* from, const gmb_semgraph_t* to, gmb_semgraph_diff_t* out) {
    memset(out, 0, sizeof *out);
    if (!from || !to) return;
    for (size_t i = 0; i < to->nsyms; i++) {
        const gmb_symnode_t* a = &to->syms[i];
        const gmb_symnode_t* b = find_sym(from, a->id);
        if (!b) { diff_add_change(out, a->id, GMB_SYMCHG_ADDED); continue; }
        gmb_symchg_kind_t k;
        if (strcmp(b->signature, a->signature) != 0) k = GMB_SYMCHG_SIGNATURE;
        else if (strcmp(b->type_name, a->type_name) != 0) k = GMB_SYMCHG_TYPE;
        else if (b->is_exported != a->is_exported) k = GMB_SYMCHG_VISIBILITY;
        else if (b->line != a->line || b->col != a->col) k = GMB_SYMCHG_MOVED;
        else continue;
        diff_add_change(out, a->id, k);
    }
    for (size_t i = 0; i < from->nsyms; i++)
        if (!find_sym(to, from->syms[i].id)) diff_add_change(out, from->syms[i].id, GMB_SYMCHG_REMOVED);
    for (size_t i = 0; i < to->nedges; i++) if (!edge_seen(from, &to->edges[i])) out->n_added_edges++;
    for (size_t i = 0; i < from->nedges; i++) if (!edge_seen(to, &from->edges[i])) out->n_removed_edges++;
    for (size_t i = 0; i < out->n_changes; i++)
        if (out->changes[i].kind != GMB_SYMCHG_ADDED)
            for (size_t j = 0; j < to->nedges; j++)
                if (strcmp(to->edges[j].to, out->changes[i].id) == 0
                    && (to->edges[j].kind == GMB_EDGE_CALLS || to->edges[j].kind == GMB_EDGE_USES
                        || to->edges[j].kind == GMB_EDGE_DEFINES))
                    impacted_add(out, to->edges[j].from);
}
void gmb_semgraph_diff_free(gmb_semgraph_diff_t* d) {
    if (!d) return;
    free(d->changes);
    for (size_t i = 0; i < d->n_impacted; i++) free(d->impacted[i]);
    free(d->impacted); memset(d, 0, sizeof *d);
}
