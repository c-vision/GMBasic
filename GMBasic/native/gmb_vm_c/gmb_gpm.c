/* ============================================================================
 * gmb_gpm.c — GPM multi-core (§16): scheduler pthread, canali, waitgroup, future
 * File: native/gmb_vm_c/gmb_gpm.c
 * ============================================================================ */

#include "gmb_gpm.h"
#include "gmb_network.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int64_t gmb_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ============================================================================
 * Task
 * ============================================================================ */

/* ---- Stack operandi dinamico (V1 piano VMC) ----
 * Crescita geometrica (raddoppio) da GMB_TASK_STACK_CAP fino al cap
 * configurabile. L'overflow NON è mai silenzioso: imposta stack_overflow e il
 * dispatcher abortisce il task con errore deterministico (come PCodeVm che
 * lancia StackOverflow invece di perdere valori). */

int gmb_task_stack_max(void)
{
    static int cached = 0;
    if (cached <= 0)
    {
        const char* e = getenv("GMBASIC_PCODE_MAX_STACK");
        long v = e ? atol(e) : 0;
        cached = (v > 0 && v <= 268435456L) ? (int)v : 1048576;
    }
    return cached;
}

int gmb_task_stack_reserve(gmb_task_t* t, int extra)
{
    if (t->stack_overflow) return 0;
    int max = gmb_task_stack_max();
    if (extra <= 0) return 1;
    if (extra > max || t->sp > max - extra) { t->stack_overflow = 1; return 0; }
    int need = t->sp + extra;
    if (need <= t->stack_cap) return 1;
    int ncap = t->stack_cap > 0 ? t->stack_cap : GMB_TASK_STACK_CAP;
    while (ncap > 0 && ncap < need) ncap *= 2;
    if (ncap <= 0 || ncap > max) ncap = max;
    if (ncap < need) { t->stack_overflow = 1; return 0; }
    gmb_stack_entry_t* ns = (gmb_stack_entry_t*)realloc(t->stack, (size_t)ncap * sizeof(gmb_stack_entry_t));
    if (!ns) { t->stack_overflow = 1; return 0; }
    t->stack = ns;
    t->stack_cap = ncap;
    return 1;
}

int gmb_task_push_move(gmb_task_t* t, gmb_stack_entry_t e)
{
    if (!gmb_task_stack_reserve(t, 1)) return -1;
    t->stack[t->sp++] = e;
    return 0;
}

void gmb_task_truncate(gmb_task_t* t, int sp)
{
    while (t->sp > sp)
    {
        gmb_stack_entry_t e = t->stack[--t->sp];
        if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
    }
    if (t->sp < 0) t->sp = 0;
}

void gmb_task_push_num(gmb_task_t* t, double v, uint8_t bt)
{
    if (!gmb_task_stack_reserve(t, 1)) return;
    gmb_stack_entry_t* e = &t->stack[t->sp++];
    e->num = v; e->ref = NULL; e->type = 1; e->basic_type = bt;
}

void gmb_task_push_ref(gmb_task_t* t, gmb_obj_t* o, uint8_t bt)
{
    if (!gmb_task_stack_reserve(t, 1)) return;
    gmb_stack_entry_t* e = &t->stack[t->sp++];
    e->num = 0; e->ref = o; e->type = 2; e->basic_type = bt;
    if (o) gmb_obj_retain(o);
}

void gmb_task_push_nil(gmb_task_t* t)
{
    if (!gmb_task_stack_reserve(t, 1)) return;
    gmb_stack_entry_t* e = &t->stack[t->sp++];
    e->num = 0; e->ref = NULL; e->type = 2; e->basic_type = GMB_BT_NULL;
}

gmb_stack_entry_t gmb_task_pop(gmb_task_t* t)
{
    gmb_stack_entry_t zero = { 0 };
    if (t->sp <= 0) return zero;
    return t->stack[--t->sp];
}

double gmb_task_pop_num(gmb_task_t* t)
{
    gmb_stack_entry_t e = gmb_task_pop(t);
    /* Se il valore poppato è un ref (stringa/dict/list/...) usato in un contesto
     * puramente numerico, viene scartato qui (contribuisce 0.0) -- va comunque
     * rilasciato: il pop trasferisce a questa funzione la proprietà del retain
     * fatto dal push originale, e qui il valore non viene né ripushato né
     * salvato altrove. Senza questo release, ogni ref scartato in un contesto
     * numerico perderebbe un refcount per sempre. */
    if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
    return e.type == 1 ? e.num : 0.0;
}

/* Il valore `e` qui è sempre il risultato di un gmb_task_pop appena fatto (mai
 * usato altrove nel frattempo, vedi i call site in gmb_vm_core.c) -- il pop
 * trasferisce a questa funzione la proprietà del retain fatto dal push
 * originale di e.ref. Quindi NON si ritiene di nuovo e.ref: lo slot prende
 * direttamente in carico quel retain già esistente ("move", non "copy").
 * Prima di questo fix si faceva un retain aggiuntivo qui sopra a quello mai
 * rilasciato dal pop, cosicché ogni assegnazione (LET/=), non solo le stringhe,
 * lasciava il vecchio valore permanentemente non liberabile (refcount mai a
 * zero) -- il leak "sistemico" descritto nel TODO. */

/* ---- Frame refcount + Box BYREF (vedi dichiarazioni in gmb_gpm.h) ---- */
void gmb_dyn_scope_clear(gmb_dyn_scope_t* s)
{
    if (!s) return;
    for (int i = 0; i < s->count; i++)
    {
        free(s->names[i]);
        if (s->vals[i].type == 2 && s->vals[i].ref) gmb_obj_release(s->vals[i].ref);
    }
    free(s->names);
    free(s->vals);
    s->names = NULL;
    s->vals = NULL;
    s->count = 0;
    s->cap = 0;
}

/* Crescita dei locals (2026-11-05): i frame partono dalla capacità dichiarata dalla
 * procedura e crescono se il codice usa più slot; prima il limite fisso a 256 faceva
 * perdere silenziosamente gli slot oltre soglia (output VM/C diverso dagli altri backend).
 * I nuovi slot sono azzerati. Ritorna 0 su OOM (il chiamante ignora la scrittura). */
static int grow_locals(double** nums, gmb_obj_t*** refs, uint8_t** types, uint8_t** basic,
                       int* cap, int need)
{
    if (need <= *cap) return 1;
    int n = *cap > 0 ? *cap : GMB_LOCALS_CAP;
    while (n < need) n *= 2;
    double* nn = (double*)realloc(*nums, (size_t)n * sizeof(double));
    if (!nn) return 0;
    gmb_obj_t** nr = (gmb_obj_t**)realloc(*refs, (size_t)n * sizeof(gmb_obj_t*));
    if (!nr) return 0;
    uint8_t* nt = (uint8_t*)realloc(*types, (size_t)n);
    if (!nt) return 0;
    uint8_t* nb = (uint8_t*)realloc(*basic, (size_t)n);
    if (!nb) return 0;
    int old = *cap;
    memset(nn + old, 0, (size_t)(n - old) * sizeof(double));
    memset(nr + old, 0, (size_t)(n - old) * sizeof(gmb_obj_t*));
    memset(nt + old, 0, (size_t)(n - old));
    memset(nb + old, 0, (size_t)(n - old));
    *nums = nn; *refs = nr; *types = nt; *basic = nb; *cap = n;
    return 1;
}

int gmb_frame_ensure_locals(gmb_frame_t* f, int n)
{
    if (!f) return 0;
    return grow_locals(&f->local_nums, &f->local_refs, &f->local_types, &f->local_basic,
                       &f->nlocals, n);
}

int gmb_task_ensure_locals(gmb_task_t* t, int n)
{
    if (!t) return 0;
    if (t->frame) return gmb_frame_ensure_locals(t->frame, n);
    return grow_locals(&t->locals_nums, &t->locals_refs, &t->locals_types, &t->locals_basic,
                       &t->locals_cap, n);
}

void gmb_frame_release(gmb_frame_t* f)
{
    if (!f) return;
    if (--f->refcount <= 0)
    {
        for (int i = 0; i < f->nlocals; i++)
            if (f->local_types[i] == 2 && f->local_refs[i]) gmb_obj_release(f->local_refs[i]);
        free(f->local_nums); free(f->local_refs); free(f->local_types); free(f->local_basic);
        if (f->upvalues_owned)
            for (int i = 0; i < 16; i++)
                if (f->upvalues[i].type == 2 && f->upvalues[i].ref) gmb_obj_release(f->upvalues[i].ref);
        if (f->constructor_instance) gmb_obj_release(f->constructor_instance);
        gmb_dyn_scope_clear(&f->dyn);
        gmb_for_clear(&f->for_state);
        free(f);
    }
}

/* ---- Box BYREF (specchio ByRefBox C#) ---- */
static void byref_release_fn(struct gmb_obj* o)
{
    gmb_obj_byref_t* b = (gmb_obj_byref_t*)o;
    if (b->is_cell && b->cell.type == 2 && b->cell.ref) gmb_obj_release(b->cell.ref);
    if (b->frame) gmb_frame_release(b->frame);
    if (b->is_array_elem && b->arr) gmb_obj_release((gmb_obj_t*)b->arr);
    free(b);
}

gmb_obj_byref_t* gmb_byref_new_local(gmb_task_t* owner, gmb_frame_t* frame, int slot)
{
    gmb_obj_byref_t* b = (gmb_obj_byref_t*)calloc(1, sizeof(gmb_obj_byref_t));
    if (!b) return NULL;
    b->header.refcount = 1;
    b->header.kind = GMB_OBJ_BYREF;
    b->header.release_fn = byref_release_fn;
    b->owner = owner;
    b->frame = frame;
    if (frame) frame->refcount++;
    b->slot = slot;
    b->is_global = 0;
    return b;
}

gmb_obj_byref_t* gmb_byref_new_cell(gmb_task_t* owner, gmb_stack_entry_t value)
{
    gmb_obj_byref_t* b = (gmb_obj_byref_t*)calloc(1, sizeof(gmb_obj_byref_t));
    if (!b) return NULL;
    b->header.refcount = 1;
    b->header.kind = GMB_OBJ_BYREF;
    b->header.release_fn = byref_release_fn;
    b->owner = owner;
    b->is_cell = 1;
    b->cell = value;
    if (value.type == 2 && value.ref) gmb_obj_retain(value.ref);
    return b;
}

gmb_obj_byref_t* gmb_byref_new_global(gmb_task_t* owner, int slot)
{
    gmb_obj_byref_t* b = (gmb_obj_byref_t*)calloc(1, sizeof(gmb_obj_byref_t));
    if (!b) return NULL;
    b->header.refcount = 1;
    b->header.kind = GMB_OBJ_BYREF;
    b->header.release_fn = byref_release_fn;
    b->owner = owner;
    b->frame = NULL;
    b->slot = slot;
    b->is_global = 1;
    return b;
}

/* `Arr(i)` come argomento BYREF (OP_MAKE_BYREF_ELEMENT, mono-dimensionale): il box trattiene
 * un ref sull'array (rilasciato in byref_release_fn) così l'array resta vivo per tutta la vita
 * del box anche se il chiamante ne perde ogni altro riferimento. `slot` è riusato come indice
 * nell'array — vedi byref_read/byref_write, ramo is_array_elem. */
gmb_obj_byref_t* gmb_byref_new_array_elem(gmb_task_t* owner, gmb_obj_shared_val_t* arr, int index)
{
    if (!arr || !arr->list || index < 0 || (size_t)index >= arr->list->count) return NULL;
    gmb_obj_byref_t* b = (gmb_obj_byref_t*)calloc(1, sizeof(gmb_obj_byref_t));
    if (!b) return NULL;
    b->header.refcount = 1;
    b->header.kind = GMB_OBJ_BYREF;
    b->header.release_fn = byref_release_fn;
    b->owner = owner;
    b->frame = NULL;
    b->slot = index;
    b->is_array_elem = 1;
    b->arr = arr;
    gmb_obj_retain((gmb_obj_t*)arr);
    return b;
}

/* Lettura risolta del target (vista borrowed, stesso contratto di load_local). */
static gmb_stack_entry_t byref_read(const gmb_obj_byref_t* b)
{
    gmb_stack_entry_t e = { 0 };
    if (!b) return e;
    if (b->is_cell) return b->cell;
    if (b->is_array_elem)
    {
        if (!b->arr || !b->arr->list || b->slot < 0 || (size_t)b->slot >= b->arr->list->count) return e;
        gmb_val_t v = gmb_list_get(*b->arr->list, (gmb_int_t)b->slot);
        if (v.kind == GMB_TYPE_OBJECT && v.v_ptr) { e.type = 2; e.ref = (gmb_obj_t*)v.v_ptr; e.basic_type = GMB_BT_OBJECT; }
        else if (v.kind == GMB_TYPE_DOUBLE) { e.type = 1; e.num = v.v_double; e.basic_type = GMB_BT_NUMBER; }
        else if (v.kind == GMB_TYPE_INT) { e.type = 1; e.num = (double)v.v_int; e.basic_type = GMB_BT_NUMBER; }
        else if (v.kind == GMB_TYPE_BOOL) { e.type = 1; e.num = v.v_bool ? -1.0 : 0.0; e.basic_type = GMB_BT_BOOL; }
        if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_BYREF)
            return byref_read((const gmb_obj_byref_t*)e.ref);
        return e;
    }
    if (!b->owner) return e;
    gmb_task_t* t = b->owner;
    if (b->is_global)
    {
        if (b->slot >= 0 && b->slot < (int)t->globals_count)
        {
            e.type = t->globals_types[b->slot];
            e.num = t->globals_nums[b->slot];
            e.ref = t->globals_refs[b->slot];
            e.basic_type = t->globals_basic[b->slot];
        }
        if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_BYREF)
            return byref_read((const gmb_obj_byref_t*)e.ref);
        return e;
    }
    double* nums; gmb_obj_t** refs; uint8_t* types; uint8_t* basic;
    if (b->frame) { nums = b->frame->local_nums; refs = b->frame->local_refs; types = b->frame->local_types; basic = b->frame->local_basic; }
    else { nums = t->locals_nums; refs = t->locals_refs; types = t->locals_types; basic = t->locals_basic; }
    int cap = b->frame ? b->frame->nlocals : t->locals_cap;
    if (b->slot < 0 || b->slot >= cap) return e;
    e.type = types[b->slot]; e.num = nums[b->slot]; e.ref = refs[b->slot]; e.basic_type = basic[b->slot];
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_BYREF)
        return byref_read((const gmb_obj_byref_t*)e.ref);
    return e;
}

