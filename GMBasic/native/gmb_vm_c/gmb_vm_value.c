/* ============================================================================
 * gmb_vm_value.c — Implementazione valori/stack VM C
 * File: native/gmb_vm_c/gmb_vm_value.c
 * ============================================================================ */

#include "gmb_vm_value.h"
#include "gmb_gpm.h"
#include "gmb_collections.h"   /* gmb_coll_bag_to_json (JSON del BAG, §0bis: una sola sorgente) */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void string_release_fn(struct gmb_obj* o)
{
    gmb_obj_string_t* s = (gmb_obj_string_t*)o;
    free(s->data);
    free(s);
}

gmb_obj_string_t* gmb_str_alloc(const char* data, uint32_t len)
{
    gmb_obj_string_t* s = (gmb_obj_string_t*)calloc(1, sizeof(gmb_obj_string_t));
    if (!s) return NULL;
    s->header.refcount = 1;
    s->header.kind = GMB_OBJ_STRING;
    s->header.release_fn = string_release_fn;
    s->length = len;
    s->capacity = len; /* nessuna scorta iniziale: la maggior parte delle stringhe non
                         * viene mai appesa in place -- la scorta cresce solo al primo
                         * gmb_str_append_inplace, quando serve davvero. */
    s->data = (char*)malloc(len + 1);
    if (!s->data) { free(s); return NULL; }
    if (data && len > 0) memcpy(s->data, data, len);
    s->data[len] = '\0';
    return s;
}

/* Cresce s->data se necessario (raddoppio geometrico della capacità, come sds) e
 * appende `data`/`len` a partire da s->length. PRECONDIZIONE (a carico del chiamante,
 * vedi gmb_vm_value.h): s->header.refcount == 1 -- nessun altro alias osserva s->data,
 * quindi mutarlo in place non corrompe altre variabili che condividono l'oggetto. */
void gmb_str_append_inplace(gmb_obj_string_t* s, const char* data, uint32_t len)
{
    if (!s || len == 0) return;
    uint32_t needed = s->length + len;
    if (needed > s->capacity)
    {
        uint32_t new_cap = s->capacity > 0 ? s->capacity * 2 : 16;
        if (new_cap < needed) new_cap = needed;
        char* nd = (char*)realloc(s->data, (size_t)new_cap + 1);
        if (!nd) return; /* OOM: lascia s invariata, coerente con le altre alloc del file
                           * che falliscono silenziosamente senza propagare un errore. */
        s->data = nd;
        s->capacity = new_cap;
    }
    if (data) memcpy(s->data + s->length, data, len);
    s->length = needed;
    s->data[s->length] = '\0';
}

gmb_obj_string_t* gmb_str_cstr(const char* s)
{
    return s ? gmb_str_alloc(s, (uint32_t)strlen(s)) : gmb_str_alloc("", 0);
}

void gmb_str_retain(gmb_obj_string_t* s)
{
    if (s) s->header.refcount++;
}

void gmb_str_release(gmb_obj_string_t* s)
{
    if (!s) return;
    if (--s->header.refcount == 0)
    {
        if (s->header.release_fn) s->header.release_fn((struct gmb_obj*)s);
        else { free(s->data); free(s); }
    }
}

uint32_t gmb_str_len(const gmb_obj_string_t* s)
{
    return s ? s->length : 0;
}

/* Chiave STRINGA di un indice numerico (dict/sorted): una DATA (tag GMB_BT_DATE) usa
 * l'ISO-8601 "yyyy-MM-ddTHH:mm:ss", come IL/PCode/C99 (`d(DATEVALUE(...))` →
 * "2023-01-10T00:00:00"); un numero il canone %.15g. Unica sede (§0bis): prima ogni sito
 * faceva snprintf("%.15g") e la chiave-data usciva "44936". */
void gmb_vm_key_from_num(char* buf, size_t cap, double num, int is_date)
{
    if (!buf || cap == 0) return;
    if (is_date)
    {
        gmb_string_t iso = gmb_dt_datetime_iso(num);
        snprintf(buf, cap, "%s", iso.data ? iso.data : "");
        gmb_string_free(&iso);
    }
    else snprintf(buf, cap, "%.15g", num);
}

/* Retain/release generici per qualsiasi gmb_obj_t (canale/wg/future/stringa) */
void gmb_obj_retain(gmb_obj_t* o) { if (o) o->refcount++; }
void gmb_obj_release(gmb_obj_t* o)
{
    if (!o) return;
    if (--o->refcount == 0)
    {
        if (o->release_fn) o->release_fn(o);
        else free(o);
    }
}

