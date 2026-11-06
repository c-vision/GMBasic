/* FILE: native/gmb_runtime_c/tests/gmb_opt_moe_swap_selfcheck.c
 *
 * Selfcheck del lettore `gmb_opt_moe_swap_dir` (opzione `moe_swap_dir` del registro).
 * Model-free: nessun modello e nessuna rete — si provano il registro opzioni e l'arrivo della
 * voce al motore che apre i pesi (dylib LLMEnv, caricata a runtime).
 *
 * Parte 1 — il lettore: vuoto -> impostato -> svuotato, troncamento sicuro, argomenti non
 *           validi, indipendenza dall'ambiente.
 * Parte 2 — il ponte: la voce impostata nel registro, letta con `gmb_opt_moe_swap_dir`
 *           (esportata da questo eseguibile, quindi visibile al motore come nell'host reale),
 *           arriva al gate che decide lo streaming degli esperti prima di `load_model`. Se il
 *           motore non è installato il caso è SALTATO con un messaggio, mai silenzioso.
 * Exit 0 = PASS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmb_runtime.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

/* Lettura con out azzerato prima della chiamata: se il lettore non scrivesse nulla, il test
 * vedrebbe comunque una stringa vuota e non un residuo del caso precedente. */
static int read_dir(char* out, int cap) {
    out[0] = '\0';
    return gmb_opt_moe_swap_dir(out, cap);
}

/* ── Parte 2: la voce del registro arriva al motore che apre i pesi ───────────────────────────
 * Il motore (LLMEnv) non si linka: è una dylib caricata a runtime, come fa il consumer. Qui NON
 * si carica un modello: si verifica solo che il registro, letto dal motore con
 * `gmb_opt_moe_swap_dir`, cambi lo stato dello swap — la stessa sequenza che il load MLX esegue
 * all'apertura dei pesi (`llm_mlx_moe_swap_apply_registry`). Motore assente o privo del ponte =
 * caso saltato con messaggio esplicito. */
#ifndef _WIN32
static int check_registry_reaches_engine(void) {
    static const char* kNames[] = { "libgmb_ai_mlx.dylib", "libllm_ai_mlx.dylib" };
    char dir[2048], cand[2400];
    void* h = NULL;

    for (int n = 0; n < 2 && h == NULL; n++) {
        gmb_paths_libs_cache_dir(dir, sizeof dir);              /* ~/.gmbasic/libs (runtime ML) */
        snprintf(cand, sizeof cand, "%s/%s/%s", dir, gmb_rid(), kNames[n]);
        h = dlopen(cand, RTLD_NOW | RTLD_LOCAL);
        if (h == NULL) {
            gmb_paths_ml_dir(gmb_rid(), dir, sizeof dir);        /* libsDir/ml/<rid> */
            snprintf(cand, sizeof cand, "%s/%s", dir, kNames[n]);
            h = dlopen(cand, RTLD_NOW | RTLD_LOCAL);
        }
    }
    if (h == NULL) {
        puts("SKIP: motore MLX non installato (nessun libgmb_ai_mlx/libllm_ai_mlx nei runtime ML)");
        return 0;
    }

    int         (*apply)(void);
    int         (*on)(void);
    const char* (*cur)(void);
    void        (*set)(const char*);
    *(void**)(&apply) = dlsym(h, "llm_mlx_moe_swap_apply_registry");
    *(void**)(&on)    = dlsym(h, "llm_mlx_moe_swap_on");
    *(void**)(&cur)   = dlsym(h, "llm_mlx_moe_swap_dir");
    *(void**)(&set)   = dlsym(h, "llm_ai_mlx_set_moe_swap_dir");
    if (apply == NULL || on == NULL || cur == NULL || set == NULL) {
        puts("SKIP: motore MLX senza il ponte registro (dylib precedente a gmb_opt_moe_swap_dir)");
        return 0;
    }

    /* 7. La voce impostata nel registro accende lo swap nel motore, con lo STESSO valore. */
    CHECK(gmb_opt_set("moe_swap_dir", "work/experts") == 0, "7 registro impostato");
    CHECK(apply() == 1, "7 il motore applica la voce (stato cambiato)");
    CHECK(on() == 1, "7 swap acceso nel motore");
    CHECK(strcmp(cur(), "work/experts") == 0, "7 dir arrivata identica al motore");

    /* 8. Rilettura senza modifiche: il motore non riapplica (idempotente, niente reset di cache). */
    CHECK(apply() == 0 && strcmp(cur(), "work/experts") == 0, "8 idempotente");

    /* 9. Voce svuotata: il motore torna resident (streaming spento), senza residui. */
    CHECK(gmb_opt_set("moe_swap_dir", "") == 0, "9 registro svuotato");
    CHECK(apply() == 1 && on() == 0 && cur()[0] == '\0', "9 motore torna resident");

    /* 10. Precedenza: una scelta esplicita del consumer vince sulla voce del registro. */
    set("/scelta-consumer");
    CHECK(gmb_opt_set("moe_swap_dir", "/models/registro") == 0, "10 registro impostato");
    CHECK(apply() == 0 && strcmp(cur(), "/scelta-consumer") == 0, "10 vince il consumer");

    /* Chiusura pulita: motore spento e registro vuoto, come all'avvio. */
    set(NULL);
    CHECK(gmb_opt_set("moe_swap_dir", "") == 0, "10 registro vuoto finale");
    return 1;
}
#endif

