/* ============================================================================
 * gmb_list.c — liste dinamiche (collezioni LIST) e boxing lista/bytes.
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"
#include "gmb_memstats.h"
#include <math.h>

/* Slice di una lista: NUOVA lista con gli elementi [start, min(end,count)) — canone
 * vm_sys_array_slice (VM/C) / ArraySlice (IL/PCode). Range invalido → copia della sorgente
 * (parità PCodeVM). Usata dagli emitter C99 per `a[start:end]` (ArraySliceExpr). */
gmb_list_t gmb_list_slice(gmb_list_t src, gmb_int_t start, gmb_int_t end) {
    gmb_list_t out = gmb_list_new();
    gmb_int_t cnt = gmb_list_count(src);
    if (start < 0 || end < start) { start = 0; end = cnt; }
    for (gmb_int_t i = start; i < end && i < cnt; i++) gmb_list_add(&out, gmb_list_get(src, i));
    return out;
}

gmb_list_t gmb_list_new(void) {
    gmb_list_t l;
    l.items = NULL;
    l.count = 0;
    l.capacity = 0;
    return l;
}

/* OWNERSHIP (vedi gmb_val_retain/release in gmb_runtime.h): la lista POSSIEDE i valori OBJECT contati
 * (release_fn != NULL). add/set/insert li trattengono; free/clear/remove_at/resize/sostituzione li
 * rilasciano. pop/dequeue NON rilasciano: l'elemento esce dalla lista e il riferimento passa al
 * chiamante (che lo tratta come in prestito → al peggio un leak, mai un use-after-free; rilasciarlo
 * qui romperebbe chi lo reinserisce subito in un altro contenitore). Valori in prestito
 * (release_fn NULL) e non-OBJECT: mai toccati. */
void gmb_list_free(gmb_list_t* l) {
    if (!l) return;
    for (size_t i = 0; i < l->count; i++) gmb_val_release(l->items[i]);
    if (l->items) { if (gmb_mem_on()) gmb_mem_untrack(l->items); free(l->items); l->items = NULL; l->count = 0; l->capacity = 0; }
}

gmb_int_t gmb_list_count(gmb_list_t l) { return (gmb_int_t)l.count; }

void gmb_list_add(gmb_list_t* l, gmb_val_t v) {
    if (!l) return;
    if (l->count >= l->capacity) {
        size_t nc = l->capacity == 0 ? 4 : l->capacity * 2;
        gmb_val_t* ni = (gmb_val_t*)realloc(l->items, nc * sizeof(gmb_val_t));
        if (!ni) return;
        if (gmb_mem_on()) gmb_mem_resize(l->items, ni, nc * sizeof(gmb_val_t), GMB_MEM_LIST);
        l->items = ni;
        l->capacity = nc;
    }
    l->items[l->count++] = v;
    gmb_val_retain(v);
}

void gmb_list_add_range(gmb_list_t* l, gmb_list_t src) {
    if (!l) return;
    for (size_t i = 0; i < src.count; i++)
        gmb_list_add(l, src.items[i]);
}

gmb_val_t gmb_list_get(gmb_list_t l, gmb_int_t i) {
    if (i < 0 || (size_t)i >= l.count) return gmb_val_nil();
    return l.items[i];
}

void gmb_list_set(gmb_list_t* l, gmb_int_t i, gmb_val_t v) {
    if (!l || i < 0 || (size_t)i >= l->count) return;
    /* trattieni il nuovo PRIMA di rilasciare il vecchio: set(i, l[i]) non deve liberarlo */
    gmb_val_retain(v);
    gmb_val_t old = l->items[i];
    l->items[i] = v;
    gmb_val_release(old);
}

static void gmb_list_ensure(gmb_list_t* l, size_t need) {
    if (need <= l->capacity) return;
    size_t nc = l->capacity == 0 ? 4 : l->capacity;
    while (nc < need) nc *= 2;
    gmb_val_t* ni = (gmb_val_t*)realloc(l->items, nc * sizeof(gmb_val_t));
    if (!ni) return;
    if (gmb_mem_on()) gmb_mem_resize(l->items, ni, nc * sizeof(gmb_val_t), GMB_MEM_LIST);
    l->items = ni;
    l->capacity = nc;
}

void gmb_list_insert(gmb_list_t* l, gmb_int_t i, gmb_val_t v) {
    if (!l) return;
    /* C# IList.Insert: no-op se idx < 0 o > Count. */
    if (i < 0 || (size_t)i > l->count) return;
    gmb_list_ensure(l, l->count + 1);
    for (size_t k = l->count; k > (size_t)i; k--) l->items[k] = l->items[k - 1];
    l->items[i] = v;
    l->count++;
    gmb_val_retain(v);
}

void gmb_list_remove_at(gmb_list_t* l, gmb_int_t i) {
    if (!l || i < 0 || (size_t)i >= l->count) return;
    gmb_val_t gone = l->items[i];
    for (size_t k = (size_t)i; k + 1 < l->count; k++) l->items[k] = l->items[k + 1];
    l->count--;
    gmb_val_release(gone);
}

gmb_bool_t gmb_list_contains(gmb_list_t l, gmb_val_t v) {
    for (size_t i = 0; i < l.count; i++) if (gmb_val_equals(l.items[i], v)) return 1;
    return 0;
}

gmb_int_t gmb_list_index_of(gmb_list_t l, gmb_val_t v) {
    for (size_t i = 0; i < l.count; i++) if (gmb_val_equals(l.items[i], v)) return (gmb_int_t)i;
    return -1;
}

void gmb_list_clear(gmb_list_t* l) {
    if (!l) return;
    for (size_t i = 0; i < l->count; i++) gmb_val_release(l->items[i]);
    l->count = 0;
}

gmb_val_t gmb_list_first(gmb_list_t l) { return l.count > 0 ? l.items[0] : gmb_val_nil(); }

gmb_val_t gmb_list_last(gmb_list_t l) { return l.count > 0 ? l.items[l.count - 1] : gmb_val_nil(); }

/* F1 Incongruenze (§0bis): MaxPair/MinPair — chiave della voce con valore max (is_max)
 * o min. Unica implementazione per VM/C, C99Native: confronto numerico
 * (GMB_TYPE_DOUBLE/INT) o stringa, numerico vince sul misto; ritorna la chiave. */