/* Write-through sul target. `e` segue il contratto di ownership del chiamante:
 * steal (retain_existing=0, store_local) o retain (retain_existing=1, store_global). */
static void byref_write(gmb_obj_byref_t* b, gmb_stack_entry_t e, int retain_existing)
{
    if (!b) return;
    if (b->is_cell)
    {
        if (b->cell.type == 2 && b->cell.ref) gmb_obj_release(b->cell.ref);
        b->cell = e;
        if (retain_existing && e.type == 2 && e.ref) gmb_obj_retain(e.ref);
        return;
    }
    if (b->is_array_elem)
    {
        if (!b->arr || !b->arr->list || b->slot < 0 || (size_t)b->slot >= b->arr->list->count) return;
        /* gmb_list_set POSSIEDE gli elementi contati: trattiene il nuovo e rilascia il vecchio da se'
         * (prima qui c'erano release/retain manuali: ora sarebbero un doppio conteggio). Contratto di
         * `e`: retain_existing=1 → il chiamante conserva il proprio ref (la lista ne prende uno suo);
         * retain_existing=0 → `e` e' ceduto (steal): dopo il set si lascia andare il ref ceduto. */
        gmb_val_t newV = (e.type == 2 && e.ref) ? gmb_make_val_obj((gmb_obj_t*)e.ref)
            : (e.type == 1 ? gmb_make_val_double(e.num) : gmb_val_nil());
        gmb_list_set(b->arr->list, (gmb_int_t)b->slot, newV);
        if (!retain_existing && e.type == 2 && e.ref) gmb_obj_release(e.ref);
        return;
    }
    if (!b->owner) return;
    gmb_task_t* t = b->owner;
    if (b->is_global)
    {
        gmb_task_ensure_globals(t, (uint32_t)(b->slot + 1));
        if (b->slot >= 0 && b->slot < (int)t->globals_count)
        {
            if (t->globals_types[b->slot] == 2 && t->globals_refs[b->slot])
                gmb_obj_release(t->globals_refs[b->slot]);
            t->globals_nums[b->slot] = e.num;
            t->globals_refs[b->slot] = e.type == 2 ? (gmb_obj_t*)e.ref : NULL;
            if (retain_existing && e.type == 2 && e.ref) gmb_obj_retain((gmb_obj_t*)e.ref);
            t->globals_types[b->slot] = e.type;
            t->globals_basic[b->slot] = e.basic_type;
        }
        return;
    }
    double* nums; gmb_obj_t** refs; uint8_t* types; uint8_t* basic;
    if (b->frame) { nums = b->frame->local_nums; refs = b->frame->local_refs; types = b->frame->local_types; basic = b->frame->local_basic; }
    else { nums = t->locals_nums; refs = t->locals_refs; types = t->locals_types; basic = t->locals_basic; }
    int cap = b->frame ? b->frame->nlocals : t->locals_cap;
    if (b->slot < 0 || b->slot >= cap) return;
    if (types[b->slot] == 2 && refs[b->slot]) gmb_obj_release(refs[b->slot]);
    nums[b->slot] = e.num;
    refs[b->slot] = e.type == 2 ? (gmb_obj_t*)e.ref : NULL;
    if (retain_existing && e.type == 2 && e.ref) gmb_obj_retain((gmb_obj_t*)e.ref);
    types[b->slot] = e.type;
    basic[b->slot] = e.basic_type;
}

gmb_stack_entry_t gmb_byref_load(const gmb_obj_byref_t* box) { return byref_read(box); }
void gmb_byref_store(gmb_obj_byref_t* box, gmb_stack_entry_t value) { byref_write(box, value, 0); }

static gmb_stack_entry_t capture_slot_byref_x(double* nums, gmb_obj_t** refs,
                                               uint8_t* types, uint8_t* basic, int cap, int slot,
                                               int box_strings)
{
    gmb_stack_entry_t out = { 0 };
    if (slot < 0 || slot >= cap) return out;
    if (types[slot] == 2 && refs[slot] && refs[slot]->kind == GMB_OBJ_BYREF)
    {
        out.type = 2; out.ref = refs[slot]; out.basic_type = GMB_BT_OBJECT;
        gmb_obj_retain(out.ref);
        return out;
    }
    /* 2026-09-24: NON proxyare i REFERENCE type (parità PCodeVM OP_CLOSURE: "NON proxyare
     * i REFERENCE type (dict/oggetti/array/chiusure): sono già condivisi per riferimento
     * catturando il ref, e un proxy a uno slot di un frame POPPATO legge garbage"). Il
     * __ClosureCtx è un DICT catturato da OP_CLOSURE: con il proxy-cell la closure
     * riceveva il BOX (non il dict) in slot 0 → OP_LOAD_MEMBER/STORE_MEMBER sul box non
     * dereferenziavano il dict → nil (bug FunctionsAdvanced/FunctionsLambdaClosures).
     * I box BYREF (caso sopra) restano proxy. Solo gli scalari (type==1) e nil diventano
     * cell — così le scritture della closure e del chiamante condividono lo slot. */
    if (types[slot] == 2 && refs[slot])
    {
        /* box_strings=1 SOLO per i target di @GET: una variabile STRINGA locale deve
         * ricevere il testo digitato, quindi serve una cella write-through. Un oggetto
         * stringa "puro" (kind GMB_OBJ_STRING senza type_name, la stessa discriminante
         * dell'adapter) è un valore scalare: imbustarlo è sicuro. Dict/oggetti/array/
         * chiusure restano NON imbustati anche qui (sono condivisi e un proxy a uno slot
         * di un frame poppato leggerebbe garbage). */
        if (!(box_strings && refs[slot]->kind == GMB_OBJ_STRING && !refs[slot]->type_name))
        {
            out.type = 2; out.ref = refs[slot]; out.basic_type = basic[slot];
            gmb_obj_retain(out.ref);
            return out;
        }
    }
    gmb_obj_byref_t* box = (gmb_obj_byref_t*)calloc(1, sizeof(*box));
    if (!box) return out;
    box->header.refcount = 1; box->header.kind = GMB_OBJ_BYREF; box->header.release_fn = byref_release_fn;
    box->is_cell = 1;
    box->cell.type = types[slot]; box->cell.num = nums[slot]; box->cell.ref = refs[slot]; box->cell.basic_type = basic[slot];
    nums[slot] = 0; refs[slot] = (gmb_obj_t*)box; types[slot] = 2; basic[slot] = GMB_BT_OBJECT;
    out.type = 2; out.ref = (gmb_obj_t*)box; out.basic_type = GMB_BT_OBJECT;
    gmb_obj_retain(out.ref); /* un ref allo slot, uno restituito alla closure */
    return out;
}

gmb_stack_entry_t gmb_task_capture_local_byref(gmb_task_t* t, int slot)
{
    if (!t) return (gmb_stack_entry_t){0};
    /* box_strings=1: anche una STRINGA va imbustata in una cella write-through. Una
     * closure cattura la VARIABILE, non il valore (parità C#/IL/C99): senza cella il
     * `s = "b"` del chiamante sostituiva l'oggetto nello slot e la closure continuava a
     * vedere l'oggetto vecchio. La cella `is_cell` possiede il valore → sopravvive
     * anche al pop del frame (closure ritornata). */
    if (slot >= 0) gmb_task_ensure_locals(t, slot + 1);
    int cap = gmb_locals_cap_of(t);
    if (t->frame) return capture_slot_byref_x(t->frame->local_nums, t->frame->local_refs,
        t->frame->local_types, t->frame->local_basic, cap, slot, 1);
    return capture_slot_byref_x(t->locals_nums, t->locals_refs, t->locals_types, t->locals_basic, cap, slot, 1);
}

/* Target di @GET (anche stringa): cella write-through sullo slot locale. */
gmb_stack_entry_t gmb_task_capture_local_cell(gmb_task_t* t, int slot)
{
    if (!t) return (gmb_stack_entry_t){0};
    if (slot >= 0) gmb_task_ensure_locals(t, slot + 1);
    int cap = gmb_locals_cap_of(t);
    if (t->frame) return capture_slot_byref_x(t->frame->local_nums, t->frame->local_refs,
        t->frame->local_types, t->frame->local_basic, cap, slot, 1);
    return capture_slot_byref_x(t->locals_nums, t->locals_refs, t->locals_types, t->locals_basic, cap, slot, 1);
}

gmb_stack_entry_t gmb_task_capture_global_byref(gmb_task_t* t, int slot)
{
    gmb_stack_entry_t out = {0};
    if (!t || slot < 0) return out;
    gmb_task_ensure_globals(t, (uint32_t)(slot + 1));
    if (slot >= (int)t->globals_count) return out;
    /* I global hanno la stessa rappresentazione degli slot locali, ma capacità dinamica. */
    if (t->globals_types[slot] == 2 && t->globals_refs[slot] && t->globals_refs[slot]->kind == GMB_OBJ_BYREF)
    {
        out.type = 2; out.ref = t->globals_refs[slot]; out.basic_type = GMB_BT_OBJECT;
        gmb_obj_retain(out.ref); return out;
    }
    gmb_obj_byref_t* box = (gmb_obj_byref_t*)calloc(1, sizeof(*box));
    if (!box) return out;
    box->header.refcount = 1; box->header.kind = GMB_OBJ_BYREF; box->header.release_fn = byref_release_fn;
    box->is_cell = 1;
    box->cell.type = t->globals_types[slot]; box->cell.num = t->globals_nums[slot];
    box->cell.ref = t->globals_refs[slot]; box->cell.basic_type = t->globals_basic[slot];
    t->globals_nums[slot] = 0; t->globals_refs[slot] = (gmb_obj_t*)box;
    t->globals_types[slot] = 2; t->globals_basic[slot] = GMB_BT_OBJECT;
    out.type = 2; out.ref = (gmb_obj_t*)box; out.basic_type = GMB_BT_OBJECT;
    gmb_obj_retain(out.ref); return out;
}

void gmb_task_store_local(gmb_task_t* t, int slot, gmb_stack_entry_t e)
{
    gmb_frame_t* f = t->frame;
    if (slot < 0) return;
    /* crescita on demand: uno slot mai visto allarga i locals (frame o top-level) */
    if (!gmb_task_ensure_locals(t, slot + 1)) return;
    double* nums; gmb_obj_t** refs; uint8_t* types; uint8_t* basic;
    if (f) { nums = f->local_nums; refs = f->local_refs; types = f->local_types; basic = f->local_basic; }
    else { nums = t->locals_nums; refs = t->locals_refs; types = t->locals_types; basic = t->locals_basic; }
    if (slot >= gmb_locals_cap_of(t)) return;
    /* BYREF write-through: lo slot contiene un box (specchio C#: ByRefBox.Value = val). */
    if (types[slot] == 2 && refs[slot] && refs[slot]->kind == GMB_OBJ_BYREF)
    {
        byref_write((gmb_obj_byref_t*)refs[slot], e, 0);
        return;
    }
    if (types[slot] == 2 && refs[slot]) gmb_obj_release(refs[slot]);
    nums[slot] = e.num;
    refs[slot] = e.type == 2 ? (gmb_obj_t*)e.ref : NULL;
    types[slot] = e.type;
    basic[slot] = e.basic_type;
    if (f) f->return_value = e;
}

gmb_stack_entry_t gmb_task_load_local(gmb_task_t* t, int slot)
{
    gmb_stack_entry_t e = { 0 };
    gmb_frame_t* f = t->frame;
    double* nums; gmb_obj_t** refs; uint8_t* types; uint8_t* basic;
    if (f) { nums = f->local_nums; refs = f->local_refs; types = f->local_types; basic = f->local_basic; }
    else { nums = t->locals_nums; refs = t->locals_refs; types = t->locals_types; basic = t->locals_basic; }
    if (slot < 0 || slot >= gmb_locals_cap_of(t)) return e;
    e.type = types[slot]; e.num = nums[slot]; e.ref = refs[slot]; e.basic_type = basic[slot];
    /* BYREF: lo slot contiene un box → risolve al valore del target. */
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_BYREF)
        return byref_read((const gmb_obj_byref_t*)e.ref);
    return e;
}

void gmb_task_ensure_globals(gmb_task_t* t, uint32_t n)
{
    if (t->globals_nums) return;
    if (n == 0) n = 256;
    t->globals_count = n;
    t->globals_nums = (double*)calloc(n, sizeof(double));
    t->globals_refs = (gmb_obj_t**)calloc(n, sizeof(gmb_obj_t*));
    t->globals_types = (uint8_t*)calloc(n, 1);
    t->globals_basic = (uint8_t*)calloc(n, 1);
}