/* ----------------------------------------------------------------------------
 * Closure (lambda che cattura upvalue)
 * ---------------------------------------------------------------------------- */
static void closure_release_fn(struct gmb_obj* o)
{
    gmb_obj_closure_t* c = (gmb_obj_closure_t*)o;
    if (c->upvalues)
    {
        for (int i = 0; i < c->num_upvalues; i++)
            if (c->upvalues[i].type == 2 && c->upvalues[i].ref) gmb_obj_release(c->upvalues[i].ref);
        free(c->upvalues);
    }
    free(c);
}

gmb_obj_closure_t* gmb_closure_new(int address, int num_upvalues)
{
    gmb_obj_closure_t* c = (gmb_obj_closure_t*)calloc(1, sizeof(gmb_obj_closure_t));
    if (!c) return NULL;
    c->header.refcount = 1;
    c->header.kind = GMB_OBJ_CLOSURE;
    c->header.release_fn = closure_release_fn;
    c->address = address;
    c->num_upvalues = num_upvalues;
    if (num_upvalues > 0)
    {
        c->upvalues = (gmb_stack_entry_t*)calloc(num_upvalues, sizeof(gmb_stack_entry_t));
        if (!c->upvalues) { free(c); return NULL; }
    }
    return c;
}

static void generator_release_fn(struct gmb_obj* o)
{
    gmb_obj_generator_t* g = (gmb_obj_generator_t*)o;
    if (g->task)
    {
        /* il task dedicato del generatore non è nel GPM scheduler (mai accodato):
         * gmb_task_free non tocca lo scheduler, libera solo frame/locals/globals/struct.
         * owns_globals=0: le globals sono del chiamante, condivise. */
        g->task->owns_globals = 0;
        gmb_task_free(g->task);
    }
    if (g->expr_list)
        gmb_obj_release((gmb_obj_t*)g->expr_list);
    free(g->err_desc);
    free(g->err_trace);
    free(g);
}

gmb_obj_generator_t* gmb_generator_new(struct gmb_task* task)
{
    gmb_obj_generator_t* g = (gmb_obj_generator_t*)calloc(1, sizeof(gmb_obj_generator_t));
    if (!g) return NULL;
    g->header.refcount = 1;
    g->header.kind = GMB_OBJ_GENERATOR;
    g->header.release_fn = generator_release_fn;
    g->task = task;
    g->done = 0;
    g->is_expr = 0;
    return g;
}

/* ----------------------------------------------------------------------------
 * Riferimento a lista/dict del runtime condiviso (gmb_val_t ARRAY/OBJECT).
 * La VM incapsula gmb_list_t/gmb_dict_t (modello transpiler) in un gmb_obj heap
 * così lo stack le gestisce con refcount. Unificazione Fase B.
 * ---------------------------------------------------------------------------- */

static void shared_val_release_fn(struct gmb_obj* o)
{
    gmb_obj_shared_val_t* s = (gmb_obj_shared_val_t*)o;
    if (s->mat)
    {
        gmb_matrix_value_free(s->mat);
        free(s->mat);
        s->mat = NULL;
    }
    if (s->list)
    {
        gmb_list_free(s->list);
        free(s->list);
    }
    if (s->dict)
    {
        gmb_dict_free(s->dict);
        free(s->dict);
    }
    free(s);
}

gmb_obj_shared_val_t* gmb_shared_list_new(void)
{
    gmb_obj_shared_val_t* s = (gmb_obj_shared_val_t*)calloc(1, sizeof(gmb_obj_shared_val_t));
    if (!s) return NULL;
    s->header.refcount = 1;
    s->header.kind = GMB_OBJ_LIST;
    s->header.release_fn = shared_val_release_fn;
    s->list = (gmb_list_t*)calloc(1, sizeof(gmb_list_t));
    if (!s->list) { free(s); return NULL; }
    *s->list = gmb_list_new();
    s->header.data = s->list;   /* F2: il runtime C (gmb_json_append_val) legge la lista via header.data */
    s->header.type_name = "gmb_list";
    /* CONVENZIONE UNICA (A'): un valore OBJECT/ARRAY porta SEMPRE l'header, mai il payload.
     * Prima questo box era kind=LIST con v_ptr=payload, e i due accessor canonici del
     * runtime la rifiutavano: gmb_val_as_list (gmb_list.c:366) pretende kind OBJECT e
     * v_ptr=header, quindi restituiva VUOTO — il sintomo "backends": []. E con payload in
     * v_ptr, gmb_obj_data leggeva fuori dal tipo (gmb_val_as_dict, gmb_dict.c:217).
     * type_name dichiarato perché i due accessor si escludono a vicenda su "dict". */
    s->val = gmb_make_val_obj(&s->header);
    return s;
}

