/* gmb_c99_emit_internal.h — header INTERNO dell'emitter C99 nativo (task 6.1.1). */
#ifndef GMB_C99_EMIT_INTERNAL_H
#define GMB_C99_EMIT_INTERNAL_H

#include "gmb_c99_emit.h"
#include "gmb_ast.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char* s; size_t len, cap; } sb_t;

static void sb_putn(sb_t* b, const char* s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 4096;
        while (nc < b->len + n + 1) nc *= 2;
        b->s = (char*)realloc(b->s, nc);
        b->cap = nc;
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;
}
static void sb_put(sb_t* b, const char* s) { sb_putn(b, s, strlen(s)); }
static void sb_printf(sb_t* b, const char* fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) sb_putn(b, tmp, (size_t)n);
}

/* ────────────────────────────── tipi ───────────────────────────────── */

enum { TY_NUM = 0, TY_STR = 1 };

/* FASE 2.6(b): variabili oggetto di sincronizzazione GPM (DIM AS CHANNEL/...) */
enum { GK_PLAIN = 0, GK_CHAN, GK_WG, GK_MUTEX, GK_RWMUTEX, GK_ONCE, GK_COND,
       GK_ATOMIC, GK_POOL, GK_SYNCMAP, GK_CONTEXT, GK_HTTPREQ, GK_HTTPRESP, GK_WEBSOCKET,
       GK_FUNCVAL /* DIM v AS FUNCTION(...)/SUB(...): valore funzione di prima classe (gmb_fn_t) */,
       GK_GEN,    /* DIM it = <Generatore>(args): oggetto generatore */
       GK_LIST,   /* collections: DIM v AS LIST<...> */
       GK_DICT,   /* collections: DIM v AS DICT<...> */
       GK_QUEUE,  /* QUEUE (FIFO) */
       GK_STACK,  /* STACK (LIFO) */
       GK_SET,    /* HASHSET (unici) */
       GK_SORTED, /* SORTED (dizionario ordinato) */
       GK_GRAPH,  /* GRAPH → handle int64_t gmb_graph_* */
       GK_MATRIX  /* MATRIX handle → int64_t gmb_matrix_* */ };

/* `var_t`/`vars_t`: definizione UNICA condivisa con gmb_c99_emit.c (questo header ne aveva
 * una copia ridotta a 43 campi → accessi fuori offset e `vars_t.v[i]` con stride errato). */
#include "gmb_c99_emit_vart.h"

typedef struct fn_info {
    gmb_stmt_t* stmt;          /* nodo GMB_STMT_FUNCTION (non posseduto) */
    vars_t locals;             /* parametri + variabili locali */
    int n_params;
    gmb_stmt_t** defers; int n_defers, defer_cap;   /* DEFER registrati (LIFO all'uscita) */
    char* go_wrap;               /* wrapper GO SUB senza waitgroup (generato una volta) */
    char* go_wrap_wg;            /* wrapper GO SUB con IN wg (generato una volta) */
    char* go_wrap_fn;            /* wrapper GO FUNCTION ... INTO (future, generato una volta) */
    char* http_wrap;             /* 2.6(p): wrapper handler HTTP route→SUB (generato una volta) */
    char* ws_msg_wrap;           /* 2.6(q): wrapper dispatch messaggio WS→SUB OnMessage/OnError (una volta) */
    int err_raise;               /* ERROR nel body SOLLEVA sempre (gmb_raise_error): il body è
                                  * eseguito dentro un wrapper GO FUNCTION con setjmp che lo
                                  * cattura → future in errore (frame propagation) */
    struct fn_info* parent;      /* FASE 2.6(j): funzione nidificata → funzione contenitore */
    int parent_i;                /* indice del contenitore (stabile tra realloc): per le λ
                                  * sintetiche il parent non è cercabile nel body — usato da
                                  * repair_parents. -1 = non impostato. */
    int n_cap;                   /* FASE 2.6(j): variabili catturate (locals con captured=1) */
    int is_lambda_cap;           /* 2026-09-03: λ con catture by-value (ctx __lctx_<nome>
                                  * allocato+inizializzato al reference) — i campi catturati
                                  * sono in locals con captured=1 */
    int is_lambda;               /* 2026-09-03: funzione sintetica da una λ → firma con
                                  * `void* __ctx` PRIMO parametro (il valore è sempre chiamato
                                  * via gmb_fn_t con ctx, anche se non cattura nulla) */
    int lambda_ret_arr_len;      /* λ che ritorna {..}: numero di residui/equazioni (m) */
    int has_gosub;               /* 2.6(s): body con siti GOSUB → RETURN con dispatch stack */
    char cls_owner[40];          /* 2.7: metodo di classe → firma con gmb_val_t* self */
    int ovl_idx;                 /* 2026-09-24 (overload): indice omonimo (0 = nome C nudo
                                  * gmb_user_<nome>, >0 = suffisso __N). Assegnato da
                                  * assign_cnames per le top-level senza cls_owner. */
} fn_info_t;

