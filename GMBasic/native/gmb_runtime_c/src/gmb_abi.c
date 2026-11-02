/* FILE: native/gmb_runtime_c/src/gmb_abi.c
 * Cartella ABI di LLMEnv (descrittori dei modelli `.gmbarch`) per il MONDO C (§0bis: C# ha il suo codice).
 *
 * LLMEnv e' il PROPRIETARIO dei descrittori; qui non ce ne sono copie. Il percorso della cartella non e' fisso: dipende
 * dal canale di installazione di LLMEnv (pkg, tar.gz, Homebrew, albero sorgente) e dalla macchina. Questo modulo lo
 * risolve quando serve (la prima chiamata di llmdesc_abi_dir) e lo ricorda nei settings:
 *
 *   1. impostazione `paths.abiDir` — dell'APPLICAZIONE (app.settings.json, cartella del .bas) per i programmi BASIC,
 *      del COMPILATORE (system.settings.json) per gmb_ai_modelio. Se valorizzata e la cartella e' valida: fine.
 *   2. altrimenti (assente, null, o percorso non piu' valido: altra macchina, LLMEnv spostato) si LOCALIZZA con
 *      llmdesc_abi_locate (LLMEnv) e il risultato viene scritto nella stessa impostazione: dalla volta dopo e' un'unica lettura.
 *   3. se LLMEnv non c'e': errore netto con le istruzioni; su terminale interattivo (o GMB_ABI_AUTOINSTALL=1)
 *      si propone di installarlo con Homebrew e si riprova.
 *
 * `llmdesc_set_abi_policy(.., required=1)`: senza ABI il caricamento di un modello FALLISCE (niente ripiego silenzioso
 * sul descrittore generico). Registrazione con un constructor: vale per ogni programma che linka il runtime.
 *
 * Compilato in due contesti: runtime (scope APP) e dylib del compilatore gmb_ai_modelio (-DGMB_ABI_SCOPE_SYSTEM). */

#include "gmb_runtime.h"
/* NIENTE `llmdesc.h` (LLMEnv) nel core: le 3 funzioni usate sono dichiarate qui come DEBOLI
 * e vivono nella dylib `gmb_runtime_ai` (SPLIT 2026-11-02). */

/* SPLIT core/AI (2026-11-02): `llmdesc` (LLMEnv) vive nella dylib `gmb_runtime_ai`.
 * Questi riferimenti sono DEBOLI: senza la dylib il core linka comunque e le funzioni che
 * richiedono l'ABI dicono esplicitamente che l'AI non è disponibile (mai un ripiego muto). */
/* SPLIT core/AI (2026-11-02): l'ABI dei modelli (LLMEnv `llmdesc`) vive nella dylib
 * `gmb_runtime_ai`. Il core non linka LLMEnv: i tre servizi sono PUNTATORI registrati dalla
 * dylib al caricamento (`gmb_abi_set_llmdesc`). Se non è caricata, l'ABI non c'è e le
 * operazioni AI riportano l'errore del gateway (mai un ripiego muto). */
static int  (*g_llmdesc_valid)(const char*) = NULL;
static int  (*g_llmdesc_locate)(char*, size_t, char*, size_t) = NULL;
static void (*g_llmdesc_policy)(int (*)(char*, size_t), int) = NULL;

void gmb_abi_set_llmdesc(int (*valid)(const char*), int (*locate)(char*, size_t, char*, size_t),
                         void (*policy)(int (*)(char*, size_t), int))
{
    g_llmdesc_valid = valid;
    g_llmdesc_locate = locate;
    g_llmdesc_policy = policy;
}

static int gmb_abi_available(void)
{
    return g_llmdesc_valid != NULL;
}

#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#if defined(GMB_ABI_SCOPE_SYSTEM)
#define ABI_SCOPE_APP 0
#else
#define ABI_SCOPE_APP 1
#endif

#define ABI_KEY "abiDir"
#define ABI_REPO_URL "https://github.com/c-vision/LLMEnv"

/* ---------------------------------------------------------------------------------------------------------------
 * JSONC: lettura e modifica TESTUALE di `paths.abiDir` (i commenti // e le altre chiavi del file restano intatti)
 * --------------------------------------------------------------------------------------------------------------- */