int main(void) {
    char buf[1024];

    /* ── Parte 1: il lettore del registro ───────────────────────────────────────────────────── */

    /* 1. VUOTO: voce svuotata -> "" (streaming spento), mai un errore. */
    CHECK(gmb_opt_set("moe_swap_dir", "") == 0, "1 set vuoto");
    CHECK(read_dir(buf, sizeof buf) == 0, "1 rc");
    CHECK(buf[0] == '\0', "1 vuoto -> stringa vuota");
    {   char same[64] = "x";
        CHECK(gmb_opt_get("moe_swap_dir", same, sizeof same) == 0 && same[0] == '\0',
              "1 coerente con gmb_opt_get (AI.Option)"); }

    /* 2. IMPOSTATO: la dir torna identica (è il valore applicato prima del load). */
    CHECK(gmb_opt_set("moe_swap_dir", "work/experts") == 0, "2 set");
    CHECK(read_dir(buf, sizeof buf) == 0, "2 rc");
    CHECK(strcmp(buf, "work/experts") == 0, "2 valore");
    CHECK(gmb_opt_get("moe_swap_dir", buf, sizeof buf) == 0 && strcmp(buf, "work/experts") == 0,
          "2 registro coerente");

    /* 3. SVUOTATO: torna spento, senza residui del valore precedente. */
    CHECK(gmb_opt_set("moe_swap_dir", "") == 0, "3 svuota");
    CHECK(read_dir(buf, sizeof buf) == 0, "3 rc");
    CHECK(buf[0] == '\0', "3 nessun residuo");

    /* 4. TRONCAMENTO SICURO: cap piccolo -> prefisso + NUL, niente scritture oltre `cap`. */
    {   char small[16];
        memset(small, 0x7f, sizeof small);
        CHECK(gmb_opt_set("moe_swap_dir", "work/experts") == 0, "4 set");
        CHECK(gmb_opt_moe_swap_dir(small, 8) == 0, "4 rc");
        CHECK(strcmp(small, "work/ex") == 0, "4 prefisso");
        CHECK(small[7] == '\0' && (unsigned char)small[8] == 0x7f, "4 nessuna scrittura oltre cap"); }

    /* 5. ARGOMENTI NON VALIDI: errore esplicito, nessun crash. */
    CHECK(gmb_opt_moe_swap_dir(NULL, 16) == -1, "5 out NULL");
    CHECK(gmb_opt_moe_swap_dir(buf, 0) == -1, "5 cap 0");
    CHECK(gmb_opt_moe_swap_dir(buf, -4) == -1, "5 cap negativo");

    /* 6. NON viene dall'ambiente: un `moe_swap_dir` esportato nel processo non popola la voce.
     *    (Il test esporta la variabile SOLO per dimostrare l'indipendenza: il lettore non ha
     *    alcun `getenv`, e se un domani ne rientrasse uno questo caso diventerebbe rosso.) */
    CHECK(gmb_opt_set("moe_swap_dir", "") == 0, "6 svuota");
#ifdef _WIN32
    _putenv_s("moe_swap_dir", "/da-env");
#else
    setenv("moe_swap_dir", "/da-env", 1);
#endif
    CHECK(read_dir(buf, sizeof buf) == 0 && buf[0] == '\0', "6 env ignorato");
#ifdef _WIN32
    _putenv_s("moe_swap_dir", "");
#else
    unsetenv("moe_swap_dir");
#endif

    /* ── Parte 2: la voce arriva al motore che apre i pesi ──────────────────────────────────── */
#ifdef _WIN32
    puts("SKIP: ponte registro -> motore non provato su Windows (dlopen non disponibile)");
#else
    check_registry_reaches_engine();
#endif

    if (fails) {
        printf("gmb_opt_moe_swap_selfcheck: %d FAIL\n", fails);
        return 1;
    }
    puts("gmb_opt_moe_swap_selfcheck OK (vuoto, impostato, svuotato, troncamento, argomenti, "
         "env ignorato, arrivo al motore)");
    return 0;
}