void gmb_task_store_global(gmb_task_t* t, int idx, gmb_stack_entry_t e)
{
    gmb_task_ensure_globals(t, (uint32_t)(idx + 1));
    if (idx >= 0 && idx < (int)t->globals_count)
    {
        /* BYREF write-through (specchio C#: il global contiene un ByRefBox). */
        if (t->globals_types[idx] == 2 && t->globals_refs[idx] &&
            t->globals_refs[idx]->kind == GMB_OBJ_BYREF)
        {
            byref_write((gmb_obj_byref_t*)t->globals_refs[idx], e, 1);
            return;
        }
        if (t->globals_types[idx] == 2 && t->globals_refs[idx]) gmb_obj_release(t->globals_refs[idx]);
        t->globals_nums[idx] = e.num;
        t->globals_refs[idx] = e.type == 2 ? (gmb_obj_t*)e.ref : NULL;
        if (e.type == 2 && e.ref) gmb_obj_retain((gmb_obj_t*)e.ref);
        t->globals_types[idx] = e.type;
        t->globals_basic[idx] = e.basic_type;
    }
}

gmb_stack_entry_t gmb_task_load_global(gmb_task_t* t, int idx)
{
    gmb_stack_entry_t e = { 0 };
    if (idx < 0 || idx >= (int)t->globals_count) return e;
    e.type = t->globals_types[idx]; e.num = t->globals_nums[idx];
    e.ref = t->globals_refs[idx]; e.basic_type = t->globals_basic[idx];
    /* BYREF: il global contiene un box → risolve al valore del target. */
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_BYREF)
        return byref_read((const gmb_obj_byref_t*)e.ref);
    return e;
}

/* ============================================================================
 * Scheduler
 * ============================================================================ */

gmb_scheduler_t* gmb_sched_create(void)
{
    gmb_scheduler_t* s = (gmb_scheduler_t*)calloc(1, sizeof(gmb_scheduler_t));
    if (!s) return NULL;
    s->queue_cap = 64;
    s->queue = (gmb_task_t**)calloc(s->queue_cap, sizeof(gmb_task_t*));
    pthread_mutex_init(&s->qlock, NULL);
    pthread_cond_init(&s->qcond, NULL);
    pthread_mutex_init(&s->all_lock, NULL);
    pthread_mutex_init(&s->clock, NULL);
    pthread_cond_init(&s->ccond, NULL);
    pthread_mutex_init(&s->tlock, NULL);
    pthread_cond_init(&s->tcond, NULL);
    return s;
}

gmb_task_t* gmb_task_create(gmb_scheduler_t* s, int start_ip)
{
    gmb_task_t* t = (gmb_task_t*)calloc(1, sizeof(gmb_task_t));
    if (!t) return NULL;
    t->id = ++s->next_id;
    t->state = GMB_TS_RUNNABLE;
    t->ip = start_ip;
    t->scheduler = s;
    t->error_line_ip = -1;
    t->error_resume_next_ip = -1;
    pthread_mutex_init(&t->lock, NULL);
    pthread_mutex_lock(&s->all_lock);
    if (s->all_count >= s->all_cap)
    {
        s->all_cap = s->all_cap ? s->all_cap * 2 : 64;
        s->all_tasks = (gmb_task_t**)realloc(s->all_tasks, s->all_cap * sizeof(gmb_task_t*));
    }
    s->all_tasks[s->all_count++] = t;
    pthread_mutex_unlock(&s->all_lock);
    /* V7a: il task contribuisce al conteggio active (completion condition). */
    pthread_mutex_lock(&s->clock);
    s->active_count++;
    pthread_mutex_unlock(&s->clock);
    return t;
}

/* Crea un task dedicato NON registrato nello scheduler (per generatori YIELD, guidati
 * sincronamente da NEXT, mai schedulati sul GPM). Non compare in all_tasks, quindi non
 * blocca gmb_sched_has_active / lo shutdown. */
gmb_task_t* gmb_task_create_unregistered(int start_ip)
{
    gmb_task_t* t = (gmb_task_t*)calloc(1, sizeof(gmb_task_t));
    if (!t) return NULL;
    t->id = 0;
    t->state = GMB_TS_RUNNABLE;
    t->ip = start_ip;
    t->error_line_ip = -1;
    t->error_resume_next_ip = -1;
    pthread_mutex_init(&t->lock, NULL);
    return t;
}

static void queue_push(gmb_scheduler_t* s, gmb_task_t* t)
{
    if (s->queue_count >= s->queue_cap)
    {
        int ncap = s->queue_cap ? s->queue_cap * 2 : 64;
        gmb_task_t** nq = (gmb_task_t**)malloc(ncap * sizeof(gmb_task_t*));
        /* ricompatta */
        int n = 0;
        for (int i = 0; i < s->queue_count; i++) nq[n++] = s->queue[(s->queue_head + i) % s->queue_cap];
        free(s->queue);
        s->queue = nq; s->queue_cap = ncap; s->queue_head = 0; s->queue_tail = n;
    }
    s->queue[s->queue_tail] = t;
    s->queue_tail = (s->queue_tail + 1) % s->queue_cap;
    s->queue_count++;
}

static gmb_task_t* queue_pop(gmb_scheduler_t* s)
{
    if (s->queue_count == 0) return NULL;
    gmb_task_t* t = s->queue[s->queue_head];
    s->queue_head = (s->queue_head + 1) % s->queue_cap;
    s->queue_count--;
    return t;
}

void gmb_sched_enqueue(gmb_scheduler_t* s, gmb_task_t* t)
{
    pthread_mutex_lock(&t->lock);
    if (t->state == GMB_TS_FINISHED) { pthread_mutex_unlock(&t->lock); return; }
    if (t->executing) { t->wake_pending = 1; pthread_mutex_unlock(&t->lock); return; }
    if (t->in_queue) { pthread_mutex_unlock(&t->lock); return; }
    t->in_queue = 1;
    pthread_mutex_unlock(&t->lock);
    t->state = GMB_TS_RUNNABLE;
    pthread_mutex_lock(&s->qlock);
    if (!s->shutdown) queue_push(s, t);
    pthread_cond_signal(&s->qcond);
    pthread_mutex_unlock(&s->qlock);
}

int gmb_sched_is_stuck(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->qlock);
    int qc = s->queue_count;
    pthread_mutex_unlock(&s->qlock);
    if (qc > 0) return 0;
    /* F1: un job pthread dedicato (BLACKBOARDWAIT/AGENTSWARM parcheggiati) e' lavoro
     * vero in corso anche se il task e' BLOCKED e nessun worker e' "executing" —
     * senza questo check il watchdog anti-deadlock lo scambierebbe per un vero stuck. */
    pthread_mutex_lock(&s->clock);
    int async_jobs = s->async_job_count;
    pthread_mutex_unlock(&s->clock);
    if (async_jobs > 0) return 0;
    pthread_mutex_lock(&s->all_lock);
    int any_executing = 0;
    for (int i = 0; i < s->all_count && !any_executing; i++)
    {
        gmb_task_t* t = s->all_tasks[i];
        pthread_mutex_lock(&t->lock);
        if (t->state != GMB_TS_FINISHED && t->executing) any_executing = 1;
        pthread_mutex_unlock(&t->lock);
    }
    pthread_mutex_unlock(&s->all_lock);
    return !any_executing;
}

void gmb_sched_release_executing(gmb_scheduler_t* s, gmb_task_t* t)
{
    int rewake;
    pthread_mutex_lock(&t->lock);
    t->executing = 0;
    rewake = t->wake_pending;
    t->wake_pending = 0;
    pthread_mutex_unlock(&t->lock);
    if (rewake) gmb_sched_enqueue(s, t);
}

/* V7a-net: i runner single-thread (gmb_vm_run/run_debug) non hanno worker che
 * consumano la run-queue: un wake hook che accoda lascerebbe in_queue=1 per sempre
 * (i successivi enqueue diventerebbero no-op e il task resterebbe BLOCKED). Questo
 * helper rimuove il task dalla coda e azzera in_queue prima del tick. */
void gmb_sched_reclaim_queued(gmb_scheduler_t* s, gmb_task_t* t)
{
    pthread_mutex_lock(&t->lock);
    int was = t->in_queue;
    t->in_queue = 0;
    pthread_mutex_unlock(&t->lock);
    if (!was) return;
    pthread_mutex_lock(&s->qlock);
    int n = s->queue_count, w = 0;
    for (int i = 0; i < n; i++)
    {
        gmb_task_t* q = s->queue[(s->queue_head + i) % s->queue_cap];
        if (q != t) s->queue[w++] = q;
    }
    s->queue_head = 0;
    s->queue_tail = w;
    s->queue_count = w;
    pthread_mutex_unlock(&s->qlock);
}

gmb_task_t* gmb_sched_find_runnable(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->qlock);
    while (s->queue_count == 0 && !s->shutdown)
        pthread_cond_wait(&s->qcond, &s->qlock);
    if (s->queue_count == 0) { pthread_mutex_unlock(&s->qlock); return NULL; }
    gmb_task_t* t = queue_pop(s);
    pthread_mutex_unlock(&s->qlock);
    if (t)
    {
        pthread_mutex_lock(&t->lock);
        t->in_queue = 0;
        t->executing = 1;
        pthread_mutex_unlock(&t->lock);
    }
    return t;
}

static void gmb_sched_wake_timer_watcher(gmb_scheduler_t* s);

void gmb_sched_signal_shutdown(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->qlock);
    s->shutdown = 1;
    pthread_cond_broadcast(&s->qcond);
    pthread_mutex_unlock(&s->qlock);
    /* Il watcher può essere in timed wait sui timer: va svegliato, altrimenti il join
     * in chiusura aspetta fino al cap di 250 ms (vedi gmb_sched_wake_timer_watcher). */
    gmb_sched_wake_timer_watcher(s);
}

bool gmb_sched_has_active(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->all_lock);
    bool active = false;
    for (int i = 0; i < s->all_count; i++)
        if (s->all_tasks[i]->state != GMB_TS_FINISHED) { active = true; break; }
    pthread_mutex_unlock(&s->all_lock);
    return active;
}

void gmb_sched_wake_expired_sleepers(gmb_scheduler_t* s, int64_t now_ms)
{
    pthread_mutex_lock(&s->all_lock);
    gmb_task_t** snap = (gmb_task_t**)malloc(s->all_count * sizeof(gmb_task_t*));
    int n = 0;
    for (int i = 0; i < s->all_count; i++)
        if (s->all_tasks[i]->state != GMB_TS_FINISHED) snap[n++] = s->all_tasks[i];
    pthread_mutex_unlock(&s->all_lock);
    for (int i = 0; i < n; i++)
    {
        gmb_task_t* t = snap[i];
        int expired = 0;
        pthread_mutex_lock(&t->lock);
        if (t->state == GMB_TS_BLOCKED && t->sleep_until_ms > 0 && t->sleep_until_ms <= now_ms)
        { t->sleep_until_ms = 0; expired = 1; }
        pthread_mutex_unlock(&t->lock);
        if (expired) gmb_sched_enqueue(s, t);
    }
    free(snap);
}

/* Specchio GpmCore.RequestCancellation/CancelTasks: flag su tutti i task non finiti,
 * azzera lo sleep pendente e li sveglia (enqueue è no-op su FINISHED/in coda/in
 * esecuzione). Il tick loop vede cancellation_requested e inizia l'unwind Cancel,
 * che drena i FINALLY prima di terminare il task. */
void gmb_sched_request_cancellation(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->all_lock);
    gmb_task_t** snap = (gmb_task_t**)malloc((s->all_count ? s->all_count : 1) * sizeof(gmb_task_t*));
    int n = 0;
    for (int i = 0; i < s->all_count; i++)
        if (s->all_tasks[i]->state != GMB_TS_FINISHED) snap[n++] = s->all_tasks[i];
    pthread_mutex_unlock(&s->all_lock);
    for (int i = 0; i < n; i++)
    {
        gmb_task_t* t = snap[i];
        pthread_mutex_lock(&t->lock);
        t->cancellation_requested = 1;
        t->sleep_until_ms = 0;
        pthread_mutex_unlock(&t->lock);
        gmb_sched_enqueue(s, t);
    }
    free(snap);
}

int64_t gmb_sched_earliest_sleep(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->all_lock);
    gmb_task_t** snap = (gmb_task_t**)malloc(s->all_count * sizeof(gmb_task_t*));
    int n = 0;
    for (int i = 0; i < s->all_count; i++)
        if (s->all_tasks[i]->state != GMB_TS_FINISHED) snap[n++] = s->all_tasks[i];
    pthread_mutex_unlock(&s->all_lock);
    int64_t earliest = -1;
    for (int i = 0; i < n; i++)
    {
        gmb_task_t* t = snap[i];
        pthread_mutex_lock(&t->lock);
        if (t->state == GMB_TS_BLOCKED && t->sleep_until_ms > 0 &&
            (earliest < 0 || t->sleep_until_ms < earliest))
            earliest = t->sleep_until_ms;
        pthread_mutex_unlock(&t->lock);
    }
    free(snap);
    return earliest;
}

void gmb_task_mark_finished(gmb_task_t* t)
{
    t->state = GMB_TS_FINISHED;
}

/* ============================================================================
 * V7a (§7.1): completion condition + timer min-heap.
 * - completion: active_count sotto clock; il main di gmb_vm_run_mt attende su
 *   ccond invece di pollare a 1ms.
 * - timer: min-heap {deadline, task, id}; il watcher attende su tcond fino alla
 *   deadline minima (cap 250ms) invece di scansionare tutti i task. I nodi sono
 *   validati al fire (membership in all_tasks + id) perché i disarm sono pigri.
 * ============================================================================ */

/* Sveglia il watcher dei timer: quando non c'è più nulla da fare deve USCIRE dal timed
 * wait (cap 250 ms) invece di dormirci fino in fondo. Senza questa sveglia ogni
 * gmb_vm_run_mt pagava ~250 ms di chiusura ogni volta che il programma durava più del
 * primo giro del watcher — misurato su una pagina BSP che fa 0,5 ms di lavoro reale:
 * run_mt = 255 ms, di cui 254 di pura attesa del join (motore BSP in C: 3,9 rps
 * invece di ~2.000). Lock: solo tcond/tlock, nessun ordine inverso altrove. */
