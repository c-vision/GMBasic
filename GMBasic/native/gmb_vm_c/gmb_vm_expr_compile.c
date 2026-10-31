/* ============================================================================
 * gmb_vm_expr_compile.c — Fase 8: AST &/EVAL → snippet opcode VM.
 * Sottoinsieme: LIT, ID globale, UN +/-/NOT, BIN arith/cmp/AND/OR,
 * ASSIGN, SEQ, IF. CALL/PRINT/& nested → fallback tree-walk.
 * Const pool locale (non si tocca il chunk). OP_HALT termina lo snippet,
 * non la VM. ponytail: cache globale sotto mutex (ceiling: & concorrenti
 * serializzati; upgrade: cache per-chunk con refcount).
 * ============================================================================ */
#include "gmb_vm_expr_compile.h"

#include "gmb_adapter.h"
#include "gmb_expr.h"
#include "gmb_vm_value.h"
#include "gmb_vm_core.h"   /* gmb_raise_basic_error: ERR 11 su divisione per zero */

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum {
    OP_PUSH_CONST = 0,
    OP_PUSH_NIL = 1,
    OP_PUSH_TRUE = 2,
    OP_PUSH_FALSE = 3,
    OP_POP = 4,
    OP_DUP = 5,
    OP_ADD = 6,
    OP_SUBTRACT = 7,
    OP_MULTIPLY = 8,
    OP_DIVIDE = 9,
    OP_POWER = 11,
    OP_NEGATE = 12,
    OP_NOT = 13,
    OP_MOD = 14,
    OP_CMP_EQ = 18,
    OP_CMP_NE = 19,
    OP_CMP_GT = 20,
    OP_CMP_GTE = 21,
    OP_CMP_LT = 22,
    OP_CMP_LTE = 23,
    OP_LOAD_GLOBAL = 25,
    OP_STORE_GLOBAL = 26,
    OP_LOAD_LOCAL = 27,
    OP_STORE_LOCAL = 28,
    OP_JUMP_FORWARD = 51,
    OP_JUMP_IF_TRUE = 53,
    OP_JUMP_IF_FALSE = 54,
    OP_HALT = 63
};

typedef struct {
    uint8_t* code;
    int len, cap;
    gmb_val_t* consts;
    int nconst, capconst;
} emit_t;

#define CACHE_N 48
typedef struct {
    char* src;
    int as_stmt;
    const gmb_chunk_t* chunk;
    /* S_eval: gli slot locali dipendono dalla procedura chiamante (stesso testo in
     * procedure diverse → slot diversi): l'indirizzo di inizio proc fa parte della chiave. */
    int proc_addr;
    int compiled;
    uint8_t* code;
    int code_len;
    gmb_val_t* consts;
    int nconst;
} cache_ent;

static cache_ent g_cache[CACHE_N];
static int g_cache_n;
static int g_cache_rr;
static pthread_mutex_t g_cache_mu = PTHREAD_MUTEX_INITIALIZER;

static void emit_byte(emit_t* e, uint8_t b)
{
    if (e->len >= e->cap) {
        int nc = e->cap ? e->cap * 2 : 64;
        uint8_t* p = (uint8_t*)realloc(e->code, (size_t)nc);
        if (!p) return;
        e->code = p;
        e->cap = nc;
    }
    if (e->code) e->code[e->len++] = b;
}

static void emit_u16(emit_t* e, int v)
{
    emit_byte(e, (uint8_t)((v >> 8) & 0xff));
    emit_byte(e, (uint8_t)(v & 0xff));
}

static int emit_jump(emit_t* e, uint8_t op)
{
    int at = e->len;
    emit_byte(e, op);
    emit_u16(e, 0);
    return at;
}

static void patch_jump(emit_t* e, int at)
{
    int off = e->len - (at + 3);
    if (off < 0) off = 0;
    if (off > 65535) off = 65535;
    if (!e->code || at + 2 >= e->len) return;
    e->code[at + 1] = (uint8_t)((off >> 8) & 0xff);
    e->code[at + 2] = (uint8_t)(off & 0xff);
}

