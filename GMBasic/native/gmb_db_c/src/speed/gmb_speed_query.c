/* gmb_speed_query.c — motore di query SPEED (JSON) condiviso dai 3 backend C.
 *
 * Port 1:1 di PCodeProcessor.SpeedQuery.cs (SpeedExecuteSelect) e di IlRuntime.SpeedSelect:
 * opera DIRETTAMENTE sul formato JSON scritto da SPEED.PUT (gmb_dict_to_json) e letto da
 * SPEED.GET — MAI sul formato binario schema-based di Database_ExecuteQuery/QueryPlan.c
 * (che si aspetta un UDT a schema fisso e fallirebbe silenziosamente sui record JSON).
 *
 * Supporta: scan FROM, JOIN (nested-loop su ON alias.key = alias.field), WHERE, GROUP BY,
 * HAVING, proiezione con aggregati (COUNT/SUM/AVG/MIN/MAX), ORDER BY, `SELECT *`.
 * L'evaluator di espressioni è gmb_expr_eval_filter (gmb_runtime) con un get-callback che
 * risolve i nomi (qualificati "Alias.Campo" o nudi) contro la riga JSON.
 *
 * Interfaccia (piano JSON, array di stringhe — niente strutture annidate lato emitter):
 *   { "from":"Clienti", "join":"Ordini", "joinCond":"Clienti.ID = Ordini.ID_Cliente",
 *     "where":"", "having":"", "star":false,
 *     "groupBy":["Clienti.Nome"], "orderBy":["Clienti.Nome"],   // "~expr" = DESC
 *     "selE":["Clienti.Nome","SUM(Ordini.Importo)"], "selA":["NomeCliente","TotaleImporto"] }
 * Ritorna in *outJson un array JSON di oggetti {colonna: valore}.
 */
#include "gmb_db_internal.h"
#include "gmb_runtime.h"
#include "gmb_eval_scope.h"
#include <ctype.h>
#include <strings.h>

#define SQ_MAX_SEL 32

/* ── riga: gmb_dict_t con chiavi nude + qualificate "Alias.Campo" ── */
typedef struct {
    gmb_dict_t* rows;
    int n, cap;
} sq_rows_t;

static void sq_rows_init(sq_rows_t* r) { r->rows = NULL; r->n = 0; r->cap = 0; }
static void sq_rows_free(sq_rows_t* r) {
    int i;
    for (i = 0; i < r->n; i++) gmb_dict_free(&r->rows[i]);
    free(r->rows);
    r->rows = NULL; r->n = 0; r->cap = 0;
}
static gmb_dict_t* sq_rows_add(sq_rows_t* r) {
    if (r->n >= r->cap) {
        r->cap = r->cap ? r->cap * 2 : 16;
        r->rows = (gmb_dict_t*)realloc(r->rows, (size_t)r->cap * sizeof(gmb_dict_t));
    }
    memset(&r->rows[r->n], 0, sizeof(gmb_dict_t));
    return &r->rows[r->n++];
}

/* ── accesso case-insensitive a un dict ── */
static gmb_val_t sq_dget(gmb_dict_t d, const char* key) {
    size_t i;
    if (!key) return gmb_val_nil();
    for (i = 0; i < d.count; i++)
        if (d.items[i].key && strcasecmp(d.items[i].key, key) == 0) return d.items[i].value;
    return gmb_val_nil();
}
static const char* sq_str(gmb_val_t v) {
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return v.v_string.data;
    return "";
}
static double sq_num(gmb_val_t v) {
    switch (v.kind) {
        case GMB_TYPE_INT: return (double)v.v_int;
        case GMB_TYPE_DOUBLE: return v.v_double;
        case GMB_TYPE_BOOL: return v.v_bool ? 1.0 : 0.0;
        case GMB_TYPE_STRING: return v.v_string.data ? atof(v.v_string.data) : 0.0;
        default: return 0.0;
    }
}
static int sq_truthy(gmb_val_t v) {
    switch (v.kind) {
        case GMB_TYPE_NIL: return 0;
        case GMB_TYPE_BOOL: return v.v_bool;
        case GMB_TYPE_INT: return v.v_int != 0;
        case GMB_TYPE_DOUBLE: return v.v_double != 0.0;
        case GMB_TYPE_STRING: return v.v_string.data && v.v_string.data[0];
        default: return 1;
    }
}

