/* test_abi_settings.c — cartella ABI di LLMEnv: lettura/scrittura testuale di paths.abiDir (JSONC con commenti), risoluzione
 * (impostazione valida, percorso obsoleto, prima esecuzione con persistenza, LLMEnv assente). Nessun modello necessario.
 * Include il .c per provare anche le funzioni statiche. Il "brew" e' un finto script nel PATH: nessuna installazione reale. */
#include "../src/gmb_abi.c"
/* Lo split core/AI (2026-11-02) ha spostato l'ABI `llmdesc` nella dylib `gmb_runtime_ai`: il core
 * (`gmb_abi.c`) la include solo su _WIN32, dove non c'e' dynamic_lookup. Il test invece usa
 * direttamente `llmdesc_abi_dir`/`llmdesc_resolve` e linka `llmdesc.c` (CMakeLists): l'header va
 * incluso qui, esplicitamente, per tutti gli altri piattaforme. */
#include "llmdesc.h"
#include <assert.h>
#include <sys/wait.h>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void put(const char* path, const char* body) { FILE* f = fopen(path, "wb"); assert(f); fputs(body, f); fclose(f); }
static char* get(const char* path) { size_t n; return abi_slurp(path, &n); }
static void reset(void) { g_abi_done = 0; g_abi_busy = 0; g_abi_dir[0] = 0; }