static gmb_val_t copy_val(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_STRING)
        return gmb_make_val_string(v.v_string.data ? v.v_string.data : "");
    return v;
}

static void free_val_str(gmb_val_t* v)
{
    if (v && v->kind == GMB_TYPE_STRING) gmb_string_free(&v->v_string);
}

static int add_const(emit_t* e, gmb_val_t v)
{
    if (e->nconst >= e->capconst) {
        int nc = e->capconst ? e->capconst * 2 : 8;
        gmb_val_t* p = (gmb_val_t*)realloc(e->consts, (size_t)nc * sizeof(gmb_val_t));
        if (!p) return 0;
        e->consts = p;
        e->capconst = nc;
    }
    e->consts[e->nconst] = copy_val(v);
    return e->nconst++;
}

static int find_global(const gmb_chunk_t* chunk, const char* name)
{
    if (!chunk || !name || !chunk->global_names) return -1;
    for (uint32_t i = 0; i < chunk->global_name_count; i++) {
        const char* n = chunk->global_names[i];
        if (n && n[0] && strcasecmp(n, name) == 0) return (int)i;
    }
    return -1;
}

/* S_eval: slot locale per nome nella procedura che contiene ip (-1 se ignoto).
 * Delega a gmb_chunk_local_slot (confronto case-insensitive). */
static int find_local(const gmb_chunk_t* chunk, int ip, const char* name)
{
    if (!chunk || chunk->ploc_count == 0) return -1;
    return gmb_chunk_local_slot(chunk, ip, name);
}

static int can_compile(const gmb_expr_node* n, const gmb_chunk_t* chunk, int ip)
{
    if (!n) return 0;
    switch (n->kind) {
        case GMB_XN_LIT: return 1;
        case GMB_XN_ID: return find_local(chunk, ip, n->name) >= 0 || find_global(chunk, n->name) >= 0;
        case GMB_XN_UN:
            if (n->op != GMB_XT_PLUS && n->op != GMB_XT_MINUS && n->op != GMB_XT_NOT) return 0;
            return can_compile(n->a, chunk, ip);
        case GMB_XN_BIN:
            switch (n->op) {
                case GMB_XT_PLUS: case GMB_XT_MINUS: case GMB_XT_STAR: case GMB_XT_SLASH:
                case GMB_XT_PERCENT: case GMB_XT_CARET:
                case GMB_XT_EQ: case GMB_XT_NE: case GMB_XT_LT: case GMB_XT_GT:
                case GMB_XT_LE: case GMB_XT_GE:
                case GMB_XT_AND: case GMB_XT_OR:
                    return can_compile(n->a, chunk, ip) && can_compile(n->b, chunk, ip);
                default: return 0;
            }
        case GMB_XN_ASSIGN:
            return (find_local(chunk, ip, n->name) >= 0 || find_global(chunk, n->name) >= 0)
                && can_compile(n->a, chunk, ip);
        case GMB_XN_SEQ: {
            if (n->nargs <= 0) return 0;
            for (int i = 0; i < n->nargs; i++)
                if (!can_compile(n->args[i], chunk, ip)) return 0;
            return 1;
        }
        case GMB_XN_IF:
            if (!can_compile(n->a, chunk, ip) || !can_compile(n->b, chunk, ip)) return 0;
            if (n->nargs >= 1 && !can_compile(n->args[0], chunk, ip)) return 0;
            return 1;
        default:
            return 0;
    }
}

static uint8_t bin_opcode(int tok)
{
    switch (tok) {
        case GMB_XT_PLUS: return OP_ADD;
        case GMB_XT_MINUS: return OP_SUBTRACT;
        case GMB_XT_STAR: return OP_MULTIPLY;
        case GMB_XT_SLASH: return OP_DIVIDE;
        case GMB_XT_PERCENT: return OP_MOD;
        case GMB_XT_CARET: return OP_POWER;
        case GMB_XT_EQ: return OP_CMP_EQ;
        case GMB_XT_NE: return OP_CMP_NE;
        case GMB_XT_LT: return OP_CMP_LT;
        case GMB_XT_GT: return OP_CMP_GT;
        case GMB_XT_LE: return OP_CMP_LTE;
        case GMB_XT_GE: return OP_CMP_GTE;
        default: return OP_HALT;
    }
}

