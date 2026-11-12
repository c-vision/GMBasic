/* FILE: native/gmb_runtime_c/src/gmb_abi.c
 * Cartella ABI di LLMEnv (descrittori dei modelli `.gmbarch`) per il MONDO C (§0bis: C# ha il suo codice).
 *
 * LLMEnv e' il PROPRIETARIO dei descrittori; qui non ce ne sono copie. Il percorso della cartella non e' fisso: dipende
 * dal canale di installazione di LLMEnv (pkg, tar.gz, Homebrew, albero sorgente) e dalla macchina. Questo modulo lo
 * risolve quando serve (la prima chiamata di llmdesc_abi_dir) e lo ricorda nei settings:
 *
 *   1. impostazione `paths.abiDir` — SOLO dell'APPLICAZIONE (`app.settings.json` nella cartella del .bas). Se
 *      valorizzata e la cartella e' valida: fine. Nessun altro file puo' contenerla: un percorso di QUESTA macchina
 *      in un settings condiviso e versionato sarebbe sbagliato su ogni altra (D16).
 *   2. altrimenti (assente, null, o percorso non piu' valido: altra macchina, LLMEnv spostato) si LOCALIZZA con
 *      llmdesc_abi_locate (LLMEnv: `$LLMDESC_ABI_DIR`, install, albero sorgente) e il risultato si scrive in
 *      `app.settings.json`: dalla volta dopo e' un'unica lettura. Lo scope COMPILATORE non scrive piu' nulla.
 *   3. se LLMEnv non c'e': errore netto con le istruzioni; su terminale interattivo (o GMB_ABI_AUTOINSTALL=1)
 *      si propone di installarlo con Homebrew e si riprova.
 *
 * `llmdesc_set_abi_policy(.., required=1)`: senza ABI il caricamento di un modello FALLISCE (niente ripiego silenzioso
 * sul descrittore generico). Registrazione con un constructor: vale per ogni programma che linka il runtime.
 *
 * Compilato in due contesti: runtime (scope APP) e dylib del compilatore gmb_ai_modelio (-DGMB_ABI_SCOPE_SYSTEM). */

#include "gmb_runtime.h"
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

#if defined(_WIN32)
/* Windows: lo split non c'è (niente dynamic_lookup) e non esiste il riferimento debole, quindi
 * l'aggancio è DIRETTO — ma solo se `llmdesc` è davvero linkato in questo eseguibile.
 * `GMB_WITH_LLMDESC` lo dichiara il build dei target che linkano llmdesc: così i servizi si
 * registrano al primo uso (nessun constructor esterno) come prima, mentre un link
 * runtime-only (solo `gmb_runtime`) NON referenzia `llmdesc_abi_valid/locate` né
 * `llmdesc_set_abi_policy` — senza il macro il link si fermava su quei tre simboli. */
#if defined(GMB_WITH_LLMDESC)
#include "llmdesc.h"
static void gmb_abi_bind_direct(void)
{
    if (g_llmdesc_valid) return;
    g_llmdesc_valid  = llmdesc_abi_valid;
    g_llmdesc_locate = llmdesc_abi_locate;
    g_llmdesc_policy = llmdesc_set_abi_policy;
}
#else
/* llmdesc non è in questo eseguibile: l'ABI dei modelli non c'è (g_llmdesc_* restano NULL). */
static void gmb_abi_bind_direct(void) { }
#endif
/* POSIX: il CORE non linka LLMEnv (split core/AI), ma i programmi C99Native linkano `llmdesc`
 * STATICAMENTE nello stesso eseguibile. Lì nessuno chiama `gmb_abi_set_llmdesc` — lo fa il
 * constructor di `gmb_runtime_ai` (dylib), che non c'è — e la policy restava INERTE: `paths.abiDir`
 * ignorato e ramo `monorepo sibling` irraggiungibile (provato 2026-11-11 su un'app reale).
 * Due modi di agganciare i tre servizi SENZA obbligare il core a linkare LLMEnv. */
#elif defined(__APPLE__)
/* Darwin: né `weak` né `weak_import` su una dichiarazione si possono lasciare irrisolti in un
 * eseguibile — ld64 li tratta come strong e il link fallisce ("Undefined symbols ...
 * _llmdesc_abi_valid", verificato con `cc` nudo, 2026-11-11). `dlsym(RTLD_DEFAULT)` non crea
 * alcun riferimento al momento del link e trova sia la copia STATICA nello stesso eseguibile
 * (programmi C99Native: libllmdesc.a) sia quella della dylib AI. */
