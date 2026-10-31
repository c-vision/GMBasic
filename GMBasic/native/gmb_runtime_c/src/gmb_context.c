/* gmb_context.c — Context Engine (P2 AI-Runtime consolidato, asse B).
 * SPECCHIO ESATTO del C# AiContextEngine: budget (chars) per agent, view curata,
 * selector/policy. Registry per-agent (kv semplice). Testi d'errore e semantica
 * identici per la parità 5/5 (mai "vicino"). */
#include "gmb_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp (normalizzazione del kind) */

#define GMB_CTX_MAX 32
#define GMB_CTX_VIEW_MAX 65536
#define GMB_CTX_MAX_SEGS 32
#define GMB_CTX_SEG_TEXT_MAX 4096

typedef struct {
    char kind[16];                          /* SYSTEM|HISTORY|TOOL|OTHER (PARSEC §3.6) */
    char text[GMB_CTX_SEG_TEXT_MAX];
} gmb_ctx_seg;

typedef struct {
    int used;
    char agent[128];
    long long budget;
    char selector[256];
    char policy[16];
    char view[GMB_CTX_VIEW_MAX];
    long long overflow;   /* overflow dell'ultima view NORMALIZZATA (v. gmb_ctx_normalize) */
    int n_seg;                              /* PARSEC §3.6: segmenti del context builder */
    gmb_ctx_seg seg[GMB_CTX_MAX_SEGS];
} gmb_ctx_entry;

static gmb_ctx_entry g_ctx[GMB_CTX_MAX];

static gmb_ctx_entry* ctx_find(const char* agent)
{
    if (!agent || !agent[0]) agent = "@default";
    for (int i = 0; i < GMB_CTX_MAX; i++)
        if (g_ctx[i].used && strcmp(g_ctx[i].agent, agent) == 0) return &g_ctx[i];
    for (int i = 0; i < GMB_CTX_MAX; i++)
        if (!g_ctx[i].used) {
            g_ctx[i].used = 1;
            snprintf(g_ctx[i].agent, sizeof(g_ctx[i].agent), "%s", agent);
            g_ctx[i].budget = 0;
            g_ctx[i].selector[0] = 0;
            snprintf(g_ctx[i].policy, sizeof(g_ctx[i].policy), "%s", "compact");
            g_ctx[i].view[0] = 0;
            return &g_ctx[i];
        }
    return NULL; /* capacity exceeded: return NULL, caller treats as no-op */
}

/* AI CONTEXT SET BUDGET agent, chars */
double gmb_ctx_set_budget(const char* agent, double chars)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e) return 0;
    e->budget = (long long)chars;
    return 1;
}

/* AI CONTEXT SET SELECTOR agent, mode (v1: no-op, conserva) */
double gmb_ctx_set_selector(const char* agent, const char* mode)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e) return 0;
    snprintf(e->selector, sizeof(e->selector), "%s", mode ? mode : "");
    return 1;
}

/* AI CONTEXT SET POLICY agent, mode (compact|truncate|error) */
double gmb_ctx_set_policy(const char* agent, const char* mode)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e) return 0;
    snprintf(e->policy, sizeof(e->policy), "%s", mode && mode[0] ? mode : "compact");
    return 1;
}

/* Normalizza un testo secondo budget+policy — STESSA formula del C# AiContextEngine.Normalize
 * (parità 5/5, byte-identica):
 *   budget<=0 o entro budget → invariato; truncate → primi budget char;
 *   compact (default) → testa 60% + marcatore con i char omessi + coda, garantito <= budget;
 *   error → il chiamante rifiuta (ritorno 0, nessun taglio silenzioso).
 * out deve avere cap >= budget+1. Ritorna 1 se ok (scritto in out), 0 se policy error. */
