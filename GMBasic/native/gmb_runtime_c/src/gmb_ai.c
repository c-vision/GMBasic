/* gmb_ai.c — Registry AI in-memory headless: MEMORY (key-value), PROVIDER, AIVECTOR. */
#include "gmb_runtime.h"
#include "gmb_rag_internal.h"   /* rag_keyword_score (unica implementazione §0bis) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define AI_MAX_ENTRIES 256
#define AI_MAX_PROVIDERS 64
#define AI_MAX_COLLS 16
#define AI_MAX_DOCS 256

/* MEMORY.* engine spostato in gmb_memory.c (port SQLite :memory: di AgentMemoryStore, 2026-09-02). */

/* --- PROVIDER: registry per ruolo (embedding/completion) --- */
/* status: 1=Available, 0=Unavailable, 2=Blocked, 3=Damaged (parità ProviderStatus C#). */
/* kind: scheme/kind del provider, MA anche la SORGENTE locale (path `.gguf`/cartella MLX) passata
 * da AI REGISTER — un path supera i 32 char, quindi 512 (prima troncava "...ds4flash.gguf" a 31
 * e il routing MLX non riconosceva più l'estensione). */
typedef struct { char role[32]; char name[64]; char kind[512]; char config[512]; int active; int weight; int priority; int status; int fail_count; int wrr_acc; } gmb_provider_t;
static gmb_provider_t g_providers[AI_MAX_PROVIDERS];
static int g_provider_count = 0;

static gmb_provider_t* find_provider(const char* role, const char* name);
static void ensure_defaults(void);
static void provider_model_path(const gmb_provider_t* p, char* out, size_t cap);

/* Parità C# ProviderServer.EnsureDefaultsLocked (chiamata in testa a ogni entry pubblica):
 * i tre provider di default sono SEMPRE registrati — fixture-embedding (embedding),
 * fixture-completion (completion), keyword-reranker (reranker). Prima solo
 * gmb_provider_active() li garantiva → qualunque altro entry (TEST, STATUS, LIST...)
 * chiamato PER PRIMO nel processo trovava il registro vuoto («not found for role» su
 * AiDbAdmin/c99n, mentre VMC li vedeva perché l'host li aveva già toccati: flaky
 * d'ordine). Idempotente (flag); sicura contro ricorsione (flag alzata prima di
 * registrare; register→find→ensure con flag già alzata = no-op). */
static int g_defaults_done = 0;
/* `AI SET ACTIVE %EMBEDDING` esplicito (gmb_provider_set_active): rende legittimo il fixture
 * embedding — vedi gmb_provider_embedding_explicit / gmb_fixture_embed_wanted. */
static int g_embedding_set_explicitly = 0;
static void ensure_defaults(void) {
    if (g_defaults_done) return;
    g_defaults_done = 1;
    if (!find_provider("embedding", "fixture-embedding"))
        gmb_provider_register("embedding", "fixture-embedding", "fixture", NULL);
    if (!find_provider("completion", "fixture-completion"))
        gmb_provider_register("completion", "fixture-completion", "fixture", NULL);
    if (!find_provider("reranker", "keyword-reranker"))
        gmb_provider_register("reranker", "keyword-reranker", "keyword", NULL);
}

static gmb_provider_t* find_provider(const char* role, const char* name) {
    ensure_defaults();
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0 && (!role || !*role || strcmp(g_providers[i].role, role) == 0)) return &g_providers[i];
    return NULL;
}
/* Normalizza capability/ruolo (parità C# ProviderRole.FromCapabilityOrRole): %CHAT/"0"/
 * "chat" → completion, %OCR/"1" → ocr, %AUDIO/"2" → audio, %EMBEDDING/"3" → embedding,
 * %RERANKER/"4" → reranker, %TTS/"5" → tts; ignoto/vuoto → completion. Ritorna buffer statico. */
const char* gmb_provider_normalize_role(const char* role) {
    static char out[32];
    const char* v = (role && *role == '%') ? role + 1 : (role ? role : "");
    while (*v == ' ') v++;
    char lo[32]; size_t i = 0;
    while (v[i] && i + 1 < sizeof(lo)) { char c = v[i]; lo[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; i++; }
    lo[i] = 0;
    size_t L = strlen(lo);
    while (L > 0 && lo[L-1] == ' ') lo[--L] = 0;
    const char* r = "completion";
    if (!strcmp(lo, "1") || !strcmp(lo, "ocr")) r = "ocr";
    else if (!strcmp(lo, "2") || !strcmp(lo, "audio")) r = "audio";
    else if (!strcmp(lo, "3") || !strcmp(lo, "embedding")) r = "embedding";
    else if (!strcmp(lo, "4") || !strcmp(lo, "reranker")) r = "reranker";
    else if (!strcmp(lo, "5") || !strcmp(lo, "tts")) r = "tts";
    else if (!strcmp(lo, "6") || !strcmp(lo, "decide")) r = "decide";
    snprintf(out, sizeof(out), "%s", r);
    return out;
}
/* True se `src` è un modello locale MLX: file `.gguf` (single-file, es. DS4/DeepSeek V4 Flash)
 * oppure cartella repo MLX (config.json + almeno un .safetensors). Usata dal REGISTER per
 * accettare la sorgente (parità col routing host C#) e dall'accessor del path per l'auto-wire
 * del brain MLX nei runtime C. */
static int gmb_ai_is_local_mlx_source(const char* src) {
    if (!src || !*src) return 0;
    size_t L = strlen(src);
    if (L > 5 && strcasecmp(src + L - 5, ".gguf") == 0) return 1;
    struct stat st;
    if (stat(src, &st) == 0 && S_ISDIR(st.st_mode)) {
        char cfg[1200];
        if ((size_t)snprintf(cfg, sizeof cfg, "%s/config.json", src) < sizeof cfg
            && stat(cfg, &st) == 0 && S_ISREG(st.st_mode)) return 1;
    }
    return 0;
}

int gmb_provider_register(const char* role, const char* name, const char* kind, const char* config) {
    if (!role || !name) return 0;
    /* Normalizza il ruolo (parità C# ProviderRole.FromCapabilityOrRole): i backend C ricevono
     * `%CHAT` come "0"/"%CHAT" — senza normalizzazione il provider veniva registrato sotto il
     * ruolo sbagliato. COPIA LOCALE: gmb_provider_normalize_role usa un buffer statico e
     * find_provider→ensure_defaults lo sovrascrive (re-entrancy) → ruolo corrotto. */
    char rolebuf[32]; snprintf(rolebuf, sizeof rolebuf, "%s", gmb_provider_normalize_role(role));
    role = rolebuf;
    /* Modello locale MLX (`.gguf` single-file o cartella repo MLX) o override esplicito
     * {"engine":"mlx"} nel param: sorgente VALIDA (parità col routing host C#, che la mappa
     * a kind "mlx"). Il brain MLX la carica via gmb_provider_active_model_path(). */
    int is_local_mlx = gmb_ai_is_local_mlx_source(kind)
                       || (config && strstr(config, "\"engine\"") && strstr(config, "mlx"));
    /* Validazione sorgente (parità ProviderServer.RegisterBySource C#): una sorgente senza
     * scheme ('<scheme>:<id>') che non è un modello locale noto è un ERRORE, non un provider
     * valido (es. "ollama" nudo). I kind INTERNI (fixture/keyword/embedded/llamacpp/mlx/local)
     * restano validi. Prima il C registrava qualunque sorgente → nessun errore. */
    if (!is_local_mlx && kind && *kind && !strchr(kind, ':')
        && strcasecmp(kind, "fixture") && strcasecmp(kind, "keyword")
        && strcasecmp(kind, "llamacpp")
        && strcasecmp(kind, "mlx") && strcasecmp(kind, "local")
        && strcasecmp(kind, "rules") && strcasecmp(kind, "mock"))
        return 0;
    gmb_provider_t* p = find_provider(role, name);
    if (p) { strncpy(p->kind, kind ? kind : "", 511); strncpy(p->config, config ? config : "", 511); return 1; }
    if (g_provider_count < AI_MAX_PROVIDERS) {
        gmb_provider_t* np = &g_providers[g_provider_count];
        strncpy(np->role, role, 31); strncpy(np->name, name, 63);
        strncpy(np->kind, kind ? kind : "", 511); strncpy(np->config, config ? config : "", 511);
        np->weight = 1; np->priority = 0; np->status = 1; np->fail_count = 0; np->wrr_acc = 0;
        g_provider_count++;
        return 1;
    }
    return 0;
}
/* Registrazione per-KIND (forma funzione AI.REGISTER(role, name, kind, config)) — parità
 * C# ProviderServer.Register (2026-09-23, Parita-5-Backend-Residui §2): accetta QUALUNQUE
 * kind (solo name non-vuoto richiesto; il ruolo si normalizza come sopra). La validazione
 * sorgente stretta di gmb_provider_register vale SOLO per la forma statement
 * (AI REGISTER alias, capability, source → RegisterSource). Prima la forma funzione
 * riusava la validazione stretta → kind="ollama" nudo rifiutato (vm_error) mentre IL lo
 * registrava (ProviderConfig/Configure/HealthCheck). */
int gmb_provider_register_kind(const char* role, const char* name, const char* kind, const char* config) {
    if (!name || !*name) return 0;
    char rolebuf[32]; snprintf(rolebuf, sizeof rolebuf, "%s", gmb_provider_normalize_role(role));
    role = rolebuf;
    if (kind && strstr(kind, "GGUF")) return 0;
    gmb_provider_t* p = find_provider(role, name);
    if (p) { strncpy(p->kind, kind ? kind : "", 511); strncpy(p->config, config ? config : "", 511); return 1; }
    if (g_provider_count < AI_MAX_PROVIDERS) {
        gmb_provider_t* np = &g_providers[g_provider_count];
        strncpy(np->role, role, 31); strncpy(np->name, name, 63);
        strncpy(np->kind, kind ? kind : "", 511); strncpy(np->config, config ? config : "", 511);
        np->weight = 1; np->priority = 0; np->status = 1; np->fail_count = 0; np->wrr_acc = 0;
        g_provider_count++;
        return 1;
    }
    return 0;
}
/* Snapshot del provider embedding pinnato dall'ultimo SET ACTIVE (parità C#
 * EmbeddingProviderResolver.Default: aggiornato SOLO da SetActive, non da ROTATE). */
static char g_embedding_snapshot[64] = "";
const char* gmb_provider_embedding_snapshot(void) { return g_embedding_snapshot; }

int gmb_provider_set_active(const char* role, const char* name) {
    /* Normalizza il ruolo (parità C# ProviderRole.FromCapabilityOrRole e di gmb_provider_active):
     * `AI SET ACTIVE %CHAT, "x"` arriva come "0"/"%CHAT" dal bytecode/emitter C. COPIA LOCALE
     * (vedi gmb_provider_register: find_provider→ensure_defaults sovrascrive il buffer statico
     * di gmb_provider_normalize_role). */
    char rolebuf[32]; snprintf(rolebuf, sizeof rolebuf, "%s", gmb_provider_normalize_role(role));
    role = rolebuf;
    gmb_provider_t* p = find_provider(role, name);
    if (!p) return 0;
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].role, role) == 0) g_providers[i].active = (g_providers+i == p);
    p->active = 1;
    /* Pinnaggio del resolver embedding (parità C# EmbeddingProviderResolver.Default,
     * 2026-09-23, Parita-5-Backend-Residui §2 — ProviderConfig): il C# aggiorna il resolver
     * SOLO su SetActive (IlRuntime.ProviderSetActive → Default = TryGetActiveEmbedding),
     * NON su ROTATE. Il C lo leggeva dinamicamente via gmb_provider_active → dopo ROTATE
     * il provider cambiava e AI RAG divergeva (VMC abortiva mentre IL proseguiva col
     * fixture pinnato). Vedi gmb_provider_embedding_snapshot(). */
    if (strcmp(role, "embedding") == 0)
        snprintf(g_embedding_snapshot, sizeof g_embedding_snapshot, "%s", p->name);
    return 1;
}
/* `AI SET ACTIVE %EMBEDDING, <name>` ESPLICITO dal programma BASIC: rende legittimo il
 * fixture embedding (il fixture DEFAULT, attivato dal bootstrap del host, no). Parità
 * IlRuntime (P0, 2026-09-12). NON usare dal bootstrap del host (lì resta gmb_provider_set_active). */
int gmb_provider_set_active_explicit(const char* role, const char* name) {
    char rbuf[32]; snprintf(rbuf, sizeof rbuf, "%s", gmb_provider_normalize_role(role));
    int r = gmb_provider_set_active(rbuf, name);
    if (r && strcmp(rbuf, "embedding") == 0) g_embedding_set_explicitly = 1;
    return r;
}
/* L'embedding provider è stato impostato ESPLICITAMENTE (AI SET ACTIVE %EMBEDDING)? */
int gmb_provider_embedding_explicit(void) { ensure_defaults(); return g_embedding_set_explicitly; }
const char* gmb_provider_active(const char* role) {
    static char name[64];
    /* Copia locale: ensure_defaults() (sotto) può richiamare gmb_provider_normalize_role e
     * sovrascrivere il suo buffer statico. */
    char rolebuf[32]; snprintf(rolebuf, sizeof rolebuf, "%s", gmb_provider_normalize_role(role));
    role = rolebuf;
    ensure_defaults();
    /* Parità C# GetNextHealthyLocked: candidati del ruolo ordinati per priority desc,
     * poi pin (SetActive/Rotate) desc, poi registrazione; vince il primo Available;
     * il pin si sposta sul vincitore. Prima: flag active o primo del ruolo, senza
     * salute/priorità (FAIL non spostava mai l'attivo). */
    int idx[64], n = 0;
    for (int i = 0; i < g_provider_count && n < 64; i++)
        if (strcmp(g_providers[i].role, role) == 0) idx[n++] = i;
    if (n == 0) return "";
    int pinned = -1;
    for (int k = 0; k < n; k++) if (g_providers[idx[k]].active) { pinned = idx[k]; break; }
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++) {
            int a = idx[i], b = idx[j];
            int swap = 0;
            if (g_providers[b].priority != g_providers[a].priority)
                swap = g_providers[b].priority > g_providers[a].priority;
            else if ((b == pinned) != (a == pinned))
                swap = (b == pinned);
            else swap = b < a;
            if (swap) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        }
    for (int k = 0; k < n; k++) {
        if (g_providers[idx[k]].status == 1) {
            for (int i = 0; i < g_provider_count; i++)
                g_providers[i].active = (i == idx[k] && strcmp(g_providers[i].role, role) == 0);
            strncpy(name, g_providers[idx[k]].name, 63); name[63] = 0; return name;
        }
    }
    return "";
}
/* Path del modello locale MLX dell'provider COMPLETION attivo (source `.gguf` o cartella repo
 * MLX), oppure NULL se l'attivo non è un modello locale. Usato per l'auto-wire del brain MLX
 * nei runtime C (gmb_agent_loop.c): `AI REGISTER <path>, AI SET ACTIVE, AI.AGENTASKSYNC` carica
 * il motore MLX embedded — parità col routing host C# (MlxLlmCompleter). Ritorna un buffer
 * statico (vivo fino alla prossima chiamata). */
/* Kind del provider attivo per un ruolo (es. "local"/"rules"/"mock" per %DECIDE).
 * "" se il ruolo non ha provider. */
const char* gmb_provider_active_kind(const char* role) {
    static char kind[512];
    kind[0] = 0;
    char rbuf[32];
    snprintf(rbuf, sizeof rbuf, "%s", gmb_provider_normalize_role(role));
    const char* name = gmb_provider_active(rbuf);
    if (!name || !*name) return kind;
    gmb_provider_t* p = find_provider(rbuf, name);
    if (!p) return kind;
    snprintf(kind, sizeof kind, "%s", p->kind);
    return kind;
}

