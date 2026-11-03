/* ============================================================================
 * gmb_vm_eval.c — EVAL / & a runtime: COMPILATORE C vero + VM/C.
 *
 * Il frammento BASIC (stringa costruita a runtime) viene compilato dal
 * compilatore C (gmb_compile_snippet) contro le tabelle di simboli del chunk
 * OSPITE: globali e procedure si risolvono sui suoi slot/indirizzi, i locali
 * della procedura corrente sono parametri BYREF (write-through sul frame del
 * chiamante). Il codice prodotto viene UNITO al chunk ospite (stesso spazio
 * indirizzi, stesso constant pool) ed eseguito con una normale chiamata
 * sincrona della VM: nessun interprete parallelo, nessun sottoinsieme.
 *
 * Prima esistevano gmb_expr (parser/valutatore separato) e gmb_vm_expr_compile
 * (snippet compiler per un piccolo set di nodi): entrambi RIMOSSI da questo path.
 * ============================================================================ */
#include "gmb_vm_eval.h"

#include "gmb_adapter.h"
#include "gmb_chunk.h"
#include "gmb_compiler.h"
#include "gmb_gpm.h"
#include "gmb_vm_core.h"
#include "gmb_vm_value.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Il COMPILATORE C è collegato solo dove serve (host VM/C e BSP): qui il simbolo è
 * debole, così la VM resta linkabile anche senza compilatore e un EVAL a runtime
 * produce un errore ESPLICITO ("compilatore non collegato") invece di un errore
 * di link. */
#if defined(__GNUC__) || defined(__clang__)
extern int gmb_compile_snippet(const char*, int, const gmb_snippet_ext_t*,
                               gmb_compiled_snippet_t*, char**) __attribute__((weak));
#endif

#define EVAL_CACHE_MAX 32

typedef struct {
    const gmb_chunk_t* host;      /* retained: il merged punta alle tabelle ospiti */
    int proc_addr;                /* contesto dei locali (procedura corrente) */
    char* src;
    int as_stmt;
    gmb_chunk_t merged;
    uint32_t entry;
    char** param_names;
    int* param_slots;             /* >=0 locale ospite, -1 campo/nome ignoto */
    int n_params;
    /* allocazioni di proprietà (il merged punta ANCHE a strutture dell'host) */
    uint8_t* own_code;
    uint32_t* own_lines;
    gmb_value_t* own_consts;
    char** own_strings; int n_own_strings;
    char** own_proc_names; uint32_t own_proc_n; uint32_t host_proc_n;
    uint32_t* own_proc_addrs; uint32_t* own_arity_addrs; uint32_t* own_arity_vals;
    char*** own_ploc_names; uint32_t own_ploc_n;
    uint32_t* own_try_ips; uint32_t* own_try_fins; uint32_t* own_try_ends;
    uint8_t* own_try_catch;
    uint64_t last_use;
} eval_ent_t;

static eval_ent_t g_cache[EVAL_CACHE_MAX];
static int g_cache_n = 0;
static uint64_t g_clock = 0;
static pthread_mutex_t g_mu;
static pthread_once_t g_mu_once = PTHREAD_ONCE_INIT;
static void eval_mu_init(void)
{
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_mu, &a);
    pthread_mutexattr_destroy(&a);
}
static char g_err[512];

static void set_err(const char* m) { snprintf(g_err, sizeof g_err, "%s", m ? m : "errore"); }
const char* gmb_vm_eval_last_error(void) { return g_err; }

/* Cattura dell'output dei PRINT del frammento (macro-comando): riversato poi
 * nello staging della task ospite per preservare l'ordine con i PRINT. */
typedef struct { char* p; size_t n, cap; } eval_cap_t;

static void eval_cap_add(eval_cap_t* c, const char* s, size_t len)
{
    if (!c || !s || !len) return;
    if (c->n + len + 1 > c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 256;
        while (nc < c->n + len + 1) nc *= 2;
        char* q = (char*)realloc(c->p, nc);
        if (!q) return;
        c->p = q; c->cap = nc;
    }
    memcpy(c->p + c->n, s, len);
    c->n += len;
    c->p[c->n] = 0;
}
static void eval_cap_str(void* ctx, const char* s, uint32_t len) { eval_cap_add((eval_cap_t*)ctx, s, len); }
static void eval_cap_num(void* ctx, double v)
{
    char b[64];
    snprintf(b, sizeof b, "%.14g", v);
    eval_cap_add((eval_cap_t*)ctx, b, strlen(b));
}
static void eval_cap_nl(void* ctx) { eval_cap_add((eval_cap_t*)ctx, "\n", 1); }