static int gmb_ctx_normalize(gmb_ctx_entry* e, const char* text, char* out, size_t cap, long long* overflow)
{
    size_t len = strlen(text);
    *overflow = 0;
    if (e->budget <= 0 || (long long)len <= e->budget) { snprintf(out, cap, "%s", text); return 1; }
    *overflow = (long long)len - e->budget;
    if (strcmp(e->policy, "error") == 0) return 0;
    if (strcmp(e->policy, "truncate") == 0) {
        size_t cut = (size_t)e->budget; if (cut >= cap) cut = cap - 1;
        memcpy(out, text, cut); out[cut] = 0;
        return 1;
    }
    /* compact: marcatore FISSO + testa 60% / coda 40% → risultato di lunghezza ESATTAMENTE budget
     * (stessa formula del C# AiContextEngine.Normalize: parità byte-identica). */
    static const char* MARKER = "\n…compattato…\n";   /* 14 char */
    long long mlen = (long long)strlen(MARKER);
    if (e->budget <= mlen) {
        size_t cut = (size_t)e->budget; if (cut >= cap) cut = cap - 1;
        memcpy(out, text, cut); out[cut] = 0;
        return 1;
    }
    long long room = e->budget - mlen;
    long long head = room * 6 / 10;
    long long tail = room - head;
    if (head + tail > (long long)len) { head = (long long)len; tail = 0; }
    size_t used = 0;
    used += (size_t)snprintf(out + used, cap - used, "%.*s", (int)head, text);
    used += (size_t)snprintf(out + used, cap - used, "%s", MARKER);
    used += (size_t)snprintf(out + used, cap - used, "%s", text + (len - (size_t)tail));
    (void)used;
    return 1;
}

/* AI CONTEXT VIEW agent, text */
double gmb_ctx_set_view(const char* agent, const char* text)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e) return 0;
    if (!text) text = "";
    char buf[GMB_CTX_VIEW_MAX];
    long long overflow = 0;
    if (!gmb_ctx_normalize(e, text, buf, sizeof(buf), &overflow)) return 0;   /* policy error: rifiuto esplicito */
    snprintf(e->view, sizeof(e->view), "%s", buf);
    e->overflow = overflow;
    return 1;
}

/* Policy corrente dell'agente (compact|truncate|error): serve al loop del worker, che decide
 * lo stop del run PRIMA di costruire la vista (speculare a AiContextEngine.GetPolicyFor). */
const char* gmb_ctx_get_policy(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    return e ? e->policy : "compact";
}

/* letture */
double gmb_ctx_used(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    return e ? (double)strlen(e->view) : 0;
}

double gmb_ctx_budget(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    return e ? (double)e->budget : 0;
}

double gmb_ctx_overflow(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    return e ? (double)e->overflow : 0;
}

gmb_string_t gmb_ctx_view(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    return gmb_string_alloc(e ? e->view : "");
}

/* §3.6 PARSEC — cache/version-hash: FNV-1a a 64 bit della view curata. Il hash è la CHIAVE della
 * prefix-cache KV (coordinamento context-cache ↔ prefix KV): se la view cambia, il hash cambia e
 * la cache-KV è invalidata. `gmb_ctx_valid` confronta "rev=<hash>" con una revisione attesa. */
static unsigned long long gmb_ctx_fnv(const char* s)
{
    unsigned long long h = 1469598103934665603ULL;
    for (; s && *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
    return h;
}

gmb_string_t gmb_ctx_hash(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    char b[32]; snprintf(b, sizeof b, "%016llx", gmb_ctx_fnv(e ? e->view : ""));
    return gmb_string_alloc(b);
}

double gmb_ctx_valid(const char* agent, const char* rev)
{
    gmb_ctx_entry* e = ctx_find(agent);
    char h[32]; snprintf(h, sizeof h, "%016llx", gmb_ctx_fnv(e ? e->view : ""));
    char b[40]; snprintf(b, sizeof b, "rev=%s", h);
    return (rev && (strcmp(rev, b) == 0 || strcmp(rev, h) == 0)) ? 1 : 0;
}

/* ─────────────────────────── PARSEC §3.6 — context builder (lato AGENTE) ─────────────────────────── */

static const char* ctx_norm_kind(const char* kind)
{
    if (!kind || !kind[0]) return "OTHER";
    if (strcasecmp(kind, "SYSTEM") == 0)  return "SYSTEM";
    if (strcasecmp(kind, "HISTORY") == 0) return "HISTORY";
    if (strcasecmp(kind, "TOOL") == 0)    return "TOOL";
    return "OTHER";
}

static long long ctx_seg_total(const gmb_ctx_entry* e)
{
    long long t = 0;
    for (int i = 0; i < e->n_seg; i++) t += (long long)strlen(e->seg[i].text);
    return t;
}

static void ctx_seg_drop(gmb_ctx_entry* e, int idx, int* dropped)
{
    for (int k = idx; k + 1 < e->n_seg; k++) e->seg[k] = e->seg[k + 1];
    e->n_seg--; (*dropped)++;
}

/* AI CONTEXT CREATE <agent> — azzera i segmenti dello store dell'agente. */
double gmb_ctxseg_create(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e) return 0;
    e->n_seg = 0;
    return 1;
}

