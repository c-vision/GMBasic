/* list_ownership_selfcheck.c — ownership dei valori OBJECT contati in gmb_list_t e nelle collezioni
 * a handle (gmb_collections.c). Gemello di dict_ownership_selfcheck.c.
 *
 * Contratto: un contenitore POSSIEDE i valori OBJECT con release_fn != NULL (oggetti contati della
 * VM): add/set/insert/enqueue/kv_set/cache_set li trattengono; free/clear/remove/sostituzione/
 * overwrite li rilasciano. pop/dequeue/extract TRASFERISCONO il riferimento al chiamante (nessun
 * release). Gli oggetti "in prestito" (release_fn NULL) e i valori non-OBJECT non vengono toccati.
 *
 * Build/run (dalla root del repo, dopo aver costruito libgmb_runtime.a — es. via gmb_vm_c/build):
 *   cc -fsanitize=address -g -Inative/gmb_runtime_c/include native/gmb_runtime_c/tests/list_ownership_selfcheck.c \
 *      native/gmb_vm_c/build/gmb_runtime_c_build/libgmb_runtime.a -lm -lpthread \
 *      -framework Cocoa -framework Foundation -framework UserNotifications \
 *      -Wl,-undefined,dynamic_lookup -o /tmp/list_own && /tmp/list_own */
#include "gmb_runtime.h"
#include "gmb_collections.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static int g_freed;

#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL %s (%s:%d)\n", msg, __FILE__, __LINE__); g_fail = 1; } } while (0)

static void counted_release(struct gmb_obj* o) { g_freed++; free(o); }
static gmb_obj_t* counted_new(void)            /* rc 1 = il riferimento del creatore */
{
    gmb_obj_t* o = (gmb_obj_t*)calloc(1, sizeof(gmb_obj_t));
    o->refcount = 1;
    o->release_fn = counted_release;
    return o;
}
static void drop(gmb_obj_t* o) { if (--o->refcount == 0) o->release_fn(o); }   /* come gmb_obj_release */
static gmb_val_t V(gmb_obj_t* o) { return gmb_make_val_obj(o); }

