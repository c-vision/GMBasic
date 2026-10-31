/* gmb_syscall_vectordb.c — handler SysCall 248-263 (RANDOMIZE/JSON/VECTORDB) estratti
 * dallo switch in gmb_vm_exec_syscall (task 6.5.2). Tabella gmb_vec_impl; dispatcher. */
#include "gmb_vm_core.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int gmb_vec_248(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    double sd; gmb_stack_entry_t aa;
    if (task->sp > 0) { aa = gmb_task_pop(task); sd = aa.type == 1 ? aa.num : (double)time(NULL); if (aa.ref) gmb_obj_release(aa.ref); }
    else sd = (double)time(NULL);
    gmb_rnd_seed((uint64_t)sd); push_num_entry(task, 0.0, GMB_BT_NUMBER);
    return 0;
}

static int gmb_vec_249(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char f[1024], v[1024]; gmb_stack_entry_t aa;
    vm_pop_str(task, v, sizeof(v)); vm_pop_str(task, f, sizeof(f));
    FILE* jf = fopen(f, "w");
    if (jf) { fputs(v, jf); fclose(jf); push_num_entry(task, 1.0, GMB_BT_NUMBER); }
    else push_num_entry(task, 0.0, GMB_BT_NUMBER);
    return 0;
}

static int gmb_vec_255(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code;
    char db[512], coll[64], fmt[16]; double dims; gmb_stack_entry_t a3;
    fmt[0] = 0;
    if (argc >= 4) vm_pop_str(task, fmt, sizeof(fmt));
    a3 = gmb_task_pop(task); dims = a3.type == 1 ? a3.num : 0;
    if (a3.ref) gmb_obj_release(a3.ref);
    vm_pop_str(task, coll, sizeof(coll)); vm_pop_str(task, db, sizeof(db));
    /* Parità builtin 501 e IL (id collezione, -1 su errore): prima spingeva un booleano. */
    push_num_entry(task, (double)gmb_rag_ensure_collection_fmt(db, coll, (int)dims, "cosine", NULL, fmt[0] ? fmt : NULL), GMB_BT_NUMBER);
    return 0;
}

static int gmb_vec_256(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    char db[512], coll[64], src[128], txt[65536], meta[8192];
    /* INDEXMETA a 5 argomenti porta i metadata (prima scartati + 1 residuo sullo stack).
     * Rotta sulle facade aiv (codici IndexTextOutcome 0/1/2/-1 come IL + trigger insert),
     * non più inline con codici booleani divergenti. */
    if (argc >= 5) vm_pop_str(task, meta, sizeof(meta)); else meta[0] = 0;
    vm_pop_str(task, txt, sizeof(txt)); vm_pop_str(task, src, sizeof(src));
    vm_pop_str(task, coll, sizeof(coll)); vm_pop_str(task, db, sizeof(db));
    push_num_entry(task, (double)gmb_aiv_index_meta(db, coll, src, txt, meta[0] ? meta : NULL), GMB_BT_NUMBER);
    return 0;
}

static int gmb_vec_258(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code;
    /* Forma-funzione a 4-9 argomenti (prima: 6 pop fissi → con arity diversa gli argomenti
     * si disallineavano e hybrid/filter venivano ignorati). Mappa posizionale con default
     * identici al builtin 507 e rotta unica search_sel (anche errore provider come IL). */
    char db[512], coll[64], q[1024], filter[4096];
    double k = 5; int hybrid = 1; int rerank = 1; double mev = 0.0; int explain = 0;
    int mev_set = 0;
    gmb_stack_entry_t a[9];
    int i, n = argc < 0 ? 0 : (argc > 9 ? 9 : argc);
    for (i = 0; i < 9; i++) { a[i].type = 0; a[i].num = 0; a[i].ref = NULL; }
    db[0] = 0; coll[0] = 0; q[0] = 0; filter[0] = 0;
    vm_pop_args_ordered(task, a, n);
    if (n >= 1 && a[0].type == 2 && a[0].ref) { gmb_obj_string_t* ms = (gmb_obj_string_t*)a[0].ref; size_t m = ms->length < 511 ? ms->length : 511; memcpy(db, ms->data, m); db[m] = 0; }
    if (n >= 2 && a[1].type == 2 && a[1].ref) { gmb_obj_string_t* ms = (gmb_obj_string_t*)a[1].ref; size_t m = ms->length < 63 ? ms->length : 63; memcpy(coll, ms->data, m); coll[m] = 0; }
    if (n >= 3 && a[2].type == 2 && a[2].ref) { gmb_obj_string_t* ms = (gmb_obj_string_t*)a[2].ref; size_t m = ms->length < 1023 ? ms->length : 1023; memcpy(q, ms->data, m); q[m] = 0; }
    if (n >= 4 && a[3].type == 1) k = a[3].num;
    if (n >= 5 && a[4].type == 1) hybrid = (int)a[4].num;
    if (n >= 6 && a[5].type == 2 && a[5].ref) { gmb_obj_string_t* ms = (gmb_obj_string_t*)a[5].ref; size_t m = ms->length < 4095 ? ms->length : 4095; memcpy(filter, ms->data, m); filter[m] = 0; }
    if (n >= 7 && a[6].type == 1) rerank = (int)a[6].num;
    if (n >= 8 && a[7].type == 1) { mev = a[7].num; mev_set = 1; }
    if (n >= 9 && a[8].type == 1) explain = (int)a[8].num;
    /* Reranker non numerico (nome/oggetto, come accetta il C#): non supportato qui = default
     * keyword (stesso limite del builtin 507). */
    {
        /* NAN senza warning a compile-time: strtod runtime (math.h NAN richiederebbe -lm link check). */
        double mev_eff = mev_set ? mev : strtod("NAN", NULL);
        char* r = gmb_rag_search_sel(db, coll, q, (int)k, hybrid, filter[0] ? filter : NULL, rerank, mev_eff, explain);
        gmb_obj_string_t* sr = gmb_str_cstr(r ? r : ""); free(r);
        push_ref_new(task, (gmb_obj_t*)sr, GMB_BT_STRING);
    }
    return 0;
}