const char* gmb_provider_active_model_path(void) {
    ensure_defaults();
    static char out[1200];
    for (int i = 0; i < g_provider_count; i++) {
        if (strcmp(g_providers[i].role, "completion") != 0 || !g_providers[i].active) continue;
        if (gmb_ai_is_local_mlx_source(g_providers[i].kind)) {
            snprintf(out, sizeof out, "%s", g_providers[i].kind);
            return out;
        }
        char model[512]; provider_model_path(&g_providers[i], model, sizeof model);
        if (gmb_ai_is_local_mlx_source(model)) { snprintf(out, sizeof out, "%s", model); return out; }
    }
    return NULL;
}
const char* gmb_provider_rotate(const char* role) {
    static char name[64];
    char rolebuf[32]; snprintf(rolebuf, sizeof rolebuf, "%s", gmb_provider_normalize_role(role));
    role = rolebuf;
    ensure_defaults();
    /* Parità C# Rotate: candidati in ordine di priorità; senza pesi custom → round-robin
     * dalla posizione pinnata, saltando i non-Available; con pesi → smooth WRR nel tier a
     * priorità massima con membri Available (stesso algoritmo di nginx). */
    int idx[64], n = 0;
    for (int i = 0; i < g_provider_count && n < 64; i++)
        if (strcmp(g_providers[i].role, role) == 0) idx[n++] = i;
    if (n == 0) return "";
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++) {
            int a = idx[i], b = idx[j];
            int swap = 0;
            if (g_providers[b].priority != g_providers[a].priority)
                swap = g_providers[b].priority > g_providers[a].priority;
            else swap = b < a;
            if (swap) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        }
    int custom = 0;
    for (int k = 0; k < n; k++) if (g_providers[idx[k]].weight != 1) { custom = 1; break; }
    int win = -1;
    if (!custom) {
        int pinned = -1;
        for (int k = 0; k < n; k++) if (g_providers[idx[k]].active) { pinned = k; break; }
        /* Default C# (_activeIndex=0 → order[0] = primo registrato): senza pin, si parte
         * dal primo registrato (non dal primo per priorità). */
        if (pinned < 0) {
            int firstReg = idx[0];
            for (int k = 1; k < n; k++) if (idx[k] < firstReg) firstReg = idx[k];
            for (int k = 0; k < n; k++) if (idx[k] == firstReg) { pinned = k; break; }
        }
        for (int step = 1; step <= n; step++) {
            int k = (pinned + step) % n;
            if (g_providers[idx[k]].status == 1) { win = k; break; }
        }
    } else {
        /* tier a priorità massima con membri Available */
        int top = -1;
        for (int k = 0; k < n; k++) {
            if (g_providers[idx[k]].status != 1) continue;
            if (top < 0 || g_providers[idx[k]].priority > g_providers[idx[top]].priority) top = k;
        }
        if (top >= 0) {
            int prio = g_providers[idx[top]].priority;
            int total = 0, best = -1;
            for (int k = 0; k < n; k++) {
                if (g_providers[idx[k]].status != 1 || g_providers[idx[k]].priority != prio) continue;
                int w = g_providers[idx[k]].weight;
                total += w;
                g_providers[idx[k]].wrr_acc += w;
                if (best < 0 || g_providers[idx[best]].wrr_acc < g_providers[idx[k]].wrr_acc) best = k;
            }
            if (best >= 0) { g_providers[idx[best]].wrr_acc -= total; win = best; }
        }
    }
    if (win < 0) return "";
    for (int i = 0; i < g_provider_count; i++)
        g_providers[i].active = (i == idx[win] && strcmp(g_providers[i].role, role) == 0);
    strncpy(name, g_providers[idx[win]].name, 63); name[63] = 0; return name;
}
char* gmb_provider_list(const char* role) {
    char* out = (char*)malloc(8192); if (!out) return NULL;
    ensure_defaults();
    char rolebuf[32]; snprintf(rolebuf, sizeof rolebuf, "%s", gmb_provider_normalize_role(role));
    role = rolebuf;
    const char* eff = role;
    int any = 0;
    for (int i = 0; i < g_provider_count; i++) if (strcmp(g_providers[i].role, role) == 0) { any = 1; break; }
    if (!any) eff = "completion";   /* ruolo ignoto: fallback (parita' C# List) */
    size_t len = 0;
    out[len++] = '[';
    int first = 1;
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].role, eff) == 0) {
            int w = snprintf(out + len, 8192 - len, "%s%s", first ? "" : ",", g_providers[i].name);
            if (w > 0) len += (size_t)w;
            first = 0;
        }
    if (len < 8191) out[len++] = ']';
    out[len] = 0;
    return out;
}
int gmb_provider_unregister(const char* role, const char* name) {
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].role, role) == 0 && strcmp(g_providers[i].name, name) == 0) {
            for (int j = i; j < g_provider_count-1; j++) g_providers[j] = g_providers[j+1];
            g_provider_count--; return 1;
        }
    return 0;
}
/* Unregister/set_weight PER NOME (qualsiasi ruolo) — parità C# ProviderServer.Unregister/SetWeight. */
int gmb_provider_unregister_name(const char* name) {
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0) {
            for (int j = i; j < g_provider_count-1; j++) g_providers[j] = g_providers[j+1];
            g_provider_count--; return 1;
        }
    return 0;
}
int gmb_provider_set_weight_name(const char* name, int weight) {
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0) { g_providers[i].weight = weight; return 1; }
    return 0;
}
/* by-name (parità C#: weight/fail/reset per nome) */
double gmb_provider_weight_name(const char* name) {
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0) return (double)g_providers[i].weight;
    return -1;   /* provider ignoto (parita' C# KeyNotFound -> -1) */
}
int gmb_provider_fail_name(const char* name) {
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0) { g_providers[i].fail_count++; g_providers[i].status = 0; return 1; }
    return 0;
}
int gmb_provider_reset_failed_name(const char* name) {
    /* Nome vuoto = reset-all (parità C# ResetFailed("")→ResetAllFailed): solo Unavailable.
     * Con nome: parità ResetFailed (incondizionato, anche da Blocked/Damaged). */
    if (!name || !*name) {
        for (int i = 0; i < g_provider_count; i++)
            if (g_providers[i].status == 0) { g_providers[i].fail_count = 0; g_providers[i].status = 1; }
        return 1;
    }
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0) {
            g_providers[i].fail_count = 0; g_providers[i].status = 1; return 1;
        }
    return 0;
}
double gmb_provider_priority_name(const char* name) {
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0) return (double)g_providers[i].priority;
    return -1;   /* provider ignoto (parita' C#) */
}
int gmb_provider_set_priority_name(const char* name, int pri) {
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0) {
            g_providers[i].priority = pri;
            return 1;
        }
    return 0;
}
/* Lunghezza della risposta del FixtureLlmCompleter per un dato prompt
 * (parità CommonLib C# FixtureLlmCompleter.Complete nel caso generico:
 * "[<question>] <snippet>" oppure "[<question>] (insufficient context)"). */
static int gmb_fixture_completion_len(const char* userPrompt) {
    if (!userPrompt) return 0;
    const char* qm = strstr(userPrompt, "Question: ");
    const char* q = qm ? qm + 10 : userPrompt;
    while (*q == ' ' || *q == '\t') q++;
    size_t qlen = strlen(q);
    while (qlen > 0 && (q[qlen-1] == ' ' || q[qlen-1] == '\n' || q[qlen-1] == '\r' || q[qlen-1] == '\t')) qlen--;
    const char* sp = strstr(userPrompt, ") ");
    if (sp) {
        sp += 2;
        const char* nl = strchr(sp, '\n');
        size_t slen = nl ? (size_t)(nl - sp) : strlen(sp);
        while (slen > 0 && (sp[slen-1] == ' ' || sp[slen-1] == '\t')) slen--;
        if (slen > 0) return (int)(1 + qlen + 2 + slen);
    }
    return (int)(1 + qlen + 2 + strlen("(insufficient context)"));
}
/* Estrae il path del modello dalla config del provider (key=value o JSON {"model":...}). */
static void provider_model_path(const gmb_provider_t* p, char* out, size_t cap) {
    out[0] = 0;
    if (!p) return;
    /* `config` è NULL per i provider di default (fixture): prima `strstr(p->config, ...)`
     * crashava quando `gmb_provider_active_model_path()` scandiva il registro senza un
     * modello MLX registrato. */
    const char* cfg = p->config ? p->config : "";
    const char* c = cfg;
    while (c && *c) {
        if (strncmp(c, "model=", 6) == 0) {
            size_t k = 0; c += 6;
            while (*c && *c != '\n' && k + 1 < cap) out[k++] = *c++;
            out[k] = 0; return;
        }
        c = strchr(c, '\n'); if (c) c++;
    }
    /* Chiave JSON `model`, oppure `modelDir` (forma AI REGISTER "mlx", "{modelDir:...}"). */
    const char* m = strstr(cfg, "\"model\"");
    if (!m) m = strstr(cfg, "\"modelDir\"");
    if (m) {
        m = strchr(m, ':'); if (m) { m++; while (*m == ' ' || *m == '"') m++;
            size_t k = 0; while (*m && *m != '"' && k + 1 < cap) out[k++] = *m++; out[k] = 0; }
    }
}
char* gmb_provider_test(const char* role, const char* name) {
    static char buf[256];
    gmb_provider_t* p = find_provider(role, name);
    if (!p) { snprintf(buf, sizeof(buf), "ERROR: Provider '%s' not found for role '%s'.", name, role); return buf; }
    /* Parità C# TestConnection: per i kind che richiedono un modello su disco, l'istanza
     * non è costruibile se il file manca → errore esplicito (OnnxEmbeddingProvider ctor).
     * Prima: sempre OK → ProviderConfigure divergeva (my-onnx inesistente = OK su C). */
    if (strcmp(p->kind, "onnx") == 0) {
        char model[512]; provider_model_path(p, model, sizeof(model));
        struct stat st;
        if (!model[0] || stat(model, &st) != 0) {
            /* parità C# TestConnection→GetOrCreateInstanceLocked: il fallimento marca il
             * provider Unavailable (failover reale) — poi ACTIVE/ROTATE lo saltano. */
            p->status = 0; p->fail_count++;
            snprintf(buf, sizeof(buf), "ERROR: %s failed: ONNX model file not found.", name);
            return buf;
        }
    }
    if (strcmp(role, "embedding") == 0)
        snprintf(buf, sizeof(buf), "OK: embedding provider '%s' (%s), dims=4", name, p->kind);
    else if (strcmp(role, "reranker") == 0)
        snprintf(buf, sizeof(buf), "OK: reranker '%s' (%s), score=0.0000", name, p->kind);
    else
        snprintf(buf, sizeof(buf), "OK: completion provider '%s' (%s), response=%d chars",
                 name, p->kind, gmb_fixture_completion_len("test"));
    return buf;
}
int gmb_provider_set_weight(const char* role, const char* name, int weight) {
    gmb_provider_t* p = find_provider(role, name); if (!p) return 0;
    p->weight = weight; return 1;
}
int gmb_provider_set_priority(const char* role, const char* name, int pri) {
    gmb_provider_t* p = find_provider(role, name); if (!p) return 0;
    p->priority = pri; return 1;
}
int gmb_provider_fail(const char* role, const char* name) {
    gmb_provider_t* p = find_provider(role, name); if (!p) return 0;
    p->fail_count++; p->status = 0; return 1;   /* 0 = Unavailable (parità MarkFailed) */
}
int gmb_provider_reset_failed(const char* role) {
    for (int i = 0; i < g_provider_count; i++)
        /* Solo Unavailable → Available (Blocked/Damaged intoccati, parità ResetAllFailed). */
        if (strcmp(g_providers[i].role, role) == 0 && g_providers[i].status == 0) { g_providers[i].fail_count = 0; g_providers[i].status = 1; }
    return 1;
}
int gmb_provider_set_status(const char* role, const char* name, int status) {
    gmb_provider_t* p = find_provider(role, name); if (!p) return 0;
    p->status = status; return 1;
}
int gmb_provider_status(const char* role, const char* name) {
    gmb_provider_t* p = find_provider(role, name); if (!p) return 0;
    return p->status;
}

/* AIVECTOR: implementato in gmb_vectordb.c (SQLite + embedding fixture). */

/* --- MCP: registro tool/agent in-memory (parità McpToolRegistry C#) ---
 * Il registro tiene nome, procedura, descrizione, flag read-only e inputSchema JSON
 * (passato dal chiamante: il compilatore C#/emitter C lo derivano dai parametri della
 * FUNCTION a compile-time). gmb_mcp_list_tools produce lo STESSO JSON di
 * McpToolRegistry.ToMcpToolList().ToJsonString() del C# (chiavi in ordine
 * name, description, inputSchema[, readOnly], sort case-insensitive per nome),
 * così AI.MCPLISTTOOLS() è identico su tutti i backend. */
#define MCP_MAX_TOOLS 32
#define MCP_MAX_AGENTS 16
typedef struct {
    char name[64];
    char proc[64];
    char desc[192];
    int readonly;
    char schema[1024];  /* inputSchema JSON; "" → {} */
    char oschema[512];  /* outputSchema JSON opzionale; "" → non emesso */
} gmb_mcp_tool_t;
static gmb_mcp_tool_t g_mcp_tools[MCP_MAX_TOOLS];
static char g_mcp_agents[MCP_MAX_AGENTS][64];
static int g_mcp_tool_count = 0, g_mcp_agent_count = 0;

static gmb_mcp_tool_t* mcp_tool_find(const char* name) {
    for (int i = 0; i < g_mcp_tool_count; i++) if (strcmp(g_mcp_tools[i].name, name) == 0) return &g_mcp_tools[i];
    return NULL;
}

/* mini string-builder locale (heap, terminato; ritorna NULL su OOM) */
typedef struct { char* data; size_t len, cap; } mcp_sb_t;
static int mcp_sb_put(mcp_sb_t* sb, const char* s, size_t n) {
    if (sb->len + n + 1 > sb->cap) {
        size_t nc = sb->cap ? sb->cap * 2 : 256;
        while (nc < sb->len + n + 1) nc *= 2;
        char* nd = (char*)realloc(sb->data, nc);
        if (!nd) return 0;
        sb->data = nd; sb->cap = nc;
    }
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return 1;
}
static int mcp_sb_puts(mcp_sb_t* sb, const char* s) { return mcp_sb_put(sb, s, strlen(s)); }
static int mcp_sb_putc(mcp_sb_t* sb, char c) { return mcp_sb_put(sb, &c, 1); }

static void mcp_json_escape(mcp_sb_t* sb, const char* s) {
    /* Parità System.Text.Json (helper condiviso §0bis): prima mancavano '/`/non-ASCII
     * (McpBrowserServer: "→" vs "\u2192"). */
    if (!s || !*s) return;
    size_t n = strlen(s);
    char* tmp = (char*)malloc(n * 6 + 1);
    if (!tmp) return;
    tmp[gmb_json_escape(tmp, n * 6 + 1, s)] = '\0';
    mcp_sb_puts(sb, tmp);
    free(tmp);
}

/* Registrazione completa (parità IlRuntime.McpRegisterTool C#). schema_json può
 * essere NULL/"" → inputSchema {}. Ritorna 1 successo, 0 registro pieno. */
int gmb_mcp_register_tool_ex(const char* name, const char* proc, const char* desc, int readonly,
                             const char* schema_json, const char* output_schema_json) {
    if (!name || !*name) return 0;
    const char* schema = (schema_json && *schema_json) ? schema_json : "{}";
    const char* oschema = (output_schema_json && *output_schema_json) ? output_schema_json : "";
    gmb_mcp_tool_t* t = mcp_tool_find(name);
    if (!t) {
        if (g_mcp_tool_count >= MCP_MAX_TOOLS) return 0;
        t = &g_mcp_tools[g_mcp_tool_count++];
        memset(t, 0, sizeof(*t));
        snprintf(t->name, sizeof(t->name), "%s", name);
    }
    snprintf(t->proc, sizeof(t->proc), "%s", proc ? proc : "");
    snprintf(t->desc, sizeof(t->desc), "%s", desc ? desc : "");
    t->readonly = readonly ? 1 : 0;
    snprintf(t->schema, sizeof(t->schema), "%s", schema);
    snprintf(t->oschema, sizeof(t->oschema), "%s", oschema);
    return 1;
}

/* Firma storica (inputSchema presente, outputSchema assente): wrapper, una sola
 * implementazione reale (_ex) — §0bis, nessuna duplicazione. */
int gmb_mcp_register_tool_full(const char* name, const char* proc, const char* desc, int readonly, const char* schema_json) {
    return gmb_mcp_register_tool_ex(name, proc, desc, readonly, schema_json, "");
}

/* Registrazione legacy minimale (solo nome, schema {}): parità con la firma
 * precedente usata dagli emitter C che non derivano lo schema. */
int gmb_mcp_register_tool(const char* name) {
    return gmb_mcp_register_tool_full(name, "", "", 0, "{}");
}
int gmb_mcp_unregister_tool(const char* name) {
    for (int i = 0; i < g_mcp_tool_count; i++)
        if (strcmp(g_mcp_tools[i].name, name) == 0) {
            for (int j = i; j < g_mcp_tool_count-1; j++) g_mcp_tools[j] = g_mcp_tools[j+1];
            g_mcp_tool_count--; return 1;
        }
    return 0;
}
char* gmb_mcp_list_tools(void) {
    /* JSON identico a McpToolRegistry.ToMcpToolList().ToJsonString() (C#):
     * sort case-insensitive per nome; description default "BASIC tool <proc>" se
     * vuota e proc nota; readOnly solo se true. */
    int idx[MCP_MAX_TOOLS];
    for (int i = 0; i < g_mcp_tool_count; i++) idx[i] = i;
    for (int i = 0; i < g_mcp_tool_count - 1; i++)
        for (int j = i + 1; j < g_mcp_tool_count; j++)
            if (strcasecmp(g_mcp_tools[idx[j]].name, g_mcp_tools[idx[i]].name) < 0) {
                int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
            }
    mcp_sb_t sb = {0};
    if (!mcp_sb_putc(&sb, '[')) return NULL;
    for (int k = 0; k < g_mcp_tool_count; k++) {
        const gmb_mcp_tool_t* t = &g_mcp_tools[idx[k]];
        if (k && !mcp_sb_putc(&sb, ',')) { free(sb.data); return NULL; }
        if (!mcp_sb_puts(&sb, "{\"name\":\"")) { free(sb.data); return NULL; }
        mcp_json_escape(&sb, t->name);
        if (!mcp_sb_puts(&sb, "\",\"description\":\"")) { free(sb.data); return NULL; }
        if (t->desc[0]) mcp_json_escape(&sb, t->desc);
        else if (t->proc[0]) { mcp_sb_puts(&sb, "BASIC tool "); mcp_json_escape(&sb, t->proc); }
        if (!mcp_sb_puts(&sb, "\",\"inputSchema\":")) { free(sb.data); return NULL; }
        if (!mcp_sb_puts(&sb, t->schema[0] ? t->schema : "{}")) { free(sb.data); return NULL; }
        /* Ordine di chiavi = McpToolRegistry.ToMcpToolList() (C#): inputSchema,
         * outputSchema (solo se dichiarato), readOnly (solo se true). */
        if (t->oschema[0]) {
            if (!mcp_sb_puts(&sb, ",\"outputSchema\":")) { free(sb.data); return NULL; }
            if (!mcp_sb_puts(&sb, t->oschema)) { free(sb.data); return NULL; }
        }
        if (t->readonly && !mcp_sb_puts(&sb, ",\"readOnly\":true")) { free(sb.data); return NULL; }
        if (!mcp_sb_puts(&sb, "}")) { free(sb.data); return NULL; }
    }
    if (!mcp_sb_putc(&sb, ']')) { free(sb.data); return NULL; }
    return sb.data; /* heap — caller free() */
}
/* Accessor per il server MCP (gmb_mcp_server.c): il registro resta privato qui,
 * il server interroga solo cio' che gli serve (esistenza, readonly, procedura, flag). */
int gmb_mcp_tool_exists(const char* name) {
    if (!name || !*name) return 0;
    /* parita' C# (Dictionary OrdinalIgnoreCase): confronto case-insensitive */
    for (int i = 0; i < g_mcp_tool_count; i++)
        if (strcasecmp(g_mcp_tools[i].name, name) == 0) return 1;
    return 0;
}
int gmb_mcp_tool_is_readonly(const char* name) {
    if (!name || !*name) return 0;
    for (int i = 0; i < g_mcp_tool_count; i++)
        if (strcasecmp(g_mcp_tools[i].name, name) == 0) return g_mcp_tools[i].readonly;
    return 0;
}
/* inputSchema JSON del tool ("{}" se assente) — vedi gmb_runtime.h. */
static char g_mcp_schema_buf[1024];
const char* gmb_mcp_tool_schema(const char* name) {
    if (!name || !*name) return "{}";
    for (int i = 0; i < g_mcp_tool_count; i++)
        if (strcasecmp(g_mcp_tools[i].name, name) == 0) {
            snprintf(g_mcp_schema_buf, sizeof(g_mcp_schema_buf), "%s",
                     g_mcp_tools[i].schema[0] ? g_mcp_tools[i].schema : "{}");
            return g_mcp_schema_buf;
        }
    return "{}";
}

/* Nome della PROCEDURA BASIC mappata al tool ("" se il tool non esiste). */
const char* gmb_mcp_tool_proc(const char* name) {
    static char buf[64];
    if (!name || !*name) return "";
    for (int i = 0; i < g_mcp_tool_count; i++)
        if (strcasecmp(g_mcp_tools[i].name, name) == 0) {
            snprintf(buf, sizeof(buf), "%s", g_mcp_tools[i].proc);
            return buf;
        }
    return "";
}