int main(void)
{
    /* --- LISTE ----------------------------------------------------------------------------- */
    /* 1) add trattiene, free rilascia; la sorgente puo' morire PRIMA della lista */
    {
        g_freed = 0;
        gmb_list_t l = gmb_list_new();
        gmb_obj_t* o = counted_new();
        gmb_list_add(&l, V(o));
        CHECK(o->refcount == 2, "add trattiene");
        drop(o);
        CHECK(g_freed == 0 && o->refcount == 1, "la lista tiene vivo l'oggetto dopo il rilascio del creatore");
        gmb_list_free(&l);
        CHECK(g_freed == 1, "free rilascia");
    }
    /* 2) set: nuovo trattenuto prima del rilascio del vecchio; reinserire lo stesso non lo libera */
    {
        g_freed = 0;
        gmb_list_t l = gmb_list_new();
        gmb_obj_t* a = counted_new(); gmb_obj_t* b = counted_new();
        gmb_list_add(&l, V(a)); drop(a);                 /* solo la lista tiene a (rc 1) */
        gmb_list_set(&l, 0, V(a));
        CHECK(g_freed == 0 && a->refcount == 1, "set(i, l[i]) non libera il valore");
        gmb_list_set(&l, 0, V(b)); drop(b);
        CHECK(g_freed == 1, "set rilascia il vecchio");
        gmb_list_free(&l);
        CHECK(g_freed == 2, "free rilascia il corrente");
    }
    /* 3) insert / remove_at / clear / resize */
    {
        g_freed = 0;
        gmb_list_t l = gmb_list_new();
        gmb_obj_t* a = counted_new(); gmb_obj_t* b = counted_new(); gmb_obj_t* c = counted_new();
        gmb_list_insert(&l, 0, V(a)); drop(a);
        gmb_list_insert(&l, 0, V(b)); drop(b);
        gmb_list_insert(&l, 1, V(c)); drop(c);
        CHECK(l.count == 3 && g_freed == 0, "insert trattiene");
        gmb_list_remove_at(&l, 1);
        CHECK(g_freed == 1, "remove_at rilascia");
        gmb_list_resize(&l, 1, 1);
        CHECK(g_freed == 2, "resize (troncamento) rilascia");
        gmb_list_clear(&l);
        CHECK(g_freed == 3, "clear rilascia");
        gmb_list_free(&l);
        CHECK(g_freed == 3, "free di lista vuota: niente doppio rilascio");
    }
    /* 4) pop / dequeue TRASFERISCONO: nessun release */
    {
        g_freed = 0;
        gmb_list_t l = gmb_list_new();
        gmb_obj_t* a = counted_new(); gmb_obj_t* b = counted_new();
        gmb_list_add(&l, V(a)); drop(a);
        gmb_list_add(&l, V(b)); drop(b);
        gmb_val_t p = gmb_list_pop(&l);
        gmb_val_t q = gmb_list_dequeue(&l);
        CHECK(g_freed == 0, "pop/dequeue non rilasciano");
        CHECK(((gmb_obj_t*)p.v_ptr)->refcount == 1 && ((gmb_obj_t*)q.v_ptr)->refcount == 1, "il riferimento passa al chiamante");
        gmb_val_release(p); gmb_val_release(q);
        CHECK(g_freed == 2, "il chiamante rilascia il proprio");
        gmb_list_free(&l);
    }
    /* 5) derivati (copia/slice/union) passano da add: bilanciati */
    {
        g_freed = 0;
        gmb_list_t l = gmb_list_new();
        gmb_obj_t* a = counted_new();
        gmb_list_add(&l, V(a)); drop(a);
        gmb_list_t c = gmb_list_shallow_copy(l);
        gmb_list_t s = gmb_list_slice(l, 0, 1);
        CHECK(a->refcount == 3, "copia e slice trattengono ciascuna");
        gmb_list_free(&l);
        CHECK(g_freed == 0, "liberare la sorgente non invalida le copie");
        gmb_list_free(&c);
        gmb_list_free(&s);
        CHECK(g_freed == 1, "l'ultimo possessore libera");
    }
    /* 6) la copia gmb_make_val_list e' un secondo possessore */
    {
        g_freed = 0;
        gmb_list_t l = gmb_list_new();
        gmb_obj_t* a = counted_new();
        gmb_list_add(&l, V(a)); drop(a);
        gmb_val_t boxed = gmb_make_val_list(l);
        gmb_list_free(&l);
        CHECK(g_freed == 0, "liberare la sorgente non libera cio' che la copia usa");
        gmb_list_t copy = gmb_val_as_list(boxed);        /* gmb_val_as_list e' a sua volta una COPIA (via add): trattiene */
        CHECK(copy.count == 1 && ((gmb_obj_t*)copy.items[0].v_ptr)->refcount == 2, "la vista e' un possessore in piu'");
        gmb_list_free(&copy);
        CHECK(a->refcount == 1 && g_freed == 0, "liberare la vista riporta il conteggio");
    }
    /* 7) in prestito (release_fn NULL) e non-OBJECT: intatti */
    {
        gmb_list_t l = gmb_list_new();
        gmb_obj_t* borrowed = gmb_obj_new(NULL);
        gmb_list_add(&l, V(borrowed));
        gmb_list_add(&l, gmb_make_val_double(3));
        gmb_list_add(&l, gmb_make_val_string("x"));
        gmb_list_set(&l, 0, gmb_make_val_double(1));
        gmb_list_free(&l);
        CHECK(borrowed->refcount == 0, "oggetto in prestito: refcount invariato");
        free(borrowed);
    }

    /* --- COLLEZIONI ------------------------------------------------------------------------ */
    /* 8) Bag (15): add trattiene, free della collezione rilascia */
    {
        g_freed = 0;
        int64_t h = gmb_collections_create(15);
        gmb_obj_t* a = counted_new();
        gmb_coll_add(h, V(a)); drop(a);
        CHECK(g_freed == 0 && a->refcount == 1, "coll_add trattiene");
        gmb_collections_free(h);
        CHECK(g_freed == 1, "collections_free rilascia");
    }
    /* 9) clear rilascia; dequeue (lista linkata) trasferisce */
    {
        g_freed = 0;
        int64_t h = gmb_collections_create(12);
        gmb_obj_t* a = counted_new(); gmb_obj_t* b = counted_new();
        gmb_coll_add(h, V(a)); drop(a);
        gmb_coll_add(h, V(b)); drop(b);
        gmb_val_t t = gmb_coll_dequeue(h);
        CHECK(g_freed == 0 && ((gmb_obj_t*)t.v_ptr)->refcount == 1, "dequeue trasferisce");
        gmb_coll_clear(h);
        CHECK(g_freed == 1, "clear rilascia i rimasti");
        gmb_val_release(t);
        CHECK(g_freed == 2, "il chiamante rilascia il trasferito");
        gmb_collections_free(h);
    }
    /* 10) RingBuffer (20): overwrite del piu' vecchio rilascia; free rilascia solo gli slot VIVI */
    {
        g_freed = 0;
        int64_t h = gmb_collections_create(20);          /* capacita' 64 */
        gmb_obj_t* objs[66];
        for (int i = 0; i < 66; i++) { objs[i] = counted_new(); gmb_coll_enqueue(h, V(objs[i])); drop(objs[i]); }
        CHECK(g_freed == 2, "enqueue su buffer pieno rilascia i 2 piu' vecchi sovrascritti");
        gmb_val_t d = gmb_coll_dequeue(h);               /* trasferisce uno slot */
        gmb_collections_free(h);                         /* slot vivi: 63 */
        CHECK(g_freed == 2 + 63, "free rilascia solo gli slot vivi (no stale)");
        gmb_val_release(d);
        CHECK(g_freed == 66, "tutti liberati esattamente una volta");
    }
    /* 11) mappa kv (8): set / sostituzione / remove / free */
    {
        g_freed = 0;
        int64_t h = gmb_collections_create(8);
        gmb_obj_t* a = counted_new(); gmb_obj_t* b = counted_new(); gmb_obj_t* c = counted_new();
        gmb_coll_kv_set(h, gmb_make_val_int(1), V(a)); drop(a);
        gmb_coll_kv_set(h, gmb_make_val_int(1), V(a));                  /* stesso valore: non lo libera */
        CHECK(g_freed == 0 && a->refcount == 1, "kv_set dello stesso valore non lo libera");
        gmb_coll_kv_set(h, gmb_make_val_int(1), V(b)); drop(b);
        CHECK(g_freed == 1, "kv_set sostituisce e rilascia il vecchio");
        gmb_coll_kv_set(h, gmb_make_val_int(2), V(c)); drop(c);
        gmb_coll_kv_remove(h, gmb_make_val_int(2));
        CHECK(g_freed == 2, "kv_remove rilascia");
        gmb_collections_free(h);
        CHECK(g_freed == 3, "free rilascia il resto");
    }
    /* 12) Cache (16): set / sostituzione / free */
    {
        g_freed = 0;
        int64_t h = gmb_collections_create(16);
        gmb_obj_t* a = counted_new(); gmb_obj_t* b = counted_new();
        gmb_coll_cache_set(h, "k", V(a), 0); drop(a);
        gmb_coll_cache_set(h, "k", V(b), 0); drop(b);
        CHECK(g_freed == 1, "cache_set sostituisce e rilascia il vecchio");
        gmb_collections_free(h);
        CHECK(g_freed == 2, "free rilascia");
    }
    /* --- TAKE: il box adotta lo storage della sorgente (nessuna copia) ------------------------------ */
    {
        g_freed = 0;
        gmb_list_t l = gmb_list_new();
        gmb_obj_t* a = counted_new();
        gmb_list_add(&l, V(a)); drop(a);                         /* solo la lista tiene a (rc 1) */
        gmb_val_t* before = l.items;
        gmb_val_t boxed = gmb_make_val_list_take(&l);
        CHECK(l.items == NULL && l.count == 0 && l.capacity == 0, "take azzera la sorgente");
        gmb_list_t view = *(gmb_list_t*)((gmb_obj_t*)boxed.v_ptr)->data;
        CHECK(view.items == before && view.count == 1, "il box adotta lo stesso array (nessuna copia)");
        CHECK(a->refcount == 1, "take non trattiene: la proprieta' si trasferisce con l'array");
        gmb_list_free(&l);                                       /* no-op sulla sorgente svuotata */
        CHECK(g_freed == 0, "free della sorgente dopo take e' un no-op (nessun double release)");
        gmb_list_t v2 = gmb_val_as_list(boxed);                  /* vista (copia con retain) */
        CHECK(a->refcount == 2, "la vista trattiene");
        gmb_list_free(&v2);
        CHECK(a->refcount == 1 && g_freed == 0, "liberare la vista riporta il conteggio");
        /* take di una lista vuota */
        gmb_list_t e = gmb_list_new();
        gmb_val_t eb = gmb_make_val_list_take(&e);
        CHECK(eb.kind == GMB_TYPE_OBJECT && eb.v_ptr, "take di lista vuota produce un box");
    }
    {
        g_freed = 0;
        gmb_dict_t d = gmb_dict_new();
        gmb_obj_t* a = counted_new();
        gmb_dict_set(&d, "k", V(a)); drop(a);
        gmb_pair_t* before = d.items;
        gmb_val_t boxed = gmb_make_val_dict_take(&d);
        CHECK(d.items == NULL && d.count == 0, "dict take azzera la sorgente");
        gmb_dict_t view = gmb_val_as_dict(boxed);
        CHECK(view.items == before && view.count == 1, "il box dict adotta lo stesso array");
        CHECK(a->refcount == 1, "dict take non trattiene");
        gmb_dict_free(&d);
        CHECK(g_freed == 0, "free della sorgente dict dopo take e' un no-op");
        CHECK(((gmb_obj_t*)gmb_dict_get(view, "k").v_ptr) == a, "valore raggiungibile dal box");
    }
    if (!g_fail) printf("list_ownership_selfcheck: OK\n");
    return g_fail;
}