gmb_string_t gmb_dict_max_min_key(const gmb_dict_t* d, int is_max)
{
    gmb_string_t empty = gmb_string_alloc("");
    if (!d || d->count == 0) return empty;
    const char* best_key = NULL;
    const char* best_str = NULL;
    double best_num = 0;
    int numeric_mode = 0;
    for (size_t i = 0; i < d->count; i++) {
        gmb_pair_t* de = &d->items[i];
        double num = 0;
        int is_num = de->value.kind == GMB_TYPE_DOUBLE || de->value.kind == GMB_TYPE_INT;
        if (is_num) num = de->value.kind == GMB_TYPE_DOUBLE ? de->value.v_double : (double)de->value.v_int;
        else if (de->value.kind == GMB_TYPE_STRING) { num = gmb_parse_num(de->value.v_string.data, &is_num); }
        if (is_num) {
            if (!numeric_mode) { numeric_mode = 1; best_key = de->key; best_str = NULL; best_num = num; }
            else if ((is_max && num > best_num) || (!is_max && num < best_num)) { best_num = num; best_key = de->key; }
        } else if (!numeric_mode) {
            const char* s = de->value.kind == GMB_TYPE_STRING ? de->value.v_string.data : "";
            if (!best_key) { best_key = de->key; best_str = s; }
            else if (best_str && ((is_max && strcmp(s, best_str) > 0) || (!is_max && strcmp(s, best_str) < 0))) { best_key = de->key; best_str = s; }
        }
    }
    (void)empty;
    return gmb_string_alloc(best_key ? best_key : "");
}

static int gmb_val_cmp(const void* a, const void* b) {
    const gmb_val_t* x = (const gmb_val_t*)a;
    const gmb_val_t* y = (const gmb_val_t*)b;
    if (x->kind == GMB_TYPE_STRING && y->kind == GMB_TYPE_STRING) {
        const char* xs = x->v_string.data ? x->v_string.data : "";
        const char* ys = y->v_string.data ? y->v_string.data : "";
        int r = strcmp(xs, ys);
        return (r > 0) - (r < 0);
    }
    if (x->kind == GMB_TYPE_INT && y->kind == GMB_TYPE_INT) return (x->v_int > y->v_int) - (x->v_int < y->v_int);
    double xd = x->kind == GMB_TYPE_INT ? (double)x->v_int : x->kind == GMB_TYPE_DOUBLE ? x->v_double : 0;
    double yd = y->kind == GMB_TYPE_INT ? (double)y->v_int : y->kind == GMB_TYPE_DOUBLE ? y->v_double : 0;
    return (xd > yd) - (xd < yd);
}

void gmb_list_sort(gmb_list_t* l) {
    if (!l || l->count < 2) return;
    qsort(l->items, l->count, sizeof(gmb_val_t), gmb_val_cmp);
}

void gmb_list_reverse(gmb_list_t* l) {
    if (!l || l->count < 2) return;
    for (size_t i = 0, j = l->count - 1; i < j; i++, j--) {
        gmb_val_t t = l->items[i]; l->items[i] = l->items[j]; l->items[j] = t;
    }
}

/* Lista circolare: ruota la testa logica di n posizioni (n>0 avanti, n<0 indietro). */
void gmb_list_rotate(gmb_list_t* l, gmb_int_t n) {
    if (!l || l->count < 2) return;
    gmb_int_t steps = n % (gmb_int_t)l->count;
    if (steps < 0) steps += (gmb_int_t)l->count;
    for (gmb_int_t k = 0; k < steps; k++) {
        gmb_val_t t = l->items[0];
        for (size_t i = 0; i + 1 < l->count; i++) l->items[i] = l->items[i + 1];
        l->items[l->count - 1] = t;
    }
}

/* Varianti COPY (non distruttive) per ARRAYS.REVERSE/SORT/UNIQUE/FLAT: restituiscono
 * una NUOVA lista, l'input resta intatto (semantica VM). */
gmb_list_t gmb_list_reverse_copy(gmb_list_t l) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = l.count; i > 0; i--) gmb_list_add(&r, l.items[i - 1]);
    return r;
}

gmb_list_t gmb_list_sort_copy(gmb_list_t l) {
    gmb_list_t r = l;
    gmb_list_sort(&r);
    return r;
}
/* (2026-09-07) ARRAYS.ASORT(list) funzione (emitter C#): ordina la lista IN PLACE (items
 * condivisi) e la ritorna — semantica byRef del parametro (StdLib Arrays.cs). gmb_list_t è
 * per valore ma condivide items, quindi gmb_list_sort(&l) muta la lista reale. */
gmb_list_t gmb_asort(gmb_list_t l) {
    gmb_list_sort(&l);
    return l;
}

gmb_list_t gmb_list_unique_copy(gmb_list_t l) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = 0; i < l.count; i++) {
        int dup = 0;
        for (size_t j = 0; j < r.count; j++)
            if (gmb_val_equals(r.items[j], l.items[i])) { dup = 1; break; }
        if (!dup) gmb_list_add(&r, l.items[i]);
    }
    return r;
}

/* COLEZIONI: copia superficiale (elementi condivisi), union/intersect/except di set. */
gmb_list_t gmb_list_shallow_copy(gmb_list_t l) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = 0; i < l.count; i++) gmb_list_add(&r, l.items[i]);
    return r;
}

/* Copia PROFONDA (elementi stringa ricopiati): X.ToList()/X.ToArray() non condividono
 * gli elementi con la sorgente (mutare la copia non tocca l'originale). */
gmb_list_t gmb_list_deep_copy(gmb_list_t l) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = 0; i < l.count; i++) {
        if (l.items[i].kind == GMB_TYPE_STRING)
            gmb_list_add(&r, gmb_make_val_string(l.items[i].v_string.data));
        else
            gmb_list_add(&r, l.items[i]);
    }
    return r;
}

/* Copia PROFONDA in ordine INVERSO: ToArray()/ToList() di uno STACK restituisce gli
 * elementi dalla CIMA (LIFO) — lo storage della pila è in ordine di push. */