#include <dlfcn.h>
static void gmb_abi_bind_direct(void)
{
    if (g_llmdesc_valid) return;
    int  (*v)(const char*)                  = (int (*)(const char*))dlsym(RTLD_DEFAULT, "llmdesc_abi_valid");
    int  (*l)(char*, size_t, char*, size_t)  = (int (*)(char*, size_t, char*, size_t))dlsym(RTLD_DEFAULT, "llmdesc_abi_locate");
    void (*p)(int (*)(char*, size_t), int)   = (void (*)(int (*)(char*, size_t), int))dlsym(RTLD_DEFAULT, "llmdesc_set_abi_policy");
    /* La POLICY e' OPZIONALE: i binari si linkano con `-Wl,-dead_strip` e nessuno riferisce
     * `llmdesc_set_abi_policy` (la si cerca solo qui, con dlsym) -> il linker la elimina.
     * Pretendendo anche lei, NESSUN servizio veniva agganciato: `gmb_abi_available()` restava
     * falso, `paths.abiDir` non veniva mai letto e la cartella la sceglieva LLMEnv da sola
     * (ramo `monorepo sibling`) — bug REPL/VM-C 2026-11-11. Con valid+locate basta: la policy
     * serve solo a far delegare LLMEnv a questo resolver, e chi la trova la registra sotto. */
    if (!v || !l) return;                     /* LLMEnv non è in questo processo */
    g_llmdesc_valid  = v;
    g_llmdesc_locate = l;
    g_llmdesc_policy = p;
}
#else
/* ELF (Linux): un riferimento DEBOLE può restare irrisolto e vale 0 quando LLMEnv non c'è,
 * senza obbligare il core a linkarlo. */
extern int  llmdesc_abi_valid(const char*) __attribute__((weak));
extern int  llmdesc_abi_locate(char*, size_t, char*, size_t) __attribute__((weak));
extern void llmdesc_set_abi_policy(int (*)(char*, size_t), int) __attribute__((weak));
static void gmb_abi_bind_direct(void)
{
    if (g_llmdesc_valid) return;
    /* La policy e' opzionale (vedi il ramo Darwin: `-dead_strip` puo' rimuoverla). */
    if (!llmdesc_abi_valid || !llmdesc_abi_locate) return;
    g_llmdesc_valid  = llmdesc_abi_valid;
    g_llmdesc_locate = llmdesc_abi_locate;
    g_llmdesc_policy = llmdesc_set_abi_policy;   /* NULL se assente: va bene */
}
#endif