static int compile_node(emit_t* e, const gmb_expr_node* n, const gmb_chunk_t* chunk, int ip)
{
    if (!n) return 0;
    switch (n->kind) {
        case GMB_XN_LIT:
            if (n->lit.kind == GMB_TYPE_NIL) { emit_byte(e, OP_PUSH_NIL); return 1; }
            if (n->lit.kind == GMB_TYPE_BOOL) {
                emit_byte(e, n->lit.v_bool ? OP_PUSH_TRUE : OP_PUSH_FALSE);
                return 1;
            }
            emit_byte(e, OP_PUSH_CONST);
            emit_u16(e, add_const(e, n->lit));
            return 1;
        case GMB_XN_ID: {
            /* S_eval: locali-prima-dei-globali (specchio vm_expr_get). */
            int lslot = find_local(chunk, ip, n->name);
            if (lslot >= 0) {
                emit_byte(e, OP_LOAD_LOCAL);
                emit_u16(e, lslot);
                return 1;
            }
            int slot = find_global(chunk, n->name);
            if (slot < 0) return 0;
            emit_byte(e, OP_LOAD_GLOBAL);
            emit_u16(e, slot);
            return 1;
        }
        case GMB_XN_UN:
            if (!compile_node(e, n->a, chunk, ip)) return 0;
            if (n->op == GMB_XT_MINUS) emit_byte(e, OP_NEGATE);
            else if (n->op == GMB_XT_NOT) emit_byte(e, OP_NOT);
            return 1;
        case GMB_XN_BIN:
            if (n->op == GMB_XT_AND) {
                if (!compile_node(e, n->a, chunk, ip)) return 0;
                int jf1 = emit_jump(e, OP_JUMP_IF_FALSE);
                if (!compile_node(e, n->b, chunk, ip)) return 0;
                int jf2 = emit_jump(e, OP_JUMP_IF_FALSE);
                emit_byte(e, OP_PUSH_TRUE);
                int jend = emit_jump(e, OP_JUMP_FORWARD);
                patch_jump(e, jf1);
                patch_jump(e, jf2);
                emit_byte(e, OP_PUSH_FALSE);
                patch_jump(e, jend);
                return 1;
            }
            if (n->op == GMB_XT_OR) {
                if (!compile_node(e, n->a, chunk, ip)) return 0;
                int jt1 = emit_jump(e, OP_JUMP_IF_TRUE);
                if (!compile_node(e, n->b, chunk, ip)) return 0;
                int jt2 = emit_jump(e, OP_JUMP_IF_TRUE);
                emit_byte(e, OP_PUSH_FALSE);
                int jend = emit_jump(e, OP_JUMP_FORWARD);
                patch_jump(e, jt1);
                patch_jump(e, jt2);
                emit_byte(e, OP_PUSH_TRUE);
                patch_jump(e, jend);
                return 1;
            }
            if (!compile_node(e, n->a, chunk, ip) || !compile_node(e, n->b, chunk, ip)) return 0;
            emit_byte(e, bin_opcode(n->op));
            return 1;
        case GMB_XN_ASSIGN: {
            /* S_eval: write-back in place sulla locale (specchio vm_expr_set). */
            int lslot = find_local(chunk, ip, n->name);
            if (!compile_node(e, n->a, chunk, ip)) return 0;
            emit_byte(e, OP_DUP);
            if (lslot >= 0) {
                emit_byte(e, OP_STORE_LOCAL);
                emit_u16(e, lslot);
            } else {
                int slot = find_global(chunk, n->name);
                if (slot < 0) return 0;
                emit_byte(e, OP_STORE_GLOBAL);
                emit_u16(e, slot);
            }
            return 1;
        }
        case GMB_XN_SEQ:
            for (int i = 0; i < n->nargs; i++) {
                if (!compile_node(e, n->args[i], chunk, ip)) return 0;
                if (i + 1 < n->nargs) emit_byte(e, OP_POP);
            }
            return n->nargs > 0;
        case GMB_XN_IF: {
            if (!compile_node(e, n->a, chunk, ip)) return 0;
            int jf = emit_jump(e, OP_JUMP_IF_FALSE);
            if (!compile_node(e, n->b, chunk, ip)) return 0;
            int jend = emit_jump(e, OP_JUMP_FORWARD);
            patch_jump(e, jf);
            if (n->nargs >= 1) {
                if (!compile_node(e, n->args[0], chunk, ip)) return 0;
            } else {
                emit_byte(e, OP_PUSH_NIL);
            }
            patch_jump(e, jend);
            return 1;
        }
        default:
            return 0;
    }
}