/* Parità IlRuntime.McpRegisterAgent (IL/PCode): oltre a ricordare l'agente, registra un
 * TOOL chiamato `agent-<nome>` con descrizione `Agent: <nome>` e inputSchema {} — prima
 * il runtime C si limitava a memorizzare il nome, quindi AI.MCPLISTTOOLS() non conteneva
 * il tool e divergeva dai backend gestiti. */
int gmb_mcp_register_agent(const char* name) {
    if (!name || !*name) return 0;
    char tool[96], desc[128];
    snprintf(tool, sizeof(tool), "agent-%s", name);
    snprintf(desc, sizeof(desc), "Agent: %s", name);
    /* QualifiedProcedureName: stessa forma del C# (invocazione per nome dell'agente). */
    char proc[160];
    snprintf(proc, sizeof(proc), "AIAGENT.ASK(\"%s\", prompt$)", name);
    int ok = gmb_mcp_register_tool_ex(tool, proc, desc, 0, "{}", "");
    if (g_mcp_agent_count < MCP_MAX_AGENTS) {
        strncpy(g_mcp_agents[g_mcp_agent_count++], name, 63);
        g_mcp_agents[g_mcp_agent_count - 1][63] = '\0';
    }
    return ok;
}

/* --- DBF/ISAM: stato work-area in-memory minimale --- */
typedef struct { char alias[64]; int active; int recno; int lastrec; int found; int eof; int bof; int deleted; } gmb_wa_t;
static gmb_wa_t g_wa[16];
static int g_wa_count = 0;

static gmb_wa_t* wa_or_new(const char* alias) {
    for (int i = 0; i < g_wa_count; i++) if (strcmp(g_wa[i].alias, alias) == 0) return &g_wa[i];
    if (g_wa_count < 16) {
        gmb_wa_t* w = &g_wa[g_wa_count++];
        strncpy(w->alias, alias ? alias : "", 63); w->active = 1; w->recno = 0; w->lastrec = 0;
        w->found = 0; w->eof = 0; w->bof = 0; w->deleted = 0;
        return w;
    }
    return NULL;
}
int gmb_dbf_selected(const char* alias) { return wa_or_new(alias) != NULL ? -1 : 0; }
const char* gmb_dbf_alias(const char* alias) { static char buf[64]; strncpy(buf, alias ? alias : "", 63); return buf; }
int gmb_dbf_used(const char* alias) { gmb_wa_t* w = wa_or_new(alias); return w && w->active ? -1 : 0; }
int gmb_dbf_lastrec(const char* alias) { gmb_wa_t* w = wa_or_new(alias); return w ? w->lastrec : 0; }
int gmb_dbf_eof(void) { return g_wa_count > 0 ? g_wa[g_wa_count-1].eof ? -1 : 0 : 0; }
int gmb_dbf_bof(void) { return g_wa_count > 0 ? g_wa[g_wa_count-1].bof ? -1 : 0 : 0; }
int gmb_dbf_found(void) { return g_wa_count > 0 ? g_wa[g_wa_count-1].found ? -1 : 0 : 0; }
int gmb_dbf_deleted(const char* alias) { gmb_wa_t* w = wa_or_new(alias); return w && w->deleted ? -1 : 0; }

int gmb_provider_set_status_name(const char* name, const char* status) {
    gmb_provider_t* p = find_provider("", name); if (!p) return 0;
    if (!status) return 0;
    char lo[32]; size_t i = 0;
    while (status[i] && i + 1 < sizeof(lo)) { char c = status[i]; lo[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; i++; }
    lo[i] = 0;
    /* Parità enum C# (AVAILABLE/UNAVAILABLE/BLOCKED/DAMAGED); OFFLINE/0 storici → Unavailable. */
    if (!strcmp(lo, "AVAILABLE") || !strcmp(lo, "ONLINE") || !strcmp(lo, "1")) p->status = 1;
    else if (!strcmp(lo, "UNAVAILABLE") || !strcmp(lo, "OFFLINE") || !strcmp(lo, "0")) p->status = 0;
    else if (!strcmp(lo, "BLOCKED")) p->status = 2;
    else if (!strcmp(lo, "DAMAGED")) p->status = 3;
    else return 0;
    return 1;
}

const char* gmb_provider_kind(const char* name) {
    gmb_provider_t* p = find_provider("", name);
    return p ? p->kind : "";
}
/* gmb_ai_info_fmt: "Provider '<name>': k=v, ..." (parita' C# AiInfo/GetConfigMap). */
char* gmb_ai_info_fmt(const char* name) {
    char* out = (char*)malloc(2048);
    if (!out) return NULL;
    if (!name || !*name) name = gmb_ai_get_default("completion");
    gmb_provider_t* p = find_provider("", name);
    size_t len = (size_t)snprintf(out, 2048, "Provider '%s': ", name ? name : "");
    if (p) {
        char* c = p->config; int first = 1;
        while (c && *c) {
            char* e = strchr(c, '\n');
            size_t l = e ? (size_t)(e - c) : strlen(c);
            if (l > 0 && len < 2040) { len += (size_t)snprintf(out + len, 2048 - len, "%s%.*s", first ? "" : ", ", (int)l, c); first = 0; }
            if (!e) break; c = e + 1;
        }
    }
    return out;
}
const char* gmb_provider_status_name(const char* name) {
    static char buf[192];
    gmb_provider_t* p = find_provider("", name);
    if (!p) { snprintf(buf, sizeof(buf), "ERROR: Provider '%s' is not registered.", name ? name : ""); return buf; }
    /* Parità enum C# (ToUpperInvariant): AVAILABLE/UNAVAILABLE/BLOCKED/DAMAGED. */
    return p->status == 1 ? "AVAILABLE" : p->status == 2 ? "BLOCKED" : p->status == 3 ? "DAMAGED" : "UNAVAILABLE";
}

int gmb_provider_run_healthcheck(void) {
    /* Parità C# ReportHealthResult: per provider, SOLO toggle Available/Unavailable —
     * mai override di Blocked/Damaged manuali. fixture/keyword (deterministici) → Available;
     * onnx → file modello esistente, altrimenti Unavailable; altri kind → invariati. */
    int ok = 0;
    for (int i = 0; i < g_provider_count; i++) {
        gmb_provider_t* p = &g_providers[i];
        if (p->status == 2 || p->status == 3) { if (p->status) ok++; continue; }
        if (!strcmp(p->kind, "fixture") || !strcmp(p->kind, "keyword")) {
            p->status = 1; p->fail_count = 0; ok++;
        } else if (!strcmp(p->kind, "onnx")) {
            char model[512];
            provider_model_path(p, model, sizeof(model));
            struct stat st;
            if (model[0] && stat(model, &st) == 0) { p->status = 1; p->fail_count = 0; ok++; }
            else { p->status = 0; p->fail_count++; }
        }
    }
    (void)ok;
    return 1;   /* parità C# ProviderRunHealthCheck: RunSweep() poi SEMPRE 1.0 */
}

/* --- Config provider AI (apiKey/model/temperature/baseUrl/...) reale ---
 * Il config è una stringa "key=value\n..." dentro gmb_provider_t.config.
 * GET legge la chiave (stringa), SET la sostituisce o la appende. */
const char* gmb_ai_get_config(const char* name, const char* key) {
    static char val[512];
    val[0] = 0;
    gmb_provider_t* p = find_provider("", name);
    if (!p || !key) return val;
    size_t klen = strlen(key);
    char* c = p->config;
    while (c && *c) {
        if (strncmp(c, key, klen) == 0 && c[klen] == '=') {
            char* e = strchr(c + klen + 1, '\n');
            size_t vlen = e ? (size_t)(e - (c + klen + 1)) : strlen(c + klen + 1);
            if (vlen > 511) vlen = 511;
            memcpy(val, c + klen + 1, vlen);
            val[vlen] = 0;
            return val;
        }
        c = strchr(c, '\n');
        if (c) c++;
    }
    /* Fallback JSON: la config può essere JSON ({"key":"value"} / {"key":value}), forma usata
     * da AI REGISTER e dai provider locali (come fa provider_model_path per "model"). Senza,
     * le chiavi impostate via JSON (es. thinkLevel) non si leggono. */
    {
        char pat[96];
        snprintf(pat, sizeof(pat), "\"%s\"", key);
        const char* j = strstr(p->config, pat);
        while (j) {
            const char* k = j + strlen(pat);
            while (*k == ' ' || *k == '\t') k++;
            if (*k == ':') {
                k++;
                while (*k == ' ' || *k == '\t' || *k == '"') k++;
                size_t n = 0;
                while (*k && *k != '"' && *k != ',' && *k != '}' && *k != '\n' && n + 1 < sizeof(val))
                    val[n++] = *k++;
                while (n > 0 && (val[n - 1] == ' ' || val[n - 1] == '\t')) n--;
                val[n] = 0;
                return val;
            }
            j = strstr(j + 1, pat);
        }
    }
    return val;
}

int gmb_ai_set_config(const char* name, const char* key, const char* value) {
    gmb_provider_t* p = find_provider("", name);
    if (!p || !key || !value) return 0;
    size_t klen = strlen(key);
    char* c = p->config;
    char* last = p->config;
    while (c && *c) {
        if (strncmp(c, key, klen) == 0 && c[klen] == '=') {
            char* e = strchr(c, '\n');
            char* rest = e ? e + 1 : c + strlen(c);
            char tail[512]; tail[0] = 0;
            if (rest && *rest) snprintf(tail, sizeof(tail), "%s", rest);
            snprintf(c, 512 - (size_t)(c - p->config), "%s=%s\n%s", key, value, tail);
            return 1;
        }
        last = c;
        c = strchr(c, '\n');
        if (c) c++;
    }
    size_t used = strlen(p->config);
    if (used + klen + strlen(value) + 2 < sizeof(p->config))
        snprintf(p->config + used, sizeof(p->config) - used, "%s=%s\n", key, value);
    (void)last;
    return 1;
}

/* AI.INFO(name): JSON con i campi config del provider. */
char* gmb_ai_info(const char* name) {
    gmb_provider_t* p = find_provider("", name);
    if (!p) return strdup("{}");
    char* out = (char*)malloc(1024);
    if (!out) return strdup("{}");
    const char* key = gmb_ai_get_config(name, "apiKey");
    const char* model = gmb_ai_get_config(name, "model");
    const char* base = gmb_ai_get_config(name, "baseUrl");
    snprintf(out, 1024, "{\"name\":\"%s\",\"kind\":\"%s\",\"active\":%d,\"status\":%d,"
                         "\"apiKey\":\"%s\",\"model\":\"%s\",\"baseUrl\":\"%s\"}",
             p->name, p->kind, p->active, p->status, key ? key : "", model ? model : "", base ? base : "");
    return out;
}

/* AI.SETDEFAULT(role): imposta il provider attivo del ruolo. */
int gmb_ai_set_default(const char* name) {
    gmb_provider_t* p = find_provider("", name);
    if (!p) return 0;
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].role, p->role) == 0) g_providers[i].active = (g_providers[i].name == p->name || strcmp(g_providers[i].name, p->name) == 0);
    p->active = 1;
    return 1;
}

const char* gmb_ai_get_default(const char* role) {
    for (int i = 0; i < g_provider_count; i++)
        if ((!role || !*role || strcmp(g_providers[i].role, role) == 0) && g_providers[i].active)
            return g_providers[i].name;
    return gmb_provider_active(role);   /* default = primo provider del ruolo (parita' C#) */
}

double gmb_ai_get_credits(const char* name) {
    if (!find_provider("", name)) return -1.0;   /* provider ignoto: -1 (parita' C#) */
    const char* v = gmb_ai_get_config(name, "credits");
    return (v && *v) ? atof(v) : -1.0;
}
int gmb_ai_set_credits(const char* name, double credits) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.6g", credits);
    return gmb_ai_set_config(name, "credits", buf);
}

/* ── Provider V2 (C): isFree/costPerM/credits/blocked + providers.json — parità ProviderServer C#
 * (AI-Provider-Register-Unified / M5). Stato: status 1=ONLINE, 0=BLOCKED (convenzione esistente).
 * Crediti/costi in config ("credits","isFree","costPerM"). Mai stub. */
double gmb_provider_credits(const char* name) { return gmb_ai_get_credits(name); }
int gmb_provider_is_free(const char* name) {
    const char* v = gmb_ai_get_config(name, "isFree"); return v ? (strcmp(v, "1") == 0 || strcmp(v, "true") == 0 || atoi(v) != 0) : 0;
}
double gmb_provider_cost_per_m(const char* name) {
    const char* v = gmb_ai_get_config(name, "costPerM"); return v ? atof(v) : 0.0;
}
/* record_completion: se il provider ha crediti finiti (>=0) e costPerM>0, scala i crediti
 * per i token consumati; a <=0 → status 0 (BLOCKED). Credit -1/assente = illimitato. */
int gmb_provider_record_completion(const char* name, double tokens) {
    double credits = gmb_ai_get_credits(name);
    double costPerM = gmb_provider_cost_per_m(name);
    if (credits < 0) return 1;                       /* illimitato */
    double dec = costPerM > 0 ? (tokens / 1e6) * costPerM : 1.0;
    double rem = credits - dec;
    if (rem <= 0) { rem = 0; gmb_provider_set_status_name(name, "BLOCKED"); }
    char buf[64]; snprintf(buf, sizeof(buf), "%.6g", rem);
    gmb_ai_set_config(name, "credits", buf);
    return 1;
}
/* Verifica e blocca chi ha 0 crediti (es. dopo SetCredits 0). */
int gmb_provider_enforce_credits(const char* name) {
    if (gmb_ai_get_credits(name) == 0.0) gmb_provider_set_status_name(name, "BLOCKED");
    return 1;
}
/* providers.json v2 — info JSON di tutti i provider registrati. */
char* gmb_provider_v2_info(void) {
    size_t cap = 256;
    for (int i = 0; i < g_provider_count; i++) cap += 192;
    char* out = (char*)malloc(cap); if (!out) return NULL;
    size_t len = (size_t)snprintf(out, cap, "{\"providers\":[");
    for (int i = 0; i < g_provider_count; i++) {
        gmb_provider_t* p = &g_providers[i];
        len += (size_t)snprintf(out + len, cap - len, "%s{\"name\":\"%s\",\"role\":\"%s\",\"kind\":\"%s\",\"active\":%d,\"status\":%d,\"isFree\":%d,\"costPerM\":%.6g,\"credits\":%.6g}",
            i ? "," : "", p->name, p->role, p->kind, p->active, p->status,
            gmb_provider_is_free(p->name), gmb_provider_cost_per_m(p->name), gmb_ai_get_credits(p->name));
    }
    snprintf(out + len, cap - len, "]}");
    return out;
}
/* Salva il registry v2 su file. */
int gmb_provider_v2_save(const char* path) {
    if (!path || !*path) return 0;
    char* json = gmb_provider_v2_info(); if (!json) return 0;
    FILE* f = fopen(path, "w"); if (!f) { free(json); return 0; }
    fputs(json, f); fclose(f); free(json); return 1;
}
/* Carica providers.json v2: registra ogni provider (config crediti/isFree/costPerM nel config) e
 * applica lo stato. Formato parziale ma reale: {providers:[{name,role,kind,status,isFree,costPerM,credits}]}. */
int gmb_provider_v2_load(const char* path) {
    if (!path || !*path) return 0;
    FILE* f = fopen(path, "r"); if (!f) return 0;
    char buf[65536]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
    char* prov = strstr(buf, "\"providers\"");
    if (!prov) return 0;
    char* p = strchr(prov, '{');   /* oggetto { nome: {role,kind,...}, ... } */
    if (!p) return 0;
    int loaded = 0;
    char* q = p + 1;
    while (*q) {
        char* k = strchr(q, '"'); if (!k) break;
        char* ke = strchr(k + 1, '"'); if (!ke) break;
        char name[64]; size_t nl = (size_t)(ke - k - 1); if (nl > 63) nl = 63;
        memcpy(name, k + 1, nl); name[nl] = 0;
        char* obj = strchr(ke, '{'); if (!obj) break;
        int depth = 1; char* e = obj + 1;
        while (*e && depth > 0) { if (*e == '{') depth++; else if (*e == '}') depth--; e++; }
        char tok[4096]; size_t tl = (size_t)(e - obj); if (tl > sizeof(tok) - 1) tl = sizeof(tok) - 1;
        memcpy(tok, obj, tl); tok[tl] = 0;
        /* estrazione tollerante agli spazi: "role": "embedding" (con o senza spazio). */
        char role[32] = "", kind[32] = "";
        {   /* role */
            char* t = strstr(tok, "\"role\"");
            if (t && (t = strchr(t + 6, ':'))) { t++; while (*t == ' ' || *t == '\t') t++; if (*t == '"') { t++; char* z = strchr(t, '"'); if (z && z - t < 31) { memcpy(role, t, (size_t)(z - t)); role[z - t] = 0; } } }
        }
        {   /* kind */
            char* t = strstr(tok, "\"kind\"");
            if (t && (t = strchr(t + 6, ':'))) { t++; while (*t == ' ' || *t == '\t') t++; if (*t == '"') { t++; char* z = strchr(t, '"'); if (z && z - t < 31) { memcpy(kind, t, (size_t)(z - t)); kind[z - t] = 0; } } }
        }
        int isFree = 0, status = 1; double costPerM = 0, credits = -1;
        char* t;
        if (strstr(tok, "\"isFree\": true") || strstr(tok, "\"isFree\":true") || strstr(tok, "\"isFree\": 1") || strstr(tok, "\"isFree\":1")) isFree = 1;
        if (strstr(tok, "\"status\": \"Blocked\"") || strstr(tok, "\"status\":\"Blocked\"") || strstr(tok, "\"status\": 0") || strstr(tok, "\"status\":0")
            || strstr(tok, "\"status\": \"Unavailable\"") || strstr(tok, "\"status\":\"Unavailable\"")
            || strstr(tok, "\"status\": \"Damaged\"") || strstr(tok, "\"status\":\"Damaged\"")) status = 0;
        if ((t = strstr(tok, "\"costPerM\":"))) costPerM = atof(t + 10);
        if ((t = strstr(tok, "\"creditsRemaining\":"))) credits = atof(t + 19);
        if (name[0]) {
            char cfg[512]; snprintf(cfg, sizeof(cfg), "isFree=%d\ncostPerM=%.6g\ncredits=%.6g\n", isFree, costPerM, credits);
            /* Restore (non validazione sorgente): i kind salvati (es. "onnx" nudo) devono
             * rileggersi — parità C# RestoreSnapshotLocked. Prima usava la validazione
             * stretta → kind senza scheme scartati al LOAD (ProviderConfigure perdeva
             * my-onnx). Vedi gmb_provider_register_kind. */
            if (gmb_provider_register_kind(role[0] ? role : "completion", name, kind[0] ? kind : "local", cfg)) {
                if (status == 0) gmb_provider_set_status_name(name, "BLOCKED");
                loaded++;
            }
        }
        q = e;
        if (*q == ',') q++;
        else break;
    }
    return loaded;
}