#include "gmb_c99_emit_ctx.h"

/* raccolta nomi variabili (raccolta passi di registrazione) */
typedef struct { char** names; int n, cap; } namelist_t;

/* Prototipi delle funzioni static esposte (split per dominio) */
const char* dom_ident(gmb_expr_t* e);
void dom_schema_defs(ctx_t* c, const char* typeName, char* out, int cap);
void emit_speed_domain(ctx_t* c, gmb_stmt_t* s);
void sb_putn(sb_t* b, const char* s, size_t n);
void sb_put(sb_t* b, const char* s);
void sb_printf(sb_t* b, const char* fmt, ...);
void c99_fail(ctx_t* c, int line, const char* fmt, ...);
var_t* vars_find(vars_t* v, const char* name);
var_t* vars_add(vars_t* v, const char* name, int type);
int suffix_type(const char* name);
vars_t* cur_scope(ctx_t* c);
var_t* lookup_var(ctx_t* c, const char* name);
int gmb_c99_emit_coll_json(ctx_t* c, const char* name);
int gmb_c99_var_coll_kind(ctx_t* c, const char* name);
int gmb_c99_var_coll_sorted(ctx_t* c, const char* name);
void sb_ident(sb_t* b, const char* name);
char* sb_ident_to(char* dst, const char* name);
void emit_ident(ctx_t* c, const char* name);
const char* ctype_name(int ty);
void sb_bound_ident(sb_t* b, const char* arr, int k);
void emit_var_ref(ctx_t* c, const var_t* v);
void sb_bound_var(sb_t* b, const var_t* v, int k);
const char* gkind_ctype(int gk);
void sb_var_ctype(sb_t* b, var_t* v);
int gmb_udt_var_type(ctx_t* c, const char* name, char* out, int cap); /* nome-tipo UDT di una var (mappa popolata al DIM) */
/* Testo del tipo BASIC dichiarato da una DIM (canone SIZEOF). Definita nel TU proprietario
 * del ctx (gmb_c99_emit.c): la mappa vive in un campo noto solo a quel TU. */