gmb_list_t gmb_list_deep_copy_rev(gmb_list_t l) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = l.count; i > 0; i--) {
        size_t k = i - 1;
        if (l.items[k].kind == GMB_TYPE_STRING)
            gmb_list_add(&r, gmb_make_val_string(l.items[k].v_string.data));
        else
            gmb_list_add(&r, l.items[k]);
    }
    return r;
}

gmb_list_t gmb_set_union(gmb_list_t a, gmb_list_t b) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = 0; i < a.count; i++) gmb_list_add(&r, a.items[i]);
    for (size_t i = 0; i < b.count; i++) {
        int dup = 0;
        for (size_t j = 0; j < r.count; j++)
            if (gmb_val_equals(r.items[j], b.items[i])) { dup = 1; break; }
        if (!dup) gmb_list_add(&r, b.items[i]);
    }
    return r;
}

gmb_list_t gmb_set_intersect(gmb_list_t a, gmb_list_t b) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = 0; i < a.count; i++) {
        int inb = 0, dup = 0;
        for (size_t j = 0; j < b.count; j++)
            if (gmb_val_equals(b.items[j], a.items[i])) { inb = 1; break; }
        for (size_t j = 0; j < r.count; j++)
            if (gmb_val_equals(r.items[j], a.items[i])) { dup = 1; break; }
        if (inb && !dup) gmb_list_add(&r, a.items[i]);
    }
    return r;
}

gmb_list_t gmb_set_except(gmb_list_t a, gmb_list_t b) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = 0; i < a.count; i++) {
        int inb = 0;
        for (size_t j = 0; j < b.count; j++)
            if (gmb_val_equals(b.items[j], a.items[i])) { inb = 1; break; }
        if (!inb) gmb_list_add(&r, a.items[i]);
    }
    return r;
}

/* Differenza simmetrica: (A \ B) U (B \ A), senza duplicati. */
gmb_list_t gmb_set_symmetric_except(gmb_list_t a, gmb_list_t b) {
    gmb_list_t r = gmb_set_except(a, b);
    gmb_list_t rb = gmb_set_except(b, a);
    for (size_t i = 0; i < rb.count; i++) {
        int dup = 0;
        for (size_t j = 0; j < r.count; j++)
            if (gmb_val_equals(r.items[j], rb.items[i])) { dup = 1; break; }
        if (!dup) gmb_list_add(&r, rb.items[i]);
    }
    gmb_list_free(&rb);
    return r;
}

gmb_list_t gmb_list_flatten_copy(gmb_list_t l) {
    gmb_list_t r = gmb_list_from_doubles(NULL, 0);
    for (size_t i = 0; i < l.count; i++) {
        gmb_val_t it = l.items[i];
        /* elemento annidato lista (gmb_make_val_list): appiattisci i suoi elementi */
        if (it.kind == GMB_TYPE_OBJECT && it.v_ptr
            && ((gmb_obj_t*)it.v_ptr)->data && strcmp(((gmb_obj_t*)it.v_ptr)->type_name ? ((gmb_obj_t*)it.v_ptr)->type_name : "", "gmb_list") == 0)
        {
            gmb_list_t sub = gmb_val_as_list(it);
            for (size_t j = 0; j < sub.count; j++) gmb_list_add(&r, sub.items[j]);
        }
        else gmb_list_add(&r, it);
    }
    return r;
}

/* ARRAYS.AJOIN su lista: unisce gli elementi (gmb_val_t) con il separatore.
 * Due passate (lunghezza totale, poi un solo malloc+copy) invece di N concat
 * via gmb_string_concat -- ognuna delle quali riallocava e ricopiava l'intero
 * prefisso già accumulato (O(n^2) su liste lunghe). Stesso pattern già usato
 * da gmb_array_join/gmb_array_join_doubles per lo stesso motivo. */
gmb_string_t gmb_list_join(gmb_list_t l, const char* sep) {
    if (l.count == 0) return gmb_string_alloc("");
    const char* s = sep ? sep : "";
    size_t seplen = strlen(s);
    gmb_string_t* pieces = (gmb_string_t*)malloc(sizeof(gmb_string_t) * l.count);
    if (!pieces) return gmb_string_alloc("");
    size_t total = 0;
    for (size_t i = 0; i < l.count; i++) {
        pieces[i] = gmb_val_to_string(l.items[i]);
        total += pieces[i].data ? pieces[i].length : 0;
        if (i > 0) total += seplen;
    }
    char* buf = (char*)malloc(total + 1);
    if (!buf) { free(pieces); return gmb_string_alloc(""); }
    size_t off = 0;
    for (size_t i = 0; i < l.count; i++) {
        if (i > 0 && seplen > 0) { memcpy(buf + off, s, seplen); off += seplen; }
        size_t ln = pieces[i].data ? pieces[i].length : 0;
        if (ln > 0) { memcpy(buf + off, pieces[i].data, ln); off += ln; }
    }
    buf[off] = '\0';
    free(pieces);
    gmb_string_t out = gmb_string_alloc(buf);
    free(buf);
    return out;
}

/* Join di una lista con ", " fra parentesi quadre (`[a, b]`): parità con l'output di un array
 * IL/C# (es. AI.PLUGINSLIST). Usato dall'emitter C99Native; l'emitter C# ha il proprio loop di
 * stampa lista. §0bis: unica implementazione del formato. */
gmb_string_t gmb_list_join_bracketed(gmb_list_t l) {
    gmb_string_t inner = gmb_list_join(l, ", ");
    size_t n = inner.data ? inner.length : 0;
    char* buf = (char*)malloc(n + 3);
    if (!buf) return gmb_string_alloc("");
    buf[0] = '[';
    if (n > 0) memcpy(buf + 1, inner.data, n);
    buf[n + 1] = ']';
    buf[n + 2] = '\0';
    gmb_string_t out = gmb_string_alloc(buf);
    free(buf);
    return out;
}

void gmb_list_push(gmb_list_t* l, gmb_val_t v) { gmb_list_add(l, v); }

gmb_val_t gmb_list_pop(gmb_list_t* l) {
    if (!l || l->count == 0) return gmb_val_nil();
    gmb_val_t v = l->items[l->count - 1];
    l->count--;
    return v;
}

void gmb_list_enqueue(gmb_list_t* l, gmb_val_t v) { gmb_list_add(l, v); }

