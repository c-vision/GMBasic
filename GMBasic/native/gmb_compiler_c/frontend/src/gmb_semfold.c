/* gmb_semfold.c — §4.10(f) Opzione A: risolve a COMPILE/LINK-time le funzioni del GMB Semantic
 * Graph (program-side) `AI.SYMBOL(name$)` / `AI.IMPACT(name$[,depth])` / `AI.CALLMAP(name$)` usando
 * il grafo `gmb_semgraph` del programma stesso, e le sostituisce con una COSTANTE STRINGA.
 *
 * Formato canonico (deve combaciare col lato C# `common/SemanticGraph`):
 *   AI.SYMBOL  -> "<kind> <label> @ <file>:<line>"   ("" se non trovato)
 *   AI.CALLMAP -> "callers=[<from>,...] callees=[<to>,...]"
 *   AI.IMPACT  -> "<id>@<depth>,..."  (chiusura all'indietro; senza il seme)
 * COMPILATORE C AUTONOMO (VM/C + C99Native). */
#include "gmb_semgraph.h"
#include "gmb_ast.h"
#include "gmb_symtab.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static gmb_semgraph_t g_g;
static int g_built;

static const char* fold_expr(gmb_expr_t* e);   /* forward */

static int call_edge(const gmb_edge_t* e, const char** from, const char** to);
/* Coppie (caller->callee) normalizzate, ordinate e deduplicate (per REVISION/CONTEXT). */
static int cmp_str(const void* a, const void* b) { return strcmp(*(const char* const*)a, *(const char* const*)b); }
static char** call_pairs(int* out_n) {
    char** v = NULL; int n = 0, cap = 0;
    for (size_t i = 0; i < g_g.nedges; i++) { const char* fr; const char* to; if (call_edge(&g_g.edges[i], &fr, &to) != 0) continue;
        if (!gmb_semgraph_definition(&g_g, to)) continue;   /* solo call a simboli UTENTE (pari al mini-walk C#) */
        char buf[600]; snprintf(buf, sizeof buf, "%s->%s", fr, to);
        int dup = 0; for (int k = 0; k < n; k++) if (strcmp(v[k], buf) == 0) { dup = 1; break; }
        if (dup) continue;
        if (n == cap) { cap = cap * 2 + 8; v = (char**)realloc(v, sizeof(char*) * (size_t)cap); }
        v[n++] = strdup(buf);
    }
    qsort(v, (size_t)n, sizeof(char*), cmp_str);
    *out_n = n; return v;
}


/* Nome del simbolo con un dato id (o l'id stesso se non trovato). */
static const char* name_of(const char* id) {
    for (size_t i = 0; i < g_g.nsyms; i++) if (strcmp(g_g.syms[i].id, id) == 0) return g_g.syms[i].name;
    return id;
}
/* (nomeChiamante, nomeChiamato) di un edge CALLS. Il C usa from=|sym|, to=<nome>. */
static int call_edge(const gmb_edge_t* e, const char** from, const char** to) {
    if (e->kind != GMB_EDGE_CALLS) return -1;
    *from = name_of(e->from); *to = e->to; return 0;
}

static int is_call(const gmb_expr_t* e, const char* name) {
    return e && e->kind == GMB_EXPR_CALL && e->ns && strcasecmp(e->ns, "LLM") == 0
        && e->var && strcasecmp(e->var, name) == 0;
}
static int literal_arg(const gmb_expr_t* e, char* out, size_t cap) {
    if (e && e->nargs >= 1 && e->args[0] && e->args[0]->kind == GMB_EXPR_STR && e->args[0]->str) {
        snprintf(out, cap, "%s", e->args[0]->str); return 0;
    }
    return -1;
}
static int int_arg(const gmb_expr_t* e, int idx, int dflt) {
    if (e && e->nargs > idx && e->args[idx] && e->args[idx]->kind == GMB_EXPR_NUM)
        return (int)e->args[idx]->num;
    return dflt;
}

