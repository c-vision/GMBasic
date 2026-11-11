/* FILE: native/gmb_runtime_c/tests/test_abi_env_precedence.c
 *
 * `$LLMDESC_ABI_DIR` VINCE sulla scoperta interna della cartella ABI, e non viene mai
 * scritta nei settings.
 *
 * PERCHÉ ESISTE (2026-11-11). La prova end-to-end manuale (MiniTest `llm/llm_arch_check` con
 * una COPIA dell'ABI in /tmp privata dei descrittori di `qwen3_moe`) mostrava il passaggio da
 * `0` a `9 tensori senza ruolo`: il comando AI leggeva la cartella dell'ambiente, non quella
 * che avrebbe risolto da sé. Quella prova però dipendeva da STATO ESTERNO (una copia preparata
 * a mano e una variabile esportata nella shell) e non era ripetibile. Qui la verifica è
 * permanente e non richiede né LLMEnv installato né modelli: due cartelle ABI finte in una dir
 * temporanea e l'osservazione della POLICY.
 *
 * PERCHÉ IN C E NON UN `.bas` MINITEST. La cartella ABI la risolvono i QUATTRO backend con
 * codici separati (§0bis: il C# ha il suo, il C ha `gmb_abi.c`) e una variabile d'ambiente non
 * si imposta da BASIC: un `.bas` dipenderebbe dall'ambiente del runner e fallirebbe dove
 * `LLMDESC_ABI_DIR` non c'è. La REGOLA sta nel test C, dove la politica è una sola.
 *
 * COSA VERIFICA
 *   1. `llmdesc_abi_locate` con `$LLMDESC_ABI_DIR` valida: fonte dichiarata `LLMDESC_ABI_DIR` e
 *      cartella = quella dell'ambiente;
 *   2. `gmb_abi_dir` (il resolver del runtime) restituisce la stessa cartella: la precedenza vale
 *      anche passando dalla policy dell'applicazione;
 *   3. quella cartella NON viene copiata in `app.settings.json`: l'ambiente resta per-processo;
 *   4. con l'ambiente NON valido (cartella esistente senza `*.gmbarch`) la scoperta riprende: la
 *      fonte non è più `LLMDESC_ABI_DIR` e un resolver che trova un'altra cartella la usa,
 *      scrivendola nei settings dell'APPLICAZIONE (comportamento voluto, per-app: D16).
 *
 * NIENTE STATO CONDIVISO: tutto vive sotto `$TMPDIR` e viene rimosso a fine test.
 */
#include "../src/gmb_abi.c"
#include "llmdesc.h"
#include <assert.h>
#include <sys/stat.h>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void put(const char* path, const char* body) { FILE* f = fopen(path, "wb"); assert(f); fputs(body, f); fclose(f); }
static void reset(void) { g_abi_done = 0; g_abi_busy = 0; g_abi_dir[0] = 0; }

/* Confronto di cartelle RISOLTE: `$TMPDIR` su macOS è `/var/folders/...`, che è un link a
 * `/private/var/...`; la risoluzione di `llmdesc` può normalizzare, quindi il confronto testo
 * non basta (e un FAIL per un link sarebbe un falso rosso). */
static int same_dir(const char* a, const char* b)
{
    char ra[4096], rb[4096];
    const char* pa = (a && *a && realpath(a, ra)) ? ra : a;
    const char* pb = (b && *b && realpath(b, rb)) ? rb : b;
    return pa && pb && strcmp(pa, pb) == 0;
}

/* Scoperta "interna" finta: deve PERDERE contro l'ambiente quando l'ambiente è valido, e
 * fornire la cartella alternativa quando l'ambiente non lo è. */
static int g_locate_hits = 0;
static char g_locate_dir[2048];
static int stub_locate(char* out, size_t cap, char* how, size_t howcap)
{
    g_locate_hits++;
    if (!g_locate_dir[0]) return 0;
    snprintf(out, cap, "%s", g_locate_dir);
    if (how) snprintf(how, howcap, "%s", "stub");
    return 1;
}