typedef struct {
    size_t paths_open;     /* indice subito dopo la '{' dell'oggetto "paths" (0 = assente) */
    int    paths_empty;    /* l'oggetto "paths" non ha chiavi */
    size_t root_open;      /* indice subito dopo la '{' radice (0 = assente) */
    int    root_empty;
    size_t val_start, val_end;   /* span del valore di abiDir (val_end > val_start se la chiave c'e') */
} AbiScan;

static size_t abi_skip_ws(const char* s, size_t i, size_t n)
{
    while (i < n)
    {
        if (isspace((unsigned char)s[i])) { i++; continue; }
        if (s[i] == '/' && i + 1 < n && s[i + 1] == '/') { while (i < n && s[i] != '\n') i++; continue; }
        if (s[i] == '/' && i + 1 < n && s[i + 1] == '*') { i += 2; while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) i++; i += 2; continue; }
        break;
    }
    return i > n ? n : i;
}

/* Un'unica passata sul testo grezzo: commenti e stringhe sono riconosciuti, quindi una menzione di "abiDir" dentro un
 * commento non e' mai scambiata per la chiave. */
static void abi_scan(const char* s, size_t n, AbiScan* r)
{
    memset(r, 0, sizeof *r);
    int depth = 0, in_paths = 0;
    size_t i = abi_skip_ws(s, 0, n);
    while (i < n)
    {
        i = abi_skip_ws(s, i, n);
        if (i >= n) break;
        char c = s[i];
        if (c == '{')
        {
            depth++;
            size_t j = abi_skip_ws(s, i + 1, n);
            if (depth == 1 && r->root_open == 0) { r->root_open = i + 1; r->root_empty = (j < n && s[j] == '}'); }
            i++; continue;
        }
        if (c == '}') { if (in_paths && depth == 2) in_paths = 0; depth--; i++; continue; }
        if (c == '[') { /* salta l'array intero (nessuna chiave utile al suo interno) */
            int d = 1; i++;
            while (i < n && d > 0) {
                if (s[i] == '"') { i++; while (i < n && s[i] != '"') { if (s[i] == '\\') i++; i++; } }
                else if (s[i] == '[') d++; else if (s[i] == ']') d--;
                i++;
            }
            continue;
        }
        if (c == '"')
        {
            size_t a = i + 1; i++;
            while (i < n && s[i] != '"') { if (s[i] == '\\') i++; i++; }
            size_t b = i; i++;                       /* b = chiusura */
            size_t j = abi_skip_ws(s, i, n);
            if (j < n && s[j] == ':')                /* e' una chiave */
            {
                size_t kl = b > a ? b - a : 0;
                if (depth == 1 && kl == 5 && strncmp(s + a, "paths", 5) == 0)
                {
                    size_t k = abi_skip_ws(s, j + 1, n);
                    if (k < n && s[k] == '{')
                    {
                        r->paths_open = k + 1;
                        size_t m = abi_skip_ws(s, k + 1, n);
                        r->paths_empty = (m < n && s[m] == '}');
                        in_paths = 1;
                    }
                }
                else if (in_paths && depth == 2 && kl == strlen(ABI_KEY) && strncmp(s + a, ABI_KEY, kl) == 0)
                {
                    size_t v = abi_skip_ws(s, j + 1, n), e = v;
                    if (v < n && s[v] == '"') { e = v + 1; while (e < n && s[e] != '"') { if (s[e] == '\\') e++; e++; } if (e < n) e++; }
                    else while (e < n && s[e] != ',' && s[e] != '}' && s[e] != '\n' && s[e] != '/') e++;
                    while (e > v && isspace((unsigned char)s[e - 1])) e--;
                    r->val_start = v; r->val_end = e;
                }
                i = j + 1;
            }
            continue;
        }
        i++;
    }
}

static char* abi_slurp(const char* path, size_t* n)
{
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len < 0 || len > 4 * 1024 * 1024) { fclose(f); return NULL; }
    char* b = (char*)malloc((size_t)len + 1); if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)len, f); fclose(f);
    b[got] = 0; *n = got; return b;
}