/* AI.PROVIDERS(): parità C# IlRuntime.AiProviders = File.ReadAllText(config/providers.json).
 * Il file LOCALE (stato utente) vince; se manca si usa il SEED condiviso del compilatore
 * (`shared/settings/providers.json`, che il bundle copia accanto all'eseguibile in
 * `config/providers.json`). Se non c'è nemmeno quello, fallback al registry serializzato. */
char* gmb_ai_providers_json(void) {
    char path[1024];
    gmb_paths_providers_json(path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (!f) {
        char seed[1200];
        if (gmb_paths_shared_settings_file("providers.json", seed, sizeof seed)) f = fopen(seed, "rb");
    }
    if (!f) return gmb_provider_v2_info();
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return gmb_provider_v2_info(); }
    long n = ftell(f);
    if (n < 0) { fclose(f); return gmb_provider_v2_info(); }
    rewind(f);
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return gmb_provider_v2_info(); }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

/* --- MCP server registry reale (parità McpConfig) --- */
#define GMB_MCP_SERVERS_MAX 64
typedef struct { char name[128]; char kind[32]; char source[512]; int enabled; } gmb_mcp_server_t;
static gmb_mcp_server_t g_mcp_servers[GMB_MCP_SERVERS_MAX];
static int g_mcp_server_count = 0;

static gmb_mcp_server_t* mcp_server_find(const char* name) {
    for (int i = 0; i < g_mcp_server_count; i++)
        if (strcmp(g_mcp_servers[i].name, name) == 0) return &g_mcp_servers[i];
    return NULL;
}

/* Carica config/mcp.json: registra i server MCP (parita' McpConfig C#). */
int gmb_mcp_load(const char* path) {
    FILE* f = fopen(path, "r"); if (!f) return 0;
    char buf[65536]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
    char* p = strstr(buf, "\"servers\""); if (!p) return 0;
    p = strchr(p, '['); if (!p) return 0;
    int loaded = 0; char* q = p;
    while ((q = strstr(q, "\"name\""))) {
        char* c = strchr(q, ':'); if (!c) break;
        char* s = strchr(c, '"'); if (!s) break;
        char* e = strchr(s + 1, '"'); if (!e) break;
        char name[128]; size_t l = (size_t)(e - s - 1); if (l > 127) l = 127; memcpy(name, s + 1, l); name[l] = 0;
        char kind[32] = "stdio";
        char* k = strstr(e, "\"kind\""); if (k) { char* kc = strchr(k, ':'); if (kc) { char* ks = strchr(kc, '"'); if (ks) { char* ke = strchr(ks + 1, '"'); if (ke && ke - ks - 1 < 31) { memcpy(kind, ks + 1, (size_t)(ke - ks - 1)); kind[ke - ks - 1] = 0; } } } }
        char cmd[256] = "";
        char* cm = strstr(e, "\"command\""); if (cm) { char* cc = strchr(cm, ':'); if (cc) { char* cs = strchr(cc, '"'); if (cs) { char* ce = strchr(cs + 1, '"'); if (ce && ce - cs - 1 < 255) { memcpy(cmd, cs + 1, (size_t)(ce - cs - 1)); cmd[ce - cs - 1] = 0; } } } }
        gmb_mcp_server_add(name, kind, cmd);
        gmb_mcp_server_set_enabled(name, 1);   /* mcp.json: server abilitati di default (parita' C#) */
        loaded++; q = e + 1;
    }
    return loaded;
}
int gmb_mcp_server_add(const char* name, const char* kind, const char* source) {
    if (!name || !*name || !kind || !*kind) return 0;
    gmb_mcp_server_t* e = mcp_server_find(name);
    if (e) { snprintf(e->kind, sizeof(e->kind), "%s", kind); snprintf(e->source, sizeof(e->source), "%s", source ? source : ""); return 1; }
    if (g_mcp_server_count >= GMB_MCP_SERVERS_MAX) return 0;
    e = &g_mcp_servers[g_mcp_server_count++];
    snprintf(e->name, sizeof(e->name), "%s", name);
    snprintf(e->kind, sizeof(e->kind), "%s", kind);
    snprintf(e->source, sizeof(e->source), "%s", source ? source : "");
    e->enabled = 0;
    return 1;
}

int gmb_mcp_server_remove(const char* name) {
    for (int i = 0; i < g_mcp_server_count; i++)
        if (strcmp(g_mcp_servers[i].name, name) == 0) {
            memmove(&g_mcp_servers[i], &g_mcp_servers[i + 1], (size_t)(g_mcp_server_count - i - 1) * sizeof(gmb_mcp_server_t));
            g_mcp_server_count--;
            return 1;
        }
    return 0;
}

int gmb_mcp_server_set_enabled(const char* name, int enabled) {
    gmb_mcp_server_t* e = mcp_server_find(name);
    if (!e) return 0;
    e->enabled = enabled ? 1 : 0;
    return 1;
}

static void mcp_server_line(const gmb_mcp_server_t* e, char* out, size_t cap) {
    snprintf(out, cap, "%s|%s|%s|%s", e->name, e->kind, e->enabled ? "ON" : "OFF", e->source);
}

char* gmb_mcp_server_list(void) {
    char* out = (char*)malloc(16384); out[0] = 0;
    for (int i = 0; i < g_mcp_server_count; i++) {
        char line[1024]; mcp_server_line(&g_mcp_servers[i], line, sizeof(line));
        if (i) strcat(out, "\n");
        strcat(out, line);
    }
    return out;
}

char* gmb_mcp_server_status(const char* name) {
    gmb_mcp_server_t* e = mcp_server_find(name);
    if (!e) return strdup("");
    char line[1024]; mcp_server_line(e, line, sizeof(line));
    return strdup(line);
}

/* Marketplace: catalogo statico (parità McpMarketplace). */
static const char* const g_mcp_marketplace[] = {
    "files|http|http://127.0.0.1:8899/mcp",
    "echo|stdio|echo-server",
    "memory|http|http://127.0.0.1:8898/mcp",
    NULL
};
char* gmb_mcp_server_marketplace_list(void) {
    char* out = (char*)malloc(4096); out[0] = 0;
    for (int i = 0; g_mcp_marketplace[i]; i++) {
        if (i) strcat(out, "\n");
        strcat(out, g_mcp_marketplace[i]);
    }
    return out;
}

/* Tool di un server: i tool registrati globalmente (g_mcp_tools) associati al server. */
char* gmb_mcp_list_tools_server(const char* server) {
    char* out = (char*)malloc(4096); out[0] = 0;
    if (!mcp_server_find(server)) { snprintf(out, 4096, "ERROR: Server MCP '%s' non configurato.", server ? server : ""); return out; }
    for (int i = 0; i < g_mcp_tool_count; i++) {
        if (i) strcat(out, "\n");
        strcat(out, g_mcp_tools[i].name);
    }
    return out;
}

char* gmb_mcp_call(const char* server, const char* tool, const char* args) {
    char* out = (char*)malloc(2048);
    if (!out) return strdup("");
    gmb_mcp_server_t* e = mcp_server_find(server);
    if (!e) { snprintf(out, 2048, "ERROR: Server MCP '%s' non configurato (usa AI MCP SERVER ADD).", server); return out; }
    snprintf(out, 2048, "MCP %s:%s(%s)", server, tool, args ? args : "");
    return out;
}

/* AI.MCPPROMPTS() — parità IlRuntime.McpPrompts (IL/PCode): un oggetto per AGENTE,
 * {"name":"<agente>","description":"Chat with agent <agente>"}, ordinati per nome
 * (l'ordine del registry gestito non è garantito: sortiamo per renderlo deterministico
 * e identico fra i backend). Prima era `[]` fisso → divergenza reale da IL. */
char* gmb_mcp_prompts(void) {
    char names[64][64];
    int n = gmb_agent_list(names, 64);
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(names[j], names[i]) < 0) {
                char t[64]; snprintf(t, sizeof(t), "%s", names[i]);
                snprintf(names[i], sizeof(names[i]), "%s", names[j]);
                snprintf(names[j], sizeof(names[j]), "%s", t);
            }
    mcp_sb_t sb = {0};
    mcp_sb_putc(&sb, '[');
    for (int i = 0; i < n; i++) {
        if (i && !mcp_sb_putc(&sb, ',')) { free(sb.data); return strdup("[]"); }
        mcp_sb_puts(&sb, "{\"name\":\"");
        mcp_json_escape(&sb, names[i]);
        mcp_sb_puts(&sb, "\",\"description\":\"Chat with agent ");
        mcp_json_escape(&sb, names[i]);
        mcp_sb_puts(&sb, "\"}");
    }
    mcp_sb_putc(&sb, ']');
    return sb.data ? sb.data : strdup("[]");
}

/* AI.MCPRESOURCES() — parità IlRuntime.McpResources (IL/PCode): un oggetto per ogni TOOL
 * registrato, {"uri":"basic://tools/<nome>","name":"<nome>","description":"<desc>"},
 * ordinati per nome. Prima era `[]` fisso → divergenza reale da IL. */
char* gmb_mcp_resources(void) {
    int idx[MCP_MAX_TOOLS];
    for (int i = 0; i < g_mcp_tool_count; i++) idx[i] = i;
    for (int i = 0; i < g_mcp_tool_count - 1; i++)
        for (int j = i + 1; j < g_mcp_tool_count; j++)
            if (strcasecmp(g_mcp_tools[idx[j]].name, g_mcp_tools[idx[i]].name) < 0) {
                int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
            }
    mcp_sb_t sb = {0};
    mcp_sb_putc(&sb, '[');
    for (int k = 0; k < g_mcp_tool_count; k++) {
        const gmb_mcp_tool_t* t = &g_mcp_tools[idx[k]];
        if (k && !mcp_sb_putc(&sb, ',')) { free(sb.data); return strdup("[]"); }
        mcp_sb_puts(&sb, "{\"uri\":\"basic://tools/");
        mcp_json_escape(&sb, t->name);
        mcp_sb_puts(&sb, "\",\"name\":\"");
        mcp_json_escape(&sb, t->name);
        mcp_sb_puts(&sb, "\",\"description\":\"");
        if (t->desc[0]) mcp_json_escape(&sb, t->desc);
        else { mcp_sb_puts(&sb, "BASIC tool "); mcp_json_escape(&sb, t->proc); }
        mcp_sb_puts(&sb, "\"}");
    }
    mcp_sb_putc(&sb, ']');
    return sb.data ? sb.data : strdup("[]");
}

/* gmb_agent_list_names: nomi agenti come gmb_list_t (lista stringhe). */
gmb_list_t gmb_agent_list_names(void) {
    char names[64][64];
    int n = gmb_agent_list(names, 64);
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_string(gmb_string_alloc(names[i]).data));
    return l;
}

/* gmb_agent_events_names: eventi agenti come gmb_list_t. */
gmb_list_t gmb_agent_events_names(void) {
    char ev[64][256];
    int n = gmb_agent_events(ev, 64);
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_string(gmb_string_alloc(ev[i]).data));
    return l;
}

/* gmb_provider_list_names: nomi provider di un ruolo come gmb_list_t (split ','). */
gmb_list_t gmb_provider_list_names(const char* role) {
    /* COPIA LOCALE del ruolo normalizzato: gmb_provider_normalize_role ritorna un buffer
     * STATICO che gmb_provider_list (invocato subito dopo) rinnormalizza sovrascrivendolo —
     * `role` resterebbe appeso al buffer mutato e la lista usciva con il ruolo SBAGLIATO
     * (es. AI.LIST("agent") dava keyword-reranker invece di fixture-completion: fallback
     * su "completion" letto da un buffer già riscritto; riprodotto 2026-09-22,
     * MiniTest-Oracolo §0.52). Stessa classe di gmb_provider_register/set_active/active. */
    char rolebuf[32];
    snprintf(rolebuf, sizeof rolebuf, "%s", gmb_provider_normalize_role(role));
    char* pl = gmb_provider_list(rolebuf);
    gmb_list_t l = gmb_list_new();
    char* p = pl ? pl : "";
    if (*p == '[') p++;
    char* s;
    while (p && *p) {
        s = strchr(p, ',');
        if (s) *s = 0;
        size_t el = strlen(p);
        if (el > 0 && p[el - 1] == ']') p[el - 1] = 0;
        if (*p) gmb_list_add(&l, gmb_make_val_string(gmb_string_alloc(p).data));
        if (!s) break;
        p = s + 1;
    }
    if (pl) free(pl);
    return l;
}

/* gmb_agent_list_str: nomi agenti con lo STESSO rendering dell'oracolo.
 * L'IL ritorna `string[]` (AiAgentList) e un array si stampa `[]` / `[a, b]` — la vecchia versione
 * comma-separata (`""` quando vuoto) divergeva dal riferimento (EXPECT `[]` in gen_AI_AgentList). */
gmb_string_t gmb_agent_list_str(void) {
    gmb_list_t l = gmb_agent_list_names();
    gmb_string_t s = gmb_string_alloc("[");
    for (size_t i = 0; i < l.count; i++) {
        gmb_val_t v = gmb_list_get(l, i);
        if (i) s = gmb_string_concat(s, gmb_string_alloc(", "));
        s = gmb_string_concat(s, gmb_string_alloc(v.kind == GMB_TYPE_STRING && v.v_string.data ? v.v_string.data : ""));
    }
    s = gmb_string_concat(s, gmb_string_alloc("]"));
    return s;
}

/* gmb_provider_weight: peso di un provider (stringa, parità getter). */
const char* gmb_provider_weight(const char* role, const char* name) {
    static char buf[32];
    gmb_provider_t* p = find_provider(role, name);
    if (!p) { buf[0] = 0; return buf; }
    snprintf(buf, sizeof(buf), "%d", p->weight);
    return buf;
}

/* GAP CHIUSI 2026-08-29 — PROVIDER CONFIGURE/PRIORITY/REGISTERLOCAL/HEALTHCHECK */

/* gmb_provider_config: configura (kind/config) di un provider — parità PROVIDER.CONFIGURE. */
int gmb_provider_config(const char* role, const char* name, const char* kind, const char* config) {
    gmb_provider_t* p = find_provider(role, name);
    if (!p) return 0;
    if (kind) strncpy(p->kind, kind, 31);
    if (config) strncpy(p->config, config, 511);
    return 1;
}

/* gmb_provider_configure_name: AI.CONFIGURE(name, key, value) — configura un provider
 * cercandolo per NOME (parità ProviderServer.Configure). */
int gmb_provider_configure_name(const char* name, const char* key, const char* value) {
    if (!name) return 0;
    for (int i = 0; i < g_provider_count; i++)
        if (strcmp(g_providers[i].name, name) == 0) {
            if (key && value)
                snprintf(g_providers[i].config, sizeof(g_providers[i].config), "{\"%s\":\"%s\"}", key, value);
            return 1;
        }
    return 0;
}

/* gmb_provider_priority: priorità di un provider (stringa, parità getter). */
const char* gmb_provider_priority(const char* role, const char* name) {
    static char buf[32];
    gmb_provider_t* p = find_provider(role, name);
    if (!p) { buf[0] = 0; return buf; }
    snprintf(buf, sizeof(buf), "%d", p->priority);
    return buf;
}

/* gmb_ai_priority: priorità del provider AI attivo (parità AI.PRIORITY). */
int gmb_ai_priority(const char* name) {
    gmb_provider_t* p = find_provider("", name);
    return p ? p->priority : 0;
}
/* gmb_ai_set_priority: imposta la priorità del provider AI attivo. */
int gmb_ai_set_priority(const char* name, int pri) {
    gmb_provider_t* p = find_provider("", name);
    if (!p) return 0;
    p->priority = pri;
    return 1;
}

/* gmb_provider_register_local: registra un provider dal path locale (auto-detect tramite
 * l'engine: per ora registra un provider di default con kind "local" e il path come config). */
int gmb_provider_register_local(const char* path) {
    if (!path || !*path) return 0;
    return gmb_provider_register("chat", "local", "local", path);
}

/* Intervallo healthcheck condiviso tra getter e setter (prima erano due static
 * separati → il getter non vedeva mai il valore impostato). Default 10 = parità C#
 * (ProviderHealthMonitor.DefaultIntervalMinutes). */
static int g_provider_hc_interval = 10;

/* gmb_provider_healthcheck_interval: intervallo healthcheck attivo (minuti, default 10). */
int gmb_provider_healthcheck_interval(const char* role, const char* name) {
    (void)role; (void)name;
    return g_provider_hc_interval;
}

/* gmb_provider_set_healthcheck_interval: imposta l'intervallo healthcheck (minuti). */
int gmb_provider_set_healthcheck_interval(const char* role, const char* name, int minutes) {
    (void)role; (void)name;
    if (minutes >= 0) g_provider_hc_interval = minutes;
    return g_provider_hc_interval;
}

/* gmb_provider_start_healthcheck / stop_healthcheck: attiva/disattiva il healthcheck periodico. */
int gmb_provider_start_healthcheck(const char* role, const char* name) {
    /* Parità C# ProviderStartHealthCheck: avvia il monitor e ritorna 1.0. La forma
     * per-provider (role,name) marca il provider Available (comportamento storico). */
    if (!role || !*role || !name || !*name) return 1;
    gmb_provider_t* p = find_provider(role, name);
    if (!p) return 0;
    p->status = 1;
    return 1;
}
int gmb_provider_stop_healthcheck(const char* role, const char* name) {
    /* Parità C# ProviderStopHealthCheck: ferma il monitor e ritorna 1.0. */
    if (!role || !*role || !name || !*name) return 1;
    gmb_provider_t* p = find_provider(role, name);
    if (!p) return 0;
    p->status = 0;
    return 1;
}

/* AI.RERANK(query, jsonResults, topK): re-ranka i hit JSON per rilevanza testuale
 * (term-overlap query vs hit.text), ordina e ritorna {"status":"Success","hits":[...]}
 * con i topK migliori — parità AiVectorRerank (reranker term-overlap, no modello esterno).
 * NOTA: lo score usa rag_keyword_score (unica implementazione §0bis, frazione
 * matches/terms come C# KeywordReranker); la vecchia rerank_score locale (conteggio
 * grezzo) è rimossa. */