gmb_obj_shared_val_t* gmb_shared_dict_new(void)
{
    gmb_obj_shared_val_t* s = (gmb_obj_shared_val_t*)calloc(1, sizeof(gmb_obj_shared_val_t));
    if (!s) return NULL;
    s->header.refcount = 1;
    s->header.kind = GMB_OBJ_DICT;
    s->header.release_fn = shared_val_release_fn;
    s->dict = (gmb_dict_t*)calloc(1, sizeof(gmb_dict_t));
    if (!s->dict) { free(s); return NULL; }
    *s->dict = gmb_dict_new();
    s->header.data = s->dict;   /* F2: il runtime C (gmb_json_append_val) legge il dict via header.data */
    s->header.type_name = "gmb_dict";
    /* Stessa convenzione A' della lista: prima dichiarava kind OBJECT ma ci metteva il
     * payload, cosi' gmb_obj_data/gmb_val_as_dict leggevano fuori dal tipo. */
    s->val = gmb_make_val_obj(&s->header);
    return s;
}

/* F2 (§0bis): builtin STRINGIFY (601) delega alla serializzazione del runtime
 * condiviso (gmb_val_to_json — il wrapper VM è riconosciuto via kind
 * GMB_OBJ_LIST/GMB_OBJ_DICT + header.data impostato in gmb_shared_*_new).
 * gmb_builtin_json.c (copia di ~120 righe della stessa logica) è stato eliminato. */
gmb_obj_string_t* gmb_vm_json_stringify(gmb_obj_t* obj)
{
    if (!obj) return NULL;
    /* Stringa: `gmb_make_val_obj` la incapsula come OBJECT → il serializer cadeva nel
     * fallback e rendeva "null" (bug VMC JSON.Stringify di un val JSON-stringa: atteso
     * "..." col contenuto, virgolette escapate, canone IL/PCode/C99Native). Boxala come
     * val STRINGA così gmb_val_to_json prende il branch stringa (quoting + escape). */
    if (obj->kind == GMB_OBJ_STRING) {
        gmb_obj_string_t* so = (gmb_obj_string_t*)obj;
        gmb_val_t sv = gmb_make_val_string(so->data ? so->data : "");
        gmb_string_t s = gmb_val_to_json(sv);
        gmb_obj_string_t* r = gmb_str_cstr(s.data ? s.data : "null");
        gmb_string_free(&s);
        return r;
    }
    /* Collezione (GMB_OBJ_COLLECTION): il serializer generico non conosce il tipo → "null"
     * (era il caso di `B.ToJson()` su un BAG, che il compilatore emette come JSON.STRINGIFY).
     * Il BAG è un MULTISET: il suo JSON è {"elemento":molteplicità,…} (parità P-Code, dove il
     * Bag è un Dictionary<object,long>), dalla funzione condivisa del runtime (§0bis).
     * Gli altri contenitori restano come prima (nessuna semantica inventata qui). */
    if (obj->kind == GMB_OBJ_COLLECTION) {
        gmb_obj_collection_t* col = (gmb_obj_collection_t*)obj;
        if (col->type_code == 15) {
            gmb_string_t bj = gmb_coll_bag_to_json(col->handle);
            gmb_obj_string_t* rb = gmb_str_cstr(bj.data ? bj.data : "{}");
            gmb_string_free(&bj);
            return rb;
        }
        /* TIMELINE: oggetto {ISO-8601: valore} (canone DATA come chiave) — prima "null". */
        if (col->type_code == 18) {
            gmb_string_t tj = gmb_coll_timeline_to_json(col->handle);
            gmb_obj_string_t* rt = gmb_str_cstr(tj.data ? tj.data : "{}");
            gmb_string_free(&tj);
            return rt;
        }
        /* MAPPE ordinate e CACHE: oggetto {"chiave":valore} nell'ordine delle chiavi
         * (Cache in ordine di nome) — prima "null" (solo il BAG era serializzato). */
        if (col->type_code == 6 || col->type_code == 7 || col->type_code == 8
            || col->type_code == 9 || col->type_code == 26 || col->type_code == 16) {
            gmb_string_t mj = gmb_coll_map_to_json(col->handle);
            gmb_obj_string_t* rm = gmb_str_cstr(mj.data ? mj.data : "{}");
            gmb_string_free(&mj);
            return rm;
        }
    }
    gmb_string_t s = gmb_val_to_json(gmb_make_val_obj(obj));
    gmb_obj_string_t* r = gmb_str_cstr(s.data ? s.data : "null");
    gmb_string_free(&s);
    return r;
}