static void cache_clear(cache_ent* e)
{
    if (!e) return;
    free(e->src);
    free(e->code);
    if (e->consts) {
        for (int i = 0; i < e->nconst; i++) free_val_str(&e->consts[i]);
        free(e->consts);
    }
    memset(e, 0, sizeof(*e));
}

static cache_ent* cache_find(const char* src, int as_stmt, const gmb_chunk_t* chunk, int proc_addr)
{
    for (int i = 0; i < g_cache_n; i++) {
        cache_ent* e = &g_cache[i];
        if (e->as_stmt == as_stmt && e->chunk == chunk && e->proc_addr == proc_addr && e->src && src && strcmp(e->src, src) == 0)
            return e;
    }
    return NULL;
}

static cache_ent* cache_insert(const char* src, int as_stmt, const gmb_chunk_t* chunk, int proc_addr)
{
    cache_ent* e;
    if (g_cache_n < CACHE_N) {
        e = &g_cache[g_cache_n++];
    } else {
        e = &g_cache[g_cache_rr];
        g_cache_rr = (g_cache_rr + 1) % CACHE_N;
        cache_clear(e);
    }
    e->src = strdup(src ? src : "");
    e->as_stmt = as_stmt;
    e->chunk = chunk;
    e->proc_addr = proc_addr;
    return e;
}

/* Indirizzo di inizio della procedura che contiene ip (massimo <= ip), o -1. */
static int current_proc_addr(const gmb_chunk_t* chunk, int ip)
{
    if (!chunk || ip < 0) return -1;
    int best = -1;
    for (uint32_t i = 0; i < chunk->ploc_count; i++) {
        int a = (int)chunk->ploc_addrs[i];
        if (a <= ip && (best < 0 || a > best)) best = a;
    }
    return best;
}

static int entry_truthy(gmb_stack_entry_t e)
{
    if (e.type == 1) return e.num != 0.0;
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) {
        const char* d = ((gmb_obj_string_t*)e.ref)->data;
        return d && d[0] != 0;
    }
    return e.type == 2 && e.ref != NULL;
}

static void release_entry(gmb_stack_entry_t e)
{
    if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
}

static int push_copy(gmb_task_t* t, gmb_stack_entry_t e)
{
    if (!gmb_task_stack_reserve(t, 1)) return 0;
    if (e.type == 2) gmb_task_push_ref(t, e.ref, e.basic_type);
    else if (e.type == 1) gmb_task_push_num(t, e.num, e.basic_type);
    else gmb_task_push_nil(t);
    return 1;
}

static int read_u16(const uint8_t* code, int len, int* ip)
{
    if (*ip + 1 >= len) { *ip = len; return 0; }
    int hi = code[(*ip)++];
    int lo = code[(*ip)++];
    return (hi << 8) | lo;
}

static double as_num(gmb_stack_entry_t e)
{
    if (e.type == 1) return e.num;
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) {
        const char* d = ((gmb_obj_string_t*)e.ref)->data;
        return d ? strtod(d, NULL) : 0.0;
    }
    return 0.0;
}

static int is_str(gmb_stack_entry_t e)
{
    return e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING;
}

