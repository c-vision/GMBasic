/* gmb_resolver.c — Resolver C (tipi/membri/arity). Compilatore C autonomo. */
#include "gmb_resolver.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* conta i parametri da una firma CSV ("a,b,c" -> 3; "" -> 0) */
static int sig_arity(const char* sig) {
    if (!sig || !*sig) return 0;
    int n = 1;
    for (const char* p = sig; *p; p++) if (*p == ',') n++;
    return n;
}

/* risolve il tipo di un identificatore semplice: simbolo (variabile/param -> type_name) o tipo. */
static int resolve_atom(const gmb_symtab_t* st, const gmb_typereg_t* tr, const char* name,
                        char* out, size_t cap) {
    int id = gmb_symtab_resolve((gmb_symtab_t*)st, name);
    if (id < 0) id = gmb_symtab_find_any((gmb_symtab_t*)st, name);
    if (id >= 0) {
        gmb_sym_t* s = gmb_symtab_get((gmb_symtab_t*)st, id);
        if (s && s->type_name && *s->type_name) { snprintf(out, cap, "%s", s->type_name); return 0; }
    }
    if (gmb_type_is_builtin(name) || gmb_typereg_find(tr, name)) { snprintf(out, cap, "%s", name); return 0; }
    return -1;
}

int gmb_resolve_name_type(const gmb_symtab_t* st, const gmb_typereg_t* tr,
                          const char* dotted, char* out, size_t cap,
                          char* base_out, size_t bcap) {
    if (!dotted || !*dotted) return -1;
    char buf[256]; snprintf(buf, sizeof buf, "%s", dotted);
    char* dot = strchr(buf, '.');
    char base[128]; snprintf(base, sizeof base, "%s", dot ? buf : buf);
    if (dot) *dot = 0;
    char cur[128]; snprintf(cur, sizeof cur, "%s", buf);
    if (base_out && bcap) snprintf(base_out, bcap, "%s", cur);
    char ty[128];
    if (resolve_atom(st, tr, cur, ty, sizeof ty) != 0) return -1;
    char* rest = dot ? dot + 1 : NULL;
    while (rest && *rest) {
        char* ndot = strchr(rest, '.');
        char member[128]; snprintf(member, sizeof member, "%s", rest);
        if (ndot) member[ndot - rest] = 0;
        const gmb_field_t* f = gmb_typereg_field(tr, ty, member);
        if (!f || !*f->type) return -1;
        snprintf(ty, sizeof ty, "%s", f->type);
        if (ndot && bcap && base_out) snprintf(base_out, bcap, "%s", member);
        rest = ndot ? ndot + 1 : NULL;
    }
    if (out && cap) snprintf(out, cap, "%s", ty);
    return 0;
}

int gmb_resolve_member(const gmb_symtab_t* st, const gmb_typereg_t* tr,
                       const char* obj, const char* member, char* err, size_t ecap) {
    char ty[128]; if (gmb_resolve_name_type(st, tr, obj, ty, sizeof ty, NULL, 0) != 0) {
        snprintf(err, ecap, "oggetto '%s' non risolto", obj ? obj : "?"); return -1;
    }
    if (gmb_type_is_builtin(ty)) return 0;   /* membri di tipi builtin: non verificati qui */
    if (!gmb_typereg_find(tr, ty)) return 0; /* tipo non registrato: niente da verificare */
    if (!gmb_typereg_field(tr, ty, member)) {
        snprintf(err, ecap, "membro '%s' inesistente sul tipo '%s'", member, ty); return -1;
    }
    return 0;
}

int gmb_resolve_call_arity(const gmb_symtab_t* st, const char* name, int argc, char* err, size_t ecap) {
    if (!name) return 0;
    int id = gmb_symtab_resolve((gmb_symtab_t*)st, name);
    if (id < 0) id = gmb_symtab_find_any((gmb_symtab_t*)st, name);
    if (id < 0) return 0;   /* non una procedura nota (builtin/esterno): nessun check */
    gmb_sym_t* s = gmb_symtab_get((gmb_symtab_t*)st, id);
    if (!s || (s->kind != GMB_SYM_PROCEDURE && s->kind != GMB_SYM_FUNCTION)) return 0;
    int want = sig_arity(s->signature);
    if (argc != want) {
        snprintf(err, ecap, "arity di '%s': attesi %d, ricevuti %d", name, want, argc); return -1;
    }
    return 0;
}