static void gmb_sched_wake_timer_watcher(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->tlock);
    pthread_cond_broadcast(&s->tcond);
    pthread_mutex_unlock(&s->tlock);
}

void gmb_sched_note_task_finished(gmb_scheduler_t* s)
{
    int idle;
    pthread_mutex_lock(&s->clock);
    if (s->active_count > 0) s->active_count--;
    idle = (s->active_count == 0);
    if (idle) pthread_cond_broadcast(&s->ccond);
    pthread_mutex_unlock(&s->clock);
    if (idle) gmb_sched_wake_timer_watcher(s);
}

/* Marcatori di attività per risorse non-task (es. receive loop WebSocket lato server):
 * tengono attiva la completion condition finché la sessione è viva, così wait_all_done
 * non scatta mentre la sessione può ancora spawnare callback (OnMessage/OnClose). */
void gmb_sched_hold_active(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->clock);
    s->active_count++;
    pthread_mutex_unlock(&s->clock);
}

void gmb_sched_release_active(gmb_scheduler_t* s)
{
    int idle;
    pthread_mutex_lock(&s->clock);
    if (s->active_count > 0) s->active_count--;
    idle = (s->active_count == 0);
    if (idle) pthread_cond_broadcast(&s->ccond);
    pthread_mutex_unlock(&s->clock);
    if (idle) gmb_sched_wake_timer_watcher(s);
}

int gmb_sched_active_count(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->clock);
    int n = s->active_count;
    pthread_mutex_unlock(&s->clock);
    return n;
}

/* F1: marcatori del job pthread dedicato (vm_try_park_str) — stesso mutex 'clock' di
 * active_count (nessun nuovo lock). Non toccano active_count/ccond: async_job_count
 * serve SOLO al watchdog gmb_sched_is_stuck, non alla completion condition (il task
 * resta comunque contato in active_count come non-FINISHED). */
void gmb_sched_async_job_begin(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->clock);
    s->async_job_count++;
    pthread_mutex_unlock(&s->clock);
}

void gmb_sched_async_job_end(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->clock);
    if (s->async_job_count > 0) s->async_job_count--;
    pthread_mutex_unlock(&s->clock);
}

int gmb_sched_wait_all_done_ms(gmb_scheduler_t* s, int64_t timeout_ms)
{
    pthread_mutex_lock(&s->clock);
    int done = 0;
    if (timeout_ms < 0)
    {
        while (s->active_count > 0)
            pthread_cond_wait(&s->ccond, &s->clock);
        done = 1;
    }
    else
    {
        struct timespec ab;
        clock_gettime(CLOCK_REALTIME, &ab);
        ab.tv_sec += (time_t)(timeout_ms / 1000);
        ab.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
        if (ab.tv_nsec >= 1000000000L) { ab.tv_sec++; ab.tv_nsec -= 1000000000L; }
        while (s->active_count > 0)
            if (pthread_cond_timedwait(&s->ccond, &s->clock, &ab) == ETIMEDOUT) break;
        done = (s->active_count == 0);
    }
    pthread_mutex_unlock(&s->clock);
    return done;
}

static void timer_heap_push(gmb_scheduler_t* s, gmb_timer_node_t n)
{
    if (s->timer_count >= s->timer_cap)
    {
        int ncap = s->timer_cap ? s->timer_cap * 2 : 32;
        gmb_timer_node_t* nt = (gmb_timer_node_t*)realloc(s->timers, ncap * sizeof(gmb_timer_node_t));
        if (!nt) return; /* nodo perso: il watcher ha comunque il cap 250ms */
        s->timers = nt;
        s->timer_cap = ncap;
    }
    int i = s->timer_count++;
    s->timers[i] = n;
    while (i > 0)
    {
        int p = (i - 1) / 2;
        if (s->timers[p].deadline <= s->timers[i].deadline) break;
        gmb_timer_node_t tmp = s->timers[p];
        s->timers[p] = s->timers[i];
        s->timers[i] = tmp;
        i = p;
    }
}

static gmb_timer_node_t timer_heap_pop(gmb_scheduler_t* s)
{
    gmb_timer_node_t top = s->timers[0];
    s->timers[0] = s->timers[--s->timer_count];
    int i = 0;
    for (;;)
    {
        int l = 2 * i + 1, r = 2 * i + 2, m = i;
        if (l < s->timer_count && s->timers[l].deadline < s->timers[m].deadline) m = l;
        if (r < s->timer_count && s->timers[r].deadline < s->timers[m].deadline) m = r;
        if (m == i) break;
        gmb_timer_node_t tmp = s->timers[m];
        s->timers[m] = s->timers[i];
        s->timers[i] = tmp;
        i = m;
    }
    return top;
}

void gmb_sched_arm_timer(gmb_scheduler_t* s, gmb_task_t* t)
{
    if (!s || !t) return;
    pthread_mutex_lock(&t->lock);
    int64_t dl = t->sleep_until_ms;
    int id = t->id;
    pthread_mutex_unlock(&t->lock);
    if (dl <= 0) return;
    pthread_mutex_lock(&s->tlock);
    gmb_timer_node_t n;
    n.deadline = dl;
    n.task = t;
    n.task_id = id;
    timer_heap_push(s, n);
    pthread_cond_signal(&s->tcond);
    pthread_mutex_unlock(&s->tlock);
}

int64_t gmb_sched_next_timer_deadline(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->tlock);
    int64_t dl = s->timer_count > 0 ? s->timers[0].deadline : -1;
    pthread_mutex_unlock(&s->tlock);
    return dl;
}

static int timer_task_is_member(gmb_scheduler_t* s, gmb_task_t* t)
{
    /* chiamato con all_lock già tenuto */
    for (int i = 0; i < s->all_count; i++)
        if (s->all_tasks[i] == t) return 1;
    return 0;
}

void gmb_sched_fire_expired_timers(gmb_scheduler_t* s, int64_t now_ms)
{
    /* Raccoglie i nodi scaduti sotto tlock, poi li valida sotto all_lock (ordine
     * lock: mai tlock tenuto mentre si prende all_lock — qui sono fasi separate). */
    gmb_timer_node_t fired[64];
    int nf = 0;
    pthread_mutex_lock(&s->tlock);
    while (s->timer_count > 0 && s->timers[0].deadline <= now_ms && nf < 64)
        fired[nf++] = timer_heap_pop(s);
    pthread_mutex_unlock(&s->tlock);
    for (int i = 0; i < nf; i++)
    {
        gmb_task_t* t = fired[i].task;
        int wake = 0;
        pthread_mutex_lock(&s->all_lock);
        if (timer_task_is_member(s, t))
        {
            pthread_mutex_lock(&t->lock);
            /* validazione: id anti-ABA; deadline coerente (i disarm e i re-arm
             * lasciano nodi pigri nello heap); stato ancora BLOCKED da sleep. */
            if (t->id == fired[i].task_id &&
                t->state == GMB_TS_BLOCKED &&
                t->sleep_until_ms > 0 && t->sleep_until_ms <= now_ms)
            {
                t->sleep_until_ms = 0;
                wake = 1;
            }
            pthread_mutex_unlock(&t->lock);
        }
        pthread_mutex_unlock(&s->all_lock);
        if (wake) gmb_sched_enqueue(s, t);
    }
}

void gmb_sched_timer_wait(gmb_scheduler_t* s, int64_t next_deadline)
{
    int64_t now = gmb_now_ms();
    int64_t wait_ms = 250;
    if (next_deadline >= 0)
    {
        wait_ms = next_deadline - now;
        if (wait_ms > 250) wait_ms = 250;
        if (wait_ms < 1) wait_ms = 1;
    }
    struct timespec ab;
    clock_gettime(CLOCK_REALTIME, &ab);
    ab.tv_sec += (time_t)(wait_ms / 1000);
    ab.tv_nsec += (long)(wait_ms % 1000) * 1000000L;
    if (ab.tv_nsec >= 1000000000L) { ab.tv_sec++; ab.tv_nsec -= 1000000000L; }
    pthread_mutex_lock(&s->tlock);
    pthread_cond_timedwait(&s->tcond, &s->tlock, &ab);
    pthread_mutex_unlock(&s->tlock);
}

static void channel_release_fn(struct gmb_obj* o)
{
    gmb_channel_t* c = (gmb_channel_t*)o;
    free(c->buffer);
    /* V6: le entry residue tengono un ref al SelectContext — rilascialo. */
    for (int i = 0; i < c->recv_count; i++)
    { gmb_wait_entry_t e = c->recv_entries[(c->recv_head + i) % c->recv_cap]; if (e.sel) gmb_select_release(e.sel); }
    for (int i = 0; i < c->send_count; i++)
    { gmb_wait_entry_t e = c->send_entries[(c->send_head + i) % c->send_cap]; if (e.sel) gmb_select_release(e.sel); }
    free(c->recv_entries);
    free(c->send_entries);
    pthread_mutex_destroy(&c->lock);
    free(c);
}

static void waitgroup_release_fn(struct gmb_obj* o)
{
    gmb_waitgroup_t* w = (gmb_waitgroup_t*)o;
    free(w->waiters);
    pthread_mutex_destroy(&w->lock);
    free(w);
}

