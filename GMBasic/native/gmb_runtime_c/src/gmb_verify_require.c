/* gmb_verify_require.c — VALUTATORE del REQUIRE (AI VERIFY … REQUIRE e loop verificato del
 * worker), in un'unità di traduzione SEPARATA dal resto di gmb_verify.c.
 *
 * PERCHÉ SEPARATO: gmb_verify.c contiene anche l'azione "COMPILE <path>" che referenzia il
 * compilatore C (gmb_compile_bas_file). Chi usa solo il valutatore (es. AI WORKER LOOP, che non
 * compila nulla) non deve trascinare quella dipendenza nel link: tenendolo in un file dedicato,
 * il linker C99Native/executor non pulla l'oggetto del COMPILE.
 *
 * Semantica (condivisa, una sola implementazione — §0bis): l'espressione BASIC `require` è
 * valutata dal valutatore runtime condiviso (gmb_expr_eval_ex) con RESULT$ (e RESULT) = result.
 * Ritorna 1 se la condizione è vera, 0 altrimenti (incluso il caso di espressione non valutabile).
 */
#include "gmb_runtime.h"
#include "gmb_expr.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Env per la valutazione del REQUIRE: espone RESULT$ e RESULT. */
typedef struct { const char* result; gmb_dict_t* locals; } gmb_verify_env_t;

static int gmb_verify_env_get(void* env, const char* name, gmb_val_t* out)
{
    gmb_verify_env_t* e = (gmb_verify_env_t*)env;
    if (strcasecmp(name, "RESULT$") == 0 || strcasecmp(name, "RESULT") == 0)
    {
        *out = gmb_make_val_string(e->result ? e->result : "");
        return 0;   /* convenzione gmb_expr: 0 = trovato */
    }
    /* LOCALI DEL CHIAMANTE (parità C# MacroEvalExprWithLocals): il REQUIRE può usare le
     * variabili in scope al sito di AI VERIFY (es. `LEN(RESULT$) > n - 8` con n locale).
     * Prima l'env esponeva solo RESULT$ → il C dava REQUIRE falso dove il C# vero. */
    if (e->locals)
    {
        gmb_val_t lv = gmb_dict_get(*e->locals, name);
        if (lv.kind != GMB_TYPE_NIL) { *out = lv; return 0; }
    }
    return -1;
}

static int gmb_verify_env_set(void* env, const char* name, gmb_val_t in)
{
    (void)env; (void)name; (void)in;
    return -1;   /* read-only: nessuna assegnazione */
}

static int gmb_verify_truthy(gmb_val_t v)
{
    switch (v.kind)
    {
        case GMB_TYPE_BOOL:   return v.v_bool != 0;
        case GMB_TYPE_INT:    return v.v_int != 0;
        case GMB_TYPE_DOUBLE: return v.v_double != 0.0;
        case GMB_TYPE_STRING:
            return v.v_string.data && v.v_string.data[0] != '\0'
                && strcmp(v.v_string.data, "0") != 0
                && strcasecmp(v.v_string.data, "false") != 0;
        default: return 0;
    }
}

/* Valuta `require` (espressione BASIC) con RESULT$ = result. 1 = vero, 0 = falso. */
static int gmb_verify_require_holds(const char* require, const char* result, gmb_dict_t* locals)
{
    if (!require || !*require) return 1;
    gmb_verify_env_t env = { result ? result : "", locals };
    gmb_val_t v = gmb_val_nil();
    /* `gmb_expr_eval_filter_ex` e NON `_ex`: in un REQUIRE `=` è UGUAGLIANZA, non
     * assegnazione. Il valutatore generico (`_ex`, semantica di & /EVAL) tratta `=` come LET
     * e il valore assegnato (una stringa non vuota) è truthy → un REQUIRE come
     * `RESULT$ = "zzz"` risultava VERO sul C e FALSO sul C# (`=` confronto) — silent-wrong su
     * un gate di accettazione. La variante filter ha `eq_is_assign=0` ed `as_stmt=0`
     * (espressione), la stessa semantica del valutatore C# (`MacroEvalExprWithLocals`). */
    int rc = gmb_expr_eval_filter_ex(require, &env, gmb_verify_env_get, gmb_verify_env_set, NULL, &v);
    if (rc != GMB_EXPR_OK) return 0;
    return gmb_verify_truthy(v);
}

/* Valutatore CONDIVISO: `result` è esposto come RESULT$/RESULT all'espressione.
 * 1 = condizione soddisfatta, 0 = no (anche per espressione non valutabile). */
int gmb_verify_require_ex(const char* require, const char* result, gmb_dict_t* locals)
{
    return gmb_verify_require_holds(require, result, locals);
}
int gmb_verify_require(const char* require, const char* result)
{
    return gmb_verify_require_holds(require, result, NULL);
}