gmb_val_t gmb_list_dequeue(gmb_list_t* l) {
    if (!l || l->count == 0) return gmb_val_nil();
    gmb_val_t v = l->items[0];
    /* trasferimento al chiamante: niente release (vedi commento di gmb_list_free) */
    for (size_t k = 0; k + 1 < l->count; k++) l->items[k] = l->items[k + 1];
    l->count--;
    return v;
}

gmb_val_t gmb_list_peek(gmb_list_t l) { return l.count > 0 ? l.items[l.count - 1] : gmb_val_nil(); }

/* --- Boxing di una LISTA dentro gmb_val_t (ANY/VARIANT che contiene una lista) ---
 * La lista è copiata su heap (gmb_list_t*) e avvolta come oggetto con type_name "gmb_list".
 * gmb_val_as_list estrae la lista (copia) per DIRECTCAST(obj, LIST). */
gmb_val_t gmb_make_val_list(gmb_list_t src) {
    gmb_list_t* p = (gmb_list_t*)malloc(sizeof(gmb_list_t));
    if (!p) return gmb_val_nil();
    *p = src;
    p->items = NULL; p->count = 0; p->capacity = 0;
    if (src.count > 0) {
        p->items = (gmb_val_t*)malloc(src.count * sizeof(gmb_val_t));
        if (p->items) {
            for (size_t i = 0; i < src.count; i++) { p->items[i] = src.items[i]; gmb_val_retain(p->items[i]); }
            p->count = src.count; p->capacity = src.count;
        }
    }
    return gmb_make_val_obj(gmb_obj_new_typed(p, "gmb_list"));
}

/* Variante "take": il box ADOTTA l'array di `*src` (nessuna copia, nessun retain: la proprieta' dei valori
 * si trasferisce insieme all'array) e lascia `*src` vuoto. Contratto: `*src` e' una lista con storage PROPRIO
 * (gmb_list_new/derivati), non un alias di un'altra lista (es. una vista di un box). Dopo la chiamata
 * `gmb_list_free(src)` e' un no-op: chi liberava la sorgente continua a funzionare. */
gmb_val_t gmb_make_val_list_take(gmb_list_t* src) {
    if (!src) return gmb_val_nil();
    gmb_list_t* p = (gmb_list_t*)malloc(sizeof(gmb_list_t));
    if (!p) return gmb_val_nil();
    *p = *src;
    gmb_obj_t* box = gmb_obj_new_typed(p, "gmb_list");
    if (!box) { free(p); return gmb_val_nil(); }   /* *src resta intatto: il chiamante lo libera */
    src->items = NULL; src->count = 0; src->capacity = 0;
    return gmb_make_val_obj(box);
}

gmb_list_t gmb_val_as_list(gmb_val_t v) {
    gmb_list_t r = gmb_list_new();
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return r;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o->data) return r;
    /* Solo liste reali: un dict (o un altro oggetto opaco non-lista) NON è una lista →
     * restituisci vuoto (mai reinterpretare un gmb_dict_t come gmb_list_t: le due
     * struct hanno layout diversi — un cast cieco legge OLTRE i campi reali,
     * comportamento indefinito che produce valori "garbage" plausibili ma corrotti,
     * senza mai un crash che lo segnali). Bug trovato con un JSON object boxed
     * (JSON.PARSE di "{...}") iterato con FOR EACH sul C99Native, che chiamava questa
     * funzione incondizionatamente assumendo SEMPRE forma-array. Stessa guardia già
     * presente nel gemello gmb_val_as_dict sotto (asimmetrica per costruzione: qui
     * rifiuta SOLO quando type_name è esplicitamente "dict", permissiva altrove, per
     * non cambiare comportamento su altri oggetti runtime-wrapped esistenti). */
    if (o->type_name && strstr(o->type_name, "dict")) return r;
    gmb_list_t* p = (gmb_list_t*)o->data;
    for (size_t i = 0; i < p->count; i++) gmb_list_add(&r, p->items[i]);
    return r;
}

/* FOR EACH su un gmb_val_t boxed (JSON/VARIANT): lista → gli elementi; dict (oggetto
 * JSON) → le chiavi. Fallback: lista vuota. */
gmb_list_t gmb_val_foreach_list(gmb_val_t v) {
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return gmb_list_new();
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o->data) return gmb_list_new();
    if (o->type_name && strstr(o->type_name, "dict"))
        return gmb_val_dict_keys(v);
    return gmb_val_as_list(v);
}

/* FOR EACH su val boxed (JSON dict/lista) — i-esima "entry". Un dict (oggetto JSON) dà
 * un KeyValuePair {Key,Value} (parità IL/PCode: `FOR EACH e IN dict` → e.Key/e.Value);
 * una lista dà l'i-esimo elemento. Unica implementazione (§0bis) condivisa da emitter C#
 * e C99Native (bug C99Native-Foreach-KVP-Dict-Boxed-Value-Stringa: un dict era iterato
 * come lista di chiavi → e.Value stringa/garbage). */
static int gmb_val_is_dict_obj(gmb_val_t v) {
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return 0;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    return o->data && o->type_name && strstr(o->type_name, "dict") != NULL;
}

gmb_val_t gmb_val_foreach_entry(gmb_val_t v, gmb_int_t i) {
    if (gmb_val_is_dict_obj(v)) {
        gmb_dict_t d = gmb_val_as_dict(v);
        if (i < 0 || i >= (gmb_int_t)d.count) return gmb_val_nil();
        gmb_dict_t e = gmb_dict_new();
        gmb_dict_set(&e, "Key", gmb_make_val_string(d.items[i].key));
        gmb_dict_set(&e, "Value", d.items[i].value);
        return gmb_make_val_dict(e);
    }
    gmb_list_t l = gmb_val_as_list(v);
    return (i >= 0 && i < (gmb_int_t)l.count) ? l.items[i] : gmb_val_nil();
}

/* Chiave (dict, stringa) / indice (lista, numerico) dell'i-esima entry. */
gmb_val_t gmb_val_foreach_key(gmb_val_t v, gmb_int_t i) {
    if (gmb_val_is_dict_obj(v)) {
        gmb_dict_t d = gmb_val_as_dict(v);
        if (i < 0 || i >= (gmb_int_t)d.count) return gmb_val_nil();
        return gmb_make_val_string(d.items[i].key);
    }
    return gmb_make_val_int(i);
}