char* gmb_ai_rerank(const char* query, const char* json_results, int top_k) {
    if (top_k <= 0) top_k = 5;
    /* Parità C# AiVectorRerank: input non parsabile → passthrough dell'input (catch →
     * jsonResults), NON empty-success. Solo il parse OK con 0 hit dà hits:[]. */
    gmb_val_t v = gmb_json_parse(json_results ? json_results : "");
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return strdup(json_results ? json_results : "");
    gmb_list_t* hits = NULL;
    if (v.v_ptr && ((gmb_obj_t*)v.v_ptr)->type_name && strstr(((gmb_obj_t*)v.v_ptr)->type_name, "dict")) {
        /* {"hits":[...]}: root dict → estrai "hits". */
        gmb_dict_t* root = (gmb_dict_t*)((gmb_obj_t*)v.v_ptr)->data;
        if (!root) return strdup(json_results ? json_results : "");
        gmb_val_t hitsV = gmb_dict_get(*root, "hits");
        if (hitsV.kind == GMB_TYPE_OBJECT && hitsV.v_ptr) hits = (gmb_list_t*)((gmb_obj_t*)hitsV.v_ptr)->data;
        if (!hits) return strdup(json_results ? json_results : "");
    } else {
        /* Array nudo (gmb_vecdb_search): [{...},...] → hits direttamente. */
        hits = (gmb_list_t*)((gmb_obj_t*)v.v_ptr)->data;
        if (!hits) return strdup(json_results ? json_results : "");
    }
    int n = hits ? (int)hits->count : 0;
    if (n <= 0) return strdup("{\"status\":\"Success\",\"hits\":[]}");
    /* punteggi per indice; selection sort discendente. */
    double* scores = (double*)malloc(sizeof(double) * (size_t)n);
    int* order = (int*)malloc(sizeof(int) * (size_t)n);
    if (!scores || !order) { free(scores); free(order); return strdup("{\"status\":\"Success\",\"hits\":[]}"); }
    for (int i = 0; i < n; i++) {
        order[i] = i;
        gmb_val_t hit = gmb_list_get(*hits, i);
        const char* text = "";
        if (hit.kind == GMB_TYPE_OBJECT && hit.v_ptr) {
            gmb_dict_t* hd = (gmb_dict_t*)((gmb_obj_t*)hit.v_ptr)->data;
            gmb_val_t tv = {0}; if (hd) tv = gmb_dict_get(*hd, "text");
            if (tv.kind == GMB_TYPE_STRING && tv.v_string.data) text = tv.v_string.data;
        }
        /* Parità C# KeywordReranker (matches/terms). Prima: conteggio grezzo. */
        scores[i] = (double)rag_keyword_score(query, text);
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (scores[order[j]] > scores[order[i]]) { int tmp = order[i]; order[i] = order[j]; order[j] = tmp; }
    int k = n < top_k ? n : top_k;
    /* Serializza PascalCase come C# VectorSearchResult (ChunkId/DocumentId/SourceId/Text/
     * Score/ModelVersion/MetadataJson/ExplainJson; score shortest-round-trip). Prima:
     * riusava il JSON d'ingresso (score vecchi). */
    char* out = (char*)malloc(65536);
    if (!out) { free(scores); free(order); return strdup("{\"status\":\"Success\",\"hits\":[]}"); }
    int pos = 0;
    pos += snprintf(out + pos, 65536 - (size_t)pos, "{\"status\":\"Success\",\"hits\":[");
    for (int i = 0; i < k; i++) {
        gmb_val_t hit = gmb_list_get(*hits, order[i]);
        long long cid = 0, did = 0;
        const char *src = "", *txt = "", *mv = "";
        if (hit.kind == GMB_TYPE_OBJECT && hit.v_ptr) {
            gmb_dict_t* hd = (gmb_dict_t*)((gmb_obj_t*)hit.v_ptr)->data;
            if (hd) {
                gmb_val_t v;
                v = gmb_dict_get(*hd, "chunkId"); if (v.kind == GMB_TYPE_INT) cid = v.v_int; else if (v.kind == GMB_TYPE_DOUBLE) cid = (long long)v.v_double;
                v = gmb_dict_get(*hd, "documentId"); if (v.kind == GMB_TYPE_INT) did = v.v_int; else if (v.kind == GMB_TYPE_DOUBLE) did = (long long)v.v_double;
                v = gmb_dict_get(*hd, "sourceId"); if (v.kind == GMB_TYPE_STRING && v.v_string.data) src = v.v_string.data;
                v = gmb_dict_get(*hd, "text"); if (v.kind == GMB_TYPE_STRING && v.v_string.data) txt = v.v_string.data;
                v = gmb_dict_get(*hd, "modelVersion"); if (v.kind == GMB_TYPE_STRING && v.v_string.data) mv = v.v_string.data;
            }
        }
        char scb[32], escs[8192], esct[8192];
        gmb_json_float_str(scb, sizeof(scb), scores[order[i]]);
        escs[gmb_json_escape(escs, sizeof(escs), src)] = '\0';
        esct[gmb_json_escape(esct, sizeof(esct), txt)] = '\0';
        if (i > 0) pos += snprintf(out + pos, 65536 - (size_t)pos, ",");
        pos += snprintf(out + pos, 65536 - (size_t)pos,
            "{\"ChunkId\":%lld,\"DocumentId\":%lld,\"SourceId\":\"%s\",\"Text\":\"%s\",\"Score\":%s,\"ModelVersion\":\"%s\",\"MetadataJson\":null,\"ExplainJson\":null}",
            cid, did, escs, esct, scb, mv);
    }
    snprintf(out + pos, 65536 - (size_t)pos, "]}");
    free(scores); free(order);
    return out;
}

/* ── PLUGIN registry (AI PLUGINS / AI.PLUGINSLIST/INFO/CALL) — registro reale ── */
#define LLMENGINE_PLUGIN_MAX 32
typedef struct { char name[128]; char kind[32]; char config[512]; int enabled; } llmengine_plugin_t;
static llmengine_plugin_t g_plugins[LLMENGINE_PLUGIN_MAX];
static int g_plugin_count = 0;

/* ── Helpers dell'archivio plugin — parità PluginRegistry/PluginIndex (C#) ──
 * Il C# è l'oracolo: un plugin vive in <pluginsDir>/<nome>/PLUGIN.md (+ tool.json) e
 * l'archivio è la CARTELLA (non il registro in-process). */

/* Cartella base: override di `AI PLUGINS SET INDEX`, altrimenti paths.pluginsDir.
 * L'override è in-process (SET INDEX) oppure persistito da un run precedente
 * (<configDir>/plugins.json, scritto da llmengine_plugin_set_index — parità C# cross-run). */
static char g_plugin_dir_override[1024];
static int g_plugin_dir_state_loaded = 0;
static void plugin_load_persisted_index(void);
static void plugin_base_dir(char* out, size_t cap) {
    if (!g_plugin_dir_state_loaded) { g_plugin_dir_state_loaded = 1; plugin_load_persisted_index(); }
    if (g_plugin_dir_override[0]) { snprintf(out, cap, "%s", g_plugin_dir_override); return; }
    gmb_paths_plugins_dir(out, cap);
}
static void plugin_dir_of(const char* name, char* out, size_t cap) {
    char base[1024]; plugin_base_dir(base, sizeof(base));
    snprintf(out, cap, "%s/%.128s", base, name ? name : "");
}
/* OrdinalIgnoreCase ASCII — PluginIndex.Scan ordina così (C#). */
static int plugin_name_ci_cmp(const char* a, const char* b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
/* ToolName(): trattini -> underscore (PluginRegistry.ToolName). */
static void plugin_tool_name(const char* name, char* out, size_t cap) {
    size_t j = 0;
    for (const char* p = name ? name : ""; *p && j + 1 < cap; p++) out[j++] = (*p == '-') ? '_' : *p;
    out[j] = 0;
}
/* MakeProcName(): PascalCase sui separatori `- _ spazio .` (PluginRegistry.MakeProcName). */
static void plugin_proc_name(const char* name, char* out, size_t cap) {
    size_t j = 0; int up = 1;
    for (const char* p = name ? name : ""; *p && j + 1 < cap; p++) {
        char ch = *p;
        if (ch == '-' || ch == '_' || ch == ' ' || ch == '.') { up = 1; continue; }
        out[j++] = up ? (char)toupper((unsigned char)ch) : ch;
        up = 0;
    }
    out[j] = 0;
}
/* Escape di una stringa JSON (equivalente del JsonObject del C#). */
static void plugin_json_escape(const char* in, char* out, size_t cap) {
    size_t j = 0;
    for (const char* p = in ? in : ""; *p && j + 6 < cap; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '"' || ch == '\\') { out[j++] = '\\'; out[j++] = (char)ch; }
        else if (ch == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
        else if (ch == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
        else if (ch == '\t') { out[j++] = '\\'; out[j++] = 't'; }
        else out[j++] = (ch < 0x20) ? ' ' : (char)ch;
    }
    out[j] = 0;
}

/* Campo scalare del frontmatter di un PLUGIN.md — semantica PluginIndex.ReadField:
 * `key:` a inizio riga, valore inline; se il valore è vuoto o un blocco scalare
 * (`|` o `>`) concatena le righe indentate successive; virgolette esterne rimosse.
 * NOTA su ReadField: il regex C# `(.+?)(?:\n\s*\w+:|…)` termina il valore a una riga
 * che inizia con UNA parola-alfa + due-punti; le righe che iniziano con una parentesi
 * (`[…]`, `{…}`) o con cifre/appici NON la chiudono → appartengono al valore. */
static int plugin_read_field(const char* file, const char* key, char* out, size_t cap) {
    out[0] = 0;
    if (!file || !key || !*key) return 0;
    FILE* f = fopen(file, "rb");
    if (!f) return 0;
    char* buf = (char*)malloc(65536);
    if (!buf) { fclose(f); return 0; }
    size_t n = fread(buf, 1, 65535, f);
    buf[n] = 0;
    fclose(f);
    char* p = buf;
    if (n >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
    size_t klen = strlen(key);
    int found = 0;
    if (strncmp(p, "---", 3) == 0) {
        p = strchr(p, '\n');
        if (p) p++;
        while (p && *p) {
            char* eol = strchr(p, '\n');
            size_t len = eol ? (size_t)(eol - p) : strlen(p);
            char* next = eol ? eol + 1 : NULL;
            if (len >= 3 && strncmp(p, "---", 3) == 0) break;      /* fine frontmatter */
            char* colon = (char*)memchr(p, ':', len);
            if (colon) {
                size_t k = (size_t)(colon - p);
                while (k > 0 && (p[k - 1] == ' ' || p[k - 1] == '\t')) k--;
                if (k == klen && strncmp(p, key, klen) == 0) {
                    const char* v = colon + 1;
                    size_t vlen = len - (size_t)(v - p);
                    while (vlen > 0 && (*v == ' ' || *v == '\t')) { v++; vlen--; }
                    while (vlen > 0 && (v[vlen - 1] == ' ' || v[vlen - 1] == '\t' || v[vlen - 1] == '\r')) vlen--;
                    int block = (vlen == 0) || (vlen <= 2 && (v[0] == '|' || v[0] == '>'));
                    size_t j = 0;
                    if (!block) for (size_t i = 0; i < vlen && j + 1 < cap; i++) out[j++] = v[i];
                    for (char* q = block ? next : NULL; q && *q; ) {
                        /* inizia una `parola:` (come nel regex C# `\n\s*\w+:`)? -> fine valore */
                        const char* r = q;
                        while (*r == ' ' || *r == '\t') r++;
                        if (isalpha((unsigned char)*r)) {
                            const char* c2 = r;
                            while (isalnum((unsigned char)*c2) || *c2 == '_') c2++;
                            const char* c3 = c2;
                            while (*c3 == ' ' || *c3 == '\t') c3++;
                            if (*c3 == ':') { q = NULL; break; }     /* nuova chiave */
                        }
                        if (!block && *r != ' ' && *r != '\t' && *r != '\r' && *r != 0 && *r != '\n') break;
                        char* qe = strchr(q, '\n');
                        size_t ql = qe ? (size_t)(qe - q) : strlen(q);
                        const char* s = q; size_t sl = ql;
                        while (sl > 0 && (*s == ' ' || *s == '\t')) { s++; sl--; }
                        while (sl > 0 && (s[sl - 1] == ' ' || s[sl - 1] == '\t' || s[sl - 1] == '\r')) sl--;
                        if (sl > 0) {
                            if (j > 0 && j + 1 < cap) out[j++] = ' ';
                            for (size_t i = 0; i < sl && j + 1 < cap; i++) out[j++] = s[i];
                        }
                        q = qe ? qe + 1 : NULL;
                    }
                    out[j] = 0;
                    if (out[0] == '"' || out[0] == '\'') {
                        size_t l = strlen(out);
                        if (l >= 2 && out[l - 1] == out[0]) { memmove(out, out + 1, l - 2); out[l - 2] = 0; }
                    }
                    found = out[0] ? 1 : 0;
                    break;
                }
            }
            p = next;
        }
    }
    free(buf);
    return found;
}

/* Rimozione ricorsiva di una cartella (Directory.Delete(recursive) del C#). */
static int plugin_rmtree(const char* path) {
    DIR* d = opendir(path);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char p[2048]; snprintf(p, sizeof(p), "%s/%s", path, e->d_name);
        struct stat st;
        if (stat(p, &st) == 0 && S_ISDIR(st.st_mode)) plugin_rmtree(p);
        else remove(p);
    }
    closedir(d);
    return rmdir(path) == 0;
}

static llmengine_plugin_t* plugin_find(const char* name) {
    for (int i = 0; i < g_plugin_count; i++)
        if (strcmp(g_plugins[i].name, name) == 0) return &g_plugins[i];
    return NULL;
}

int llmengine_plugin_register(const char* name, const char* kind, const char* config) {
    if (!name || !*name) return 0;
    llmengine_plugin_t* p = plugin_find(name);
    if (p) { snprintf(p->kind, sizeof(p->kind), "%s", kind ? kind : ""); snprintf(p->config, sizeof(p->config), "%s", config ? config : ""); return 1; }
    if (g_plugin_count >= LLMENGINE_PLUGIN_MAX) return 0;
    p = &g_plugins[g_plugin_count++];
    snprintf(p->name, sizeof(p->name), "%s", name);
    snprintf(p->kind, sizeof(p->kind), "%s", kind ? kind : "");
    snprintf(p->config, sizeof(p->config), "%s", config ? config : "");
    p->enabled = 0;
    return 1;
}

/* AI PLUGINS CREATE <name>,<desc>: scheletro COMPLETO dell'archivio — PLUGIN.md +
 * tool.json a 4 chiavi (ToolName underscore, procedure PascalCase, inputSchema) —
 * parità PluginRegistry.Create (C#). Ritorna 1 se entrambi i file esistono. */
/* mkdir ricorsivo (parità Directory.CreateDirectory C#: crea anche i padri mancanti,
 * es. l'override di SET INDEX su un percorso nuovo). Ritorna 0 se la cartella esiste
 * (o è stata creata), -1 altrimenti. */
static int plugin_mkdirs(const char* path) {
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

int llmengine_plugin_create(const char* name, const char* desc) {
    if (!name || !*name) return 0;
    const char* d = desc ? desc : "";
    char dir[1200]; plugin_dir_of(name, dir, sizeof(dir));
    plugin_mkdirs(dir);
    char tool[128], proc[160], dj[1024];
    plugin_tool_name(name, tool, sizeof(tool));
    plugin_proc_name(name, proc, sizeof(proc));
    plugin_json_escape(d, dj, sizeof(dj));
    char pj[1400]; snprintf(pj, sizeof(pj), "%s/PLUGIN.md", dir);
    FILE* f = fopen(pj, "wb");
    if (f) {
        fprintf(f, "---\nname: %s\ndescription: %s\ntrigger: \nenabled: true\ntools: [%s]\nrequires: []\nprocedures: [%s]\n---\n# Plugin %s\n\nIstruzioni operative per l'agente.\n",
                name, d, tool, proc, name);
        fclose(f);
    }
    char tj[1400]; snprintf(tj, sizeof(tj), "%s/tool.json", dir);
    f = fopen(tj, "wb");
    if (f) {
        fprintf(f, "{\"name\":\"%s\",\"description\":\"%s\",\"procedure\":\"%s\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}",
                tool, dj, proc);
        fclose(f);
    }
    /* Il C# NON registra su CREATE (PluginRegistry.Create scrive solo i file); il
     * registro si popola con ENABLE (PluginRegistry.Enable → Register). */
    return (access(pj, F_OK) == 0 && access(tj, F_OK) == 0) ? 1 : 0;
}

/* Unregister dal registro in-process (parità PluginRegistry.Unregister C#): shift
 * dell'array. Ritorna 1 se il record esisteva ed è stato rimosso. */
static int plugin_unregister_name(const char* name) {
    for (int i = 0; i < g_plugin_count; i++) {
        if (strcasecmp(g_plugins[i].name, name ? name : "") == 0) {
            for (int j = i; j + 1 < g_plugin_count; j++) g_plugins[j] = g_plugins[j + 1];
            g_plugin_count--;
            return 1;
        }
    }
    return 0;
}

int llmengine_plugin_registered(const char* name) { return plugin_find(name) ? 1 : 0; }

int llmengine_plugin_set_enabled(const char* name, int enabled) {
    if (enabled) {
        /* parità PluginRegistry.Enable (C#): Register richiede PLUGIN.md su disco;
         * se il record non è nel registro lo aggiunge, poi abilita. */
        char dir[1200]; plugin_dir_of(name, dir, sizeof(dir));
        char pj[1400]; snprintf(pj, sizeof(pj), "%s/PLUGIN.md", dir);
        if (access(pj, F_OK) != 0) return 0;
        llmengine_plugin_register(name, "local", "");
        return 1;
    }
    /* parità PluginRegistry.Disable (C#): Unregister → il record ESCE dal registro
     * (AI.PLUGINLIST() non lo elenca più). Re-ENABLE richiede re-Register (Enable). */
    return plugin_unregister_name(name);
}

/* AI.PLUGINLIST(): il REGISTRO in-process (parità PluginRegistry.List C# — IlRuntime.
 * AiPluginList) — distinto da AI.PLUGINSLIST() che scansiona l'archivio su disco. */
gmb_list_t llmengine_plugin_list_registry(void) {
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < g_plugin_count; i++)
        gmb_list_add(&l, gmb_make_val_string(gmb_string_alloc(g_plugins[i].name).data));
    return l;
}

gmb_list_t llmengine_plugin_list_names(void) {
    /* Parità IlRuntime.AiPluginsList → PluginIndex.Scan (SCANSIONE della directory con
       ordinamento OrdinalIgnoreCase: gmb_plugins_scan filtra per PLUGIN.md e ordina).
       gmb_plugins_scan è già ordinato: nessuna seconda passata qui. In un processo C
       fresco il registro è vuoto e la lista usciva `[]` mentre l'oracolo C# elenca i
       plugin su disco. */
    char names[256][64];
    int n = gmb_plugins_scan(names, 256);
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++)
        gmb_list_add(&l, gmb_make_val_string(gmb_string_alloc(names[i]).data));
    return l;
}

/* Scansiona <pluginsDir> e ritorna i nomi delle sottocartelle CON PLUGIN.md, ordinate
 * OrdinalIgnoreCase — parità PluginIndex.Scan (C#): solo le cartelle con PLUGIN.md
 * contano come plugin. */
int gmb_plugins_scan(char names[][64], int max) {
    char dir[1024]; plugin_base_dir(dir, sizeof(dir));
    DIR* d = opendir(dir); if (!d) return 0;
    int n = 0; struct dirent* e;
    while ((e = readdir(d)) && n < max) {
        if (e->d_name[0] == '.') continue;
        char p[1400]; snprintf(p, sizeof(p), "%s/%s/PLUGIN.md", dir, e->d_name);
        if (access(p, F_OK) == 0) { snprintf(names[n], 64, "%s", e->d_name); n++; }
    }
    closedir(d);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (plugin_name_ci_cmp(names[i], names[j]) > 0) {
                char t[64]; memcpy(t, names[i], 64); memcpy(names[i], names[j], 64); memcpy(names[j], t, 64);
            }
    return n;
}
/* MatchByTrigger (parità PluginIndex.MatchByTrigger C#): 1 se il prompt contiene il trigger
 * di un plugin noto, spezzato su ','; per ogni phrase vale Contains(phrase) OPPURE (phrase
 * senza spazio e >=4 char) Contains di ogni parola >=4 char. Usato da AI AGENT ASK per far
 * partire il tool-loop solo quando il task menziona davvero una skill (contesto minimo). */
int llmengine_plugin_match_trigger(const char* prompt) {
    if (!prompt || !*prompt) return 0;
    char names[256][64];
    int n = gmb_plugins_scan(names, 256);
    if (n <= 0) return 0;
    char base[1024]; plugin_base_dir(base, sizeof(base));
    for (int i = 0; i < n; i++) {
        char pj[1400], trig[512];
        snprintf(pj, sizeof(pj), "%s/%s/PLUGIN.md", base, names[i]);
        trig[0] = 0;
        plugin_read_field(pj, "trigger", trig, sizeof(trig));
        if (!trig[0]) continue;
        const char* p = trig;
        while (*p) {
            const char* comma = strchr(p, ',');
            size_t l = comma ? (size_t)(comma - p) : strlen(p);
            while (l && (*p == ' ' || *p == '\t')) { p++; l--; }
            while (l && (p[l-1] == ' ' || p[l-1] == '\t')) l--;
            if (l) {
                char phrase[512];
                if (l >= sizeof(phrase)) l = sizeof(phrase) - 1;
                memcpy(phrase, p, l); phrase[l] = 0;
                if (strcasestr(prompt, phrase)) return 1;
                if (!strchr(phrase, ' ') && l >= 4) {
                    char tmp[512]; snprintf(tmp, sizeof(tmp), "%s", phrase);
                    char* save = NULL;
                    for (char* w = strtok_r(tmp, " ", &save); w; w = strtok_r(NULL, " ", &save))
                        if (strlen(w) >= 4 && strcasestr(prompt, w)) return 1;
                }
            }
            if (!comma) break;
            p = comma + 1;
        }
    }
    return 0;
}

/* AI.PLUGINSINFO(name) -> SOLO la description da PLUGIN.md su disco ("") se assente —
 * parità IlRuntime.AiPluginInfo: `PluginRegistry.Get(name)?.Description ?? ""` (C#). */
char* llmengine_plugin_info(const char* name) {
    char dir[1200]; plugin_dir_of(name, dir, sizeof(dir));
    char pj[1400]; snprintf(pj, sizeof(pj), "%s/PLUGIN.md", dir);
    char desc[1024];
    if (plugin_read_field(pj, "description", desc, sizeof(desc))) return strdup(desc);
    return strdup("");
}

/* ── AI PLUGINS ADD(path) — parità IlRuntime.AiPluginsAdd (C#): copia plugin da un
 * percorso sorgente nell'archivio. Se <path> è una DIRECTORY: ogni sottocartella CON
 * PLUGIN.md viene copiata nell'archivio. Se è .../PLUGIN.md: copia il singolo plugin
 * (cartella padre). Altrimenti ritorna 0. Ritorna 1 se qualcosa è stato copiato. ── */
static int plugin_copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb"); if (!in) return 0;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[8192]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in); fclose(out);
    return 1;
}
int llmengine_plugin_add(const char* path) {
    if (!path || !*path) return 0;
    char full[1200]; snprintf(full, sizeof(full), "%s", path);
    /* espandi ~/ (parità PathExpansion.Expand del C#) */
    if (full[0] == '~' && (full[1] == '/' || full[1] == 0)) {
        const char* home = getenv("HOME");
        char tmp[1200]; snprintf(tmp, sizeof(tmp), "%s%s", home ? home : "", full + 1);
        snprintf(full, sizeof(full), "%s", tmp);
    }
    struct stat st;
    if (stat(full, &st) != 0) return 0;
    char base[1024]; plugin_base_dir(base, sizeof(base));
    int copied = 0;
    if (S_ISDIR(st.st_mode)) {
        /* directory: ogni sottocartella con PLUGIN.md → archivio */
        DIR* d = opendir(full); if (!d) return 0;
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char sub[1400]; snprintf(sub, sizeof(sub), "%s/%s", full, e->d_name);
            char pj[1500]; snprintf(pj, sizeof(pj), "%s/PLUGIN.md", sub);
            struct stat s2;
            if (stat(pj, &s2) != 0 || !S_ISREG(s2.st_mode)) continue;
            char dest[1024]; snprintf(dest, sizeof(dest), "%s/%s", base, e->d_name);
            plugin_mkdirs(dest);
            DIR* sd = opendir(sub); if (!sd) continue;
            struct dirent* f;
            while ((f = readdir(sd)) != NULL) {
                if (f->d_name[0] == '.') continue;
                char fsrc[1600], fdst[1600];
                snprintf(fsrc, sizeof(fsrc), "%s/%s", sub, f->d_name);
                snprintf(fdst, sizeof(fdst), "%s/%s", dest, f->d_name);
                struct stat s3;
                if (stat(fsrc, &s3) == 0 && S_ISREG(s3.st_mode)) copied += plugin_copy_file(fsrc, fdst);
            }
            closedir(sd);
        }
        closedir(d);
    }
    else {
        /* file PLUGIN.md singolo: la cartella padre è il nome del plugin */
        const char* slash = strrchr(full, '/');
        if (!slash || strcasecmp(slash + 1, "PLUGIN.md") != 0) return 0;
        char parent[1200]; snprintf(parent, sizeof(parent), "%s", full);
        char* ps = strrchr(parent, '/'); if (!ps) return 0; *ps = 0;
        const char* name = strrchr(parent, '/'); name = name ? name + 1 : parent;
        char dest[1024]; snprintf(dest, sizeof(dest), "%s/%s", base, name);
        plugin_mkdirs(dest);
        char fdst[1600]; snprintf(fdst, sizeof(fdst), "%s/PLUGIN.md", dest);
        copied += plugin_copy_file(full, fdst);
    }
    if (copied > 0) llmengine_plugin_refresh();
    return copied > 0 ? 1 : 0;
}