int main(void)
{
    /* In produzione i tre servizi ABI li registra la dylib `gmb_runtime_ai`; questo test linka
     * `llmdesc.c`, quindi fa la stessa registrazione, come `test_abi_settings`. */
    gmb_abi_set_llmdesc(llmdesc_abi_valid, llmdesc_abi_locate, llmdesc_set_abi_policy);
    llmdesc_set_abi_policy(gmb_abi_dir, 1);

    char tmpl[1024];
    snprintf(tmpl, sizeof tmpl, "%s/gmb_abi_env_XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    char* root = mkdtemp(tmpl);
    if (!root) return 77;   /* niente dir temporanea: non è un fallimento della regola */

    char work[1200], envabi[1200], other[1200], empty[1200], cmd[4000], out[2048], how[128], v[512];
    snprintf(work, sizeof work, "%s/work", root);
    snprintf(envabi, sizeof envabi, "%s/env_abi", root);
    snprintf(other, sizeof other, "%s/other_abi", root);
    snprintf(empty, sizeof empty, "%s/empty_abi", root);
    snprintf(cmd, sizeof cmd, "mkdir -p '%s' '%s' '%s' '%s'", work, envabi, other, empty);
    (void)system(cmd);
    snprintf(cmd, sizeof cmd, "%s/env_model.gmbarch", envabi); put(cmd, "{}\n");
    snprintf(cmd, sizeof cmd, "%s/other_model.gmbarch", other); put(cmd, "{}\n");
    if (chdir(work) != 0) return 77;
    remove("app.settings.json");

    /* ---- 1. l'ambiente VINCE: la fonte dichiarata è LLMDESC_ABI_DIR ---- */
    setenv("LLMDESC_ABI_DIR", envabi, 1);
    how[0] = '\0';
    CHECK(llmdesc_abi_locate(out, sizeof out, how, sizeof how) == 1);
    CHECK(same_dir(out, envabi));
    CHECK(strcmp(how, "LLMDESC_ABI_DIR") == 0);

    /* ---- 2. la stessa cartella esce dal resolver del runtime ---- */
    reset();
    CHECK(gmb_abi_dir(out, sizeof out) == 1);
    CHECK(same_dir(out, envabi));

    /* ---- 3. ... e NON finisce nei settings: l'ambiente resta per-processo ---- */
    CHECK(access("app.settings.json", F_OK) != 0);
    CHECK(abi_file_get("app.settings.json", v, sizeof v) == 0);

    /* ---- 4. ambiente NON valido (cartella senza descrittori): la scoperta riprende ---- */
    setenv("LLMDESC_ABI_DIR", empty, 1);
    how[0] = '\0';
    CHECK(llmdesc_abi_locate(out, sizeof out, how, sizeof how) == 0 || strcmp(how, "LLMDESC_ABI_DIR") != 0);

    /* Il resolver dell'applicazione, con l'ambiente ormai inutilizzabile, usa la cartella trovata
     * dalla scoperta e la scrive nei settings dell'APPLICAZIONE (per-app: D16). */
    gmb_abi_set_llmdesc(llmdesc_abi_valid, stub_locate, llmdesc_set_abi_policy);
    snprintf(g_locate_dir, sizeof g_locate_dir, "%s", other);
    g_locate_hits = 0;
    reset();
    CHECK(gmb_abi_dir(out, sizeof out) == 1);
    CHECK(same_dir(out, other));
    CHECK(g_locate_hits > 0);
    CHECK(abi_file_get("app.settings.json", v, sizeof v) == 1 && same_dir(v, other));

    unsetenv("LLMDESC_ABI_DIR");
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", root);
    (void)system(cmd);
    if (g_fail == 0) printf("test_abi_env_precedence: OK\n");
    return g_fail ? 1 : 0;
}