/* Valore (dict) / elemento (lista) dell'i-esima entry. */
gmb_val_t gmb_val_foreach_value(gmb_val_t v, gmb_int_t i) {
    if (gmb_val_is_dict_obj(v)) {
        gmb_dict_t d = gmb_val_as_dict(v);
        if (i < 0 || i >= (gmb_int_t)d.count) return gmb_val_nil();
        return d.items[i].value;
    }
    gmb_list_t l = gmb_val_as_list(v);
    return (i >= 0 && i < (gmb_int_t)l.count) ? l.items[i] : gmb_val_nil();
}

/* Indice su un gmb_val_t opaco: se contiene una lista ritorna l'elemento i-esimo,
   se un dict ritorna il valore per chiave (stringa), altrimenti NIL.
   Un indice NUMERICO su un dict viene convertito alla sua rappresentazione decimale
   ("0","1",...): è così che gmb_val_tuple() chiavia gli elementi (dict con chiavi
   "0".."n-1", cfr. gmb_dict.c) — senza questa conversione tup(i) con i numerico
   cercava la chiave "" (sempre assente) e tornava sempre NIL (bug
   tuple/parita transpiler-native, valori <nil>/0 nonostante la compilazione OK). */
gmb_val_t gmb_val_index(gmb_val_t v, gmb_val_t idx) {
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return gmb_val_nil();
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o->data) return gmb_val_nil();
    if (o->type_name && strstr(o->type_name, "dict"))
    {
        char numkey[32];
        const char* key;
        if (idx.kind == GMB_TYPE_STRING) key = idx.v_string.data;
        else {
            gmb_int_t ik = idx.kind == GMB_TYPE_DOUBLE ? (gmb_int_t)idx.v_double
                          : idx.kind == GMB_TYPE_INT ? idx.v_int : -1;
            if (ik < 0) return gmb_val_nil();
            snprintf(numkey, sizeof(numkey), "%lld", (long long)ik);
            key = numkey;
        }
        return gmb_dict_get(*(gmb_dict_t*)o->data, key);
    }
    gmb_list_t* p = (gmb_list_t*)o->data;
    gmb_int_t i = idx.kind == GMB_TYPE_DOUBLE ? (gmb_int_t)idx.v_double : idx.kind == GMB_TYPE_INT ? idx.v_int : 0;
    if (i < 0 || i >= (gmb_int_t)p->count) return gmb_val_nil();
    return p->items[i];
}

/* Accesso AMBIGUO `x.membro[idx]` vs `x[idx].membro`: il parser nativo FONDE le due forme
 * nell'unica CALL("x.membro", [idx]) — l'ordine sintattico si perde. Si risolve a RUNTIME in
 * base alla FORMA del valore base: se `x` è un DICT → `x["membro"][idx]` (columnar, dict-di-
 * liste); se `x` è una LISTA → `x[idx]["membro"]` (row-based, lista-di-dict). Usato
 * dall'emitter C99Native per i valori JSON boxed (parità C# IndexedAccess/MemberAccess). */
gmb_val_t gmb_val_member_or_index(gmb_val_t base, const char* key, gmb_val_t idx) {
    if (base.kind != GMB_TYPE_OBJECT || !base.v_ptr) return gmb_val_nil();
    gmb_obj_t* o = (gmb_obj_t*)base.v_ptr;
    if (!o->data) return gmb_val_nil();
    if (o->type_name && strstr(o->type_name, "dict"))
        return gmb_val_index(gmb_dict_get(*(gmb_dict_t*)o->data, key), idx);
    return gmb_dict_get(gmb_val_as_dict(gmb_val_index(base, idx)), key);
}

/* --- Boxing di un ARRAY DI BYTE dentro gmb_val_t (DICT di ARRAY OF BYTE) ---
 * Copia su heap e avvolge come oggetto type_name "gmb_bytes"; gmb_val_as_bytes
 * estrae (array allocato + lunghezza) per `dict("key")` su un array di byte. */
gmb_val_t gmb_make_val_bytes(const uint8_t* data, size_t len) {
    size_t* p = (size_t*)malloc(sizeof(size_t) + len);
    if (!p) return gmb_val_nil();
    p[0] = len;
    if (data && len > 0) memcpy(p + 1, data, len);
    return gmb_make_val_obj(gmb_obj_new_typed(p, "gmb_bytes"));
}

uint8_t* gmb_val_as_bytes(gmb_val_t v, size_t* len_out) {
    *len_out = 0;
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return NULL;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o->data) return NULL;
    size_t* p = (size_t*)o->data;
    size_t len = p[0];
    uint8_t* out = (uint8_t*)malloc(len ? len : 1);
    if (!out) return NULL;
    if (len > 0) memcpy(out, (const uint8_t*)(p + 1), len);
    *len_out = len;
    return out;
}

/* REDIM(list, newlen, preserve): ridimensiona una lista.
   preserve=1 → copia min(old,new) elementi, riempie i nuovi con nil;
   preserve=0 → azzera. Ritorna 0 su successo. */
gmb_int_t gmb_list_resize(gmb_list_t* l, gmb_int_t newlen, int preserve) {
    if (!l || newlen < 0) return -1;
    size_t old = l->count;
    if ((size_t)newlen == old) return 0;
    gmb_list_ensure(l, (size_t)newlen);
    if (preserve) {
        if ((size_t)newlen < old) {
            for (size_t i = (size_t)newlen; i < old; i++) gmb_val_release(l->items[i]);
            l->count = (size_t)newlen;
        } else {
            for (size_t i = old; i < (size_t)newlen; i++) l->items[i] = gmb_val_nil();
            l->count = (size_t)newlen;
        }
    } else {
        for (size_t i = 0; i < old; i++) gmb_val_release(l->items[i]);   /* azzera: tutti i vecchi escono */
        for (size_t i = 0; i < (size_t)newlen; i++) l->items[i] = gmb_val_nil();
        l->count = (size_t)newlen;
    }
    return 0;
}