static int gmb_abi_available(void)
{
    gmb_abi_bind_direct();
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

/* ---------------------------------------------------------------------------------------------------------------
 * Validazione JSONC (commenti ammessi), usata SOLO come guardia prima di una scrittura.
 * Un `app.settings.json` rotto dall'utente (troncato, virgola di troppo, testo qualunque) non deve
 * essere "corretto" a metà: si lascia com'è e la cartella si ritrova lo stesso in memoria.
 * --------------------------------------------------------------------------------------------------------------- */

static void abi_j_ws(const char* s, size_t n, size_t* i)
{
    while (*i < n)
    {
        if (isspace((unsigned char)s[*i])) { (*i)++; continue; }
        if (s[*i] == '/' && *i + 1 < n && s[*i + 1] == '/') { while (*i < n && s[*i] != '\n') (*i)++; continue; }
        if (s[*i] == '/' && *i + 1 < n && s[*i + 1] == '*') { *i += 2; while (*i + 1 < n && !(s[*i] == '*' && s[*i + 1] == '/')) (*i)++; *i += 2; continue; }
        break;
    }
}

static int abi_j_value(const char* s, size_t n, size_t* i, int depth);

static int abi_j_string(const char* s, size_t n, size_t* i)
{
    if (*i >= n || s[*i] != '"') return 0;
    (*i)++;
    while (*i < n)
    {
        if (s[*i] == '\\') { *i += 2; continue; }
        if (s[*i] == '"') { (*i)++; return 1; }
        if ((unsigned char)s[*i] < 0x20) return 0;      /* controllo non escapato: non è JSON */
        (*i)++;
    }
    return 0;                                           /* stringa non chiusa */
}

static int abi_j_number(const char* s, size_t n, size_t* i)
{
    size_t j = *i;
    if (j < n && s[j] == '-') j++;
    if (j >= n || !isdigit((unsigned char)s[j])) return 0;
    if (s[j] == '0') j++; else while (j < n && isdigit((unsigned char)s[j])) j++;
    if (j < n && s[j] == '.') { j++; if (j >= n || !isdigit((unsigned char)s[j])) return 0; while (j < n && isdigit((unsigned char)s[j])) j++; }
    if (j < n && (s[j] == 'e' || s[j] == 'E'))
    {
        j++; if (j < n && (s[j] == '+' || s[j] == '-')) j++;
        if (j >= n || !isdigit((unsigned char)s[j])) return 0;
        while (j < n && isdigit((unsigned char)s[j])) j++;
    }
    *i = j; return 1;
}

static int abi_j_object(const char* s, size_t n, size_t* i, int depth)
{
    (*i)++;                                             /* '{' */
    abi_j_ws(s, n, i);
    if (*i < n && s[*i] == '}') { (*i)++; return 1; }
    for (;;)
    {
        abi_j_ws(s, n, i);
        if (!abi_j_string(s, n, i)) return 0;
        abi_j_ws(s, n, i);
        if (*i >= n || s[*i] != ':') return 0;
        (*i)++;
        if (!abi_j_value(s, n, i, depth + 1)) return 0;
        abi_j_ws(s, n, i);
        if (*i < n && s[*i] == ',') { (*i)++; continue; }
        if (*i < n && s[*i] == '}') { (*i)++; return 1; }
        return 0;
    }
}

static int abi_j_array(const char* s, size_t n, size_t* i, int depth)
{
    (*i)++;                                             /* '[' */
    abi_j_ws(s, n, i);
    if (*i < n && s[*i] == ']') { (*i)++; return 1; }
    for (;;)
    {
        if (!abi_j_value(s, n, i, depth + 1)) return 0;
        abi_j_ws(s, n, i);
        if (*i < n && s[*i] == ',') { (*i)++; continue; }
        if (*i < n && s[*i] == ']') { (*i)++; return 1; }
        return 0;
    }
}

static int abi_j_value(const char* s, size_t n, size_t* i, int depth)
{
    if (depth > 64) return 0;                           /* annidamento patologico */
    abi_j_ws(s, n, i);
    if (*i >= n) return 0;
    switch (s[*i])
    {
        case '{': return abi_j_object(s, n, i, depth);
        case '[': return abi_j_array(s, n, i, depth);
        case '"': return abi_j_string(s, n, i);
        case 't': if (*i + 4 <= n && strncmp(s + *i, "true", 4) == 0) { *i += 4; return 1; } return 0;
        case 'f': if (*i + 5 <= n && strncmp(s + *i, "false", 5) == 0) { *i += 5; return 1; } return 0;
        case 'n': if (*i + 4 <= n && strncmp(s + *i, "null", 4) == 0) { *i += 4; return 1; } return 0;
        default: return abi_j_number(s, n, i);
    }
}

/* 1 = tutto il testo è UN valore JSONC valido e nulla lo segue (spazi e commenti esclusi). */
static int abi_json_ok(const char* s, size_t n)
{
    if (!s || n == 0) return 0;
    size_t i = 0;
    if (!abi_j_value(s, n, &i, 0)) return 0;
    abi_j_ws(s, n, &i);
    return i == n;
}

/* Scrive paths.abiDir in `file` (lo crea se manca); sostituzione atomica. 1 = scritto. */
static int abi_file_set(const char* file, const char* dir)
{
    char q[4200]; abi_quote(dir, q, sizeof q);
    size_t n = 0; char* s = abi_slurp(file, &n);
    /* Il file C'E' ma non è JSON valido (o non è leggibile / troppo grande): NON lo tocchiamo.
     * Sovrascriverlo distruggerebbe impostazioni dell'utente per una chiave che si ritrova da sola
     * a ogni avvio (`llmdesc_abi_locate`). La cartella resta comunque risolta in memoria. */
    if (access(file, F_OK) == 0 && (!s || !abi_json_ok(s, n))) { free(s); return 0; }
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

/* Cartella dell'APPLICAZIONE nota al processo ma diversa dalla CWD: la REPL/VM-C lancia il
 * programma da un'altra cartella, quindi `app.settings.json` accanto al `.bas` non si vedrebbe.
 * La imposta l'host con `--entry-source` (`gmb_abi_set_app_dir`), senza nuove env. */
static char g_abi_app_dir[2048];

void gmb_abi_set_app_dir(const char* dir)
{
    snprintf(g_abi_app_dir, sizeof g_abi_app_dir, "%s", dir ? dir : "");
}

/* File dove SCRIVERE l'impostazione: SOLO `app.settings.json` (cartella del programma).
 *
 * Lo scope COMPILATORE (`gmb_ai_modelio`) NON scrive niente: `paths.abiDir` è un'impostazione
 * dell'APPLICAZIONE, e i settings di sistema sono condivisi e versionati — un percorso di questa
 * macchina lì dentro romperebbe il compilatore su tutte le altre (D16). Il compilatore risolve
 * con `llmdesc_abi_locate` (env `LLMDESC_ABI_DIR`, install, albero sorgente): nessuna cache
 * necessaria, e `gmb doctor --abi-dir` / `gmbadmin first-use --abi-dir` la stampano per la shell. */
static int abi_target_file(char* out, size_t cap)
{
#if ABI_SCOPE_APP
    /* 1. la CWD ha gia' il file (programma lanciato dalla sua cartella): caso normale. */
    if (access("app.settings.json", R_OK) == 0) { snprintf(out, cap, "app.settings.json"); return 1; }
    /* 2. cartella dell'applicazione nota (REPL/VM-C: il `.bas` sta altrove): si usa il file
     *    SOLO se esiste gia' — non si creano settings in una cartella che non e' quella di
     *    esecuzione (la prima esecuzione li crea dove il programma gira). */
    if (g_abi_app_dir[0])
    {
        char cand[4200];
        snprintf(cand, sizeof cand, "%s/app.settings.json", g_abi_app_dir);
        if (access(cand, R_OK) == 0) { snprintf(out, cap, "%s", cand); return 1; }
        return 0;
    }
    /* 3. prima esecuzione nella cartella da cui gira il programma: qui si crea. */
    snprintf(out, cap, "app.settings.json");
    return 1;
#else
    (void)out; (void)cap;
    return 0;
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
    /* NIENTE lettura dei settings di sistema: la cartella ABI è per-APPLICAZIONE (D16). Un valore
     * lasciato lì da versioni precedenti si ignora: `llmdesc` la ritrova da sé in un attimo. */
    /* senza la dylib AI (LLMEnv) non c'è ABI: si esce in silenzio — l'errore vero lo dà
     * l'operazione AI quando viene invocata (`gmb_ai_last_error`). */
    if (!gmb_abi_available())
    {
        g_abi_dir[0] = 0;
        g_abi_done = 1; g_abi_busy = 0;
        snprintf(out, cap, "%s", g_abi_dir);
        return 0;
    }
    /* 2. prima esecuzione (o percorso non piu' valido): localizza e ricorda.
     * Il puntatore va verificato: `gmb_abi_available()` garantisce `g_llmdesc_valid`, non
     * `g_llmdesc_locate` (una registrazione parziale lo lascia a NULL) e una chiamata a 0 e'
     * un crash dell'eseguibile, non un errore ABI. */
    if (!ok && g_llmdesc_locate && g_llmdesc_locate(found, sizeof found, how, sizeof how)) ok = 2;
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

    /* Percorso CANONICO prima di usarlo/scriverlo: i candidati del ramo sibling nascono da
     * `<root>/../..` e finirebbero nei settings con i `..` dentro (`realpath` non c'è su Windows,
     * dove il ramo sibling non esiste). */
#if !defined(_WIN32)
    if (ok && found[0]) { char rp[4096]; if (realpath(found, rp)) snprintf(found, sizeof found, "%s", rp); }
#endif

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
            "          If LLMEnv is already installed, set paths.%s in app.settings.json (or the LLMDESC_ABI_DIR\n"
            "          environment variable; `gmb doctor --abi-dir` prints the folder).\n",
            ABI_REPO_URL, ABI_REPO_URL, ABI_KEY);
    }
    snprintf(g_abi_dir, sizeof g_abi_dir, "%s", found);
    g_abi_done = 1; g_abi_busy = 0;
    snprintf(out, cap, "%s", g_abi_dir);
    return g_abi_dir[0] ? 1 : 0;
}