/* Valore di paths.abiDir in `file` (1 = valorizzato). */
static int abi_file_get(const char* file, char* out, size_t cap)
{
    size_t n; char* s = abi_slurp(file, &n); if (!s) return 0;
    AbiScan r; abi_scan(s, n, &r);
    int ok = 0;
    if (r.val_end > r.val_start + 2 && s[r.val_start] == '"')
    {
        size_t o = 0;
        for (size_t i = r.val_start + 1; i + 1 < r.val_end && o + 1 < cap; i++)
        {
            if (s[i] == '\\' && i + 2 < r.val_end) i++;
            out[o++] = s[i];
        }
        out[o] = 0; ok = o > 0;
    }
    free(s);
    return ok;
}

/* JSON-escape (solo " e \) di un percorso. */
static void abi_quote(const char* in, char* out, size_t cap)
{
    size_t o = 0;
    for (; *in && o + 3 < cap; in++) { if (*in == '"' || *in == '\\') out[o++] = '\\'; out[o++] = *in; }
    out[o] = 0;
}

/* Scrive paths.abiDir in `file` (lo crea se manca); sostituzione atomica. 1 = scritto. */
static int abi_file_set(const char* file, const char* dir)
{
    char q[4200]; abi_quote(dir, q, sizeof q);
    size_t n = 0; char* s = abi_slurp(file, &n);
    char* out = NULL; size_t cap = n + 1024 + strlen(q) * 2, o = 0;
    out = (char*)malloc(cap); if (!out) { free(s); return 0; }
#define PUT(p, len) do { memcpy(out + o, (p), (len)); o += (len); } while (0)
    if (!s)
    {
        o = (size_t)snprintf(out, cap, "{\n  \"paths\": {\n    \"%s\": \"%s\"\n  }\n}\n", ABI_KEY, q);
    }
    else
    {
        AbiScan r; abi_scan(s, n, &r);
        char val[4300]; size_t vl = (size_t)snprintf(val, sizeof val, "\"%s\"", q);
        if (r.val_end > r.val_start)                       /* chiave presente: sostituisci il valore */
        { PUT(s, r.val_start); PUT(val, vl); PUT(s + r.val_end, n - r.val_end); }
        else if (r.paths_open)                             /* "paths" c'e': aggiungi la chiave in testa */
        {
            char ins[4400]; size_t il = (size_t)snprintf(ins, sizeof ins, "\n    \"%s\": %s%s", ABI_KEY, val, r.paths_empty ? "\n  " : ",");
            PUT(s, r.paths_open); PUT(ins, il); PUT(s + r.paths_open, n - r.paths_open);
        }
        else if (r.root_open)                              /* niente "paths": aggiungi la sezione */
        {
            char ins[4500]; size_t il = (size_t)snprintf(ins, sizeof ins, "\n  \"paths\": {\n    \"%s\": %s\n  }%s", ABI_KEY, val, r.root_empty ? "\n" : ",");
            PUT(s, r.root_open); PUT(ins, il); PUT(s + r.root_open, n - r.root_open);
        }
        else { free(s); free(out); return 0; }             /* non e' un oggetto JSON: non lo tocchiamo */
    }
#undef PUT
    char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.abi.tmp", file);
    FILE* f = fopen(tmp, "wb");
    int ok = 0;
    if (f) { ok = fwrite(out, 1, o, f) == o; ok = (fclose(f) == 0) && ok; if (ok) ok = rename(tmp, file) == 0; if (!ok) remove(tmp); }
    free(s); free(out);
    return ok;
}

/* ---------------------------------------------------------------------------------------------------------------
 * File dei settings dello scope
 * --------------------------------------------------------------------------------------------------------------- */

extern int gmb_paths_repo_root(char* out, size_t cap);
extern int gmb_paths_settings_file(char* out, size_t cap);

/* File dove SCRIVERE l'impostazione. APP: app.settings.json (cartella del programma). SYSTEM: la copia locale editabile
 * config/system.settings.json (creata dal template Resources/ se manca; mai il template stesso). 0 = nessun posto sicuro. */