/* ----------------------------------------------------------------------------
 * AMAP/AFILTER/AREDUCE/AEVAL con callback gmb_fn_t (funzione di prima classe).
 * Il callback ha firma gmb_int_t(*)(void* ctx, gmb_int_t elem); gli elementi sono
 * estratti a int (v_int/v_double). Stessa semantica della VM/C (gmb_vm_exec_builtin
 * case 514/515/516/518), adattata al modello transpiler.
 * ---------------------------------------------------------------------------- */
static gmb_int_t gmb_list_elem_to_int(gmb_val_t v)
{
    switch (v.kind)
    {
        case GMB_TYPE_INT:   return v.v_int;
        case GMB_TYPE_DOUBLE: return (gmb_int_t)v.v_double;
        case GMB_TYPE_BOOL:  return v.v_bool ? 1 : 0;
        default: return 0;
    }
}

static gmb_double_t gmb_list_elem_to_double(gmb_val_t v)
{
    switch (v.kind)
    {
        case GMB_TYPE_INT:   return (gmb_double_t)v.v_int;
        case GMB_TYPE_DOUBLE: return v.v_double;
        case GMB_TYPE_BOOL:  return v.v_bool ? 1.0 : 0.0;
        default: return 0.0;
    }
}

gmb_list_t gmb_list_map(gmb_list_t l, gmb_fn_t fn)
{
    gmb_list_t out = gmb_list_new();
    if (!fn.fn) return out;
    /* Le lambda C99 transpilate hanno firma gmb_double_t(*)(void*, gmb_double_t):
     * chiamate con la STESSA firma (mai cast a int — UB sui registri ARM). */
    gmb_double_t (*cb)(void*, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t))fn.fn;
    for (size_t i = 0; i < l.count; i++)
    {
        gmb_double_t r = cb(fn.ctx, gmb_list_elem_to_double(l.items[i]));
        gmb_list_add(&out, gmb_make_val_double(r));
    }
    return out;
}

gmb_list_t gmb_list_filter(gmb_list_t l, gmb_fn_t fn)
{
    gmb_list_t out = gmb_list_new();
    if (!fn.fn) return out;
    gmb_double_t (*cb)(void*, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t))fn.fn;
    for (size_t i = 0; i < l.count; i++)
    {
        gmb_val_t it = l.items[i];
        if (cb(fn.ctx, gmb_list_elem_to_double(it)) != 0)
            gmb_list_add(&out, it);
    }
    return out;
}

gmb_double_t gmb_list_reduce(gmb_list_t l, gmb_fn_t fn, gmb_double_t init)
{
    if (!fn.fn) return init;
    /* AREDUCE: la callback transpilata ha firma gmb_double_t(*)(void*, gmb_double_t,
     * gmb_double_t) — due argomenti (acc, elem). */
    gmb_double_t (*cb)(void*, gmb_double_t, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t, gmb_double_t))fn.fn;
    gmb_double_t acc = init;
    for (size_t i = 0; i < l.count; i++)
        acc = cb(fn.ctx, acc, gmb_list_elem_to_double(l.items[i]));
    return acc;
}

void gmb_list_foreach(gmb_list_t l, gmb_fn_t fn)
{
    if (!fn.fn) return;
    gmb_double_t (*cb)(void*, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t))fn.fn;
    for (size_t i = 0; i < l.count; i++)
        cb(fn.ctx, gmb_list_elem_to_double(l.items[i]));
}

gmb_list_t gmb_list_from_doubles(const gmb_double_t* arr, gmb_int_t n)
{
    gmb_list_t out = gmb_list_new();
    for (gmb_int_t i = 0; i < n; i++)
        gmb_list_add(&out, gmb_make_val_double(arr[i]));
    return out;
}

gmb_list_t gmb_list_from_ints(const gmb_int_t* arr, gmb_int_t n)
{
    gmb_list_t out = gmb_list_new();
    for (gmb_int_t i = 0; i < n; i++)
        gmb_list_add(&out, gmb_make_val_int(arr[i]));
    return out;
}

gmb_list_t gmb_list_from_strings(const gmb_string_t* arr, gmb_int_t n)
{
    gmb_list_t out = gmb_list_new();
    for (gmb_int_t i = 0; i < n; i++)
        gmb_list_add(&out, gmb_make_val_string(arr[i].data ? arr[i].data : ""));
    return out;
}

gmb_int_t* gmb_list_to_ints_array(gmb_list_t l, gmb_int_t* len_out)
{
    gmb_int_t* out = (gmb_int_t*)calloc(l.count ? l.count : 1, sizeof(gmb_int_t));
    for (size_t i = 0; i < l.count; i++)
        out[i] = gmb_list_elem_to_int(l.items[i]);
    *len_out = (gmb_int_t)l.count;
    return out;
}

gmb_double_t* gmb_list_to_doubles_array(gmb_list_t l, gmb_int_t* len_out)
{
    gmb_double_t* out = (gmb_double_t*)calloc(l.count ? l.count : 1, sizeof(gmb_double_t));
    for (size_t i = 0; i < l.count; i++)
        out[i] = (gmb_double_t)gmb_list_elem_to_int(l.items[i]);
    *len_out = (gmb_int_t)l.count;
    return out;
}

/* Ponte LISTA -> ARRAY numerico per gli ARGOMENTI array dei SUB/FUNCTION (C99Native).
 * In questo backend i byte "grezzi" possono vivere in una gmb_list_t (es.
 * `DIM b() AS BYTE : b = IO.READBYTES(p)`): per passarli a un parametro `arr()` serve un
 * `gmb_double_t*`. Come gmb_bytes_as_doubles: buffer a ROTAZIONE, nessuna allocazione da
 * liberare dal chiamante (vita valida per l'espressione che lo usa). Prima l'argomento
 * veniva rifiutato ("l'argomento N non è un array", 2026-11-19). */
