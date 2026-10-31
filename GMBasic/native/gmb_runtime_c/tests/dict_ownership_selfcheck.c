/* dict_ownership_selfcheck.c — ownership dei valori OBJECT contati nei gmb_dict_t.
 *
 * Contratto verificato (gmb_dict.c): un dict POSSIEDE i valori OBJECT con release_fn != NULL
 * (oggetti contati della VM): set li trattiene, free/clear/remove/sostituzione li rilasciano; la
 * copia (gmb_make_val_dict) e' un secondo possessore. Gli oggetti "in prestito" (release_fn NULL:
 * transpiler / box runtime) non vengono mai toccati.
 *
 * Build/run (dalla root del repo, dopo aver costruito libgmb_runtime.a — es. via gmb_vm_c/build):
 *   cc -fsanitize=address -g -Inative/gmb_runtime_c/include native/gmb_runtime_c/tests/dict_ownership_selfcheck.c \
 *      native/gmb_vm_c/build/gmb_runtime_c_build/libgmb_runtime.a -lm -lpthread \
 *      -framework Cocoa -framework Foundation -framework UserNotifications \
 *      -Wl,-undefined,dynamic_lookup -o /tmp/dict_own && /tmp/dict_own
 * (con ASAN un double-free o un use-after-free dei valori fa fallire il test). */
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static int g_freed;

#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL %s (%s:%d)\n", msg, __FILE__, __LINE__); g_fail = 1; } } while (0)

static void counted_release(struct gmb_obj* o) { g_freed++; free(o); }

/* oggetto contato come quelli della VM: refcount 1 (il riferimento del creatore) + release_fn */
static gmb_obj_t* counted_new(void)
{
    gmb_obj_t* o = (gmb_obj_t*)calloc(1, sizeof(gmb_obj_t));
    o->refcount = 1;
    o->release_fn = counted_release;
    return o;
}
static void drop(gmb_obj_t* o) { if (--o->refcount == 0) o->release_fn(o); }   /* come gmb_obj_release */

int main(void)
{
    /* 1) set trattiene, free rilascia: la sorgente puo' morire PRIMA del dict (bug "backends": "") */
    {
        gmb_dict_t d = gmb_dict_new();
        gmb_obj_t* o = counted_new();
        gmb_dict_set(&d, "k", gmb_make_val_obj(o));
        CHECK(o->refcount == 2, "set trattiene (refcount 2)");
        drop(o);                                  /* il creatore lascia andare il suo riferimento */
        CHECK(g_freed == 0, "il dict tiene vivo l'oggetto dopo che il creatore l'ha rilasciato");
        CHECK(((gmb_obj_t*)gmb_dict_get(d, "k").v_ptr)->refcount == 1, "valore ancora valido e posseduto dal dict");
        gmb_dict_free(&d);
        CHECK(g_freed == 1, "free del dict rilascia il valore posseduto");
    }
    /* 2) sostituzione: rilascia il vecchio; set(k, d[k]) non libera il valore che sta reinserendo */
    {
        g_freed = 0;
        gmb_dict_t d = gmb_dict_new();
        gmb_obj_t* a = counted_new(); gmb_obj_t* b = counted_new();
        gmb_dict_set(&d, "k", gmb_make_val_obj(a)); drop(a);        /* ora solo il dict tiene a */
        gmb_dict_set(&d, "k", gmb_make_val_obj(a));                 /* a e' del dict (refcount 1): reinserire lo stesso */
        CHECK(g_freed == 0 && a->refcount == 1, "reinserire lo stesso valore non lo libera");
        gmb_dict_set(&d, "k", gmb_make_val_obj(b)); drop(b);
        CHECK(g_freed == 1, "sostituire rilascia il vecchio valore");
        gmb_dict_free(&d);
        CHECK(g_freed == 2, "free rilascia il valore corrente");
    }
    /* 3) remove / clear rilasciano */
    {
        g_freed = 0;
        gmb_dict_t d = gmb_dict_new();
        gmb_obj_t* a = counted_new(); gmb_obj_t* b = counted_new();
        gmb_dict_set(&d, "a", gmb_make_val_obj(a)); drop(a);
        gmb_dict_set(&d, "b", gmb_make_val_obj(b)); drop(b);
        CHECK(gmb_dict_remove(&d, "a"), "remove trova la chiave");
        CHECK(g_freed == 1, "remove rilascia il valore");
        gmb_dict_clear(&d);
        CHECK(g_freed == 2, "clear rilascia i valori rimasti");
    }
    /* 4) oggetti in prestito (release_fn NULL) e valori non-OBJECT: mai toccati */
    {
        gmb_dict_t d = gmb_dict_new();
        gmb_obj_t* borrowed = gmb_obj_new(NULL);       /* refcount 0, release_fn NULL: modello transpiler */
        gmb_dict_set(&d, "o", gmb_make_val_obj(borrowed));
        gmb_dict_set(&d, "n", gmb_make_val_double(3));
        gmb_dict_set(&d, "s", gmb_make_val_string("x"));
        gmb_dict_free(&d);
        CHECK(borrowed->refcount == 0, "oggetto in prestito: refcount invariato");
        free(borrowed);
    }
    /* 5) la copia (gmb_make_val_dict) e' un secondo possessore: liberare la sorgente non la invalida */
    {
        g_freed = 0;
        gmb_dict_t src = gmb_dict_new();
        gmb_obj_t* o = counted_new();
        gmb_dict_set(&src, "k", gmb_make_val_obj(o)); drop(o);      /* solo src lo tiene (refcount 1) */
        gmb_val_t boxed = gmb_make_val_dict(src);                   /* la copia trattiene (refcount 2) */
        gmb_dict_free(&src);                                        /* src muore: refcount 1, NON liberato */
        CHECK(g_freed == 0, "liberare la sorgente non libera cio' che la copia usa");
        gmb_dict_t copy = gmb_val_as_dict(boxed);
        CHECK(copy.count == 1 && ((gmb_obj_t*)copy.items[0].value.v_ptr)->refcount == 1, "la copia vede ancora un oggetto valido");
    }
    if (!g_fail) printf("dict_ownership_selfcheck: OK\n");
    return g_fail;
}