/* AI CONTEXT ADD <agent>, <kind>, <text> — appende un segmento (kind normalizzato). */
double gmb_ctx_add(const char* agent, const char* kind, const char* text)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e || e->n_seg >= GMB_CTX_MAX_SEGS) return 0;
    int i = e->n_seg++;
    snprintf(e->seg[i].kind, sizeof(e->seg[i].kind), "%s", ctx_norm_kind(kind));
    snprintf(e->seg[i].text, sizeof(e->seg[i].text), "%s", text ? text : "");
    return 1;
}

/* AI CONTEXT MAP <agent> — stampa la mappa (report deterministico, due righe) -> 1. */
double gmb_ctx_map(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    const char* a = (agent && agent[0]) ? agent : "@default";
    long long total = e ? ctx_seg_total(e) : 0;
    long long sys = 0, hist = 0, tool = 0, other = 0;
    if (e) for (int i = 0; i < e->n_seg; i++) {
        long long L = (long long)strlen(e->seg[i].text);
        if (strcmp(e->seg[i].kind, "SYSTEM") == 0) sys += L;
        else if (strcmp(e->seg[i].kind, "HISTORY") == 0) hist += L;
        else if (strcmp(e->seg[i].kind, "TOOL") == 0) tool += L;
        else other += L;
    }
    printf("ctx %s: segs=%d chars=%lld budget=%lld overflow=%lld\n",
           a, e ? e->n_seg : 0, total, e ? e->budget : 0, e ? e->overflow : 0);
    printf("SYSTEM=%lld HISTORY=%lld TOOL=%lld OTHER=%lld\n", sys, hist, tool, other);
    return 1;
}

/* AI CONTEXT TRIM <agent> — elimina i segmenti non-SYSTEM più VECCHI finché il totale <= budget.
 * Ritorna il numero di segmenti rimossi. */
double gmb_ctx_trim(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e || e->budget <= 0) return 0;
    long long total = ctx_seg_total(e);
    int dropped = 0;
    while (total > e->budget) {
        int idx = -1;
        for (int i = 0; i < e->n_seg; i++)
            if (strcmp(e->seg[i].kind, "SYSTEM") != 0) { idx = i; break; }
        if (idx < 0) break;   /* resta solo SYSTEM: pinned */
        total -= (long long)strlen(e->seg[idx].text);
        ctx_seg_drop(e, idx, &dropped);
    }
    return (double)dropped;
}

/* AI CONTEXT GC <agent> — eviction: (1) segmenti DUPLICATI (stesso kind+text); (2) HISTORY
 * oltre gli ultimi 8. Ritorna il numero di segmenti rimossi. */
double gmb_ctx_gc(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e) return 0;
    int dropped = 0;
    for (int i = 0; i < e->n_seg; i++) {
        for (int j = i + 1; j < e->n_seg; ) {
            if (strcmp(e->seg[i].kind, e->seg[j].kind) == 0 &&
                strcmp(e->seg[i].text, e->seg[j].text) == 0) ctx_seg_drop(e, j, &dropped);
            else j++;
        }
    }
    int hist = 0;
    for (int i = 0; i < e->n_seg; i++) if (strcmp(e->seg[i].kind, "HISTORY") == 0) hist++;
    for (int i = 0; i < e->n_seg && hist > 8; ) {
        if (strcmp(e->seg[i].kind, "HISTORY") == 0) { ctx_seg_drop(e, i, &dropped); hist--; }
        else i++;
    }
    return (double)dropped;
}

/* AI CONTEXT COMMIT <agent> — costruisce la VIEW dai segmenti (SYSTEM per primi, poi gli altri
 * nell'ordine) e la normalizza secondo budget+policy (come AI CONTEXT VIEW). Ritorna 1 ok, 0 policy error. */
double gmb_ctx_commit(const char* agent)
{
    gmb_ctx_entry* e = ctx_find(agent);
    if (!e) return 0;
    char buf[GMB_CTX_VIEW_MAX]; size_t used = 0; buf[0] = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < e->n_seg; i++) {
            int is_sys = (strcmp(e->seg[i].kind, "SYSTEM") == 0);
            if ((pass == 0) != is_sys) continue;
            int n = snprintf(buf + used, sizeof(buf) - used, "%s", e->seg[i].text);
            if (n > 0) used += (size_t)n;
            if (used >= sizeof(buf) - 1) { used = sizeof(buf) - 1; break; }
        }
    return gmb_ctx_set_view(agent, buf);
}