#define GMB_LIST_AS_DOUBLES_SLOTS 8
const gmb_double_t* gmb_list_as_doubles(gmb_list_t l, gmb_int_t* len_out)
{
    static gmb_double_t* buf[GMB_LIST_AS_DOUBLES_SLOTS];
    static size_t cap[GMB_LIST_AS_DOUBLES_SLOTS];
    static int slot = 0;
    int i = slot;
    slot = (slot + 1) % GMB_LIST_AS_DOUBLES_SLOTS;
    size_t n = l.count;
    if (n + 1 > cap[i]) {
        gmb_double_t* nb = (gmb_double_t*)realloc(buf[i], (n + 1) * sizeof(gmb_double_t));
        if (!nb) { if (len_out) *len_out = 0; return buf[i]; }
        buf[i] = nb;
        cap[i] = n + 1;
    }
    for (size_t k = 0; k < n; k++)
        buf[i][k] = (gmb_double_t)gmb_list_elem_to_int(l.items[k]);
    if (len_out) *len_out = (gmb_int_t)n;
    return buf[i];
}

gmb_string_t* gmb_list_to_strings_array(gmb_list_t l, gmb_int_t* len_out)
{
    gmb_string_t* out = (gmb_string_t*)calloc(l.count ? l.count : 1, sizeof(gmb_string_t));
    for (size_t i = 0; i < l.count; i++) {
        gmb_val_t v = l.items[i];
        const char* s = (v.kind == GMB_TYPE_STRING && v.v_string.data) ? v.v_string.data : "";
        out[i] = gmb_string_alloc(s);
    }
    *len_out = (gmb_int_t)l.count;
    return out;
}

gmb_int_t gmb_list_scan(gmb_list_t l, gmb_fn_t fn)
{
    if (!fn.fn) return -1;
    gmb_double_t (*cb)(void*, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t))fn.fn;
    for (size_t i = 0; i < l.count; i++)
        if (cb(fn.ctx, gmb_list_elem_to_double(l.items[i]))) return (gmb_int_t)i;
    return -1;
}

/* Somma degli elementi numerici di una lista. */
double gmb_list_sum(gmb_list_t l)
{
    double total = 0;
    for (size_t i = 0; i < l.count; i++)
    {
        gmb_val_t v = l.items[i];
        if (v.kind == GMB_TYPE_INT) total += (double)v.v_int;
        else if (v.kind == GMB_TYPE_DOUBLE) total += v.v_double;
        else if (v.kind == GMB_TYPE_BOOL) total += v.v_bool ? 1.0 : 0.0;
    }
    return total;
}

/* ABINARYSEARCH — parita IL (CommonLib.ArrayUtils.BinarySearch): VERA ricerca binaria su
 * lista GIÀ ORDINATA per il confronto numerico; indice 0-based o -1. Confronto ESATTO
 * (==), non fabs tolleranza: prima era una scan lineare con fabs(d-target)<1e-12. */