static void mutex_release_fn(struct gmb_obj* o)
{
    gmb_mutex_t* m = (gmb_mutex_t*)o;
    free(m->waiters);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

static void rwlock_release_fn(struct gmb_obj* o)
{
    gmb_rwlock_t* rw = (gmb_rwlock_t*)o;
    free(rw->waiters);
    pthread_mutex_destroy(&rw->lock);
    free(rw);
}

static void once_release_fn(struct gmb_obj* o)
{
    gmb_once_t* on = (gmb_once_t*)o;
    free(on->waiters);
    pthread_mutex_destroy(&on->lock);
    free(on);
}

static void future_release_fn(struct gmb_obj* o)
{
    gmb_future_t* f = (gmb_future_t*)o;
    /* Il future POSSiede f->result (il produttore gli trasferisce il proprio riferimento:
     * gmb_future_resolve riceve un oggetto fresco e non lo rilascia). I waiter lo ritengono
     * con push_ref_entry (retain). Senza questa release l'oggetto risultato di OGNI future
     * (FETCH/GO FUNCTION) restava irraggiungibile → leak (bug RuntimeC-Dict-Boxato...). */
    if (f->result) gmb_obj_release(f->result);
    free(f->waiters);
    pthread_mutex_destroy(&f->lock);
    free(f);
}

static void atomic_release_fn(struct gmb_obj* o)
{
    free(o);
}

static void pool_release_fn(struct gmb_obj* o)
{
    gmb_pool_t* p = (gmb_pool_t*)o;
    gmb_pool_destroy(p);
}

static void cond_release_fn(struct gmb_obj* o)
{
    gmb_cond_t* c = (gmb_cond_t*)o;
    free(c->waiters);
    pthread_mutex_destroy(&c->lock);
    free(c);
}

static void context_release_fn(struct gmb_obj* o)
{
    gmb_context_destroy((gmb_context_t*)o);
}

static void concurrentmap_release_fn(struct gmb_obj* o)
{
    gmb_concurrentmap_destroy((gmb_concurrentmap_t*)o);
}

/* ============================================================================
 * Channel
 * ============================================================================ */

gmb_channel_t* gmb_channel_create(int capacity)
{
    gmb_channel_t* c = (gmb_channel_t*)calloc(1, sizeof(gmb_channel_t));
    if (!c) return NULL;
    c->header.refcount = 1;
    c->header.kind = GMB_OBJ_CHANNEL;
    c->header.release_fn = channel_release_fn;
    pthread_mutex_init(&c->lock, NULL);
    /* Parità C# ChannelInfo: capacity 0 = unbuffered (handshake Go make(chan T)),
     * implementato come 1 slot fisico in transito — il sender del valore successivo
     * parcheggia finché un receiver non preleva. Prima era :10 (divergenza). */
    c->capacity = capacity > 0 ? capacity : 1;
    c->buffer_cap = c->capacity;
    c->buffer = (gmb_stack_entry_t*)calloc(c->buffer_cap, sizeof(gmb_stack_entry_t));
    return c;
}

void gmb_channel_destroy(gmb_channel_t* c)
{
    if (!c) return;
    /* Il buffer RITIENE i ref (protocollo send): rilascia quelli non consumati. */
    for (int i = 0; i < c->buffer_count; i++)
    {
        gmb_stack_entry_t* e = &c->buffer[(c->buffer_head + i) % c->buffer_cap];
        if (e->type == 2 && e->ref) gmb_obj_release(e->ref);
    }
    free(c->buffer);
    for (int i = 0; i < c->recv_count; i++)
    { gmb_wait_entry_t e = c->recv_entries[(c->recv_head + i) % c->recv_cap]; if (e.sel) gmb_select_release(e.sel); }
    for (int i = 0; i < c->send_count; i++)
    { gmb_wait_entry_t e = c->send_entries[(c->send_head + i) % c->send_cap]; if (e.sel) gmb_select_release(e.sel); }
    free(c->recv_entries);
    free(c->send_entries);
    pthread_mutex_destroy(&c->lock);
    free(c);
}

/* V6: code di entry (non raw task pointer). Ogni push ritiene il SelectContext della
 * entry (se presente); chi fa pop/detach rilascia quel ref DOPO averlo usato (claim). */
static void cq_push(gmb_wait_entry_t** arr, int* cap, int* head, int* tail, int* count, gmb_wait_entry_t e)
{
    if (*count >= *cap)
    {
        int ncap = *cap ? *cap * 2 : 8;
        gmb_wait_entry_t* na = (gmb_wait_entry_t*)malloc(ncap * sizeof(gmb_wait_entry_t));
        int n = 0;
        for (int i = 0; i < *count; i++) na[n++] = (*arr)[(*head + i) % *cap];
        free(*arr);
        *arr = na; *cap = ncap; *head = 0; *tail = n;
    }
    if (e.sel) gmb_select_retain(e.sel);
    (*arr)[*tail] = e;
    *tail = (*tail + 1) % *cap;
    (*count)++;
}

static gmb_wait_entry_t cq_pop(gmb_wait_entry_t** arr, int* cap, int* head, int* tail, int* count)
{
    gmb_wait_entry_t e = { 0 };
    if (*count == 0) return e;
    e = (*arr)[*head];
    *head = (*head + 1) % *cap;
    (*count)--;
    return e;
}

/* Detach loser (V6): rimuove TUTTE le entry del SelectContext `s` dalla coda, rilasciando
 * il ref che il push aveva preso. Chiamata sotto il lock del canale. */
static void cq_remove_sel(gmb_wait_entry_t** arr, int* cap, int* head, int* tail, int* count, gmb_select_ctx_t* s)
{
    int n = *count;
    for (int i = 0; i < n; i++)
    {
        gmb_wait_entry_t e = (*arr)[*head];
        *head = (*head + 1) % *cap;
        (*count)--;
        if (e.sel == s) { gmb_select_release(e.sel); continue; }
        /* riaccoda in coda */
        if (*count >= *cap)
        {
            int ncap = *cap ? *cap * 2 : 8;
            gmb_wait_entry_t* na = (gmb_wait_entry_t*)malloc(ncap * sizeof(gmb_wait_entry_t));
            int k = 0;
            for (int j = 0; j < *count; j++) na[k++] = (*arr)[(*head + j) % *cap];
            free(*arr);
            *arr = na; *cap = ncap; *head = 0; *tail = k;
        }
        (*arr)[*tail] = e;
        *tail = (*tail + 1) % *cap;
        (*count)++;
    }
}

/* Protocollo wake di una entry appena poppata (il caller rilascia il ref sel dopo).
 * Entry semplice → il task va svegliato. Entry SELECT → claim CAS: solo il winner viene
 * svegliato (il claim fa anche il detach dei loser); i loser ritornano NULL. */
static gmb_task_t* wake_task_for_entry(gmb_wait_entry_t e)
{
    if (!e.task) return NULL;
    if (!e.sel) return e.task;
    if (gmb_select_claim(e.sel, e.case_idx)) return e.task;
    return NULL;
}

int gmb_channel_is_closed(gmb_channel_t* c)
{
    pthread_mutex_lock(&c->lock);
    int r = c->closed;
    pthread_mutex_unlock(&c->lock);
    return r;
}

/* V5 (parità ChannelInfo.Close): chiusura atomica. I waiter vengono TUTTI estratti
 * sotto lo stesso lock e restituiti in array malloc (il chiamante li riaccoda allo
 * scheduler e libera gli array): receiver → receive ok=false; sender → errore
 * send-su-chiuso. Ritorna 1 se la chiusura è avvenuta ora, 0 se già chiuso
 * (double-close = errore, come il panic Go). */
int gmb_channel_close(gmb_channel_t* c, gmb_wait_entry_t** wake_recv, int* n_recv,
                      gmb_wait_entry_t** wake_send, int* n_send)
{
    *wake_recv = NULL; *wake_send = NULL;
    *n_recv = 0; *n_send = 0;
    pthread_mutex_lock(&c->lock);
    if (c->closed) { pthread_mutex_unlock(&c->lock); return 0; }
    c->closed = 1;
    if (c->recv_count > 0)
    {
        *wake_recv = (gmb_wait_entry_t*)malloc(c->recv_count * sizeof(gmb_wait_entry_t));
        gmb_wait_entry_t e;
        while (*wake_recv && (e = cq_pop(&c->recv_entries, &c->recv_cap, &c->recv_head,
                                         &c->recv_tail, &c->recv_count)).task != NULL)
            (*wake_recv)[(*n_recv)++] = e;
    }
    if (c->send_count > 0)
    {
        *wake_send = (gmb_wait_entry_t*)malloc(c->send_count * sizeof(gmb_wait_entry_t));
        gmb_wait_entry_t e;
        while (*wake_send && (e = cq_pop(&c->send_entries, &c->send_cap, &c->send_head,
                                         &c->send_tail, &c->send_count)).task != NULL)
            (*wake_send)[(*n_send)++] = e;
    }
    pthread_mutex_unlock(&c->lock);
    return 1;
}

int gmb_channel_send_or_park(gmb_channel_t* c, gmb_obj_t* value, double num, int is_num,
    gmb_task_t* sender, gmb_select_ctx_t* sel, int case_idx, gmb_task_t** woken_receiver)
{
    pthread_mutex_lock(&c->lock);
    if (c->closed)
    {
        /* Go: send su canale chiuso = panic (qui errore BASIC catturabile). */
        *woken_receiver = NULL;
        pthread_mutex_unlock(&c->lock);
        return -1;
    }
    if (c->buffer_count < c->capacity)
    {
        /* buffer con spazio: accoda entry completa. Il buffer RITIENE il ref: il
         * chiamante può rilasciare/troncare il proprio (prima il buffer prendeva in
         * prestito il ref del chiamante → use-after-free su truncate dello stack). */
        gmb_stack_entry_t e = { 0 };
        if (is_num) { e.num = num; e.type = 1; e.basic_type = GMB_BT_NUMBER; }
        else if (value) { e.ref = value; e.type = 2; e.basic_type = GMB_BT_STRING; gmb_obj_retain(value); }
        else { e.type = 2; e.basic_type = GMB_BT_NULL; }
        c->buffer[(c->buffer_head + c->buffer_count) % c->buffer_cap] = e;
        c->buffer_count++;
        gmb_wait_entry_t we = cq_pop(&c->recv_entries, &c->recv_cap, &c->recv_head, &c->recv_tail, &c->recv_count);
        pthread_mutex_unlock(&c->lock);
        /* V6: claim fuori dal lock del canale (ordine lock sicuro). */
        *woken_receiver = wake_task_for_entry(we);
        if (we.sel) gmb_select_release(we.sel);
        return 1;
    }
    /* parcheggia il mittente */
    sender->state = GMB_TS_BLOCKED;
    gmb_wait_entry_t pe = { sender, sel, case_idx };
    cq_push(&c->send_entries, &c->send_cap, &c->send_head, &c->send_tail, &c->send_count, pe);
    *woken_receiver = NULL;
    pthread_mutex_unlock(&c->lock);
    return 0;
}

bool gmb_channel_recv_or_park(gmb_channel_t* c, gmb_task_t* receiver, gmb_obj_t** value,
    double* num, int* is_num, int* closed_out, gmb_select_ctx_t* sel, int case_idx,
    gmb_task_t** woken_sender)
{
    if (closed_out) *closed_out = 0;
    pthread_mutex_lock(&c->lock);
    /* Canale chiuso e buffer vuoto: receive restituisce SUBITO ok=false (Go).
     * Se il buffer ha ancora elementi vengono serviti prima (qui sotto): i valori
     * buffered restano ricevibili dopo la chiusura, poi ok=false. */
    if (c->closed && c->buffer_count == 0)
    {
        *value = NULL; *num = 0; *is_num = 0;
        if (closed_out) *closed_out = 1;
        *woken_sender = NULL;
        pthread_mutex_unlock(&c->lock);
        return true;
    }
    if (c->buffer_count > 0)
    {
        gmb_stack_entry_t e = c->buffer[c->buffer_head];
        c->buffer_head = (c->buffer_head + 1) % c->buffer_cap;
        c->buffer_count--;
        if (e.type == 1) { *num = e.num; *is_num = 1; *value = NULL; }
        else { *value = e.ref; *is_num = 0; *num = 0; }
        gmb_wait_entry_t we = cq_pop(&c->send_entries, &c->send_cap, &c->send_head, &c->send_tail, &c->send_count);
        pthread_mutex_unlock(&c->lock);
        *woken_sender = wake_task_for_entry(we);
        if (we.sel) gmb_select_release(we.sel);
        return true;
    }
    receiver->state = GMB_TS_BLOCKED;
    gmb_wait_entry_t pe = { receiver, sel, case_idx };
    cq_push(&c->recv_entries, &c->recv_cap, &c->recv_head, &c->recv_tail, &c->recv_count, pe);
    *woken_sender = NULL;
    pthread_mutex_unlock(&c->lock);
    return false;
}

void gmb_channel_wake_one_sender(gmb_channel_t* c, gmb_task_t** out_sender)
{
    pthread_mutex_lock(&c->lock);
    gmb_wait_entry_t we = cq_pop(&c->send_entries, &c->send_cap, &c->send_head, &c->send_tail, &c->send_count);
    pthread_mutex_unlock(&c->lock);
    *out_sender = wake_task_for_entry(we);
    if (we.sel) gmb_select_release(we.sel);
}

bool gmb_channel_try_send(gmb_channel_t* c, gmb_obj_t* value, double num, int is_num,
    gmb_task_t** woken_receiver)
{
    pthread_mutex_lock(&c->lock);
    if (c->closed) { *woken_receiver = NULL; pthread_mutex_unlock(&c->lock); return false; }
    if (c->buffer_count < c->capacity)
    {
        /* Stesso protocollo di send_or_park: il buffer RITIENE il ref. */
        gmb_stack_entry_t e = { 0 };
        if (is_num) { e.num = num; e.type = 1; e.basic_type = GMB_BT_NUMBER; }
        else if (value) { e.ref = value; e.type = 2; e.basic_type = GMB_BT_STRING; gmb_obj_retain(value); }
        else { e.type = 2; e.basic_type = GMB_BT_NULL; }
        c->buffer[(c->buffer_head + c->buffer_count) % c->buffer_cap] = e;
        c->buffer_count++;
        gmb_wait_entry_t we = cq_pop(&c->recv_entries, &c->recv_cap, &c->recv_head, &c->recv_tail, &c->recv_count);
        pthread_mutex_unlock(&c->lock);
        *woken_receiver = wake_task_for_entry(we);
        if (we.sel) gmb_select_release(we.sel);
        return true;
    }
    *woken_receiver = NULL;
    pthread_mutex_unlock(&c->lock);
    return false;
}

bool gmb_channel_try_recv(gmb_channel_t* c, gmb_obj_t** value, double* num, int* is_num,
    gmb_task_t** woken_sender)
{
    pthread_mutex_lock(&c->lock);
    if (c->buffer_count > 0)
    {
        gmb_stack_entry_t e = c->buffer[c->buffer_head];
        c->buffer_head = (c->buffer_head + 1) % c->buffer_cap;
        c->buffer_count--;
        if (e.type == 1) { *num = e.num; *is_num = 1; *value = NULL; }
        else { *value = e.ref; *is_num = 0; *num = 0; }
        gmb_wait_entry_t we = cq_pop(&c->send_entries, &c->send_cap, &c->send_head, &c->send_tail, &c->send_count);
        pthread_mutex_unlock(&c->lock);
        *woken_sender = wake_task_for_entry(we);
        if (we.sel) gmb_select_release(we.sel);
        return true;
    }
    *woken_sender = NULL;
    pthread_mutex_unlock(&c->lock);
    return false;
}

/* ============================================================================
 * SelectContext (V6) — specchio di SelectContext in GpmChannels.cs
 * ============================================================================ */

gmb_select_ctx_t* gmb_select_create(gmb_task_t* task, int cap)
{
    gmb_select_ctx_t* s = (gmb_select_ctx_t*)calloc(1, sizeof(gmb_select_ctx_t));
    if (!s) return NULL;
    s->refcount = 1;               /* ref del task (rilasciato alla ri-esecuzione del SETUP) */
    s->winner = -1;
    s->task = task;
    if (cap < 1) cap = 1;
    s->cap = cap;
    s->chans = (gmb_channel_t**)calloc((size_t)cap, sizeof(gmb_channel_t*));
    s->cases = (int*)calloc((size_t)cap, sizeof(int));
    pthread_mutex_init(&s->lock, NULL);
    return s;
}

void gmb_select_retain(gmb_select_ctx_t* s)
{
    if (s) __atomic_add_fetch(&s->refcount, 1, __ATOMIC_SEQ_CST);
}

void gmb_select_release(gmb_select_ctx_t* s)
{
    if (!s) return;
    if (__atomic_sub_fetch(&s->refcount, 1, __ATOMIC_SEQ_CST) == 0)
    {
        free(s->chans);
        free(s->cases);
        pthread_mutex_destroy(&s->lock);
        free(s);
    }
}

void gmb_select_register(gmb_select_ctx_t* s, gmb_channel_t* ch, int case_idx)
{
    pthread_mutex_lock(&s->lock);
    if (s->count < s->cap)
    {
        s->chans[s->count] = ch;
        s->cases[s->count] = case_idx;
        s->count++;
    }
    pthread_mutex_unlock(&s->lock);
}

int gmb_select_pending(gmb_select_ctx_t* s)
{
    pthread_mutex_lock(&s->lock);
    int p = s->winner == -1;
    pthread_mutex_unlock(&s->lock);
    return p;
}

void gmb_select_detach(gmb_select_ctx_t* s)
{
    /* Snapshot sotto il lock del ctx, poi rimuove le entry canale per canale (lock dei
     * canali presi uno alla volta, MAI tenendo il lock del ctx — ordine lock sicuro). */
    pthread_mutex_lock(&s->lock);
    int n = s->count;
    gmb_channel_t** chans = (gmb_channel_t**)malloc((size_t)(n ? n : 1) * sizeof(gmb_channel_t*));
    if (chans) for (int i = 0; i < n; i++) chans[i] = s->chans[i];
    else n = 0;
    pthread_mutex_unlock(&s->lock);
    for (int i = 0; i < n; i++)
    {
        gmb_channel_t* c = chans[i];
        pthread_mutex_lock(&c->lock);
        cq_remove_sel(&c->recv_entries, &c->recv_cap, &c->recv_head, &c->recv_tail, &c->recv_count, s);
        cq_remove_sel(&c->send_entries, &c->send_cap, &c->send_head, &c->send_tail, &c->send_count, s);
        pthread_mutex_unlock(&c->lock);
    }
    free(chans);
}

int gmb_select_claim(gmb_select_ctx_t* s, int case_idx)
{
    pthread_mutex_lock(&s->lock);
    if (s->winner != -1) { pthread_mutex_unlock(&s->lock); return 0; }
    s->winner = case_idx;
    pthread_mutex_unlock(&s->lock);
    /* Il vincitore stacca tutte le iscrizioni (anche la propria, se ancora in coda). */
    gmb_select_detach(s);
    return 1;
}

/* ============================================================================
 * WaitGroup
 * ============================================================================ */

gmb_waitgroup_t* gmb_wg_create(void)
{
    gmb_waitgroup_t* w = (gmb_waitgroup_t*)calloc(1, sizeof(gmb_waitgroup_t));
    if (!w) return NULL;
    w->header.refcount = 1;
    w->header.kind = GMB_OBJ_WAITGROUP;
    w->header.release_fn = waitgroup_release_fn;
    pthread_mutex_init(&w->lock, NULL);
    return w;
}

void gmb_wg_destroy(gmb_waitgroup_t* w)
{
    if (!w) return;
    free(w->waiters);
    pthread_mutex_destroy(&w->lock);
    free(w);
}

void gmb_wg_add(gmb_waitgroup_t* w, int n)
{
    pthread_mutex_lock(&w->lock);
    w->counter += n;
    pthread_mutex_unlock(&w->lock);
}

bool gmb_wg_should_wait(gmb_waitgroup_t* w, gmb_task_t* t)
{
    pthread_mutex_lock(&w->lock);
    if (w->counter <= 0) { pthread_mutex_unlock(&w->lock); return false; }
    if (w->wcount >= w->wcap)
    {
        w->wcap = w->wcap ? w->wcap * 2 : 8;
        w->waiters = (gmb_task_t**)realloc(w->waiters, w->wcap * sizeof(gmb_task_t*));
    }
    w->waiters[w->wcount++] = t;
    t->state = GMB_TS_BLOCKED;
    pthread_mutex_unlock(&w->lock);
    return true;
}

void gmb_wg_done(gmb_waitgroup_t* w, gmb_scheduler_t* s)
{
    gmb_task_t** to_wake = NULL;
    int n = 0;
    pthread_mutex_lock(&w->lock);
    w->counter--;
    if (w->counter <= 0)
    {
        to_wake = (gmb_task_t**)malloc((w->wcount ? w->wcount : 1) * sizeof(gmb_task_t*));
        for (int i = 0; i < w->wcount; i++) to_wake[n++] = w->waiters[i];
        w->wcount = 0;
    }
    pthread_mutex_unlock(&w->lock);
    for (int i = 0; i < n; i++) gmb_sched_enqueue(s, to_wake[i]);
    free(to_wake);
}

/* ============================================================================
 * Mutex — specchio di PCodeMutex (parità Go sync.Mutex)
 * ============================================================================ */

gmb_mutex_t* gmb_mutex_create(void)
{
    gmb_mutex_t* m = (gmb_mutex_t*)calloc(1, sizeof(gmb_mutex_t));
    if (!m) return NULL;
    m->header.refcount = 1;
    m->header.kind = GMB_OBJ_MUTEX;
    m->header.release_fn = mutex_release_fn;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

void gmb_mutex_destroy(gmb_mutex_t* m)
{
    if (!m) return;
    free(m->waiters);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

bool gmb_mutex_try_lock(gmb_mutex_t* m, gmb_task_t* t)
{
    pthread_mutex_lock(&m->lock);
    if (!m->held)
    {
        m->held = 1;
        m->owner = t;
        pthread_mutex_unlock(&m->lock);
        return true;
    }
    /* HANDOFF (specchio PCodeMutex.TryLock): al risveglio il task ri-esegue OP_LOCK;
     * gmb_mutex_unlock gli ha già trasferito la proprietà → lo riconosciamo e gli
     * concediamo il lock SENZA ri-parcheggiarlo (altrimenti deadlock: held=true e
     * nessun detentore che possa sbloccare). */
    if (m->owner == t)
    {
        m->owner = NULL;
        pthread_mutex_unlock(&m->lock);
        return true;
    }
    if (m->wcount >= m->wcap)
    {
        m->wcap = m->wcap ? m->wcap * 2 : 8;
        m->waiters = (gmb_task_t**)realloc(m->waiters, m->wcap * sizeof(gmb_task_t*));
    }
    m->waiters[m->wcount++] = t;   /* coda FIFO come PCodeMutex._waiters */
    t->state = GMB_TS_BLOCKED;     /* contratto park §16: State=Blocked dentro lo stesso lock */
    pthread_mutex_unlock(&m->lock);
    return false;
}

void gmb_mutex_unlock(gmb_mutex_t* m, gmb_scheduler_t* s)
{
    gmb_task_t* to_wake = NULL;
    pthread_mutex_lock(&m->lock);
    m->held = 0;
    m->owner = NULL;
    if (m->wcount > 0)
    {
        /* Parità Go: Unlock passa la proprietà al waiter svegliato (handoff). */
        to_wake = m->waiters[0];
        memmove(&m->waiters[0], &m->waiters[1], (m->wcount - 1) * sizeof(gmb_task_t*));
        m->wcount--;
        m->held = 1;
        m->owner = to_wake;
    }
    pthread_mutex_unlock(&m->lock);
    if (to_wake) gmb_sched_enqueue(s, to_wake);
}

/* MUTEX.TRYLOCK (Go 1.18, specchio PCodeMutex.TryLockNonBlocking): acquisisce solo se
 * libero; MAI parcheggia e MAI sfrutta l'handoff (se held → false subito). */
bool gmb_mutex_try_lock_nb(gmb_mutex_t* m)
{
    pthread_mutex_lock(&m->lock);
    if (m->held) { pthread_mutex_unlock(&m->lock); return false; }
    m->held = 1;
    m->owner = NULL;
    pthread_mutex_unlock(&m->lock);
    return true;
}

/* ============================================================================
 * RwMutex — specchio di PCodeRwMutex (parità Go sync.RWMutex)
 * ============================================================================ */

gmb_rwlock_t* gmb_rwlock_create(void)
{
    gmb_rwlock_t* rw = (gmb_rwlock_t*)calloc(1, sizeof(gmb_rwlock_t));
    if (!rw) return NULL;
    rw->header.refcount = 1;
    rw->header.kind = GMB_OBJ_RWMUTEX;
    rw->header.release_fn = rwlock_release_fn;
    pthread_mutex_init(&rw->lock, NULL);
    return rw;
}

void gmb_rwlock_destroy(gmb_rwlock_t* rw)
{
    if (!rw) return;
    free(rw->waiters);
    pthread_mutex_destroy(&rw->lock);
    free(rw);
}

static void rwlock_enqueue(gmb_rwlock_t* rw, gmb_task_t* t, int is_writer)
{
    if (rw->wcount >= rw->wcap)
    {
        rw->wcap = rw->wcap ? rw->wcap * 2 : 8;
        rw->waiters = (gmb_rwlock_waiter_t*)realloc(rw->waiters, rw->wcap * sizeof(gmb_rwlock_waiter_t));
    }
    rw->waiters[rw->wcount].task = t;
    rw->waiters[rw->wcount].is_writer = is_writer;
    rw->wcount++;
    if (is_writer) rw->writer_waiting++;
    t->state = GMB_TS_BLOCKED;   /* contratto park §16: State=Blocked dentro lo stesso lock */
}

bool gmb_rwlock_try_lock(gmb_rwlock_t* rw, gmb_task_t* t)
{
    pthread_mutex_lock(&rw->lock);
    if (!rw->writer_active && rw->readers == 0 && rw->writer_waiting == 0)
    {
        rw->writer_active = 1;
        rw->writer_owner = t;
        pthread_mutex_unlock(&rw->lock);
        return true;
    }
    /* HANDOFF (specchio PCodeRwMutex.TryLock): l'handoff di unlock gli ha già
     * ri-assegnato il lock → riconosci il destinatario e concedi. */
    if (rw->writer_owner == t)
    {
        rw->writer_owner = NULL;
        pthread_mutex_unlock(&rw->lock);
        return true;
    }
    rwlock_enqueue(rw, t, 1);
    pthread_mutex_unlock(&rw->lock);
    return false;
}

/* Prende il primo waiter la cui condizione di ingresso è già soddisfatta (handoff):
 * un writer se readers==0; un reader se non c'è writer attivo né in attesa.
 * NOTA (fix vs C#): per il reader NON pre-incrementa readers — il reader svegliato
 * incrementa nella retry di gmb_rwlock_try_rlock; il pre-incremento C# contava il
 * lettore due volte (leak: RUNLOCK non porta mai readers a 0 → writer/reader affamati). */
static gmb_task_t* rwlock_take_next_compatible(gmb_rwlock_t* rw)
{
    if (rw->wcount == 0) return NULL;
    gmb_rwlock_waiter_t cand = rw->waiters[0];
    if (cand.is_writer)
    {
        if (rw->readers == 0 && !rw->writer_active)
        {
            memmove(&rw->waiters[0], &rw->waiters[1], (rw->wcount - 1) * sizeof(gmb_rwlock_waiter_t));
            rw->wcount--;
            rw->writer_waiting--;
            rw->writer_active = 1;
            rw->writer_owner = cand.task;
            return cand.task;
        }
        return NULL; /* writer in testa non ancora acquisibile: non svegliare reader dietro */
    }
    if (rw->writer_active || rw->writer_waiting > 0)
        return NULL; /* reader in testa bloccato dalla writer-preference */
    memmove(&rw->waiters[0], &rw->waiters[1], (rw->wcount - 1) * sizeof(gmb_rwlock_waiter_t));
    rw->wcount--;
    return cand.task;   /* sveglia 1 alla volta, come PCodeRwMutex */
}

void gmb_rwlock_unlock(gmb_rwlock_t* rw, gmb_scheduler_t* s)
{
    pthread_mutex_lock(&rw->lock);
    rw->writer_active = 0;
    rw->writer_owner = NULL;
    gmb_task_t* to_wake = rwlock_take_next_compatible(rw);
    pthread_mutex_unlock(&rw->lock);
    if (to_wake) gmb_sched_enqueue(s, to_wake);
}

bool gmb_rwlock_try_rlock(gmb_rwlock_t* rw, gmb_task_t* t)
{
    pthread_mutex_lock(&rw->lock);
    if (!rw->writer_active && rw->writer_waiting == 0)
    {
        rw->readers++;
        pthread_mutex_unlock(&rw->lock);
        return true;
    }
    rwlock_enqueue(rw, t, 0);
    pthread_mutex_unlock(&rw->lock);
    return false;
}

void gmb_rwlock_runlock(gmb_rwlock_t* rw, gmb_scheduler_t* s)
{
    pthread_mutex_lock(&rw->lock);
    if (rw->readers > 0) rw->readers--;
    gmb_task_t* to_wake = (rw->readers == 0) ? rwlock_take_next_compatible(rw) : NULL;
    pthread_mutex_unlock(&rw->lock);
    if (to_wake) gmb_sched_enqueue(s, to_wake);
}

/* MUTEX.TRYLOCK su RWMUTEX (scrittore, Go 1.18 — specchio
 * PCodeRwMutex.TryLockNonBlocking): solo se nessun lettore attivo, nessuno scrittore
 * attivo e nessuno scrittore in coda (writer-preference). MAI parcheggia. */
bool gmb_rwlock_try_lock_nb(gmb_rwlock_t* rw)
{
    pthread_mutex_lock(&rw->lock);
    if (!rw->writer_active && rw->readers == 0 && rw->writer_waiting == 0)
    {
        rw->writer_active = 1;
        rw->writer_owner = NULL;
        pthread_mutex_unlock(&rw->lock);
        return true;
    }
    pthread_mutex_unlock(&rw->lock);
    return false;
}

/* ============================================================================
 * Once — specchio di PCodeOnce (parità Go sync.Once)
 * ============================================================================ */

gmb_once_t* gmb_once_create(void)
{
    gmb_once_t* o = (gmb_once_t*)calloc(1, sizeof(gmb_once_t));
    if (!o) return NULL;
    o->header.refcount = 1;
    o->header.kind = GMB_OBJ_ONCE;
    o->header.release_fn = once_release_fn;
    pthread_mutex_init(&o->lock, NULL);
    return o;
}

void gmb_once_destroy(gmb_once_t* o)
{
    if (!o) return;
    free(o->waiters);
    pthread_mutex_destroy(&o->lock);
    free(o);
}

int gmb_once_try_begin(gmb_once_t* o, gmb_task_t* t)
{
    pthread_mutex_lock(&o->lock);
    if (o->state == GMB_ONCE_NOT_RUN)
    {
        o->state = GMB_ONCE_RUNNING;
        pthread_mutex_unlock(&o->lock);
        return GMB_ONCE_RUNNING;   /* Started: esegui la procedura */
    }
    if (o->state == GMB_ONCE_DONE)
    {
        pthread_mutex_unlock(&o->lock);
        return GMB_ONCE_DONE;      /* già eseguito: salta */
    }
    if (o->wcount >= o->wcap)
    {
        o->wcap = o->wcap ? o->wcap * 2 : 8;
        o->waiters = (gmb_task_t**)realloc(o->waiters, o->wcap * sizeof(gmb_task_t*));
    }
    o->waiters[o->wcount++] = t;
    t->state = GMB_TS_BLOCKED;     /* contratto park §16 */
    pthread_mutex_unlock(&o->lock);
    return GMB_ONCE_NOT_RUN;       /* Parked */
}

void gmb_once_complete(gmb_once_t* o, gmb_scheduler_t* s)
{
    gmb_task_t** to_wake = NULL;
    int n = 0;
    pthread_mutex_lock(&o->lock);
    o->state = GMB_ONCE_DONE;
    if (o->wcount > 0)
    {
        n = o->wcount;
        to_wake = o->waiters;
        o->waiters = NULL;
        o->wcap = 0;
        o->wcount = 0;
    }
    pthread_mutex_unlock(&o->lock);
    if (to_wake)
    {
        for (int i = 0; i < n; i++) gmb_sched_enqueue(s, to_wake[i]);
        free(to_wake);
    }
}

/* ============================================================================
 * Future
 * ============================================================================ */

gmb_future_t* gmb_future_create(void)
{
    gmb_future_t* f = (gmb_future_t*)calloc(1, sizeof(gmb_future_t));
    if (!f) return NULL;
    f->header.refcount = 1;
    f->header.kind = GMB_OBJ_FUTURE;
    f->header.release_fn = future_release_fn;
    pthread_mutex_init(&f->lock, NULL);
    return f;
}

void gmb_future_destroy(gmb_future_t* f)
{
    if (!f) return;
    free(f->waiters);
    pthread_mutex_destroy(&f->lock);
    free(f);
}

bool gmb_future_register_waiter(gmb_future_t* f, gmb_task_t* t)
{
    pthread_mutex_lock(&f->lock);
    if (f->is_resolved) { pthread_mutex_unlock(&f->lock); return false; }
    if (f->wcount >= f->wcap)
    {
        f->wcap = f->wcap ? f->wcap * 2 : 8;
        f->waiters = (gmb_task_t**)realloc(f->waiters, f->wcap * sizeof(gmb_task_t*));
    }
    f->waiters[f->wcount++] = t;
    t->state = GMB_TS_BLOCKED;
    pthread_mutex_unlock(&f->lock);
    return true;
}

void gmb_future_resolve(gmb_future_t* f, gmb_obj_t* value, double num, int is_num, gmb_scheduler_t* s)
{
    gmb_task_t** to_wake = NULL;
    int n = 0;
    pthread_mutex_lock(&f->lock);
    if (f->is_resolved) { pthread_mutex_unlock(&f->lock); return; }
    f->is_resolved = 1;
    f->result = value; f->result_num = num; f->result_is_num = is_num;
    if (f->wcount > 0)
    {
        to_wake = (gmb_task_t**)malloc(f->wcount * sizeof(gmb_task_t*));
        for (int i = 0; i < f->wcount; i++) to_wake[n++] = f->waiters[i];
        f->wcount = 0;
    }
    pthread_mutex_unlock(&f->lock);
    for (int i = 0; i < n; i++) gmb_sched_enqueue(s, to_wake[i]);
    free(to_wake);
}

int gmb_future_is_resolved(gmb_future_t* f)
{
    pthread_mutex_lock(&f->lock);
    int r = f->is_resolved;
    pthread_mutex_unlock(&f->lock);
    return r;
}

/* V4g: specchio PCodeFutureValue.Fail — la goroutine è terminata con errore; chi
 * legge il future (implicit await) deve vedersi propagare l'errore, non un null. */
void gmb_future_fail(gmb_future_t* f, const char* msg, gmb_scheduler_t* s)
{
    gmb_task_t** to_wake = NULL;
    int n = 0;
    pthread_mutex_lock(&f->lock);
    if (f->is_resolved) { pthread_mutex_unlock(&f->lock); return; }
    f->is_resolved = 1;
    f->is_failed = 1;
    snprintf(f->fail_msg, sizeof(f->fail_msg), "%s", msg ? msg : "unknown error");
    if (f->wcount > 0)
    {
        to_wake = (gmb_task_t**)malloc(f->wcount * sizeof(gmb_task_t*));
        for (int i = 0; i < f->wcount; i++) to_wake[n++] = f->waiters[i];
        f->wcount = 0;
    }
    pthread_mutex_unlock(&f->lock);
    for (int i = 0; i < n; i++) gmb_sched_enqueue(s, to_wake[i]);
    free(to_wake);
}

void gmb_sched_destroy(gmb_scheduler_t* s)
{
    if (!s) return;
    for (int i = 0; i < s->all_count; i++) gmb_task_free(s->all_tasks[i]);
    free(s->all_tasks);
    free(s->queue);
    free(s->timers);
    pthread_mutex_destroy(&s->qlock);
    pthread_cond_destroy(&s->qcond);
    pthread_mutex_destroy(&s->all_lock);
    pthread_mutex_destroy(&s->clock);
    pthread_cond_destroy(&s->ccond);
    pthread_mutex_destroy(&s->tlock);
    pthread_cond_destroy(&s->tcond);
    free(s);
}

void gmb_task_free(gmb_task_t* t)
{
    if (!t) return;
    /* Rimuove il task dalla lista all_tasks dello scheduler: un task può essere liberato
     * PRIMA dello shutdown (es. il route task HTTP, gmb_task_free in gmb_vm_http_handler)
     * ma resta in all_tasks → gmb_sched_destroy lo liberava di nuovo (double-free → abort). */
    if (t->scheduler)
    {
        pthread_mutex_lock(&t->scheduler->all_lock);
        for (int i = 0; i < t->scheduler->all_count; i++)
            if (t->scheduler->all_tasks[i] == t)
            {
                t->scheduler->all_tasks[i] = t->scheduler->all_tasks[t->scheduler->all_count - 1];
                t->scheduler->all_count--;
                break;
            }
        pthread_mutex_unlock(&t->scheduler->all_lock);
    }
    /* §16: job DNS in volo (task liberato mentre parcheggiato sulla pipe): lo
     * abbandona — lo slot del pool viene liberato dal worker al completamento. */
    if (t->net_dns_job) { gmb_net_dns_cancel(t->net_dns_job); t->net_dns_job = 0; }
    while (t->frame) { gmb_frame_t* f = t->frame; t->frame = f->prev; gmb_frame_release(f); }
    /* Stato FOR del task (usato solo senza frame attivo: top-level/__main e goroutine
     * radice GO SUB): mappa sparsa, va liberata esplicitamente. */
    gmb_for_clear(&t->for_state);
    /* V1: lo stack operandi è heap — rilascia i ref residui (prima erano leak). */
    for (int i = 0; i < t->sp; i++)
        if (t->stack[i].type == 2 && t->stack[i].ref) gmb_obj_release(t->stack[i].ref);
    free(t->stack);
    /* V4: voci TRY refcountate condivise fra try_stack e cleanup_stack. */
    for (int i = 0; i < t->try_count; i++) gmb_try_entry_release(t->try_stack[i]);
    for (int i = 0; i < t->cleanup_count; i++) gmb_try_entry_release(t->cleanup_stack[i]);
    if (t->unwind_active)
    {
        if (t->unwind.has_error_value &&
            t->unwind.error_value.type == 2 && t->unwind.error_value.ref)
            gmb_obj_release(t->unwind.error_value.ref);
        gmb_try_entry_release(t->unwind.target_handler);
    }
    /* V6: ref del task al SelectContext di un GO SELECT ancora parcheggiato. */
    if (t->select_ctx) { gmb_select_release(t->select_ctx); t->select_ctx = NULL; }
    /* V6d: ref del task al Once iniziato (OP_ONCE_BEGIN senza OP_ONCE_END raggiunto). */
    if (t->pending_once) { gmb_obj_release((gmb_obj_t*)t->pending_once); t->pending_once = NULL; }
    /* SCOPE DINAMICO: fallback top-level (vedi gmb_gpm.h gmb_task_t.dyn_base). */
    if (t->dyn_base) { gmb_dyn_scope_clear(t->dyn_base); free(t->dyn_base); t->dyn_base = NULL; }
    /* Gruppo implicito dei figli senza IN (WAIT nuda, bug 2026-10-01): rilascia il ref
     * del genitore; i figli trattengono il proprio (release al loro finish). */
    if (t->implicit_waitgroup) { gmb_obj_release((gmb_obj_t*)t->implicit_waitgroup); t->implicit_waitgroup = NULL; }
    for (int i = 0; i < t->locals_cap; i++)
        if (t->locals_types[i] == 2 && t->locals_refs[i]) gmb_obj_release(t->locals_refs[i]);
    free(t->locals_nums); free(t->locals_refs); free(t->locals_types); free(t->locals_basic);
    if (t->globals_nums && t->owns_globals)
    {
        for (uint32_t i = 0; i < t->globals_count; i++)
            if (t->globals_types[i] == 2 && t->globals_refs[i]) gmb_obj_release(t->globals_refs[i]);
        free(t->globals_nums);
        free(t->globals_refs);
        free(t->globals_types);
        free(t->globals_basic);
    }
    pthread_mutex_destroy(&t->lock);
    free(t);
}

/* ============================================================================
 * §25 — Atomic (specchio PCodeAtomic, parità Go sync/atomic)
 * ============================================================================ */

gmb_atomic_t* gmb_atomic_create(void)
{
    gmb_atomic_t* a = (gmb_atomic_t*)calloc(1, sizeof(gmb_atomic_t));
    if (!a) return NULL;
    a->header.refcount = 1;
    a->header.kind = GMB_OBJ_ATOMIC;
    a->header.release_fn = atomic_release_fn;
    return a;
}

void gmb_atomic_destroy(gmb_atomic_t* a)
{
    free(a);
}

long long gmb_atomic_add(gmb_atomic_t* a, long long delta)
{
    /* Parità Go atomic.AddInt64: ritorna il NUOVO valore (IL/PCode/C99 concordano).
     * __atomic_fetch_add ritorna il VECCHIO → sommare delta. */
    return __atomic_fetch_add(&a->value, delta, __ATOMIC_SEQ_CST) + delta;
}

long long gmb_atomic_load(gmb_atomic_t* a)
{
    return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST);
}

void gmb_atomic_store(gmb_atomic_t* a, long long v)
{
    __atomic_store_n(&a->value, v, __ATOMIC_SEQ_CST);
}

int gmb_atomic_cas(gmb_atomic_t* a, long long expected, long long desired)
{
    return __atomic_compare_exchange_n(&a->value, &expected, desired, 0,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ? 1 : 0;
}

long long gmb_atomic_swap(gmb_atomic_t* a, long long v)
{
    return __atomic_exchange_n(&a->value, v, __ATOMIC_SEQ_CST);
}

/* ============================================================================
 * §25 — Pool (specchio PCodePool, parità Go sync.Pool)
 * ============================================================================ */

gmb_pool_t* gmb_pool_create(void)
{
    gmb_pool_t* p = (gmb_pool_t*)calloc(1, sizeof(gmb_pool_t));
    if (!p) return NULL;
    p->header.refcount = 1;
    p->header.kind = GMB_OBJ_POOL;
    p->header.release_fn = pool_release_fn;
    pthread_mutex_init(&p->lock, NULL);
    return p;
}

void gmb_pool_destroy(gmb_pool_t* p)
{
    if (!p) return;
    for (int i = 0; i < p->count; i++)
    {
        gmb_stack_entry_t e = p->items[(p->head + i) % p->cap];
        if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
    }
    free(p->items);
    pthread_mutex_destroy(&p->lock);
    free(p);
}

void gmb_pool_put(gmb_pool_t* p, gmb_stack_entry_t e)
{
    pthread_mutex_lock(&p->lock);
    if (p->count >= p->cap)
    {
        int ncap = p->cap ? p->cap * 2 : 8;
        gmb_stack_entry_t* ni = (gmb_stack_entry_t*)malloc((size_t)ncap * sizeof(gmb_stack_entry_t));
        if (ni)
        {
            for (int i = 0; i < p->count; i++) ni[i] = p->items[(p->head + i) % p->cap];
            free(p->items);
            p->items = ni;
            p->cap = ncap;
            p->head = 0;
        }
    }
    if (p->count < p->cap)
    {
        p->items[(p->head + p->count) % p->cap] = e;   /* move: il retain resta */
        p->count++;
    }
    else if (e.type == 2 && e.ref) gmb_obj_release(e.ref); /* OOM: scarta */
    pthread_mutex_unlock(&p->lock);
}

gmb_stack_entry_t gmb_pool_get(gmb_pool_t* p)
{
    gmb_stack_entry_t e = { 0 };
    pthread_mutex_lock(&p->lock);
    if (p->count > 0)
    {
        e = p->items[p->head];
        p->items[p->head].type = 0;
        p->items[p->head].ref = NULL;
        p->head = (p->head + 1) % p->cap;
        p->count--;
    }
    pthread_mutex_unlock(&p->lock);
    return e;   /* {0} = vuoto → NULL (Go: Pool senza New) */
}

/* ============================================================================
 * §25 — Cond (specchio PCodeCond, parità Go sync.Cond)
 * ============================================================================ */

gmb_cond_t* gmb_cond_create(void)
{
    gmb_cond_t* c = (gmb_cond_t*)calloc(1, sizeof(gmb_cond_t));
    if (!c) return NULL;
    c->header.refcount = 1;
    c->header.kind = GMB_OBJ_COND;
    c->header.release_fn = cond_release_fn;
    pthread_mutex_init(&c->lock, NULL);
    return c;
}

void gmb_cond_destroy(gmb_cond_t* c)
{
    if (!c) return;
    free(c->waiters);
    pthread_mutex_destroy(&c->lock);
    free(c);
}

void gmb_cond_wait(gmb_cond_t* c, gmb_obj_t* mutex_obj, gmb_task_t* t, gmb_scheduler_t* s)
{
    /* Parità Go c.Wait(): rilascia il mutex (MUTEX o RWMUTEX, come PCodeCond) e
     * parcheggia. NON riavvolge l'IP: il compiler emette OP_LOCK dopo OP_COND_WAIT. */
    if (mutex_obj)
    {
        if (mutex_obj->kind == GMB_OBJ_MUTEX)
            gmb_mutex_unlock((gmb_mutex_t*)mutex_obj, s);
        else if (mutex_obj->kind == GMB_OBJ_RWMUTEX)
            gmb_rwlock_unlock((gmb_rwlock_t*)mutex_obj, s);
    }
    pthread_mutex_lock(&c->lock);
    if (c->wcount >= c->wcap)
    {
        c->wcap = c->wcap ? c->wcap * 2 : 8;
        c->waiters = (gmb_task_t**)realloc(c->waiters, c->wcap * sizeof(gmb_task_t*));
    }
    c->waiters[c->wcount++] = t;
    t->state = GMB_TS_BLOCKED;   /* contratto park §16 */
    pthread_mutex_unlock(&c->lock);
}

void gmb_cond_signal(gmb_cond_t* c, gmb_scheduler_t* s)
{
    gmb_task_t* to_wake = NULL;
    pthread_mutex_lock(&c->lock);
    if (c->wcount > 0)
    {
        to_wake = c->waiters[0];
        memmove(&c->waiters[0], &c->waiters[1], (c->wcount - 1) * sizeof(gmb_task_t*));
        c->wcount--;
    }
    pthread_mutex_unlock(&c->lock);
    if (to_wake) gmb_sched_enqueue(s, to_wake);
}

void gmb_cond_broadcast(gmb_cond_t* c, gmb_scheduler_t* s)
{
    gmb_task_t** to_wake = NULL;
    int n = 0;
    pthread_mutex_lock(&c->lock);
    if (c->wcount > 0)
    {
        n = c->wcount;
        to_wake = c->waiters;
        c->waiters = NULL;
        c->wcap = 0;
        c->wcount = 0;
    }
    pthread_mutex_unlock(&c->lock);
    if (to_wake)
    {
        for (int i = 0; i < n; i++) gmb_sched_enqueue(s, to_wake[i]);
        free(to_wake);
    }
}

/* ============================================================================
 * §25 — Context (specchio PCodeContext, parità Go context.Context)
 * ============================================================================ */

/* Close del canale DONE + wake dei waiter (stesso pattern di OP_CHANNEL_CLOSE:
 * entry di SELECT passano dal claim CAS del SelectContext). */
static void context_close_done_and_wake(gmb_channel_t* ch, gmb_scheduler_t* s)
{
    gmb_wait_entry_t* wake_recv = NULL; gmb_wait_entry_t* wake_send = NULL;
    int n_recv = 0, n_send = 0;
    if (!gmb_channel_close(ch, &wake_recv, &n_recv, &wake_send, &n_send))
        return;   /* già chiuso (idempotente come Close() in C#) */
    for (int i = 0; i < n_recv; i++)
    {
        gmb_wait_entry_t e = wake_recv[i];
        if (!e.sel) gmb_sched_enqueue(s, e.task);
        else
        {
            if (gmb_select_claim(e.sel, e.case_idx)) gmb_sched_enqueue(s, e.task);
            gmb_select_release(e.sel);
        }
    }
    for (int i = 0; i < n_send; i++)
    {
        gmb_wait_entry_t e = wake_send[i];
        if (!e.sel) gmb_sched_enqueue(s, e.task); /* ri-esegue SEND → errore su chiuso */
        else
        {
            if (gmb_select_claim(e.sel, e.case_idx)) gmb_sched_enqueue(s, e.task);
            gmb_select_release(e.sel);
        }
    }
    free(wake_recv);
    free(wake_send);
}

static void context_cancel_inner(gmb_context_t* c, gmb_scheduler_t* s)
{
    gmb_channel_t* to_close = NULL;
    gmb_context_t** to_propagate = NULL;
    int n_children = 0;
    pthread_mutex_lock(&c->lock);
    if (c->cancelled) { pthread_mutex_unlock(&c->lock); return; }
    c->cancelled = 1;
    to_close = c->done_channel;
    if (c->ccount > 0)
    {
        /* Snapshot con RETAIN: dopo lo unlock un figlio potrebbe essere rilasciato
         * dal BASIC; il ref extra lo tiene vivo durante la propagazione. */
        n_children = c->ccount;
        to_propagate = c->children;
        c->children = NULL;
        c->ccap = 0;
        c->ccount = 0;
        for (int i = 0; i < n_children; i++)
            gmb_obj_retain((gmb_obj_t*)to_propagate[i]);
    }
    pthread_mutex_unlock(&c->lock);
    if (to_close) context_close_done_and_wake(to_close, s);
    for (int i = 0; i < n_children; i++)
    {
        context_cancel_inner(to_propagate[i], s);
        gmb_obj_release((gmb_obj_t*)to_propagate[i]);   /* ref del children-list del parent */
        gmb_obj_release((gmb_obj_t*)to_propagate[i]);   /* ref extra dello snapshot */
    }
    free(to_propagate);
}

gmb_context_t* gmb_context_create(gmb_context_t* parent)
{
    gmb_context_t* c = (gmb_context_t*)calloc(1, sizeof(gmb_context_t));
    if (!c) return NULL;
    c->header.refcount = 1;
    c->header.kind = GMB_OBJ_CONTEXT;
    c->header.release_fn = context_release_fn;
    pthread_mutex_init(&c->lock, NULL);
    c->parent = parent;
    if (parent)
    {
        int parent_cancelled = 0;
        pthread_mutex_lock(&parent->lock);
        if (parent->cancelled) parent_cancelled = 1;
        else
        {
            if (parent->ccount >= parent->ccap)
            {
                parent->ccap = parent->ccap ? parent->ccap * 2 : 4;
                parent->children = (gmb_context_t**)realloc(parent->children,
                    parent->ccap * sizeof(gmb_context_t*));
            }
            if (parent->children)
            {
                parent->children[parent->ccount++] = c;
                gmb_obj_retain((gmb_obj_t*)c);   /* il children-list possiede un ref */
            }
        }
        pthread_mutex_unlock(&parent->lock);
        /* Parità Go: un figlio di un parent già cancellato nasce cancellato. */
        if (parent_cancelled) c->cancelled = 1;
    }
    return c;
}

void gmb_context_destroy(gmb_context_t* c)
{
    if (!c) return;
    if (c->parent)
    {
        gmb_context_t* p = c->parent;
        pthread_mutex_lock(&p->lock);
        for (int i = 0; i < p->ccount; i++)
        {
            if (p->children[i] == c)
            {
                memmove(&p->children[i], &p->children[i + 1],
                        (p->ccount - i - 1) * sizeof(gmb_context_t*));
                p->ccount--;
                break;
            }
        }
        pthread_mutex_unlock(&p->lock);
    }
    for (int i = 0; i < c->ccount; i++)
        gmb_obj_release((gmb_obj_t*)c->children[i]);
    free(c->children);
    if (c->done_channel) gmb_obj_release((gmb_obj_t*)c->done_channel);
    pthread_mutex_destroy(&c->lock);
    free(c);
}

void gmb_context_cancel(gmb_context_t* c, gmb_scheduler_t* s)
{
    context_cancel_inner(c, s);
}

int gmb_context_is_cancelled(gmb_context_t* c)
{
    pthread_mutex_lock(&c->lock);
    int v = c->cancelled;
    pthread_mutex_unlock(&c->lock);
    return v;
}

gmb_channel_t* gmb_context_done(gmb_context_t* c, gmb_wait_entry_t** wake_recv, int* n_recv,
                                gmb_wait_entry_t** wake_send, int* n_send)
{
    *wake_recv = NULL; *wake_send = NULL; *n_recv = 0; *n_send = 0;
    pthread_mutex_lock(&c->lock);
    if (!c->done_channel) c->done_channel = gmb_channel_create(0);
    gmb_channel_t* ch = c->done_channel;
    if (ch) gmb_obj_retain((gmb_obj_t*)ch);   /* ref restituito al chiamante */
    int cancelled = c->cancelled;
    pthread_mutex_unlock(&c->lock);
    /* Parità Go: ctx.Done() su ctx già cancellato è già chiuso. Close idempotente. */
    if (ch && cancelled)
        gmb_channel_close(ch, wake_recv, n_recv, wake_send, n_send);
    return ch;
}

/* ============================================================================
 * §21 parità Go sync.Map — ConcurrentMap (specchio PCodeConcurrentMap)
 * ============================================================================ */

static int cmap_key_eq(gmb_stack_entry_t a, gmb_stack_entry_t b)
{
    if (a.type != b.type) return 0;
    if (a.type == 1) return a.num == b.num;
    if (a.type == 2 && a.ref && b.ref &&
        a.ref->kind == GMB_OBJ_STRING && b.ref->kind == GMB_OBJ_STRING)
        return strcmp(((gmb_obj_string_t*)a.ref)->data, ((gmb_obj_string_t*)b.ref)->data) == 0;
    return 0;
}

static gmb_stack_entry_t cmap_copy_with_retain(gmb_stack_entry_t e)
{
    if (e.type == 2 && e.ref) gmb_obj_retain(e.ref);
    return e;
}

gmb_concurrentmap_t* gmb_concurrentmap_create(void)
{
    gmb_concurrentmap_t* m = (gmb_concurrentmap_t*)calloc(1, sizeof(gmb_concurrentmap_t));
    if (!m) return NULL;
    m->header.refcount = 1;
    m->header.kind = GMB_OBJ_CONCURRENTMAP;
    m->header.release_fn = concurrentmap_release_fn;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

void gmb_concurrentmap_destroy(gmb_concurrentmap_t* m)
{
    if (!m) return;
    for (int i = 0; i < m->count; i++)
    {
        if (m->items[i].key.type == 2 && m->items[i].key.ref) gmb_obj_release(m->items[i].key.ref);
        if (m->items[i].val.type == 2 && m->items[i].val.ref) gmb_obj_release(m->items[i].val.ref);
    }
    free(m->items);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

void gmb_concurrentmap_store(gmb_concurrentmap_t* m, gmb_stack_entry_t key, gmb_stack_entry_t val)
{
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < m->count; i++)
    {
        if (cmap_key_eq(m->items[i].key, key))
        {
            /* chiave esistente: sostituisci il valore (il vecchio viene rilasciato);
             * la entry chiave del chiamante NON viene adottata → rilasciala. */
            if (m->items[i].val.type == 2 && m->items[i].val.ref) gmb_obj_release(m->items[i].val.ref);
            m->items[i].val = val;                       /* adotta il retain del nuovo */
            if (key.type == 2 && key.ref) gmb_obj_release(key.ref);
            pthread_mutex_unlock(&m->lock);
            return;
        }
    }
    if (m->count >= m->cap)
    {
        int ncap = m->cap ? m->cap * 2 : 8;
        m->items = (gmb_cmap_entry_t*)realloc(m->items, (size_t)ncap * sizeof(gmb_cmap_entry_t));
        m->cap = ncap;
    }
    m->items[m->count].key = key;                        /* adotta */
    m->items[m->count].val = val;                        /* adotta */
    m->count++;
    pthread_mutex_unlock(&m->lock);
}

gmb_stack_entry_t gmb_concurrentmap_load(gmb_concurrentmap_t* m, gmb_stack_entry_t key)
{
    gmb_stack_entry_t out = {0};
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < m->count; i++)
        if (cmap_key_eq(m->items[i].key, key)) { out = cmap_copy_with_retain(m->items[i].val); break; }
    pthread_mutex_unlock(&m->lock);
    return out;
}

gmb_stack_entry_t gmb_concurrentmap_delete(gmb_concurrentmap_t* m, gmb_stack_entry_t key)
{
    gmb_stack_entry_t out = {0};
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < m->count; i++)
    {
        if (cmap_key_eq(m->items[i].key, key))
        {
            out = m->items[i].val;                       /* move: nessun retain extra */
            if (m->items[i].key.type == 2 && m->items[i].key.ref) gmb_obj_release(m->items[i].key.ref);
            memmove(&m->items[i], &m->items[i + 1], (size_t)(m->count - i - 1) * sizeof(gmb_cmap_entry_t));
            m->count--;
            break;
        }
    }
    pthread_mutex_unlock(&m->lock);
    return out;
}

gmb_stack_entry_t gmb_concurrentmap_load_or_store(gmb_concurrentmap_t* m, gmb_stack_entry_t key,
                                                  gmb_stack_entry_t val)
{
    gmb_stack_entry_t out = {0};
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < m->count; i++)
    {
        if (cmap_key_eq(m->items[i].key, key))
        {
            /* già presente: ritorna il valore esistente; gli argomenti del chiamante
             * (chiave e valore proposti) NON sono adottati → rilasciali. */
            out = cmap_copy_with_retain(m->items[i].val);
            if (key.type == 2 && key.ref) gmb_obj_release(key.ref);
            if (val.type == 2 && val.ref) gmb_obj_release(val.ref);
            pthread_mutex_unlock(&m->lock);
            return out;
        }
    }
    if (m->count >= m->cap)
    {
        int ncap = m->cap ? m->cap * 2 : 8;
        m->items = (gmb_cmap_entry_t*)realloc(m->items, (size_t)ncap * sizeof(gmb_cmap_entry_t));
        m->cap = ncap;
    }
    m->items[m->count].key = key;                        /* adotta */
    m->items[m->count].val = val;                        /* adotta */
    m->count++;
    out = cmap_copy_with_retain(val);                    /* copia con retain per il chiamante */
    pthread_mutex_unlock(&m->lock);
    return out;
}