static int run_snippet(gmb_task_t* task, const uint8_t* code, int code_len,
                       const gmb_val_t* consts, int nconst)
{
    int ip = 0;
    while (ip < code_len) {
        uint8_t op = code[ip++];
        switch (op) {
            case OP_PUSH_CONST: {
                int idx = read_u16(code, code_len, &ip);
                if (idx < 0 || idx >= nconst) { gmb_task_push_nil(task); break; }
                gmb_stack_entry_t e = gmb_adapter_val_to_stack(consts[idx]);
                if (gmb_task_push_move(task, e) != 0) { release_entry(e); return -1; }
                break;
            }
            case OP_PUSH_NIL: gmb_task_push_nil(task); break;
            case OP_PUSH_TRUE: gmb_task_push_num(task, -1.0, GMB_BT_BOOL); break;
            case OP_PUSH_FALSE: gmb_task_push_num(task, 0.0, GMB_BT_BOOL); break;
            case OP_POP:
                if (task->sp <= 0) return -1;
                release_entry(gmb_task_pop(task));
                break;
            case OP_DUP:
                if (task->sp <= 0) return -1;
                if (!push_copy(task, task->stack[task->sp - 1])) return -1;
                break;
            case OP_ADD: {
                if (task->sp < 2) return -1;
                gmb_stack_entry_t b = gmb_task_pop(task);
                gmb_stack_entry_t a = gmb_task_pop(task);
                if (is_str(a) || is_str(b)) {
                    gmb_val_t va = gmb_adapter_stack_to_val(a);
                    gmb_val_t vb = gmb_adapter_stack_to_val(b);
                    gmb_string_t sa = (va.kind == GMB_TYPE_STRING)
                        ? va.v_string : gmb_double_to_string(gmb_val_to_double(va));
                    gmb_string_t sb = (vb.kind == GMB_TYPE_STRING)
                        ? vb.v_string : gmb_double_to_string(gmb_val_to_double(vb));
                    int own_a = va.kind != GMB_TYPE_STRING;
                    int own_b = vb.kind != GMB_TYPE_STRING;
                    gmb_string_t cat = gmb_string_concat(sa, sb);
                    gmb_val_t vr = gmb_make_val_string(cat.data ? cat.data : "");
                    gmb_string_free(&cat);
                    if (own_a) gmb_string_free(&sa);
                    if (own_b) gmb_string_free(&sb);
                    if (va.kind == GMB_TYPE_STRING) gmb_string_free(&va.v_string);
                    if (vb.kind == GMB_TYPE_STRING) gmb_string_free(&vb.v_string);
                    gmb_stack_entry_t out = gmb_adapter_val_to_stack(vr);
                    gmb_string_free(&vr.v_string);
                    release_entry(a);
                    release_entry(b);
                    if (gmb_task_push_move(task, out) != 0) { release_entry(out); return -1; }
                } else {
                    double r = as_num(a) + as_num(b);
                    release_entry(a);
                    release_entry(b);
                    gmb_task_push_num(task, r, GMB_BT_NUMBER);
                }
                break;
            }
            case OP_SUBTRACT:
            case OP_MULTIPLY:
            case OP_DIVIDE:
            case OP_MOD:
            case OP_POWER: {
                if (task->sp < 2) return -1;
                gmb_stack_entry_t b = gmb_task_pop(task);
                gmb_stack_entry_t a = gmb_task_pop(task);
                double da = as_num(a), db = as_num(b), r = 0.0;
                /* Divisione/MOD per zero → ERR 11 catturabile (stessa semantica di
                 * gmb_vm_core.c, IL, PCode e C99Native): prima il valutatore restituiva 0.0
                 * in silenzio anche con ON ERROR attivo (divergenza
                 * Bugs/(solved) - Divisione-Per-Zero-Divergente-4-Gambe). */
                if (db == 0.0 && (op == OP_DIVIDE || op == OP_MOD)) {
                    release_entry(a);
                    release_entry(b);
                    gmb_raise_basic_error(task, 11, "Division by zero");
                    break;
                }
                if (op == OP_SUBTRACT) r = da - db;
                else if (op == OP_MULTIPLY) r = da * db;
                else if (op == OP_DIVIDE) r = da / db;
                else if (op == OP_MOD) r = fmod(da, db);
                else r = pow(da, db);
                release_entry(a);
                release_entry(b);
                gmb_task_push_num(task, r, GMB_BT_NUMBER);
                break;
            }
            case OP_NEGATE: {
                if (task->sp <= 0) return -1;
                gmb_stack_entry_t a = gmb_task_pop(task);
                double r = -as_num(a);
                release_entry(a);
                gmb_task_push_num(task, r, GMB_BT_NUMBER);
                break;
            }
            case OP_NOT: {
                if (task->sp <= 0) return -1;
                gmb_stack_entry_t a = gmb_task_pop(task);
                int t = entry_truthy(a);
                release_entry(a);
                gmb_task_push_num(task, t ? 0.0 : -1.0, GMB_BT_BOOL);
                break;
            }
            case OP_CMP_EQ: case OP_CMP_NE: case OP_CMP_LT:
            case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE: {
                if (task->sp < 2) return -1;
                gmb_stack_entry_t b = gmb_task_pop(task);
                gmb_stack_entry_t a = gmb_task_pop(task);
                int cmp;
                if (is_str(a) || is_str(b)) {
                    gmb_val_t va = gmb_adapter_stack_to_val(a);
                    gmb_val_t vb = gmb_adapter_stack_to_val(b);
                    char bufa[64], bufb[64];
                    const char* sa = (va.kind == GMB_TYPE_STRING && va.v_string.data)
                        ? va.v_string.data : "";
                    const char* sb = (vb.kind == GMB_TYPE_STRING && vb.v_string.data)
                        ? vb.v_string.data : "";
                    if (va.kind != GMB_TYPE_STRING) {
                        snprintf(bufa, sizeof bufa, "%.14g", gmb_val_to_double(va));
                        sa = bufa;
                    }
                    if (vb.kind != GMB_TYPE_STRING) {
                        snprintf(bufb, sizeof bufb, "%.14g", gmb_val_to_double(vb));
                        sb = bufb;
                    }
                    cmp = strcmp(sa, sb);
                    if (va.kind == GMB_TYPE_STRING) gmb_string_free(&va.v_string);
                    if (vb.kind == GMB_TYPE_STRING) gmb_string_free(&vb.v_string);
                } else {
                    double da = as_num(a), db = as_num(b);
                    cmp = (da < db) ? -1 : (da > db) ? 1 : 0;
                }
                int ok = 0;
                if (op == OP_CMP_EQ) ok = cmp == 0;
                else if (op == OP_CMP_NE) ok = cmp != 0;
                else if (op == OP_CMP_LT) ok = cmp < 0;
                else if (op == OP_CMP_GT) ok = cmp > 0;
                else if (op == OP_CMP_LTE) ok = cmp <= 0;
                else ok = cmp >= 0;
                release_entry(a);
                release_entry(b);
                gmb_task_push_num(task, ok ? -1.0 : 0.0, GMB_BT_BOOL);
                break;
            }
            case OP_LOAD_GLOBAL: {
                int slot = read_u16(code, code_len, &ip);
                gmb_stack_entry_t e = gmb_task_load_global(task, slot);
                if (!push_copy(task, e)) return -1;
                break;
            }
            case OP_LOAD_LOCAL: {
                int slot = read_u16(code, code_len, &ip);
                gmb_stack_entry_t e = gmb_task_load_local(task, slot);
                if (!push_copy(task, e)) return -1;
                break;
            }
            case OP_STORE_GLOBAL: {
                int slot = read_u16(code, code_len, &ip);
                if (task->sp <= 0) return -1;
                gmb_stack_entry_t e = gmb_task_pop(task);
                gmb_task_store_global(task, slot, e);
                release_entry(e);
                break;
            }
            case OP_STORE_LOCAL: {
                int slot = read_u16(code, code_len, &ip);
                if (task->sp <= 0) return -1;
                gmb_stack_entry_t e = gmb_task_pop(task);
                gmb_task_store_local(task, slot, e);
                release_entry(e);
                break;
            }
            case OP_JUMP_FORWARD:
                ip += read_u16(code, code_len, &ip);
                break;
            case OP_JUMP_IF_TRUE:
            case OP_JUMP_IF_FALSE: {
                int off = read_u16(code, code_len, &ip);
                if (task->sp <= 0) return -1;
                gmb_stack_entry_t c = gmb_task_pop(task);
                int t = entry_truthy(c);
                release_entry(c);
                if ((op == OP_JUMP_IF_TRUE) ? t : !t) ip += off;
                break;
            }
            case OP_HALT:
                return 0;
            default:
                return -1;
        }
    }
    return 0;
}

