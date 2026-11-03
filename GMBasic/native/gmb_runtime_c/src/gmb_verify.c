/* gmb_verify.c — AI VERIFY ... REQUIRE (verifier/retry) — Orchestrazione agenti (B2), mondo C.
 *
 * Replica la semantica di Basic.IlBackend.IlRuntime.AiVerify (C#): ogni tentativo esegue
 * `action`; se produce un errore si ritenta; altrimenti `require` (espressione BASIC valutata
 * con RESULT$ = risultato del tentativo) deve dare true. Dopo maxRetries fallimento ESPLICITO.
 *
 * Azione v1: "COMPILE <path>" — compila il file con il COMPILATORE C (gmb_compile_bas_file).
 * Il riferimento al compilatore è DEBOLE (weak): se il compilatore C è collegato (VM/C host,
 * executor, C99Native) il COMPILE funziona; altrimenti si ritorna un errore ESPLICITO (mai
 * uno stub silenzioso, AGENTS §0bis). Il REQUIRE è valutato con il valutatore runtime condiviso
 * gmb_expr_eval_ex (parità MacroEvalExpr C#).
 *
 * Runtime CONDIVISO (AGENTS §0bis): VM/C, C99Native usano le stesse gmb_verify_*.
 */
#include "gmb_runtime.h"
#include "gmb_eval_scope.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>

/* Compilatore C (native/gmb_compiler_c): riferimento FORTE. gmb_verify.o viene pullato dalla
 * static lib SOLO se il programma usa gmb_verify_run → chi usa AI VERIFY deve linkare il
 * compilatore C (host VM/C, gate C99, harness C99Native). Se manca, è un ERRORE di link
 * esplicito (mai uno stub silenzioso). NB: su Darwin il weak-undefined non linka senza flag,
 * perciò niente __attribute__((weak)). */
extern int gmb_compile_bas_file(const char* path, uint8_t** out_data, size_t* out_len, char** diag);

/* Esegue un tentativo dell'azione. Ritorna malloc("Compile OK") su successo, oppure malloc
 * di un messaggio d'errore. `*is_error` = 1 se errore. */
static char* gmb_verify_attempt(const char* action, int* is_error)
{
    *is_error = 0;
    const char* t = action ? action : "";
    while (*t == ' ' || *t == '\t') t++;
    /* Azione v1: "COMPILE <path>" (case-insensitive). */
    if (strncasecmp(t, "COMPILE", 7) == 0 && (t[7] == ' ' || t[7] == '\t'))
    {
        const char* p = t + 7;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p)
        {
            *is_error = 1;
            return strdup("COMPILE: path mancante");
        }
        uint8_t* data = NULL; size_t len = 0; char* diag = NULL;
        int rc = gmb_compile_bas_file(p, &data, &len, &diag);
        if (data) free(data);
        if (rc == 0) return strdup("Compile OK");
        *is_error = 1;
        char* msg = (char*)malloc(strlen(diag ? diag : "errore di compilazione") + 16);
        if (msg) snprintf(msg, strlen(diag ? diag : "errore") + 16, "%s", diag ? diag : "errore di compilazione");
        if (diag) free(diag);
        return msg;
    }
    *is_error = 1;
    char* msg = (char*)malloc(strlen(action ? action : "") + 96);
    if (msg) snprintf(msg, strlen(action ? action : "") + 96,
        "azione non supportata '%s' (v1 supporta solo 'COMPILE <path>')", action ? action : "");
    return msg;
}

/* AI VERIFY <action> REQUIRE <require> [maxRetries=3]. Ritorna malloc:
 * - successo: il risultato del tentativo accettato (es. "Compile OK");
 * - fallimento: stringa "Error: ...". Il chiamante (builtin) può decidere se propagare. */
/* Variante con i LOCALI del chiamante passati come COPPIE (nome, valore) in due array
 * paralleli. Esiste per i backend che emettono C (C99Native): costruire un gmb_dict_t in
 * una statement-expression `({ … })` nel punto di chiamata rendeva l'emitter instabile su
 * più locali; con due compound-literal il codice generato resta un'espressione pura.
 * Semantica identica a gmb_verify_run_ex(action, require, retries, &dict). */
char* gmb_verify_run_named(const char* action, const char* require, int max_retries,
                           int n_locals, const char* const* names, const gmb_val_t* vals)
{
    gmb_dict_t lv = gmb_dict_new();
    if (names && vals && n_locals > 0)
        for (int i = 0; i < n_locals; i++)
            if (names[i] && names[i][0]) gmb_dict_set(&lv, names[i], vals[i]);
    char* res = gmb_verify_run_ex(action, require, max_retries, n_locals > 0 ? &lv : NULL);
    gmb_dict_free(&lv);
    return res;
}

char* gmb_verify_run_ex(const char* action, const char* require, int max_retries, gmb_dict_t* locals)
{
    if (max_retries < 1) max_retries = 1;
    if (max_retries > 100) max_retries = 100;
    char* last = NULL;
    for (int attempt = 1; attempt <= max_retries; attempt++)
    {
        int is_error = 0;
        char* result = gmb_verify_attempt(action, &is_error);
        if (!is_error)
        {
            if (gmb_verify_require_ex(require, result, locals))
            {
                if (last) free(last);
                return result;
            }
            if (last) free(last);
            last = (char*)malloc(strlen(result) + 64);
            if (last) snprintf(last, strlen(result) + 64,
                "Tentativo %d: il risultato non soddisfa il REQUIRE.", attempt);
            free(result);
        }
        else
        {
            if (last) free(last);
            last = (char*)malloc(strlen(result) + 64);
            if (last) snprintf(last, strlen(result) + 64, "Tentativo %d fallito: %s", attempt, result);
            free(result);
        }
    }
    char* out = (char*)malloc((last ? strlen(last) : 0) + 64);
    if (out) snprintf(out, (last ? strlen(last) : 0) + 64, "Error: AI VERIFY fallito dopo %d tentativi: %s",
        max_retries, last ? last : (action ? action : ""));
    if (last) free(last);
    return out;
}

char* gmb_verify_run(const char* action, const char* require, int max_retries)
{
    return gmb_verify_run_ex(action, require, max_retries, NULL);
}
