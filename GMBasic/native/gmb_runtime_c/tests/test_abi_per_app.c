/* FILE: native/gmb_runtime_c/tests/test_abi_per_app.c
 *
 * `paths.abiDir` si scrive SOLO in `app.settings.json` — MAI nei settings di sistema.
 *
 * PERCHÉ DUE ESECUZIONI DELLO STESSO FILE. `gmb_abi.c` è compilato in due contesti e la regola
 * vale per entrambi:
 *   • APP    (runtime: `gmb_boot`, `gmb_runtime_ai`): l'unico bersaglio è `app.settings.json`
 *            nella cartella del programma;
 *   • SYSTEM (`gmb_ai_modelio`, `-DGMB_ABI_SCOPE_SYSTEM`): NON scrive niente. I settings di
 *            sistema sono condivisi e versionati: un percorso di QUESTA macchina lì dentro
 *            romperebbe il compilatore su tutte le altre (D16).
 * Il `CMakeLists.txt` costruisce quindi questo sorgente due volte, con e senza la macro, e
 * registra DUE test: la differenza di comportamento è così verificata, non dedotta.
 *
 * PERCHÉ IN C E NON UN `.bas` MINITEST. La regola è un EFFETTO SU FILE prodotto dal runtime C,
 * mentre un MiniTest `.bas` confronta solo l'output: non può asserire «questo file esiste e
 * quell'altro no», non può ripulire `app.settings.json` fra i casi, e — peggio — l'esito
 * dipenderebbe dalla presenza di LLMEnv sulla macchina (in CI manca). Qui si costruiscono due
 * cartelle ABI finte in `$TMPDIR`: nessuna dipendenza esterna, nessun modello.
 *
 * COSA VERIFICA
 *   APP:    c'è un bersaglio ed è `app.settings.json`; una risoluzione che trova la cartella la
 *           scrive lì; nella cartella di lavoro resta QUEL SOLO file (nessun settings di sistema).
 *   SYSTEM: nessun bersaglio; la risoluzione FUNZIONA (restituisce la cartella) ma non scrive
 *           nulla — né `app.settings.json` né `system.settings.json`.
 *   ENTRAMBI: un `app.settings.json` già presente ma con JSON NON VALIDO resta **intatto byte per
 *           byte**: non si distrugge e non si "corregge" a metà (caso reale: file troncato da un
 *           editor, che il vecchio codice avrebbe riscritto restando invalido).
 *   APP:    `paths.abiDir` STANTIO con un albero `monorepo sibling` disponibile: la cartella si
 *           ritrova per quella via (scoperta disattivata) e il percorso nuovo **si riscrive**.
 */
#include "../src/gmb_abi.c"
#include "llmdesc.h"
#include <assert.h>
#include <dirent.h>      /* opendir/readdir/closedir per contare i file scritti */
#include <stdlib.h>      /* malloc/free del confronto byte per byte */
#include <string.h>
#include <sys/stat.h>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void put(const char* path, const char* body) { FILE* f = fopen(path, "wb"); assert(f); fputs(body, f); fclose(f); }
static void reset(void) { g_abi_done = 0; g_abi_busy = 0; g_abi_dir[0] = 0; }

/* Cartelle risolte: `$TMPDIR` su macOS è `/var/folders/...`, link a `/private/var/...`. */
static int same_dir(const char* a, const char* b)
{
    char ra[4096], rb[4096];
    const char* pa = (a && *a && realpath(a, ra)) ? ra : a;
    const char* pb = (b && *b && realpath(b, rb)) ? rb : b;
    return pa && pb && strcmp(pa, pb) == 0;
}

/* Quanti file (non nascosti) contiene la cartella: serve a dimostrare che la risoluzione non
 * scrive ALTRI settings oltre a quello previsto. */
static int count_files(const char* dir)
{
    DIR* d = opendir(dir);
    if (!d) return -1;
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) if (e->d_name[0] != '.') n++;
    closedir(d);
    return n;
}

/* Contenuto grezzo di un file: serve al caso «JSON non valido» per dimostrare che non cambia.
 * Un file assente restituisce NULL (distinzione necessaria: NULL = non c'era, non «vuoto»). */
