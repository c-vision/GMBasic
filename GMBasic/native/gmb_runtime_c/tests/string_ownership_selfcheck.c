/* string_ownership_selfcheck.c — stringhe CONTATE della VM/C dentro liste e dict.
 *
 * Contratto: gmb_make_val_string_counted nasce fluttuante (rc 0); add/set/insert la trattengono
 * (gmb_val_retain), free/clear/remove/sovrascrittura la rilasciano; a 1→0 il buffer e' liberato. Un release
 * a rc 0 non libera (nessun underflow). gmb_val_discard/gmb_string_free del creatore liberano solo a rc 0.
 * NB: il controllo di conteggio usa malloc_zone_statistics → eseguire SENZA -fsanitize; con ASAN (che ha il proprio
 * allocatore) il test verifica solo l'assenza di use-after-free/double-free. Le stringhe normali (senza marcatore) non vengono mai toccate.
 *
 * Build/run (dalla root del repo, dopo aver costruito libgmb_runtime.a — es. via gmb_vm_c/build):
 *   cc -fsanitize=address -g -Inative/gmb_runtime_c/include native/gmb_runtime_c/tests/string_ownership_selfcheck.c \
 *      native/gmb_vm_c/build/gmb_runtime_c_build/libgmb_runtime.a -lm -lpthread \
 *      -framework Cocoa -framework Foundation -framework UserNotifications \
 *      -Wl,-undefined,dynamic_lookup -o /tmp/str_own && /tmp/str_own */
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc/malloc.h>

static int g_fail;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL %s (%s:%d)\n", msg, __FILE__, __LINE__); g_fail = 1; } } while (0)

static size_t in_use(void) { malloc_statistics_t st; malloc_zone_statistics(NULL, &st); return st.size_in_use; }

int main(void)
{
    /* 1. lista: add trattiene, free libera */
    {
        size_t base = in_use();
        for (int i = 0; i < 20000; i++) {
            gmb_list_t l = gmb_list_new();
            gmb_list_add(&l, gmb_make_val_string_counted("alfa"));
            gmb_list_add(&l, gmb_make_val_string_counted_n("beta-xyz", 4));
            CHECK(strcmp(l.items[1].v_string.data, "beta") == 0, "n byte");
            gmb_list_free(&l);
        }
        CHECK(in_use() < base + 64 * 1024, "lista: stringhe contate liberate da gmb_list_free");
    }
    /* 2. condivisa fra due liste (copia per valore): free di entrambe, nessun double-free */
    {
        gmb_list_t a = gmb_list_new(), b = gmb_list_new();
        gmb_list_add(&a, gmb_make_val_string_counted("cond"));
        gmb_list_add(&b, a.items[0]);
        gmb_list_free(&a);
        CHECK(strcmp(b.items[0].v_string.data, "cond") == 0, "b vede ancora la stringa dopo free(a)");
        gmb_list_free(&b);
    }
    /* 3. fluttuante: release a rc 0 non libera */
    {
        gmb_val_t v = gmb_make_val_string_counted("fl");
        gmb_val_release(v);
        CHECK(strcmp(v.v_string.data, "fl") == 0, "release a rc 0 non libera");
        gmb_val_t w = v;
        gmb_list_t t = gmb_list_new();
        gmb_list_add(&t, v);            /* rc 1: trattenuta */
        gmb_val_discard(w);             /* scarto del creatore: NON libera (la tiene la lista) */
        gmb_string_free(&w.v_string);   /* idem: no-op su stringa trattenuta */
        CHECK(strcmp(t.items[0].v_string.data, "fl") == 0, "discard/free del creatore non toccano una stringa trattenuta");
        gmb_list_free(&t);
        gmb_val_t f = gmb_make_val_string_counted("tmp");
        gmb_val_discard(f);             /* fluttuante: liberata */
        gmb_val_t pl = gmb_make_val_string("plain");
        gmb_val_discard(pl);            /* normale: mai toccata */
        CHECK(strcmp(pl.v_string.data, "plain") == 0, "discard non tocca stringhe normali");
        free(pl.v_string.data);
    }
    /* 4. dict: set, sovrascrittura, remove, free */
    {
        size_t base = in_use();
        for (int i = 0; i < 20000; i++) {
            gmb_dict_t d = gmb_dict_new();
            gmb_dict_set(&d, "k", gmb_make_val_string_counted("uno"));
            gmb_dict_set(&d, "k", gmb_make_val_string_counted("due"));
            CHECK(strcmp(gmb_dict_get(d, "k").v_string.data, "due") == 0, "sovrascrittura");
            gmb_dict_set(&d, "r", gmb_make_val_string_counted("tre"));
            gmb_dict_remove(&d, "r");
            gmb_dict_free(&d);
        }
        CHECK(in_use() < base + 64 * 1024, "dict: stringhe contate liberate");
    }
    /* 5. stringhe normali: mai toccate dal contenitore */
    {
        gmb_val_t p = gmb_make_val_string("plain");
        gmb_list_t l = gmb_list_new();
        gmb_list_add(&l, p);
        gmb_list_free(&l);
        CHECK(strcmp(p.v_string.data, "plain") == 0, "stringa normale non liberata dal contenitore");
        free(p.v_string.data);
    }
    /* 6. clear / remove_at / pop (pop cede il riferimento) */
    {
        gmb_list_t l = gmb_list_new();
        gmb_list_add(&l, gmb_make_val_string_counted("a"));
        gmb_list_add(&l, gmb_make_val_string_counted("b"));
        gmb_list_add(&l, gmb_make_val_string_counted("c"));
        gmb_val_t popped = gmb_list_pop(&l);
        CHECK(strcmp(popped.v_string.data, "c") == 0, "pop");
        gmb_val_release(popped);   /* il chiamante rilascia il riferimento ceduto */
        gmb_list_clear(&l);
        gmb_list_free(&l);
    }
    if (!g_fail) printf("string_ownership_selfcheck OK\n");
    return g_fail;
}