/* ── get-callback per gmb_expr_eval_filter: risolve il nome nella riga ── */
static int sq_get(void* env, const char* name, gmb_val_t* out) {
    gmb_dict_t* d = (gmb_dict_t*)env;
    *out = sq_dget(*d, name);
    return GMB_EXPR_OK;
}

static gmb_val_t sq_eval(const char* expr, gmb_dict_t* row) {
    gmb_val_t res = gmb_val_nil();
    if (!expr || !expr[0]) return res;
    int rc = gmb_eval_filter_env(expr, sq_get, NULL, row, NULL,
                                  NULL, NULL, NULL, NULL, &res);
    if (rc != 0)
        return gmb_val_nil();
    return res;
}

/* ── confronto stile SpeedCompare (case-insensitive su stringhe) ── */
static int sq_cmp(gmb_val_t a, gmb_val_t b) {
    if (a.kind == GMB_TYPE_NIL && b.kind == GMB_TYPE_NIL) return 0;
    if (a.kind == GMB_TYPE_NIL) return -1;
    if (b.kind == GMB_TYPE_NIL) return 1;
    if (a.kind == GMB_TYPE_STRING && b.kind == GMB_TYPE_STRING)
        return strcasecmp(sq_str(a), sq_str(b));
    return (sq_num(a) < sq_num(b)) ? -1 : (sq_num(a) > sq_num(b) ? 1 : 0);
}

/* ── scansione di una tabella: ogni record JSON → dict qualificato con alias ── */
static void sq_qualify(gmb_dict_t* d, const char* alias) {
    size_t i;
    char q[160];
    if (!alias || !alias[0]) return;
    for (i = 0; i < d->count; i++) {
        snprintf(q, sizeof(q), "%s.%s", alias, d->items[i].key);
        gmb_dict_set(d, q, d->items[i].value);
    }
}
static void sq_scan(Database* db, int tableId, const char* alias, sq_rows_t* out) {
    if (!db || tableId < 0) return;
    if (!Database_GoTop(db, tableId)) return;
    while (!Database_Eof(db, tableId)) {
        uint8_t* val = NULL; int vlen = 0;
        if (Database_GetCurrent(db, tableId, &val, &vlen) && val && vlen > 0) {
            char* js = (char*)malloc((size_t)vlen + 1);
            if (js) {
                gmb_dict_t* d;
                memcpy(js, val, (size_t)vlen); js[vlen] = 0;
                d = sq_rows_add(out);
                gmb_json_parse_to_dict(js, d);
                sq_qualify(d, alias);
                free(js);
            }
        }
        if (val) free(val);
        if (!Database_Skip(db, tableId, 1)) break;
    }
}

/* ── JOIN: merge di due dict (left + right, chiavi già qualificate) ── */
static void sq_merge(gmb_dict_t* dst, gmb_dict_t* src) {
    size_t i;
    for (i = 0; i < src->count; i++) gmb_dict_set(dst, src->items[i].key, src->items[i].value);
}

/* Scompone la condizione di JOIN "X = Y" in (expr lato FROM, campo lato JOIN) —
 * come DecomposeJoinConditionRuntime: vince il lato che porta il prefisso fromAlias. */