/* ---------- §4.10 write-side lifecycle: rev dei patch (per LLM.VERIFY/COMMIT) ---------- */
static const char* g_patch_revs[256]; static int g_npatch_revs;
static void collect_patch_revs(gmb_stmt_t** body, int n) {
    for (int i = 0; i < n; i++) { gmb_stmt_t* st = body[i]; if (!st) continue;
        if (st->kind == GMB_STMT_AIPATCH) { if (g_npatch_revs < 256) g_patch_revs[g_npatch_revs++] = st->patch_rev ? st->patch_rev : ""; }
        collect_patch_revs(st->then_body, st->n_then); collect_patch_revs(st->else_body, st->n_else);
        collect_patch_revs(st->for_body, st->n_for); collect_patch_revs(st->while_body, st->n_while); collect_patch_revs(st->do_body, st->n_do);
        collect_patch_revs(st->func_body, st->n_funcbody);
    }
}

/* Calcola la stringa del fold per una call AI.SYMBOL/IMPACT/CALLMAP. Ritorna strdup o NULL. */
static char* query(const gmb_expr_t* e) {
    if (is_call(e, "VERIFY") || is_call(e, "COMMIT")) {
        int np = 0; char** v = call_pairs(&np);
        unsigned long long h = 1469598103934665603ULL;
        for (int i = 0; i < np; i++) for (const char* q = v[i]; *q; q++) { h ^= (unsigned char)*q; h *= 1099511628211ULL; }
        for (int i = 0; i < np; i++) free(v[i]); free(v);
        int isv = is_call(e, "VERIFY");
        if (g_npatch_revs == 0) return strdup(isv ? "verify: none" : "commit: none");
        char rbuf[96]; snprintf(rbuf, sizeof rbuf, "rev=%016llx", h);
        int all = 1;
        for (int i = 0; i < g_npatch_revs; i++) if (strcmp(g_patch_revs[i], rbuf) != 0) { all = 0; break; }
        if (isv) return strdup(all ? "verify: ok" : "verify: rejected");
        return strdup(all ? "commit: applied" : "commit: rejected");
    }
    if (is_call(e, "REPOMAP")) {
        int np = 0; char** v = call_pairs(&np);
        char rbuf[8192]; size_t oo = 0; oo += (size_t)snprintf(rbuf + oo, sizeof rbuf - oo, "repomap: [");
        for (int i = 0; i < np; i++) oo += (size_t)snprintf(rbuf + oo, sizeof rbuf - oo, "%s%s", i ? ";" : "", v[i]);
        snprintf(rbuf + oo, sizeof rbuf - oo, "]");
        for (int i = 0; i < np; i++) free(v[i]); free(v);
        return strdup(rbuf);
    }
    if (is_call(e, "REVISION") || is_call(e, "CHECKREV")) {
        int np = 0; char** v = call_pairs(&np);
        unsigned long long h = 1469598103934665603ULL;
        for (int i = 0; i < np; i++) for (const char* q = v[i]; *q; q++) { h ^= (unsigned char)*q; h *= 1099511628211ULL; }
        for (int i = 0; i < np; i++) free(v[i]); free(v);
        char rbuf[96]; snprintf(rbuf, sizeof rbuf, "rev=%016llx", h);
        if (is_call(e, "HOWREV")) {}
        if (is_call(e, "CHECKREV")) {
            char wr[64]; if (literal_arg(e, wr, sizeof wr) != 0) wr[0] = 0;
            return strdup(strcmp(wr, rbuf) == 0 ? "match" : "mismatch");
        }
        return strdup(rbuf);
    }
    if (is_call(e, "FILEOUTLINE")) {
        /* §3.6 PARSEC — outline per-file: i simboli DEFINITI nel file `fl` (basename o suffix),
         * "kind name", in ordine di (line, name), separati da ";". */
        char fl[256]; if (literal_arg(e, fl, sizeof fl) != 0) fl[0] = 0;
        const char* base = strrchr(fl, '/'); base = base ? base + 1 : fl;
        int idx[2048]; int ni = 0;
        for (size_t i = 0; i < g_g.nsyms && ni < 2048; i++) {
            const char* f = g_g.syms[i].file;
            const char* fb = strrchr(f, '/'); fb = fb ? fb + 1 : f;
            if (fl[0] && (strcasecmp(fb, fl) == 0 || strcasecmp(fb, base) == 0 ||
                          (strlen(f) >= strlen(fl) && strcasecmp(f + strlen(f) - strlen(fl), fl) == 0)))
                idx[ni++] = (int)i;
        }
        for (int i = 0; i < ni; i++) for (int j = i + 1; j < ni; j++) {
            const gmb_symnode_t* a = &g_g.syms[idx[i]]; const gmb_symnode_t* b = &g_g.syms[idx[j]];
            if (a->line > b->line || (a->line == b->line && strcmp(a->name, b->name) > 0)) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        }
        char rbuf[8192]; size_t oo = 0; rbuf[0] = 0;
        for (int i = 0; i < ni; i++) { const gmb_symnode_t* s2 = &g_g.syms[idx[i]];
            oo += (size_t)snprintf(rbuf + oo, sizeof rbuf - oo, "%s%s %s", i ? ";" : "", gmb_sym_kind_name(s2->kind), s2->name); }
        return strdup(rbuf);
    }
    char name[256];
    if (literal_arg(e, name, sizeof name) != 0 || !name[0]) return NULL;
    const gmb_symnode_t* s = gmb_semgraph_definition(&g_g, name);
    if (is_call(e, "FINDSYMBOL")) {
        int np = 0; char** v = call_pairs(&np);
        /* nomi distinti presenti nelle coppie (parity-safe: stesse call del C#). */
        char names[1024][128]; int nn = 0;
        for (int i = 0; i < np; i++) { const char* arrow = strstr(v[i], "->"); if (!arrow) continue; size_t cl = (size_t)(arrow - v[i]); const char* cal = arrow + 2;
            const char* cands[2] = { NULL, cal }; char fr[128]; if (cl > 127) cl = 127; memcpy(fr, v[i], cl); fr[cl] = 0; cands[0] = fr;
            for (int k = 0; k < 2; k++) { const char* nm = cands[k]; int dup = 0; for (int j = 0; j < nn; j++) if (strcmp(names[j], nm) == 0) dup = 1; if (!dup && nn < 1024) snprintf(names[nn++], sizeof names[0], "%s", nm); } }
        /* ordina */
        for (int i = 0; i < nn; i++) for (int j = i + 1; j < nn; j++) if (strcmp(names[i], names[j]) > 0) { char t[128]; memcpy(t, names[i], sizeof t); memcpy(names[i], names[j], sizeof t); memcpy(names[j], t, sizeof t); }
        char rbuf[8192]; size_t oo = 0;
        if (is_call(e, "FINDSYMBOL")) {
            char pre[128]; if (literal_arg(e, pre, sizeof pre) != 0) pre[0] = 0;
            oo += (size_t)snprintf(rbuf + oo, sizeof rbuf - oo, "find %s: [", pre); int first = 1;
            for (int i = 0; i < nn; i++) if (strcasestr(names[i], pre)) { oo += (size_t)snprintf(rbuf + oo, sizeof rbuf - oo, "%s%s", first ? "" : ",", names[i]); first = 0; }
            snprintf(rbuf + oo, sizeof rbuf - oo, "]");
        } else {
            oo += (size_t)snprintf(rbuf + oo, sizeof rbuf - oo, "repomap: [");
            for (int i = 0; i < np; i++) oo += (size_t)snprintf(rbuf + oo, sizeof rbuf - oo, "%s%s", i ? ";" : "", v[i]);
            snprintf(rbuf + oo, sizeof rbuf - oo, "]");
        }
        for (int i = 0; i < np; i++) free(v[i]); free(v);
        return strdup(rbuf);
    }
    if (!s) return strdup("");   /* simbolo inesistente: stringa vuota (mai crash) */
    char buf[4096]; size_t o = 0; buf[0] = 0;
    if (is_call(e, "SYMBOL")) {
        if (!s) return strdup("");
        o += (size_t)snprintf(buf + o, sizeof buf - o, "%s %s",
                              gmb_sym_kind_name(s->kind), s->name);
        return strdup(buf);
    }
    if (is_call(e, "CALLMAP")) {
        if (!s) return strdup("");
        const char* nm = s->name; int firstc = 1, firstl = 1;
        o += (size_t)snprintf(buf + o, sizeof buf - o, "callers=[");
        for (size_t i = 0; i < g_g.nedges; i++) { const char* fr; const char* to; if (call_edge(&g_g.edges[i], &fr, &to) != 0) continue;
            if (strcmp(to, nm) == 0) o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%s", firstc ? "" : ",", fr), firstc = 0; }
        o += (size_t)snprintf(buf + o, sizeof buf - o, "] callees=[");
        for (size_t i = 0; i < g_g.nedges; i++) { const char* fr; const char* to; if (call_edge(&g_g.edges[i], &fr, &to) != 0) continue;
            if (strcmp(fr, nm) == 0 && gmb_semgraph_definition(&g_g, to)) o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%s", firstl ? "" : ",", to), firstl = 0; }
        o += (size_t)snprintf(buf + o, sizeof buf - o, "]");
        return strdup(buf);
    }
    if (is_call(e, "INSPECT")) {
        if (!s) return strdup("");
        const char* inm = s->name; int ifc = 1, ifl = 1;
        o += (size_t)snprintf(buf + o, sizeof buf - o, "%s %s callers=[", gmb_sym_kind_name(s->kind), s->name);
        for (size_t i = 0; i < g_g.nedges; i++) { const char* fr; const char* to; if (call_edge(&g_g.edges[i], &fr, &to) != 0) continue;
            if (strcmp(to, inm) == 0) o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%s", ifc ? "" : ",", fr), ifc = 0; }
        o += (size_t)snprintf(buf + o, sizeof buf - o, "] callees=[");
        for (size_t i = 0; i < g_g.nedges; i++) { const char* fr; const char* to; if (call_edge(&g_g.edges[i], &fr, &to) != 0) continue;
            if (strcmp(fr, inm) == 0 && gmb_semgraph_definition(&g_g, to)) o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%s", ifl ? "" : ",", to), ifl = 0; }
        o += (size_t)snprintf(buf + o, sizeof buf - o, "]");
        return strdup(buf);
    }
    if (is_call(e, "CONTEXT")) {
        int dep = int_arg(e, 1, 1); if (dep < 0) dep = 0;
        int np = 0; char** v = call_pairs(&np);
        char seen[512][128]; int nseen = 0;
        snprintf(seen[nseen++], sizeof seen[0], "%s", s->name);
        char frontier[256][128]; int nf = 1; snprintf(frontier[0], sizeof frontier[0], "%s", s->name);
        for (int lvl = 0; lvl < dep && nf > 0; lvl++) {
            char next[256][128]; int nn = 0;
            for (int fi = 0; fi < nf; fi++)
                for (int i = 0; i < np; i++) {
                    const char* arrow = strstr(v[i], "->"); if (!arrow) continue;
                    size_t cl = (size_t)(arrow - v[i]);
                    const char* callee = arrow + 2;
                    const char* other = NULL;
                    if (strlen(frontier[fi]) == cl && strncmp(v[i], frontier[fi], cl) == 0) other = callee;
                    else if (strcmp(callee, frontier[fi]) == 0) other = v[i];
                    if (!other) continue; size_t ol = other == callee ? strlen(other) : cl;
                    char nm[128]; size_t cp = other == callee ? strlen(other) : cl; if (cp > 127) cp = 127; memcpy(nm, other, cp); nm[cp] = 0;
                    int dup = 0; for (int k = 0; k < nseen; k++) if (strcmp(seen[k], nm) == 0) { dup = 1; break; }
                    if (!dup && nseen < 512) { snprintf(seen[nseen++], sizeof seen[0], "%s", nm); if (nn < 256) snprintf(next[nn++], sizeof next[0], "%s", nm); }
                }
            for (int i = 0; i < nn; i++) snprintf(frontier[i], sizeof frontier[0], "%s", next[i]); nf = nn;
        }
        /* ordina i nomi per output deterministico */
        int idx[512]; for (int i = 0; i < nseen; i++) idx[i] = i;
        for (int i = 0; i < nseen; i++) for (int j = i + 1; j < nseen; j++) if (strcmp(seen[idx[i]], seen[idx[j]]) > 0) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        size_t oo = 0; oo += (size_t)snprintf(buf + oo, sizeof buf - oo, "context %s: [", s->name);
        for (int i = 0; i < nseen; i++) oo += (size_t)snprintf(buf + oo, sizeof buf - oo, "%s%s", i ? "," : "", seen[idx[i]]);
        snprintf(buf + oo, sizeof buf - oo, "]");
        for (int i = 0; i < np; i++) free(v[i]); free(v);
        return strdup(buf);
    }
    if (is_call(e, "PATCH")) {
        /* receipt: target + impatto (callers transitivi). Il gate revisione e' CHECKREV. */
        const char* cur2[256]; int nc2 = 1; cur2[0] = s->name;
        const char* seen2[512]; int ns2 = 0; int first2 = 1;
        o += (size_t)snprintf(buf + o, sizeof buf - o, "patch target=%s impact=[", s->name);
        for (int d = 0; d < 32 && nc2 > 0; d++) {
            const char* nxt2[256]; int nn2 = 0;
            for (int i = 0; i < nc2; i++)
                for (size_t j = 0; j < g_g.nedges; j++) { const char* fr; const char* to; if (call_edge(&g_g.edges[j], &fr, &to) != 0) continue;
                    if (strcmp(to, cur2[i]) != 0) continue;
                    int dup = (strcmp(fr, s->name) == 0); for (int k = 0; k < ns2 && !dup; k++) if (strcmp(seen2[k], fr) == 0) dup = 1;
                    if (!dup && nn2 < 256) { seen2[ns2 < 511 ? ns2++ : 511] = fr; nxt2[nn2++] = fr;
                        o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%s@%d", first2 ? "" : ",", fr, d + 1); first2 = 0; } }
            for (int i = 0; i < nn2; i++) cur2[i] = nxt2[i]; nc2 = nn2; if (nc2 > 256) nc2 = 256;
        }
        snprintf(buf + o, sizeof buf - o, "] decision=APPLY");
        return strdup(buf);
    }
    if (is_call(e, "IMPACT")) {
        if (!s) return strdup("");
        int depth = int_arg(e, 1, 32);
        /* BFS all'indietro sui chiamanti (nomi); output "<nome>@<depth>", senza il seme. */
        const char* cur[256]; int cd[256]; int ncur = 1; cur[0] = s->name; cd[0] = 0;
        const char* seen[512]; int nseen = 0; int first = 1;
        for (int d = 0; d < depth && ncur > 0; d++) {
            const char* nxt[256]; int nn = 0;
            for (int i = 0; i < ncur; i++)
                for (size_t j = 0; j < g_g.nedges; j++) { const char* fr; const char* to; if (call_edge(&g_g.edges[j], &fr, &to) != 0) continue;
                    if (strcmp(to, cur[i]) != 0) continue;
                    int dup = (strcmp(fr, s->name) == 0); for (int k = 0; k < nseen && !dup; k++) if (strcmp(seen[k], fr) == 0) dup = 1;
                    if (!dup && nn < 256) { seen[nseen < 511 ? nseen++ : 511] = fr; nxt[nn++] = fr;
                        o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%s@%d", first ? "" : ",", fr, d + 1); first = 0; } }
            for (int i = 0; i < nn; i++) { cur[i] = nxt[i]; cd[i] = d + 1; } ncur = nn; if (ncur > 256) ncur = 256;
        }
        return strdup(buf);
    }
    return NULL;
}

static void fold_into(gmb_expr_t* e) {
    char* r = query(e);
    if (!r) return;
    e->kind = GMB_EXPR_STR;
    free(e->str); e->str = r;
    e->var = NULL; e->ns = NULL;   /* non piu' una call: la stringa e' il valore */
}

static void walk_expr(gmb_expr_t* e) {
    if (!e) return;
    if (is_call(e, "SYMBOL") || is_call(e, "IMPACT") || is_call(e, "CALLMAP") || is_call(e, "REVISION") || is_call(e, "CONTEXT") || is_call(e, "CHECKREV") || is_call(e, "PATCH") || is_call(e, "FINDSYMBOL") || is_call(e, "REPOMAP") || is_call(e, "FILEOUTLINE") || is_call(e, "VERIFY") || is_call(e, "COMMIT") || is_call(e, "INSPECT")) { fold_into(e); return; }
    switch (e->kind) {
        case GMB_EXPR_CALL: for (int i = 0; i < e->nargs; i++) walk_expr(e->args[i]); break;
        case GMB_EXPR_BINARY: walk_expr(e->left); walk_expr(e->right); break;
        case GMB_EXPR_UNARY: walk_expr(e->left); break;
        case GMB_EXPR_COMPREHENSION:
            walk_expr(e->comp_src); walk_expr(e->comp_where); walk_expr(e->comp_order);
            walk_expr(e->comp_sel); walk_expr(e->comp_key);
            for (int i = 0; i < e->n_comp_more; i++) walk_expr(e->comp_more_src[i]);
            break;
        case GMB_EXPR_LAMBDA: walk_expr(e->lambda_expr); break;
        default: break;
    }
}
static void walk_body(gmb_stmt_t** body, int n);
static void walk_stmt(gmb_stmt_t* s) {
    if (!s) return;
    switch (s->kind) {
        case GMB_STMT_FUNCTION: walk_body(s->func_body, s->n_funcbody); break;
        case GMB_STMT_LET: case GMB_STMT_ASSIGN: walk_expr(s->value); walk_expr(s->array_index); break;
        case GMB_STMT_CALL: walk_expr(s->expr); break;
        case GMB_STMT_IF: walk_expr(s->cond); walk_body(s->then_body, s->n_then); walk_body(s->else_body, s->n_else); break;
        case GMB_STMT_FOR: walk_expr(s->for_from); walk_expr(s->for_to); walk_expr(s->for_step); walk_body(s->for_body, s->n_for); break;
        case GMB_STMT_WHILE: walk_expr(s->cond); walk_body(s->while_body, s->n_while); break;
        case GMB_STMT_DO: walk_expr(s->do_until); walk_expr(s->do_while); walk_body(s->do_body, s->n_do); break;
        case GMB_STMT_PRINT: if (s->print_using) walk_expr(s->print_using); for (int i = 0; i < s->nprint; i++) walk_expr(s->print_args[i]); break;
        case GMB_STMT_RETURN: walk_expr(s->value); break;
        case GMB_STMT_EXPR: walk_expr(s->expr); break;
        case GMB_STMT_DIM: walk_expr(s->array_size); break;
        default: break;
    }
}
static void walk_body(gmb_stmt_t** body, int n) { for (int i = 0; i < n; i++) walk_stmt(body[i]); }



/* ---------- §4.10 write-side: apply dei AI PATCH (revision-bound) ---------- */
static gmb_stmt_t* g_funcs[512]; static int g_nfuncs;
static void collect_funcs(gmb_stmt_t** body, int n) {
    for (int i = 0; i < n; i++) { gmb_stmt_t* st = body[i]; if (!st) continue;
        if (st->kind == GMB_STMT_FUNCTION) { if (g_nfuncs < 512) g_funcs[g_nfuncs++] = st; collect_funcs(st->func_body, st->n_funcbody); }
        collect_funcs(st->then_body, st->n_then); collect_funcs(st->else_body, st->n_else);
        collect_funcs(st->for_body, st->n_for); collect_funcs(st->while_body, st->n_while); collect_funcs(st->do_body, st->n_do);
    }
}
static void apply_patches(gmb_stmt_t** body, int n, const char* cur_rev) {
    for (int i = 0; i < n; i++) { gmb_stmt_t* st = body[i]; if (!st) continue;
        if (st->kind == GMB_STMT_AIPATCH && st->patch_target && *st->patch_target
            && st->patch_rev && cur_rev && strcmp(st->patch_rev, cur_rev) == 0) {
            for (int t = 0; t < g_nfuncs; t++) {
                const char* fn = g_funcs[t]->func_name ? g_funcs[t]->func_name : "";
                if (strcasecmp(fn, st->patch_target) == 0) {
                    g_funcs[t]->func_body = st->then_body; g_funcs[t]->n_funcbody = st->n_then;
                    st->then_body = NULL; st->n_then = 0;   /* trasferisci la proprieta': evita il double-free */
                }
            }
        }
        apply_patches(st->then_body, st->n_then, cur_rev); apply_patches(st->else_body, st->n_else, cur_rev);
        apply_patches(st->for_body, st->n_for, cur_rev); apply_patches(st->while_body, st->n_while, cur_rev); apply_patches(st->do_body, st->n_do, cur_rev);
        apply_patches(st->func_body, st->n_funcbody, cur_rev);
    }
}
/* Hash FNV-1a delle coppie di chiamata (pari a AI.REVISION lato C#/C). */
static void cur_revision(char* out, size_t cap) {
    int np = 0; char** v = call_pairs(&np);
    unsigned long long h = 1469598103934665603ULL;
    for (int i = 0; i < np; i++) for (const char* q = v[i]; *q; q++) { h ^= (unsigned char)*q; h *= 1099511628211ULL; }
    for (int i = 0; i < np; i++) free(v[i]); free(v);
    snprintf(out, cap, "rev=%016llx", h);
}

/* Pre-pass: costruisce il grafo una volta e fonde le call AI.SYMBOL/IMPACT/CALLMAP. */
void gmb_semfold_program(gmb_stmt_t** stmts, int n, const char* file);
void gmb_semfold_program(gmb_stmt_t** stmts, int n, const char* file) {
    if (g_built) { gmb_semgraph_free(&g_g); memset(&g_g, 0, sizeof g_g); }
    gmb_semgraph_extract(&g_g, stmts, n, file); g_built = 1;
    g_npatch_revs = 0; collect_patch_revs(stmts, n);   /* rev dei patch, PRIMA del fold dei receipt */
    for (int i = 0; i < n; i++) walk_stmt(stmts[i]);
    /* §4.10 write-side: applica i AI PATCH (revision-bound) prima dell'emit. */
    { g_nfuncs = 0; collect_funcs(stmts, n); char rv[32]; cur_revision(rv, sizeof rv); apply_patches(stmts, n, rv); }
}

/* §4.10 esposizione agenti (lato C, indipendente dal C#): interroga il grafo GIÀ costruito da
 * `gmb_semfold_program` per il tool-loop `code.*`. Riusa la STESSA `query()` del fold:
 * costruisce un expr sintetico `LLM.<verb>(<arg>[, depth])` (una sola sorgente di formato,
 * nessuna duplicazione col C#). `verb` = SYMBOL|CALLMAP|IMPACT|CONTEXT|FINDSYMBOL|FILEOUTLINE|
 * REPOMAP|INSPECT. `depth < 0` = argomento assente. Ritorna 0 (out = stringa canonica) o -1. */
int gmb_semfold_query_str(const char* verb, const char* arg, int depth, char* out, size_t cap);
int gmb_semfold_query_str(const char* verb, const char* arg, int depth, char* out, size_t cap) {
    if (!out || cap == 0) return -1;
    out[0] = 0;
    if (!g_built || verb == NULL) return -1;
    gmb_expr_t e; memset(&e, 0, sizeof e);
    e.kind = GMB_EXPR_CALL; e.ns = (char*)"LLM"; e.var = (char*)verb; e.nargs = 0;
    gmb_expr_t a0, a1;
    if (arg != NULL) { memset(&a0, 0, sizeof a0); a0.kind = GMB_EXPR_STR; a0.str = (char*)arg; e.args[e.nargs++] = &a0; }
    if (depth >= 0) { memset(&a1, 0, sizeof a1); a1.kind = GMB_EXPR_NUM; a1.num = depth; e.args[e.nargs++] = &a1; }
    char* r = query(&e);
    if (r == NULL) return -1;
    snprintf(out, cap, "%s", r);
    free(r);
    return 0;
}