int main(void)
{
    /* SPLIT core/AI (2026-11-02): in produzione i tre servizi ABI li registra la dylib
     * `gmb_runtime_ai` (constructor in gmb_ai_register.c). Il test linka `llmdesc.c`
     * direttamente, quindi fa la stessa registrazione qui, prima di ogni uso. */
    gmb_abi_set_llmdesc(llmdesc_abi_valid, llmdesc_abi_locate, llmdesc_set_abi_policy);
    /* La policy ("la cartella ABI la risolve l'applicazione") la registra il constructor di
     * gmb_abi.c, che qui gira PRIMA di questa registrazione: la si applica quindi esplicitamente,
     * come farebbe il core quando i servizi sono gia' presenti. */
    llmdesc_set_abi_policy(gmb_abi_dir, 1);
    /* sotto $TMPDIR (non /tmp): una cartella ABI/ dispersa in un antenato (es. /tmp/ABI) falserebbe la ricerca risalendo dalla cwd */
    char tmpl[1024]; snprintf(tmpl, sizeof tmpl, "%s/gmb_abi_test_XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp"); char* root = mkdtemp(tmpl);
    if (!root) return 77;
    char app[1200], fake[1200], good[1200], prefix[1200], bindir[1200], cmd[3000];
    snprintf(good, sizeof good, "%s/llmenv_a/ABI", root);
    snprintf(prefix, sizeof prefix, "%s/cellar", root);
    snprintf(bindir, sizeof bindir, "%s/bin", root);
    snprintf(cmd, sizeof cmd, "mkdir -p '%s' '%s/lib/llmenv/ABI' '%s' '%s/work'", good, prefix, bindir, root); (void)system(cmd);
    snprintf(cmd, sizeof cmd, "%s/x.gmbarch", good); put(cmd, "{}");
    snprintf(cmd, sizeof cmd, "%s/lib/llmenv/ABI/y.gmbarch", prefix); put(cmd, "{}");
    snprintf(fake, sizeof fake, "%s/bin/brew", root);
    snprintf(cmd, sizeof cmd, "#!/bin/sh\n[ \"$1\" = \"--prefix\" ] && echo '%s'\n", prefix); put(fake, cmd); chmod(fake, 0755);
    snprintf(app, sizeof app, "%s/work", root);
    chdir(app);
    unsetenv("LLMDESC_ABI_DIR"); unsetenv("GMB_ABI_AUTOINSTALL");
    char path[2400]; snprintf(path, sizeof path, "%s/bin:/usr/bin:/bin", root); setenv("PATH", path, 1);

    /* ---- 1. modifica testuale: i commenti e le altre chiavi restano, anche con la parola "abiDir" in un commento ---- */
    const char* src =
        "{\n  // note: abiDir is set below\n  \"a\": 1,\n  /* \"paths\": { \"abiDir\": \"x\" } */\n"
        "  \"paths\": {\n    // libs\n    \"libsDir\": null\n  },\n  \"z\": [1, {\"paths\": 2}]\n}\n";
    put("app.settings.json", src);
    char v[512];
    CHECK(abi_file_get("app.settings.json", v, sizeof v) == 0);               /* il commento non conta */
    CHECK(abi_file_set("app.settings.json", "/p/one") == 1);
    CHECK(abi_file_get("app.settings.json", v, sizeof v) == 1 && strcmp(v, "/p/one") == 0);
    char* t = get("app.settings.json");
    CHECK(strstr(t, "// note: abiDir is set below") && strstr(t, "// libs") && strstr(t, "\"libsDir\": null") && strstr(t, "\"z\": [1,"));
    free(t);
    CHECK(abi_file_set("app.settings.json", "/p/tw\"o") == 1);                /* sostituzione in place, con escape */
    CHECK(abi_file_get("app.settings.json", v, sizeof v) == 1 && strcmp(v, "/p/tw\"o") == 0);
    t = get("app.settings.json"); { int n = 0; for (char* q = t; (q = strstr(q, "\"abiDir\"")); q++) n++; CHECK(n == 2); }   /* 1 nel commento a blocco + 1 reale */ free(t);
    put("app.settings.json", "{\"a\": 1}");                                    /* niente sezione paths */
    CHECK(abi_file_set("app.settings.json", "/p/three") == 1 && abi_file_get("app.settings.json", v, sizeof v) && strcmp(v, "/p/three") == 0);
    put("app.settings.json", "{}");
    CHECK(abi_file_set("app.settings.json", "/p/four") == 1 && abi_file_get("app.settings.json", v, sizeof v) && strcmp(v, "/p/four") == 0);
    put("app.settings.json", "{ \"paths\": {} }");
    CHECK(abi_file_set("app.settings.json", "/p/five") == 1 && abi_file_get("app.settings.json", v, sizeof v) && strcmp(v, "/p/five") == 0);
    put("app.settings.json", "{ \"paths\": { \"abiDir\": null } }");           /* null -> valore */
    CHECK(abi_file_get("app.settings.json", v, sizeof v) == 0);
    CHECK(abi_file_set("app.settings.json", "/p/six") == 1 && abi_file_get("app.settings.json", v, sizeof v) && strcmp(v, "/p/six") == 0);
    remove("app.settings.json");
    CHECK(abi_file_set("app.settings.json", "/p/new") == 1 && abi_file_get("app.settings.json", v, sizeof v) && strcmp(v, "/p/new") == 0);   /* file nuovo */
    put("app.settings.json", "not json at all");
    CHECK(abi_file_set("app.settings.json", "/p/x") == 0);                    /* non si tocca cio' che non e' JSON */
    t = get("app.settings.json"); CHECK(strcmp(t, "not json at all") == 0); free(t);

    /* ---- 2. impostazione valorizzata e valida: nessuna ricerca ---- */
    char out[2048];
    snprintf(cmd, sizeof cmd, "{ \"paths\": { \"abiDir\": \"%s\" } }", good); put("app.settings.json", cmd);
    reset();
    CHECK(gmb_abi_dir(out, sizeof out) == 1 && strcmp(out, good) == 0);
    /* registrata come risolutore: llmdesc_abi_dir passa da qui */
    reset();
    CHECK(llmdesc_abi_dir(out, sizeof out) == 1 && strcmp(out, good) == 0);

    /* ---- 3. percorso obsoleto (altra macchina) -> localizza (finto brew) -> riscrive nei settings ---- */
    put("app.settings.json", "{ \"paths\": { \"abiDir\": \"/gone/elsewhere/ABI\" } }");
    reset();
    CHECK(gmb_abi_dir(out, sizeof out) == 1);
    snprintf(cmd, sizeof cmd, "%s/lib/llmenv/ABI", prefix);
    CHECK(strcmp(out, cmd) == 0);
    CHECK(abi_file_get("app.settings.json", v, sizeof v) == 1 && strcmp(v, cmd) == 0);

    /* ---- 4. prima esecuzione, nessun file di settings -> lo crea ---- */
    remove("app.settings.json"); reset();
    CHECK(gmb_abi_dir(out, sizeof out) == 1 && abi_file_get("app.settings.json", v, sizeof v) == 1);

    /* ---- 5. LLMEnv assente: errore netto, il caricamento del modello fallisce ---- */
    remove("app.settings.json"); remove(fake); reset();
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", prefix); (void)system(cmd);            /* il prefisso del finto brew non esiste piu' */
    snprintf(path, sizeof path, "%s/bin:/usr/bin:/bin", root); setenv("PATH", path, 1);
    CHECK(gmb_abi_dir(out, sizeof out) == 0);
    CHECK(access("app.settings.json", F_OK) != 0);                            /* niente di scritto se non si e' trovato nulla */
    snprintf(cmd, sizeof cmd, "%s/model", root); mkdir(cmd, 0755);
    snprintf(path, sizeof path, "%s/model/config.json", root); put(path, "{\"model_type\":\"llama\"}");
    LlmDesc d; memset(&d, 0, sizeof d);
    reset();
    CHECK(llmdesc_resolve(cmd, &d, NULL, 0) == -1);

    snprintf(cmd, sizeof cmd, "rm -rf '%s'", root); (void)system(cmd);
    if (g_fail == 0) printf("test_abi_settings: OK\n");
    return g_fail ? 1 : 0;
}