static void sq_decompose_join(const char* cond, const char* fromAlias, const char* joinAlias,
                              char* leftOut, int leftCap, char* rightOut, int rightCap) {
    const char* eq; const char* p; const char* start;
    char lhs[160] = "", rhs[160] = "";
    leftOut[0] = 0; rightOut[0] = 0;
    if (!cond) return;
    eq = strchr(cond, '=');
    if (!eq) { snprintf(leftOut, leftCap, "%s", cond); return; }
    /* lato sinistro: trim */
    start = cond; while (start < eq && isspace((unsigned char)*start)) start++;
    p = eq; while (p > start && isspace((unsigned char)p[-1])) p--;
    { int n = (int)(p - start); if (n > 159) n = 159; memcpy(lhs, start, (size_t)n); lhs[n] = 0; }
    /* lato destro: trim */
    start = eq + 1; while (*start && isspace((unsigned char)*start)) start++;
    p = start + strlen(start); while (p > start && isspace((unsigned char)p[-1])) p--;
    { int n = (int)(p - start); if (n > 159) n = 159; memcpy(rhs, start, (size_t)n); rhs[n] = 0; }
    {
        char lpre[80], rpre[80];
        snprintf(lpre, sizeof(lpre), "%s.", fromAlias ? fromAlias : "");
        snprintf(rpre, sizeof(rpre), "%s.", joinAlias ? joinAlias : "");
        if (strncasecmp(lhs, lpre, strlen(lpre)) == 0) {
            snprintf(leftOut, leftCap, "%s", lhs);
            { const char* dot = strrchr(rhs, '.'); snprintf(rightOut, rightCap, "%s", dot ? dot + 1 : rhs); }
        } else if (strncasecmp(rhs, lpre, strlen(lpre)) == 0) {
            snprintf(leftOut, leftCap, "%s", rhs);
            { const char* dot = strrchr(lhs, '.'); snprintf(rightOut, rightCap, "%s", dot ? dot + 1 : lhs); }
        } else {
            snprintf(leftOut, leftCap, "%s", lhs);
            { const char* dot = strrchr(rhs, '.'); snprintf(rightOut, rightCap, "%s", dot ? dot + 1 : rhs); }
        }
    }
}

/* ── aggregati: riconosce FUNC(inner) come proiezione aggregata ── */
static int sq_detect_agg(const char* expr, char* funcOut, int funcCap, char* innerOut, int innerCap) {
    static const char* F[] = { "COUNT", "SUM", "AVG", "MIN", "MAX" };
    const char* lp; const char* nameStart; const char* nameEnd;
    size_t i, fl, nl;
    if (!expr) return 0;
    while (*expr && isspace((unsigned char)*expr)) expr++;
    /* Nome funzione = identificatore immediatamente prima di '(' (dopo l'ultimo '.'):
     * accetta anche la qualifica namespace (`MATH.Sum(...)` → "Sum"). */
    lp = strchr(expr, '(');
    if (!lp) return 0;
    nameEnd = lp;
    nameStart = nameEnd;
    while (nameStart > expr && (isalnum((unsigned char)nameStart[-1]) || nameStart[-1] == '_')) nameStart--;
    nl = (size_t)(nameEnd - nameStart);
    for (i = 0; i < sizeof(F) / sizeof(F[0]); i++) {
        fl = strlen(F[i]);
        if (nl == fl && strncasecmp(nameStart, F[i], fl) == 0) {
            const char* inner = lp + 1;
            size_t il = strlen(inner);
            while (il > 0 && (inner[il - 1] == ')' || isspace((unsigned char)inner[il - 1]))) il--;
            snprintf(funcOut, funcCap, "%s", F[i]);
            if (il > (size_t)(innerCap - 1)) il = (size_t)(innerCap - 1);
            memcpy(innerOut, inner, il); innerOut[il] = 0;
            return 1;
        }
    }
    return 0;
}
static gmb_val_t sq_aggregate(const char* func, gmb_dict_t* group, int groupN, const char* inner) {
    int i;
    double acc = 0.0; int cnt = 0; double mn = 0, mx = 0;
    if (strcasecmp(func, "COUNT") == 0 && (!inner || !inner[0] || strcmp(inner, "*") == 0))
        return gmb_make_val_double((double)groupN);
    for (i = 0; i < groupN; i++) {
        gmb_val_t v = sq_eval(inner, &group[i]);
        if (v.kind == GMB_TYPE_NIL) continue;
        {
            double d = sq_num(v);
            if (cnt == 0) { mn = d; mx = d; } else { if (d < mn) mn = d; if (d > mx) mx = d; }
            acc += d; cnt++;
        }
    }
    if (strcasecmp(func, "SUM") == 0) return gmb_make_val_double(acc);
    if (strcasecmp(func, "AVG") == 0) return gmb_make_val_double(cnt > 0 ? acc / cnt : 0.0);
    if (strcasecmp(func, "MIN") == 0) return cnt > 0 ? gmb_make_val_double(mn) : gmb_val_nil();
    if (strcasecmp(func, "MAX") == 0) return cnt > 0 ? gmb_make_val_double(mx) : gmb_val_nil();
    return gmb_val_nil();
}