/* Cerca la direttiva IMPORT per `alias` (o ns) nell'AST. Ritorna 0 + ns in out, o -1. */
static int imp_scan(gmb_stmt_t** stmts, int n, const char* alias, char* out, size_t cap) {
    for (int i = 0; i < n; i++) {
        gmb_stmt_t* s = stmts[i];
        if (!s) continue;
        if (s->kind == GMB_STMT_IMPORT) {
            if ((s->imp_alias && strcasecmp(s->imp_alias, alias) == 0) ||
                (s->imp_ns && strcasecmp(s->imp_ns, alias) == 0)) {
                snprintf(out, cap, "%s", s->imp_ns ? s->imp_ns : "");
                return 0;
            }
        }
        if (imp_scan(s->then_body, s->n_then, alias, out, cap) == 0) return 0;
        if (imp_scan(s->else_body, s->n_else, alias, out, cap) == 0) return 0;
        if (imp_scan(s->func_body, s->n_funcbody, alias, out, cap) == 0) return 0;
    }
    return -1;
}
int gmb_resolve_import(const gmb_stmt_t** stmts, int n, const char* alias, char* out, size_t cap) {
    return imp_scan((gmb_stmt_t**)stmts, n, alias, out, cap);
}

static int imp_member_scan(gmb_stmt_t** stmts, int n, const char* alias, const char* member, char* out, size_t cap) {
    for (int i = 0; i < n; i++) {
        gmb_stmt_t* s = stmts[i];
        if (!s) continue;
        if (s->kind == GMB_STMT_IMPORT) {
            int match = (s->imp_alias && strcasecmp(s->imp_alias, alias) == 0) ||
                        (s->imp_ns && strcasecmp(s->imp_ns, alias) == 0);
            if (match) {
                if (s->n_imp_members == 0) { snprintf(out, cap, "%s", s->imp_ns ? s->imp_ns : ""); return 0; }
                for (int mi = 0; mi < s->n_imp_members; mi++)
                    if (strcasecmp(s->imp_members[mi], member) == 0) { snprintf(out, cap, "%s", s->imp_ns ? s->imp_ns : ""); return 0; }
                return -1;
            }
        }
        if (imp_member_scan(s->then_body, s->n_then, alias, member, out, cap) == 0) return 0;
        if (imp_member_scan(s->else_body, s->n_else, alias, member, out, cap) == 0) return 0;
        if (imp_member_scan(s->func_body, s->n_funcbody, alias, member, out, cap) == 0) return 0;
    }
    return -1;
}
int gmb_resolve_import_member(const gmb_stmt_t** stmts, int n, const char* alias, const char* member, char* out, size_t cap) {
    return imp_member_scan((gmb_stmt_t**)stmts, n, alias, member, out, cap);
}

int gmb_resolve_overload(const gmb_symtab_t* st, const char* name, int argc, int* out_id, char* err, size_t ecap) {
    if (out_id) *out_id = -1;
    if (!st || !name) return -1;
    int ids[64]; int m = gmb_symtab_find_all((gmb_symtab_t*)st, name, ids, 64);
    int fallback = -1;
    for (int i = 0; i < m; i++) {
        gmb_sym_t* sy = gmb_symtab_get((gmb_symtab_t*)st, ids[i]);
        if (!sy) continue;
        if (sy->kind != GMB_SYM_PROCEDURE && sy->kind != GMB_SYM_FUNCTION && sy->kind != GMB_SYM_METHOD) continue;
        if (sig_arity(sy->signature) == argc) { if (out_id) *out_id = ids[i]; return 0; }
        if (fallback < 0) fallback = ids[i];
    }
    if (fallback >= 0) {  /* nessuna arità esatta: nessun overload combacia */
        if (err) snprintf(err, ecap, "nessun overload di '%s' con %d argomenti", name, argc);
        return -1;
    }
    if (err) snprintf(err, ecap, "'%s' non e' una procedura overloadabile", name);
    return -1;
}