int gmb_var_declared_type(ctx_t* c, const char* name, char* out, int cap);
int sync_gkind(const char* tn);
void defer_add(ctx_t* c, gmb_stmt_t* body);
int defer_count(ctx_t* c);
const char* defer_exit_label(ctx_t* c);
int body_has_done(gmb_stmt_t** list, int n);
int body_has_gosub(gmb_stmt_t** list, int n);
int cur_scope_has_gosub(ctx_t* c);
void emit_gosub_return(ctx_t* c, const char* alt);
int label_in_body(gmb_stmt_t** list, int n, const char* name);
void collect_num_targets(gmb_stmt_t** list, int n, int* out, int* no);
void collect_data_items(ctx_t* c, gmb_stmt_t** list, int n);
int gpm_kind_p(int k);
int onerr_stmt_p(gmb_stmt_t* s);
void gpm_scan(ctx_t* c, gmb_stmt_t* s);
int stmt_redim_name(gmb_stmt_t* s, const char* name);
void emit_defer_epilogue(ctx_t* c);
void emit_arr_wb(ctx_t* c);
void emit_fn_return(ctx_t* c);
void gofn_err_scan(ctx_t* c, gmb_stmt_t* s);
int catch_member_kind(ctx_t* c, const char* var);
int future_index(ctx_t* c, const char* var);
int dim_arr_init_kind(const gmb_expr_t* e);
int dim_val_init_kind(const gmb_expr_t* e);
struct cls_info* cls_find(ctx_t* c, const char* name);
const char* cls_field_ctype(ctx_t* c, const char* tn);
const char* cls_field_type_of(struct cls_info* ci, const char* f);
int cls_field_arr_size(const char* tn);      /* #slot totali di un campo-array ("T[N]"→N+1) */
long cls_field_arr_dim(const char* tn, int d); /* limite SUPERIORE della d-esima dim */
void sb_cls_field(sb_t* b, struct cls_info* ci, const char* base, const char* field);
int cls_field_exists(struct cls_info* ci, const char* f);
int cls_field_is_str(struct cls_info* ci, const char* f);
struct cls_info* cls_static_find(ctx_t* c, const char* clsname, const char* f, int* idx);
struct cls_info* cls_field_owner(ctx_t* c, const char* clsname, const char* f);
const char* prop_accessor_name(const char* prop, const char* kind);
fn_info_t* cls_find_method(ctx_t* c, const char* clsname, const char* meth);
fn_info_t* cls_find_method_chain(ctx_t* c, const char* clsname, const char* meth);
void cls_emit_flat_fields(ctx_t* c, sb_t* b, struct cls_info* ci, int depth);
int cls_flat_field_count(ctx_t* c, struct cls_info* ci, int depth);
int cls_overrides(ctx_t* c, const char* clsname, const char* meth, fn_info_t** out, int cap);
int cls_iface_impls(ctx_t* c, const char* ifname, const char* meth, fn_info_t** out, int cap);
const char* expr_class(ctx_t* c, gmb_expr_t* e);
void emit_virt_call(ctx_t* c, fn_info_t** defs, int ndefs, void (*emit_inst)(ctx_t*, void*), void* ud, gmb_expr_t** margs, int nmargs);
int etype(ctx_t* c, gmb_expr_t* e);
void register_list(ctx_t* c, gmb_stmt_t** list, int n, vars_t* scope);
int use_global(ctx_t* c, vars_t* scope, const char* name);
void register_stmt(ctx_t* c, gmb_stmt_t* s, vars_t* scope);
int propagate_stmt(ctx_t* c, gmb_stmt_t* s, vars_t* scope);
void emit_str_literal(ctx_t* c, const char* s);
fn_info_t* find_fn(ctx_t* c, const char* name);
int fn_ret_is_str(const fn_info_t* fn);
int fn_ret_is_func(const fn_info_t* fn);
const char* fn_ret_ctype(ctx_t* c, const fn_info_t* fn);
int bi_is(const char* name, const char* base);
void nl_add(namelist_t* nl, const char* name);
void nl_expr_cb(const char* name, void* ud);
void expr_var_names(gmb_expr_t* e, void (*cb)(const char*, void*), void* ud);
void stmt_var_names(gmb_stmt_t* s, namelist_t* nl);
int fn_capture_children(ctx_t* c, fn_info_t* p, fn_info_t** kids, int maxk);
int byref_param_captured_by_closure(ctx_t* c, fn_info_t* fn, int idx);
void hoist_nested_fns(ctx_t* c, int parent_idx);
void emit_str_operand(ctx_t* c, gmb_expr_t* a);
int emit_bf_tray(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_builtin(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
void emit_flat_index(ctx_t* c, var_t* v, gmb_expr_t** idx, int nidx);
void vi_emit(ctx_t* c, void* ud);
void emit_arr_len_of(ctx_t* c, var_t* av);
void emit_arr_len_ref(ctx_t* c, const var_t* v);
int is_byval_arr_param(ctx_t* c, const char* name);
void emit_dim_arr_init(ctx_t* c, gmb_stmt_t* s);
void emit_arr_call_arg(ctx_t* c, fn_info_t* fn, int i, gmb_expr_t* arg, int line);
void emit_call(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int op_is(const char* op, const char* want);
int boxed_member_is_str(const char* m);
void emit_boxed_member_read(ctx_t* c, var_t* bv, const char* member);
void emit_expr(ctx_t* c, gmb_expr_t* e);
/* Operando numerico (unbox di un gmb_val_t/membro boxed). Non-static: condivisa con
 * gmb_c99_emit_builtin.c (es. ROUND su un campo di oggetto opaco). */
void emit_num_operand(ctx_t* c, gmb_expr_t* e);
void emit_sprite_domain(ctx_t* c, gmb_stmt_t* s);
void ind(ctx_t* c);
/* Marca l'uso di un widget TUI (emette #include "gmb_tui_widgets.h" + gmb_tui_app.h).
 * Definito in gmb_c99_emit.c (unico proprietario del layout di ctx_t). */
void c99_mark_tui(ctx_t* c);
void c99_mark_ui(ctx_t* c);
void emit_num_label(ctx_t* c, int bas);
void emit_list(ctx_t* c, gmb_stmt_t** list, int n);
void emit_ret_value(ctx_t* c, gmb_expr_t* value);
void emit_arr_literal_assign(ctx_t* c, const char* target, gmb_expr_t* lit, int line);
void emit_assignment(ctx_t* c, const char* target, gmb_expr_t* value, int line);
void emit_handle(ctx_t* c, gmb_expr_t* e, int line);
var_t* emit_net_target(ctx_t* c, const char* name);
void emit_net_lhs(ctx_t* c, var_t* v);
void emit_expr_dflt(ctx_t* c, gmb_expr_t* e, const char* dflt);
void emit_net_str(ctx_t* c, gmb_expr_t* e);
void emit_net_buf_assign(ctx_t* c, var_t* v, const char* buf, const char* lenvar);
const char* go_wrapper_for(ctx_t* c, fn_info_t* fn, int has_wg);
const char* gofn_wrapper_for(ctx_t* c, fn_info_t* fn);
const char* ws_msg_wrapper_for(ctx_t* c, fn_info_t* fn);
const char* http_route_wrapper_for(ctx_t* c, fn_info_t* fn);
void emit_stmt(ctx_t* c, gmb_stmt_t* s);
void emit_function(ctx_t* c, fn_info_t* fn);
void emit_host_env(ctx_t* c, sb_t* fin);
void add_fn_info(ctx_t* c, gmb_stmt_t* s, const char* cls_owner);
void emit_matrix_handle(ctx_t* c, const var_t* v);
void emit_arr_arg_len(ctx_t* c, gmb_expr_t* a);
int emit_bf_strings(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_strings_extra(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_io_files(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_system(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_json(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
/* Metodo di un contenitore TIPIZZATO a handle (HEAP/MAXHEAP/BAG/TRIE) emesso dal dispatch
 * canonico per TokenKind (gmb_c99_emit.c: try_emit_var_method) — il file dei builtin deve
 * usare QUELLO invece di riscrivere i casi lista per nome (regola d'oro + §0bis). */
int gmb_c99_coll_handle_method(ctx_t* c, const char* recv, const char* m, gmb_expr_t** args, int nargs, int line);
int gmb_c99_coll_handle_supports(ctx_t* c, const char* recv, const char* m, int nargs);
int js_stringify(ctx_t* c, gmb_expr_t** a, int n);
int js_extract_emit(ctx_t* c, gmb_expr_t** a, int n, int typed);   /* JSON.STRINGIFY/TOJSON: emissione riusata dalle statement */
int emit_bf_conversions(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_math(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_sync(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_graphics(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_kmeans(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_compression(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_grep(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_message(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_context(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_fetch(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_bounds(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_stats_arr(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_stats_dict(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_kdtree(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_finance(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_finance_defaults(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_matrix(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_rdd(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_sql(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_joy(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_dist(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_hash(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_ts(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_encoding(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_finance_scalar(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_finance_cashflow(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_security(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_memory(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_memory_names(ctx_t* c, const char* name, const char* ns, gmb_expr_t** args, int nargs, int line);
int emit_bf_convert(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_arrays(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_uncertainty(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int gmb_expr_is_fn_value(ctx_t* c, const gmb_expr_t* e);
/* base(idx).member su variabile boxed/JSON (condivisa con gmb_c99_emit_builtin.c per CSTR). */
int is_json_index_member_call(ctx_t* c, gmb_expr_t* e);

/* ── SCOPE DINAMICO (DYNAMIC) — implementate in gmb_c99_emit_dynamic.c ──────────────
 * Nomi dichiarati DYNAMIC (per NOME), tipo inferito, e wrapper di emissione verso il
 * runtime condiviso gmb_dscope (parità emitter C#). Vedi Guide/Variables-Scope-Spec §2.5. */
void gmb_c99_collect_dynamic(ctx_t* c, gmb_stmt_t** list, int n);
int  gmb_c99_dyn_is(ctx_t* c, const char* name);
int  gmb_c99_dyn_is_str(ctx_t* c, const char* name);
int  gmb_c99_list_declares_dynamic(gmb_stmt_t** list, int n);
void gmb_c99_emit_dyn_declare(ctx_t* c, gmb_stmt_t* s);
void gmb_c99_emit_dyn_val(ctx_t* c, gmb_expr_t* e);
int  gmb_c99_try_emit_dyn_read(ctx_t* c, const char* name);
int  gmb_c99_try_emit_dyn_write(ctx_t* c, const char* target, gmb_expr_t* value);

#endif