/* ── AI.PLUGINCALL — invocazione procedura del plugin (parità PluginRegistry.Call C#) ──
 * Il C# risolve la procedura del plugin e la invoca via CallHandler (GPM). Nel C puro la
 * procedura non è raggiungibile dal runtime (non conosce la VM): il backend che ha la VM
 * (VM/C) installa un hook (llmengine_plugin_set_call_hook) e la invoca sul task corrente;
 * senza hook il plugin viene solo VERIFICATO (registrato o con PLUGIN.md su disco) e la
 * chiamata ritorna un risultato dichiarativo. */
typedef char* (*llmengine_plugin_call_fn)(const char* name, const char* arg, void* ctx);
static llmengine_plugin_call_fn g_plugin_call_cb = NULL;
static void* g_plugin_call_ctx = NULL;
void llmengine_plugin_set_call_hook(llmengine_plugin_call_fn fn, void* ctx) { g_plugin_call_cb = fn; g_plugin_call_ctx = ctx; }
char* llmengine_plugin_call(const char* name, const char* arg) {
    if (g_plugin_call_cb) {
        char* r = g_plugin_call_cb(name, arg, g_plugin_call_ctx);
        if (r) return r;
    }
    /* parità PluginRegistry.Call (C#): SOLO il registro in-process conta (Get(name)==null
     * → ""). Il disco NON è consultato: un plugin su disco ma non ENABLE-dato risponde "". */
    if (!plugin_find(name)) return strdup("");
    char* out = (char*)malloc(2048);
    if (!out) return strdup("");
    snprintf(out, 2048, "PLUGIN %s(%s) OK", name, arg ? arg : "");
    return out;
}

/* ── Config AI (log, HF, save/load) — porting da IlRuntime (AiConfig + ProviderConfig) ── */

static char g_ai_log_path[1024] = "";
static char g_ai_hf_token[512] = "";
static char g_ai_hf_repo[512] = "";

int gmb_ai_set_log_path(const char* path) {
    if (!path) return 0;
    strncpy(g_ai_log_path, path, sizeof(g_ai_log_path) - 1);
    g_ai_log_path[sizeof(g_ai_log_path) - 1] = '\0';
    return 1;
}
const char* gmb_ai_log_path(void) { return g_ai_log_path; }

int gmb_ai_log_write(const char* file, const char* text, int newline) {
    if (!file || !text) return 0;
    char full[1152];
    if (g_ai_log_path[0])
        snprintf(full, sizeof(full), "%s/%s", g_ai_log_path, file);
    else
        snprintf(full, sizeof(full), "%s", file);
    FILE* f = fopen(full, "a");
    if (!f) return 0;
    if (newline) fprintf(f, "%s\n", text); else fprintf(f, "%s", text);
    fclose(f);
    return 1;
}
int gmb_ai_log_write_line(const char* file, const char* text) { return gmb_ai_log_write(file, text, 1); }

const char* gmb_ai_set_hf_token(const char* value) {
    if (value) { strncpy(g_ai_hf_token, value, sizeof(g_ai_hf_token) - 1); g_ai_hf_token[sizeof(g_ai_hf_token) - 1] = '\0'; }
    return g_ai_hf_token;
}
const char* gmb_ai_set_hf_repo(const char* value) {
    if (value) { strncpy(g_ai_hf_repo, value, sizeof(g_ai_hf_repo) - 1); g_ai_hf_repo[sizeof(g_ai_hf_repo) - 1] = '\0'; }
    return g_ai_hf_repo;
}
const char* gmb_ai_hf_repo(void) { return g_ai_hf_repo; }

int gmb_ai_save_config(void) { return 1; }   /* config AI (728/729): in-memory, nessun file */
int gmb_ai_load_config(void) { return 1; }

/* AI.SAVE/AI.LOAD (417/418) — REGISTRO provider su file, parità C# ProviderServer.Save/Load:
 * SAVE scrive lo snapshot (formato providers.json v2); LOAD SOSTITUISCE il registro (il C#
 * fa RestoreSnapshotLocked: i provider registrati dopo il salvataggio spariscono). */
int gmb_provider_save_registry(const char* path) {
    /* Formato providers.json C# (oggetto per nome + version) — quello che v2_load sa
     * rileggere. v2_info emette un ARRAY e non è round-trip-safe (bug pre-esistente). */
    if (!path || !*path) return 0;
    size_t cap = 512;
    for (int i = 0; i < g_provider_count; i++) cap += 256;
    char* out = (char*)malloc(cap);
    if (!out) return 0;
    size_t len = (size_t)snprintf(out, cap, "{\"version\":1,\"providers\":{");
    for (int i = 0; i < g_provider_count; i++) {
        gmb_provider_t* p = &g_providers[i];
        const char* st = p->status == 1 ? "Available" : p->status == 2 ? "Blocked" : p->status == 3 ? "Damaged" : "Unavailable";
        len += (size_t)snprintf(out + len, cap - len,
            "%s\"%s\":{\"role\":\"%s\",\"kind\":\"%s\",\"status\":\"%s\",\"weight\":%d,\"priority\":%d}",
            i ? "," : "", p->name, p->role, p->kind, st, p->weight, p->priority);
    }
    snprintf(out + len, cap - len, "}}");
    FILE* f = fopen(path, "w");
    if (!f) { free(out); return 0; }
    fputs(out, f); fclose(f); free(out);
    return 1;
}
int gmb_provider_load_registry(const char* path) {
    if (!path || !*path) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    /* Replace (parità C# RestoreSnapshotLocked): svuota il registro; il file contiene
     * già i default salvati, quindi non ri-aggiungerli (g_defaults_done=1). */
    g_provider_count = 0;
    g_defaults_done = 1;
    return gmb_provider_v2_load(path) ? 1 : 0;
}

/* ── AI PATH / AI SKILLS — porting da IlRuntime (AiConfig + SkillIndex), registry in-memory ── */

#define GMB_AI_PATH_MAX 64
#define GMB_AI_SKILL_MAX 2048
static char g_ai_model_paths[GMB_AI_PATH_MAX][1024];
static int g_ai_model_path_count = 0;
static char g_ai_skill_paths[GMB_AI_PATH_MAX][1024];
static int g_ai_skill_path_count = 0;
static char g_ai_skill_index_path[1024] = "";
static char g_ai_skills[GMB_AI_SKILL_MAX][128];
static char g_ai_skill_desc[GMB_AI_SKILL_MAX][256];
static int g_ai_skill_count = 0;

static int ai_path_add(char (*arr)[1024], int* n, const char* p) {
    if (!p || !*p || *n >= GMB_AI_PATH_MAX) return 0;
    for (int i = 0; i < *n; i++) if (strcmp(arr[i], p) == 0) return 1;
    strncpy(arr[*n], p, 1023); arr[*n][1023] = '\0'; (*n)++;
    return 1;
}
static int ai_path_remove(char (*arr)[1024], int* n, const char* p) {
    if (!p || !*p) return 0;
    for (int i = 0; i < *n; i++) if (strcmp(arr[i], p) == 0) { for (int j = i; j < *n - 1; j++) strcpy(arr[j], arr[j+1]); (*n)--; return 1; }
    return 0;
}
/* Walk ricorsivo dei path skill (parità SkillIndex.Scan gestito: conta ogni cartella con
   SKILL.md, a qualsiasi profondità, dedupe per nome via cap; ordine non rilevante). */
/* Legge la description dal frontmatter YAML di SKILL.md (parita' SkillIndex.ReadDescription). */
static void ai_skill_read_desc(const char* skillmd, char* out, size_t cap) {
    out[0] = 0;
    FILE* f = fopen(skillmd, "rb"); if (!f) return;
    char buf[8192]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
    char* fm = strstr(buf, "---");
    if (!fm) return;
    char* end = strstr(fm + 3, "\n---");
    if (!end) return;
    char* d = strstr(fm, "description:");
    if (!d || d > end) return;
    d += 12; while (*d == ' ' || *d == '"' || *d == '\t') d++;
    size_t i = 0;
    while (*d && d < end && *d != '\n' && i + 1 < cap) {
        out[i++] = *d; d++;
    }
    while (i > 0 && (out[i-1] == ' ' || out[i-1] == '"' || out[i-1] == '\r')) i--;
    out[i] = 0;
}
static void ai_skill_walk(const char* dir, int depth) {
    DIR* d = opendir(dir);
    if (!d || depth > 24) { if (d) closedir(d); return; }
    struct dirent* e;
    while ((e = readdir(d)) && g_ai_skill_count < GMB_AI_SKILL_MAX) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || e->d_name[0] == '.') continue;
        char full[2048];
        int r = snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        if (r < 0 || r >= (int)sizeof(full)) continue;
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        char mark[2048];
        snprintf(mark, sizeof(mark), "%s/SKILL.md", full);
        if (access(mark, F_OK) == 0 && g_ai_skill_count < GMB_AI_SKILL_MAX) {
            strncpy(g_ai_skills[g_ai_skill_count], e->d_name, 127);
            g_ai_skills[g_ai_skill_count][127] = '\0';
            ai_skill_read_desc(mark, g_ai_skill_desc[g_ai_skill_count], sizeof(g_ai_skill_desc[0]));
            g_ai_skill_count++;
        }
        ai_skill_walk(full, depth + 1);
    }
    closedir(d);
}
static void ai_skill_scan(void) {
    g_ai_skill_count = 0;
    for (int i = 0; i < g_ai_skill_path_count; i++) ai_skill_walk(g_ai_skill_paths[i], 0);
    for (int i = 0; i < g_ai_skill_count; i++)   /* C# SkillIndex.Scan: OrderBy(Name) */
        for (int j = i + 1; j < g_ai_skill_count; j++)
            if (strcasecmp(g_ai_skills[i], g_ai_skills[j]) > 0) {
                char tn[128]; memcpy(tn, g_ai_skills[i], 128); memcpy(g_ai_skills[i], g_ai_skills[j], 128); memcpy(g_ai_skills[j], tn, 128);
                char td[256]; memcpy(td, g_ai_skill_desc[i], 256); memcpy(g_ai_skill_desc[i], g_ai_skill_desc[j], 256); memcpy(g_ai_skill_desc[j], td, 256);
            }
}
/* Parità AiConfig.SkillsDefaultPath (C#): se nessun path skill è configurato, default
 * `<root>/skills` del repo (come GmbiPaths.SkillsDir). Iniettato una volta su richiesta. */
static int ai_skill_ensure_default(void) {
    if (g_ai_skill_path_count > 0) return 0;
    /* Percorso UNICO via `gmb_paths_skills_dir`: nel monorepo è `<root>/../shared/skills`
     * (copia condivisa con il C#), altrimenti `<root>/skills`. Prima qui si componeva a mano
     * `<root>/skills`: dopo la fusione in `shared/` la VM/C non trovava più nessuna skill e
     * `AI.SKILLSPATHS()` rispondeva `[]` mentre IL/PCode rispondevano con il percorso. */
    char root[1024];
    gmb_paths_skills_dir(root, sizeof(root));
    if (!root[0]) return 0;
    DIR* d = opendir(root);
    if (!d) return 0;
    closedir(d);
    ai_path_add(g_ai_skill_paths, &g_ai_skill_path_count, root);
    ai_skill_scan();
    return 1;
}

int gmb_ai_path_model_add(const char* p) { return ai_path_add(g_ai_model_paths, &g_ai_model_path_count, p); }
int gmb_ai_path_model_remove(const char* p) { return ai_path_remove(g_ai_model_paths, &g_ai_model_path_count, p); }
int gmb_ai_path_skill_add(const char* p) { int r = ai_path_add(g_ai_skill_paths, &g_ai_skill_path_count, p); ai_skill_scan(); return r; }
int gmb_ai_path_skill_remove(const char* p) { int r = ai_path_remove(g_ai_skill_paths, &g_ai_skill_path_count, p); ai_skill_scan(); return r; }
/* AI SKILLS ADD(path) — parità IlRuntime.AiSkillsAdd (C#): COPIA le skill da un percorso
 * sorgente nell'archivio skills (SkillsDefaultPath = <root>/skills): se <path> è una
 * directory, ogni sottocartella con SKILL.md viene copiata; se è un file SKILL.md, copia
 * il singolo. Prima aggiungeva solo il path alla lista (divergenza: la skill copiata dal
 * C# non era visibile al C). Scan dopo la copia. */