static char* slurp_all(const char* path, size_t* n)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    size_t cap = len > 0 ? (size_t)len : 0;
    char* b = (char*)malloc(cap + 1);
    size_t got = b ? fread(b, 1, cap, f) : 0;
    if (b) b[got] = 0;
    fclose(f);
    if (n) *n = got;
    return b;
}

/* Scoperta finta: restituisce sempre la cartella ABI preparata dal test (nessun LLMEnv reale). */
static char g_locate_dir[2048];
static int stub_locate(char* out, size_t cap, char* how, size_t howcap)
{
    if (!g_locate_dir[0]) return 0;
    snprintf(out, cap, "%s", g_locate_dir);
    if (how) snprintf(how, howcap, "%s", "stub");
    return 1;
}

int main(void)
{
    gmb_abi_set_llmdesc(llmdesc_abi_valid, stub_locate, llmdesc_set_abi_policy);
    llmdesc_set_abi_policy(gmb_abi_dir, 1);

    char tmpl[1024];
    snprintf(tmpl, sizeof tmpl, "%s/gmb_abi_perapp_XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    char* root = mkdtemp(tmpl);
    if (!root) return 77;   /* niente dir temporanea: non è un fallimento della regola */

    char work[1200], good[1200], cmd[3000], out[2048], target[2048];
    snprintf(work, sizeof work, "%s/work", root);
    snprintf(good, sizeof good, "%s/abi", root);
    snprintf(cmd, sizeof cmd, "mkdir -p '%s' '%s'", work, good); (void)system(cmd);
    snprintf(cmd, sizeof cmd, "%s/model.gmbarch", good); put(cmd, "{}\n");
    if (chdir(work) != 0) return 77;
    remove("app.settings.json");
    remove("system.settings.json");
    snprintf(g_locate_dir, sizeof g_locate_dir, "%s", good);

#if ABI_SCOPE_APP
    /* ---- CONTESTO APPLICAZIONE: l'unico bersaglio è app.settings.json ---- */
    CHECK(abi_target_file(target, sizeof target) == 1);
    CHECK(strstr(target, "app.settings.json") != NULL);

    reset();
    CHECK(gmb_abi_dir(out, sizeof out) == 1 && same_dir(out, good));
    {
        char v[2048];
        CHECK(abi_file_get("app.settings.json", v, sizeof v) == 1 && same_dir(v, good));
    }
    CHECK(access("system.settings.json", F_OK) != 0);      /* nessun settings di sistema qui ... */
    CHECK(count_files(".") == 1);                          /* ... e l'unico file scritto è il suo */
#else
    /* ---- CONTESTO COMPILATORE: nessun bersaglio, nessuna scrittura ---- */
    CHECK(abi_target_file(target, sizeof target) == 0);

    reset();
    CHECK(gmb_abi_dir(out, sizeof out) == 1 && same_dir(out, good));   /* risolve ... */
    CHECK(access("app.settings.json", F_OK) != 0);                     /* ... ma non scrive */
    CHECK(access("system.settings.json", F_OK) != 0);
    CHECK(count_files(".") == 0);
#endif

    /* ---- IL FILE ESISTE MA IL JSON NON E' VALIDO: non si distrugge ----
     * Il caso peggiore e' il file TRONCATO a metà: `abi_scan` ci troverebbe comunque la chiave e il
     * vecchio codice la riscriverebbe dentro un testo ancora invalido. Ora la guardia `abi_json_ok`
     * blocca la scrittura: la cartella si risolve in memoria, il file resta com'era. */
    {
        const char* corrupt = "{ \"paths\": { \"abiDir\": \"/gone/elsewhere/ABI\"\n";
        put("app.settings.json", corrupt);
        size_t before_n = 0; char* before = slurp_all("app.settings.json", &before_n);
        CHECK(before != NULL && before_n > 0);
        CHECK(abi_json_ok(before, before_n) == 0);            /* il test parte DAVVERO da JSON non valido */
        if (!before) { before = (char*)""; }

        reset();
        CHECK(gmb_abi_dir(out, sizeof out) == 1 && same_dir(out, good));   /* risolve lo stesso ... */
        {
            size_t after_n = 0; char* after = slurp_all("app.settings.json", &after_n);
            CHECK(after != NULL && after_n == before_n && memcmp(after, before, before_n) == 0);
            free(after);
        }
        CHECK(access("app.settings.json.abi.tmp", F_OK) != 0);  /* ... nessun temporaneo lasciato ... */
        CHECK(count_files(".") == 1);                           /* ... e nessun altro file creato */
        free(before);
    }

    /* ---- e la scrittura torna appena il file torna valido ---- */
    {
        put("app.settings.json", "{}\n");
        reset();
        CHECK(gmb_abi_dir(out, sizeof out) == 1 && same_dir(out, good));
#if ABI_SCOPE_APP
        {
            char v[2048];
            CHECK(abi_file_get("app.settings.json", v, sizeof v) == 1 && same_dir(v, good));
            size_t n2 = 0; char* b2 = slurp_all("app.settings.json", &n2);
            CHECK(b2 != NULL && abi_json_ok(b2, n2) == 1);      /* quello scritto è JSON valido */
            free(b2);
        }
#else
        {
            char v[2048];
            CHECK(abi_file_get("app.settings.json", v, sizeof v) == 0);   /* SYSTEM: `{}` resta `{}` */
            size_t n2 = 0; char* b2 = slurp_all("app.settings.json", &n2);
            CHECK(b2 != NULL && n2 == 3 && memcmp(b2, "{}\n", 3) == 0);
            free(b2);
        }
#endif
    }

    /* ---- abiDir STANTIO + ramo `monorepo sibling`: ri-localizza E RISCRIVE ----
     * La scoperta di LLMEnv è spenta (`g_locate_dir` vuota), quindi l'unica via che resta è il ramo
     * 2b di `gmb_abi_dir`: `gmb_paths_repo_root` + le tre posizioni `…/LLMEnv/ABI`. L'albero finto
     * riproduce la disposizione reale (`<root>/tree/GMBasic` e `<root>/LLMEnv/ABI`). */
    {
        char tree[1400], gmroot[1500], sib[1400], f[1500];
        snprintf(tree, sizeof tree, "%s/tree", root);
        snprintf(cmd, sizeof cmd, "mkdir -p '%s/GMBasic' '%s/LLMEnv/ABI'", tree, root); (void)system(cmd);
        snprintf(sib, sizeof sib, "%s/LLMEnv/ABI", root);
        snprintf(f, sizeof f, "%s/model.gmbarch", sib); put(f, "{}\n");
        snprintf(gmroot, sizeof gmroot, "%s/GMBasic", tree);
        setenv("GMBASIC_ROOT", gmroot, 1);
        g_locate_dir[0] = 0;                                   /* nessuna scoperta: solo il ramo sibling */

        put("app.settings.json", "{ \"paths\": { \"abiDir\": \"/gone/elsewhere/ABI\" } }\n");
        reset();
        CHECK(gmb_abi_dir(out, sizeof out) == 1 && same_dir(out, sib));
#if ABI_SCOPE_APP
        {
            char v[2048];
            CHECK(abi_file_get("app.settings.json", v, sizeof v) == 1 && same_dir(v, sib));   /* riscritto */
            size_t n3 = 0; char* b3 = slurp_all("app.settings.json", &n3);
            CHECK(b3 != NULL && abi_json_ok(b3, n3) == 1);       /* e il file resta JSON valido */
            free(b3);
            CHECK(count_files(".") == 1);
        }
#else
        {
            char v[2048];                                       /* SYSTEM: risolve ma non scrive */
            CHECK(abi_file_get("app.settings.json", v, sizeof v) == 1 && strcmp(v, "/gone/elsewhere/ABI") == 0);
        }
#endif
        unsetenv("GMBASIC_ROOT");
    }

    /* Messaggio finale UNICO, dopo tutti i casi: l'ordine con cui sono verificati è quello qui
     * elencato, e il ramo sibling gira DOPO il blocco #if (quindi non può essere annunciato prima). */
#if ABI_SCOPE_APP
    printf("test_abi_per_app: OK (APP: app.settings.json unico bersaglio; JSON corrotto intatto; abiDir stantio -> sibling risolto e riscritto)\n");
#else
    printf("test_abi_per_app_system: OK (SYSTEM: nessuna scrittura; JSON corrotto intatto; abiDir stantio -> sibling risolto, file intatto)\n");
#endif

    snprintf(cmd, sizeof cmd, "rm -rf '%s'", root); (void)system(cmd);
    return g_fail ? 1 : 0;
}