static uint32_t chunk_proc_addr_at(const gmb_chunk_t* ch, int ip)
{
    uint32_t best = 0; int have = 0;
    if (!ch || !ch->ploc_addrs || ch->ploc_count == 0) return 0;
    for (uint32_t i = 0; i < ch->ploc_count; i++) {
        int a = (int)ch->ploc_addrs[i];
        if (a <= ip && (!have || a > (int)best)) { best = (uint32_t)a; have = 1; }
    }
    return best;
}

static void merged_free(eval_ent_t* e)
{
    for (int i = 0; i < e->n_own_strings; i++) free(e->own_strings[i]);
    free(e->own_strings);
    if (e->own_proc_names) {
        for (uint32_t i = e->host_proc_n; i < e->host_proc_n + e->own_proc_n; i++)
            free(e->own_proc_names[i]);
    }
    free(e->own_proc_names); free(e->own_proc_addrs);
    free(e->own_arity_addrs); free(e->own_arity_vals);
    if (e->own_ploc_names) {
        for (uint32_t i = 0; i < e->own_ploc_n; i++) free(e->own_ploc_names[i]);
        free(e->own_ploc_names);
    }
    free(e->own_code); free(e->own_lines); free(e->own_consts);
    free(e->own_try_ips); free(e->own_try_fins); free(e->own_try_ends); free(e->own_try_catch);
    if (e->host) gmb_chunk_free((gmb_chunk_t*)e->host);
    for (int i = 0; i < e->n_params; i++) free(e->param_names[i]);
    free(e->param_names); free(e->param_slots);
    free(e->src);
    memset(e, 0, sizeof *e);
}

static char* own_str(eval_ent_t* e, const char* s)
{
    char* d = strdup(s ? s : "");
    if (d) {
        e->own_strings = (char**)realloc(e->own_strings, (size_t)(e->n_own_strings + 1) * sizeof(char*));
        e->own_strings[e->n_own_strings++] = d;
    }
    return d;
}

/* Unione chunk ospite + frammento: codice concatenato, constant pool allineato
 * (prefisso ospite + nuove), tabelle proc/ploc/try concatenate. */