static int abi_target_file(char* out, size_t cap)
{
#if ABI_SCOPE_APP
    snprintf(out, cap, "app.settings.json");
    return 1;
#else
    char root[2048] = "", cur[2048] = "";
    int have_root = gmb_paths_repo_root(root, sizeof root);
    int have_cur = gmb_paths_settings_file(cur, sizeof cur);
    if (have_cur && strstr(cur, "/Resources/") == NULL) { snprintf(out, cap, "%s", cur); return 1; }   /* copia locale (o cwd) */
    if (!have_root) return 0;
    char probe[2200]; snprintf(probe, sizeof probe, "%s/GMBASIC.sln", root);
    if (access(probe, F_OK) != 0 && !(getenv("GMBASIC_ROOT") && *getenv("GMBASIC_ROOT"))) return 0;      /* niente cartelle nella cwd altrui */
    char dir[2200]; snprintf(dir, sizeof dir, "%s/config", root); mkdir(dir, 0755);
    snprintf(out, cap, "%s/config/system.settings.json", root);
    if (have_cur)   /* parti dal template, cosi' la copia locale non perde le altre impostazioni */
    {
        size_t n; char* t = abi_slurp(cur, &n);
        if (t) { FILE* f = fopen(out, "wb"); if (f) { fwrite(t, 1, n, f); fclose(f); } free(t); }
    }
    return 1;
#endif
}

/* ---------------------------------------------------------------------------------------------------------------
 * Risoluzione
 * --------------------------------------------------------------------------------------------------------------- */

static char g_abi_dir[2048];
static int  g_abi_done = 0;
static int  g_abi_busy = 0;      /* evita la ricorsione se qualcosa dentro risolve a sua volta un modello */

static void abi_expand(const char* in, char* out, size_t cap)
{
    const char* h = getenv("HOME");
    if (in[0] == '~' && (in[1] == '/' || in[1] == '\0') && h && *h) snprintf(out, cap, "%s%s", h, in + 1);
    else snprintf(out, cap, "%s", in);
}

/* Valore salvato e valido in `file`? */
static int abi_stored(const char* file, char* out, size_t cap)
{
    char v[2048]; if (!file || !file[0] || !abi_file_get(file, v, sizeof v)) return 0;
    char e[2048]; abi_expand(v, e, sizeof e);
    if (!gmb_abi_available() || !g_llmdesc_valid(e)) return 0;
    snprintf(out, cap, "%s", e);
    return 1;
}

#if !defined(_WIN32) && !defined(_WIN64)
static int abi_have_brew(char* cmd, size_t cap)
{
    static const char* c[] = { "/opt/homebrew/bin/brew", "/usr/local/bin/brew", "/home/linuxbrew/.linuxbrew/bin/brew" };
    for (int i = 0; i < 3; i++) if (access(c[i], X_OK) == 0) { snprintf(cmd, cap, "%s", c[i]); return 1; }
    if (system("command -v brew >/dev/null 2>&1") == 0) { snprintf(cmd, cap, "brew"); return 1; }
    return 0;
}

/* Propone (o, con GMB_ABI_AUTOINSTALL=1, esegue) l'installazione di LLMEnv con Homebrew. 1 = installazione riuscita. */
static int abi_offer_install(void)
{
    const char* ai = getenv("GMB_ABI_AUTOINSTALL");
    int auto_yes = ai && ai[0] == '1';
    int interactive = isatty(0) && isatty(2);
    char brew[256];
    if (!auto_yes && !interactive) return 0;
    if (!abi_have_brew(brew, sizeof brew))
    {
        fprintf(stderr, "[GMBASIC] Homebrew not found: download the LLMEnv installer (.pkg) from %s/releases and run it.\n", ABI_REPO_URL);
        return 0;
    }
    if (!auto_yes)
    {
        fprintf(stderr, "[GMBASIC] Install LLMEnv now with Homebrew (brew tap c-vision/llmenv && brew install llmenv)? [y/N] ");
        fflush(stderr);
        char ans[16] = "";
        if (!fgets(ans, sizeof ans, stdin) || (ans[0] != 'y' && ans[0] != 'Y')) return 0;
    }
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s tap c-vision/llmenv %s && %s install llmenv", brew, ABI_REPO_URL, brew);
    fprintf(stderr, "[GMBASIC] Running: %s\n", cmd);
    return system(cmd) == 0;
}
#else
static int abi_offer_install(void) { return 0; }
#endif