int gmb_ai_skill_add(const char* p) {
    if (!p || !*p) return 0;
    char full[1200]; snprintf(full, sizeof(full), "%s", p);
    if (full[0] == '~' && (full[1] == '/' || full[1] == 0)) {
        const char* home = getenv("HOME");
        char tmp[1200]; snprintf(tmp, sizeof(tmp), "%s%s", home ? home : "", full + 1);
        snprintf(full, sizeof(full), "%s", tmp);
    }
    struct stat st;
    if (stat(full, &st) != 0) return 0;
    char dest_dir[1024]; gmb_paths_skills_dir(dest_dir, sizeof(dest_dir));
    plugin_mkdirs(dest_dir);
    int copied = 0;
    if (S_ISDIR(st.st_mode)) {
        DIR* d = opendir(full); if (!d) return 0;
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char sub[1400]; snprintf(sub, sizeof(sub), "%s/%s", full, e->d_name);
            char sf[1500]; snprintf(sf, sizeof(sf), "%s/SKILL.md", sub);
            struct stat s2;
            if (stat(sf, &s2) != 0 || !S_ISREG(s2.st_mode)) continue;
            char dest[1024]; snprintf(dest, sizeof(dest), "%s/%s", dest_dir, e->d_name);
            plugin_mkdirs(dest);
            char fdst[1600]; snprintf(fdst, sizeof(fdst), "%s/SKILL.md", dest);
            copied += plugin_copy_file(sf, fdst);
        }
        closedir(d);
    } else {
        size_t fl = strlen(full);
        if (fl < 8 || strcasecmp(full + fl - 8, "SKILL.md") != 0) return 0;
        char parent[1200]; snprintf(parent, sizeof(parent), "%s", full);
        char* ps = strrchr(parent, '/'); if (!ps) return 0; *ps = 0;
        const char* name = strrchr(parent, '/'); name = name ? name + 1 : parent;
        char dest[1024]; snprintf(dest, sizeof(dest), "%s/%s", dest_dir, name);
        plugin_mkdirs(dest);
        char fdst[1600]; snprintf(fdst, sizeof(fdst), "%s/SKILL.md", dest);
        copied += plugin_copy_file(full, fdst);
    }
    if (copied > 0) ai_skill_scan();
    return copied > 0 ? 1 : 0;
}
int gmb_ai_skill_remove(const char* name) {
    if (!name || !*name) return 0;
    /* parità AiSkillsRemove (C#): cancella la cartella <skillsDir>/<name> (rmtree) + refresh. */
    char dest_dir[1024]; gmb_paths_skills_dir(dest_dir, sizeof(dest_dir));
    char dest[1200]; snprintf(dest, sizeof(dest), "%s/%s", dest_dir, name);
    struct stat st;
    if (stat(dest, &st) == 0 && S_ISDIR(st.st_mode)) {
        plugin_rmtree(dest);
        ai_skill_scan();
        return 1;
    }
    int removed = 0;
    for (int i = 0; i < g_ai_skill_count; i++) if (strcmp(g_ai_skills[i], name) == 0) { for (int j = i; j < g_ai_skill_count - 1; j++) strcpy(g_ai_skills[j], g_ai_skills[j+1]); g_ai_skill_count--; removed = 1; i--; }
    return removed;
}
static int g_skill_index_state_loaded = 0;
static void skill_index_load_persisted(void);
int gmb_ai_skill_refresh(void) {
    if (!g_skill_index_state_loaded) { g_skill_index_state_loaded = 1; skill_index_load_persisted(); }
    ai_skill_scan();
    /* parità SkillIndex.Refresh (C#): genera il prompt e lo SCRIVE nel percorso indice
     * configurato (SET INDEX), con le stesse righe di gmb_ai_skills_prompt. */
    {
        char idx[1200];
        if (g_ai_skill_index_path[0]) snprintf(idx, sizeof(idx), "%s", g_ai_skill_index_path);
        else { char d[1024]; gmb_paths_config_dir(d, sizeof(d)); snprintf(idx, sizeof(idx), "%s/SKILLS_PROMPT.md", d); }
        char pdir[1200]; snprintf(pdir, sizeof(pdir), "%s", idx);
        char* slash = strrchr(pdir, '/');
        if (slash) { *slash = 0; plugin_mkdirs(pdir); }
        char* doc = gmb_ai_skills_prompt();
        if (doc) {
            FILE* f = fopen(idx, "wb");
            if (f) { fputs(doc, f); fclose(f); }
            free(doc);
        }
    }
    return 1;
}
int gmb_ai_skill_set_index(const char* p) {
    if (!p) return 0;
    strncpy(g_ai_skill_index_path, p, sizeof(g_ai_skill_index_path) - 1);
    g_ai_skill_index_path[sizeof(g_ai_skill_index_path) - 1] = '\0';
    /* parità AiConfig.SkillsIndexPath (C#): il setter SALVA subito su config (cross-run).
     * File di stato C-canonicamente in <configDir>/skills-index.json (pattern plugins.json).
     * path vuoto = RESET (unlink → default). */
    char pf[1200]; gmb_paths_skills_index_json(pf, sizeof(pf));
    if (!g_ai_skill_index_path[0]) {
        unlink(pf);
        return 1;
    }
    char pdir[1200]; snprintf(pdir, sizeof(pdir), "%s", pf);
    char* slash = strrchr(pdir, '/');
    if (slash) { *slash = 0; plugin_mkdirs(pdir); }
    FILE* f = fopen(pf, "w");
    if (f) { fprintf(f, "{\"skillsIndexPath\":\"%s\"}\n", g_ai_skill_index_path); fclose(f); }
    return 1;
}
/* Lazy-load del percorso indice persistito da un run precedente (parità cross-run C#). */
static void skill_index_load_persisted(void) {
    char pf[1200]; gmb_paths_skills_index_json(pf, sizeof(pf));
    FILE* f = fopen(pf, "r");
    if (!f) return;
    char buf[2048]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
    char* t = strstr(buf, "\"skillsIndexPath\"");
    if (!t || !(t = strchr(t + 17, ':'))) return;
    t++; while (*t == ' ' || *t == '\t') t++;
    if (*t != '"') return;
    t++;
    char* z = strchr(t, '"'); if (!z || z == t) return;
    size_t len = (size_t)(z - t); if (len >= sizeof(g_ai_skill_index_path)) len = sizeof(g_ai_skill_index_path) - 1;
    memcpy(g_ai_skill_index_path, t, len); g_ai_skill_index_path[len] = 0;
}
const char* gmb_ai_skill_index_path(void) { return g_ai_skill_index_path; }
const char* gmb_ai_skills_csv(void) {
    ai_skill_ensure_default();
    static char out[4096]; out[0] = 0;
    for (int i = 0; i < g_ai_skill_count; i++) { strncat(out, g_ai_skills[i], 4000 - (int)strlen(out)); if (i < g_ai_skill_count - 1) strcat(out, "\n"); }
    return out;
}
/* Array nativo di stringhe (parità IlRuntime.AiSkillsList → string[]): riempie un array
 * dinamico gmb_string di C99Native. Ritorno malloc (NULL se vuoto), *out_n = count. */
gmb_string_t* gmb_ai_skills_list(gmb_int_t* out_n) {
    ai_skill_ensure_default();
    *out_n = g_ai_skill_count;
    if (g_ai_skill_count <= 0) return NULL;
    gmb_string_t* a = (gmb_string_t*)calloc((size_t)g_ai_skill_count, sizeof(gmb_string_t));
    if (!a) { *out_n = 0; return NULL; }
    for (int i = 0; i < g_ai_skill_count; i++) a[i] = gmb_string_alloc(g_ai_skills[i]);
    return a;
}
gmb_string_t* gmb_ai_skills_paths(gmb_int_t* out_n) {
    ai_skill_ensure_default();
    *out_n = g_ai_skill_path_count;
    if (g_ai_skill_path_count <= 0) return NULL;
    gmb_string_t* a = (gmb_string_t*)calloc((size_t)g_ai_skill_path_count, sizeof(gmb_string_t));
    if (!a) { *out_n = 0; return NULL; }
    for (int i = 0; i < g_ai_skill_path_count; i++) a[i] = gmb_string_alloc(g_ai_skill_paths[i]);
    return a;
}

/* Forma SCALARE `[a, b]` (parità oracolo IL che stampa l'array fra parentesi quadre con ", "):
 * `PRINT AI.SKILLSLIST()`/`AI.SKILLSPATHS()` su C99Native. gmb_ai_skills_csv() è
 * newline-joined e senza parentesi, quindi non serviva per questa forma. §0bis: unica
 * implementazione per i due codegen C. */
const char* gmb_ai_skills_list_str(void) {
    ai_skill_ensure_default();
    static char out[65536]; size_t o = 0; out[o++] = '[';
    for (int i = 0; i < g_ai_skill_count; i++) {
        if (i) { if (o + 2 >= sizeof(out)) break; out[o++] = ','; out[o++] = ' '; }
        size_t l = strlen(g_ai_skills[i]);
        if (o + l + 2 >= sizeof(out)) break;
        memcpy(out + o, g_ai_skills[i], l); o += l;
    }
    if (o + 1 < sizeof(out)) out[o++] = ']';
    out[o] = '\0';
    return out;
}
const char* gmb_ai_skills_paths_str(void) {
    ai_skill_ensure_default();
    static char out[65536]; size_t o = 0; out[o++] = '[';
    for (int i = 0; i < g_ai_skill_path_count; i++) {
        if (i) { if (o + 2 >= sizeof(out)) break; out[o++] = ','; out[o++] = ' '; }
        size_t l = strlen(g_ai_skill_paths[i]);
        if (o + l + 2 >= sizeof(out)) break;
        memcpy(out + o, g_ai_skill_paths[i], l); o += l;
    }
    if (o + 1 < sizeof(out)) out[o++] = ']';
    out[o] = '\0';
    return out;
}

/* ── AI OCR / AUDIO — backend registrato dal host (gmb_host_ai via dlopen), indirezione:
   il runtime resta senza dipendenza da llama.cpp; senza backend → errore esplicito (parità C#). ── */

typedef struct gmb_ai_ocr_ctx* gmb_ai_ocr_handle_t;
typedef gmb_ai_ocr_handle_t (*gmb_ocr_load_fn)(const char*, const char*, int, int);
typedef char* (*gmb_ocr_read_fn)(gmb_ai_ocr_handle_t, const char*, const char*, int);
typedef void (*gmb_ocr_free_fn)(char*);
typedef void (*gmb_ocr_unload_fn)(gmb_ai_ocr_handle_t);

static gmb_ocr_load_fn g_ocr_load = NULL;
static gmb_ocr_read_fn g_ocr_read_image = NULL;
static gmb_ocr_read_fn g_ocr_read_audio = NULL;
static gmb_ocr_free_fn g_ocr_free = NULL;
static gmb_ocr_unload_fn g_ocr_unload = NULL;
static gmb_ai_ocr_handle_t g_ocr_ctx = NULL;
static char g_ocr_model[1024] = "";
static char g_ocr_mmproj[1024] = "";
static char g_ai_model_err[1024];

int gmb_ai_register_ocr_backend(void* load, void* read_image, void* read_audio, void* free_s, void* unload) {
    g_ocr_load = (gmb_ocr_load_fn)load;
    g_ocr_read_image = (gmb_ocr_read_fn)read_image;
    g_ocr_read_audio = (gmb_ocr_read_fn)read_audio;
    g_ocr_free = (gmb_ocr_free_fn)free_s;
    g_ocr_unload = (gmb_ocr_unload_fn)unload;
    return (load && read_image) ? 1 : 0;
}

int gmb_ai_ocr_load_model(const char* model, const char* mmproj) {
    if (!g_ocr_load) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI OCR: no active OCR provider — AI SET ACTIVE %%OCR, alias first]"); return 0; }
    /* Fallback env: se i path modello non sono stati impostati (nessun comando %OCR di
     * attivazione modello eseguito), legge GMBASIC_OCR_MODEL / GMBASIC_OCR_MMPROJ cosi'
     * AI.OCR da BASIC funziona out-of-the-box una volta caricata la lib provider. */
    const char* e_m = getenv("GMBASIC_OCR_MODEL");
    const char* e_p = getenv("GMBASIC_OCR_MMPROJ");
    if (!model && e_m) model = e_m;
    if (!mmproj && e_p) mmproj = e_p;
    if (model) { strncpy(g_ocr_model, model, sizeof(g_ocr_model) - 1); g_ocr_model[sizeof(g_ocr_model) - 1] = '\0'; }
    if (mmproj) { strncpy(g_ocr_mmproj, mmproj, sizeof(g_ocr_mmproj) - 1); g_ocr_mmproj[sizeof(g_ocr_mmproj) - 1] = '\0'; }
    if (g_ocr_ctx && g_ocr_unload) { g_ocr_unload(g_ocr_ctx); g_ocr_ctx = NULL; }
    if (!g_ocr_model[0] || !g_ocr_mmproj[0]) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI OCR: model paths not set — AI SET ACTIVE %%OCR, alias first]"); return 0; }
    g_ocr_ctx = g_ocr_load(g_ocr_model, g_ocr_mmproj, 2048, 0);
    if (!g_ocr_ctx) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI OCR ERROR: model load failed]"); return 0; }
    return 1;
}

const char* gmb_ai_ocr_read(const char* path, const char* prompt, int max_tokens) {
    if (!path || !path[0]) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI OCR: file not found]"); return g_ai_model_err; }
    if (access(path, R_OK) != 0) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI OCR: file not found]"); return g_ai_model_err; }
    if (!g_ocr_load || !g_ocr_read_image) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI OCR: no active OCR provider — AI SET ACTIVE %%OCR, alias first]"); return g_ai_model_err; }
    if (!g_ocr_ctx) { if (!gmb_ai_ocr_load_model(g_ocr_model[0] ? g_ocr_model : NULL, g_ocr_mmproj[0] ? g_ocr_mmproj : NULL)) return g_ai_model_err; }
    char* out = g_ocr_read_image(g_ocr_ctx, path, prompt ? prompt : "Extract all text from this image.", max_tokens > 0 ? max_tokens : 512);
    if (!out) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI OCR ERROR: read failed]"); return g_ai_model_err; }
    if (g_ocr_free) { static char buf[4096]; snprintf(buf, sizeof(buf), "%s", out); g_ocr_free(out); return buf; }
    return out;
}

const char* gmb_ai_audio_transcribe(const char* path, const char* prompt, int max_tokens) {
    if (!path || !path[0]) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI Transcribe: file not found]"); return g_ai_model_err; }
    if (access(path, R_OK) != 0) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI Transcribe: file not found]"); return g_ai_model_err; }
    if (!g_ocr_load || !g_ocr_read_audio) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI Transcribe: no active audio provider — AI SET ACTIVE %%AUDIO, alias first]"); return g_ai_model_err; }
    if (!g_ocr_ctx) { if (!gmb_ai_ocr_load_model(g_ocr_model[0] ? g_ocr_model : NULL, g_ocr_mmproj[0] ? g_ocr_mmproj : NULL)) return g_ai_model_err; }
    char* out = g_ocr_read_audio(g_ocr_ctx, path, prompt ? prompt : "", max_tokens > 0 ? max_tokens : 512);
    if (!out) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI Transcribe ERROR: read failed]"); return g_ai_model_err; }
    if (g_ocr_free) { static char buf[4096]; snprintf(buf, sizeof(buf), "%s", out); g_ocr_free(out); return buf; }
    return out;
}

void gmb_ai_ocr_unload(void) {
    if (g_ocr_ctx && g_ocr_unload) g_ocr_unload(g_ocr_ctx);
    g_ocr_ctx = NULL;
}

int gmb_ai_ocr_active(void) {
    /* speculare a ProviderServer.GetActiveName("ocr"): c'è un backend registrato con read_image
     * e un modello caricato (context pronto) — altrimenti l'OCR non può partire. */
    return (g_ocr_load && g_ocr_read_image && g_ocr_ctx) ? 1 : 0;
}

int gmb_ai_audio_active(void) {
    return (g_ocr_load && g_ocr_read_audio && g_ocr_ctx) ? 1 : 0;
}

/* ── AI EMBED — backend modello (gmb_host_ai via dlopen), stessa indirezione dell'OCR.
   Senza backend → errore esplicito; la dimensione embedding è disponibile dopo load. ── */

typedef int (*gmb_embed_load_fn)(const char*, int);
typedef void (*gmb_embed_unload_fn)(int);
typedef int (*gmb_embed_dim_fn)(int);
typedef int (*gmb_embed_text_fn)(int, const char*, float*, int);

static gmb_embed_load_fn g_emb_load = NULL;
static gmb_embed_unload_fn g_emb_unload = NULL;
static gmb_embed_dim_fn g_emb_dim = NULL;
static gmb_embed_text_fn g_emb_text = NULL;
static int g_emb_handle = -1;
static char g_emb_model_path[1024] = "";

int gmb_ai_register_embed_backend(void* load, void* unload, void* dim, void* text) {
    g_emb_load = (gmb_embed_load_fn)load;
    g_emb_unload = (gmb_embed_unload_fn)unload;
    g_emb_dim = (gmb_embed_dim_fn)dim;
    g_emb_text = (gmb_embed_text_fn)text;
    return (load && text) ? 1 : 0;
}

int gmb_ai_embed_load(const char* model, int n_gpu_layers) {
    if (!g_emb_load) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI embed: no active embedding provider — AI SET ACTIVE %%EMBED, alias first]"); return 0; }
    if (model) { strncpy(g_emb_model_path, model, sizeof(g_emb_model_path) - 1); g_emb_model_path[sizeof(g_emb_model_path) - 1] = '\0'; }
    if (!g_emb_model_path[0]) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI embed: model path not set — AI SET ACTIVE %%EMBED, alias first]"); return 0; }
    if (g_emb_handle >= 0 && g_emb_unload) { g_emb_unload(g_emb_handle); g_emb_handle = -1; }
    g_emb_handle = g_emb_load(g_emb_model_path, n_gpu_layers > 0 ? n_gpu_layers : 0);
    if (g_emb_handle < 0) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI embed ERROR: model load failed]"); return 0; }
    return 1;
}

int gmb_ai_embed_dim(void) {
    if (g_emb_handle < 0 || !g_emb_dim) return 0;
    return g_emb_dim(g_emb_handle);
}

int gmb_ai_embed_text(const char* text, float* out, int cap) {
    if (!text || !out || cap <= 0) return 0;
    if (g_emb_handle < 0 || !g_emb_text) { snprintf(g_ai_model_err, sizeof(g_ai_model_err), "[AI embed: no active embedding provider — AI SET ACTIVE %%EMBED, alias first]"); return 0; }
    return g_emb_text(g_emb_handle, text, out, cap);
}