/* ----------------------------------------------------------------------------
 * Stack operandi
 * ---------------------------------------------------------------------------- */

void gmb_stack_init(gmb_stack_t* s) { s->sp = 0; }

void gmb_stack_push_num(gmb_stack_t* s, double v, uint8_t basic_type)
{
    if (s->sp >= GMB_STACK_CAP) return;
    gmb_stack_entry_t* e = &s->slots[s->sp++];
    e->num = v; e->ref = NULL; e->type = 1; e->basic_type = basic_type;
}

void gmb_stack_push_ref(gmb_stack_t* s, gmb_obj_t* o, uint8_t basic_type)
{
    if (s->sp >= GMB_STACK_CAP) return;
    gmb_stack_entry_t* e = &s->slots[s->sp++];
    e->num = 0; e->ref = o; e->type = 2; e->basic_type = basic_type;
    if (o) gmb_obj_retain(o);
}

void gmb_stack_push_nil(gmb_stack_t* s)
{
    if (s->sp >= GMB_STACK_CAP) return;
    gmb_stack_entry_t* e = &s->slots[s->sp++];
    e->num = 0; e->ref = NULL; e->type = 2; e->basic_type = GMB_BT_NULL;
}

gmb_stack_entry_t gmb_stack_pop(gmb_stack_t* s)
{
    gmb_stack_entry_t zero = { 0 };
    if (s->sp <= 0) return zero;
    return s->slots[--s->sp];
}

/* Pop che rilascia anche l'oggetto referenziato (per scarti espliciti). */
gmb_stack_entry_t gmb_stack_pop_release(gmb_stack_t* s)
{
    gmb_stack_entry_t e = gmb_stack_pop(s);
    if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
    return e;
}

gmb_stack_entry_t gmb_stack_peek(const gmb_stack_t* s, int from_top)
{
    gmb_stack_entry_t zero = { 0 };
    int idx = s->sp - 1 - from_top;
    if (idx < 0 || idx >= s->sp) return zero;
    return s->slots[idx];
}

double gmb_stack_pop_num(gmb_stack_t* s)
{
    gmb_stack_entry_t e = gmb_stack_pop(s);
    if (e.type == 1) return e.num;
    return 0.0;
}

gmb_obj_t* gmb_stack_pop_ref(gmb_stack_t* s)
{
    gmb_stack_entry_t e = gmb_stack_pop(s);
    return (gmb_obj_t*)e.ref;
}

int gmb_stack_size(const gmb_stack_t* s) { return s->sp; }

void gmb_stack_drop(gmb_stack_t* s, int n)
{
    while (n-- > 0 && s->sp > 0) gmb_stack_pop_release(s);
}

/* ---- Graph (Collections) ---- */
static void graph_release_fn(struct gmb_obj* o)
{
    gmb_obj_graph_t* g = (gmb_obj_graph_t*)o;
    if (g->handle > 0)
    {
        extern void gmb_graph_free(int64_t h);
        gmb_graph_free(g->handle);
    }
    free(g);
}

gmb_obj_graph_t* gmb_graph_obj_new(int64_t handle)
{
    gmb_obj_graph_t* g = (gmb_obj_graph_t*)calloc(1, sizeof(gmb_obj_graph_t));
    if (!g) return NULL;
    g->header.refcount = 1;
    g->header.kind = GMB_OBJ_GRAPH;
    g->header.release_fn = graph_release_fn;
    g->handle = handle;
    return g;
}

/* ---- Collections generiche ---- */
static void collection_release_fn(struct gmb_obj* o)
{
    gmb_obj_collection_t* c = (gmb_obj_collection_t*)o;
    if (c->handle > 0)
    {
        extern void gmb_collections_free(int64_t h);
        gmb_collections_free(c->handle);
    }
    free(c);
}

gmb_obj_collection_t* gmb_collection_obj_new(int64_t handle, int type_code)
{
    gmb_obj_collection_t* c = (gmb_obj_collection_t*)calloc(1, sizeof(gmb_obj_collection_t));
    if (!c) return NULL;
    c->header.refcount = 1;
    c->header.kind = GMB_OBJ_COLLECTION;
    c->header.release_fn = collection_release_fn;
    c->handle = handle;
    c->type_code = type_code;
    return c;
}