static int merged_build(eval_ent_t* e, const gmb_chunk_t* host,
                        const gmb_compiled_snippet_t* sn,
                        uint32_t host_proc_count, uint32_t consts_seed)
{
    const gmb_compiled_chunk_t* s = &sn->chunk;
    uint32_t host_code = host ? host->code_len : 0;
    uint32_t host_consts = host ? host->consts_len : 0;
    uint32_t hp = host ? host->proc_count : 0;
    uint32_t new_procs = (s->proc_count > host_proc_count) ? s->proc_count - host_proc_count : 0;
    uint32_t new_consts = (s->consts_len > consts_seed) ? s->consts_len - consts_seed : 0;

    memset(&e->merged, 0, sizeof e->merged);
    e->host = host ? gmb_chunk_retain((gmb_chunk_t*)host) : NULL;

    uint32_t total = host_code + s->code_len;
    e->own_code = (uint8_t*)calloc(total ? total : 1, 1);
    if (!e->own_code) return -1;
    if (host_code) memcpy(e->own_code, host->code, host_code);
    if (s->code_len) memcpy(e->own_code + host_code, s->code, s->code_len);
    e->merged.code = e->own_code;
    e->merged.code_len = total;
    if (host && host->lines) {
        e->own_lines = (uint32_t*)calloc(total ? total : 1, sizeof(uint32_t));
        if (e->own_lines) memcpy(e->own_lines, host->lines, (size_t)host_code * sizeof(uint32_t));
        e->merged.lines = e->own_lines;
    }

    uint32_t nc = host_consts + new_consts;
    e->own_consts = (gmb_value_t*)calloc(nc ? nc : 1, sizeof(gmb_value_t));
    if (!e->own_consts) return -1;
    if (host_consts) memcpy(e->own_consts, host->constants, (size_t)host_consts * sizeof(gmb_value_t));
    for (uint32_t i = 0; i < new_consts; i++) {
        const gmb_cconst_t* sc = &s->consts[consts_seed + i];
        gmb_value_t* dc = &e->own_consts[host_consts + i];
        switch (sc->type) {
            case GMB_CCONST_DOUBLE: dc->type = GMB_CONST_DOUBLE; dc->d = sc->d; break;
            case GMB_CCONST_INT:    dc->type = GMB_CONST_INT;    dc->d = sc->d; break;
            case GMB_CCONST_NIL:    dc->type = GMB_CONST_NIL;    break;
            case GMB_CCONST_STRING:
                dc->type = GMB_CONST_STRING;
                dc->s = own_str(e, sc->s ? sc->s : "");
                if (!dc->s) return -1;
                break;
            default:
                set_err("frammento BASIC: costante composta non supportata a runtime");
                return -1;
        }
    }
    e->merged.constants = e->own_consts;
    e->merged.consts_len = nc;

    e->host_proc_n = hp;
    e->own_proc_n = new_procs;
    e->merged.proc_count = hp + new_procs;
    e->own_proc_names = (char**)calloc(e->merged.proc_count ? e->merged.proc_count : 1, sizeof(char*));
    e->own_proc_addrs = (uint32_t*)calloc(e->merged.proc_count ? e->merged.proc_count : 1, sizeof(uint32_t));
    e->own_arity_addrs = (uint32_t*)calloc(e->merged.proc_count ? e->merged.proc_count : 1, sizeof(uint32_t));
    e->own_arity_vals = (uint32_t*)calloc(e->merged.proc_count ? e->merged.proc_count : 1, sizeof(uint32_t));
    for (uint32_t i = 0; i < hp; i++) {
        e->own_proc_names[i] = (char*)(host->proc_names ? host->proc_names[i] : NULL);
        e->own_proc_addrs[i] = host->proc_addrs ? host->proc_addrs[i] : 0;
        e->own_arity_addrs[i] = host->arity_addrs ? host->arity_addrs[i] : 0;
        e->own_arity_vals[i] = host->arity_vals ? host->arity_vals[i] : 0;
    }
    for (uint32_t i = 0; i < new_procs; i++) {
        uint32_t si = host_proc_count + i;
        e->own_proc_names[hp + i] = own_str(e, s->proc_names ? s->proc_names[si] : "");
        e->own_proc_addrs[hp + i] = s->proc_addrs ? s->proc_addrs[si] : 0;
        e->own_arity_vals[hp + i] = (si < s->arity_count) ? s->arity_vals[si] : 0;
        e->own_arity_addrs[hp + i] = e->own_proc_addrs[hp + i];
    }
    e->merged.proc_names = e->own_proc_names;
    e->merged.proc_addrs = e->own_proc_addrs;
    e->merged.arity_addrs = e->own_arity_addrs;
    e->merged.arity_vals = e->own_arity_vals;
    e->merged.arity_count = e->merged.proc_count;

    uint32_t hloc = host ? host->ploc_count : 0;
    uint32_t sloc = s->ploc_count;
    e->own_ploc_n = sloc;
    e->merged.ploc_count = hloc + sloc;
    e->merged.ploc_addrs = (uint32_t*)calloc(e->merged.ploc_count ? e->merged.ploc_count : 1, sizeof(uint32_t));
    e->merged.ploc_counts = (uint32_t*)calloc(e->merged.ploc_count ? e->merged.ploc_count : 1, sizeof(uint32_t));
    e->merged.ploc_names = (char***)calloc(e->merged.ploc_count ? e->merged.ploc_count : 1, sizeof(char**));
    e->own_ploc_names = (char***)calloc(sloc ? sloc : 1, sizeof(char**));
    for (uint32_t i = 0; i < hloc; i++) {
        e->merged.ploc_addrs[i] = host->ploc_addrs ? host->ploc_addrs[i] : 0;
        e->merged.ploc_counts[i] = host->ploc_counts ? host->ploc_counts[i] : 0;
        e->merged.ploc_names[i] = host->ploc_names ? host->ploc_names[i] : NULL;
    }
    for (uint32_t i = 0; i < sloc; i++) {
        uint32_t cnt = s->ploc_counts ? s->ploc_counts[i] : 0;
        char** names = (char**)calloc(cnt ? cnt : 1, sizeof(char*));
        e->own_ploc_names[i] = names;
        e->merged.ploc_addrs[hloc + i] = s->ploc_addrs ? s->ploc_addrs[i] : 0;
        e->merged.ploc_counts[hloc + i] = cnt;
        e->merged.ploc_names[hloc + i] = names;
        for (uint32_t j = 0; j < cnt; j++) {
            const char* nm = (s->ploc_names && s->ploc_names[i]) ? s->ploc_names[i][j] : NULL;
            names[j] = own_str(e, nm);
        }
    }

    uint32_t ht = host ? host->try_region_count : 0;
    uint32_t st = s->try_region_count;
    if (ht + st > 0) {
        uint32_t n = ht + st;
        e->own_try_ips = (uint32_t*)calloc(n, sizeof(uint32_t));
        e->own_try_fins = (uint32_t*)calloc(n, sizeof(uint32_t));
        e->own_try_ends = (uint32_t*)calloc(n, sizeof(uint32_t));
        e->own_try_catch = (uint8_t*)calloc(n, 1);
        for (uint32_t i = 0; i < ht; i++) {
            e->own_try_ips[i] = host->try_ips[i];
            e->own_try_fins[i] = host->try_finally_ips[i];
            e->own_try_ends[i] = host->try_end_ips[i];
            e->own_try_catch[i] = host->try_has_catch[i];
        }
        for (uint32_t i = 0; i < st; i++) {
            e->own_try_ips[ht + i] = s->try_ips[i];
            e->own_try_fins[ht + i] = s->try_finally_ips[i];
            e->own_try_ends[ht + i] = s->try_end_ips[i];
            e->own_try_catch[ht + i] = s->try_has_catch[i];
        }
        e->merged.try_ips = e->own_try_ips;
        e->merged.try_finally_ips = e->own_try_fins;
        e->merged.try_end_ips = e->own_try_ends;
        e->merged.try_has_catch = e->own_try_catch;
        e->merged.try_region_count = n;
    }

    if (host) {
        e->merged.global_names = host->global_names;
        e->merged.global_name_count = host->global_name_count;
        e->merged.global_count = host->global_count;
        e->merged.plnum_addrs = host->plnum_addrs;
        e->merged.plnum_flags = host->plnum_flags;
        e->merged.plnum_counts = host->plnum_counts;
        e->merged.plnum_count = host->plnum_count;
        e->merged.stmt_starts = host->stmt_starts;
        e->merged.stmt_ends = host->stmt_ends;
        e->merged.stmt_range_count = host->stmt_range_count;
        e->merged.uses_tray = host->uses_tray;
        e->merged.entry_point = host->entry_point;
    }
    e->merged.local_count = (host && host->local_count > s->local_count)
        ? host->local_count : s->local_count;
    e->entry = sn->entry;
    e->merged.refs = 1;

    /* firma del wrapper (parametri BYREF) */
    e->n_params = sn->n_params;
    if (sn->n_params > 0) {
        e->param_names = (char**)calloc((size_t)sn->n_params, sizeof(char*));
        e->param_slots = (int*)calloc((size_t)sn->n_params, sizeof(int));
        for (int i = 0; i < sn->n_params; i++) {
            e->param_names[i] = strdup(sn->param_names[i] ? sn->param_names[i] : "");
            e->param_slots[i] = sn->param_slots[i];
        }
    }
    return 0;
}