int gmb_abi_dir(char* out, size_t cap)
{
    if (g_abi_done) { snprintf(out, cap, "%s", g_abi_dir); return g_abi_dir[0] ? 1 : 0; }
    if (g_abi_busy) return 0;
    g_abi_busy = 1;
    char target[2048] = "", found[2048] = "", how[128] = "";
    int have_target = abi_target_file(target, sizeof target);
    int ok = 0;

    /* 1. impostazione salvata: dello scope; per l'applicazione vale anche quella globale del compilatore */
    if (have_target && abi_stored(target, found, sizeof found)) ok = 1;
#if ABI_SCOPE_APP
    if (!ok)
    {
        char sys[2048];
        if (gmb_paths_settings_file(sys, sizeof sys) && abi_stored(sys, found, sizeof found)) ok = 1;
    }
#endif
    /* senza la dylib AI (LLMEnv) non c'è ABI: si esce in silenzio — l'errore vero lo dà
     * l'operazione AI quando viene invocata (`gmb_ai_last_error`). */
    if (!gmb_abi_available())
    {
        g_abi_dir[0] = 0;
        g_abi_done = 1; g_abi_busy = 0;
        snprintf(out, cap, "%s", g_abi_dir);
        return 0;
    }
    /* 2. prima esecuzione (o percorso non piu' valido): localizza e ricorda */
    if (!ok && g_llmdesc_locate(found, sizeof found, how, sizeof how)) ok = 2;
    /* 2b. MONOREPO: LLMEnv è un progetto FRATELLO (<repo>/../LLMEnv/ABI). llmdesc_abi_locate
     * conosce solo install (pkg/Homebrew) e albero sorgente RISALENDO da cwd/eseguibile/libreria:
     * in un run di test (cwd nei binari, libreria in una cache) non trova NULLA, e ogni
     * descrittore cade sul GENERICO in silenzio — sintomo: `LLM ARCH SHOW` con `ruoli (0)`,
     * path vuoto e `arch` preso dal solo config.json su TUTTE le gambe (verificato 2026-10-29,
     * dopo che un `paths.abiDir` rimasto puntato a una /tmp sparita aveva invalidato la cache
     * della risoluzione). Qui si prova la posizione del monorepo, che è stabile. */
    if (!ok)
    {
        char root[2048];
        if (gmb_paths_repo_root(root, sizeof root) && root[0])
        {
            static const char* tails[] = { "/../../LLMEnv/ABI", "/../LLMEnv/ABI", "/LLMEnv/ABI" };
            for (int t = 0; t < 3 && !ok; ++t)
            {
                char cand[4096];
                snprintf(cand, sizeof cand, "%s%s", root, tails[t]);
                if (g_llmdesc_valid && g_llmdesc_valid(cand))
                {
                    snprintf(found, sizeof found, "%s", cand);
                    snprintf(how, sizeof how, "monorepo sibling");
                    ok = 2;
                }
            }
        }
    }
    /* 3. LLMEnv assente: propone l'installazione */
    if (!ok && g_llmdesc_locate && abi_offer_install() &&
        g_llmdesc_locate(found, sizeof found, how, sizeof how)) ok = 2;

    if (ok == 2 && have_target && strncmp(how, "source tree", 11) != 0 && strcmp(how, "LLMDESC_ABI_DIR") != 0)
        abi_file_set(target, found);   /* l'albero sorgente si ritrova da solo; l'env e' per-processo: non si salvano */

    if (!ok)
    {
        found[0] = 0;
        fprintf(stderr,
            "[GMBASIC] ERROR: LLMEnv model descriptors (the ABI folder, *.gmbarch files) were not found.\n"
            "          Model loading and conversion need LLMEnv. Install it with:\n"
            "            brew tap c-vision/llmenv %s && brew install llmenv\n"
            "          or download the installer from %s/releases (an ABI-enabled release, 0.2.0 or later).\n"
            "          If LLMEnv is already installed, set paths.%s in %s (or the LLMDESC_ABI_DIR environment variable).\n",
            ABI_REPO_URL, ABI_REPO_URL, ABI_KEY, have_target ? target : "system.settings.json");
    }
    snprintf(g_abi_dir, sizeof g_abi_dir, "%s", found);
    g_abi_done = 1; g_abi_busy = 0;
    snprintf(out, cap, "%s", g_abi_dir);
    return g_abi_dir[0] ? 1 : 0;
}

/* Registrazione: richiesta della cartella ABI = risolta da qui, e senza ABI il caricamento del modello fallisce. */
__attribute__((constructor)) static void gmb_abi_register(void)
{
    if (g_llmdesc_policy) g_llmdesc_policy(gmb_abi_dir, 1);
}

/* Riferimento forzato dal modulo paths: tiene il file oggetto nel link anche da libreria statica. */
void gmb_abi_link_anchor(void) { }