static void drain_to(gmb_task_t* task, int saved_sp)
{
    while (task->sp > saved_sp) release_entry(gmb_task_pop(task));
}

int gmb_vm_expr_try_compiled(gmb_task_t* task, const gmb_chunk_t* chunk,
                             const char* src, int as_stmt, gmb_val_t* result)
{
    if (!task) return -1;
    if (!src) src = "";
    while (*src == ' ' || *src == '\t' || *src == '\n' || *src == '\r') src++;
    if (!*src) {
        if (result) *result = gmb_val_nil();
        else if (!as_stmt) gmb_task_push_nil(task);
        return 0;
    }

    pthread_mutex_lock(&g_cache_mu);
    /* S_eval: la proc corrente fa parte della chiave (slot locali per-proc). */
    int pip = task ? task->ip : -1;
    int paddr = current_proc_addr(chunk, pip);
    cache_ent* ent = cache_find(src, as_stmt, chunk, paddr);
    if (!ent) {
        gmb_expr_prog prog;
        int prc = gmb_expr_parse(src, as_stmt, 1, &prog);
        emit_t em;
        memset(&em, 0, sizeof(em));
        int ok = (prc == GMB_EXPR_OK && prog.root && can_compile(prog.root, chunk, pip)
                  && compile_node(&em, prog.root, chunk, pip));
        if (ok) emit_byte(&em, OP_HALT);
        ent = cache_insert(src, as_stmt, chunk, paddr);
        if (ok && em.code) {
            ent->compiled = 1;
            ent->code = em.code;
            ent->code_len = em.len;
            ent->consts = em.consts;
            ent->nconst = em.nconst;
        } else {
            ent->compiled = 0;
            free(em.code);
            if (em.consts) {
                for (int i = 0; i < em.nconst; i++) free_val_str(&em.consts[i]);
                free(em.consts);
            }
        }
        gmb_expr_prog_free(&prog);
    }
    if (!ent->compiled) {
        pthread_mutex_unlock(&g_cache_mu);
        return 1;
    }

    int saved_sp = task->sp;
    int rc = run_snippet(task, ent->code, ent->code_len, ent->consts, ent->nconst);
    pthread_mutex_unlock(&g_cache_mu);
    if (rc != 0) {
        drain_to(task, saved_sp);
        return -1;
    }

    if (as_stmt) {
        if (result) {
            if (task->sp > saved_sp) {
                gmb_stack_entry_t e = gmb_task_pop(task);
                *result = gmb_adapter_stack_to_val(e);
                release_entry(e);
            } else {
                *result = gmb_val_nil();
            }
        }
        drain_to(task, saved_sp);
        return 0;
    }
    if (result) {
        if (task->sp > saved_sp) {
            gmb_stack_entry_t e = gmb_task_pop(task);
            *result = gmb_adapter_stack_to_val(e);
            release_entry(e);
        } else {
            *result = gmb_val_nil();
        }
        drain_to(task, saved_sp);
        return 0;
    }
    if (task->sp > saved_sp + 1) {
        gmb_stack_entry_t keep = gmb_task_pop(task);
        drain_to(task, saved_sp);
        if (gmb_task_push_move(task, keep) != 0) release_entry(keep);
    }
    return 0;
}