/* nome colonna di default da un'espressione: ultimo segmento dopo '.' */
static void sq_display_name(const char* expr, char* out, int cap) {
    const char* dot;
    if (!expr || !expr[0]) { snprintf(out, cap, "expr"); return; }
    dot = strrchr(expr, '.');
    snprintf(out, cap, "%s", dot ? dot + 1 : expr);
}

/* ── ORDER BY: insertion sort stabile su una lista di dict ── */
static int sq_order_cmp(gmb_dict_t* a, gmb_dict_t* b, const char** exprs, const int* descs, int n) {
    int i;
    for (i = 0; i < n; i++) {
        gmb_val_t va = sq_eval(exprs[i], a);
        gmb_val_t vb = sq_eval(exprs[i], b);
        int c = sq_cmp(va, vb);
        if (c != 0) return descs[i] ? -c : c;
    }
    return 0;
}

int gmb_db_speed_select(int64_t h, const char* planJson, gmb_string_t* outJson)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    Database* db;
    gmb_dict_t plan;
    char fromAlias[64], joinAlias[64], joinCond[256], where[256], having[256];
    char joinLeft[160], joinRight[64];
    int fromTable, joinTable, star;
    const char* selE[SQ_MAX_SEL]; const char* selA[SQ_MAX_SEL]; int nSel = 0;
    const char* groupBy[SQ_MAX_SEL]; int nGroup = 0;
    const char* orderE[SQ_MAX_SEL]; int orderD[SQ_MAX_SEL]; int nOrder = 0;
    sq_rows_t rows, joined, result;
    int i, j, hasAgg = 0;
    char aggFunc[SQ_MAX_SEL][16]; char aggInner[SQ_MAX_SEL][128]; int isAgg[SQ_MAX_SEL];
    gmb_val_t vSelE, vSelA, vGroup, vOrder;

    if (outJson) *outJson = gmb_string_alloc("");
    if (!hh || !hh->u.speed.db || !planJson) return GMB_DB_ERR;
    db = hh->u.speed.db;

    memset(&plan, 0, sizeof(plan));
    gmb_json_parse_to_dict(planJson, &plan);

    snprintf(fromAlias, sizeof(fromAlias), "%s", sq_str(sq_dget(plan, "from")));
    snprintf(joinAlias, sizeof(joinAlias), "%s", sq_str(sq_dget(plan, "join")));
    snprintf(joinCond, sizeof(joinCond), "%s", sq_str(sq_dget(plan, "joinCond")));
    snprintf(where, sizeof(where), "%s", sq_str(sq_dget(plan, "where")));
    snprintf(having, sizeof(having), "%s", sq_str(sq_dget(plan, "having")));
    star = sq_truthy(sq_dget(plan, "star"));

    fromTable = gmb_db_speed_table_by_alias(fromAlias);
    joinTable = joinAlias[0] ? gmb_db_speed_table_by_alias(joinAlias) : -1;
    if (fromTable < 0) fromTable = gmb_db_speed_curtable(h);
    if (fromTable < 0) { gmb_dict_free(&plan); return GMB_DB_ERR; }

    vSelE = sq_dget(plan, "selE"); vSelA = sq_dget(plan, "selA");
    vGroup = sq_dget(plan, "groupBy"); vOrder = sq_dget(plan, "orderBy");
    {
        gmb_list_t le = gmb_val_as_list(vSelE), la = gmb_val_as_list(vSelA);
        int ne = (int)gmb_list_count(le), na = (int)gmb_list_count(la);
        for (i = 0; i < ne && nSel < SQ_MAX_SEL; i++) {
            const char* e = sq_str(gmb_list_get(le, i));
            selE[nSel] = e;
            selA[nSel] = (i < na) ? sq_str(gmb_list_get(la, i)) : "";
            nSel++;
        }
    }
    {
        gmb_list_t lg = gmb_val_as_list(vGroup);
        int ng = (int)gmb_list_count(lg);
        for (i = 0; i < ng && nGroup < SQ_MAX_SEL; i++) groupBy[nGroup++] = sq_str(gmb_list_get(lg, i));
    }
    {
        gmb_list_t lo = gmb_val_as_list(vOrder);
        int no = (int)gmb_list_count(lo);
        for (i = 0; i < no && nOrder < SQ_MAX_SEL; i++) {
            const char* e = sq_str(gmb_list_get(lo, i));
            if (e[0] == '~') { orderD[nOrder] = 1; orderE[nOrder] = e + 1; }
            else { orderD[nOrder] = 0; orderE[nOrder] = e; }
            nOrder++;
        }
    }

    for (i = 0; i < nSel; i++) {
        char fn[16], in[128];
        isAgg[i] = sq_detect_agg(selE[i], fn, (int)sizeof(fn), in, (int)sizeof(in));
        if (isAgg[i]) { snprintf(aggFunc[i], sizeof(aggFunc[i]), "%s", fn); snprintf(aggInner[i], sizeof(aggInner[i]), "%s", in); hasAgg = 1; }
    }

    sq_rows_init(&rows); sq_rows_init(&joined); sq_rows_init(&result);
    sq_scan(db, fromTable, fromAlias, &rows);

    if (joinAlias[0] && joinTable >= 0) {
        sq_rows_t right;
        sq_rows_init(&right);
        sq_scan(db, joinTable, joinAlias, &right);
        sq_decompose_join(joinCond, fromAlias, joinAlias, joinLeft, (int)sizeof(joinLeft), joinRight, (int)sizeof(joinRight));
        for (i = 0; i < rows.n; i++) {
            gmb_val_t lk = sq_eval(joinLeft, &rows.rows[i]);
            for (j = 0; j < right.n; j++) {
                gmb_val_t rv = sq_dget(right.rows[j], joinRight);
                if (sq_cmp(lk, rv) != 0) continue;
                {
                    gmb_dict_t* m = sq_rows_add(&joined);
                    sq_merge(m, &rows.rows[i]);
                    sq_merge(m, &right.rows[j]);
                }
            }
        }
        sq_rows_free(&right);
    } else {
        for (i = 0; i < rows.n; i++) {
            gmb_dict_t* m = sq_rows_add(&joined);
            sq_merge(m, &rows.rows[i]);
        }
    }

    /* WHERE */
    if (where[0]) {
        int w = 0;
        for (i = 0; i < joined.n; i++)
            if (sq_truthy(sq_eval(where, &joined.rows[i]))) joined.rows[w++] = joined.rows[i];
        joined.n = w;
    }

    if (nGroup > 0 || hasAgg) {
        int* used = (int*)calloc((size_t)(joined.n > 0 ? joined.n : 1), sizeof(int));
        for (i = 0; i < joined.n; i++) {
            char key[512]; int k;
            gmb_dict_t* outRow;
            if (used[i]) continue;
            key[0] = 0;
            for (k = 0; k < nGroup; k++) {
                gmb_val_t gv = sq_eval(groupBy[k], &joined.rows[i]);
                char part[128];
                if (gv.kind == GMB_TYPE_STRING) snprintf(part, sizeof(part), "%s", sq_str(gv));
                else snprintf(part, sizeof(part), "%.10g", sq_num(gv));
                if (k) strncat(key, "|", sizeof(key) - strlen(key) - 1);
                strncat(key, part, sizeof(key) - strlen(key) - 1);
            }
            /* raccogli il gruppo */
            {
                sq_rows_t grp; sq_rows_init(&grp);
                for (j = i; j < joined.n; j++) {
                    char jk[512]; int m;
                    if (used[j]) continue;
                    jk[0] = 0;
                    for (m = 0; m < nGroup; m++) {
                        gmb_val_t gv = sq_eval(groupBy[m], &joined.rows[j]);
                        char part[128];
                        if (gv.kind == GMB_TYPE_STRING) snprintf(part, sizeof(part), "%s", sq_str(gv));
                        else snprintf(part, sizeof(part), "%.10g", sq_num(gv));
                        if (m) strncat(jk, "|", sizeof(jk) - strlen(jk) - 1);
                        strncat(jk, part, sizeof(jk) - strlen(jk) - 1);
                    }
                    if (strcmp(jk, key) == 0) { used[j] = 1; sq_merge(sq_rows_add(&grp), &joined.rows[j]); }
                }
                outRow = sq_rows_add(&result);
                for (k = 0; k < nSel; k++) {
                    char name[160];
                    if (selA[k] && selA[k][0]) snprintf(name, sizeof(name), "%s", selA[k]);
                    else sq_display_name(selE[k], name, (int)sizeof(name));
                    if (isAgg[k]) gmb_dict_set(outRow, name, sq_aggregate(aggFunc[k], grp.rows, grp.n, aggInner[k]));
                    else if (grp.n > 0) gmb_dict_set(outRow, name, sq_eval(selE[k], &grp.rows[0]));
                }
                sq_rows_free(&grp);
            }
        }
        free(used);
        /* HAVING */
        if (having[0]) {
            int w = 0;
            for (i = 0; i < result.n; i++)
                if (sq_truthy(sq_eval(having, &result.rows[i]))) result.rows[w++] = result.rows[i];
            result.n = w;
        }
    } else if (star || nSel == 0) {
        for (i = 0; i < joined.n; i++) {
            gmb_dict_t* outRow = sq_rows_add(&result);
            for (j = 0; (size_t)j < joined.rows[i].count; j++) {
                const char* k = joined.rows[i].items[j].key;
                if (strchr(k, '.')) continue; /* salta le chiavi qualificate */
                gmb_dict_set(outRow, k, joined.rows[i].items[j].value);
            }
        }
    } else {
        for (i = 0; i < joined.n; i++) {
            gmb_dict_t* outRow = sq_rows_add(&result);
            for (j = 0; j < nSel; j++) {
                char name[160];
                if (selA[j] && selA[j][0]) snprintf(name, sizeof(name), "%s", selA[j]);
                else sq_display_name(selE[j], name, (int)sizeof(name));
                gmb_dict_set(outRow, name, sq_eval(selE[j], &joined.rows[i]));
            }
        }
    }

    /* ORDER BY: insertion sort stabile */
    for (i = 1; i < result.n; i++) {
        gmb_dict_t key = result.rows[i];
        j = i - 1;
        while (j >= 0 && sq_order_cmp(&result.rows[j], &key, orderE, orderD, nOrder) > 0) {
            result.rows[j + 1] = result.rows[j];
            j--;
        }
        result.rows[j + 1] = key;
    }

    /* serializza array JSON di oggetti */
    if (outJson) {
        char* buf = NULL; size_t cap = 0, pos = 0;
        buf = (char*)malloc(64); cap = 64;
        if (buf) {
            buf[pos++] = '['; buf[pos] = 0;
            for (i = 0; i < result.n; i++) {
                if (i) {
                    if (pos + 2 > cap) { cap = (pos + 2) * 2; buf = (char*)realloc(buf, cap); }
                    buf[pos++] = ',';
                }
                gmb_json_append_dict(&result.rows[i], &buf, &cap, &pos);
            }
            if (pos + 2 > cap) { cap = (pos + 2) * 2; buf = (char*)realloc(buf, cap); }
            buf[pos++] = ']'; buf[pos] = 0;
            *outJson = gmb_string_alloc(buf);
            free(buf);
        }
    }

    sq_rows_free(&rows); sq_rows_free(&joined); sq_rows_free(&result);
    gmb_dict_free(&plan);
    return GMB_DB_OK;
}