static eval_ent_t* cache_find(const gmb_chunk_t* host, int paddr, const char* src, int as_stmt)
{
    for (int i = 0; i < g_cache_n; i++) {
        eval_ent_t* e = &g_cache[i];
        if (e->host == host && e->proc_addr == paddr && e->as_stmt == as_stmt
            && e->src && strcmp(e->src, src) == 0) {
            e->last_use = ++g_clock;
            return e;
        }
    }
    return NULL;
}

static eval_ent_t* cache_build(const gmb_chunk_t* host, int paddr, const char* src, int as_stmt)
{
    gmb_snippet_ext_t ext;
    memset(&ext, 0, sizeof ext);
    ext.global_names = (const char* const*)host->global_names;
    ext.global_count = (int)host->global_name_count;
    ext.proc_names = (const char* const*)host->proc_names;
    ext.proc_addrs = host->proc_addrs;
    ext.proc_arities = host->arity_vals;
    ext.proc_count = (int)host->proc_count;
    ext.consts_seed = host->consts_len;
    ext.code_base = (int)host->code_len;

    /* locali della procedura corrente: nomi per slot dallo snapshot ploc */
    const char* local_names[GMB_LOCALS_CAP];
    memset(local_names, 0, sizeof local_names);
    int nlocals = 0;
    if (host->ploc_addrs) {
        int best = -1;
        for (uint32_t i = 0; i < host->ploc_count; i++) {
            int a = (int)host->ploc_addrs[i];
            if (a <= paddr && (best < 0 || a > (int)host->ploc_addrs[best])) best = (int)i;
        }
        if (best >= 0 && host->ploc_names && host->ploc_names[best]) {
            uint32_t cnt = host->ploc_counts[best];
            if (cnt > GMB_LOCALS_CAP) cnt = GMB_LOCALS_CAP;
            for (uint32_t j = 0; j < cnt; j++) local_names[j] = host->ploc_names[best][j];
            nlocals = (int)cnt;
        }
    }
    ext.local_names = local_names;
    ext.local_count = nlocals;

    if (!gmb_compile_snippet) {
        set_err("EVAL/frammenti: compilatore C non collegato in questo binario");
        return NULL;
    }

    gmb_compiled_snippet_t sn;
    char* cerr = NULL;
    if (gmb_compile_snippet(src, as_stmt, &ext, &sn, &cerr) != 0) {
        set_err(cerr ? cerr : "compilazione fallita");
        free(cerr);
        return NULL;
    }
    free(cerr);

    eval_ent_t* slot;
    if (g_cache_n < EVAL_CACHE_MAX) slot = &g_cache[g_cache_n++];
    else {
        int lru = 0;
        for (int i = 1; i < g_cache_n; i++) if (g_cache[i].last_use < g_cache[lru].last_use) lru = i;
        slot = &g_cache[lru];
        merged_free(slot);
    }
    memset(slot, 0, sizeof *slot);
    slot->src = strdup(src);
    slot->proc_addr = paddr;
    slot->as_stmt = as_stmt;
    if (merged_build(slot, host, &sn, (uint32_t)host->proc_count, host->consts_len) != 0) {
        merged_free(slot);
        gmb_compiled_snippet_free(&sn);
        if (!g_err[0]) set_err("unione del frammento con il chunk fallita");
        return NULL;
    }
    gmb_compiled_snippet_free(&sn);
    slot->last_use = ++g_clock;
    return slot;
}