/* Registrazione: richiesta della cartella ABI = risolta da qui, e senza ABI il caricamento del modello fallisce. */
__attribute__((constructor)) static void gmb_abi_register(void)
{
    gmb_abi_bind_direct();   /* Windows: binding statico; altrove no-op */
    if (g_llmdesc_policy) g_llmdesc_policy(gmb_abi_dir, 1);
}

/* Riferimento forzato dal modulo paths: tiene il file oggetto nel link anche da libreria statica.
 *
 * Inoltre tiene VIVO `gmb_abi_set_llmdesc` per il lookup A LOAD-TIME della dylib
 * `gmb_runtime_ai`: il suo costruttore la risolve con `-undefined dynamic_lookup` (flat namespace)
 * e NESSUN codice dell'eseguibile la chiama. ld64 la esporta comunque; lld di `zig cc` la
 * dead-strippa perché nulla la referenzia (misurato il 2026-11-12: eseguibile con 198 simboli
 * esportati invece di 4020, `_gmb_abi_set_llmdesc` assente, e il costruttore della dylib saltava
 * a 0 — EXC_BAD_ACCESS at 0x0 prima di `main`). Prendere qui l'indirizzo rende il simbolo
 * raggiungibile, quindi resta nel link ed è esportato da entrambi i linker. `volatile` perché
 * l'ottimizzatore non può togliere la relocazione. */
void gmb_abi_link_anchor(void)
{
    static void (*volatile keep_services)(int (*)(const char*),
                                          int (*)(char*, size_t, char*, size_t),
                                          void (*)(int (*)(char*, size_t), int)) = gmb_abi_set_llmdesc;
    (void)keep_services;
}