gmb_int_t gmb_list_binary_search(gmb_list_t l, double target)
{
    if (l.count == 0) return -1;
    gmb_int_t lo = 0, hi = (gmb_int_t)l.count - 1;
    while (lo <= hi)
    {
        gmb_int_t mid = lo + (hi - lo) / 2;
        gmb_val_t it = l.items[mid];
        double d = it.kind == GMB_TYPE_DOUBLE ? it.v_double : it.kind == GMB_TYPE_INT ? (double)it.v_int : 0;
        if (d == target) return mid;
        else if (d < target) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* RANGE(start, end[, step]) → lista di numeri (inclusivo). step==0 → 1. */
gmb_list_t gmb_list_range(gmb_double_t start, gmb_double_t end, gmb_double_t step) {
    gmb_list_t out = gmb_list_new();
    if (step == 0) step = 1;
    if (step > 0)
        for (gmb_double_t v = start; v <= end; v += step) gmb_list_add(&out, gmb_make_val_double(v));
    else
        for (gmb_double_t v = start; v >= end; v += step) gmb_list_add(&out, gmb_make_val_double(v));
    return out;
}

/* Max/Min degli elementi numerici di una lista (empty → 0). */
double gmb_list_max(gmb_list_t l) {
    double m = 0; int first = 1;
    for (size_t i = 0; i < l.count; i++) {
        double d = gmb_list_elem_to_double(l.items[i]);
        if (first) { m = d; first = 0; }
        else if (d > m) m = d;
    }
    return m;
}

double gmb_list_min(gmb_list_t l) {
    double m = 0; int first = 1;
    for (size_t i = 0; i < l.count; i++) {
        double d = gmb_list_elem_to_double(l.items[i]);
        if (first) { m = d; first = 0; }
        else if (d < m) m = d;
    }
    return m;
}

/* TRIE/BAG/metodi extra: prefisso, findall, conteggi. Operano su liste di stringhe o val. */
gmb_double_t gmb_list_starts_with(gmb_list_t l, const char* pref) {
    if (!pref) return 0;
    for (size_t i = 0; i < l.count; i++) {
        const char* d = l.items[i].v_string.data;
        if (d && strncmp(d, pref, strlen(pref)) == 0) return -1;
    }
    return 0;
}
gmb_list_t gmb_list_find_all(gmb_list_t l, const char* pref) {
    gmb_list_t r = gmb_list_new();
    if (!pref) return r;
    size_t pl = strlen(pref);
    for (size_t i = 0; i < l.count; i++) {
        const char* d = l.items[i].v_string.data;
        if (d && strncmp(d, pref, pl) == 0) gmb_list_add(&r, l.items[i]);
    }
    return r;
}
gmb_int_t gmb_list_unique_count(gmb_list_t l) {
    gmb_int_t n = 0;
    for (size_t i = 0; i < l.count; i++) {
        int dup = 0;
        for (size_t j = 0; j < i; j++) if (gmb_val_equals(l.items[j], l.items[i])) { dup = 1; break; }
        if (!dup) n++;
    }
    return n;
}
gmb_int_t gmb_list_count_of(gmb_list_t l, gmb_val_t v) {
    gmb_int_t n = 0;
    for (size_t i = 0; i < l.count; i++) if (gmb_val_equals(l.items[i], v)) n++;
    return n;
}
gmb_dict_t gmb_bag_counts(gmb_list_t l) {
    gmb_dict_t d = gmb_dict_new();
    for (size_t i = 0; i < l.count; i++) {
        const char* key = l.items[i].v_string.data ? l.items[i].v_string.data : "";
        gmb_val_t cur = gmb_dict_get(d, key);
        gmb_int_t n = cur.kind == GMB_TYPE_DOUBLE ? (gmb_int_t)cur.v_double : 0;
        gmb_dict_set(&d, key, gmb_make_val_double((double)(n + 1)));
    }
    return d;
}

/* collezioni numeriche: somma prefissa/intervallo e update per-indice (BITREE approx) */
gmb_double_t gmb_list_sum_range(gmb_list_t l, gmb_int_t from_i, gmb_int_t to_i) {
    gmb_double_t t = 0;
    for (gmb_int_t i = from_i; i <= to_i && i < (gmb_int_t)l.count; i++) {
        gmb_val_t v = l.items[i];
        if (v.kind == GMB_TYPE_DOUBLE) t += v.v_double;
        else if (v.kind == GMB_TYPE_INT) t += (gmb_double_t)v.v_int;
    }
    return t;
}
void gmb_list_add_at(gmb_list_t* l, gmb_int_t i, gmb_double_t delta) {
    if (!l || i < 0 || (size_t)i >= l->count) return;
    gmb_val_t cur = l->items[i];
    double base = cur.kind == GMB_TYPE_DOUBLE ? cur.v_double : cur.kind == GMB_TYPE_INT ? (double)cur.v_int : 0;
    gmb_list_set(l, i, gmb_make_val_double(base + delta));
}

/* Lettura numerica di lista: NAN se fuori range (safe-access/IS NIL). */
gmb_double_t gmb_list_get_or_nan(gmb_list_t l, gmb_int_t i) {
    if (i < 0 || (size_t)i >= l.count) return (gmb_double_t)(0.0 / 0.0);
    gmb_val_t v = l.items[i];
    if (v.kind == GMB_TYPE_DOUBLE) return v.v_double;
    if (v.kind == GMB_TYPE_INT) return (gmb_double_t)v.v_int;
    return (gmb_double_t)(0.0 / 0.0);
}

/* join di liste/array numerici o di stringhe con separatore (AJOIN). */
static void gmb_join_buf(gmb_string_t* acc, const char* add, const char* sep, int* first) {
    gmb_string_t cat = *first ? gmb_string_alloc(add)
                              : gmb_string_concat(gmb_string_concat(*acc, gmb_string_alloc(sep)), gmb_string_alloc(add));
    if (!*first) gmb_string_free(acc);
    *acc = cat;
    *first = 0;
}
gmb_string_t gmb_double_array_join(const gmb_double_t* a, gmb_int_t n, const char* sep) {
    gmb_string_t r = gmb_string_alloc("");
    int first = 1;
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_string_t t = gmb_double_to_string(a[i]);
        gmb_join_buf(&r, t.data ? t.data : "", sep, &first);
    }
    return r;
}
gmb_string_t gmb_string_array_join(const gmb_string_t* a, gmb_int_t n, const char* sep) {
    gmb_string_t r = gmb_string_alloc("");
    int first = 1;
    for (gmb_int_t i = 0; i < n; i++) gmb_join_buf(&r, a[i].data ? a[i].data : "", sep, &first);
    return r;
}
gmb_string_t gmb_list_join_doubles(gmb_list_t l, const char* sep) {
    gmb_string_t r = gmb_string_alloc("");
    int first = 1;
    for (size_t i = 0; i < l.count; i++) {
        gmb_string_t t = gmb_val_to_string(l.items[i]);
        gmb_join_buf(&r, t.data ? t.data : "", sep, &first);
    }
    return r;
}
/* FOR EACH su stringa: carattere i-esimo come stringa di un char. */
gmb_string_t gmb_string_char_at(gmb_string_t s, gmb_int_t i) {
    if (i < 0 || !s.data) return gmb_string_alloc("");
    size_t n = strlen(s.data);
    if ((size_t)i >= n) return gmb_string_alloc("");
    char b[2]; b[0] = s.data[i]; b[1] = 0;
    return gmb_string_alloc(b);
}

/* AJOIN universale (emitter C# `.Join`, ARRAYS.AJOIN): unisce gli elementi della
 * lista (numerici o stringhe) col separatore dato, senza spazi. */
gmb_string_t gmb_ajoin(gmb_list_t l, const char* sep) {
    gmb_string_t r = gmb_string_alloc("");
    int first = 1;
    const char* s = sep ? sep : "";
    for (size_t i = 0; i < l.count; i++) {
        gmb_string_t t = gmb_val_to_string(l.items[i]);
        const char* td = t.data ? t.data : "";
        if (!first) {
            gmb_string_t pre = gmb_string_alloc(s);
            gmb_string_t c1 = gmb_string_concat(r, pre);
            gmb_string_t c2 = gmb_string_concat(c1, gmb_string_alloc(td));
            r = c2;
        } else {
            r = gmb_string_alloc(td);
            first = 0;
        }
    }
    return r;
}

/* RANGE(a, b [, step]) → lista numerica inclusiva (FOR EACH n IN RANGE(...)). */
gmb_list_t gmb_range(double a, double b) {
    gmb_list_t r = gmb_list_new();
    for (double v = a; v <= b; v += 1.0) gmb_list_add(&r, gmb_make_val_double(v));
    return r;
}
gmb_list_t gmb_range_step(double a, double b, double step) {
    gmb_list_t r = gmb_list_new();
    if (step == 0) step = 1;
    for (double v = a; (step > 0 ? v <= b : v >= b); v += step) gmb_list_add(&r, gmb_make_val_double(v));
    return r;
}

/* Scrittura di un elemento su un valore JSON/collezione boxed (lista o dict), per
 * C99Native: gmb_val_as_list/dict restituiscono COPIE (items condivisi ma struct
 * copiata), quindi non vanno usati per mutare. Qui si accede al puntatore reale
 * (o->data) e si distingue lista (indice numerico) da dict (chiave stringa).
 * Ritorna 1 se ha scritto, 0 se il contenitore non è indicizzabile. */
int gmb_val_index_set(gmb_val_t v, gmb_val_t key, gmb_val_t nv)
{
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return 0;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o->data) return 0;
    if (o->type_name && strstr(o->type_name, "dict"))
    {
        const char* k = gmb_val_to_string(key).data;
        gmb_dict_set((gmb_dict_t*)o->data, k ? k : "", nv);
        return 1;
    }
    gmb_list_t* p = (gmb_list_t*)o->data;
    gmb_list_set(p, gmb_val_to_int(key), nv);
    return 1;
}