/* ── driver ─────────────────────────────────────────────────────────────────── */
int gmb_vm_eval_text_env(gmb_task_t* task, const char* src, int as_stmt,
                         gmb_eval_env_get_fn env_get, gmb_eval_env_set_fn env_set,
                         void* env_ctx, gmb_val_t* out)
{
    g_err[0] = 0;
    if (out) *out = gmb_val_nil();
    if (!task || !task->vm_ctx) { set_err("EVAL: nessuna VM"); return -1; }
    gmb_vm_t* vm = (gmb_vm_t*)task->vm_ctx;
    const gmb_chunk_t* host = vm->chunk;
    if (!host) { set_err("EVAL: nessun chunk"); return -1; }
    if (!src) src = "";

    int paddr = (int)chunk_proc_addr_at(host, task->ip);

    pthread_once(&g_mu_once, eval_mu_init);
    pthread_mutex_lock(&g_mu);
    eval_ent_t* e = cache_find(host, paddr, src, as_stmt);
    if (!e) e = cache_build(host, paddr, src, as_stmt);
    if (!e) { pthread_mutex_unlock(&g_mu); return -1; }

    /* Argomenti: locali del chiamante come BYREF write-through; nomi ignoti
     * (campi DBF / variabili nuove) come celle BYREF con il valore del record. */
    int argc = e->n_params;
    gmb_obj_byref_t** cells = (gmb_obj_byref_t**)calloc((size_t)(argc ? argc : 1), sizeof(void*));
    for (int i = 0; i < argc; i++) {
        int slot = e->param_slots[i];
        if (slot >= 0) {
            gmb_stack_entry_t b = gmb_task_capture_local_byref(task, slot);
            if (b.type == 2 && b.ref) { gmb_task_push_ref(task, b.ref, b.basic_type); gmb_obj_release(b.ref); }
            else gmb_task_push_nil(task);
        } else {
            gmb_val_t fv = gmb_val_nil();
            int own_fv = 0;
            if (env_get) {
                /* contratto env: valore BORROWED (non va liberato) */
                if (env_get(env_ctx, e->param_names[i], &fv) != 0) fv = gmb_val_nil();
            } else {
                gmb_vm_dbf_field_get(e->param_names[i], &fv);   /* assente = nil */
                own_fv = 1;
            }
            gmb_stack_entry_t se = gmb_adapter_val_to_stack(fv);
            if (own_fv && fv.kind == GMB_TYPE_STRING) gmb_string_free(&fv.v_string);
            gmb_obj_byref_t* box = gmb_byref_new_cell(task, se);
            if (se.type == 2 && se.ref) gmb_obj_release(se.ref);
            if (!box) { gmb_task_push_nil(task); continue; }
            cells[i] = box;
            gmb_task_push_ref(task, (gmb_obj_t*)box, GMB_BT_OBJECT);
        }
    }

    eval_cap_t cap;
    memset(&cap, 0, sizeof cap);
    void (*s_str)(void*, const char*, uint32_t) = vm->print_str;
    void (*s_num)(void*, double) = vm->print_num;
    void (*s_nl)(void*) = vm->print_newline;
    void* s_ctx = vm->print_ctx;
    vm->print_str = eval_cap_str;
    vm->print_num = eval_cap_num;
    vm->print_newline = eval_cap_nl;
    vm->print_ctx = &cap;

    const gmb_chunk_t* saved_chunk = vm->chunk;
    vm->chunk = &e->merged;
    gmb_val_t result = gmb_val_nil();
    int want_result = (as_stmt != 1);
    int rc = gmb_vm_call_sync(task, (int)e->entry, argc, want_result ? &result : NULL);
    vm->chunk = saved_chunk;
    vm->print_str = s_str; vm->print_num = s_num;
    vm->print_newline = s_nl; vm->print_ctx = s_ctx;
    if (cap.p) { gmb_vm_emit_captured(vm, task, cap.p); free(cap.p); }

    /* Scritture sui campi: riporta il valore della cella sul record attivo. */
    for (int i = 0; i < argc; i++) {
        gmb_obj_byref_t* box = cells ? cells[i] : NULL;
        if (!box) continue;
        gmb_stack_entry_t cur = gmb_byref_load(box);   /* vista BORROWED: non si rilascia */
        gmb_val_t v = gmb_adapter_stack_to_val(cur);
        if (env_set) env_set(env_ctx, e->param_names[i], v);
        else gmb_vm_dbf_field_set(e->param_names[i], v);
        if (v.kind == GMB_TYPE_STRING) gmb_string_free(&v.v_string);
        gmb_obj_release((gmb_obj_t*)box);
    }
    free(cells);
    pthread_mutex_unlock(&g_mu);

    if (rc != 0) { set_err("esecuzione del frammento BASIC fallita"); return -1; }
    if (out && want_result) *out = result;
    else if (result.kind == GMB_TYPE_STRING) gmb_string_free(&result.v_string);
    return 0;
}

int gmb_vm_eval_text(gmb_task_t* task, const char* src, int as_stmt, gmb_val_t* out)
{
    return gmb_vm_eval_text_env(task, src, as_stmt, NULL, NULL, NULL, out);
}

/* Hook per i contesti senza VM (REQUIRE): usa la task corrente del thread.
 * -2 = nessuna task (il chiamante sceglie, nessun fallback silenzioso). */
static int eval_env_hook(const char* src, int as_stmt,
                         gmb_eval_env_get_fn get, gmb_eval_env_set_fn set,
                         void* env, gmb_val_t* out)
{
    gmb_task_t* t = gmb_vm_tls_task();
    if (!t) return -2;
    return gmb_vm_eval_text_env(t, src, as_stmt, get, set, env, out);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void gmb_eval_env_register(void)
{
    gmb_eval_register_env(eval_env_hook);
}
#endif

void gmb_vm_eval_cache_clear(void)
{
    pthread_once(&g_mu_once, eval_mu_init);
    pthread_mutex_lock(&g_mu);
    for (int i = 0; i < g_cache_n; i++) merged_free(&g_cache[i]);
    g_cache_n = 0;
    pthread_mutex_unlock(&g_mu);
}