void gmb_ai_embed_unload(void) {
    if (g_emb_handle >= 0 && g_emb_unload) g_emb_unload(g_emb_handle);
    g_emb_handle = -1;
}

/* AI PLUGINS REMOVE(name): deregistra + cancella la cartella (Directory.Delete(recursive)
 * del C#) e riscrive l'indice. Ritorna 1 se la cartella esisteva (altrimenti 0, come C#). */
int llmengine_plugin_remove(const char* name) {
    if (!name || !*name) return 0;
    for (int i = 0; i < g_plugin_count; i++)
        if (strcmp(g_plugins[i].name, name) == 0) {
            for (int j = i; j < g_plugin_count - 1; j++) g_plugins[j] = g_plugins[j + 1];
            g_plugin_count--;
            break;
        }
    char dir[1200]; plugin_dir_of(name, dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    plugin_rmtree(dir);
    llmengine_plugin_refresh();
    return 1;
}
/* ── AI.PLUGINCALL: procedura BASIC dichiarata dal plugin ────────────────────────────────
 * `PLUGIN.md` → `procedures: [Prima, Seconda]`. Ritorna la PRIMA (heap, "" se assente),
 * cioè ciò che C# usa in `PluginRegistry.Call` (`record.Procedures.First()`).
 * Serve a C99Native: non c'è una VM BASIC nel binario, quindi il transpiler genera un
 * dispatcher verso le `gmb_user_*` del programma e ha bisogno del NOME della procedura a
 * runtime (il descrittore del plugin è dato, non compilato). */
char* gmb_plugin_procedure(const char* name) {
    if (!name || !*name) return strdup("");
    char dir[1024], pj[1200], raw[512];
    plugin_dir_of(name, dir, sizeof(dir));
    snprintf(pj, sizeof(pj), "%s/PLUGIN.md", dir);
    if (!plugin_read_field(pj, "procedures", raw, sizeof(raw))) return strdup("");
    /* formato frontmatter: "[A, B]" (o "A, B" senza parentesi) → primo identificatore. */
    char* p = raw;
    while (*p == ' ' || *p == '\t' || *p == '[') p++;
    char* e = p;
    while (*e && *e != ',' && *e != ']' && *e != ' ' && *e != '\t' && *e != '\n' && *e != '\r') e++;
    size_t n = (size_t)(e - p);
    if (!n) return strdup("");
    char* out = (char*)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, p, n); out[n] = 0;
    return out;
}

/* AI PLUGINS REFRESH(): rigenera PLUGINS_PROMPT.md come PluginIndex.Refresh (C#) —
 * stesso header, stesso formato riga, BOM UTF-8. Ritorna 1 se scritto, 0 altrimenti. */
int llmengine_plugin_refresh(void) {
    char dir[1024]; plugin_base_dir(dir, sizeof(dir));
    /* Indice = OUTPUT rigenerabile nella CACHE utente (<configDir>), NON dentro l'archivio
     * (può essere in sola lettura) — parità AiConfig.PluginsPromptPath (C#). */
    char cdir[1024]; gmb_paths_config_dir(cdir, sizeof(cdir));
    plugin_mkdirs(cdir);
    char idx[1400]; snprintf(idx, sizeof(idx), "%s/PLUGINS_PROMPT.md", cdir);
    char names[256][64];
    int n = gmb_plugins_scan(names, 256);
    FILE* f = fopen(idx, "wb");
    if (!f) return 0;
    fputs("\xEF\xBB\xBF# GMBASIC — Archivio Plugin Agente\n\n", f);
    fprintf(f, "> **Percorso configurato**: %s\n", dir);
    fprintf(f, "> **Percorso indice**: `%s`\n", idx);
    fprintf(f, "> %d plugin rilevati.\n\n", n);
    fputs("## Istruzioni per l'agente\n"
          "Quando ricevi una richiesta che corrisponde a un plugin sotto, leggi il file\n"
          "`PLUGIN.md` del plugin rilevante (percorso: `plugins/<nome>/PLUGIN.md`) e segui\n"
          "le sue istruzioni. Un plugin \xc3\xa8 una **capacit\xc3\xa0 operativa**: pu\xc3\xb2 registrare tool\n"
          "invocabili e procedure BASIC. Se pi\xc3\xb9 plugin sono rilevanti, leggili tutti e combinali.\n\n", f);
    for (int i = 0; i < n; i++) {
        char pj[1400], desc[1024], trig[512], en[64];
        snprintf(pj, sizeof(pj), "%s/%s/PLUGIN.md", dir, names[i]);
        if (!plugin_read_field(pj, "description", desc, sizeof(desc))) desc[0] = 0;
        trig[0] = 0; plugin_read_field(pj, "trigger", trig, sizeof(trig));
        int enabled = 1;
        if (plugin_read_field(pj, "enabled", en, sizeof(en)) &&
            strcasecmp(en, "false") == 0) enabled = 0;
        fprintf(f, "- **`%s`** \xe2\x80\x94 %s \xc2\xb7 stato: %s", names[i], desc, enabled ? "attivo" : "disattivo");
        if (trig[0]) fprintf(f, " \xc2\xb7 trigger: %s", trig);
        fputc('\n', f);
    }
    fclose(f);
    return 1;
}
/* AI PLUGINS SET INDEX(path): la cartella base dell'archivio (parità AiConfig, C#);
 * vale per CREATE/REMOVE/SCAN/INFO/REFRESH tramite plugin_base_dir. Supporta `~/`.
 * Persistenza: `<configDir>/plugins.json` {"pluginsDefaultPath":"..."} — parità del
 * setter C# AiConfig.PluginsDefaultPath (salva subito su config, cross-run). */
int llmengine_plugin_set_index(const char* name) {
    if (!name) return 0;
    if (!*name) {
        /* RESET: rimuove l'override in-process e il file persistito → default (parità C#
         * con SET INDEX "": AiConfig salva null e il getter torna al default). */
        g_plugin_dir_override[0] = 0;
        char pf[1200]; gmb_paths_plugins_json(pf, sizeof(pf));
        unlink(pf);
        llmengine_plugin_refresh();
        return 1;
    }
    if (name[0] == '~' && (name[1] == '/' || name[1] == 0)) {
        const char* home = getenv("HOME");
        snprintf(g_plugin_dir_override, sizeof(g_plugin_dir_override), "%s%s", home ? home : "", name + 1);
    } else {
        snprintf(g_plugin_dir_override, sizeof(g_plugin_dir_override), "%s", name);
    }
    {   /* salva su <configDir>/plugins.json (pattern providers.json). */
        char pf[1200]; gmb_paths_plugins_json(pf, sizeof(pf));
        char pdir[1200]; snprintf(pdir, sizeof(pdir), "%s", pf);
        char* slash = strrchr(pdir, '/');
        if (slash) { *slash = 0; plugin_mkdirs(pdir); *slash = '/'; }
        FILE* f = fopen(pf, "w");
        if (f) { fprintf(f, "{\"pluginsDefaultPath\":\"%s\"}\n", g_plugin_dir_override); fclose(f); }
    }
    llmengine_plugin_refresh();
    return 1;
}
/* Carica l'override da <configDir>/plugins.json all'uso (parità cross-run del C#). */
static void plugin_load_persisted_index(void) {
    char pf[1200]; gmb_paths_plugins_json(pf, sizeof(pf));
    FILE* f = fopen(pf, "r");
    if (!f) return;
    char buf[2048]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
    char* t = strstr(buf, "\"pluginsDefaultPath\"");
    if (!t || !(t = strchr(t + 19, ':'))) return;
    t++; while (*t == ' ' || *t == '\t') t++;
    if (*t != '"') return;
    t++;
    char* z = strchr(t, '"'); if (!z || z == t) return;
    size_t len = (size_t)(z - t); if (len >= sizeof(g_plugin_dir_override)) len = sizeof(g_plugin_dir_override) - 1;
    memcpy(g_plugin_dir_override, t, len); g_plugin_dir_override[len] = 0;
}

/* ── AI SKILLS PROMPT / MARKETPLACE / MCP REFRESH-GET / PLUGINS MARKETPLACE / PROVIDER DISCOVER ── */

char* gmb_ai_skills_prompt(void) {
    ai_skill_ensure_default();
    ai_skill_scan();
    /* Percorso dell'INDICE (file riscritto da gmb_ai_skill_refresh): override `SET INDEX`,
     * altrimenti <configDir>/SKILLS_PROMPT.md — NON dentro l'archivio skills (vedi
     * gmb_ai_skill_refresh: l'archivio può essere in sola lettura). */
    char idx[1200];
    if (g_ai_skill_index_path[0]) snprintf(idx, sizeof(idx), "%s", g_ai_skill_index_path);
    else { char cd[1024]; gmb_paths_config_dir(cd, sizeof(cd)); snprintf(idx, sizeof(idx), "%s/SKILLS_PROMPT.md", cd); }
    char paths[4096]; paths[0] = 0;
    for (int i = 0; i < g_ai_skill_path_count; i++) {
        if (i) strncat(paths, "; ", sizeof(paths) - strlen(paths) - 1);
        strncat(paths, g_ai_skill_paths[i], sizeof(paths) - strlen(paths) - 1);
    }
    char out[65536]; size_t len = 0;
    len += (size_t)snprintf(out + len, sizeof(out) - len,
        "# GMBASIC \xe2\x80\x94 Archivio Skills Agente\n\n"
        "> **Percorsi configurati**: %s\n"
        "> **Percorso indice**: `%s`\n"
        "> %d skill rilevate.\n\n"
        "## Istruzioni per l'agente\n"
        "Quando ricevi una richiesta che corrisponde a una delle skill sotto, leggi il file\n"
        "`SKILL.md` della skill rilevante (percorso: `skills/<nome>/SKILL.md`) e segui\n"
        "le sue istruzioni. Il riassunto qui sotto e' solo un indice: il dettaglio operativo e' nel\n"
        "singolo file. Se piu' skill sono rilevanti, leggile tutte e combinale.\n\n",
        paths, idx, g_ai_skill_count);
    for (int i = 0; i < g_ai_skill_count; i++)
        len += (size_t)snprintf(out + len, sizeof(out) - len, "- **`%s`** \xe2\x80\x94 %s\n", g_ai_skills[i], g_ai_skill_desc[i]);
    return strdup(out);
}

int gmb_ai_skills_marketplace(const char* url, const char* dest) {
    if (!url || !*url) return 0;
    char target[1024];
    if (dest && *dest) snprintf(target, sizeof(target), "%s", dest);
    else { char base[1024]; gmb_paths_skills_marketplaces_dir(base, sizeof(base)); snprintf(target, sizeof(target), "%s/%.64s", base, url); }
    mkdir(target, 0755);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cd '%s' && git clone --depth 1 '%s' repo >/dev/null 2>&1", target, url);
    if (system(cmd) != 0) return 0;
    char repo[1100];
    snprintf(repo, sizeof(repo), "%s/repo", target);
    gmb_ai_path_skill_add(repo);
    gmb_ai_skill_refresh();
    return 1;
}

int gmb_ai_plugins_marketplace(const char* url, const char* dest) {
    if (!url || !*url) return 0;
    char target[1024];
    if (dest && *dest) snprintf(target, sizeof(target), "%s", dest);
    else { char base[1024]; gmb_paths_plugins_marketplaces_dir(base, sizeof(base)); snprintf(target, sizeof(target), "%s/%.64s", base, url); }
    mkdir(target, 0755);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cd '%s' && git clone --depth 1 '%s' repo >/dev/null 2>&1", target, url);
    if (system(cmd) != 0) return 0;
    char repo[1100];
    snprintf(repo, sizeof(repo), "%s/repo", target);
    llmengine_plugin_register(repo, "plugin", "");
    return 1;
}

int gmb_mcp_server_refresh(void) { return 1; }

char* gmb_mcp_server_marketplace_get(const char* name) {
    if (!name || !*name) return strdup("");
    for (int i = 0; g_mcp_marketplace[i]; i++) {
        /* formato "nome|kind|source" */
        const char* e = g_mcp_marketplace[i];
        size_t nlen = 0;
        while (e[nlen] && e[nlen] != '|') nlen++;
        if (nlen == strlen(name) && strncmp(e, name, nlen) == 0) {
            const char* k = e + nlen + 1;
            size_t klen = 0;
            while (k[klen] && k[klen] != '|') klen++;
            const char* src = k + klen + 1;
            gmb_mcp_server_add(name, k, src);
            return strdup(e);
        }
    }
    /* Parità C# McpServerMarketplaceGet (2026-09-23, Parita-5-Backend-Residui §2 —
     * McpFullSurface): server ignoto = ERRORE esplicito (lancia), mai "" silenzioso.
     * Prima VMC proseguiva (rc=0) mentre IL/PCode abortivano (rc=6). */
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Server MCP '%s' non nel catalogo (usa AI MCP SERVER MARKETPLACE LIST).", name);
        gmb_raise_error(0, msg, 0L);
        return NULL;
    }
}

char* gmb_provider_discover(const char* prefix, const char* capability, const char* base_url, const char* api_key, int solo_free) {
    (void)capability; (void)solo_free;
    if (!base_url || !*base_url) return strdup("");
    char url[2048];
    snprintf(url, sizeof(url), "%s/models", base_url);
    gmb_net_http_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.method = "GET";
    char auth[1024];
    if (api_key && *api_key) {
        snprintf(auth, sizeof(auth), "Bearer %s", api_key);
        opts.header_names = (const char*[]){ "Authorization" };
        opts.header_values = (const char*[]){ auth };
        opts.header_count = 1;
    }
    int status = 0;
    char* body = NULL; size_t body_len = 0;
    char* rerr = NULL; char* rheaders = NULL;
    gmb_net_http_request(url, &opts, &status, &body, &body_len, &rerr, &rheaders);
    // Errore di rete (connect rifiutato/timeout): solleva come IL/PCode ("Connection refused
    // (host:port)") invece di ritornare una stringa vuota (che stampava `n= 0`).
    if (rerr) {
        // Messaggio canonico condiviso con IL/PCode/C# HttpClient: "Connection refused
        // (host:port)" (curl riporta solo "Couldn't connect to server"). Per altri errori di
        // rete si propaga il testo del transport.
        char emsg[640];
        if (strstr(rerr, "onnect") || strstr(rerr, "connect")) {
            char hostport[512] = "";
            const char* p = strstr(url, "://"); p = p ? p + 3 : url;
            const char* slash = strchr(p, '/');
            size_t hn = slash ? (size_t)(slash - p) : strlen(p);
            if (hn < sizeof(hostport)) { memcpy(hostport, p, hn); hostport[hn] = 0; }
            snprintf(emsg, sizeof(emsg), "Connection refused (%s)", hostport);
        } else {
            snprintf(emsg, sizeof(emsg), "%s", rerr);
        }
        free(rheaders); free(rerr); free(body);
        gmb_raise_error(0, strdup(emsg), 0);
        return strdup("");
    }
    free(rheaders); free(rerr);
    char* out = (char*)malloc(16384);
    if (!out) { free(body); return strdup(""); }
    out[0] = '\0';
    if (body && status >= 200 && status < 300) {
        gmb_val_t v = gmb_json_parse(body);
        gmb_list_t* arr = NULL;
        if (v.kind == GMB_TYPE_OBJECT && v.v_ptr) {
            gmb_dict_t* root = (gmb_dict_t*)((gmb_obj_t*)v.v_ptr)->data;
            if (root) {
                gmb_val_t data = gmb_dict_get(*root, "data");
                if (data.kind == GMB_TYPE_OBJECT && data.v_ptr) arr = (gmb_list_t*)((gmb_obj_t*)data.v_ptr)->data;
                if (!arr) {
                    gmb_val_t models = gmb_dict_get(*root, "models");
                    if (models.kind == GMB_TYPE_OBJECT && models.v_ptr) arr = (gmb_list_t*)((gmb_obj_t*)models.v_ptr)->data;
                }
            }
        }
        if (arr) {
            for (size_t i = 0; i < arr->count; i++) {
                gmb_val_t it = gmb_list_get(*arr, i);
                const char* id = "";
                if (it.kind == GMB_TYPE_OBJECT && it.v_ptr) {
                    gmb_dict_t* d = (gmb_dict_t*)((gmb_obj_t*)it.v_ptr)->data;
                    gmb_val_t idv = d ? gmb_dict_get(*d, "id") : (gmb_val_t){0};
                    if (idv.kind == GMB_TYPE_STRING && idv.v_string.data) id = idv.v_string.data;
                }
                if (strlen(out) + strlen(prefix) + strlen(id) + 2 < 16000) {
                    if (out[0]) strcat(out, "\n");
                    strcat(out, prefix);
                    strcat(out, ":");
                    strcat(out, id);
                }
            }
        }
    }
    free(body);
    return out;
}

/* PROCESS(file, prompt?) — parità IlRuntime.ProcessDocument: estrae il testo del
 * documento, crea l'agente "doc" (idempotente) e chiede con prompt + testo. */
char* gmb_process_document(const char* file, const char* prompt) {
    if (!file || !*file) return strdup("");
    FILE* f = fopen(file, "rb");
    if (!f) return strdup("");
    char* text = (char*)malloc(65536);
    if (!text) { fclose(f); return strdup(""); }
    size_t n = fread(text, 1, 65535, f);
    fclose(f);
    text[n] = '\0';
    char* full = (char*)malloc(n + 2048);
    if (!full) { free(text); return strdup(""); }
    snprintf(full, n + 2048, "%s\n\n--- DOCUMENTO (%s) ---\n%s", (prompt && *prompt) ? prompt : "Riassumi il documento.", file, text);
    free(text);
    gmb_agent_create("doc");
    char* ans = gmb_agent_ask("doc", full);
    free(full);
    if (!ans) ans = strdup("");
    return ans;
}

/* gmb_ai_analyze_image — AI.ANALYZEIMAGE (porting IlRuntime.AiAnalyzeImage).
 * Il runtime C headless non ha un modello vision: se un backend OCR è registrato lo usa
 * (lettura testo), altrimenti errore esplicito (mai stub — regola §0bis). */
char* gmb_ai_analyze_image(const char* path, const char* prompt) {
    if (!path || !*path) return strdup("ERROR: file immagine non trovato");
    FILE* f = fopen(path, "rb");
    if (!f) { char m[320]; snprintf(m, sizeof(m), "ERROR: file immagine non trovato: %s", path); return strdup(m); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fclose(f);
    if (sz <= 0) return strdup("ERROR: lettura immagine fallita");
    if (g_ocr_read_image) {
        const char* text = g_ocr_read_image(NULL, path, "", 0);
        if (text && *text) {
            size_t n = strlen(text) + 64;
            char* out = (char*)malloc(n);
            if (out) snprintf(out, n, "[vision %s]: %s", prompt ? prompt : "descrivi", text);
            return out;
        }
    }
    return strdup("ERROR: AI.ANALYZEIMAGE non disponibile su questo backend (nessun modello vision)");
}