static int gmb_vec_259(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char db[512], coll[64];
    vm_pop_str(task, coll, sizeof(coll)); vm_pop_str(task, db, sizeof(db));
    push_num_entry(task, (double)gmb_rag_count(db, coll), GMB_BT_NUMBER);
    return 0;
}

static int gmb_vec_260(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char db[512];
    vm_pop_str(task, db, sizeof(db));
    char* r = gmb_rag_list_collections(db);
    gmb_obj_string_t* sr = gmb_str_cstr(r ? r : ""); free(r);
    push_ref_new(task, (gmb_obj_t*)sr, GMB_BT_STRING);
    return 0;
}

static int gmb_vec_261(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char db[512], coll[64], src[128];
    vm_pop_str(task, src, sizeof(src)); vm_pop_str(task, coll, sizeof(coll)); vm_pop_str(task, db, sizeof(db));
    push_num_entry(task, (double)gmb_rag_delete_source(db, coll, src), GMB_BT_NUMBER);
    return 0;
}

static int gmb_vec_262(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code;
    char db[512], coll[64], path[1024], extra[1024];
    /* La forma-funzione porta pattern/loader opzionali (4°/5° arg): il backend C li ignora
     * (gap noto), ma vanno POPPATI per non sporcare lo stack del task. */
    if (argc >= 5) vm_pop_str(task, extra, sizeof(extra));
    if (argc >= 4) vm_pop_str(task, extra, sizeof(extra));
    vm_pop_str(task, path, sizeof(path)); vm_pop_str(task, coll, sizeof(coll)); vm_pop_str(task, db, sizeof(db));
    push_num_entry(task, (double)gmb_aiv_load_documents(db, coll, path, ""), GMB_BT_NUMBER);
    return 0;
}

static int gmb_vec_263(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char db[512], coll[64];
    vm_pop_str(task, coll, sizeof(coll)); vm_pop_str(task, db, sizeof(db));
    int n = gmb_aiv_purge_deprecated(db, coll);
    push_num_entry(task, (double)n, GMB_BT_NUMBER);
    return 0;
}

static int gmb_vec_default(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)argc;
    char rmsg[64]; snprintf(rmsg, sizeof(rmsg), "syscall %d non implementato", code);
    vm_error(task, rmsg);
    return 0;
}

typedef struct { int code; int (*handler)(gmb_vm_t*, gmb_task_t*, int, int); } gmb_vec_impl_t;
static const gmb_vec_impl_t gmb_vec_impl[] = {
    { 248, gmb_vec_248 },
    { 249, gmb_vec_249 },
    { 250, gmb_vec_249 },
    { 255, gmb_vec_255 },
    { 256, gmb_vec_256 },
    { 257, gmb_vec_256 },
    { 258, gmb_vec_258 },
    { 259, gmb_vec_259 },
    { 260, gmb_vec_260 },
    { 261, gmb_vec_261 },
    { 262, gmb_vec_262 },
    { 263, gmb_vec_263 },
};

int gmb_syscall_vectordb(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    for (size_t i = 0; i < sizeof(gmb_vec_impl) / sizeof(gmb_vec_impl[0]); i++)
        if (gmb_vec_impl[i].code == code)
            return gmb_vec_impl[i].handler(vm, task, code, argc);
    return gmb_vec_default(vm, task, code, argc);
}
